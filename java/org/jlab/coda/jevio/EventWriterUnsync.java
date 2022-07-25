/*
 * Copyright (c) 2018, Jefferson Science Associates, all rights reserved.
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 */

package org.jlab.coda.jevio;


import com.lmax.disruptor.AlertException;
import org.jlab.coda.hipo.*;

import java.io.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.AsynchronousFileChannel;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.BitSet;
import java.util.concurrent.*;

/**
 * An EventWriter object is used for writing events to a file or to a byte buffer.
 * This class does NOT write versions 1-4 data, only version 6!
 * This class is not thread-safe.
 *
 * <pre><code>
 *
 *            FILE Uncompressed
 *
 *    +----------------------------------+
 *    +                                  +
 *    +      General File Header         +
 *    +                                  +
 *    +----------------------------------+
 *    +----------------------------------+
 *    +                                  +
 *    +     Index Array (optional)       +
 *    +                                  +
 *    +----------------------------------+
 *    +----------------------------------+
 *    +      User Header (optional)      +
 *    +        --------------------------+
 *    +       |        Padding           +
 *    +----------------------------------+
 *    +----------------------------------+
 *    +                                  +
 *    +          Data Record 1           +
 *    +                                  +
 *    +----------------------------------+
 *                    .
 *                    .
 *                    .
 *    +----------------------------------+
 *    +                                  +
 *    +          Data Record N           +
 *    +                                  +
 *    +----------------------------------+
 *
 * ---------------------------------------------
 * ---------------------------------------------
 *
 *              FILE Compressed
 *
 *    +----------------------------------+
 *    +                                  +
 *    +      General File Header         +
 *    +                                  +
 *    +----------------------------------+
 *    +----------------------------------+
 *    +                                  +
 *    +     Index Array (optional)       +
 *    +                                  +
 *    +----------------------------------+
 *    +----------------------------------+
 *    +      User Header (optional)      +
 *    +        --------------------------+
 *    +       |         Padding          +
 *    +----------------------------------+
 *    +----------------------------------+
 *    +           Compressed             +
 *    +          Data Record 1           +
 *    +                                  +
 *    +----------------------------------+
 *                    .
 *                    .
 *                    .
 *    +----------------------------------+
 *    +           Compressed             +
 *    +          Data Record N           +
 *    +                                  +
 *    +----------------------------------+
 *
 *    The User Header contains a data record which
 *    holds the dictionary and first event, if any.
 *    The general file header, index array, and
 *    user header are never compressed.
 *
 *    Writing a buffer is done without the general file header
 *    and the index array and user header which follow.
 *    
 *
 * </code></pre>
 *
 * @author timmer
 */
final public class EventWriterUnsync implements AutoCloseable {

    /**
     * Class used to take data-filled record from supply, compress it,
     * and release it back to the supply.
     */
    class RecordCompressor extends Thread {
        /** Keep track of this thread with id number. */
        private final int num;

        /**
         * Constructor.
         * @param threadNumber unique thread number starting at 0.
         */
        RecordCompressor(int threadNumber) {num = threadNumber;}

        @Override
        public void run() {
            try {
                // The first time through, we need to release all records coming before
                // our first in case there are < num records before close() is called.
                // This way close() is not waiting for thread #12 to get and subsequently
                // release items 0 - 11 when there were only 5 records total.
                // (threadNumber starts at 0).
                supply.release(num, num - 1);

                while (true) {
                    if (Thread.interrupted()) {
//System.out.println("RecordCompressor:  quit thread through interrupt 1");
                        return;
                    }

                    // Get the next record for this thread to compress
                    RecordRingItem item = supply.getToCompress(num);
                    // Pull record out of wrapping object
                    RecordOutputStream record = item.getRecord();
                    // Set compression type
                    RecordHeader header = record.getHeader();
                    header.setCompressionType(compressionType);

                    // Do compression
                    record.build();
                    // Release back to supply
                    supply.releaseCompressor(item);
                }
            }
            catch (AlertException e) {
                // We've been woken up in getToWrite through a user calling supply.errorAlert()
//System.out.println("RecordCompressor:  quit thread through alert");
            }
            catch (InterruptedException e) {
                // We've been interrupted while blocked in getToCompress
                // which means we're all done.
//System.out.println("RecordCompressor:  quit thread through interrupt 2");
            }
            catch (Exception e) {
                // Catch problems with buffer overflow and multiple compressing threads
                supply.haveError(true);
                supply.setError(e.getMessage());
            }
        }
    }


    /**
     * Class used to take data-filled record from supply, write it,
     * and release it back to the supply. Only one of these exists
     * which makes tracking indexes and lengths easy.
     */
    class RecordWriter extends Thread {

        /** The highest sequence to have been currently processed. */
        private volatile long lastSeqProcessed = -1;

        /** Place to store event when disk is full. */
        private RecordRingItem storedItem;

        /** Force write to disk. */
        private volatile boolean forceToDisk;

        /** Id of RecordRingItem that initiated the forced write. */
        private volatile long forcedRecordId;


        /**
         * Store the id of the record which is forcing a write to disk,
         * even if disk is "full".
         * The idea is that we look for this record and once it has been
         * written, then we don't force any following records to disk
         * (unless we're told to again by the calling of this function).
         * Generally, for an emu, this method gets called when control events
         * arrive. In particular, when the END event comes, it must be written
         * to disk with all the events that preceded it.
         *
         * @param id id of record causing the forced write to disk.
         */
        void setForcedRecordId(long id) {
            forcedRecordId = id;
            forceToDisk = true;
        }


        /** Wait for the last item to be processed, then exit thread. */
        void waitForLastItem() {
            while (supply.getLastSequence() > lastSeqProcessed) {
                Thread.yield();
            }
            // Interrupt this thread, not the thread calling this method
            this.interrupt();
        }

        private RecordRingItem storeRecordCopy(RecordRingItem rec) {
            storedItem = new RecordRingItem(rec);
            return storedItem;
        }

        @Override
        public void run() {
            try {

                while (true) {
                    if (Thread.interrupted()) {
                        return;
                    }

                    // Get the next record for this thread to write
                    RecordRingItem item = supply.getToWrite();
                    long currentSeq = item.getSequence();

                    // Only need to check the disk when writing the first record following
                    // a file split. That first write will create the file. If there isn't
                    // enough room, then flag will be set.
                    boolean checkDisk = item.isCheckDisk();

                    // Check the disk before we try to write if about to create another file,
                    // we're told to check the disk and we're not forcing to disk
                    if ((bytesWritten < 1) && checkDisk && (!forceToDisk)) {

                        // If there isn't enough free space to write the complete, projected
                        // size file, and we're not trying to force the write ...
                        // store a COPY of the record for now and release the original so
                        // that writeEventToFile() does not block.
                        //
                        // So here is the problem. We are stuck in a loop here if disk is full.
                        // If events are flowing and since writing data to file is the bottleneck,
                        // it is likely that all records have been filled and published onto
                        // the ring. AND, writeEventToFile() blocks in a spin as it tries to get the
                        // next record from the ring which, unfortunately, never comes.
                        //
                        // When writeEventToFile() blocks, it can't respond by returning a "false" value.
                        // This plays havoc with code like the emu which is not expecting the write
                        // to block (at least not for very long).
                        //
                        // To break writeEventToFile() out of its spinning block, we make a copy of the
                        // item we're trying to write and release the original record. This allows
                        // writeEventToFile() to grab a new (newly released) record, write an event into
                        // it, and return to the caller. From then on, writeEventToFile() can prevent
                        // blocking by checking for a full disk (which it couldn't do before since
                        // the signal came too late).

                        while (fullDisk() && (!forceToDisk)) {
                            // Wait for a sec and try again
                            Thread.sleep(1000);

                            // If we released the item in a previous loop, don't do it again
                            if (!item.isAlreadyReleased()) {
                                // Copy item
                                RecordRingItem copiedItem = storeRecordCopy(item);
                                // Release original so we don't block writeEvent()
                                supply.releaseWriter(item);
                                item = copiedItem;
                            }

                            // Wait until space opens up
                        }

                        // If we're here, there must be free space available even
                        // if there previously wasn't.
                    }

                    // Write current item to file
                    writeToFileMT(item, forceToDisk);

                    // Turn off forced write to disk, if the record which
                    // initially triggered it, has now been written.
                    if (forceToDisk && (forcedRecordId == item.getId()))  {
//System.out.println("  write: WROTE the record that triggered force, reset to false");
                        forceToDisk = false;
                    }

                    // Now we're done with this sequence
                    lastSeqProcessed = currentSeq;

                    // Split file if needed
                    if (item.splitFileAfterWrite()) {
                        splitFile();
                    }

// TODO: do we need this???
                    // Release back to supply
                    //supply.releaseWriter(item);

                }
            }
            catch (AlertException e) {
                // Woken up in getToWrite through user call to supply.errorAlert()
//System.out.println("RecordWriter: quit thread through alert");
            }
            catch (InterruptedException e) {
                // Interrupted while blocked in getToWrite which means we're all done
//System.out.println("RecordWriter: quit thread through interrupt 2");
            }
            catch (Exception e) {
                // Here if error in file writing
//System.out.println("RecordWriter: quit thread through file writing error");

                //StringWriter sw = new StringWriter();
                //PrintWriter pw  = new PrintWriter(sw);
                //e.printStackTrace(pw);
                //supply.setError(sw.toString());

                supply.haveError(true);
                supply.setError(e.getMessage());
            }
        }
    }


    /** Class used to close files in order received, each in its own thread,
     *  to avoid slowing down while file splitting. */
    private final class FileCloser {

        /** Thread pool with 1 thread. */
        private final ExecutorService threadPool;

        /** Constructor. */
        private FileCloser() {threadPool = Executors.newSingleThreadExecutor();}

        /** Close the thread pool in this object while executing all existing tasks. */
        void close() {threadPool.shutdown();}



        /**
         * Close the given file, in the order received, in a separate thread.
         * @param afc file channel to close
         */
        void closeAsyncFile(AsynchronousFileChannel afc,
                            Future<Integer> future1, Future<Integer> future2,
                            FileHeader fileHeader, ArrayList<Integer> recordLengths,
                            long bytesWritten, long fileWritingPosition,  int recordNumber,
                            boolean addingTrailer, boolean writeIndex,
                            boolean release1, boolean release2,
                            RecordRingItem ringItem1, RecordRingItem ringItem2) {

            threadPool.submit(new CloseAsyncFChan(afc, future1, future2,
                                                 fileHeader, recordLengths,
                                                 bytesWritten, fileWritingPosition,
                                                 recordNumber, addingTrailer, writeIndex,
                                                 release1, release2, ringItem1, ringItem2 ));
        }


        private class CloseAsyncFChan implements Runnable {

            // Quantities that may change between when this object is created and
            // when this thread is run. Store these values so they maintain the
            // correct value.
            private final long filePos;
            private final int recordNum;
            private final boolean writeIndx;
            private final Future<Integer> ftr1, ftr2;
            private final boolean addTrailer;
            private final FileHeader fHeader;
            private final long bytesWrittenToFile;
            private final RecordRingItem item1, item2;
            private final ArrayList<Integer> recLengths;
            private final AsynchronousFileChannel afChannel;
            private final boolean releaseItem1, releaseItem2;

            // Local storage
            private byte[] hdrArray = new byte[RecordHeader.HEADER_SIZE_BYTES];
            private ByteBuffer hdrBuffer = ByteBuffer.wrap(hdrArray);


            CloseAsyncFChan(AsynchronousFileChannel afc,
                            Future<Integer> future1, Future<Integer> future2,
                            FileHeader fileHeader, ArrayList<Integer> recordLengths,
                            long bytesWritten, long fileWritingPosition,  int recordNumber,
                            boolean addingTrailer, boolean writeIndex,
                            boolean release1, boolean release2,
                            RecordRingItem ringItem1, RecordRingItem ringItem2) {

                afChannel = afc;
                ftr1 = future1;
                ftr2 = future2;
                item1 = ringItem1;
                item2 = ringItem2;
                writeIndx = writeIndex;
                recordNum = recordNumber;
                releaseItem1 = release1;
                releaseItem2 = release2;
                addTrailer = addingTrailer;
                filePos = fileWritingPosition;
                bytesWrittenToFile = bytesWritten;

                // Fastest way to copy objects
                fHeader = (FileHeader) fileHeader.clone();
                recLengths = (ArrayList<Integer>) recordLengths.clone();
            }


//            public void run_OneAsyncWrite() {
//                // Finish writing to current file
//                if (ftr1 != null) {
//                    try {
//                        // Wait for last write to end before we continue
//                        ftr1.get();
//                    }
//                    catch (Exception e) {}
//                }
//
//                // Release resources back to the ring
//                supply.releaseWriterSequential(item1);
//
//                try {
//                    if (addTrailer && !noFileWriting) {
//                    //if (addTrailer) {
//                        writeTrailerToFile();
//                    }
//                }
//                catch (Exception e) {}
//
//                try {
//                    afChannel.close();
//                }
//                catch (IOException e) {
//                    e.printStackTrace();
//                }
//            }

            public void run() {
                 // Finish writing to current file
                 if (ftr1 != null) {
                     try {
                         // Wait for last write to end before we continue
                         ftr1.get();
                     }
                     catch (Exception e) {}
                 }

                 if (ftr2 != null) {
                     try {
                         ftr2.get();
                     }
                     catch (Exception e) {}
                 }

                 // Release resources back to the ring
                 if (releaseItem1 && (item1 != null))
                     supply.releaseWriter(item1);

                 if (releaseItem2 && (item2 != null))
                     supply.releaseWriter(item2);

                 try {
                     if (addTrailer && !noFileWriting) {
                         writeTrailerToFile();
                     }
                 }
                 catch (Exception e) {
                     e.printStackTrace();
                 }

                 try {
                     afChannel.close();
                 }
                 catch (IOException e) {
                     e.printStackTrace();
                 }
             }


            /**
             * Write a general header as the last "header" or trailer in the file
             * optionally followed by an index of all record lengths.
             * This writes synchronously.
             * This is a modified version of {@link EventWriterUnsync#writeTrailerToFile(boolean)} that allows
             * writing the trailer to the file being closed without affecting the
             * file currently being written.
             *
             * @throws IOException if problems writing to file.
             */
            private void writeTrailerToFile() throws IOException {

                // Our position, right now, is just before trailer

                // If we're NOT adding a record index, just write trailer
                if (!writeIndx) {
                    try {
                        // hdrBuffer is only used in this method
                        hdrBuffer.position(0).limit(RecordHeader.HEADER_SIZE_BYTES);
                        RecordHeader.writeTrailer(hdrBuffer, 0, recordNum, null);
                    }
                    catch (HipoException e) {/* never happen */}

                    // We don't want to let the closer thread do the work of seeing that
                    // this write completes since it'll just complicate the code.
                    // As this is the absolute last write to the file,
                    // just make sure it gets done right here.
                    Future<Integer> f = afChannel.write(hdrBuffer, filePos);
                    try {f.get();}
                    catch (Exception e) {
                        throw new IOException(e);
                    }
                }
                else {
                    // Write trailer with index

                    // How many bytes are we writing here?
                    int bytesToWrite = RecordHeader.HEADER_SIZE_BYTES + 4*recLengths.size();

                    // Make sure our array can hold everything
                    if (hdrArray.length < bytesToWrite) {
                        hdrArray = new byte[bytesToWrite];
                        hdrBuffer = ByteBuffer.wrap(hdrArray).order(byteOrder);
                    }
                    hdrBuffer.limit(bytesToWrite).position(0);

                    // Place data into hdrBuffer - both header and index
                    try {
                        RecordHeader.writeTrailer(hdrBuffer, 0, recordNum, recLengths);
                    }
                    catch (HipoException e) {/* never happen */}
                    Future<Integer> f = afChannel.write(hdrBuffer, filePos);
                    try {f.get();}
                    catch (Exception e) {
                        throw new IOException(e);
                    }
                }

                // Update file header's trailer position word
                hdrBuffer.position(0).limit(8);
                hdrBuffer.putLong(0, bytesWrittenToFile);
                Future<Integer> f = afChannel.write(hdrBuffer, FileHeader.TRAILER_POSITION_OFFSET);
                try {f.get();}
                catch (Exception e) {
                    throw new IOException(e);
                }

                // Update file header's bit-info word
                if (addTrailerIndex) {
                    int bitInfo = fHeader.setBitInfo(fHeader.hasFirstEvent(),
                                                     fHeader.hasDictionary(),
                                                        true);
                    hdrBuffer.position(0).limit(4);
                    hdrBuffer.putInt(0, bitInfo);
                    f = afChannel.write(hdrBuffer, FileHeader.BIT_INFO_OFFSET);
                    try {f.get();}
                    catch (Exception e) {
                        throw new IOException(e);
                    }
                }

                // Update file header's record count word
                ByteBuffer bb = ByteBuffer.allocate(4);
                bb.order(byteOrder);
                bb.putInt(0, recordNum - 1);
                f = afChannel.write(bb, FileHeader.RECORD_COUNT_OFFSET);
                try {f.get();}
                catch (Exception e) {
                    throw new IOException(e);
                }
            }

        };

    }


    /** Dictionary and first event are stored in user header part of file header.
     *  They're written as a record which allows multiple events. */
    private RecordOutputStream commonRecord;

    /** Record currently being filled. */
    private RecordOutputStream currentRecord;

    /** Record supply item from which current record comes from. */
    private RecordRingItem currentRingItem;

    /** Fast supply of record items for filling, compressing and writing. */
    private RecordSupply supply;

    /** Max number of bytes held by all records in the supply. */
    private int maxSupplyBytes;

    /** Type of compression being done on data
     *  (0=none, 1=LZ4fastest, 2=LZ4best, 3=gzip). */
    private CompressionType compressionType;

    /** The estimated ratio of compressed to uncompressed data.
     *  (Used to figure out when to split a file). Percentage of original size. */
    private int compressionFactor;

    /** List of record length followed by count to be optionally written in trailer. */
    private ArrayList<Integer> recordLengths = new ArrayList<>(1500);

    /** Number of uncompressed bytes written to the current file/buffer at the moment,
     * including ending header and NOT the total in all split files. */
//TODO: DOES THIS NEED TO BE VOLATILE IF MT write????????????????????????????????
    private long bytesWritten;

    /** Do we add a last header or trailer to file/buffer? */
    private boolean addingTrailer = true;

    /** Do we add a record index to the trailer? */
    private boolean addTrailerIndex;

    /** Byte array large enough to hold a header/trailer. */
    private byte[] headerArray = new byte[RecordHeader.HEADER_SIZE_BYTES];

    /** ByteBuffer large enough to hold a header/trailer. */
    private ByteBuffer headerBuffer = ByteBuffer.wrap(headerArray);

    /** Threads used to compress data. */
    private RecordCompressor[] recordCompressorThreads;

    /** Thread used to write data to file/buffer. */
    private RecordWriter recordWriterThread;

    /** Number of records written to split-file/buffer at current moment. */
    private int recordsWritten;

    /** Running count of the record number. The next one to use starting with 1. */
    private int recordNumber;



    /**
     * Dictionary to include in xml format in the first event of the first record
     * when writing the file.
     */
    private String xmlDictionary;

    /** Byte array containing dictionary in evio format but <b>without</b> record header. */
    private byte[] dictionaryByteArray;

    /** Byte array containing firstEvent in evio format but <b>without</b> record header. */
    private byte[] firstEventByteArray;

    /** <code>True</code> if {@link #close()} was called, else <code>false</code>. */
    private boolean closed;

    /** <code>True</code> if writing to file, else <code>false</code>. */
    private boolean toFile;

    /** <code>True</code> if appending to file, <code>false</code> if (over)writing. */
    private boolean append;

    /** <code>True</code> if appending to file/buffer with dictionary, <code>false</code>. */
    private boolean hasAppendDictionary;

    /**
     * Total number of events written to buffer or file (although may not be flushed yet).
     * Will be the same as eventsWrittenToBuffer (- dictionary) if writing to buffer.
     * If the file being written to is split, this value refers to all split files
     * taken together. Does NOT include dictionary(ies).
     */
    private int eventsWrittenTotal;

    /** The byte order in which to write a file or buffer. */
    private ByteOrder byteOrder;

    //-----------------------
    // Buffer related members
    //-----------------------

    /** CODA id of buffer sender. */
    private int sourceId;

    /** Total size of the buffer in bytes. */
    private int bufferSize;

    /**
     * The output buffer when writing to a buffer.
     * The buffer internal to the currentRecord when writing to a file
     * and which is a reference to one of the internalBuffers.
     * When dealing with files, this buffer does double duty and is
     * initially used to read in record headers before appending data
     * to an existing file and such.
     */
    private ByteBuffer buffer;

    /** Two internal buffers, first element last used in the future1 write,
     * the second last used in future2 write. */
    private ByteBuffer[] usedBuffers;

    /** Three internal buffers used for writing to a file. */
    private ByteBuffer[] internalBuffers;

    /** Number of bytes written to the current buffer for the common record. */
    private int commonRecordBytesToBuffer;

    /** Number of events written to final destination buffer or file's current record
     * NOT including dictionary (& first event?). */
    private int eventsWrittenToBuffer;

    //-----------------------
    // File related members
    //-----------------------

    /** Total size of the internal buffers in bytes. */
    private int internalBufSize;

    /** Variable used to stop accepting events to be included in the inner buffer
     * holding the current block. Used when disk space is inadequate. */
    private boolean diskIsFull;

    /** Variable used to stop accepting events to be included in the inner buffer
     * holding the current block. Used when disk space is inadequate.
     * This is volatile and therefore works between threads. */
    private volatile boolean diskIsFullVolatile;

    /** When forcing events to disk, this identifies which events for the writing thread. */
    private long idCounter = 0;

    /** Header for file only. */
    private FileHeader fileHeader;

    /** Header of file being appended to. */
    private FileHeader appendFileHeader;

    /** File currently being written to. */
    private File currentFile;

    /** Path object corresponding to file currently being written. */
    private Path currentFilePath;

    /** Objects to allow efficient, asynchronous file writing. */
    private Future<Integer> future1, future2;

    /** Objects to allow efficient, asynchronous file writing. */
    private Future<Integer> prevFuture1, prevFuture2;

    /** RingItem1 is associated with future1, etc. When a write is finished,
     * the associated ring item need to be released - but not before! */
    private RecordRingItem ringItem1, ringItem2;

    /** Index for selecting which future (1 or 2) to use for next file write. */
    private int futureIndex;

    /** The asynchronous file channel, used for writing a file. */
    private AsynchronousFileChannel asyncFileChannel;

    /** The location of the next write in the file. */
   private long fileWritingPosition;

    /** Split number associated with output file to be written next. */
    private int splitNumber;

    /** Number of split files produced by this writer. */
    private int splitCount;

    /** Part of filename without run or split numbers. */
    private String baseFileName;

    /** Number of C-style int format specifiers contained in baseFileName. */
    private int specifierCount;

    /** Run number possibly used in naming split files. */
    private int runNumber;

    /**
     * Do we split the file into several smaller ones (val > 0)?
     * If so, this gives the maximum number of bytes to make each file in size.
     */
    private long split;

    /**
     * If splitting file, the amount to increment the split number each time another
     * file is written.
     */
    private int splitIncrement;

    /** Track bytes written to help split a file. */
    private long splitEventBytes;

    /** Track events written to help split a file. */
    private int splitEventCount;

    /**
     * Id of this specific data stream.
     * In CODA, a data stream is a chain of ROCS and EBs ending in a single specific ER.
     */
    private int streamId;

    /** The total number of data streams in DAQ. */
    private int streamCount;

    /** Writing to file with single thread? */
    private boolean singleThreadedCompression;

    /** Is it OK to overwrite a previously existing file? */
    private boolean overWriteOK;

    /** Number of events actually written to the current file - not the total in
     * all split files - including dictionary. */
    private int eventsWrittenToFile;

    /** Does file have a trailer with record indexes? */
    private boolean hasTrailerWithIndex;

    /** File header's user header length in bytes. */
    private int userHeaderLength;

    /** File header's user header's padding in bytes. */
    private int userHeaderPadding;

    /** File header's index array length in bytes. */
    private int indexLength;

    //-----------------------
    /**
     * Flag to do everything except the actual writing of data to file.
     * Set true for testing purposes ONLY.
     */
    private boolean noFileWriting = false;

    //-----------------------

    /** Object used to close files in a separate thread when splitting
     *  so as to allow writing speed not to dip so low. */
    private FileCloser fileCloser;


    //---------------------------------------------
    // FILE Constructors
    //---------------------------------------------

    /**
     * Creates an <code>EventWriter</code> for writing to a file in native byte order.
     * If the file already exists, its contents will be overwritten.
     * If it doesn't exist, it will be created.
     *
     * @param file the file object to write to.<br>
     * @throws EvioException file cannot be created
     */
    public EventWriterUnsync(File file) throws EvioException {
        this(file, false);
    }

    /**
     * Creates an <code>EventWriter</code> for writing to a file in native byte order.
     * If the file already exists, its contents will be overwritten unless
     * it is being appended to. If it doesn't exist, it will be created.
     *
     * @param file     the file object to write to.<br>
     * @param append   if <code>true</code> and the file already exists,
     *                 all events to be written will be appended to the
     *                 end of the file.
     *
     * @throws EvioException file cannot be created
     */
    public EventWriterUnsync(File file, boolean append) throws EvioException {
        this(file, null, append);
    }

    /**
     * Creates an <code>EventWriter</code> for writing to a file in NATIVE byte order.
     * If the file already exists, its contents will be overwritten unless
     * it is being appended to. If it doesn't exist, it will be created.
     *
     * @param file       the file object to write to.<br>
     * @param dictionary dictionary in xml format or null if none.
     * @param append     if <code>true</code> and the file already exists,
     *                   all events to be written will be appended to the
     *                   end of the file.
     *
     * @throws EvioException file cannot be created
     */
    public EventWriterUnsync(File file, String dictionary, boolean append) throws EvioException {
        this(file.getPath(), null, null,
             0, 0, 0, 0,
             ByteOrder.nativeOrder(), dictionary, false,
             append, null, 0, 0, 1, 1,
             CompressionType.RECORD_UNCOMPRESSED, 1, 8, 0);

    }

    /**
     * Creates an <code>EventWriter</code> for writing to a file in native byte order.
     * If the file already exists, its contents will be overwritten.
     * If it doesn't exist, it will be created.
     *
     * @param filename name of the file to write to.<br>
     * @throws EvioException file cannot be created
     */
    public EventWriterUnsync(String filename) throws EvioException {
        this(filename, false);
    }

    /**
     * Creates an <code>EventWriter</code> for writing to a file in NATIVE byte order.
     * If the file already exists, its contents will be overwritten unless
     * it is being appended to. If it doesn't exist, it will be created.
     *
     * @param filename name of the file to write to.<br>
     * @param append   if <code>true</code> and the file already exists,
     *                 all events to be written will be appended to the
     *                 end of the file.
     *
     * @throws EvioException file cannot be created
     */
    public EventWriterUnsync(String filename, boolean append) throws EvioException {
        this(filename, append, ByteOrder.nativeOrder());
    }

    /**
     * Creates an <code>EventWriter</code> for writing to a file in the
     * specified byte order.
     * If the file already exists, its contents will be overwritten unless
     * it is being appended to. If it doesn't exist, it will be created.
     *
     * @param filename  name of the file to write to.<br>
     * @param append    if <code>true</code> and the file already exists,
     *                  all events to be written will be appended to the
     *                  end of the file.
     * @param byteOrder the byte order in which to write the file.
     *
     * @throws EvioException file cannot be created
     */
    public EventWriterUnsync(String filename, boolean append, ByteOrder byteOrder) throws EvioException {
        this(filename, null, null,
             0, 0, 0, 0,
             byteOrder, null, false,
             append, null, 0, 0, 1, 1,
             CompressionType.RECORD_UNCOMPRESSED, 1, 8, 0);
    }

    /**
     * Create an <code>EventWriter</code> for writing events to a file.
     * If the file already exists, its contents will be overwritten
     * unless the "overWriteOK" argument is <code>false</code> in
     * which case an exception will be thrown. Unless ..., the option to
     * append these events to an existing file is <code>true</code>,
     * in which case everything is fine. If the file doesn't exist,
     * it will be created. Byte order defaults to big endian if arg is null.
     * File can be split while writing.<p>
     *
     * The base file name may contain up to 2, C-style integer format specifiers using
     * "d" and "x" (such as <b>%03d</b>, or <b>%x</b>).
     * If more than 2 are found, an exception will be thrown.
     * If no "0" precedes any integer between the "%" and the "d" or "x" of the format specifier,
     * it will be added automatically in order to avoid spaces in the file name.
     * The first specifier will be substituted with the given runNumber value.
     * If the file is being split, the second will be substituted with the split number
     * which starts at 0.
     * If 2 specifiers exist and the file is not being split, no substitutions are made.
     * If no specifier for the splitNumber exists, it is tacked onto the end of the file
     * name after a dot (.).
     * If streamCount &gt; 1, the split number is calculated starting with streamId and incremented
     * by streamCount each time. In this manner, all split files will have unique, sequential
     * names even though there are multiple parallel ERs.
     * <p>
     *
     * The base file name may contain characters of the form <b>$(ENV_VAR)</b>
     * which will be substituted with the value of the associated environmental
     * variable or a blank string if none is found.<p>
     *
     * The base file name may also contain occurrences of the string "%s"
     * which will be substituted with the value of the runType arg or nothing if
     * the runType is null.<p>
     *
     * If multiple streams of data, each writing a file, end up with the same file name,
     * they can be differentiated by a stream id, starting split # and split increment.<p>
     *
     * If there are multiple compression threads, then the maxRecordSize arg is used to determine
     * the max size of each record in the internally created RecordSupply. If a single event,
     * larger than this max size were to be written, a record's memory is expanded to
     * accommodate it. However, if there is only 1 compression/writing thread, this argument
     * is not used and the record will be a max size given by the bufferSize arg. If a single
     * event larger than this were then to be written, an exception would be thrown.
     *
     * @param baseName      base file name used to generate complete file name (may not be null)
     * @param directory     directory in which file is to be placed
     * @param runType       name of run type configuration to be used in naming files
     * @param runNumber     number of the CODA run, used in naming files
     * @param split         if &lt; 1, do not split file, write to only one file of unlimited size.
     *                      Else this is max size in bytes to make a file
     *                      before closing it and starting writing another.
     * @param maxRecordSize max number of uncompressed data bytes each record can hold.
     *                      Value of &lt; 8MB results in default of 8MB.
     *                      The size of the record will not be larger than this size
     *                      unless a single event itself is larger. Used only if compressionThreads
     *                      &gt; 1.
     * @param maxEventCount max number of events each record can hold.
     *                      Value &lt;= O means use default (1M).
     * @param byteOrder     the byte order in which to write the file. This is ignored
     *                      if appending to existing file. Defaults to Big Endian if null.
     * @param xmlDictionary dictionary in xml format or null if none.
     * @param overWriteOK   if <code>false</code> and the file already exists,
     *                      an exception is thrown rather than overwriting it.
     * @param append        if <code>true</code> append written data to given file.
     * @param firstEvent    the first event written into each file (after any dictionary)
     *                      including all split files; may be null. Useful for adding
     *                      common, static info into each split file.
     * @param streamId      streamId number (100 &gt; id &gt; -1) for file name
     * @param splitNumber   number at which to start the split numbers
     * @param splitIncrement amount to increment split number each time another file is created.
     * @param streamCount    total number of streams in DAQ.
     * @param compressionType    type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip).
     * @param compressionThreads number of threads doing compression simultaneously. Defaults to
     *                      1 if = 0 or 1. If 1, just uses the main thread to do compression,
     *                      if any, and handling of writing to file.
     * @param ringSize      number of records in supply ring. If set to &lt; compressionThreads,
     *                      it is forced to equal that value and is also forced to be a multiple of
     *                      2, rounded up.
     * @param bufferSize    number of bytes to make each internal buffer which will
     *                      be storing events before writing them to a file.
     *                      9MB = default if bufferSize = 0. Used only if compresstionThreads = 1.
     *
     * @throws EvioException if maxRecordSize or maxEventCount exceed limits;
     *                       if streamCount &gt; 1 and streamId &lt; 0;
     *                       if defined dictionary or first event while appending;
     *                       if splitting file while appending;
     *                       if file name arg is null;
     *                       if file could not be opened, positioned, or written to;
     *                       if file exists but user requested no over-writing or appending;
     *                       if streamId &lt; 0, splitNumber &lt; 0, or splitIncrement &lt; 1.
     */
    public EventWriterUnsync(String baseName, String directory, String runType,
                             int runNumber, long split,
                             int maxRecordSize, int maxEventCount,
                             ByteOrder byteOrder, String xmlDictionary,
                             boolean overWriteOK, boolean append,
                             EvioBank firstEvent, int streamId,
                             int splitNumber, int splitIncrement, int streamCount,
                             CompressionType compressionType, int compressionThreads,
                             int ringSize, int bufferSize)
            throws EvioException {

        if (baseName == null) {
            throw new EvioException("baseName arg is null");
        }

        if (streamId < 0 || splitNumber < 0 || splitIncrement < 1) {
            throw new EvioException("streamId < 0, splitNumber < 0, or splitIncrement < 1");
        }

        if (runNumber < 1) {
            runNumber = 1;
        }

        if (byteOrder == null) {
            byteOrder = ByteOrder.BIG_ENDIAN;
        }

        if (append) {
            if (split > 0) {
                throw new EvioException("Cannot specify split when appending");
            }
            else if ((xmlDictionary != null) || (firstEvent != null)) {
                throw new EvioException("Cannot specify dictionary or first event when appending");
            }
        }

        // Store arguments
        this.split          = split;
        this.append         = append;
        this.runNumber      = runNumber;
        this.byteOrder      = byteOrder;   // byte order may be overwritten if appending
        this.overWriteOK    = overWriteOK;
        this.xmlDictionary  = xmlDictionary;
        this.streamId       = streamId;
        this.splitNumber    = splitNumber;
        this.splitIncrement = splitIncrement;
        this.streamCount    = streamCount;

        // Only add trailer index if writing file
        addTrailerIndex = true;

        // compressionType = 0 before creating commonRecord, so NO compression in common record.
        // But be sure byteOrder is set so commonRecord has the correct byteOrder.

        if (xmlDictionary != null || firstEvent != null) {
            // Create the common record here, but don't write it to file
            // until the file header is written to the file in writeFileHeader()
            // which in turn is written by writeToFile() which is only called
            // right after a file is created.
            createCommonRecord(xmlDictionary, firstEvent, null, null);
        }

        this.compressionType = compressionType;

        // How much compression will data experience? Percentage of original size.
        switch (compressionType) {
            case RECORD_COMPRESSION_LZ4:  // LZ4
                compressionFactor = 58;
                break;
            case RECORD_COMPRESSION_LZ4_BEST: // LZ4 best
                compressionFactor = 47;
                break;
            case RECORD_COMPRESSION_GZIP: // GZIP
                compressionFactor = 42;
                break;

            case RECORD_UNCOMPRESSED: // NONE
            default:
                compressionFactor = 100;
        }

        if (compressionThreads < 1) {
            compressionThreads = 1;
        }

        toFile = true;
        recordNumber = 1;
//System.out.println("EventWriterUnsync constr: split = "  + split);
//System.out.println("EventWriterUnsync constr: record # set to 1");

        // The following may not be backwards compatible.
        // Make substitutions in the baseName to create the base file name.
        if (directory != null) baseName = directory + "/" + baseName;
        StringBuilder builder = new StringBuilder(100);
        int[] returnVals  = Utilities.generateBaseFileName(baseName, runType, builder);
        specifierCount    = returnVals[0];
        baseFileName      = builder.toString();
        // Also create the first file's name with more substitutions
        String fileName   = Utilities.generateFileName(baseFileName, specifierCount,
                                                       runNumber, split, splitNumber,
                                                       streamId, streamCount);
        // All subsequent split numbers are calculated by adding the splitIncrement
        this.splitNumber += splitIncrement;

        currentFilePath = Paths.get(fileName);
        currentFile = currentFilePath.toFile();

        // If we can't overwrite or append and file exists, throw exception
        if (!overWriteOK && !append && (currentFile.exists() && currentFile.isFile())) {
            throw new EvioException("File exists but user requested no over-writing or appending, "
                                            + currentFile.getPath());
        }

        // There are some inefficiencies below. It appears (7/2022) that
        // the buffers created immediately following are only necessary
        // for the case where singleThreadedCompression = true;
        // However, I'm reluctant to make any more major changes in the code
        // at this point, so we'll just let things sit as they are.

        // Create internal storage buffers.
        // Since we're doing I/O to a file, a direct buffer is more efficient.
        // Besides, the JVM will convert a non-direct to a direct one anyway
        // when doing the I/O (and copy all that data again).
        // Make sure these are DIRECT buffers or performance suffers!
        // The reason there are 3 internal buffers is that we'll be able to
        // do 2 asynchronous writes at once while still filling up the third
        // simultaneously.

        // Allow the user to set the size of the internal buffers up to a point.
        // Value of 0 means use default of 9MB. This value is consistent with
        // RecordOutputStream's own default. Won't use any size < 1MB.
        if (bufferSize < 1) {
            internalBufSize = 9437184;
        }
        else {
            internalBufSize = bufferSize < 1000000 ? 1000000 : bufferSize;
        }
        usedBuffers     = new ByteBuffer[2];
        internalBuffers = new ByteBuffer[3];
        internalBuffers[0] = ByteBuffer.allocateDirect(internalBufSize);
        internalBuffers[1] = ByteBuffer.allocateDirect(internalBufSize);
        internalBuffers[2] = ByteBuffer.allocateDirect(internalBufSize);
        internalBuffers[0].order(byteOrder);
        internalBuffers[1].order(byteOrder);
        internalBuffers[2].order(byteOrder);
        headerBuffer.order(byteOrder);
        buffer = internalBuffers[0];

        // Evio file
        fileHeader = new FileHeader(true);

        if (append) {
            try {
                // For reading existing file and preparing for stream writes
                asyncFileChannel = AsynchronousFileChannel.open(currentFilePath,
                                                                StandardOpenOption.READ,
                                                                StandardOpenOption.WRITE);

                // If we have an empty file, that's OK.
                // Otherwise we have to examine it for compatibility
                // and position ourselves for the first write.
                if (asyncFileChannel.size() > 0) {
                    // Look at file header to find endianness & version.
                    // Endianness given in constructor arg, when appending, is ignored.
                    // this.byteOrder set in next call.
                    examineFileHeader();

                    // Oops, gotta redo this since file has different byte order
                    // than specified in constructor arg.
                    if (this.byteOrder != byteOrder) {
                        byteOrder = this.byteOrder;
                        internalBuffers[0].order(byteOrder);
                        internalBuffers[1].order(byteOrder);
                        internalBuffers[2].order(byteOrder);
                        headerBuffer.order(byteOrder);
                    }

                    // Prepare for appending by moving file position to end of last record
                    // Needs buffer to be defined and set to proper endian (which is done just above).
                    toAppendPosition();

                    // File position is now after the last event written.
                }
            }
            catch (FileNotFoundException e) {
                throw new EvioException("File could not be opened for writing, " +
                                                currentFile.getPath(), e);
            }
            catch (IOException e) {
                throw new EvioException("File could not be positioned for appending, " +
                                                currentFile.getPath(), e);
            }
        }

//System.out.println("EventWriterUnsync const: writing with order = " +byteOrder);
        // Compression threads
        if (compressionThreads == 1) {
            // When writing single threaded, just fill/compress/write one record at a time
            singleThreadedCompression = true;
            currentRecord = new RecordOutputStream(buffer, maxEventCount,
                                                   compressionType,
                                                   HeaderType.EVIO_RECORD);
        }
        else {
            // Number of ring items must be >= # of compressionThreads, plus 2 which
            // are being written, plus 1 being filled - all simultaneously.
            // ringSize = 16;
            if (ringSize < compressionThreads + 3) {
                ringSize = compressionThreads + 3;
            }

            // AND must be power of 2
            ringSize = Utilities.powerOfTwo(ringSize, true);
//System.out.println("EventWriterUnsync constr: record ring size set to " + ringSize);

            supply = new RecordSupply(ringSize, byteOrder,
                                      compressionThreads,
                                      maxEventCount, maxRecordSize,
                                      compressionType);

            // Do a quick calculation as to how much data a ring full
            // of records can hold since we may have to write that to
            // disk before we can shut off the spigot when disk is full.
            maxSupplyBytes = supply.getMaxRingBytes();

            // Number of available bytes in file's disk partition
            long freeBytes = currentFile.getParentFile().getFreeSpace();

            // If there isn't enough to accommodate 1 split of the file + full supply + 10MB extra,
            // then don't even start writing ...
            if (freeBytes < split + maxSupplyBytes + 10000000) {
//System.out.println("EventWriterUnsync constr: Disk is FULL");
                diskIsFull = true;
                diskIsFullVolatile = true;
            }

            // Create and start compression threads
            recordCompressorThreads = new RecordCompressor[compressionThreads];
            for (int i = 0; i < compressionThreads; i++) {
                recordCompressorThreads[i] = new RecordCompressor(i);
                recordCompressorThreads[i].start();
            }

            // Create and start writing thread
            recordWriterThread = new RecordWriter();
            recordWriterThread.start();

            // Get a single blank record to start writing into
            currentRingItem = supply.get();
            currentRecord   = currentRingItem.getRecord();

            // When obtained from supply, record has record number = 1.
            // This is fine in single threaded compression which sets runNumber
            // just before being written, in (try)compressAndWriteToFile.
            // But needs setting if multiple threads:
            currentRecord.getHeader().setRecordNumber(recordNumber++);
        }

        // Object to close files in a separate thread when splitting, to speed things up
        if (split > 0) {
            fileCloser = new FileCloser();
        }
    }


    //---------------------------------------------
    // BUFFER Constructors
    //---------------------------------------------

    /**
     * Create an <code>EventWriter</code> for writing events to a ByteBuffer.
     * Uses the default number and size of records in buffer.
     * Will overwrite any existing data in buffer!
     *
     * @param buf            the buffer to write to.
     * @throws EvioException if buf arg is null
     */
    public EventWriterUnsync(ByteBuffer buf) throws EvioException {

        this(buf, 0, 0, null, 1, CompressionType.RECORD_UNCOMPRESSED);
    }

    /**
     * Create an <code>EventWriter</code> for writing events to a ByteBuffer.
     * Uses the default number and size of records in buffer.
     *
     * @param buf            the buffer to write to.
     * @param xmlDictionary  dictionary in xml format or null if none.
     * @throws EvioException if buf arg is null
     */
    public EventWriterUnsync(ByteBuffer buf, String xmlDictionary) throws EvioException {

        this(buf, 0, 0, xmlDictionary, 1, CompressionType.RECORD_UNCOMPRESSED);
    }


    /**
     * Create an <code>EventWriter</code> for writing events to a ByteBuffer.
     * The buffer's position is set to 0 before writing.
     * When writing a buffer, only 1 record is used.
     * Any dictionary will be put in a commonRecord and that record will be
     * placed in the user header associated with the single record.
     *
     * @param buf             the buffer to write to starting at position = 0.
     * @param maxRecordSize   max number of data bytes each record can hold.
     *                        Value of &lt; 8MB results in default of 8MB.
     *                        The size of the record will not be larger than this size
     *                        unless a single event itself is larger.
     *                        <b>Currently this arg is unused. Only 1 record will be
     *                        used in the given buffer.</b>
     * @param maxEventCount   max number of events each record can hold.
     *                        Value &lt;= O means use default (1M).
     * @param xmlDictionary   dictionary in xml format or null if none.
     * @param recordNumber    number at which to start record number counting.
     * @param compressionType type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip)
     *
     * @throws EvioException if maxRecordSize or maxEventCount exceed limits;
     *                       if buf arg is null;
     */
    public EventWriterUnsync(ByteBuffer buf, int maxRecordSize, int maxEventCount,
                             String xmlDictionary, int recordNumber,
                             CompressionType compressionType)
            throws EvioException {

        if (buf == null) {
            throw new EvioException("Buffer arg cannot be null");
        }

        this.toFile          = false;
        this.append          = false;
        this.buffer          = buf;
        this.byteOrder       = buf.order();
        this.recordNumber    = recordNumber;
//System.out.println("EventWriterUnsync constr: record # set to " + recordNumber);

        this.xmlDictionary   = xmlDictionary;
        this.compressionType = compressionType;

        // How much compression will data experience? Percentage of original size.
        switch (compressionType) {
            case RECORD_COMPRESSION_LZ4:
                compressionFactor = 58;
                break;
            case RECORD_COMPRESSION_LZ4_BEST:
                compressionFactor = 47;
                break;
            case RECORD_COMPRESSION_GZIP:
                compressionFactor = 42;
                break;

            case RECORD_UNCOMPRESSED:
            default:
                compressionFactor = 100;
        }

        // Get buffer ready for writing
        buffer.clear();
        bufferSize = buf.capacity();
        headerBuffer.order(byteOrder);

        // Write any record containing dictionary and first event, first
        if (xmlDictionary != null) {
            createCommonRecord(xmlDictionary, null, null, null);
        }

        // When writing to buffer, just fill/compress/write one record at a time
        currentRecord = new RecordOutputStream(buf, maxEventCount,
                                               compressionType,
                                               HeaderType.EVIO_RECORD);

        RecordHeader header = currentRecord.getHeader();
        header.setBitInfo(false, xmlDictionary != null);
    }


    /**
     * Initialization new buffer (not from constructor).
     * The buffer's position is set to 0 before writing.
     * Only called by {@link #setBuffer(ByteBuffer)} and
     * {@link #setBuffer(ByteBuffer, BitSet, int)}.
     *
     * @param buf            the buffer to write to.
     * @param bitInfo        set of bits to include in first record header.
     * @param recordNumber   number at which to start record number counting.
     * @param useCurrentBitInfo   regardless of bitInfo arg's value, use the
     *                            current value of bitInfo in the reinitialized buffer.
     */
    private void reInitializeBuffer(ByteBuffer buf, BitSet bitInfo, int recordNumber,
                                    boolean useCurrentBitInfo) {

        this.buffer       = buf;
        this.byteOrder    = buf.order();
        this.recordNumber = recordNumber;

        // Init variables
        split  = 0L;
        toFile = false;
        closed = false;
        eventsWrittenTotal = 0;
        eventsWrittenToBuffer = 0;
        bytesWritten = 0L;
        headerBuffer.order(byteOrder);
        buffer.clear();
        bufferSize = buffer.capacity();

        // Deal with bitInfo
        RecordHeader header = currentRecord.getHeader();

        // This will reset the record - header and all buffers (including buf)
        currentRecord.setBuffer(buffer);

        if (!useCurrentBitInfo) {
            header.setBitInfoWord(bitInfo);
        }
//System.out.println("reInitializeBuffer: after reset, record # -> " + recordNumber);

        // Only necessary to do this when using EventWriter in EMU's
        // RocSimulation module. Only the ROC sends sourceId in header.
        header.setUserRegisterFirst(sourceId);
    }


    /**
     * If writing file, is the partition it resides on full?
     * Not full, in this context, means there's enough space to write
     * a full split file + a full record + an extra 10MB as a safety factor.
     *
     * @return true if the partition the file resides on is full, else false.
     */
    public boolean isDiskFull() {
        if (!toFile) return false;
        return diskIsFull;
    }


    /**
     * Set the buffer being written into (initially set in constructor).
     * This method allows the user to avoid having to create a new EventWriter
     * each time a bank needs to be written to a different buffer.
     * This does nothing if writing to a file.<p>
     * Do <b>not</b> use this method unless you know what you are doing.
     *
     * @param buf the buffer to write to.
     * @param bitInfo        set of bits to include in first record header.
     * @param recordNumber   number at which to start record number counting.
     * @throws EvioException if this object was not closed prior to resetting the buffer,
     *                       buffer arg is null, or writing to file.
     */
    public void setBuffer(ByteBuffer buf, BitSet bitInfo, int recordNumber) throws EvioException {
        if (toFile) return;
        if (buf == null) {
            throw new EvioException("Buffer arg null");
        }
        if (!closed) {
            throw new EvioException("Close EventWriter before changing buffers");
        }

//System.out.println("setBuffer: call reInitializeBuf with INPUT record # " + recordNumber);
        reInitializeBuffer(buf, bitInfo, recordNumber, false);
    }


    /**
     * Set the buffer being written into (initially set in constructor).
     * This method allows the user to avoid having to create a new EventWriter
     * each time a bank needs to be written to a different buffer.
     * This does nothing if writing to a file.<p>
     * Do <b>not</b> use this method unless you know what you are doing.
     *
     * @param buf the buffer to write to.
     * @throws EvioException if this object was not closed prior to resetting the buffer,
     *                       buffer arg is null, or writing to file.
     */
    public void setBuffer(ByteBuffer buf) throws EvioException {
        if (toFile) return;
        if (buf == null) {
            throw new EvioException("Buffer arg null");
        }
        if (!closed) {
            throw new EvioException("Close EventWriter before changing buffers");
        }

//System.out.println("setBuffer: call reInitializeBuf with local record # " + recordNumber);
        reInitializeBuffer(buf, null, recordNumber, true);
    }


    /**
     * Get the buffer being written into.
     * If writing to a buffer, this was initially supplied by user in constructor.
     * If writing to a file, return null.
     * Although this method may seems useful, it requires a detailed knowledge of
     * this class's internals. The {@link #getByteBuffer()} method is much more
     * useful to the user.
     *
     * @return buffer being written into
     */
    private ByteBuffer getBuffer() {
        if (toFile()) return null;
        return buffer;
    }


    /**
     * If writing to a file, return null.
     * If writing to a buffer, get a duplicate of the user-given buffer
     * being written into. The buffer's position will be 0 and its
     * limit will be the size of the valid data. Basically, it's
     * ready to be read from. The returned buffer shares data with the
     * original buffer but has separate limit, position, and mark.
     * Useful if trying to send buffer over the network.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or writeEvent.
     *
     * @return buffer being written into, made ready for reading;
     *         null if writing to file
     */
    public ByteBuffer getByteBuffer() {
        // It does NOT make sense to give the caller the internal buffer
        // used in writing to files. That buffer may contain nothing and
        // most probably won't contain the full file contents.
        if (toFile()) return null;

        ByteBuffer buf = buffer.duplicate().order(buffer.order());
        buf.limit((int)bytesWritten);

        // Get buffer ready for reading
        buf.flip();
        return buf;
    }


    /**
     * <p>
     * Increase internal buffer size when writing to a file with a single
     * compression thread. Design to accommodate a single event which
     * exceeds the current memory limit. This happens automatically for
     * multiple compression threads, but with only one such thread,
     * code get excercised in the constructor in which
     * this object creates buffers "by hand" and passes one to the constructor
     * for RecordOutputStream. Since it's not created by the constructor, it
     * cannot be automatically increased in size for a large event. Thus, we do
     * it here instead.</p>
     *
     * This method can only be called if all events have been written to file
     * and so record is empty.
     *
     * @param bytes size in bytes of new internal buffers.
     */
    private void expandInternalBuffers(int bytes) {

        if ((bytes <= internalBufSize) || !toFile || !singleThreadedCompression) {
            return;
        }

        // Give it 20% more than asked for
        internalBufSize = bytes / 10 * 12;

        internalBuffers[0] = ByteBuffer.allocateDirect(internalBufSize);
        internalBuffers[1] = ByteBuffer.allocateDirect(internalBufSize);
        internalBuffers[2] = ByteBuffer.allocateDirect(internalBufSize);
        internalBuffers[0].order(byteOrder);
        internalBuffers[1].order(byteOrder);
        internalBuffers[2].order(byteOrder);
        buffer = internalBuffers[0];

        if (future1 != null) {
            try {
                // Wait for last write to end before we continue
                future1.get();
            }
            catch (Exception e) {}
        }

        if (future2 != null) {
            try {
                future2.get();
            }
            catch (Exception e) {}
        }

        // Start over
        future1 = null;
        future2 = null;
        usedBuffers[0] = null;
        usedBuffers[1] = null;

        currentRecord.setBuffer(buffer);

//        System.out.println("expandInternalBuffers: buffer cap = " + buffer.capacity() +
//                ", lim = " + buffer.limit() + ", pos = " + buffer.position());
    }


    /**
     * Set the value of the source Id in the first block header.
     * Only necessary to do this when using EventWriter in EMU's
     * RocSimulation module. Only the ROC sends sourceId in header.
     * In evio 6, the source id is stored in user register 1. In
     * earlier versions it's stored in reserved1.
     * This should only be used internally by CODA in emu software.
     *
     * @param sourceId  value of the source Id.
     */
    public void setSourceId(int sourceId) {
        this.sourceId = sourceId;
        RecordHeader header = currentRecord.getHeader();
        header.setUserRegisterFirst(sourceId);
    }


    /**
     * Set the bit info of a record header for a specified CODA event type.
     * Must be called AFTER {@link RecordHeader#setBitInfo(boolean, boolean)} or
     * {@link RecordHeader#setBitInfoWord(int)} in order to have change preserved.
     * This should only be used internally by CODA in emu software.
     *
     * @param type event type (0=ROC raw, 1=Physics, 2=Partial Physics,
     *             3=Disentangled, 4=User, 5=Control, 15=Other,
     *             else = nothing set).
     */
    public void setEventType(int type) {
        RecordHeader header = currentRecord.getHeader();
        header.setBitInfoEventType(type);
    }


    /**
     * Is this object writing to file?
     * @return {@code true} if writing to file, else {@code false}.
     */
    public boolean toFile() {return toFile;}


    /**
     * Has {@link #close()} been called (without reopening by calling
     * {@link #setBuffer(ByteBuffer)}) ?
     *
     * @return {@code true} if this object closed, else {@code false}.
     */
    public boolean isClosed() {return closed;}


    /**
     * Get the name of the current file being written to.
     * Returns null if no file.
     * @return the name of the current file being written to.
     */
    public String getCurrentFilename() {
        if (currentFile != null) {
            return currentFile.getName();
        }
        return null;
    }


    /**
     * If writing to a buffer, get the number of bytes written to it
     * including the trailer.
     * @return number of bytes written to buffer
     */
    public int getBytesWrittenToBuffer() {return (int)bytesWritten;}


    /**
     * Get the full name or path of the current file being written to.
     * Returns null if no file.
     * @return the full name or path of the current file being written to.
     */
    public String getCurrentFilePath() {
        if (currentFile != null) {
            return currentFile.getPath();
        }
        return null;
    }


    /**
     * Get the current split number which is the split number of file
     * to be written next. Warning, this value may be changing.
     * @return the current split number which is the split number of file to be written next.
     */
    public int getSplitNumber() {return splitNumber;}


    /**
     * Get the number of split files produced by this writer.
     * @return number of split files produced by this writer.
     */
    public int getSplitCount() {return splitCount;}


    /**
     * Get the current block (record) number.
     * Warning, this value may be changing.
     * @return the current block (record) number.
     */
    public int getBlockNumber() {return recordNumber;}


    /**
     * Get the current record number.
     * Warning, this value may be changing.
     * @return the current record number.
     */
    public int getRecordNumber() {return recordNumber;}


    /**
     * Get the number of events written to a file/buffer.
     * Remember that a particular event may not yet be
     * flushed to the file/buffer.
     * If the file being written to is split, the returned
     * value refers to all split files taken together.
     *
     * @return number of events written to a file/buffer.
     */
    public int getEventsWritten() {
//        System.out.println("getEventsWritten: eventsWrittenTotal = " + eventsWrittenTotal +
//                ", curRec.getEvCount = " + currentRecord.getEventCount());
//        return eventsWrittenTotal + currentRecord.getEventCount();
        return eventsWrittenTotal;
    }


    /**
     * Get the byte order of the buffer/file being written into.
     * @return byte order of the buffer/file being written into.
     */
    public ByteOrder getByteOrder() {return byteOrder;}


    /**
     * Set the number with which to start block (record) numbers.
     * This method does nothing if events have already been written.
     * @param startingRecordNumber  the number with which to start block
     *                              (record) numbers.
     */
    public void setStartingBlockNumber(int startingRecordNumber) {
        // If events have been written already, forget about it
        if (eventsWrittenTotal > 0) return;
        recordNumber = startingRecordNumber;
//System.out.println("setStartingBLOCKNumber: set to " + recordNumber);
    }


    /**
     * Set the number with which to start record numbers.
     * This method does nothing if events have already been written.
     * @param startingRecordNumber  the number with which to start record numbers.
     */
    public void setStartingRecordNumber(int startingRecordNumber) {
        // If events have been written already, forget about it
        if (eventsWrittenTotal > 0) return;
        recordNumber = startingRecordNumber;
//System.out.println("setStartingRecordNumber: set to " + recordNumber);
    }


    /**
     * Set an event which will be written to the file as
     * well as to all split files. It's called the "first event" as it will be the
     * first event written to each split file if this method
     * is called early enough or the first event was defined in the constructor.
     * In evio version 6, any dictionary and the first event are written to a
     * common record which is stored in the user-header part of the file header if
     * writing to a file. The common record data is never compressed.<p>
     *
     * <b>FILE:</b>
     * Since this method can only be called after the constructor, the common record may
     * have already been written with its dictionary and possibly another firstEvent.
     * If that is the case, the event given here will be written immediately somewhere
     * in the body of the file. Any subsequent splits will have this event as the first
     * event in the file header. On the other hand, if the common record has not yet been
     * written to the file, this event becomes the first event in the file header.<p>
     *
     * <b>BUFFER:</b>
     * By its nature this method is not all that useful for writing to a buffer since
     * the buffer is never split. For that reason it throws an exception.<p>
     *
     * Do not call this while simultaneously calling
     * close, flush, writeEvent, or getByteBuffer.
     *
     * @param node node representing event to be placed first in each file written
     *             including all splits. If null, no more first events are written
     *             to any files.
     * @throws IOException   if error writing to file
     * @throws EvioException if first event is opposite byte order of internal buffer;
     *                       if bad data format;
     *                       if close() already called;
     *                       if writing to buffer;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    public void setFirstEvent(EvioNode node) throws EvioException, IOException {

        if (closed) {return;}

        // If there is no common record now ...
        if (node == null && xmlDictionary == null) {
            commonRecord = null;
            return;
        }

        if (!toFile) {
            throw new EvioException("cannot write first event to buffer");
        }

        // There's no way to remove an event from a record, so reconstruct it.
        createCommonRecord(xmlDictionary, null, node, null);

        if ((recordsWritten > 0) && (node != null)) {
            // If we've already written the file header, it's too late to place
            // the common record there, so write first event as a regular event.
            // The new common record will be written to the file header of the
            // next split.
            writeEvent(node, false);
        }
    }


    /**
     * Set an event which will be written to the file as
     * well as to all split files. It's called the "first event" as it will be the
     * first event written to each split file if this method
     * is called early enough or the first event was defined in the constructor.
     * In evio version 6, any dictionary and the first event are written to a
     * common record which is stored in the user-header part of the file header if
     * writing to a file. When writing to a buffer it's stored in the first record's
     * user-header. The common record data is never compressed.<p>
     *
     * <b>FILE:</b>
     * Since this method can only be called after the constructor, the common record may
     * have already been written with its dictionary and possibly another firstEvent.
     * If that is the case, the event given here will be written immediately somewhere
     * in the body of the file. Any subsequent splits will have this event as the first
     * event in the file header. On the other hand, if the common record has not yet been
     * written to the file, this event becomes the first event in the file header.<p>
     *
     * <b>BUFFER:</b>
     * By its nature this method is not all that useful for writing to a buffer since
     * the buffer is never split. For that reason it throws an exception.<p>
     *
     * Do not call this while simultaneously calling
     * close, flush, writeEvent, or getByteBuffer.
     *
     * @param buffer buffer containing event to be placed first in each file written
     *               including all splits. If null, no more first events are written
     *               to any files.
     * @throws IOException   if error writing to file
     * @throws EvioException if first event is opposite byte order of internal buffer;
     *                       if bad data format;
     *                       if close() already called;
     *                       if writing to buffer;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    public void setFirstEvent(ByteBuffer buffer) throws EvioException, IOException {

        if (closed) {return;}

        if ((buffer == null || buffer.remaining() < 8) && xmlDictionary == null) {
            commonRecord = null;
            return;
        }

        if (!toFile) {
            throw new EvioException("cannot write first event to buffer");
        }

        createCommonRecord(xmlDictionary, null, null, buffer);

        if ((recordsWritten > 0) && (buffer != null) && (buffer.remaining() > 7)) {
            writeEvent(buffer, false, false);
        }
    }


    /**
     * Set an event which will be written to the file as
     * well as to all split files. It's called the "first event" as it will be the
     * first event written to each split file if this method
     * is called early enough or the first event was defined in the constructor.
     * In evio version 6, any dictionary and the first event are written to a
     * common record which is stored in the user-header part of the file header if
     * writing to a file. When writing to a buffer it's stored in the first record's
     * user-header. The common record data is never compressed.<p>
     *
     * <b>FILE:</b>
     * Since this method can only be called after the constructor, the common record may
     * have already been written with its dictionary and possibly another firstEvent.
     * If that is the case, the event given here will be written immediately somewhere
     * in the body of the file. Any subsequent splits will have this event as the first
     * event in the file header. On the other hand, if the common record has not yet been
     * written to the file, this event becomes the first event in the file header.<p>
     *
     * <b>BUFFER:</b>
     * By its nature this method is not all that useful for writing to a buffer since
     * the buffer is never split. For that reason it throws an exception.<p>
     *
     * Do not call this while simultaneously calling
     * close, flush, writeEvent, or getByteBuffer.
     *
     * @param bank event to be placed first in each file written including all splits.
     *             If null, no more first events are written to any files.
     * @throws IOException   if error writing to file
     * @throws EvioException if first event is opposite byte order of internal buffer;
     *                       if bad data format;
     *                       if close() already called;
     *                       if writing to buffer;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    public void setFirstEvent(EvioBank bank) throws EvioException, IOException {

        if (closed) {return;}

        if (bank == null && xmlDictionary == null) {
            commonRecord = null;
            return;
        }

        if (!toFile) {
            throw new EvioException("cannot write first event to buffer");
        }

        createCommonRecord(xmlDictionary, bank, null, null);

        if ((recordsWritten > 0) && (bank != null)) {
            writeEvent(bank, null, false, false);
        }
    }


    /**
     * Create and fill the common record which contains the dictionary and first event.
     * Use the firstBank as the first event if specified, else try using the
     * firstNode if specified, else try the firstBuf.
     *
     * @param xmlDictionary  xml dictionary
     * @param firstBank      first event as EvioBank
     * @param firstNode      first event as EvioNode
     * @param firstBuf       first event as ByteBuffer
     * @throws EvioException if dictionary is in improper format
     */
    private void createCommonRecord(String xmlDictionary, EvioBank firstBank,
                                    EvioNode firstNode, ByteBuffer firstBuf)
                            throws EvioException {

        // Create record if necessary, else clear it
        if (commonRecord == null) {
            // No compression please ...
            commonRecord = new RecordOutputStream(byteOrder, 0, 0, CompressionType.RECORD_UNCOMPRESSED);
        }
        else {
            commonRecord.reset();
        }

        // Place dictionary & first event into a single record
        if (xmlDictionary != null) {
            // 56 is the minimum number of characters for a valid xml dictionary
            if (xmlDictionary.length() < 56) {
                throw new EvioException("Dictionary improper format");
            }

            try {
                // Turn dictionary data into ascii (not evio bank)
                dictionaryByteArray = xmlDictionary.getBytes("US-ASCII");
            } catch (UnsupportedEncodingException e) {/* never happen */}

            // Add to record which will be our file header's "user header"
            commonRecord.addEvent(dictionaryByteArray);
        }
        else {
            dictionaryByteArray = null;
        }

        // Convert first event into bytes
        if (firstBank != null) {
            firstEventByteArray = Utilities.bankToBytes(firstBank, byteOrder);
            // Add to record which will be our file header's "user header"
            commonRecord.addEvent(firstEventByteArray);
        }
        else if (firstNode != null) {
            ByteBuffer firstEventBuf = firstNode.getStructureBuffer(true);
            firstEventByteArray = firstEventBuf.array();
            commonRecord.addEvent(firstEventByteArray);
        }
        else if (firstBuf != null) {
            firstEventByteArray = ByteDataTransformer.toByteArray(firstBuf);
            commonRecord.addEvent(firstEventByteArray);
        }
        else {
            firstEventByteArray = null;
        }

        commonRecord.build();
        commonRecordBytesToBuffer = 4*commonRecord.getHeader().getLengthWords();
    }


    /**
     * Create and write a general file header into the file.
     * The general header's user header is the common record which
     * contains any existing dictionary and first event.<p>
     *
     * Call this method AFTER file split or, in constructor, after the file
     * name is created in order to ensure a consistent value for file split number.
     */
    private void writeFileHeader() {
        // For the file header, our "user header" will be the common record,
        // which is a record containing the dictionary and first event.

        fileHeader.reset();
        // File split # in header. Go back to last one as currently is set for the next split.
        fileHeader.setFileNumber(splitNumber - splitIncrement);

        // Check to see if we have dictionary and/or first event
        int commonRecordBytes = 0;
        int commonRecordCount = 0;

        if (commonRecord != null) {
            commonRecordCount = commonRecord.getEventCount();
            if (commonRecordCount > 0) {
                commonRecordBytes = commonRecord.getHeader().getLength();
                boolean haveDict = dictionaryByteArray != null;
                boolean haveFE   = firstEventByteArray != null;
                fileHeader.setBitInfo(haveFE, haveDict, false);
            }
            // Sets file header length too
            fileHeader.setUserHeaderLength(commonRecordBytes);
        }

        // Index array is unused

        // Total header size in bytes
        int bytes = fileHeader.getLength();
        byte[] array = new byte[bytes];
        ByteBuffer buf = ByteBuffer.wrap(array);
        buf.order(byteOrder);

        // Write file header into array
        try {
            fileHeader.writeHeader(buf, 0);
        }
        catch (HipoException e) {/* never happen */}

        // Write user header into array if necessary
        if (commonRecordBytes > 0) {
            ByteBuffer commonBuf = commonRecord.getBinaryBuffer();
            byte[] commonArray = commonBuf.array();
            if (commonArray != null) {
                System.arraycopy(commonArray, commonBuf.arrayOffset(),
                                 array, FileHeader.HEADER_SIZE_BYTES, commonRecordBytes);
            }
            else {
                commonBuf.get(array, FileHeader.HEADER_SIZE_BYTES, commonRecordBytes);
            }
        }

        // Write array into file
        asyncFileChannel.write(buf, 0);

        eventsWrittenTotal = eventsWrittenToFile = commonRecordCount;
        bytesWritten = bytes;
        fileWritingPosition += bytes;
    }


    /**
     * This method flushes any remaining internally buffered data to file.
     * Calling {@link #close()} automatically does this so it isn't necessary
     * to call before closing. This method should only be used when writing
     * events at such a low rate that it takes an inordinate amount of time
     * for internally buffered data to be written to the file.<p>
     *
     * Calling this may easily kill performance. May not call this when simultaneously
     * calling writeEvent, close, setFirstEvent, or getByteBuffer.
     */
    public void flush() {

        if (closed) {
            return;
        }

        if (toFile) {
            if (singleThreadedCompression) {
                try {
                    compressAndWriteToFile(true);
                }
                catch (Exception e) {
                    e.printStackTrace();
                }
            }
            else {
                // Write any existing data.
                currentRingItem.forceToDisk(true);
                if (currentRecord.getEventCount() > 0) {
                    // Send current record back to ring
                    supply.publish(currentRingItem);
                }

                // Get another empty record from ring
                currentRingItem = supply.get();
                currentRecord = currentRingItem.getRecord();
            }
        }
        else {
            flushCurrentRecordToBuffer();
        }
    }


    /**
     * This method flushes any remaining data to file and disables this object.
     * May not call this when simultaneously calling
     * writeEvent, flush, setFirstEvent, or getByteBuffer.
     */
    public void close() {
        if (closed) {
            return;
        }

        // If buffer ...
        if (!toFile) {
            flushCurrentRecordToBuffer();
            // Write empty last header
            try {
// System.out.println("EventWriterUnsync: writing trailer to buffer");
                writeTrailerToBuffer(addTrailerIndex);
            }
            catch (EvioException e) {
                // We're here if buffer is too small
                e.printStackTrace();
            }
        }
        // If file ...
        else {
            // Write record to file
            if (singleThreadedCompression) {
                if (currentRecord.getEventCount() > 0) {
                    try {
                        compressAndWriteToFile(false);
                    }
                    catch (Exception e) {
                        e.printStackTrace();
                    }
                }
            }
            else {
                // If we're building a record, send it off
                // to compressing thread since we're done.
                // This should never happen as END event forces things through.
                if (currentRecord.getEventCount() > 0) {
                    // Put it back in supply for compressing and force to disk
                    supply.publish(currentRingItem);
                }

                // Since the writer thread is the last to process each record,
                // wait until it's done with the last item, then exit the thread.
//System.out.println("EventWriterUnsync: close waiting for writing thd");
                recordWriterThread.waitForLastItem();
//System.out.println("EventWriterUnsync: close done waiting for writing thd");

                // Stop all compressing threads which by now are stuck on get
                for (RecordCompressor rc : recordCompressorThreads) {
//System.out.println("EventWriterUnsync: interrupt compress thd");
                    rc.interrupt();
                }
            }

            // Write trailer
            if (addingTrailer) {
                // Write the trailer
                try {
                    writeTrailerToFile(addTrailerIndex);
                }
                catch (IOException e) {
                    e.printStackTrace();
                }
            }

            try {
                // Find & update file header's record count word
                ByteBuffer bb = ByteBuffer.allocate(4);
                bb.order(byteOrder);
                bb.putInt(0, recordNumber - 1);
                Future<Integer> future = asyncFileChannel.write(bb, FileHeader.RECORD_COUNT_OFFSET);
                future.get();
            }
            catch (Exception e) {
                e.printStackTrace();
            }

            // Finish writing to current file
            try {
                if (future1 != null) {
                    try {
                        // Wait for last write to end before we continue
                        future1.get();
                    }
                    catch (Exception e) {}
                }

                if (future2 != null) {
                    try {
                        future2.get();
                    }
                    catch (Exception e) {}
                }

                if (asyncFileChannel != null) asyncFileChannel.close();
                // Close split file handler thread pool
                if (fileCloser != null) fileCloser.close();
            }
            catch (IOException e) {}

            // Since the supply takes a lot of mem, allow for GC to operate here
            supply = null;
            currentRecord = null;
            recordWriterThread = null;
            recordCompressorThreads = null;
            ringItem1 = null;
            ringItem2 = null;
            currentRingItem = null;
       }

       recordLengths.clear();
       closed = true;
    }


    /**
     * Reads part of the file header in order to determine
     * the evio version # and endianness of the file in question.
     *
     * @throws EvioException not in append mode, contains too little data, is not in proper format,
     *                       version earlier than 6, and all other exceptions.
     * @throws IOException   premature EOF or file reading error.
     */
    protected void examineFileHeader() throws IOException, EvioException {

        // Only for append mode - only used for files
        if (!append) {
            // Internal logic error, should never have gotten here
            throw new EvioException("need to be in append mode");
        }

        int nBytes;

        buffer.clear();
        buffer.limit(FileHeader.HEADER_SIZE_BYTES);

        Future<Integer> f = asyncFileChannel.read(buffer, 0L);
        try {
            nBytes = f.get();
        }
        catch (Exception e) {
            throw new IOException(e);
        }

        // Check to see if we read the whole header
        if (nBytes != FileHeader.HEADER_SIZE_BYTES) {
            throw new EvioException("bad file format");
        }

        try {
            // Parse header info
            appendFileHeader = new FileHeader();
            buffer.position(0);
            // Buffer's position/limit does not change
            appendFileHeader.readHeader(buffer);

            // Set the byte order to match the buffer/file's ordering.
            byteOrder = appendFileHeader.getByteOrder();

            hasAppendDictionary = appendFileHeader.hasDictionary();
            hasTrailerWithIndex = appendFileHeader.hasTrailerWithIndex();
            indexLength         = appendFileHeader.getIndexLength();
            userHeaderLength    = appendFileHeader.getUserHeaderLength();
            userHeaderPadding   = appendFileHeader.getUserHeaderLengthPadding();

//            int fileID      = appendFileHeader.getFileId();
//            int headerLen   = appendFileHeader.getHeaderLength();
//            int recordCount = appendFileHeader.getEntries();
//            int fileNum     = appendFileHeader.getFileNumber();
//
//            System.out.println("file ID          = " + fileID);
//            System.out.println("fileNumber       = " + fileNum);
//            System.out.println("headerLength     = " + headerLen);
//            System.out.println("recordCount      = " + recordCount);
//            System.out.println("trailer index    = " + hasTrailerWithIndex);
//            System.out.println("bit info         = " + Integer.toHexString(bitInfo));
//            System.out.println();
        }
        catch (Exception a) {
            throw new EvioException(a);
        }
    }


    /**
     * This method positions a file for the first {@link #writeEvent(EvioBank)}
     * in append mode. It places the writing position after the last event (not record header).
     *
     * @throws IOException     if file reading/writing problems
     * @throws EvioException   if bad file/buffer format; if not in append mode
     */
    private void toAppendPosition() throws EvioException, IOException {

        // Only for append mode
        if (!append) {
            throw new EvioException("need to be in append mode");
        }

        // Jump over the file header, index array, and user header & padding.
        // This puts us at the beginning of the first record header
        fileWritingPosition = FileHeader.HEADER_SIZE_BYTES + indexLength +
                              userHeaderLength + userHeaderPadding;

        long fileSize = asyncFileChannel.size();

        boolean lastRecord, isTrailer, readEOF = false;
        int recordLen, eventCount, nBytes, bitInfo, headerPosition;
        Future<Integer> future;

        long bytesLeftInFile = fileSize;

//        int indexArrayLen, userHeaderLen, compDataLen, unCompDataLen;
//        int userPadding, dataPadding, compPadding;
//        int headerLen, currentPosition, compType, compWord;

        // The file's record #s may be fine or they may be messed up.
        // Assume they start with one and increment from there. That
        // way any additional records now added to the file will have a
        // reasonable # instead of incrementing from the last existing
        // record.
        recordNumber = 1;
//        System.out.println("toAppendPos:     record # = 1");

        // To read in all of the normal record header set this to 40 bytes.
        // To read the bare minimum to do the append set this to 24 bytes,
        // but be sure to comment out lines reading beyond this point in the header.
        int headerBytesToRead = 40;

        while (true) {
            nBytes = 0;

            // Read in most of the normal record header, 40 bytes.
            // Skip the last 16 bytes which are only 2 user registers.
            buffer.clear();
            buffer.limit(headerBytesToRead);

            while (nBytes < headerBytesToRead) {
                // There is no internal asyncFileChannel position
                int partial;

                future = asyncFileChannel.read(buffer, fileWritingPosition);
                try {
                    partial = future.get();
                }
                catch (Exception e) {
                    throw new IOException(e);
                }

                // If EOF ...
                if (partial < 0) {
                    if (nBytes != 0) {
                        throw new EvioException("bad buffer format");
                    }
                    // Missing last empty record header
                    readEOF = true;
                    break;
                }
                nBytes += partial;
                bytesLeftInFile -= partial;
            }

            // If we did not read correct # of bytes or didn't run into EOF right away
            if (nBytes != 0 && nBytes != headerBytesToRead) {
                throw new EvioException("internal file reading error");
            }

            headerPosition = 0;
            fileWritingPosition += headerBytesToRead;

            bitInfo    = buffer.getInt(headerPosition + RecordHeader.BIT_INFO_OFFSET);
            recordLen  = buffer.getInt(headerPosition + RecordHeader.RECORD_LENGTH_OFFSET);
            eventCount = buffer.getInt(headerPosition + RecordHeader.EVENT_COUNT_OFFSET);
            lastRecord = RecordHeader.isLastRecord(bitInfo);
            isTrailer  = RecordHeader.isEvioTrailer(bitInfo);

////          If reading entire header, change headerBytesToRead from 24 to 40
//            headerLen     = buffer.getInt(headerPosition + RecordHeader.HEADER_LENGTH_OFFSET);
//            userHeaderLen = buffer.getInt(headerPosition + RecordHeader.USER_LENGTH_OFFSET);
//            indexArrayLen = buffer.getInt(headerPosition + RecordHeader.INDEX_ARRAY_OFFSET);
//            unCompDataLen = buffer.getInt(headerPosition + RecordHeader.UNCOMPRESSED_LENGTH_OFFSET);
//
//            compWord      = buffer.getInt(headerPosition + RecordHeader.COMPRESSION_TYPE_OFFSET);
//            compType = compWord >>> 28;
//            // If there is compression ...
//            if (compType != 0) {
//                compDataLen = compWord & 0xfffffff;
//            }
//
//            System.out.println("bitInfo      = 0x" + Integer.toHexString(bitInfo));
//            System.out.println("recordLength = " + recordLen);
//            System.out.println("headerLength = " + headerLen);
//            System.out.println("eventCount   = " + eventCount);
//            System.out.println("lastRecord   = " + lastRecord);
//            System.out.println();

            if (!isTrailer) {
                // Update vector with record size & event count
                recordLengths.add(4 * recordLen);
                recordLengths.add(eventCount);
            }

            // Track total number of events in file/buffer (minus dictionary)
            eventsWrittenTotal += eventCount;

            recordNumber++;
//System.out.println("                 record # = " + recordNumber);

            // Stop at the last record. The file may not have a last record if
            // improperly terminated. Running into an End-Of-File will flag
            // this condition.
            if (isTrailer || lastRecord || readEOF) {
                break;
            }

            // Hop to next record header
            int bytesToNextBlockHeader = 4*recordLen - headerBytesToRead;
            if (bytesLeftInFile < bytesToNextBlockHeader) {
                throw new EvioException("bad file format");
            }
            fileWritingPosition += bytesToNextBlockHeader;
            bytesLeftInFile     -= bytesToNextBlockHeader;
        }

        if (hasAppendDictionary) {
            eventsWrittenToFile = eventsWrittenToBuffer = eventsWrittenTotal + 1;
        }
        else {
            eventsWrittenToFile = eventsWrittenToBuffer = eventsWrittenTotal;
        }

        //-------------------------------------------------------------------------------
        // If we're here, we've just read the last record header (at least 6 words of it).
        // File position is just past header, but buffer position is just before it.
        // Either that or we ran into end of file (last record header missing).
        //
        // If EOF, last record header missing, we're good.
        //
        // Else check to see if the last record contains data. If it does,
        // change a single bit so it's not labeled as the last record,
        // then jump past all data.
        //
        // Else if there is no data, position file before it as preparation for writing
        // the next record.
        //-------------------------------------------------------------------------------

        // If no last, empty record header in file ...
        if (readEOF) {
            // It turns out we need to do nothing. The constructor that
            // calls this method will write out the next record header.
            recordNumber--;
//System.out.println("                 record # = " + recordNumber);
        }
        // else if last record or has NO data in it ...
        else if (isTrailer || eventCount < 1) {
            // We already partially read in the record header, now back up so we can overwrite it.
            // If using buffer, we never incremented the position, so we're OK.
            recordNumber--;
//System.out.println("                 record # = " + recordNumber);
            fileWritingPosition -= headerBytesToRead;
//System.out.println("toAppendPos: position (bkup) = " + fileWritingPosition);
        }
        // If last record has event(s) in it ...
        else {
            // Clear last record bit in 6th header word
            bitInfo = RecordHeader.clearLastRecordBit(bitInfo);

            // Rewrite header word with new bit info & hop over record

            // File now positioned right after the last header to be read
            // Back up to before 6th block header word
            fileWritingPosition -= headerBytesToRead - RecordHeader.BIT_INFO_OFFSET;
//System.out.println("toAppendPosition: writing over last block's 6th word, back up %d words" +(8 - 6));

            // Write over 6th block header word
            buffer.clear();
            buffer.putInt(bitInfo);
            buffer.flip();

            //fileChannel.write(buffer);
            future = asyncFileChannel.write(buffer, fileWritingPosition);
            try {
                future.get();
            }
            catch (Exception e) {
                throw new IOException(e);
            }

            // Hop over the entire block
//System.out.println("toAppendPosition: wrote over last block's 6th word, hop over %d words" +
//                   (recordLen - (6 + 4)));
            fileWritingPosition += (4 * recordLen) - (RecordHeader.BIT_INFO_OFFSET + 4);
        }

//System.out.println("toAppendPos: file pos = " + fileWritingPosition);
        bytesWritten = fileWritingPosition;
        recordsWritten = recordNumber - 1;

        // We should now be in a state identical to that if we had
        // just now written everything currently in the file/buffer.
        buffer.clear();
    }


    /**
     * Is there room to write this many bytes to an output buffer as a single event?
     * Will always return true when writing to a file.
     * @param bytes number of bytes to write
     * @return {@code true} if there still room in the output buffer, else {@code false}.
     */
    public boolean hasRoom(int bytes) {
//System.out.println("User buffer size (" + currentRecord.getInternalBufferCapacity() + ") - bytesWritten (" + bytesWritten +
//      ") - trailer (" + trailerBytes() +  ") = (" +
//         ((currentRecord.getInternalBufferCapacity() - bytesWritten) >= bytes + RecordHeader.HEADER_SIZE_BYTES) +
//      ") >= ? " + bytes);
        return toFile() || ((currentRecord.getInternalBufferCapacity() -
                             bytesWritten - trailerBytes()) >= bytes);
    }

    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file (for multiple compression
     * threads), the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
     * If writing to a buffer, once the record is full this method returns
     * false - indicating that the last event was NOT written to the record.
     * To finish the writing process, call {@link #close()}. This will
     * compress the data if desired and then write it to the buffer.<p>
     *
     * The buffer must contain only the event's data (event header and event data)
     * and must <b>not</b> be in complete evio file format.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     *<b>kill</b> performance when writing to a file.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param node       object representing the event to write in buffer form
     * @param force      if writing to disk, force it to write event to the disk.
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         record event count limit exceeded, or bank and bankBuffer args are both null.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if null node arg;
     */
    public boolean writeEvent(EvioNode node, boolean force) throws EvioException, IOException {
        // Duplicate buffer so we can set pos & limit without messing others up
        return writeEvent(node, force, true, false);
    }


    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file (for multiple compression
     * threads), the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
     * If writing to a buffer, once the record is full this method returns
     * false - indicating that the last event was NOT written to the record.
     * To finish the writing process, call {@link #close()}. This will
     * compress the data if desired and then write it to the buffer.<p>
     *
     * The buffer must contain only the event's data (event header and event data)
     * and must <b>not</b> be in complete evio file format.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     *<b>kill</b> performance when writing to a file.
     * A true 3rd arg can be used when the backing buffer
     * of the node is accessed by multiple threads simultaneously. This allows
     * that buffer's limit and position to be changed without interfering
     * with the other threads.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param node       object representing the event to write in buffer form
     * @param force      if writing to disk, force it to write event to the disk.
     * @param duplicate  if true, duplicate node's buffer so its position and limit
     *                   can be changed without issue.
     *
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         record event count limit exceeded, or bank and bankBuffer args are both null.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if null node arg;
     */
    public boolean writeEvent(EvioNode node, boolean force, boolean duplicate)
            throws EvioException, IOException {
        return writeEvent(node, force, true, false);
    }


   /**
    * Write an event (bank) into a record in evio/hipo version 6 format.
    * Once the record is full and if writing to a file (for multiple compression
    * threads), the record will be sent to a thread which may compress the data,
    * then it will be sent to a thread to write the record to file.
    * If there is only 1 compression thread, it's all done in the thread which
    * call this method.<p>
    * If writing to a buffer, once the record is full this method returns
    * false - indicating that the last event was NOT written to the record.
    * To finish the writing process, call {@link #close()}. This will
    * compress the data if desired and then write it to the buffer.<p>
    *
    * The buffer must contain only the event's data (event header and event data)
    * and must <b>not</b> be in complete evio file format.
    * Do not call this while simultaneously calling
    * close, flush, setFirstEvent, or getByteBuffer.<p>
    *
    * Be warned that injudicious use of a true 2nd arg, the force flag, will
    *<b>kill</b> performance when writing to a file.
    * A true 3rd arg can be used when the backing buffer
    * of the node is accessed by multiple threads simultaneously. This allows
    * that buffer's limit and position to be changed without interfering
    * with the other threads.<p>
    *
    * This method is not used to write the dictionary or the first event
    * which are both placed in the common record which, in turn, is the
    * user header part of the file header.<p>
    *
    * @param node       object representing the event to write in buffer form
    * @param force      if writing to disk, force it to write event to the disk.
    * @param duplicate  if true, duplicate node's buffer so its position and limit
    *                   can be changed without issue.
    * @param ownRecord  if true, write event in its own record regardless
    *                   of event count and record size limits.
    *
    * @return if writing to buffer: true if event was added to record, false if buffer full,
    *         record event count limit exceeded, or bank and bankBuffer args are both null.
    *
    * @throws IOException   if error writing file
    * @throws EvioException if event is opposite byte order of internal buffer;
    *                       if close() already called;
    *                       if bad eventBuffer format;
    *                       if file could not be opened for writing;
    *                       if file exists but user requested no over-writing;
    *                       if null node arg;
    */
    public boolean writeEvent(EvioNode node, boolean force, boolean duplicate, boolean ownRecord)
            throws EvioException, IOException {

        if (node == null) {
            throw new EvioException("null node arg");
        }

//        int origLim=0,origPos=0;
        ByteBuffer eventBuffer, bb = node.getBuffer();

        // Duplicate buffer so we can set pos & limit without messing others up
        if (duplicate) {
            eventBuffer = bb.duplicate().order(bb.order());
        }
        else {
            eventBuffer = bb;
//            origLim = bb.limit();
//            origPos = bb.position();
        }

        int pos = node.getPosition();
        eventBuffer.limit(pos + node.getTotalBytes()).position(pos);
        return writeEvent(null, eventBuffer, force, ownRecord);

        // Shouldn't the pos & lim be reset for non-duplicate?
        // It don't think it matters since node knows where to
        // go inside the buffer.
//        if (!duplicate) {
//            bb.limit(origLim).position(origPos);
//        }
    }


    /**
     * Write an event (bank) into a record and eventually to a file in evio/hipo
     * version 6 format.
     * Once the record is full and if writing with multiple compression
     * threads, the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
     *
     * <b>
     * If splitting files, this method returns false if disk partition is too full
     * to write the complete, next split file. If force arg is true, write anyway.
     * DO NOT mix calling this method with calling
     * {@link #writeEvent(EvioBank, ByteBuffer, boolean, boolean)}
     * (or the various writeEvent() methods which call it).
     * Results are unpredictable as it messes up the
     * logic used to quit writing to full disk.
     * </b>
     *
     * The buffer must contain only the event's data (event header and event data)
     * and must <b>not</b> be in complete evio file format.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     * <b>kill</b> performance when writing to a file.
     * A true 3rd arg can be used when the backing buffer
     * of the node is accessed by multiple threads simultaneously. This allows
     * that buffer's limit and position to be changed without interfering
     * with the other threads.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param node       object representing the event to write in buffer form
     * @param force      if writing to disk, force it to write event to the disk.
     * @param duplicate  if true, duplicate node's buffer so its position and limit
     *                   can be changed without issue.
     *
     * @return true if event was added to record. If splitting files, false if disk
     *         partition too full to write the complete, next split file.
     *         False if interrupted. If force arg is true, write anyway.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if null node arg;
     */
    public boolean writeEventToFile(EvioNode node, boolean force, boolean duplicate)
            throws EvioException, IOException {
        return writeEventToFile(node, force, duplicate, false);
    }


    /**
     * Write an event (bank) into a record and eventually to a file in evio/hipo
     * version 6 format.
     * Once the record is full and if writing with multiple compression
     * threads, the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
     *
     * <b>
     * If splitting files, this method returns false if disk partition is too full
     * to write the complete, next split file. If force arg is true, write anyway.
     * DO NOT mix calling this method with calling
     * {@link #writeEvent(EvioBank, ByteBuffer, boolean, boolean)}
     * (or the various writeEvent() methods which call it).
     * Results are unpredictable as it messes up the
     * logic used to quit writing to full disk.
     * </b>
     *
     * The buffer must contain only the event's data (event header and event data)
     * and must <b>not</b> be in complete evio file format.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     * <b>kill</b> performance when writing to a file.
     * A true 3rd arg can be used when the backing buffer
     * of the node is accessed by multiple threads simultaneously. This allows
     * that buffer's limit and position to be changed without interfering
     * with the other threads.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param node       object representing the event to write in buffer form
     * @param force      if writing to disk, force it to write event to the disk.
     * @param duplicate  if true, duplicate node's buffer so its position and limit
     *                   can be changed without issue.
     * @param ownRecord  if true, write event in its own record regardless
     *                   of event count and record size limits.
     *
     * @return true if event was added to record. If splitting files, false if disk
     *         partition too full to write the complete, next split file.
     *         False if interrupted. If force arg is true, write anyway.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if null node arg;
     */
    public boolean writeEventToFile(EvioNode node, boolean force,
                                    boolean duplicate, boolean ownRecord)
            throws EvioException, IOException {

        if (node == null) {
            throw new EvioException("null node arg");
        }

        ByteBuffer eventBuffer, bb = node.getBuffer();

        // Duplicate buffer so we can set pos & limit without messing others up
        if (duplicate) {
            eventBuffer = bb.duplicate().order(bb.order());
        }
        else {
            eventBuffer = bb;
        }

        int pos = node.getPosition();
        eventBuffer.limit(pos + node.getTotalBytes()).position(pos);
        return writeEventToFile(null, eventBuffer, force, ownRecord);
    }

    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file (for multiple compression
     * threads), the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
     * If writing to a buffer, once the record is full this method returns
     * false - indicating that the last event was NOT written to the record.
     * To finish the writing process, call {@link #close()}. This will
     * compress the data if desired and then write it to the buffer.<p>
     *
     * The buffer must contain only the event's data (event header and event data)
     * and must <b>not</b> be in complete evio file format.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param bankBuffer the bank (as a ByteBuffer object) to write.
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         record event count limit exceeded, or bank and bankBuffer args are both null.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing.
     */
    public boolean writeEvent(ByteBuffer bankBuffer) throws EvioException, IOException {
        return writeEvent(null, bankBuffer, false, false);
    }


    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file (for multiple compression
     * threads), the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
     * If writing to a buffer, once the record is full this method returns
     * false - indicating that the last event was NOT written to the record.
     * To finish the writing process, call {@link #close()}. This will
     * compress the data if desired and then write it to the buffer.<p>
     *
     * The buffer must contain only the event's data (event header and event data)
     * and must <b>not</b> be in complete evio file format.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     * <b>kill</b> performance when writing to a file.
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param bankBuffer the bank (as a ByteBuffer object) to write.
     * @param force      if writing to disk, force it to write event to the disk.
     *
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         record event count limit exceeded, or bank and bankBuffer args are both null.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing.
     */
    public boolean writeEvent(ByteBuffer bankBuffer, boolean force)
            throws EvioException, IOException {
        return writeEvent(null, bankBuffer, force, false);
    }


    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file (for multiple compression
     * threads), the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
     * If writing to a buffer, once the record is full this method returns
     * false - indicating that the last event was NOT written to the record.
     * To finish the writing process, call {@link #close()}. This will
     * compress the data if desired and then write it to the buffer.<p>
     *
     * The buffer must contain only the event's data (event header and event data)
     * and must <b>not</b> be in complete evio file format.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     * <b>kill</b> performance when writing to a file.
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param bankBuffer the bank (as a ByteBuffer object) to write.
     * @param force      if writing to disk, force it to write event to the disk.
     * @param ownRecord  if true, write event in its own record regardless
     *                   of event count and record size limits.
     *
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         record event count limit exceeded, or bank and bankBuffer args are both null.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing.
     */
    public boolean writeEvent(ByteBuffer bankBuffer, boolean force, boolean ownRecord)
            throws EvioException, IOException {
        return writeEvent(null, bankBuffer, force, ownRecord);
    }


    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file (for multiple compression
     * threads), the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
     * If writing to a buffer, once the record is full this method returns
     * false - indicating that the last event was NOT written to the record.
     * To finish the writing process, call {@link #close()}. This will
     * compress the data if desired and then write it to the buffer.<p>
     *
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param bank   the bank to write.
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         record event count limit exceeded, or bank and bankBuffer args are both null.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;.
     */
    public boolean writeEvent(EvioBank bank)
            throws EvioException, IOException {
        return writeEvent(bank, null, false, false);
    }


    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file (for multiple compression
     * threads), the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
     * If writing to a buffer, once the record is full this method returns
     * false - indicating that the last event was NOT written to the record.
     * To finish the writing process, call {@link #close()}. This will
     * compress the data if desired and then write it to the buffer.<p>
     *
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * Be warned that injudicious use of the 2nd arg, the force flag, will
     * <b>kill</b> performance when writing to a file.
     *
     * @param bank   the bank to write.
     * @param force  if writing to disk, force it to write event to the disk.
     *
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         record event count limit exceeded, or bank and bankBuffer args are both null.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;.
     */
    public boolean writeEvent(EvioBank bank, boolean force)
            throws EvioException, IOException {
        return writeEvent(bank, null, force, false);
    }


    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file (for multiple compression
     * threads), the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
     * If writing to a buffer, once the record is full this method returns
     * false - indicating that the last event was NOT written to the record.
     * To finish the writing process, call {@link #close()}. This will
     * compress the data if desired and then write it to the buffer.<p>
     *
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * Be warned that injudicious use of the 2nd arg, the force flag, will
     * <b>kill</b> performance when writing to a file.
     *
     * @param bank   the bank to write.
     * @param force  if writing to disk, force it to write event to the disk.
     * @param ownRecord  if true, write event in its own record regardless
     *                   of event count and record size limits.
     *
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         record event count limit exceeded, or bank and bankBuffer args are both null.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;.
     */
    public boolean writeEvent(EvioBank bank, boolean force, boolean ownRecord)
            throws EvioException, IOException {
        return writeEvent(bank, null, force, ownRecord);
    }


    /**
     * Write an event (bank) into a record in evio/hipo version 6 format.
     * Once the record is full and if writing to a file (for multiple compression
     * threads), the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
     * If writing to a buffer, once the record is full this method returns
     * false - indicating that the last event was NOT written to the record.
     * To finish the writing process, call {@link #close()}. This will
     * compress the data if desired and then write it to the buffer.<p>
     *
     * The event to be written may be in one of two forms.
     * The first is as an EvioBank object and the second is as a ByteBuffer
     * containing only the event's data (event header and event data) and must
     * <b>not</b> be in complete evio file format.
     * The first non-null of the bank arguments will be written.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     *<b>kill</b> performance when writing to a file.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param bank       the bank (as an EvioBank object) to write.
     * @param bankBuffer the bank (as a ByteBuffer object) to write.
     * @param force      if writing to disk, force it to write event to the disk.
     * @param ownRecord  if true, write event in its own record regardless
     *                   of event count and record size limits.
     *
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         record event count limit exceeded, or bank and bankBuffer args are both null.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if bad bankBuffer format;
     *                       if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing.
     */
    private boolean writeEvent(EvioBank bank, ByteBuffer bankBuffer,
                               boolean force, boolean ownRecord)
            throws EvioException, IOException {

        if (closed) {
            throw new EvioException("close() has already been called");
        }

        boolean fitInRecord;
        boolean splittingFile = false;
        // See how much space the event will take up
        int currentEventBytes;

        // Which bank do we write?
        if (bankBuffer != null) {
            if (bankBuffer.order() != byteOrder) {
                throw new EvioException("event buf is " + bankBuffer.order() + ", and writer is " + byteOrder);
            }

            // Event size in bytes (from buffer ready to read)
            currentEventBytes = bankBuffer.remaining();

            // Size must be multiple of 4 bytes (whole 32-bit ints)
            // The following is the equivalent of the mod operation:
            //  (currentEventBytes % 4 != 0) but is much faster (x mod 2^n == x & (2^n - 1))
            if ((currentEventBytes & 3) != 0) {
                throw new EvioException("bad bankBuffer format");
            }

            // Check for inconsistent lengths
            if (currentEventBytes != 4 * (bankBuffer.getInt(bankBuffer.position()) + 1)) {
                throw new EvioException("inconsistent event lengths: total bytes from event = " +
                                       (4*(bankBuffer.getInt(bankBuffer.position()) + 1)) +
                                        ", from buffer = " + currentEventBytes);
            }
        }
        else if (bank != null) {
            currentEventBytes = bank.getTotalBytes();
        }
        else {
            return false;
        }

        // If writing to buffer, we're not multi-threading compression & writing.
        // Do it all in this thread, right now.
        if (!toFile) {
            return writeToBuffer(bank, bankBuffer);
        }

        // If here, we're writing to a file ...

        // If we're splitting files,
        // we must have written at least one real event before we
        // can actually split the file.
        if ((split > 0) && (splitEventCount > 0)) {
            // Is event, along with the previous events, large enough to split the file?
            // For simplicity ignore the headers which will take < 2Kb.
            // Take any compression roughly into account.
            long totalSize = (currentEventBytes + splitEventBytes)*compressionFactor/100;

            // If we're going to split the file, set a couple flags
            if (totalSize > split) {
//                System.out.println("Split at total size = " + totalSize +
//                                   ", ev = " + currentEventBytes +
//                                   ", prev = " + splitEventBytes +
//                                   ", compressed data = " + ((currentEventBytes + splitEventBytes)*compressionFactor/100));
                splittingFile = true;
            }
        }

        // First, if multithreaded write, check for any errors that may have
        // occurred asynchronously in the write or one of the compression threads.
        if (!singleThreadedCompression && supply.haveError()) {
            // Wake up any of these threads waiting for another record
            supply.errorAlert();
            throw new IOException(supply.getError());
        }

        // Including this event, this is the total data size & event count
        // for this split file.
        splitEventBytes += currentEventBytes;
        splitEventCount++;

        // If event is big enough to split the file ...
        if (splittingFile) {
            if (singleThreadedCompression) {
                try {
                    compressAndWriteToFile(false);
                }
                catch (InterruptedException e) {
                    return false;
                }
                catch (ExecutionException e) {
                    throw new IOException(e);
                }
                
                splitFile();
            }
            else {
                // Set flag to split file
                currentRingItem.splitFileAfterWrite(true);
                // Send current record back to ring without adding event
                supply.publish(currentRingItem);

                // Get another empty record from ring.
                // Record number reset for new file.
                recordNumber = 1;
                currentRingItem = supply.get();
                currentRecord = currentRingItem.getRecord();
                currentRecord.getHeader().setRecordNumber(recordNumber++);
            }

            // Reset split-tracking variables
            splitEventBytes = 0L;
            splitEventCount = 0;
        }

        // If event to be written in its own record
        if (ownRecord) {
            fitInRecord = false;
        }
        // Try adding event to current record.
        // One event is guaranteed to fit in a record no matter the size,
        // IFF using multithreaded compression.
        else if (bankBuffer != null) {
            fitInRecord = currentRecord.addEvent(bankBuffer);
        }
        else {
            fitInRecord = currentRecord.addEvent(bank);
        }

//System.out.println("writeEvent: fit in record = "  + fitInRecord);

        // If no room or too many events ...
        if (!fitInRecord) {
            if (singleThreadedCompression) {
                // Only try to write what we have if there's something to write
                if (currentRecord.getEventCount() > 0) {
                    try {
                        compressAndWriteToFile(false);
                    }
                    catch (InterruptedException e) {
                        return false;
                    }
                    catch (ExecutionException e) {
                        throw new IOException(e);
                    }

                    // Try add eventing to it (1 very big event many not fit)
                    if (bankBuffer != null) {
                        fitInRecord = currentRecord.addEvent(bankBuffer);
                    }
                    else {
                        fitInRecord = currentRecord.addEvent(bank);
                    }
                }

                // Since we're here, we have a single event too big to fit in the
                // allocated buffers. So expand buffers and try again.
                if (!fitInRecord) {
//System.out.println("writeEvent: expand buffers to " + (currentEventBytes/10*12) + " bytes");
                    expandInternalBuffers(currentEventBytes);

                    if (bankBuffer != null) {
                        fitInRecord = currentRecord.addEvent(bankBuffer);
                    }
                    else {
                        fitInRecord = currentRecord.addEvent(bank);
                    }

                    if (!fitInRecord) {
                        throw new EvioException("cannot fit event into buffer");
                    }
//System.out.println("writeEvent: EVENT FINALLY FIT!");
                }
            }
            else {
                // This "if" is only needed since we may want event in its own record
                if (currentRecord.getEventCount() > 0) {
                    // Send current record back to ring without adding event
                    supply.publish(currentRingItem);

                    // Get another empty record from ring
                    currentRingItem = supply.get();
                    currentRecord = currentRingItem.getRecord();
                    currentRecord.getHeader().setRecordNumber(recordNumber++);
                }

                // Add event to it (guaranteed to fit)
                if (bankBuffer != null) {
                    currentRecord.addEvent(bankBuffer);
                }
                else {
                    currentRecord.addEvent(bank);
                }
            }
        }

        // If event must be physically written to disk,
        // or the event must be written in its own record ...
        if (force || ownRecord) {
            if (singleThreadedCompression) {
                try {
                    compressAndWriteToFile(force);
                }
                catch (InterruptedException e) {
                    return false;
                }
                catch (ExecutionException e) {
                    throw new IOException(e);
                }
            }
            else {
                // Tell writer to force this record to disk if necessary
                currentRingItem.forceToDisk(force);
                // Send current record back to ring
                supply.publish(currentRingItem);

                // Get another empty record from ring
                currentRingItem = supply.get();
                currentRecord = currentRingItem.getRecord();
                currentRecord.getHeader().setRecordNumber(recordNumber++);
            }
        }

        return true;
    }


    /**
     * Write an event (bank) into a record and eventually to a file in evio/hipo
     * version 6 format.
     * Once the record is full and if writing with multiple compression
     * threads, the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
     *
     * <b>
     * If splitting files, this method returns false if disk partition is too full
     * to write the complete, next split file. If force arg is true, write anyway.
     * DO NOT mix calling this method with calling
     * {@link #writeEvent(EvioBank, ByteBuffer, boolean, boolean)}
     * (or the various writeEvent() methods which call it).
     * Results are unpredictable as it messes up the
     * logic used to quit writing to full disk.
     * </b>
     *
     * The event to be written may be in one of two forms.
     * The first is as an EvioBank object and the second is as a ByteBuffer
     * containing only the event's data (event header and event data) and must
     * <b>not</b> be in complete evio file format.
     * The first non-null of the bank arguments will be written.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     *<b>kill</b> performance when writing to a file.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param bank       the bank (as an EvioBank object) to write.
     * @param bankBuffer the bank (as a ByteBuffer object) to write.
     *
     * @return true if event was added to record. If splitting files, false if disk
     *         partition too full to write the complete, next split file.
     *         False if interrupted. If force arg is true, write anyway.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if both buffer args are null;
     *                       if bad bankBuffer format;
     *                       if close() already called;
     *                       if not writing to file;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing.
     */
    synchronized public boolean writeEventToFile(EvioBank bank, ByteBuffer bankBuffer,
                                                 boolean force)
            throws EvioException, IOException {
        return writeEventToFile(bank, bankBuffer, force, false);
    }


    /**
     * Write an event (bank) into a record and eventually to a file in evio/hipo
     * version 6 format.
     * Once the record is full and if writing with multiple compression
     * threads, the record will be sent to a thread which may compress the data,
     * then it will be sent to a thread to write the record to file.
     * If there is only 1 compression thread, it's all done in the thread which
     * call this method.<p>
     *
     * <b>
     * If splitting files, this method returns false if disk partition is too full
     * to write the complete, next split file. If force arg is true, write anyway.
     * DO NOT mix calling this method with calling
     * {@link #writeEvent(EvioBank, ByteBuffer, boolean, boolean)}
     * (or the various writeEvent() methods which call it).
     * Results are unpredictable as it messes up the
     * logic used to quit writing to full disk.
     * </b>
     *
     * The event to be written may be in one of two forms.
     * The first is as an EvioBank object and the second is as a ByteBuffer
     * containing only the event's data (event header and event data) and must
     * <b>not</b> be in complete evio file format.
     * The first non-null of the bank arguments will be written.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     *<b>kill</b> performance when writing to a file.<p>
     *
     * This method is not used to write the dictionary or the first event
     * which are both placed in the common record which, in turn, is the
     * user header part of the file header.<p>
     *
     * @param bank       the bank (as an EvioBank object) to write.
     * @param bankBuffer the bank (as a ByteBuffer object) to write.
     * @param force      if writing to disk, force it to write event to the disk.
     * @param ownRecord  if true, write event in its own record regardless
     *                   of event count and record size limits.
     *
     * @return true if event was added to record. If splitting files, false if disk
     *         partition too full to write the complete, next split file.
     *         False if interrupted. If force arg is true, write anyway.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if both buffer args are null;
     *                       if bad bankBuffer format;
     *                       if close() already called;
     *                       if not writing to file;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing.
     */
    public boolean writeEventToFile(EvioBank bank, ByteBuffer bankBuffer,
                                    boolean force, boolean ownRecord)
            throws EvioException, IOException {

        if (closed) {
            throw new EvioException("close() has already been called");
        }

        if (!toFile) {
            throw new EvioException("cannot write to buffer with this method");
        }

        // If here, we're writing to a file ...

        // First, if multithreaded write, check for any errors that may have
        // occurred asynchronously in the write or one of the compression threads.
        // Also check to see if disk is full.
        if (!singleThreadedCompression) {
            if (supply.haveError()) {
                // Wake up any of these threads waiting for another record
                supply.errorAlert();
                throw new IOException(supply.getError());
            }

            // With multithreaded writing, if the writing thread discovers that
            // the disk partition is full, everything that has made it past this check
            // and all the records in the pipeline (ring in this case) will be
            // written.
            if (diskIsFullVolatile && !force) {
                // Check again to see if it's still full
                if (fullDisk()) {
                    // Still full
                    return false;
                }
                System.out.println("writeEventToFile: disk is NOT full, emptied");
            }
        }
        // If single threaded write, and we can't allow more events in due to limited disk space
        else if (diskIsFull && !force) {
            // Actually check disk again
            if (fullDisk()) {
                return false;
            }
        }

        boolean fitInRecord;
        boolean splittingFile = false;
        // See how much space the event will take up
        int currentEventBytes;

        // Which bank do we write?
        if (bankBuffer != null) {
            if (bankBuffer.order() != byteOrder) {
                throw new EvioException("event buf is " + bankBuffer.order() + ", and writer is " + byteOrder);
            }

            // Event size in bytes (from buffer ready to read)
            currentEventBytes = bankBuffer.remaining();

            // Size must be multiple of 4 bytes (whole 32-bit ints)
            // The following is the equivalent of the mod operation:
            //  (currentEventBytes % 4 != 0) but is much faster (x mod 2^n == x & (2^n - 1))
            if ((currentEventBytes & 3) != 0) {
                throw new EvioException("bad bankBuffer format");
            }

            // Check for inconsistent lengths
            if (currentEventBytes != 4 * (bankBuffer.getInt(bankBuffer.position()) + 1)) {
                throw new EvioException("inconsistent event lengths: total bytes from event = " +
                                       (4*(bankBuffer.getInt(bankBuffer.position()) + 1)) +
                                        ", from buffer = " + currentEventBytes);
            }
        }
        else if (bank != null) {
            currentEventBytes = bank.getTotalBytes();
        }
        else {
            throw new EvioException("both buffer args are null");
        }

        // If we're splitting files,
        // we must have written at least one real event before we
        // can actually split the file.
        if ((split > 0) && (splitEventCount > 0)) {
            // Is event, along with the previous events, large enough to split the file?
            // For simplicity ignore the headers which will take < 2Kb.
            // Take any compression roughly into account.
            long totalSize = (currentEventBytes + splitEventBytes)*compressionFactor/100;

            // If we're going to split the file, set a couple flags
            if (totalSize > split) {
                splittingFile = true;
            }
        }

        // Including this event, this is the total data size & event count
        // for this split file.
        splitEventBytes += currentEventBytes;
        splitEventCount++;

        // If event is big enough to split the file, write what we already have
        // (not including current event).
        if (splittingFile) {
            if (singleThreadedCompression) {
                try {
                    // Should always be able to finish writing current file
                    compressAndWriteToFile(force);
                }
                catch (InterruptedException e) {
                    return false;
                }
                catch (ExecutionException e) {
                    throw new IOException(e);
                }

                splitFile();
            }
            else {
                // Set flag to split file. In this case, allow split to happen
                // even if disk partition is "full" since we've allowed enough
                // space for that.
                currentRingItem.splitFileAfterWrite(true);
                currentRingItem.setCheckDisk(false);
                // Send current record back to ring without adding event
                supply.publish(currentRingItem);

                // Get another empty record from ring.
                // Record number reset for new file.
                recordNumber = 1;
                currentRingItem = supply.get();
                currentRecord = currentRingItem.getRecord();
                currentRecord.getHeader().setRecordNumber(recordNumber++);
            }

            // Reset split-tracking variables
            splitEventBytes = 0L;
            splitEventCount = 0;
//System.out.println("Will split, reset splitEventBytes = "  + splitEventBytes);
        }

        // If event to be written in its own record
        if (ownRecord) {
            fitInRecord = false;
        }
        // Try adding event to current record.
        // One event is guaranteed to fit in a record no matter the size,
        // IFF using multithreaded compression.
        // If record's memory is expanded to fit the one big event,
        // every subsequent record written using this object may be
        // bigger as well.
        else if (bankBuffer != null) {
            fitInRecord = currentRecord.addEvent(bankBuffer);
        }
        else {
            fitInRecord = currentRecord.addEvent(bank);
        }

//System.out.println("writeEventToFile: fit in record = "  + fitInRecord);

        // If no room or too many events in record, write out current record first,
        // then start working on a new record with this event.

        // If no room or too many events ...
        if (!fitInRecord) {

            // We will not end up here if the file just split, so
            // splitEventBytes and splitEventCount will NOT have
            // just been set to 0.

            if (singleThreadedCompression) {
                // Only try to write what we have if there's something to write
                if (currentRecord.getEventCount() > 0) {
//System.out.println("writeEventToFile: single threaded compression, force = " + force);
                    try {
                        // This might be the first write after the file split.
                        // If so, return false if disk is full,
                        // otherwise write what we already have first.
                        if (!tryCompressAndWriteToFile(force)) {
                            // Undo stuff since we're no longer writing
                            splitEventCount--;
                            splitEventBytes -= currentEventBytes;
//System.out.println("writeEventToFile: did NOT write existing record to disk");
                            return false;
                        }
//System.out.println("writeEventToFile: wrote existing record to disk");
                    }
                    catch (InterruptedException e) {
                        return false;
                    }
                    catch (ExecutionException e) {
                        throw new IOException(e);
                    }

                    // Try add eventing to it (1 very big event many not fit)
                    if (bankBuffer != null) {
                        fitInRecord = currentRecord.addEvent(bankBuffer);
                    }
                    else {
                        fitInRecord = currentRecord.addEvent(bank);
                    }
                }

                // Since we're here, we have a single event too big to fit in the
                // allocated buffers. So expand buffers and try again.
                if (!fitInRecord) {
//System.out.println("writeEventToFile: expand buffers to " + (currentEventBytes/10*12) + " bytes");
                    expandInternalBuffers(currentEventBytes);

                    if (bankBuffer != null) {
                        fitInRecord = currentRecord.addEvent(bankBuffer);
                    }
                    else {
                        fitInRecord = currentRecord.addEvent(bank);
                    }

                    if (!fitInRecord) {
                        throw new EvioException("cannot fit event into buffer");
                    }
//System.out.println("writeEventToFile: EVENT FINALLY FIT!");
                }
            }
            else {
                // This "if" is only needed since we may want event in its own record
                if (currentRecord.getEventCount() > 0) {
                    currentRingItem.setCheckDisk(true);
                    supply.publish(currentRingItem);
                    currentRingItem = supply.get();
                    currentRecord = currentRingItem.getRecord();
                    currentRecord.getHeader().setRecordNumber(recordNumber++);
                }

                // Add event to it which is guaranteed to fit
                if (bankBuffer != null) {
                    currentRecord.addEvent(bankBuffer);
                }
                else {
                    currentRecord.addEvent(bank);
                }
            }
        }

        // If event must be physically written to disk,
        // or the event must be written in its own record ...
        if (force || ownRecord) {
            if (singleThreadedCompression) {
                try {
                    if (!tryCompressAndWriteToFile(force)) {
                        splitEventCount--;
                        splitEventBytes -= currentEventBytes;
                        return false;
                    }
                }
                catch (InterruptedException e) {
                    return false;
                }
                catch (ExecutionException e) {
                    throw new IOException(e);
                }
            }
            else {
                if (force) {
                    // Force things to disk by telling the writing
                    // thread which record started the force to disk.
                    // This will force this record, along with all preceeding records in
                    // the pipeline, to the file.
                    // Once it's written, we can go back to the normal of not
                    // forcing things to disk.
                    currentRingItem.setId(++idCounter);
                    recordWriterThread.setForcedRecordId(idCounter);
                }

                supply.publish(currentRingItem);
                currentRingItem = supply.get();
                currentRecord = currentRingItem.getRecord();
                currentRecord.getHeader().setRecordNumber(recordNumber++);
            }
        }

        return true;
    }


    /**
     * Check to see if the disk is full.
     * Is it able to store 1 full split, 1 supply of records, and a 10MB buffer zone?
     * Two variables are set, one volatile and one not, depending on needs.
     * @return  true if full, else false.
     */
    private boolean fullDisk() {
        // How much free space is available on the disk?
        long freeBytes = currentFile.getParentFile().getFreeSpace();
        // If there isn't enough free space to write the complete, projected size file
        // plus full records + 10MB extra ...
        diskIsFull = freeBytes < split + maxSupplyBytes + 10000000;
        if (!singleThreadedCompression) {
            diskIsFullVolatile = diskIsFull;
        }
        return diskIsFull;
    }


    /**
     * Compress data and write record to file. Does nothing if close() already called.
     * Used when doing compression & writing to file in a single thread.
     *
     * @param force  if true, force writing event physically to disk.
     *
     * @throws EvioException if this object already closed;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     * @throws IOException   if error opening or forcing file write.
     * @throws ExecutionException     if error writing file.
     * @throws InterruptedException   if this thread was interrupted while waiting
     *                                for file write to complete.
     */
    private void compressAndWriteToFile(boolean force)
                        throws EvioException, IOException,
                               InterruptedException, ExecutionException {

        RecordHeader header = currentRecord.getHeader();
        header.setRecordNumber(recordNumber);
        header.setCompressionType(compressionType);
        currentRecord.build();
        // Resets currentRecord too
        writeToFile(force, false);
     }


    /**
     * Compress data and write record to file. Does nothing if close() already called.
     * Used when doing compression & writing to file in a single thread.
     * Will not write file if no room on disk (and force arg is false).
     *
     * @param force  if true, force writing event physically to disk.
     * @return true if everything normal; false if a new file needs to be created
     *         (first write after a split) but there is not enough free space on
     *         the disk partition for the next, complete file.
     *         If force arg is true, write anyway.
     *
     * @throws EvioException if this object already closed;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     * @throws IOException   if error opening or forcing file write.
     * @throws ExecutionException     if error writing file.
     * @throws InterruptedException   if this thread was interrupted while waiting
     *                                for file write to complete.
     */
    private boolean tryCompressAndWriteToFile(boolean force)
                        throws EvioException, IOException,
                               InterruptedException, ExecutionException {

        RecordHeader header = currentRecord.getHeader();
        header.setRecordNumber(recordNumber);
        header.setCompressionType(compressionType);
        currentRecord.build();
        return writeToFile(force, true);
     }


    /**
     * For single threaded compression, write record to file.
     * In this case, we have 1 record, but 3 buffers.
     * Two buffers can be written at any one time, while the 3rd is being filled
     * in the record.
     * Does nothing if close() already called.
     *
     * @param force force it to write event to the disk.
     * @param checkDisk if true and if a new file needs to be created but there is
     *                  not enough free space on the disk partition for the
     *                  complete intended file, return false without creating or
     *                  writing to file. If force arg is true, write anyway.
     *
     * @return true if everything normal; false if a new file needs to be created
     *         (first write after a split) but there is not enough free space on
     *         the disk partition for the next, complete file and checkDisk arg is true.
     *
     * @throws EvioException if this object already closed;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     * @throws IOException          if error opening or forcing file write.
     * @throws ExecutionException   if error writing file.
     * @throws InterruptedException if this thread was interrupted while waiting
     *                              for file write to complete.
     */
    private boolean writeToFile(boolean force, boolean checkDisk)
            throws EvioException, IOException,
                   InterruptedException, ExecutionException {
        if (closed) {
            throw new EvioException("close() has already been called");
        }

        // This actually creates the file so do it only once
        if (bytesWritten < 1) {

            // We want to check to see if there is enough room to write the
            // next split, before it's written. Thus, before writing the first block
            // of a new file, we check to see if there's space for the whole thing.
            // (Actually, the whole split + current block + some extra for safety).
            if (checkDisk && (!force) && fullDisk()) {
                // If we're told to check the disk, and we're not forcing things,
                // AND disk is full, don't write the block.
                return false;
            }

            try {
                asyncFileChannel = AsynchronousFileChannel.open(currentFilePath,
                                                                StandardOpenOption.TRUNCATE_EXISTING,
                                                                StandardOpenOption.CREATE,
                                                                StandardOpenOption.WRITE);

                fileWritingPosition = 0L;
                splitCount++;
            }
            catch (FileNotFoundException e) {
                throw new EvioException("File could not be opened for writing, " +
                                                currentFile.getPath(), e);
            }

            // Write out the beginning file header including common record
            writeFileHeader();
        }

        // Which buffer do we fill next?
        ByteBuffer unusedBuffer;

        // We need one of the 2 future jobs to be completed in order to proceed

        // If 1st time thru, proceed without waiting
        if (future1 == null) {
            futureIndex = 0;
            // Fill 2nd buffer next
            unusedBuffer = internalBuffers[1];
        }
        // If 2nd time thru, proceed without waiting
        else if (future2 == null) {
            futureIndex = 1;
            // Fill 3rd buffer next
            unusedBuffer = internalBuffers[2];
        }
        // After first 2 times, wait until one of the
        // 2 futures is finished before proceeding
        else {

            boolean future1Done = future1.isDone();
            boolean future2Done = future2.isDone();

            // If first write done ...
            if (future1Done) {
                future1.get();

                // Reuse the buffer future1 just finished using
                unusedBuffer = usedBuffers[0];
                futureIndex  = 0;

                // future1 is finished, and future2 might be as
                // well but just check for errors right now
                if (future2Done) {
                    future2.get();
                }
            }
            // only 2nd write is done
            else if (future2Done) {
                future2.get();
                unusedBuffer = usedBuffers[1];
                futureIndex  = 1;
            }
            // Neither are finished, so wait for one of them to finish
            else {
                // If the last write to be submitted was future2, wait for 1
                if (futureIndex == 0) {
                    // Wait for write to end before we continue
                    future1.get();
                    unusedBuffer = usedBuffers[0];
                }
                // Otherwise, wait for 2
                else {
                    future2.get();
                    unusedBuffer = usedBuffers[1];
                }
            }
        }

        // Get record to write
        RecordOutputStream record = currentRecord;
        RecordHeader header = record.getHeader();

        // Length of this record
        int bytesToWrite = header.getLength();
        int eventCount   = header.getEntries();
        recordLengths.add(bytesToWrite);
        // Trailer's index has count following length
        recordLengths.add(eventCount);

        // Data to write
        ByteBuffer buf = record.getBinaryBuffer();

        if (futureIndex == 0) {
            future1 = asyncFileChannel.write(buf, fileWritingPosition);
            // Keep track of which buffer future1 used so it can be reused when done
            usedBuffers[0] = buf;
            futureIndex = 1;
        }
        else {
            future2 = asyncFileChannel.write(buf, fileWritingPosition);
            usedBuffers[1] = buf;
            futureIndex = 0;
        }

        // Next buffer to work with
        buffer = unusedBuffer;
        // Clear buffer since we don't know what state it was left in
        // after write to file AND setBuffer uses its position to start from
        buffer.clear();
        record.setBuffer(buffer);
        record.reset();

        // Force it to write to physical disk (KILLS PERFORMANCE!!!, 15x-20x slower),
        // but don't bother writing the metadata (arg to force()) since that slows it
        // down even more.
        // TODO: This may not work since data may NOT have been written yet!
        if (force) asyncFileChannel.force(false);

        // Keep track of what is written to this, one, file
        recordNumber++;
        recordsWritten++;
        bytesWritten        += bytesToWrite;
        fileWritingPosition += bytesToWrite;
        eventsWrittenToFile += eventCount;
        eventsWrittenTotal  += eventCount;

//        if (false) {
//            System.out.println("    writeToFile: after last header written, Events written to:");
//            System.out.println("                 cnt total (no dict) = " + eventsWrittenTotal);
//            System.out.println("                 file cnt total (dict) = " + eventsWrittenToFile);
//            System.out.println("                 internal buffer cnt (dict) = " + eventsWrittenToBuffer);
//            System.out.println("                 bytes-written  = " + bytesToWrite);
//            System.out.println("                 bytes-to-file = " + bytesWritten);
//            System.out.println("                 record # = " + recordNumber);
//        }

        return true;
    }


//    /**
//     * For multi-threaded compression, write record to file.
//     * In this case we do NOT have 1 record with 3 buffers.
//     * Instead we have a ring of records, each with its own buffers.
//     * Does nothing if close() already called.
//     *
//     * @param item  contains record to write to the disk if compression is multi-threaded.
//     * @param force force it to write event to the disk.
//     *
//     * @throws EvioException if this object already closed;
//     *                       if file could not be opened for writing;
//     *                       if file exists but user requested no over-writing;
//     * @throws IOException   if error writing file
//     */
//    private void writeToFileMT_OneAsyncWrite(RecordRingItem item, boolean force)
//                                throws EvioException, IOException {
//        if (closed) {
//            throw new EvioException("close() has already been called");
//        }
//
//        // This actually creates the file so do it only once
//        if (bytesWritten < 1) {
//            try {
//                System.out.println("Creating channel to " + currentFilePath);
//                asyncFileChannel = AsynchronousFileChannel.open(currentFilePath,
//                                                                StandardOpenOption.TRUNCATE_EXISTING,
//                                                                StandardOpenOption.CREATE,
//                                                                StandardOpenOption.WRITE);
//                fileWritingPosition = 0L;
//                splitCount++;
//            }
//            catch (FileNotFoundException e) {
//                throw new EvioException("File could not be opened for writing, " +
//                                                currentFile.getPath(), e);
//            }
//
//            // Write out the beginning file header including common record
//            writeFileHeader();
//        }
//
//        // We the future job to be completed in order to proceed
//
//        // If 1st time thru, proceed without waiting
//        if (future1 != null) {
//            // If write NOT done ...
//            if (!future1.isDone()) {
//                try {
//                    // Wait for write to end before we continue
//                    future1.get();
//                }
//                catch (Exception e) {
//                    throw new IOException(e);
//                }
//            }
//
//            supply.releaseWriterSequential(ringItem1);
//        }
//
//        // Get record to write
//        RecordOutputStream record = item.getRecord();
//        RecordHeader header = record.getHeader();
//
//        // Length of this record
//        int bytesToWrite = header.getLength();
//        int eventCount   = header.getEntries();
//        recordLengths.add(bytesToWrite);
//        // Trailer's index has count following length
//        recordLengths.add(eventCount);
//
//        // Data to write
//        ByteBuffer buf = record.getBinaryBuffer();
//
//        //buf.position(buffer.limit() - 20);
//        if (noFileWriting) {
//            future1 = new Future<Integer>() {
//                public boolean cancel(boolean mayInterruptIfRunning) {return false;}
//                public boolean isCancelled() {return false;}
//                public Integer get() {return null;}
//                public Integer get(long timeout, TimeUnit unit) {return null;}
//                // Simulate the write being done immediately
//                public boolean isDone() {return true;}
//            };
//        }
//        else {
//            future1 = asyncFileChannel.write(buf, fileWritingPosition);
//        }
//
//        ringItem1 = item;
//
//        // Force it to write to physical disk (KILLS PERFORMANCE!!!, 15x-20x slower),
//        // but don't bother writing the metadata (arg to force()) since that slows it
//        // down even more.
//        // TODO: This may not work since data may NOT have been written yet!
//        if (force) asyncFileChannel.force(false);
//
//        // Keep track of what is written to this, one, file
//        recordNumber++;
//        recordsWritten++;
//        bytesWritten        += bytesToWrite;
//        fileWritingPosition += bytesToWrite;
//        eventsWrittenToFile += eventCount;
//        eventsWrittenTotal  += eventCount;
//
//        //fileWritingPosition += 20;
//        //bytesWritten  += 20;
//
////        if (false) {
////            System.out.println("    writeToFile: after last header written, Events written to:");
////            System.out.println("                 cnt total (no dict) = " + eventsWrittenTotal);
////            System.out.println("                 file cnt total (dict) = " + eventsWrittenToFile);
////            System.out.println("                 internal buffer cnt (dict) = " + eventsWrittenToBuffer);
////            System.out.println("                 bytes-written  = " + bytesToWrite);
////            System.out.println("                 bytes-to-file = " + bytesWritten);
////            System.out.println("                 record # = " + recordNumber);
////        }
//    }


    /**
     * For multi-threaded compression, write record to file.
     * In this case we do NOT have 1 record with 3 buffers.
     * Instead we have a ring of records, each with its own buffers.
     * Does nothing if close() already called.
     *
     * @param item  contains record to write to the disk if compression is multi-threaded.
     * @param force force it to write event to the disk.
     *
     * @throws EvioException if this object already closed;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     * @throws IOException          if error opening or forcing file write.
     * @throws ExecutionException   if error writing file.
     * @throws InterruptedException if this thread was interrupted while waiting
     */
    private void writeToFileMT(RecordRingItem item, boolean force)
                      throws EvioException, IOException,
                             InterruptedException, ExecutionException {
        if (closed) {
            throw new EvioException("close() has already been called");
        }

        // This actually creates the file so do it only once
        if (bytesWritten < 1) {
            try {
                //System.out.println("Creating channel to " + currentFilePath);
                asyncFileChannel = AsynchronousFileChannel.open(currentFilePath,
                                                                StandardOpenOption.TRUNCATE_EXISTING,
                                                                StandardOpenOption.CREATE,
                                                                StandardOpenOption.WRITE);
                fileWritingPosition = 0L;
                splitCount++;
            }
            catch (FileNotFoundException e) {
                throw new EvioException("File could not be opened for writing, " +
                                                currentFile.getPath(), e);
            }

            // Write out the beginning file header including common record
            writeFileHeader();
        }

        // We need one of the 2 future jobs to be completed in order to proceed

        // If 1st time thru, proceed without waiting
        if (future1 == null) {
            futureIndex = 0;
        }
        // If 2nd time thru, proceed without waiting
        else if (future2 == null) {
            futureIndex = 1;
        }
        // After first 2 times, wait until one of the
        // 2 futures is finished before proceeding
        else {

            boolean future1Done = future1.isDone();
            boolean future2Done = future2.isDone();

            // If first write done ...
            if (future1Done) {
                // If this write was done the last time this method called,
                // don't release something previously released! Bad!
                if (prevFuture1 != future1) {
                    future1.get();
                    futureIndex = 0;
                    // Release record back to supply now that we're done writing it
                    supply.releaseWriter(ringItem1);
                    prevFuture1 = future1;
                }

                // future1 is finished and future2 might be as well
                if (future2Done && (prevFuture2 != future2)) {
                    future2.get();
                    supply.releaseWriter(ringItem2);
                    prevFuture2 = future2;
                }
            }
            // only 2nd write is done
            else if (future2Done) {
                // Don't release something previously released
                if (prevFuture2 != future2) {
                    future2.get();
                    supply.releaseWriter(ringItem2);
                }

                futureIndex = 1;
                prevFuture2 = future2;
            }
            // Neither are finished, so wait for one of them to finish
            else {
                // If the last write to be submitted was future2, wait for 1
                if (futureIndex == 0) {
                    // Wait for write to end before we continue
                    future1.get();
                    supply.releaseWriter(ringItem1);
                    prevFuture1 = future1;
                }
                // Otherwise, wait for 2
                else {
                    future2.get();
                    supply.releaseWriter(ringItem2);
                    prevFuture2 = future2;
                }
            }
        }

        // Get record to write
        RecordOutputStream record = item.getRecord();
        RecordHeader header = record.getHeader();

        // Length of this record
        int bytesToWrite = header.getLength();
        int eventCount   = header.getEntries();
        recordLengths.add(bytesToWrite);
        // Trailer's index has count following length
        recordLengths.add(eventCount);

        // Data to write
        ByteBuffer buf = record.getBinaryBuffer();

        if (futureIndex == 0) {
            //buf.position(buffer.limit() - 20);
            if (noFileWriting) {
                future1 = new Future<Integer>() {
                    public boolean cancel(boolean mayInterruptIfRunning) {return false;}
                    public boolean isCancelled() {return false;}
                    public Integer get() {return null;}
                    public Integer get(long timeout, TimeUnit unit) {return null;}
                    // Simulate the write being done immediately
                    public boolean isDone() {return true;}
                };
            }
            else {
                future1 = asyncFileChannel.write(buf, fileWritingPosition);
            }
            ringItem1 = item;
            futureIndex = 1;

            // If forcing to disk, wait here until done
            if (force) {
                // Wait for write to end before we continue
                future1.get();
                supply.releaseWriter(ringItem1);
                prevFuture1 = future1;
                futureIndex = 0;

                // Make sure the other, possibly unfinished, write is done as well
                if (prevFuture2 != future2) {
                    future2.get();
                    supply.releaseWriter(ringItem2);
                    prevFuture2 = future2;
                }
            }
        }
        else {
            //buf.position(buffer.limit() - 20);
            if (noFileWriting) {
                future2 = new Future<Integer>() {
                    public boolean cancel(boolean mayInterruptIfRunning) {return false;}
                    public boolean isCancelled() {return false;}
                    public Integer get() {return null;}
                    public Integer get(long timeout, TimeUnit unit) {return null;}
                    public boolean isDone() {return true;}
                };
            }
            else {
                future2 = asyncFileChannel.write(buf, fileWritingPosition);
            }
            ringItem2 = item;
            futureIndex = 0;

            if (force) {
                future2.get();
                supply.releaseWriter(ringItem2);
                prevFuture2 = future2;
                futureIndex = 1;

                if (prevFuture1 != future1) {
                    future1.get();
                    supply.releaseWriter(ringItem1);
                    prevFuture1 = future1;
                }
            }
        }

        // Force it to write to physical disk (KILLS PERFORMANCE!!!, 15x-20x slower),
        // but don't bother writing the metadata (arg to force()) since that slows it
        // down even more.
        if (force) asyncFileChannel.force(false);

        // Keep track of what is written to this, one, file
        //recordNumber++;
        recordsWritten++;
        bytesWritten        += bytesToWrite;
        fileWritingPosition += bytesToWrite;
        eventsWrittenToFile += eventCount;
        eventsWrittenTotal  += eventCount;

        //fileWritingPosition += 20;
        //bytesWritten  += 20;

//        if (false) {
//            System.out.println("    writeToFile: after last header written, Events written to:");
//            System.out.println("                 cnt total (no dict) = " + eventsWrittenTotal);
//            System.out.println("                 file cnt total (dict) = " + eventsWrittenToFile);
//            System.out.println("                 internal buffer cnt (dict) = " + eventsWrittenToBuffer);
//            System.out.println("                 bytes-written  = " + bytesToWrite);
//            System.out.println("                 bytes-to-file = " + bytesWritten);
//            System.out.println("                 record # = " + recordNumber);
//        }
    }


    /**
     * Split the file for multithreaded compression.
     * Never called when output is to buffer.
     * It writes the trailer which includes an index of all records.
     * Then it closes the old file (forcing unflushed data to be written)
     * and creates the name of the new one.
     *
     * @throws EvioException if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     */
    private void splitFile() throws EvioException {

        if (asyncFileChannel != null) {
            // Finish writing data & trailer and then close existing file -
            // all in a separate thread for speed. Copy over values so they
            // don't change in the meantime.
            fileCloser.closeAsyncFile(asyncFileChannel, future1, future2,
                                      fileHeader, recordLengths, bytesWritten,
                                      fileWritingPosition, recordNumber,
                                      addingTrailer, addTrailerIndex,
                                      (prevFuture1 != future1), (prevFuture2 != future2),
                                      ringItem1, ringItem2);

            // Reset for next write
            if (!singleThreadedCompression) {
                prevFuture1 = prevFuture2 = future1 = future2 = null;
            }
            recordLengths.clear();
            // Right now no file is open for writing
            asyncFileChannel = null;
        }

        // Create the next file's name
        String fileName = Utilities.generateFileName(baseFileName, specifierCount,
                                                     runNumber, split, splitNumber,
                                                     streamId, streamCount);
        splitNumber += splitIncrement;
        currentFile = new File(fileName);
        currentFilePath = Paths.get(fileName);

        // If we can't overwrite and file exists, throw exception
        if (!overWriteOK && (currentFile.exists() && currentFile.isFile())) {
            // If we're doing a multithreaded write ...
            if (supply != null) {
                supply.haveError(true);
                supply.setError("file exists but user requested no over-writing");
            }
            
            throw new EvioException("file exists but user requested no over-writing, "
                    + currentFile.getPath());
        }

        // Reset file values for reuse
        if (singleThreadedCompression) {
            recordNumber = 1;
        }
        recordsWritten      = 0;
        bytesWritten        = 0L;
        eventsWrittenToFile = 0;

//System.out.println("    splitFile: generated file name = " + fileName + ", record # = " + recordNumber);
    }


    /**
     * Write a general header as the last "header" or trailer in the file
     * optionally followed by an index of all record lengths.
     * This writes synchronously.
     *
     * @param writeIndex if true, write an index of all record lengths in trailer.
     * @throws IOException if problems writing to file.
     */
    private void writeTrailerToFile(boolean writeIndex) throws IOException {

        // Keep track of where we are right now which is just before trailer
        long trailerPosition = bytesWritten;

        int bytesToWrite;

        // If we're NOT adding a record index, just write trailer
        if (!writeIndex) {
            try {
                // headerBuffer is only used in this method
                headerBuffer.position(0).limit(RecordHeader.HEADER_SIZE_BYTES);
                RecordHeader.writeTrailer(headerBuffer, 0, recordNumber, null);
            }
            catch (HipoException e) {/* never happen */}
            //bytesToWrite = RecordHeader.HEADER_SIZE_BYTES;

            // We don't want to let the closer thread do the work of seeing that
            // this write completes since it'll just complicate the code.
            // As this is the absolute last write to the file,
            // just make sure it gets done right here.
            Future<Integer> f = asyncFileChannel.write(headerBuffer, fileWritingPosition);
            try {f.get();}
            catch (Exception e) {
                throw new IOException(e);
            }
        }
        else {
            // Write trailer with index

            // How many bytes are we writing here?
            bytesToWrite = RecordHeader.HEADER_SIZE_BYTES + 4*recordLengths.size();

            // Make sure our array can hold everything
            if (headerArray.length < bytesToWrite) {
                headerArray = new byte[bytesToWrite];
                headerBuffer = ByteBuffer.wrap(headerArray).order(byteOrder);
            }
            headerBuffer.position(0).limit(bytesToWrite);

            // Place data into headerBuffer - both header and index
            try {
                RecordHeader.writeTrailer(headerBuffer, 0, recordNumber,
                                          recordLengths);
            }
            catch (HipoException e) {/* never happen */}
            Future<Integer> f = asyncFileChannel.write(headerBuffer, fileWritingPosition);
            try {f.get();}
            catch (Exception e) {
                throw new IOException(e);
            }
        }

        // Update file header's trailer position word
        headerBuffer.position(0).limit(8);
        headerBuffer.putLong(0, trailerPosition);
        Future<Integer> f = asyncFileChannel.write(headerBuffer, FileHeader.TRAILER_POSITION_OFFSET);
        try {f.get();}
        catch (Exception e) {
            throw new IOException(e);
        }

        // Update file header's bit-info word
        if (addTrailerIndex) {
            int bitInfo = fileHeader.setBitInfo(fileHeader.hasFirstEvent(),
                                                fileHeader.hasDictionary(),
                                                true);
            headerBuffer.position(0).limit(4);
            headerBuffer.putInt(0, bitInfo);
            f = asyncFileChannel.write(headerBuffer, FileHeader.BIT_INFO_OFFSET);
            try {f.get();}
            catch (Exception e) {
                throw new IOException(e);
            }
        }
    }


// TODO: THIS DOES NOT LOOK RIGHT, recordNumber may need to be incremented ..........................................
    /**
     * Flush everything in currentRecord into the buffer.
     * There is only one record containing events which is written into the buffer.
     * Following that record is a trailer - an empty record with only a header
     * in order to signify that there are no more events to follow. The trailer
     * could contain an index of all events in the buffer, although this is never
     * done when transferring evio data in buffers over the network.
     */
    private void flushCurrentRecordToBuffer() {

        int eventCount = currentRecord.getEventCount();
        // If nothing in current record, return
        if (eventCount < 1) {
            return;
        }

        // Do construction of record in buffer and possibly compression of its data
        if (commonRecord != null) {
            currentRecord.build(commonRecord.getBinaryBuffer());
        }
        else {
            currentRecord.build();
        }

        // Get record header
        RecordHeader header = currentRecord.getHeader();
        // Get/set record info before building
        header.setRecordNumber(recordNumber);

//        System.out.println("flushCurrentRecordToBuffer: comp size = " + header.getCompressedDataLength() +
//                ", comp words = " + header.getCompressedDataLengthWords() + ", padding = " +
//                header.getCompressedDataLengthPadding());

        int bytesToWrite = header.getLength();
        // Store length & count for possible trailer index

        recordLengths.add(bytesToWrite);
        // Trailer's index has count following length
        recordLengths.add(eventCount);

        // Keep track of what is written
        recordsWritten++;

        // We need to reset lengths here since the data may now be compressed
        bytesWritten = bytesToWrite;
    }


    /**
     * Write bank to current record. If it doesn't fit, return false.
     * The currentRecord will always accept at least one event if it's not
     * writing into a user-provided buffer, expanding memory if it has to.
     * A bank in buffer form has priority, if it's null, then it looks
     * at the bank in EvioBank object form.
     *
     * @param bank        bank to write in EvioBank object form.
     * @param bankBuffer  bank to write in buffer form .
     * @return true if event was added to buffer, false if the buffer is full or
     *         event count limit exceeded.
     */
    private boolean writeToBuffer(EvioBank bank, ByteBuffer bankBuffer) {
        boolean fitInRecord;

        if (bankBuffer != null) {
            // Will this fit the event being written PLUS the ending trailer?
            fitInRecord = currentRecord.addEvent(bankBuffer, trailerBytes());
        }
        else {
            fitInRecord = currentRecord.addEvent(bank, trailerBytes());
        }

        if (fitInRecord) {
             // Update the current block header's size and event count as best we can.
             // Does NOT take compression or trailer into account.
             bytesWritten = commonRecordBytesToBuffer + currentRecord.getUncompressedSize();
             eventsWrittenTotal++;
             eventsWrittenToBuffer++;
        }

        return fitInRecord;
    }


    /**
     * How many bytes make up the desired trailer?
     * @return  number of bytes that make up the desired trailer.
     */
    private int trailerBytes() {
        int len = 0;
        if (addingTrailer) len += RecordHeader.HEADER_SIZE_BYTES;
        if (addTrailerIndex) len += 4 * recordLengths.size();
        return len;
    }


    /**
     * Write a general header as the last "header" or trailer in the buffer
     * optionally followed by an index of all record lengths.
     * @param writeIndex if true, write an index of all record lengths in trailer.
     * @throws EvioException if not enough room in buffer to hold trailer.
     */
    private void writeTrailerToBuffer(boolean writeIndex) throws EvioException {

//System.out.println("writeTrailerToBuffer: internal buf, lim = " + buffer.limit() +
//                ", pos = " + buffer.position());

        // If we're NOT adding a record index, just write trailer
        if (!writeIndex) {
            // Make sure buffer can hold a trailer
            if ((buffer.capacity() - (int)bytesWritten) < RecordHeader.HEADER_SIZE_BYTES) {
                throw new EvioException("not enough room in buffer");
            }

            try {
//int bytesToWrite = RecordHeader.HEADER_SIZE_BYTES + 4*recordLengths.size();
//System.out.println("writeTrailerToBuffer: bytesToWrite = " + bytesToWrite + ", record index len = " + recordLengths.size());
//System.out.println("writeTrailerToBuffer: bytesWritten = " + (int)bytesWritten);
//System.out.println("writeTrailerToBuffer: write trailer without index, buf limit so far = " + buffer.limit());
                int bytes = RecordHeader.writeTrailer(buffer, (int)bytesWritten,
                                                      recordNumber, recordLengths);
                bytesWritten += bytes;
                buffer.limit((int)bytesWritten);
            }
            catch (HipoException e) {/* never happen */}
        }
        else {
            // Write trailer with index

            // How many bytes are we writing here?
            int bytesToWrite = RecordHeader.HEADER_SIZE_BYTES + 4*recordLengths.size();
//System.out.println("writeTrailerToBuffer: bytesToWrite = " + bytesToWrite + ", record index len = " + recordLengths.size());

            // Make sure our buffer can hold everything
            if ((buffer.capacity() - (int) bytesWritten) < bytesToWrite) {
                throw new EvioException("not enough room in buffer");
            }

            try {
                // Place data into buffer - both header and index
//System.out.println("writeTrailerToBuffer: start writing at pos = " + bytesWritten);
                int bytes = RecordHeader.writeTrailer(buffer, (int) bytesWritten,
                                                      recordNumber, recordLengths);
                bytesWritten += bytes;
                buffer.limit((int)bytesWritten);
            }
            catch (HipoException e) {/* never happen */}
        }
    }

}
