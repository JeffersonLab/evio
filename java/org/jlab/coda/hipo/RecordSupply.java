/*
 *   Copyright (c) 2017.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.hipo;

import java.nio.ByteOrder;

import com.lmax.disruptor.*;

import static com.lmax.disruptor.RingBuffer.createSingleProducer;

/**
 * This thread-safe, lock-free class is used to provide a very fast supply
 * of RecordRingItems which are reused (using Disruptor software package).<p>
 *
 * It is a supply of RecordRingItems in which a single producer does a {@link #get()},
 * fills the record with data, and finally does a {@link #publish(RecordRingItem)}
 * to let consumers know the data is ready.<p>
 *
 * This class is setup to handle 2 types of consumers.
 * The first type is a thread which compresses a record's data.
 * The number of such consumers is set in the constructor.
 * Each of these will call {@link #getToCompress(int)} to get a record
 * and eventually call {@link #releaseCompressor(RecordRingItem)} to indicate it is
 * finished compressing and the record is available for writing to disk.<p>
 *
 * The second type of consumer is a single thread which writes all compressed
 * records to a file. This will call {@link #getToWrite()} to get a record
 * and eventually call {@link #releaseWriter(RecordRingItem)} to indicate it is
 * finished writing and the record is available for being filled with new data.<p>
 *
 * Due to the multithreaded nature of writing files using this class, a mechanism
 * for reporting errors that occur in the writing and compressing threads is provided.
 * Also, and probably more importantly, one can call errorAlert() to notify any
 * compression or write threads that an error has occurred. That way these threads
 * can clean up and exit.<p>
 *
 * It transparently makes sure that all records are written in the proper order.
 *
 * <pre><code>
 *
 *   This is a graphical representation of how our ring buffer is set up.
 *
 *   (1) The producer who calls get() will get a ring item allowing a record to be
 *       filled. That same user does a publish() when done with the record.
 *
 *   (2) The consumer who calls getToCompress() will get that ring item and will
 *       compress its data. There may be any number of compression threads
 *       as long as <b># threads &lt;= # of ring items!!!</b>.
 *       That same user does a releaseCompressor() when done with the record.
 *
 *   (3) The consumer who calls getToWrite() will get that ring item and will
 *       write its data to a file or another buffer. There may be only 1
 *       such thread. This same user does a releaseWriter() when done with the record.
 *
 *                       ||
 *                       ||  writeBarrier
 *           ^gt;           ||
 *         /            _____
 *    Write thread     /  |  \
 *              ---&gt;  /1 _|_ 2\  &lt;---- Compression Threads 1-M
 *  ================ |__/   \__|               |
 *                   |6 |   | 3|               V
 *             ^     |__|___|__| ==========================
 *             |      \ 5 | 4 /       compressBarrier
 *         Producer-&gt;  \__|__/
 *
 *
 * </code></pre>
 *
 * @version 6.0
 * @since 6.0 9/21/17
 * @author timmer
 */
public class RecordSupply {

    /** Byte order of RecordOutputStream in each RecordRingItem. */
    private ByteOrder order;
    /** Ring buffer. */
    private final RingBuffer<RecordRingItem> ringBuffer;

    /** Max number of events each record can hold.
     *  Value of O means use default (1M). */
    private int maxEventCount;
    /** Max number of uncompressed data bytes each record can hold.
     *  Value of < 8MB results in default of 8MB. */
    private int maxBufferSize;
    /** Type type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip). */
    private CompressionType compressionType;
    /** Number of threads doing compression simultaneously. */
    private int compressionThreadCount = 1;
    /** Number of records held in this supply. */
    private int ringSize;

    // Stuff for reporting errors

    /** Do we have an error writing and/or compressing data? */
    private volatile boolean haveError;
    /** Error string. */
    private volatile String error;

    // Stuff for reporting conditions (disk is full)

    /** Writing of a RecordRingItem to disk has been stopped
     * due to the disk partition being full. */
    private volatile boolean diskFull;

    // Stuff for compression threads

    /** Ring barrier to prevent records from being used by write thread
     *  before compression threads release them. */
    private final SequenceBarrier compressBarrier;
    /** Sequences for compressing data, one per compression thread. */
    private final Sequence[] compressSeqs;
    /** Array of next sequences (index of next item desired),
     *  one per compression thread. */
    private long nextCompressSeqs[];
    /** Array of available sequences (largest index of sequentially available items),
     *  one per compression thread. */
    private long availableCompressSeqs[];

    // Stuff for writing thread

    /** Ring barrier to prevent records from being re-used by producer
     *  before write thread releases them. */
    private final SequenceBarrier writeBarrier;
    /** Sequence for writing data. */
    private final Sequence writeSeq;
    /** Index of next item desired. */
    private long nextWriteSeq;
    /** Largest index of sequentially available items. */
    private long availableWriteSeq;

    // For thread safety in getToWrite() & releaseWriter()

    /** The last sequence to have been released after writing. */
    private long lastSequenceReleased = -1L;
    /** The highest sequence to have asked for release after writing. */
    private long maxSequence = -1L;
    /** The number of sequences between maxSequence &
     * lastSequenceReleased which have called releaseWriter(), but not been released yet. */
    private int between;



    /** Class used to initially create all items in ring buffer. */
    private final class RecordFactory implements EventFactory<RecordRingItem> {
        public RecordRingItem newInstance() {
            return new RecordRingItem(order, maxEventCount, maxBufferSize, compressionType);
        }
    }


    /**
     * Constructor. Ring size of 4 records, compression thread count of 1,
     * LZ4 compression, little endian data.
     */
    public RecordSupply() {
        // IllegalArgumentException is never thrown here
        this(4, ByteOrder.LITTLE_ENDIAN, 1, 0, 0, CompressionType.RECORD_COMPRESSION_LZ4);
    }


    /**
     * Constructor.
     * @param ringSize        number of RecordRingItem objects in ring buffer.
     * @param order           byte order of RecordOutputStream in each RecordRingItem object.
     * @param threadCount     number of threads simultaneously doing compression.
     *                        Must be &lt;= ringSize.
     * @param maxEventCount   max number of events each record can hold.
     *                        Value &lt;= O means use default (1M).
     * @param maxBufferSize   max number of uncompressed data bytes each record can hold.
     *                        Value of &lt; 8MB results in default of 8MB.
     * @param compressionType type of data compression to do.
     * @throws IllegalArgumentException if args &lt; 1, ringSize not power of 2,
     *                                  threadCount &gt; ringSize.
     */
    public RecordSupply(int ringSize, ByteOrder order,
                        int threadCount, int maxEventCount, int maxBufferSize,
                        CompressionType compressionType)
            throws IllegalArgumentException {

        if (ringSize < 1 || Integer.bitCount(ringSize) != 1) {
            throw new IllegalArgumentException("ringSize must be a power of 2");
        }

        if (ringSize < threadCount) {
            throw new IllegalArgumentException("threadCount must be <= ringSize");
        }

        // # compression threads defaults to 1 if given bad value
        if (threadCount > 0) {
            this.compressionThreadCount = threadCount;
        }

        this.order = order;
        this.ringSize = ringSize;
        this.maxEventCount = maxEventCount;
        this.maxBufferSize = maxBufferSize;
        this.compressionType = compressionType;

        // Create ring buffer with "ringSize" # of elements
        ringBuffer = createSingleProducer(new RecordFactory(), ringSize,
                                          new SpinCountBackoffWaitStrategy(10000, new LiteBlockingWaitStrategy()));
                                    //      new YieldingWaitStrategy());

        // Thread which fills records is considered the "producer" and doesn't need a barrier

        // Barrier & sequences so record-COMPRESSING threads can get records.
        // This is the first group of consumers which all share the same barrier.
        compressBarrier = ringBuffer.newBarrier();
        compressSeqs = new Sequence[compressionThreadCount];
        nextCompressSeqs = new long[compressionThreadCount];
        availableCompressSeqs = new long[compressionThreadCount];

        for (int i=0; i < compressionThreadCount; i++) {
            compressSeqs[i] = new Sequence(Sequencer.INITIAL_CURSOR_VALUE);
            // Each thread will get different records from each other.
            // First thread gets 0, 2nd thread gets 1, etc.
            long firstSeqToGet = Sequencer.INITIAL_CURSOR_VALUE + 1 + i;
            nextCompressSeqs[i] = firstSeqToGet;
            // Release, in advance, records to be skipped next.
            // This keeps things from hanging up.
            if (i != 0) {
                compressSeqs[i].set(firstSeqToGet - 1);
            }
            // Initialize with -1's
            availableCompressSeqs[i] = -1L;
        }

        // Barrier & sequence so a single record-WRITING thread can get records.
        // This barrier comes after all compressing threads and depends on them
        // first releasing their records.
        writeBarrier = ringBuffer.newBarrier(compressSeqs);
        writeSeq = new Sequence(Sequencer.INITIAL_CURSOR_VALUE);
        nextWriteSeq = Sequencer.INITIAL_CURSOR_VALUE + 1;
        availableWriteSeq = -1;
        // After this writing thread releases a record, make it available for re-filling.
        // In other words, this is the last consumer.
        ringBuffer.addGatingSequences(writeSeq);
    }

    
    /**
     * Method to have sequence barriers throw a Disruptor's AlertException.
     * In this case, we can use it to warn write and compress threads which
     * are waiting on barrier.waitFor() in {@link #getToCompress(int)} and
     * {@link #getToWrite()}. Do this in case of a write, compress, or some other error.
     * This allows any threads waiting on these 2 methods to wake up, clean up,
     * and exit.
     */
    public void errorAlert() {
        writeBarrier.alert();
        compressBarrier.alert();
    }

    
    /**
     * Get the max number of bytes the records in this supply can hold all together.
     * @return max number of bytes the records in this supply can hold all together.
     */
    public int getMaxRingBytes() {return (int) (ringSize*1.1*maxBufferSize);}

    /**
     * Get the number of records in this supply.
     * @return number of records in this supply.
     */
    public int getRingSize() {return ringSize;}

    /**
     * Get the byte order of all records in this supply.
     * @return byte order of all records in this supply.
     */
    public ByteOrder getOrder() {return order;}

    /**
     * Get the percentage of data-filled but unwritten records in ring.
     * Value of 0 means everything's been written. Value of 100 means
     * that all records in the ring are filled with data (perhaps in
     * various stages of being compressed) and have not been written yet.
     *
     * @return percentage of used records in ring.
     */
    public long getFillLevel() {
        return 100*(ringBuffer.getCursor() - ringBuffer.getMinimumGatingSequence())/ringBuffer.getBufferSize();
    }

    /**
     * Get the sequence of last ring buffer item published (seq starts at 0).
     * @return sequence of last ring buffer item published (seq starts at 0).
     */
    public long getLastSequence() {
        return ringBuffer.getCursor();
    }

    /**
     * Get the next available record item from the ring buffer.
     * Use it to write data into the record.
     * @return next available record item in ring buffer in order to write data into it.
     */
    public RecordRingItem get() {
        // Producer gets next available record
        long getSequence = ringBuffer.next();

        // Get object in that position (sequence) of ring buffer
        RecordRingItem bufItem = ringBuffer.get(getSequence);

        // This reset does not change compression type, fileId, or header type
        bufItem.reset();

        // Store sequence for later releasing of record
        bufItem.fromProducer(getSequence);

        return bufItem;
    }

    /**
     * Tell consumers that the record item is ready for consumption.
     * To be used in conjunction with {@link #get()}.
     * @param item record item available for consumers' use.
     */
    public void publish(RecordRingItem item) {
        if (item == null) return;
        ringBuffer.publish(item.getSequence());
    }

    /**
     * Get the next available record item from the ring buffer
     * in order to compress the data already in it.
     * @param threadNumber number of thread (0,1, ...) used to compress.
     *                     This number cannot exceed (compressionThreadCount - 1).
     * @return next available record item in ring buffer
     *         in order to compress data already in it.
     * @throws AlertException  if {@link #errorAlert()} called.
     * @throws InterruptedException if thread interrupted.
     */
    public RecordRingItem getToCompress(int threadNumber)
                              throws AlertException, InterruptedException {

        RecordRingItem item = null;

        try  {
            // Only wait for read of volatile memory if necessary ...
            if (availableCompressSeqs[threadNumber] < nextCompressSeqs[threadNumber]) {
                // Return # of largest consecutively available item
                availableCompressSeqs[threadNumber] = compressBarrier.waitFor(nextCompressSeqs[threadNumber]);
            }

            // Get the item since we know it's available
            item = ringBuffer.get(nextCompressSeqs[threadNumber]);
            // Store variables that will help free this item when release is called
            item.fromConsumer(nextCompressSeqs[threadNumber], compressSeqs[threadNumber]);
            // Set the next item we'll be trying to get.
            // Note that different compression threads get different items.
            nextCompressSeqs[threadNumber] += compressionThreadCount;
        }
        catch (final com.lmax.disruptor.TimeoutException ex) {
            // Never happen since we don't use timeout wait strategy
            ex.printStackTrace();
        }

        return item;
    }

    /**
     * Get the next available record item from the ring buffer
     * in order to write data into it.
     * @return next available record item in ring buffer
     *         in order to write data into it.
     * @throws AlertException  if {@link #errorAlert()} called.
     * @throws InterruptedException if thread interrupted.
     */
    public RecordRingItem getToWrite() throws AlertException, InterruptedException {

        RecordRingItem item = null;

        try  {
            if (availableWriteSeq < nextWriteSeq) {
                availableWriteSeq = writeBarrier.waitFor(nextWriteSeq);
            }

            item = ringBuffer.get(nextWriteSeq);
            item.fromConsumer(nextWriteSeq++, writeSeq);
        }
        catch (final com.lmax.disruptor.TimeoutException ex) {
            ex.printStackTrace();
        }

        return item;
    }

    /**
     * A compressing thread releases its claim on the given ring buffer item
     * so it becomes available for use by writing thread behind the write barrier.
     * Because a compressing thread gets only every Nth record where
     * N = compressionThreadCount, once it releases this record it also
     * needs to release all events coming after, up until the one it will
     * take next. In other words, also release the records it will skip over
     * next. This allows close() to be called at any time without things
     * hanging up.<p>
     *
     * To be used in conjunction with {@link #getToCompress(int)}.
     * @param item item in ring buffer to release for reuse.
     */
    public void releaseCompressor(RecordRingItem item) {
        if (item == null) return;
        item.getSequenceObj().set(item.getSequence() + compressionThreadCount - 1);
    }

    /**
     * A writer thread releases its claim on the given ring buffer item
     * so it becomes available for reuse by the producer.
     * To be used in conjunction with {@link #getToWrite()}.<p>
     *
     * Care must be taken to ensure thread-safety.
     * This method may only be called if the writing is done IN THE SAME THREAD
     * as the calling of this method so that items are released in sequence
     * as ensured by the caller.
     * Otherwise use {@link #releaseWriter(RecordRingItem)}.
     *
     * @param item item in ring buffer to release for reuse.
     * @return false if item not released since item is null, else true.
     */
    public boolean releaseWriterSequential(RecordRingItem item) {
        if (item == null || item.isAlreadyReleased()) return false;
        item.getSequenceObj().set(item.getSequence());
        return true;
    }

    /**
     * A writer thread releases its claim on the given ring buffer item
     * so it becomes available for reuse by the producer.
     * To be used in conjunction with {@link #getToWrite()}.<p>
     *
     * Care must be taken to ensure thread-safety.
     * The following can happen if no precautions are taken.
     * In the case of EventWriterUnsync, writing to a file involves 2, simultaneous,
     * asynchronous writes to a file - both in separate threads to the thread which
     * calls the "write" method. If the writing of the later item finishes first, it
     * releases it's item and sequence which, unfortunately, also releases the
     * previous item's sequence (which is still being written). When the first write is complete,
     * it also releases its item. However item.getSequenceObj() will return null
     * (causing NullPointerException) because it was already released thereby allowing
     * it to be reused and reset called on it.<p>
     *
     * In order to prevent such a scenario, releaseWriter ensures that items are only
     * released in sequence.<p>
     *
     * If the same item is released more than once, bad things will happen.
     * Thus the caller must take steps to prevent it. To avoid problems,
     * call {@link RecordRingItem#setAlreadyReleased(boolean)} and set to true if
     * item is released but will still be used in some manner.
     *
     * @param item item in ring buffer to release for reuse.
     * @return false if item not released since item is null, else true.
     */
    public boolean releaseWriter(RecordRingItem item) {
        if (item == null) {
            return false;
        }

        if (item.isAlreadyReleased()) {
//System.out.println("RecordSupply: item already released!");
            return false;
        }

        synchronized (this) {
            long seq = item.getSequence();

            // If we got a new max ...
            if (seq > maxSequence) {
                // If the old max was > the last released ...
                if (maxSequence > lastSequenceReleased) {
                    // we now have another sequence between last released & new max
                    between++;
                }

                // Set the new max
                maxSequence = seq;
            }
            // If we're < max and > last, then we're in between
            else if (seq > lastSequenceReleased) {
                between++;
            }

            // If we now have everything between last & max, release it all.
            // This way higher sequences are never released before lower.
            if ((maxSequence - lastSequenceReleased - 1L) == between) {
                item.getSequenceObj().set(maxSequence);
                lastSequenceReleased = maxSequence;
                between = 0;
            }
        }
        return true;
    }


    /**
     * Release claim on ring items up to sequenceNum for the given compressor thread.
     * For internal use only - to free up records that the compressor thread will skip
     * over anyway.
     * @param threadNum    compressor thread number
     * @param sequenceNum  sequence to release
     */
    public void release(int threadNum, long sequenceNum) {
        if (sequenceNum < 0) return;
        compressSeqs[threadNum].set(sequenceNum);
    }


    /**
     * Has an error occurred in writing or compressing data?
     * @return {@code true} if an error occurred in writing or compressing data.
     */
    public boolean haveError() {return haveError;}

    /**
     * Set whether an error occurred in writing or compressing data.
     * @param err {@code true} if an error occurred in writing or compressing data.
     */
    public void haveError(boolean err) {haveError = err;}

    /**
     * If there is an error, this contains the error message.
     * @return error message if there is an error.
     */
    public String getError() {return error;}

    /**
     * Set the error message.
     * @param err error message.
     */
    public void setError(String err) {error = err;}

    /**
     * Has the writing of a RecordRingItem to disk has been stopped
     * due to the disk partition being full?
     * @return  true if he writing of a RecordRingItem to disk has been stopped
     *          due to the disk partition being full.
     */
    public boolean isDiskFull() {return diskFull;}

    /**
     * Set whether the writing of a RecordRingItem to disk has been stopped
     * due to the disk partition being full.
     * @param full true if he writing of a RecordRingItem to disk has been stopped
     *             due to the disk partition being full.
     */
    public void setDiskFull(boolean full) {diskFull = full;}

}
