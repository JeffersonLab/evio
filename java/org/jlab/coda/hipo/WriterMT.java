/*
 *   Copyright (c) 2016.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */

package org.jlab.coda.hipo;

import com.lmax.disruptor.AlertException;
import org.jlab.coda.jevio.EvioBank;
import org.jlab.coda.jevio.EvioNode;
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
 * This class is for writing Evio/HIPO files only (not buffers)
 * while multithreading the compression of data.<p>
 *
 * At the center of how this class works is an ultra-fast ring buffer containing
 * a store of empty records. As the user calls one of the {@link #addEvent} methods,
 * it gradually fills one of those empty records with data. When the record is full,
 * it's put back into the ring to wait for one of the compression thread to grab it and compress it.
 * After compression, it's again placed back into the ring and waits for a final thread to
 * write it to file. After being written, the record is freed up for reuse.
 * This entire ring functionality is encapsulated in 2 classes,
 * {@link RecordSupply} and {@link RecordRingItem}.
 *
 * @version 6.0
 * @since 6.0 8/10/17
 * @author gavalian
 * @author timmer
 */

public class WriterMT implements AutoCloseable {

    /** Number of bytes written to file/buffer at current moment. */
    private long writerBytesWritten;
    /** Byte array containing user Header. */
    private byte[] userHeader;
    /** Size in bytes of userHeader array. */
    int userHeaderLength;
    /** Evio format "first" event to store in file header's user header. */
    private byte[] firstEvent;
    /** Length in bytes of firstEvent. */
    private int firstEventLength;
    /** Max number of events an internal record can hold. */
    private int maxEventCount;
    /** Max number of uncompressed data bytes an internal record can hold. */
    private int maxBufferSize;
    /** Number which is incremented and stored with each successive written record starting at 1. */
    private int recordNumber = 1;
    /** Number of threads doing compression simultaneously. */
    private int compressionThreadCount = 1;

    /** Object for writing file. */
    private RandomAccessFile outStream;

    /** The file channel, used for writing a file, derived from outStream. */
    private FileChannel fileChannel;

    /** Header to write to file. */
    private FileHeader fileHeader;

    /** String containing evio-format XML dictionary to store in file header's user header. */
    String dictionary;

    /** If dictionary and or firstEvent exist, this buffer contains them both as a record. */
    ByteBuffer dictionaryFirstEventBuffer;

    /** Byte order of data to write to file. */
    private ByteOrder byteOrder = ByteOrder.LITTLE_ENDIAN;

    /** Current record being filled with data. */
    private RecordOutputStream outputRecord;

    /** Byte array large enough to hold a header/trailer. */
    private byte[] headerArray = new byte[RecordHeader.HEADER_SIZE_BYTES];

    /** Type of compression to use on file. Default is none. */
    private CompressionType compressionType = CompressionType.RECORD_UNCOMPRESSED;

    /** List of record lengths interspersed with record event counts
      * to be optionally written in trailer. */
    private ArrayList<Integer> recordLengths = new ArrayList<Integer>(1500);

    /** Fast, thread-safe, lock-free supply of records. */
    private RecordSupply supply;

    /** Thread used to write data to file/buffer. */
    private RecordWriter recordWriterThread;

    /** Threads used to compress data. */
    private RecordCompressor[] recordCompressorThreads;

    /** Current ring Item from which current record is taken. */
    private RecordRingItem ringItem;

    /** Do we add a last header or trailer to file/buffer? */
    private boolean addingTrailer = true;
    /** Do we add a record index to the trailer? */
    private boolean addTrailerIndex;
    /** Has close() been called? */
    private boolean closed;
    /** Has open() been called? */
    private boolean opened;
    /** Has the first record been written already? */
    private boolean firstRecordWritten;
    /** Has a dictionary been defined? */
    private boolean haveDictionary;
    /** Has a first event been defined? */
    private boolean haveFirstEvent;
    /** Has caller defined a file header's user-header which is not dictionary/first-event? */
    private boolean haveUserHeader;

    
    /**
     * Default constructor is single-threaded, no compression. Little endian.
     * <b>No</b> file is opened. Any file will have little endian byte order.
     * 1M max event count and 8M max buffer size.
     */
    public WriterMT() {
        fileHeader = new FileHeader(true); // evio file
        supply = new RecordSupply(8, byteOrder, compressionThreadCount, 0, 0, CompressionType.RECORD_UNCOMPRESSED);

        // Get a single blank record to start writing into
        ringItem = supply.get();
        outputRecord = ringItem.getRecord();
    }

    /**
     * Constructor with byte order. <b>No</b> file is opened.
     * Any dictionary will be placed in the user header which will create a conflict if
     * user tries to call {@link #open(String, byte[])} with another user header array.
     *
     * @param order byte order of written file
     * @param maxEventCount max number of events a record can hold.
     *                      Value &lt;= O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes a record can hold.
     *                      Value of &lt; 8MB results in default of 8MB.
     * @param compressionType type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip)
     * @param compressionThreads number of threads doing compression simultaneously
     * @param ringSize      number of records in supply ring, must be multiple of 2
     *                      and &gt;= compressionThreads.
     *
     * @throws IllegalArgumentException if invalid compression type.
     */
    public WriterMT(ByteOrder order, int maxEventCount, int maxBufferSize,
                    CompressionType compressionType, int compressionThreads, int ringSize)
            throws IllegalArgumentException {

        this(HeaderType.EVIO_FILE, order, maxEventCount, maxBufferSize,
             null, null, 0, compressionType, compressionThreads, false, ringSize);
    }


    /**
     * Constructor with byte order.
     * <b>No</b> file is opened.
     * Any dictionary will be placed in the user header which will create a conflict if
     * user tries to call {@link #open(String, byte[])} with another user header array.
     *
     * @param hType         the type of the file. If set to {@link HeaderType#HIPO_FILE},
     *                      the header will be written with the first 4 bytes set to HIPO.
     * @param order         byte order of written file
     * @param maxEventCount max number of events a record can hold.
     *                      Value &lt;= O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes a record can hold.
     *                      Value of &lt; 8MB results in default of 8MB.
     * @param dictionary    string holding an evio format dictionary to be placed in userHeader.
     * @param firstEvent    byte array containing an evio event to be included in userHeader.
     *                      It must be in the same byte order as the order argument.
     * @param firstEventLen number of valid bytes in firstEvent.
     * @param compType      type of data compression to do (one, lz4 fast, lz4 best, gzip)
     * @param compressionThreads number of threads doing compression simultaneously
     * @param addTrailerIndex if true, we add a record index to the trailer.
     * @param ringSize      number of records in supply ring, must be multiple of 2
     *                      and &gt;= compressionThreads.
     *
     * @throws IllegalArgumentException if invalid compression type.
     */
    public WriterMT(HeaderType hType, ByteOrder order,
                    int maxEventCount, int maxBufferSize,
                    String dictionary, byte[] firstEvent, int firstEventLen,
                    CompressionType compType, int compressionThreads,
                    boolean addTrailerIndex, int ringSize)
            throws IllegalArgumentException {

        this.dictionary = dictionary;
        this.firstEvent = firstEvent;
        if (firstEvent == null || firstEventLen < 0) {
            firstEventLen = 0;
        }
        firstEventLength     = firstEventLen;
        this.maxEventCount   = maxEventCount;
        this.maxBufferSize   = maxBufferSize;
        this.addTrailerIndex = addTrailerIndex;

        if (order != null) {
            byteOrder = order;
        }
        if (compType != null) {
            compressionType = compType;
        }
        compressionThreadCount = compressionThreads;

        if (hType == HeaderType.HIPO_FILE) {
            fileHeader = new FileHeader(false);
        } else {
            fileHeader = new FileHeader(true);
        }

        haveDictionary = dictionary.length() > 0;
        haveFirstEvent = (firstEvent != null && firstEventLen > 0);

        if (haveDictionary) {
            System.out.println("WriterMT CON: create dict, len = " + dictionary.length());
        }

        if (haveFirstEvent) {
            System.out.println("WriterMT CON: create first event, len = " + firstEventLen);
        }

        if (haveDictionary || haveFirstEvent)  {
            dictionaryFirstEventBuffer = createDictionaryRecord();
System.out.println("WriterMT CON: created dict/firstEv buffer, order = " + byteOrder +
        ", dic/fe buff remaining = " + dictionaryFirstEventBuffer.remaining());
        }

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

        supply = new RecordSupply(ringSize, byteOrder, compressionThreads,
                                  maxEventCount, maxBufferSize, compressionType);

        // Get a single blank record to start writing into
        ringItem = supply.get();
        outputRecord = ringItem.getRecord();

        // TODO: start up threads ?????
    }


    /**
     * Constructor with filename.
     * The output file will be created with no user header.
     * File byte order is little endian.
     * 
     * @param filename output file name
     * @throws HipoException if filename arg is null or bad.
     */
    public WriterMT(String filename) throws HipoException {
        this();
        open(filename);
    }

    /**
     * Constructor with filename and byte order.
     * The output file will be created with no user header.
     * 
     * @param filename      output file name
     * @param order         byte order of written file or null for default (little endian)
     * @param maxEventCount max number of events a record can hold.
     *                      Value of O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes a record can hold.
     *                      Value of &lt; 8MB results in default of 8MB.
     * @param compressionType type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip)
     * @param compressionThreads number of threads doing compression simultaneously
     * @param ringSize      number of records in supply ring, must be multiple of 2
     *                      and &gt;= compressionThreads.
     *
     * @throws HipoException if filename arg is null or bad.
     */
    public WriterMT(String filename, ByteOrder order, int maxEventCount, int maxBufferSize,
                    CompressionType compressionType, int compressionThreads, int ringSize)
        throws HipoException {

        this(order, maxEventCount, maxBufferSize, compressionType, compressionThreads, ringSize);
        open(filename);
    }

    //////////////////////////////////////////////////////////////////////

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
            catch (AlertException e) {
                 // We've been notified that an error has occurred
 System.out.println("   Compressor: thread " + num + " exiting due to error of some sort");
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

        /** Stop the thread. */
        void stopThread() {
            try {
                // Send signal to interrupt it
                this.interrupt();

                // Wait for it to stop
                this.join(1);

                if (this.isAlive()) {
                    // If that didn't work, send Alert signal to ring
                    supply.errorAlert();

                    this.join();
                    //std::cout << "RecordWriter JOINED from alert" << std::endl;
                }
            }
            catch (InterruptedException e) {}
        }

        /** Wait for the last item to be processed, then exit thread. */
        void waitForLastItem() {
            try {
                while (supply.getLastSequence() > lastSeqProcessed) {
                    Thread.yield();
                }
            }
            catch (Exception e) {}

            // Stop this thread, not the calling thread
            stopThread();
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
                    // Followed by events in record
                    recordLengths.add(header.getEntries());
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
            catch (AlertException e) {
                 // We've been notified that an error has occurred
 System.out.println("   Writer: thread exiting due to error of some sort");
             }
        }
    }


    /**
     * Create a buffer representation of a record
     * containing the dictionary and/or the first event.
     * No compression.
     * @return buffer representation of record containing dictionary and/or first event,
     *         of zero size if first event and dictionary don't exist.
     */
    ByteBuffer createDictionaryRecord() {
        return Writer.createRecord(dictionary, firstEvent,
                                   byteOrder, fileHeader, null);
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
     * Get the compression type for the file being written.
     * @return compression type for the file being written.
     */
    CompressionType getCompressionType() {return compressionType;}

    /**
     * Does this writer add a trailer to the end of the file?
     * @return true if this writer adds a trailer to the end of the file, else false.
     */
    public boolean addTrailer() {return addingTrailer;}

    /**
     * Set whether this writer adds a trailer to the end of the file.
     * @param addingTrailer if true, at the end of file, add an ending header (trailer)
     *                      with no index of records and no following data.
     *                      Update the file header to contain a file offset to the trailer.
     */
    public void addTrailer(boolean addingTrailer) {this.addingTrailer = addingTrailer;}

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
            addingTrailer = true;
        }
    }

    /**
     * Open a new file and write file header with no user header.
     * @param filename output file name
     * @throws HipoException if filename arg is null or bad,
     *                       if this method already called without being
     *                       followed by reset.
     */
    public final void open(String filename) throws HipoException {
        open(filename, null);
    }

    /**
     * Open a file and write file header with given user's header.
     * User header is automatically padded when written.
     * @param filename disk file name.
     * @param userHdr byte array representing the optional user's header.
     * @throws HipoException if filename arg is null or bad,
     *                       if this method already called without being
     *                       followed by reset.
     */
    public final void open(String filename, byte[] userHdr) throws HipoException {

        if (opened) {
           throw new HipoException("currently open, call reset() first");
        }

        if (filename == null || filename.length() < 1) {
            throw new HipoException("bad filename");
        }

        ByteBuffer fileHeaderBuffer;
        haveUserHeader = false;

        // User header given as arg has precedent
        if (userHdr != null) {
            haveUserHeader = true;
System.out.println("writerMT::open: given a valid user header to write");
            fileHeaderBuffer = createHeader(userHdr);
        }
        else {
            // If dictionary & firstEvent not defined and user header not given ...
            if (dictionaryFirstEventBuffer.remaining() < 1) {
                fileHeaderBuffer = createHeader((byte []) null);
            }
            // else place dictionary and/or firstEvent into
            // record which becomes user header
            else {
System.out.println("writerMT::open: given a valid dict/first ev header to write");
                fileHeaderBuffer = createHeader(dictionaryFirstEventBuffer);
            }
        }

        try {
            outStream = new RandomAccessFile(filename, "rw");
            fileChannel = outStream.getChannel();
            outStream.write(fileHeaderBuffer.array());

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

        opened = true;
    }
    
    /**
     * Convenience method that sets compression type for the file.
     * The compression type is set for internal record.
     * The record data will be compressed according to the given type.
     * @param compression compression type
     * @return this object
     */
    public final WriterMT setCompressionType(CompressionType compression) {
        outputRecord.getHeader().setCompressionType(compression);
        compressionType = compression;
        return this;
    }

    /**
     * Create and return a buffer containing a general file header
     * followed by the user header given in the argument.
     * If user header is not padded to 4-byte boundary, it's done here.
     * @param userHdr byte array containing a user-defined header, may be null.
     * @return ByteBuffer containing a file header followed by the user-defined header
     */
    public ByteBuffer createHeader(byte[] userHdr) {

        // Amount of user data in bytes
        int userHeaderBytes = 0;
        if (userHdr != null) {
            userHeaderBytes = userHdr.length;
        }
        fileHeader.reset();
        if (haveUserHeader) {
            fileHeader.setBitInfo(false, false, addTrailerIndex);
        }
        else {
            fileHeader.setBitInfo(haveFirstEvent, haveDictionary, addTrailerIndex);
        }
        fileHeader.setUserHeaderLength(userHeaderBytes);

        // Amount of user data in bytes + padding
        int totalLen = fileHeader.getLength();
        byte[] array = new byte[totalLen];
        ByteBuffer buf = ByteBuffer.wrap(array);
        buf.order(byteOrder);

        try {
            fileHeader.writeHeader(buf, 0);
        }
        catch (HipoException e) {/* never happen */}

        if (userHeaderBytes > 0) {
            System.arraycopy(userHdr, 0, array,
                             RecordHeader.HEADER_SIZE_BYTES, userHeaderBytes);
        }

        // Get ready to read, buffer position is still 0
        buf.limit(totalLen);
        return buf;
    }


    /**
     * Return a buffer with a general file header followed by the given user header (userHdr).
     * The buffer is cleared and set to desired byte order prior to writing.
     * If user header is not padded to 4-byte boundary, it's done here.
     *
     * @param userHdr buffer containing a user-defined header which must be READY-TO-READ!
     * @return buffer containing a file header followed by the user-defined header.
     */
    ByteBuffer createHeader(ByteBuffer userHdr) {

        // Amount of user data in bytes
        int userHeaderBytes = 0;
        if (userHdr != null) {
            userHeaderBytes = userHdr.remaining();
        }
        fileHeader.reset();
        if (haveUserHeader) {
            fileHeader.setBitInfo(false, false, addTrailerIndex);
        }
        else {
            fileHeader.setBitInfo(haveFirstEvent, haveDictionary, addTrailerIndex);
        }
        fileHeader.setUserHeaderLength(userHeaderBytes);

        int totalLen = fileHeader.getLength();
        byte[] array = new byte[totalLen];
        ByteBuffer buf = ByteBuffer.wrap(array);
        buf.order(byteOrder);

        try {
            fileHeader.writeHeader(buf, 0);
        }
        catch (HipoException e) {/* never happen */}

        if (userHeaderBytes > 0) {
            if (userHdr.hasArray()) {
                System.arraycopy(userHdr.array(), userHdr.arrayOffset() + userHdr.position(),
                                 array, RecordHeader.HEADER_SIZE_BYTES, userHeaderBytes);
            }
            else {
                buf.position(RecordHeader.HEADER_SIZE_BYTES);
                buf.put(userHdr);
                buf.position(0);
            }
        }

        // Get ready to read, buffer position is still 0
        buf.limit(totalLen);
        return buf;
    }


    /**
     * Write a general header as the last "header" or trailer in the file
     * optionally followed by an index of all record lengths.
     *
     * It's best <b>NOT</b> to call this directly. The way to write a trailer to
     * is to call {@link #addTrailer(boolean)} or {@link #addTrailerWithIndex(boolean)}.
     * Then when {@link #close()} is called, the trailer will be written.

     * @param writeIndex if true, write an index of all record lengths in trailer.
     * @param recordNum record number for trailing record.
     * @throws IOException if problems writing to file.
     */
    private void writeTrailer(boolean writeIndex, int recordNum) throws IOException {

        // Keep track of where we are right now which is just before trailer
        long trailerPosition = writerBytesWritten;

        // If we're NOT adding a record index, just write trailer
        if (!writeIndex) {
            try {
                RecordHeader.writeTrailer(headerArray, recordNum, byteOrder);
            }
            catch (HipoException e) {/* never happen */}
            
            writerBytesWritten += RecordHeader.HEADER_SIZE_BYTES;
            outStream.write(headerArray, 0, RecordHeader.HEADER_SIZE_BYTES);
        }
        else {
            // Write trailer with index

            // How many bytes are we writing here?
            int dataBytes = RecordHeader.HEADER_SIZE_BYTES + 4*recordLengths.size();

            // Make sure our array can hold everything
            if (headerArray.length < dataBytes) {
//System.out.println("Allocating byte array of " + dataBytes + " bytes in size");
                headerArray = new byte[dataBytes];
            }

            // Place data into headerArray - both header and index
            try {
                RecordHeader.writeTrailer(headerArray, 0, recordNum,
                                          byteOrder, recordLengths);
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
        if (writeIndex && addTrailerIndex) {
            outStream.seek(FileHeader.BIT_INFO_OFFSET);
            if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
                outStream.writeInt(Integer.reverseBytes(fileHeader.getBitInfoWord()));
            }
            else {
                outStream.writeInt(fileHeader.getBitInfoWord());
            }
        }
    }


    /**
     * Appends the record to the file.
     * Using this method in conjunction with {@link #addEvent(byte[], int, int)}
     * is not thread-safe.
     * @param rec record object
     * @throws IllegalArgumentException if arg's byte order is opposite to output endian.
     * @throws HipoException if we cannot replace internal buffer of the current record
     *                       if it needs to be expanded since the buffer was provided
     *                       by the user.
     */
    public void writeRecord(RecordOutputStream rec)
            throws IllegalArgumentException, HipoException {

        // Need to ensure that the given record has a byte order the same as output
        if (rec.getByteOrder() != byteOrder) {
            throw new IllegalArgumentException("byte order of record is wrong");
        }

        // If we have already written stuff into our current internal record ...
        if (outputRecord.getEventCount() > 0) {
            // Put it back in supply for compressing
            supply.publish(ringItem);

            // Now get another, empty record.
            // This may block if threads are busy compressing
            // and/or writing all records in supply.
            ringItem = supply.get();
            outputRecord = ringItem.getRecord();
        }

        // Copy rec into an empty record taken from the supply
        outputRecord.transferDataForReading(rec);

        // Put it back in supply for compressing (building)
        supply.publish(ringItem);

        // Get another
        ringItem = supply.get();
        outputRecord = ringItem.getRecord();
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
     * @throws HipoException if buffer arg is null.
     */
    public void addEvent(byte[] buffer, int offset, int length) throws HipoException {

        if (buffer == null) {
            throw new HipoException("arg is null");
        }

        // Try putting data into current record being filled
        boolean status = outputRecord.addEvent(buffer, offset, length);

        // If record is full ...
        if (!status) {
            // Put it back in supply for compressing
            supply.publish(ringItem);

            // Now get another, empty record.
            // This may block if threads are busy compressing
            // and/or writing all records in supply.
            ringItem = supply.get();
            outputRecord = ringItem.getRecord();

            // Adding the first event to a record is guaranteed to work
            outputRecord.addEvent(buffer, offset, length);
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
     * @throws HipoException if buffer arg is null.
     */
    public void addEvent(byte[] buffer) throws HipoException {
        addEvent(buffer, 0, buffer.length);
    }

    /**
     * Add a ByteBuffer to the internal record. If the length of
     * the buffer exceeds the maximum size of the record, the record
     * will be written to the file (compressed if the flag is set).
     * And another record will be obtained from the supply to receive the buffer.
     * Using this method in conjunction with writeRecord() is not thread-safe.
     * <b>The byte order of event's data must
     * match the byte order given in constructor!</b>
     *
     * @param buffer array to add to the file.
     * @throws HipoException if buffer arg is null or byte order is wrong.
     */
    public void addEvent(ByteBuffer buffer) throws HipoException {

        if (buffer == null) {
            throw new HipoException("arg is null");
        }

        if (buffer.order() != byteOrder) {
            throw new HipoException("buffer arg byte order is wrong");
        }

        boolean status = outputRecord.addEvent(buffer);

        // If record is full ...
        if (!status) {
            // Put it back in supply for compressing
            supply.publish(ringItem);

            // Now get another, empty record.
            // This may block if threads are busy compressing
            // and/or writing all records in supply.
            ringItem = supply.get();
            outputRecord = ringItem.getRecord();

            // Adding the first event to a record is guaranteed to work
            outputRecord.addEvent(buffer);
        }
    }

    /**
     * Add an EvioBank to the internal record. If the length of
     * the bank exceeds the maximum size of the record, the record
     * will be written to the file (compressed if the flag is set).
     * Internal record will be reset to receive new buffers.
     * Using this method in conjunction with writeRecord() is not thread-safe.
     *
     * @param bank event to add to the file.
     * @throws HipoException if arg is null.
     */
    public void addEvent(EvioBank bank) throws HipoException {

        if (bank == null) {
            throw new HipoException("arg is null");
        }

        boolean status = outputRecord.addEvent(bank);

        // If record is full ...
        if (!status) {
            // Put it back in supply for compressing
            supply.publish(ringItem);

            // Now get another, empty record.
            // This may block if threads are busy compressing
            // and/or writing all records in supply.
            ringItem = supply.get();
            outputRecord = ringItem.getRecord();

            // Adding the first event to a record is guaranteed to work
            outputRecord.addEvent(bank);
        }
    }

    /**
     * Add an EvioNode to the internal record. If the length of
     * the data exceeds the maximum size of the record, the record
     * will be written to the file (compressed if the flag is set).
     * Internal record will be reset to receive new buffers.
     * Using this method in conjunction with writeRecord() is not thread-safe.
     * <b>The byte order of node's data must
     * match the byte order given in constructor!</b>
     *
     * @param node node to add to the file.
     * @throws HipoException if node is null or does not correspond to a bank.
     */
    public void addEvent(EvioNode node) throws HipoException {

        if (node == null) {
            throw new HipoException("arg is null");
        }

        ByteBuffer buffer = node.getBuffer();

        if (buffer.order() != byteOrder) {
            throw new HipoException("node arg's buffer's byte order is wrong");
        }

        boolean status = outputRecord.addEvent(node);

        // If record is full ...
        if (!status) {
            // Put it back in supply for compressing
            supply.publish(ringItem);

            // Now get another, empty record.
            // This may block if threads are busy compressing
            // and/or writing all records in supply.
            ringItem = supply.get();
            outputRecord = ringItem.getRecord();

            // Adding the first event to a record is guaranteed to work
            outputRecord.addEvent(node);
        }
    }


    //---------------------------------------------------------------------

    /** Get this object ready for re-use.
     * Follow calling this with call to {@link #open(String)}. */
    public void reset() {
        outputRecord.reset();
        fileHeader.reset();
        writerBytesWritten = 0L;
        recordNumber = 1;
        addingTrailer = false;
    }

    /**
     * Close opened file. If the output record contains events,
     * they will be flushed to file. Trailer and its optional index
     * written if requested.<p>
     * <b>The addEvent or addRecord methods must no longer be called.</b>
     */
    public void close() {

        // If we're in the middle of building a record, send it off since we're done
        if (outputRecord.getEventCount() > 0) {
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
            int recordCount = (int)supply.getLastSequence() + 1;

            if (addingTrailer) {
                recordCount++;
                // Write the trailer
                writeTrailer(addTrailerIndex, recordCount);
            }

            // Need to update the record count in file header
            outStream.seek(FileHeader.RECORD_COUNT_OFFSET);
            if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
                outStream.writeInt(Integer.reverseBytes(recordCount));
            }
            else {
                outStream.writeInt(recordCount);
            }
            outStream.close();
            recordLengths.clear();

        } catch (IOException ex) {
            Logger.getLogger(WriterMT.class.getName()).log(Level.SEVERE, null, ex);
        }

        closed = true;
        opened = false;
    }
}
