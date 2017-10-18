/*
 *   Copyright (c) 2016.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */

package org.jlab.coda.hipo;

import org.jlab.coda.jevio.ByteDataTransformer;
import org.jlab.coda.jevio.EvioException;
import org.jlab.coda.jevio.Utilities;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.util.ArrayList;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * Class to write Evio/HIPO files.
 *
 * @version 6.0
 * @since 6.0 8/10/17
 * @author gavalian
 * @author timmer
 */

public class WriterMT implements AutoCloseable {

    /** Do we write to a file or a buffer? */
    private boolean toFile = true;

    // If writing to file ...

    /** Object for writing file. */
    private RandomAccessFile  outStream;
    /** The file channel, used for writing a file, derived from outStream. */
    private FileChannel  fileChannel;
    /** Header to write to file. */
    private FileHeader  fileHeader;

    // If writing to buffer ...

    /** The buffer being written to. */
    private ByteBuffer buffer;

    // For both files & buffers

    /** Fast, thread-safe, lock-free supply of records. */
    private RecordSupply supply;

    /** Number of threads doing compression simultaneously. */
    private int compressionThreadCount = 1;

    /** Threads used to compress data. */
    private RecordCompressor[] recordCompressorThreads;

    /** Thread used to write data to file/buffer. */
    private RecordWriter recordWriterThread;

    /** Byte order of data to write to file/buffer. */
    private ByteOrder byteOrder = ByteOrder.LITTLE_ENDIAN;

    /** Current record being filled with data. */
    private RecordOutputStream record;

    /** Current ring Item from which current record is taken. */
    private RecordRingItem ringItem;

    /** Byte array large enough to hold a header/trailer. */
    private byte[]  headerArray = new byte[RecordHeader.HEADER_SIZE_BYTES];
    /** Type of compression to use on file. Default is none. */
    private int    compressionType;
    /** Number of bytes written to file/buffer at current moment. */
    private long   writerBytesWritten;
    /** Number which is incremented and stored with each successive written record starting at 1. */
    private int   recordNumber = 1;
    /** Do we add a last header or trailer to file/buffer? */
    private boolean addTrailer;
    /** Do we add a record index to the trailer? */
    private boolean addTrailerIndex;

    /** List of record lengths to be optionally written in trailer. */
    private ArrayList<Integer> recordLengths = new ArrayList<Integer>(1500);

    /**
     * Default constructor. Compression is single-threaded LZ4. Little endian.
     * <b>No</b> file is opened. Any file will have little endian byte order.
     */
    public WriterMT() {
        compressionThreadCount = 1;
        fileHeader = new FileHeader(true); // evio file
        supply = new RecordSupply(8, byteOrder, compressionThreadCount,
                                  0, 0, 1);

        // Get a single blank record to start writing into
        ringItem = supply.get();
        record   = ringItem.getRecord();
    }

    /**
     * Constructor with byte order.
     * <b>No</b> file is opened.
     * Any dictionary will be placed in the user header which will create a conflict if
     * user tries to call {@link #open(String, byte[])} with another user header array.
     *
     * @param order byte order of written file
     * @param maxEventCount max number of events a record can hold.
     *                      Value <= O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes a record can hold.
     *                      Value of < 8MB results in default of 8MB.
     * @param compressionType type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip)
     * @param compressionThreads number of threads doing compression simultaneously
     * @param ringSize      number of records in supply ring, must be multiple of 2
     *                      and >= compressionThreads.
     *
     * @throws IllegalArgumentException if invalid compression type.
     */
    public WriterMT(ByteOrder order, int maxEventCount, int maxBufferSize,
                    int compressionType, int compressionThreads, int ringSize)
            throws IllegalArgumentException {

        if (order != null) {
            byteOrder = order;
        }
        
        if (compressionType > -1 && compressionType < 4) {
            this.compressionType = compressionType;
        }
        else {
            throw new IllegalArgumentException("compressionType must be 0,1,2,or 3");
        }

        compressionThreadCount = compressionThreads;

        // Number of ring items must be >= compressionThreads
        int finalRingSize = ringSize;
        if (finalRingSize < compressionThreads) {
            finalRingSize = compressionThreads;
        }
        // AND is must be multiple of 2
        finalRingSize = Utilities.powerOfTwo(finalRingSize, true);

        if (finalRingSize != ringSize) {
            System.out.println("WriterMT: change to ring size = " + finalRingSize);
        }

        fileHeader = new FileHeader(true);  // evio file
        supply = new RecordSupply(ringSize, byteOrder, compressionThreads,
                                  maxEventCount, maxBufferSize, compressionType);

        // Get a single blank record to start writing into
        ringItem = supply.get();
        record   = ringItem.getRecord();
    }

    /**
     * Constructor with filename.
     * The output file will be created with no user header.
     * File byte order is little endian.
     * @param filename output file name
     */
    public WriterMT(String filename){
        this();
        open(filename);
    }

    /**
     * Constructor with filename & byte order.
     * The output file will be created with no user header.
     * @param filename      output file name
     * @param order         byte order of written file or null for default (little endian)
     * @param maxEventCount max number of events a record can hold.
     *                      Value of O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes a record can hold.
     *                      Value of < 8MB results in default of 8MB.
     * @param compressionType type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip)
     * @param compressionThreads number of threads doing compression simultaneously
     * @param ringSize      number of records in supply ring, must be multiple of 2
     *                      and >= compressionThreads.
     */
    public WriterMT(String filename, ByteOrder order, int maxEventCount, int maxBufferSize,
                    int compressionType, int compressionThreads, int ringSize) {
        this(order, maxEventCount, maxBufferSize, compressionType, compressionThreads, ringSize);
        open(filename);
    }

    /**
     * Constructor for writing to a ByteBuffer. Byte order is taken from the buffer.
     * @param buf buffer in to which to write events and/or records.
     * @param maxEventCount max number of events a record can hold.
     *                      Value of O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes a record can hold.
     *                      Value of < 8MB results in default of 8MB.
     */
    public WriterMT(ByteBuffer buf, int maxEventCount, int maxBufferSize) {
        buffer = buf;
        byteOrder = buf.order();
    }


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
                // The first time thru, we need to release all records up to our first
                // in case there are < num records before close() is called.
                // This way close() is not waiting for thread #12 to get and subsequently
                // release items 0 - 11 when there were only 5 records total.
                // (threadNumber starts at 0).
                supply.release(num, num - 1);

                while (true) {
                    if (Thread.interrupted()) {
                        return;
                    }
//System.out.println("   Compressor: try getting record to compress");

                    // Get the next record for this thread to compress
                    RecordRingItem item = supply.getToCompress(num);
                    // Pull record out of wrapping object
                    RecordOutputStream record = item.getRecord();

                    // Set compression type and record #
                    RecordHeader header = record.getHeader();
                    // Fortunately for us, the record # is also the sequence # + 1 !
                    header.setRecordNumber((int)(item.getSequence() + 1L));
                    header.setCompressionType(compressionType);
//System.out.println("   Compressor: set record # to " + header.getRecordNumber());
                    // Do compression
                    record.build();
//System.out.println("   Compressor: got record, header = \n" + header);

//System.out.println("   Compressor: release item to supply");
                    // Release back to supply
                    supply.releaseCompressor(item);
                }
            }
            catch (InterruptedException e) {
                // We've been interrupted while blocked in getToCompress
                // which means we're all done.
System.out.println("   Compressor: thread " + num + " INTERRUPTED");
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

        /** Wait for the last item to be processed, then exit thread. */
        void waitForLastItem() {
            while (supply.getLastSequence() > lastSeqProcessed) {
                Thread.yield();
            }
            // Interrupt this thread, not the calling thread
            this.interrupt();
        }

        @Override
        public void run() {
            try {
                long currentSeq;

                while (true) {
                    if (Thread.interrupted()) {
                        return;
                    }

//System.out.println("   Writer: try getting record to write");
                    // Get the next record for this thread to write
                    RecordRingItem item = supply.getToWrite();
                    currentSeq = item.getSequence();
                    // Pull record out of wrapping object
                    RecordOutputStream record = item.getRecord();

                    // Do write
                    RecordHeader header = record.getHeader();
//System.out.println("   Writer: got record, header = \n" + header);
                    int bytesToWrite = header.getLength();
                    // Record length of this record
                    recordLengths.add(bytesToWrite);
                    writerBytesWritten += bytesToWrite;

                    try {
                        ByteBuffer buf = record.getBinaryBuffer();
                        if (buf.hasArray()) {
//System.out.println("   Writer: use outStream to write file, buf pos = " + buf.position() +
//        ", lim = " + buf.limit() + ", bytesToWrite = " + bytesToWrite);
                            outStream.write(buf.array(), 0, bytesToWrite);
                        }
                        else {
//System.out.println("   Writer: use fileChannel to write file");
                            // binary buffer is ready to read after build()
                            fileChannel.write(buf);
                        }
                        record.reset();
                    } catch (IOException ex) {
                        ex.printStackTrace();
                        Logger.getLogger(WriterMT.class.getName()).log(Level.SEVERE, null, ex);
                    }

                    // Release back to supply
//System.out.println("   Writer: release ring item back to supply");
                    supply.releaseWriter(item);

                    // Now we're done with this sequence
                    lastSeqProcessed = currentSeq;
                }
            }
            catch (InterruptedException e) {
                // We've been interrupted while blocked in getToWrite
                // which means we're all done.
System.out.println("   Writer: thread INTERRUPTED");
            }
        }
    }


    /**
     * Get the file's byte order.
     * @return file's byte order.
     */
    public ByteOrder getByteOrder() {return byteOrder;}

    /**
     * Get the file header.
     * @return file header.
     */
    public FileHeader getFileHeader() {return fileHeader;}

//    /**
//     * Get the internal record's header.
//     * @return internal record's header.
//     */
//    public RecordHeader getRecordHeader() {return record.getHeader();}

//    /**
//     * Get the internal record used to add events to file.
//     * @return internal record used to add events to file.
//     */
//    public RecordOutputStream getRecord() {return record;}

    /**
     * Does this writer add a trailer to the end of the file?
     * @return true if this writer adds a trailer to the end of the file, else false.
     */
    public boolean addTrailer() {return addTrailer;}

    /**
     * Set whether this writer adds a trailer to the end of the file.
     * @param addTrailer if true, at the end of file, add an ending header (trailer)
     *                   with no index of records and no following data.
     *                   Update the file header to contain a file offset to the trailer.
     */
    public void addTrailer(boolean addTrailer) {this.addTrailer = addTrailer;}

    /**
     * Does this writer add a trailer with a record index to the end of the file?
     * @return true if this writer adds a trailer with a record index
     *         to the end of the file, else false.
     */
    public boolean addTrailerWithIndex() {return addTrailerIndex;}

    /**
     * Set whether this writer adds a trailer with a record index to the end of the file.
     * @param addTrailingIndex if true, at the end of file, add an ending header (trailer)
     *                         with an index of all records but with no following data.
     *                         Update the file header to contain a file offset to the trailer.
     */
    public void addTrailerWithIndex(boolean addTrailingIndex) {
        addTrailerIndex = addTrailingIndex;
        if (addTrailingIndex) {
            addTrailer = true;
        }
    }

    /**
     * Open a new file and write file header with no user header.
     * @param filename output file name
     */
    public final void open(String filename) {open(filename, new byte[]{});}

    /**
     * Open a file and write file header with given user's header.
     * User header is automatically padded when written.
     * @param filename disk file name.
     * @param userHeader byte array representing the optional user's header.
     */
    public final void open(String filename, byte[] userHeader){

        if (userHeader == null) {
            userHeader = new byte[0];
        }

        try {
            outStream = new RandomAccessFile(filename, "rw");
            fileChannel = outStream.getChannel();
            // Create complete file header here (general file header + index array + user header)
            ByteBuffer headerBuffer = createHeader(userHeader);
            // Write this to file
            outStream.write(headerBuffer.array());

        } catch (FileNotFoundException ex) {
            Logger.getLogger(WriterMT.class.getName()).log(Level.SEVERE, null, ex);
        } catch (IOException ex) {
            Logger.getLogger(WriterMT.class.getName()).log(Level.SEVERE, null, ex);
        }

        writerBytesWritten = (long) (fileHeader.getLength());

        // Create and start compression threads
        recordCompressorThreads = new RecordCompressor[compressionThreadCount];
        for (int i=0; i < compressionThreadCount; i++) {
            recordCompressorThreads[i] = new RecordCompressor(i);
            recordCompressorThreads[i].start();
        }
        
        // Create and start writing thread
        recordWriterThread = new RecordWriter();
        recordWriterThread.start();

//        // Get a single blank record to start writing into
//        ringItem = supply.get();
//        record   = ringItem.getRecord();
    }
    
    /**
     * Convenience method that sets compression type for the file.
     * When writing to the file, record data will be compressed
     * according to the given type.
     * @param compression compression type
     * @return this object
     */
    public final WriterMT setCompressionType(int compression){
        if (compression > -1 && compression < 4) {
            compressionType = compression;
        }
        return this;
    }

    /**
     * Create and return a byte array containing a general file header
     * followed by the user header given in the argument.
     * If user header is not padded to 4-byte boundary, it's done here.
     * @param userHeader byte array containing a user-defined header
     * @return byte array containing a file header followed by the user-defined header
     */
    public ByteBuffer createHeader(byte[] userHeader){
        // Amount of user data in bytes
        int userHeaderBytes = userHeader.length;

        fileHeader.reset();
        fileHeader.setUserHeaderLength(userHeaderBytes);
        // Amount of user data in bytes + padding
        int userHeaderPaddedBytes = 4*fileHeader.getUserHeaderLengthWords();
        int bytes = RecordHeader.HEADER_SIZE_BYTES + userHeaderPaddedBytes;
        fileHeader.setLength(bytes);

        byte[] array = new byte[bytes];
        ByteBuffer buffer = ByteBuffer.wrap(array);
        buffer.order(byteOrder);

        try {
            fileHeader.writeHeader(buffer, 0);
        }
        catch (HipoException e) {/* never happen */}
        System.arraycopy(userHeader, 0, array,
                         RecordHeader.HEADER_SIZE_BYTES, userHeaderBytes);

        return buffer;
    }


    /**
     * Write a general header as the last "header" or trailer in the file
     * optionally followed by an index of all record lengths.
     * @param writeIndex if true, write an index of all record lengths in trailer.
     * @throws IOException if problems writing to file.
     */
    private void writeTrailer(boolean writeIndex) throws IOException {

        // Keep track of where we are right now which is just before trailer
        long trailerPosition = writerBytesWritten;

        // If we're NOT adding a record index, just write trailer
        if (!writeIndex) {
            try {
                FileHeader.writeTrailer(headerArray, recordNumber, byteOrder, null);
            }
            catch (HipoException e) {/* never happen */}
            writerBytesWritten += RecordHeader.HEADER_SIZE_BYTES;
            outStream.write(headerArray, 0, RecordHeader.HEADER_SIZE_BYTES);
        }
        else {
            // Create the index of record lengths in proper byte order
            byte[] recordIndex = new byte[4 * recordLengths.size()];
            try {
                for (int i = 0; i < recordLengths.size(); i++) {
                    ByteDataTransformer.toBytes(recordLengths.get(i), byteOrder,
                                                recordIndex, 4*i);
//System.out.println("Writing record length = " + recordOffsets.get(i) +
//", = 0x" + Integer.toHexString(recordOffsets.get(i)));
                }
            }
            catch (EvioException e) {/* never happen */}

            // Write trailer with index

            // How many bytes are we writing here?
            int dataBytes = RecordHeader.HEADER_SIZE_BYTES + recordIndex.length;

            // Make sure our array can hold everything
            if (headerArray.length < dataBytes) {
//System.out.println("Allocating byte array of " + dataBytes + " bytes in size");
                headerArray = new byte[dataBytes];
            }

            // Place data into headerArray - both header and index
            try {
                FileHeader.writeTrailer(headerArray, recordNumber,
                                          byteOrder, recordIndex);
            }
            catch (HipoException e) {/* never happen */}
            writerBytesWritten += dataBytes;
            outStream.write(headerArray, 0, dataBytes);
        }

        // Find & update file header's trailer position word
        outStream.seek(FileHeader.TRAILER_POSITION_OFFSET);
        if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
            outStream.writeLong(Long.reverseBytes(trailerPosition));
        }
        else {
            outStream.writeLong(trailerPosition);
        }

        // Find & update file header's bit-info word
        if (addTrailerIndex) {
            outStream.seek(RecordHeader.BIT_INFO_OFFSET);
            int bitInfo = fileHeader.setBitInfo(false, false, true);
            if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
                outStream.writeInt(Integer.reverseBytes(bitInfo));
            }
            else {
                outStream.writeInt(bitInfo);
            }
        }

        return;
    }





    /**
     * Write this record into one taken from the supply.
     * Using this method in conjunction with {@link #addEvent(byte[], int, int)}
     * is not thread-safe.
     * @param rec record object
     * @throws IllegalArgumentException if arg's byte order is opposite to record supply.
     */
    public void writeRecord(RecordOutputStream rec) throws IllegalArgumentException {

        // Need to ensure that the given record has a byte order the same
        // as all the records in the supply.
        if (rec.getByteOrder() != byteOrder) {
            throw new IllegalArgumentException("byte order of record is wrong");
        }

        // If we have already written stuff into our current internal record ...
        if (record.getEventCount() > 0) {
            // Put it back in supply for compressing
            supply.publish(ringItem);

            // Now get another, empty record.
            // This may block if threads are busy compressing
            // and/or writing all records in supply.
            ringItem = supply.get();
            record = ringItem.getRecord();
        }

        // Copy rec into an empty record taken from the supply
        record.copy(rec);

        // Make sure given record is consistent with this writer
        RecordHeader header = record.getHeader();
        header.setCompressionType(compressionType);
        header.setRecordNumber(recordNumber++);
        record.setByteOrder(byteOrder);

        // Put it back in supply for compressing
        supply.publish(ringItem);

        // Get another
        ringItem = supply.get();
        record = ringItem.getRecord();
    }

    /**
     * Add a byte array to the current internal record. If the length of
     * the buffer exceeds the maximum size of the record, the record
     * will be written to the file (compressed if the flag is set).
     * And another record will be obtained from the supply to receive the buffer.
     * Using this method in conjunction with {@link #writeRecord(RecordOutputStream)}
     * is not thread-safe.
     *
     * @param buffer array to add to the file.
     * @param offset offset into array from which to start writing data.
     * @param length number of bytes to write from array.
     */
    public void addEvent(byte[] buffer, int offset, int length) {
        // Try putting data into current record being filled
        boolean status = record.addEvent(buffer, offset, length);

        // If record is full ...
        if (!status) {
            // Put it back in supply for compressing
            supply.publish(ringItem);

            // Now get another, empty record.
            // This may block if threads are busy compressing
            // and/or writing all records in supply.
            ringItem = supply.get();
            record   = ringItem.getRecord();

            // Adding the first event to a record is guaranteed to work
            record.addEvent(buffer, offset, length);
        }
    }

    /**
     * Add a byte array to the current internal record. If the length of
     * the buffer exceeds the maximum size of the record, the record
     * will be written to the file (compressed if the flag is set).
     * And another record will be obtained from the supply to receive the buffer.
     * Using this method in conjunction with {@link #writeRecord(RecordOutputStream)}
     * is not thread-safe.
     *
     * @param buffer array to add to the file.
     */
    public void addEvent(byte[] buffer){
        addEvent(buffer, 0, buffer.length);
    }

    //---------------------------------------------------------------------

    /** Get this object ready for re-use.
     * Follow calling this with call to {@link #open(String)}. */
    public void reset() {
        record.reset();
        fileHeader.reset();
        writerBytesWritten = 0L;
        recordNumber = 1;
        addTrailer = false;
    }

    /**
     * Close opened file. If the output record contains events,
     * they will be flushed to file. Trailer and its optional index
     * written if requested.<p>
     * <b>The addEvent or addRecord methods must no longer be called.</b>
     */
    public void close(){

        // If we're in the middle of building a record, send it off since we're done
        if (record.getEventCount() > 0) {
            // Put it back in supply for compressing
            supply.publish(ringItem);
        }

        // Since the writer thread is the last to process each record,
        // wait until it's done with the last item, then exit the thread.
        recordWriterThread.waitForLastItem();

        // Stop all compressing threads which by now are stuck on get
        for (RecordCompressor rc : recordCompressorThreads) {
            rc.interrupt();
        }

        try {
            if (addTrailer) {
                // Write the trailer
                writeTrailer(addTrailerIndex);
            }

            outStream.close();
            //System.out.println("[writer] ---> bytes written " + writerBytesWritten);
        } catch (IOException ex) {
            Logger.getLogger(WriterMT.class.getName()).log(Level.SEVERE, null, ex);
        }
    }
}
