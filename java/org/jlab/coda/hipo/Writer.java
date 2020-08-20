/*
 *   Copyright (c) 2016.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */

package org.jlab.coda.hipo;

import org.jlab.coda.jevio.ByteDataTransformer;
import org.jlab.coda.jevio.EvioBank;
import org.jlab.coda.jevio.EvioException;
import org.jlab.coda.jevio.EvioNode;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.AsynchronousFileChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.concurrent.Future;

/**
 * Class to write Evio/HIPO files.
 * This class replaces the Writer class with a more efficient writing system.
 * Two asynchronous writes are done simultaneously in this class for greater
 * file writing speed.
 *
 * @version 6.0
 * @since 6.0 8/10/17
 * @author gavalian
 * @author timmer
 */

public class Writer implements AutoCloseable {

    /** Do we write to a file or a buffer? */
    private boolean toFile = true;

    // If writing to file ...

    /** The file channel, used for asynchronously writing a file. */
    private AsynchronousFileChannel asyncFileChannel;
    /** Header to write to file, created in constructor. */
    private FileHeader fileHeader;
    /** Objects to allow efficient, asynchronous file writing. */
    private Future<Integer> future1, future2;
    /** Index for selecting which future (1 or 2) to use for next file write. */
    private int futureIndex;
    /** Two internal records, first element last used in the future1 write,
     * the second last used in future2 write. */
    private RecordOutputStream[] usedRecords;
    /** Three internal records used for writing to a file. */
    private RecordOutputStream[] internalRecords;
    /** The location of the next write in the file. */
    private long fileWritingPosition;

    // If writing to buffer ...
    
    /** Buffer being written to. */
    private ByteBuffer buffer;

    // For both files & buffers

    /** Buffer containing user Header. */
    private ByteBuffer userHeaderBuffer;
    /** Byte array containing user Header. */
    private byte[] userHeader;
    /** Offset of valid data in userHeader. */
    private int userHeaderOffset;
    /** Number of valid bytes in userHeader (starting at userHeaderOffset). */
    private int userHeaderLength;

    /** String containing evio-format XML dictionary to store in file header's user header. */
    private String dictionary;
    /** If dictionary and or firstEvent exist, this buffer contains them both as a record. */
    private ByteBuffer dictionaryFirstEventBuffer;
    /** Evio format "first" event to store in file header's user header. */
    private byte[] firstEvent;

    /** Byte order of data to write to file/buffer. */
    private ByteOrder byteOrder = ByteOrder.LITTLE_ENDIAN;
    /** Record currently being filled. */
    private RecordOutputStream outputRecord;
    /** Byte array large enough to hold a header/trailer. */
    private byte[] headerArray = new byte[RecordHeader.HEADER_SIZE_BYTES];
    /** Byte array large enough to hold a header/trailer. */
    private ByteBuffer headerBuffer = ByteBuffer.wrap(headerArray);

    /** Type of compression to use on file. Default is none. */
    private CompressionType compressionType = CompressionType.RECORD_UNCOMPRESSED;

    /** List of record lengths interspersed with record event counts
     * to be optionally written in trailer. */
    private ArrayList<Integer> recordLengths = new ArrayList<Integer>(1500);

    /** Number of bytes written to file/buffer at current moment. */
    private long writerBytesWritten;
    /** Number which is incremented and stored with each successive written record starting at 1. */
    private int recordNumber = 1;

    /** Do we add a last header or trailer to file/buffer? */
    private boolean addTrailer = true;
    /** Do we add a record index to the trailer? */
    private boolean addTrailerIndex;
    /** Has close() been called? */
    private boolean closed;
    /** Has open() been called? */
    private boolean opened;
    /** Has the first record been written to buffer already? */
    private boolean firstRecordWritten;
    /** Has a dictionary been defined? */
    private boolean haveDictionary;
    /** Has a first event been defined? */
    private boolean haveFirstEvent;
    /** Has caller defined a file header's user-header which is not dictionary/first-event? */
    private boolean haveUserHeader;

    /**
     * Default constructor.
     * <b>No</b> file is opened. Any file will have little endian byte order.
     */
    public Writer() {
        outputRecord = new RecordOutputStream();
        fileHeader = new FileHeader(true);
        headerBuffer.order(byteOrder);
    }

    /**
     * Constructor with byte order. <b>No</b> file is opened.
     * File header type is evio file ({@link HeaderType#EVIO_FILE}).
     * @param order byte order of written file. Little endian if null.
     * @param maxEventCount max number of events a record can hold.
     *                      Value of O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes a record can hold.
     *                      Value of &lt; 8MB results in default of 8MB.
     */
    public Writer(ByteOrder order, int maxEventCount, int maxBufferSize) {
        this(HeaderType.EVIO_FILE, order, maxEventCount, maxBufferSize);
    }

    /**
     * Constructor with byte order and header type. <b>No</b> file is opened.
     * @param hType the type of the file. If set to {@link HeaderType#HIPO_FILE},
     *              the header will be written with the first 4 bytes set to HIPO.
     * @param order byte order of written file. Little endian if null.
     * @param maxEventCount max number of events a record can hold.
     *                      Value of O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes a record can hold.
     *                      Value of &lt; 8MB results in default of 8MB.
     */
    public Writer(HeaderType hType, ByteOrder order, int maxEventCount, int maxBufferSize) {
        this(hType, order, maxEventCount, maxBufferSize, null, null,
             CompressionType.RECORD_UNCOMPRESSED, false);
    }

    /**
     * Constructor with byte order. The given file is opened so any subsequent call to
     * open will fail.
     * This method places the dictionary and first event into the file header's user header.
     *
     * @param hType         the type of the file. If set to {@link HeaderType#HIPO_FILE},
     *                      the header will be written with the first 4 bytes set to HIPO.
     * @param order         byte order of written file. Little endian if null.
     * @param maxEventCount max number of events a record can hold.
     *                      Value of O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes a record can hold.
     *                      Value of &lt; 8MB results in default of 8MB.
     * @param dictionary    string holding an evio format dictionary to be placed in userHeader.
     * @param firstEvent    byte array containing an evio event to be included in userHeader.
     *                      It must be in the same byte order as the order argument.
     * @param compType      type of data compression to use.
     * @param addTrailerIndex if true, we add a record index to the trailer.
     */
    public Writer(HeaderType hType, ByteOrder order, int maxEventCount, int maxBufferSize,
                  String dictionary, byte[] firstEvent,
                  CompressionType compType, boolean addTrailerIndex) {

        if (order != null) {
            byteOrder = order;
        }
        this.dictionary = dictionary;
        this.firstEvent = firstEvent;
        this.compressionType = compType;
        this.addTrailerIndex = addTrailerIndex;
        headerBuffer.order(byteOrder);

        // Create a place to store records currently being written
        usedRecords = new RecordOutputStream[2];

        // Create some extra output records so we can be
        // asynchronously writing 2 while filling 1.
        internalRecords = new RecordOutputStream[3];

        internalRecords[0] = new RecordOutputStream(byteOrder, maxEventCount,
                                                    maxBufferSize, compType, hType);
        internalRecords[1] = new RecordOutputStream(byteOrder, maxEventCount,
                                                    maxBufferSize, compType, hType);
        internalRecords[2] = new RecordOutputStream(byteOrder, maxEventCount,
                                                    maxBufferSize, compType, hType);
        outputRecord = internalRecords[0];

        if (hType == HeaderType.HIPO_FILE){
            fileHeader = new FileHeader(false);
        } else {
            fileHeader = new FileHeader(true);
        }

        haveDictionary = dictionary != null;
        haveFirstEvent = (firstEvent != null && firstEvent.length > 0);

        if (haveDictionary || haveFirstEvent)  {
            dictionaryFirstEventBuffer = createDictionaryRecord();
        }
     }

    /**
     * Constructor with filename.
     * The output file will be created with no user header and therefore
     * {@link #open(String)} is called in this method and should not be called again.
     * File byte order is little endian.
     * @param filename output file name
     * @throws IOException  if file cannot be found or IO error writing to file
     */
    public Writer(String filename) throws IOException {
        this();
        try {
            open(filename);
        }
        catch (HipoException e) {/* never happen */}
    }

    /**
     * Constructor with filename and byte order.
     * The output file will be created with no user header and therefore
     * {@link #open(String)} is called in this method and should not be called again.
     * LZ4 compression.
     * @param filename      output file name
     * @param order         byte order of written file or null for default (little endian)
     * @param maxEventCount max number of events a record can hold.
     *                      Value of O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes a record can hold.
     *                      Value of &lt; 8MB results in default of 8MB.
     * @throws IOException  if file cannot be found or IO error writing to file
     */
    public Writer(String filename, ByteOrder order, int maxEventCount, int maxBufferSize)
            throws IOException {

        this(order, maxEventCount, maxBufferSize);
        try {
            open(filename);
        }
        catch (HipoException e) {/* never happen */}
    }

    //////////////////////////////////////////////////////////////////////

    /**
     * Constructor for writing to a ByteBuffer. Byte order is taken from the buffer.
     * No compression.
     * @param buf buffer in to which to write events and/or records.
     */
    public Writer(ByteBuffer buf) {
        this(buf, 0, 0, null, null);
    }

    /**
     * Constructor with byte order.
     * This method places the dictionary and first event into the file header's user header.
     * No compression.
     *
     * @param buf           buffer in to which to write events and/or records.
     * @param maxEventCount max number of events a record can hold.
     *                      Value of O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes a record can hold.
     *                      Value of &lt; 8MB results in default of 8MB.
     * @param dictionary    string holding an evio format dictionary to be placed in userHeader.
     * @param firstEvent    byte array containing an evio event to be included in userHeader.
     *                      It must be in the same byte order as the order argument.
     */
    public Writer(ByteBuffer buf, int maxEventCount, int maxBufferSize,
                  String dictionary, byte[] firstEvent) {

        byteOrder = buf.order();
        buffer = buf;
        headerBuffer.order(byteOrder);

        this.dictionary = dictionary;
        this.firstEvent = firstEvent;
        outputRecord = new RecordOutputStream(byteOrder, maxEventCount, maxBufferSize, CompressionType.RECORD_UNCOMPRESSED);

        haveDictionary = dictionary != null;
        haveFirstEvent = (firstEvent != null && firstEvent.length > 0);

        if (haveDictionary || haveFirstEvent)  {
            dictionaryFirstEventBuffer = createDictionaryRecord();
            // make this the user header by default since open() may not get called for buffers
            userHeader = dictionaryFirstEventBuffer.array();
            userHeaderLength = dictionaryFirstEventBuffer.remaining();
            userHeaderOffset = 0;
            userHeaderBuffer = dictionaryFirstEventBuffer;
        }

        toFile = false;
    }

    //////////////////////////////////////////////////////////////////////

    /**
     * Get the buffer being written to.
     * @return buffer being written to.
     */
    public ByteBuffer getBuffer() {return buffer;}

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
//    public RecordHeader getRecordHeader() {return outputRecord.getHeader();}
//
//    /**
//     * Get the internal record used to add events to file.
//     * @return internal record used to add events to file.
//     */
//    public RecordOutputStream getRecord() {return outputRecord;}

    /**
     * Convenience method that gets compression type for the file being written.
     * @return compression type for the file being written.
     */
    public CompressionType getCompressionType() {return compressionType;}

    /**
     * Convenience method that sets compression type for the file.
     * The compression type is also set for internal record(s).
     * When writing to the file, record data will be compressed
     * according to the given type.
     * @param compression compression type
     * @return this object
     */
    public final Writer setCompressionType(CompressionType compression) {
        if (toFile) {
            compressionType = compression;
            internalRecords[0].getHeader().setCompressionType(compression);
            internalRecords[1].getHeader().setCompressionType(compression);
            internalRecords[2].getHeader().setCompressionType(compression);
        }
        return this;
    }

    /**
     * Does this writer add a trailer to the end of the file/buffer?
     * @return true if this writer adds a trailer to the end of the file/buffer, else false.
     */
    public boolean addTrailer() {return addTrailer;}

    /**
     * Set whether this writer adds a trailer to the end of the file/buffer.
     * @param add if true, at the end of file/buffer, add an ending header (trailer)
     *            with no index of records and no following data.
     *            Update the file header to contain a file offset to the trailer.
     */
    public void addTrailer(boolean add) {addTrailer = add;}

    /**
     * Does this writer add a trailer with a record index to the end of the file?
     * Or, if writing to a buffer, is a trailer added with no index?
     * @return if writing to a file: true if this writer adds a trailer with a record index
     *         to the end of the file, else false. If writing to a buffer, true if this
     *         writer adds a traile to the end of the buffer, else false.
     */
    public boolean addTrailerWithIndex() {return addTrailerIndex;}

    /**
     * Set whether this writer adds a trailer with a record index to the end of the file.
     * Or, if writing to a buffer, set whether a trailer is added with no index.
     * @param addTrailingIndex if true, at the end of file, add an ending header (trailer)
     *                         with an index of all records but with no following data.
     *                         Update the file header to contain a file offset to the trailer.
     *                         If true, and writing to a buffer, add a trailer with no index
     *                         to the end of the buffer.
     */
    public void addTrailerWithIndex(boolean addTrailingIndex) {
        addTrailerIndex = addTrailingIndex;
        if (addTrailingIndex) {
            addTrailer = true;
        }
        
        if (!toFile) {
            addTrailerIndex = false;
        }
    }

    /**
     * Open a new file and write file header with no user header.
     * @param filename output file name
     * @throws HipoException if open already called without being followed by calling close.
     * @throws IOException if file cannot be found or IO error writing to file
     */
    public final void open(String filename) throws HipoException, IOException {
        open(filename, null);
    }

    /**
     * Open a file and write file header with given user header.
     * User header is automatically padded when written.
     * @param filename   name of file to write to.
     * @param userHeader byte array representing the optional user's header.
     *                   If this is null AND dictionary and/or first event are given,
     *                   the dictionary and/or first event will be placed in its
     *                   own record and written as the user header.
     * @throws HipoException filename arg is null, if constructor specified writing to a buffer,
     *                       or if open() was already called without being followed by reset().
     * @throws IOException   if file cannot be found or IO error writing to file
     */
    public final void open(String filename, byte[] userHeader)
            throws HipoException, IOException {

        if (opened) {
            throw new HipoException("currently open, call reset() first");
        }
        else if (!toFile) {
            throw new HipoException("can only write to a buffer, call open(buffer, userHeader)");
        }

        if (filename == null) {
            throw new HipoException("filename arg is null");
        }

        ByteBuffer headBuffer;
        haveUserHeader = false;

        // User header given as arg has precedent
        if (userHeader != null) {
            haveUserHeader = true;
            headBuffer = createHeader(userHeader);
        }
        else {
            // If dictionary & firstEvent not defined and user header not given ...
            if (dictionaryFirstEventBuffer == null) {
                userHeader = new byte[0];
                headBuffer = createHeader(userHeader);
            }
            // else treat buffer containing dictionary and/or firstEvent as user header
            else {
                headBuffer = createHeader(dictionaryFirstEventBuffer);
            }
        }

        // Write this to file
        // Path object corresponding to file currently being written
        Path currentFilePath = Paths.get(filename);

        asyncFileChannel = AsynchronousFileChannel.open(currentFilePath,
                                //StandardOpenOption.TRUNCATE_EXISTING,
                                StandardOpenOption.CREATE_NEW,
                                StandardOpenOption.CREATE,
                                StandardOpenOption.WRITE);

        asyncFileChannel.write(headBuffer, 0L);
        fileWritingPosition = fileHeader.getLength();
        writerBytesWritten = (long) (fileHeader.getLength());
        opened = true;
    }


    /**
     * Specify a buffer and write first record header with given user header.
     * User header is automatically padded when written.
     * @param buf        buffer to writer to.
     * @param userHeader byte array representing the optional user's header.
     *                   <b>Warning: this will not be used until first record is written!
     *                   So don't go changing it in the meantime!</b>
     *                   If this is null AND dictionary and/or first event are given,
     *                   the dictionary and/or first event will be placed in its
     *                   own record and written as the user header of the first record's
     *                   header.
     * @throws HipoException buf arg is null, if constructor specified writing to a file,
     *                       or if open() was already called without being followed by reset().
     */
    public final void open(ByteBuffer buf, byte[] userHeader) throws HipoException {
        open(buf, userHeader, 0, userHeader.length);
    }


    /**
     * Specify a buffer and write first record header with given user header.
     * User header is automatically padded when written.
     * @param buf        buffer to writer to.
     * @param userHdr    byte array representing the optional user's header.
     *                   <b>Warning: this will not be used until first record is written!
     *                   So don't go changing it in the meantime!</b>
     *                   If this is null AND dictionary and/or first event are given,
     *                   the dictionary and/or first event will be placed in its
     *                   own record and written as the user header of the first record's
     *                   header.
     * @param off        offset of valid data in userHdr.
     * @param len        length of valid data (bytes) in userHdr (starting at off).
     * @throws HipoException buf arg is null, if constructor specified writing to a file,
     *                       or if open() was already called without being followed by reset().
     */
    public final void open(ByteBuffer buf, byte[] userHdr, int off, int len) throws HipoException {

        if (opened) {
            throw new HipoException("currently open, call reset() first");
        }
        else if (toFile) {
            throw new HipoException("can only write to a file, call open(filename, userHeader)");
        }

        if (buf == null || off < 0 || len < 0) {
            throw new HipoException("bad arg");
        }

        if (userHdr == null) {
            if (dictionaryFirstEventBuffer != null) {
                userHeader = dictionaryFirstEventBuffer.array();
                userHeaderBuffer = dictionaryFirstEventBuffer;
                userHeaderOffset = 0;
                userHeaderLength = dictionaryFirstEventBuffer.remaining();
            }
            else {
                userHeader = null;
                userHeaderBuffer = null;
                userHeaderOffset = off;
                userHeaderLength = 0;
            }
        }
        else if (userHdr.length > 0 && len > 0) {
            userHeader = userHdr;
            userHeaderBuffer = ByteBuffer.wrap(userHdr).order(byteOrder);
            userHeaderOffset = off;
            userHeaderLength = len;
        }
        else if (dictionaryFirstEventBuffer != null) {
            userHeader = dictionaryFirstEventBuffer.array();
            userHeaderBuffer = dictionaryFirstEventBuffer;
            userHeaderOffset = 0;
            userHeaderLength = dictionaryFirstEventBuffer.remaining();
        }
        else {
            userHeader = null;
            userHeaderBuffer = null;
            userHeaderOffset = off;
            userHeaderLength = 0;
        }

        buffer = buf;
        buffer.order(byteOrder);

        opened = true;
    }

    /**
     * Create a buffer representation of a record
     * containing the dictionary and/or the first event.
     * No compression.
     * @return buffer representation of record
     *         containing dictionary and/or first event.
     *         Null if both are null.
     */
    private ByteBuffer createDictionaryRecord() {
        return createRecord(dictionary, firstEvent, byteOrder, fileHeader, null);
    }

    /**
     * Create a buffer representation of a record
     * containing dictionary and/or first event.
     * No compression.
     * @param dictionary   dictionary xml string
     * @param firstEvent   bytes representing evio event
     * @param byteOrder    byte order of returned byte array
     * @param fileHeader   file header to update with dictionary/first-event info (may be null).
     * @param recordHeader record header to update with dictionary/first-event info (may be null).
     * @return buffer representation of record
     *         containing dictionary and/or first event.
     *         Null if both are null.
     */
    static public ByteBuffer createRecord(String dictionary, byte[] firstEvent,
                                          ByteOrder byteOrder,
                                          FileHeader fileHeader,
                                          RecordHeader recordHeader) {

        if (dictionary == null && firstEvent == null) return null;

        // Create record.
        // Bit of chicken&egg problem, so start with default internal buf size.
        RecordOutputStream record = new RecordOutputStream(byteOrder, 2, 0,
                                                           CompressionType.RECORD_UNCOMPRESSED);

        // How much data we got?
        int bytes = 0;
        
        if (dictionary != null) {
            bytes += dictionary.length();
        }

        if (firstEvent != null) {
System.out.println("createRecord: add first event bytes " + firstEvent.length);
            bytes += firstEvent.length;
        }

        // If we have huge dictionary/first event ...
        if (bytes > record.getInternalBufferCapacity()) {
            record = new RecordOutputStream(byteOrder, 2, bytes,
                                            CompressionType.RECORD_UNCOMPRESSED);
        }

        // Add dictionary to record
        if (dictionary != null) {
            record.addEvent(dictionary.getBytes(StandardCharsets.US_ASCII));
            // Also need to set bits in headers
            if (fileHeader != null)   fileHeader.hasDictionary(true);
            if (recordHeader != null) recordHeader.hasDictionary(true);
        }

        // Add first event to record
        if (firstEvent != null) {
System.out.println("createRecord: add first event to record");
            record.addEvent(firstEvent);
            if (fileHeader   != null)   fileHeader.hasFirstEvent(true);
            if (recordHeader != null) recordHeader.hasFirstEvent(true);
        }

        // Make events into record. Pos = 0, limit = # valid bytes.
        record.build();

        // Ready-to-read buffer contains record data
        return record.getBinaryBuffer();
    }

    /**
     * Create and return a buffer containing a general file header
     * followed by the user header given in the argument.
     * If user header is not padded to 4-byte boundary, it's done here.
     * @param userHeader byte array containing a user-defined header, may be null.
     * @return buffer containing a file header followed by the user-defined header.
     * @throws HipoException if writing to buffer.
     */
    public ByteBuffer createHeader(byte[] userHeader) throws HipoException {
        if (!toFile) {
            throw new HipoException("call only if writing to file");
        }

//System.out.println("createHeader: IN, fe bit = " + fileHeader.hasFirstEvent());

        // Amount of user data in bytes
        int userHeaderBytes = 0;
        if (userHeader != null) {
            userHeaderBytes = userHeader.length;
        }
        fileHeader.reset();
        if (haveUserHeader) {
            fileHeader.setBitInfo(false, false, addTrailerIndex);
        }
        else {
            fileHeader.setBitInfo(haveFirstEvent, haveDictionary, addTrailerIndex);
        }
        fileHeader.setUserHeaderLength(userHeaderBytes);

//System.out.println("createHeader: after set user header len, fe bit = " + fileHeader.hasFirstEvent());
        int totalLen = fileHeader.getLength();
        byte[] array = new byte[totalLen];
        ByteBuffer buffer = ByteBuffer.wrap(array);
        buffer.order(byteOrder);

        try {
//System.out.println("createHeader: will write file header into buffer: hasFE = " + fileHeader.hasFirstEvent());
            fileHeader.writeHeader(buffer, 0);
        }
        catch (HipoException e) {/* never happen */}

        if (userHeaderBytes > 0) {
            System.arraycopy(userHeader, 0, array,
                             FileHeader.HEADER_SIZE_BYTES, userHeaderBytes);
        }

        // Get ready to read, buffer position is still 0
        buffer.limit(totalLen);
        return buffer;
    }

    /**
     * Create and return a buffer containing a general file header
     * followed by the user header given in the argument.
     * If user header is not padded to 4-byte boundary, it's done here.
     * @param userHeader buffer containing a user-defined header which must be READY-TO-READ!
     * @return buffer containing a file header followed by the user-defined header.
     * @throws HipoException if writing to buffer.
     */
    public ByteBuffer createHeader(ByteBuffer userHeader) throws HipoException {
        if (!toFile) {
            throw new HipoException("call only if writing to file");
        }
//System.out.println("createHeader: IN, fe bit = " + fileHeader.hasFirstEvent());

        // Amount of user data in bytes
        int userHeaderBytes = 0;
        if (userHeader != null) {
            userHeaderBytes = userHeader.remaining();
        }
        fileHeader.reset();
        if (haveUserHeader) {
            fileHeader.setBitInfo(false, false, addTrailerIndex);
        }
        else {
            fileHeader.setBitInfo(haveFirstEvent, haveDictionary, addTrailerIndex);
        }
        fileHeader.setUserHeaderLength(userHeaderBytes);

//System.out.println("createHeader: after set user header len, fe bit = " + fileHeader.hasFirstEvent());
        int totalLen = fileHeader.getLength();
        byte[] array = new byte[totalLen];
        ByteBuffer buffer = ByteBuffer.wrap(array);
        buffer.order(byteOrder);

        try {
//System.out.println("createHeader: will write file header into buffer: hasFE = " + fileHeader.hasFirstEvent());
            fileHeader.writeHeader(buffer, 0);
        }
        catch (HipoException e) {/* never happen */}

        if (userHeaderBytes > 0) {
            if (userHeader.hasArray()) {
                System.arraycopy(userHeader.array(),
                                 userHeader.arrayOffset() + userHeader.position(),
                                 array, FileHeader.HEADER_SIZE_BYTES, userHeaderBytes);
            }
            else {
                userHeader.get(array, 0, userHeaderBytes);
            }
        }

        // Get ready to read, buffer position is still 0
        buffer.limit(totalLen);
        return buffer;
    }

    /**
     * Write a general header as the last "header" or trailer
     * optionally followed by an index of all record lengths.
     * It's best <b>NOT</b> to call this directly. The way to write a trailer to
     * file or buffer is to call {@link #addTrailer(boolean)} or
     * {@link #addTrailerWithIndex(boolean)}. Then when {@link #close()} is
     * called, the trailer will be written.
     * @param writeIndex if true, write an index of all record lengths in trailer.
     * @param recordNum record number for trailing record.
     * @throws IOException if error writing to file.
     */
    private void writeTrailer(boolean writeIndex, int recordNum) throws IOException {

        // If we're NOT adding a record index, just write trailer
        if (!writeIndex) {
            try {
                RecordHeader.writeTrailer(headerArray, recordNum, byteOrder);
                // TODO: not really necessary to keep track here?
                writerBytesWritten += RecordHeader.HEADER_SIZE_BYTES;

                if (toFile) {
                    headerBuffer.limit(RecordHeader.HEADER_SIZE_BYTES).position(0);
                    Future f = asyncFileChannel.write(headerBuffer, fileWritingPosition);
                    try {f.get();}
                    catch (Exception e) {
                        throw new IOException(e);
                    }
                    // TODO: this is not necessary since it's the last write??
                    fileWritingPosition += RecordHeader.HEADER_SIZE_BYTES;
                }
                else {
                    buffer.put(headerArray, 0, RecordHeader.HEADER_SIZE_BYTES);
                }
            }
            catch (HipoException ex) {
                // never happen
            }
            return;
        }

        // Write trailer with index

        // How many bytes are we writing here?
        int dataBytes = RecordHeader.HEADER_SIZE_BYTES + 4*recordLengths.size();

        // Make sure our array can hold everything
        if (headerArray.length < dataBytes) {
//System.out.println("Allocating byte array of " + dataBytes + " bytes in size");
            headerArray = new byte[dataBytes];
            headerBuffer = ByteBuffer.wrap(headerArray);
            headerBuffer.order(byteOrder);
        }

        try {
            // Place data into headerArray - both header and index
            RecordHeader.writeTrailer(headerArray, 0, recordNum,
                                      byteOrder, recordLengths);
            // TODO: not really necessary to keep track here?
            writerBytesWritten += dataBytes;
            if (toFile) {
                headerBuffer.limit(dataBytes).position(0);
                Future f = asyncFileChannel.write(headerBuffer, fileWritingPosition);
                try {f.get();}
                catch (Exception e) {
                    throw new IOException(e);
                }
                // TODO: this is not necessary since it's the last write??
                fileWritingPosition += dataBytes;
            }
            else {
                buffer.put(headerArray, 0, dataBytes);
            }
        }
        catch (HipoException ex) {
            // never happen
        }
    }

    /**
     * Appends the record to the file.
     * Using this method in conjunction with addEvent() is not thread-safe.
     * @param record record object
     * @throws IllegalArgumentException if arg's byte order is opposite to output endian.
     * @throws IOException if error writing to file.
     */
    public void writeRecord(RecordOutputStream record) throws IOException {
        // Need to ensure that the given record has a byte order the same as output
        if (record.getByteOrder() != byteOrder) {
            throw new IllegalArgumentException("byte order of record is wrong");
        }

        // If we have already written stuff into our current internal record,
        // write that first.
        if (outputRecord.getEventCount() > 0) {
            writeOutput();
        }

        // Wait for previous (if any) file writes to finish
        waitForFileWrites();

        RecordHeader header = record.getHeader();

        // Make sure given record is consistent with this writer
        header.setCompressionType(compressionType);
        header.setRecordNumber(recordNumber++);
        record.build();

        int bytesToWrite = header.getLength();
        // Record length of this record
        recordLengths.add(bytesToWrite);
        // Followed by events in record
        recordLengths.add(header.getEntries());
        writerBytesWritten += bytesToWrite;

        if (toFile) {
            // After call to build(), binary buffer is read to read
            Future f = asyncFileChannel.write(record.getBinaryBuffer(), fileWritingPosition);
            try {f.get();}
            catch (Exception e) {
                throw new IOException(e);
            }
            fileWritingPosition += bytesToWrite;
        }
        else {
            buffer.put(record.getBinaryBuffer().array(), 0, bytesToWrite);
        }
    }

    // Use internal outputRecordStream to write individual events

    /**
     * Add a byte array to the internal record. If the length of
     * the buffer exceeds the maximum size of the record, the record
     * will be written to the file (compressed if the flag is set).
     * Internal record will be reset to receive new buffers.
     * Using this method in conjunction with writeRecord() is not thread-safe.
     * <b>The byte order of event's byte array must
     * match the byte order given in constructor!</b>
     *
     * @param buffer array to add to the file.
     * @throws IOException if cannot write to file.
     */
    public void addEvent(byte[] buffer) throws IOException {
        addEvent(buffer,0,buffer.length);
    }

    /**
     * Add a byte array to the internal record. If the length of
     * the buffer exceeds the maximum size of the record, the record
     * will be written to the file (compressed if the flag is set).
     * Internal record will be reset to receive new buffers.
     * Using this method in conjunction with writeRecord() is not thread-safe.
     * <b>The byte order of event's byte array must
     * match the byte order given in constructor!</b>
     *
     * @param buffer array to add to the file.
     * @param offset offset into array from which to start writing data.
     * @param length number of bytes to write from array.
     * @throws IOException if cannot write to file.
     */
    public void addEvent(byte[] buffer, int offset, int length) throws IOException {
        boolean status = outputRecord.addEvent(buffer, offset, length);
        if (!status){
            writeOutput();
            outputRecord.addEvent(buffer, offset, length);
        }
    }

    /**
     * Add a ByteBuffer to the internal record. If the length of
     * the buffer exceeds the maximum size of the record, the record
     * will be written to the file (compressed if the flag is set).
     * Internal record will be reset to receive new buffers.
     * Using this method in conjunction with writeRecord() is not thread-safe.
     * <b>The byte order of event's data must
     * match the byte order given in constructor!</b>
     *
     * @param buffer array to add to the file.
     * @throws HipoException if buf arg's byte order is wrong.
     * @throws IOException if cannot write to file.
     */
    public void addEvent(ByteBuffer buffer) throws IOException, HipoException {
        if (buffer.order() != byteOrder) {
            throw new HipoException("buffer arg byte order is wrong");
        }

        boolean status = outputRecord.addEvent(buffer);
        if (!status){
            writeOutput();
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
     * @throws IOException if cannot write to file.
     */
    public void addEvent(EvioBank bank) throws IOException {
        boolean status = outputRecord.addEvent(bank);
        if (!status){
            writeOutput();
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
     * @throws HipoException if node does not correspond to a bank.
     * @throws IOException if cannot write to file.
     */
    public void addEvent(EvioNode node) throws HipoException, IOException {
        boolean status = outputRecord.addEvent(node);
        if (!status){
            writeOutput();
            outputRecord.addEvent(node);
        }
    }


    /** Wait for all files writes to finish. */
    private void waitForFileWrites() {
        if (!toFile) return;

        boolean write1IsDone = true, write2IsDone = true;
        if (future1 != null) write1IsDone = future1.isDone();
        if (future2 != null) write2IsDone = future2.isDone();

        while (!(write1IsDone && write2IsDone)) {
            try {
                Thread.sleep(1);
            }
            catch (InterruptedException e) {}

            if (future1 != null) write1IsDone = future1.isDone();
            if (future2 != null) write2IsDone = future2.isDone();
        }
    }


    /**
     * Write internal record with incremented record # to file or buffer.
     * @throws IOException if cannot write to file.
     */
    private void writeOutput() throws IOException {
        if (!toFile) {
            writeOutputToBuffer();
            return;
        }

        // Which record do we fill next?
        RecordOutputStream unusedRecord;

        // We need one of the 2 future jobs to be completed in order to proceed

        // If 1st time thru, proceed without waiting
        if (future1 == null) {
            futureIndex = 0;
            // Fill 2nd record next
            unusedRecord = internalRecords[1];
        }
        // If 2nd time thru, proceed without waiting
        else if (future2 == null) {
            futureIndex = 1;
            // Fill 3rd record next
            unusedRecord = internalRecords[2];
        }
        // After first 2 times, wait until one of the
        // 2 futures is finished before proceeding
        else {
            // If future1 is finished writing, proceed
            if (future1.isDone()) {
                futureIndex = 0;
                // Reuse the record future1 just finished writing
                unusedRecord = usedRecords[0];
            }
            // If future2 is finished writing, proceed
            else if (future2.isDone()) {
                futureIndex = 1;
                unusedRecord = usedRecords[1];
            }
            // Neither are finished, so wait for one of them to finish
            else {
                // If the last write to be submitted was future2, wait for 1
                if (futureIndex == 0) {
                    try {
                        // Wait for write to end before we continue
                        future1.get();
                        unusedRecord = usedRecords[0];
                    }
                    catch (Exception e) {
                        throw new IOException(e);
                    }
                }
                // Otherwise, wait for 2
                else {
                    try {
                        future2.get();
                        unusedRecord = usedRecords[1];
                    }
                    catch (Exception e) {
                        throw new IOException(e);
                    }
                }
            }
        }

        RecordHeader header = outputRecord.getHeader();
        header.setRecordNumber(recordNumber++);
        header.setCompressionType(compressionType);
        outputRecord.build();

        // Length of this record
        int bytesToWrite = header.getLength();
        int eventCount   = header.getEntries();

        // Trailer's index has length followed by count
        recordLengths.add(bytesToWrite);
        recordLengths.add(eventCount);
        writerBytesWritten += bytesToWrite;

        // Data to write
        ByteBuffer buf = outputRecord.getBinaryBuffer();

        if (futureIndex == 0) {
            future1 = asyncFileChannel.write(buf, fileWritingPosition);
            // Keep track of which buffer future1 used so it can be reused when done
            usedRecords[0] = outputRecord;
            futureIndex = 1;
        }
        else {
            future2 = asyncFileChannel.write(buf, fileWritingPosition);
            usedRecords[1] = outputRecord;
            futureIndex = 0;
        }

        // Next buffer to work with
        outputRecord = unusedRecord;
        outputRecord.reset();

        // Keep track of what is written to this, one, file
        fileWritingPosition += bytesToWrite;
    }


    /** Write internal record with incremented record # to buffer. */
    private void writeOutputToBuffer() {
        RecordHeader header = outputRecord.getHeader();
        header.setRecordNumber(recordNumber++);
        header.setCompressionType(compressionType);

        // For writing to buffer, user header cannot be written
        // in file header since there is none. So write it into
        // first record header instead.
        if (!firstRecordWritten) {
            // Create a buffer out of userHeader array or dictionary/first event.
            // User header given as arg has precedent.
            // This will take care of any unpadded user header data.
            outputRecord.build(userHeaderBuffer);
            firstRecordWritten = true;
        }
        else {
            outputRecord.build();
        }
        
        int bytesToWrite = header.getLength();
        // Record length of this record
        recordLengths.add(bytesToWrite);
        // Followed by events in record
        recordLengths.add(header.getEntries());
        writerBytesWritten += bytesToWrite;

        if (outputRecord.getBinaryBuffer().hasArray()) {
            if (buffer.hasArray()) {
                System.arraycopy(outputRecord.getBinaryBuffer().array(), 0,
                                 buffer.array(), buffer.arrayOffset() + buffer.position(),
                                 bytesToWrite);
            }
            else {
                buffer.put(outputRecord.getBinaryBuffer().array(), 0, bytesToWrite);
            }
        }
        else {
            if (buffer.hasArray()) {
                outputRecord.getBinaryBuffer().get(buffer.array(),
                                                   buffer.arrayOffset() + buffer.position(),
                                                   bytesToWrite);
            }
            else {
                buffer.put(outputRecord.getBinaryBuffer());
            }
        }
        
        outputRecord.reset();
    }


    //---------------------------------------------------------------------


    /** Get this object ready for re-use.
     * Follow calling this with call to {@link #open(String)}. */
    public void reset() {
        outputRecord.reset();
        fileHeader.reset();
        writerBytesWritten = 0L;
        recordNumber = 1;
        addTrailer = false;
        firstRecordWritten = false;

        closed = false;
        opened = false;
    }


    /**
     * Close opened file. If the output record contains events,
     * they will be flushed to file/buffer. Trailer and its optional index
     * written if requested.
     * @throws IOException if error writing to file
     */
    public void close() throws IOException {
        if (closed) return;

        if (outputRecord.getEventCount() > 0) {
            writeOutput();
        }

        int recordCount = recordNumber - 1;

        if (addTrailer) {
            recordCount++;

            // Keep track of where we are right now which is just before trailer
            long trailerPosition = writerBytesWritten;

            // Write the trailer
            writeTrailer(addTrailerIndex, recordCount);

            if (toFile) {
                try {
                    // Find & update file header's trailer position word
                    ByteBuffer bb = ByteBuffer.allocate(8);
                    bb.order(byteOrder);
                    bb.putLong(0, trailerPosition);
                    Future future = asyncFileChannel.write(bb, FileHeader.TRAILER_POSITION_OFFSET);
                    future.get();
                }
                catch (Exception e) {
                    e.printStackTrace();
                }

                // Find & update file header's bit-info word
                if (addTrailerIndex) {
                    try {
                        int bitInfo = fileHeader.hasTrailerWithIndex(true);

                        ByteBuffer bb = ByteBuffer.allocate(4);
                        bb.order(byteOrder);
                        bb.putInt(0, bitInfo);
                        Future future = asyncFileChannel.write(bb, FileHeader.BIT_INFO_OFFSET);
                        future.get();
                    }
                    catch (Exception e) {
                        e.printStackTrace();
                    }
                }
            }
        }

        if (toFile) {
            // Finish async writes to current file
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

            try {
                // Find & update file header's record count word
                ByteBuffer bb = ByteBuffer.allocate(4);
                bb.order(byteOrder);
                bb.putInt(0, recordCount);
                Future future = asyncFileChannel.write(bb, FileHeader.RECORD_COUNT_OFFSET);
                future.get();
            }
            catch (Exception e) {
                e.printStackTrace();
            }

            asyncFileChannel.close();
            recordLengths.clear();
        }
        
        closed = true;
        opened = false;
       //System.out.println("[writer] ---> bytes written " + writerBytesWritten);
    }

}
