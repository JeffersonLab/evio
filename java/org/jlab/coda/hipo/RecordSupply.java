/*
 *   Copyright (c) 2017.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.hipo;

import java.nio.ByteOrder;
import java.util.Arrays;

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
 *       as long as <b># threads <= # of ring items!!!</b>.
 *       That same user does a release() when done with the record.
 *
 *   (3) The consumer who calls getToWrite() will get that ring item and will
 *       write its data to a file or another buffer. There may be only 1
 *       such thread. This same user does a release() when done with the record.
 *
 *                       ||
 *                       ||  writeBarrier
 *           >           ||
 *         /            _____
 *    Write thread     /  |  \
 *              --->  /1 _|_ 2\  <---- Compression Threads 1-M
 *  ================ |__/   \__|               |
 *                   |6 |   | 3|               V
 *             ^     |__|___|__| ==========================
 *             |      \ 5 | 4 /       compressBarrier
 *         Producer->  \__|__/
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
    /** Type type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip). */
    private int compressionType;
    /** Number of threads doing compression simultaneously. */
    private int compressionThreadCount = 1;
    /** Ring buffer. */
    private final RingBuffer<RecordRingItem> ringBuffer;

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


    /** Class used to initially create all items in ring buffer. */
    private final class RecordFactory implements EventFactory<RecordRingItem> {
        public RecordRingItem newInstance() {
            return new RecordRingItem(order, compressionType);
        }
    }


    /**
     * Constructor. Ring size of 4 records, compression thread count of 1,
     * LZ4 compression, little endian data.
     */
    public RecordSupply() {
        // IllegalArgumentException is never thrown here
        this(4, ByteOrder.LITTLE_ENDIAN, 1, 1);
    }


    /**
     * Constructor.
     * @param ringSize     number of RecordRingItem objects in ring buffer.
     * @param order        byte order of RecordOutputStream in each RecordRingItem object.
     * @param threadCount  number of threads simultaneously doing compression.
     *                     Must be <= ringSize.
     * @param compressionType type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip).
     * @throws IllegalArgumentException if args < 1, ringSize not power of 2,
     *                                  threadCount > ringSize, compression type invalid.
     */
    public RecordSupply(int ringSize, ByteOrder order,
                        int threadCount, int compressionType)
            throws IllegalArgumentException {

        if (ringSize < 1 || Integer.bitCount(ringSize) != 1) {
            throw new IllegalArgumentException("ringSize must be a power of 2");
        }

        if (ringSize < threadCount) {
            throw new IllegalArgumentException("threadCount must be <= ringSize");
        }

        if (compressionType < 0 || compressionType > 3) {
            throw new IllegalArgumentException("compressionType must be 0,1,2,or 3");
        }

        // # compression threads defaults to 1 if given bad value
        if (threadCount > 0) {
            this.compressionThreadCount = threadCount;
        }

        this.order = order;
        this.compressionType = compressionType;

        // Create ring buffer with "ringSize" # of elements
        ringBuffer = createSingleProducer(new RecordFactory(), ringSize,
                                          new YieldingWaitStrategy());

        // The thread which fills records is considered the "producer"
        // and doesn't need a barrier.

        // Barrier & sequences so record-COMPRESSING threads can get records.
        // This is the first group of consumers which all share the same barrier.
        compressBarrier = ringBuffer.newBarrier();
        compressSeqs = new Sequence[compressionThreadCount];
        nextCompressSeqs = new long[compressionThreadCount];
        availableCompressSeqs = new long[compressionThreadCount];
        // Initialize with -1's
        Arrays.fill(availableCompressSeqs, -1);

        for (int i=0; i < compressionThreadCount; i++) {
            compressSeqs[i] = new Sequence(Sequencer.INITIAL_CURSOR_VALUE);
            // Each thread will get different records from each other.
            // First thread gets 0, 2nd thread gets 1, etc.
            nextCompressSeqs[i] = compressSeqs[i].get() + 1 + i;
        }

        // Barrier & sequence so a single record-WRITING thread can get records.
        // This barrier comes after all compressing threads and depends on them
        // first releasing their records.
        writeBarrier = ringBuffer.newBarrier(compressSeqs);
        writeSeq = new Sequence(Sequencer.INITIAL_CURSOR_VALUE);
        nextWriteSeq = writeSeq.get() + 1;
        availableWriteSeq = -1;
        // After this writing thread releases a record, make it available for re-filling.
        // In other words, this is the last consumer.
        ringBuffer.addGatingSequences(writeSeq);
    }


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
     */
    public RecordRingItem getToCompress(int threadNumber) throws InterruptedException {

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
        catch (final AlertException ex) {
            ex.printStackTrace();
        }

        return item;
    }

    /**
     * Get the next available record item from the ring buffer
     * in order to write data into it.
     * @return next available record item in ring buffer
     *         in order to write data into it.
     */
    public RecordRingItem getToWrite() throws InterruptedException {

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
        catch (final AlertException ex) {
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
        return;
    }

    /**
     * A writer thread releases its claim on the given ring buffer item
     * so it becomes available for reuse by the producer.
     * To be used in conjunction with {@link #getToWrite()}.
     * @param item item in ring buffer to release for reuse.
     */
    public void releaseWriter(RecordRingItem item) {
        if (item == null) return;
        item.getSequenceObj().set(item.getSequence());
        return;
    }

    /**
     * Release claim on ring items up to sequenceNum for the given compressor thread.
     * Used internally to free up records that the compressor thread will skip
     * over anyway.
     * @param threadNum    compressor thread number
     * @param sequenceNum  sequence to release
     */
    void release(int threadNum, long sequenceNum) {
        compressSeqs[threadNum].set(sequenceNum);
    }

}
