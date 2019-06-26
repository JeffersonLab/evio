/*
 *   Copyright (c) 2016.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */

package org.jlab.coda.hipo;

import org.jlab.coda.jevio.*;

import java.io.IOException;
import java.io.RandomAccessFile;
import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.util.ArrayList;

/**
 * Class to write Evio/HIPO files.
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

    /** Object for writing file. */
    private RandomAccessFile outStream;
    /** The file channel, used for writing a file, derived from outStream. */
    private FileChannel fileChannel;
    /** Header to write to file, created in constructor. */
    private FileHeader fileHeader;

    // If writing to buffer ...
    
    /** Buffer being written to. */
    private ByteBuffer buffer;
    /** Buffer containing user Header. */
    private ByteBuffer userHeaderBuffer;
    /** Byte array containing user Header. */
    private byte[] userHeader;
    /** Has the first record been written already? */
    private boolean firstRecordWritten;

    // For both files & buffers

    /** String containing evio-format XML dictionary to store in file header's user header. */
    private String dictionary;
    /** Evio format "first" event to store in file header's user header. */
    private byte[] firstEvent;
    /** If dictionary and or firstEvent exist, this buffer contains them both as a record. */
    ByteBuffer dictionaryFirstEventBuffer;

    /** Byte order of data to write to file/buffer. */
    private ByteOrder byteOrder = ByteOrder.LITTLE_ENDIAN;
    /** Internal Record. */
    private RecordOutputStream outputRecord;
    /** Byte array large enough to hold a header/trailer. */
    private byte[] headerArray = new byte[RecordHeader.HEADER_SIZE_BYTES];
    /** Type of compression to use on file. Default is none. */
    private int compressionType;
    /** Number of bytes written to file/buffer at current moment. */
    private long writerBytesWritten;
    /** Number which is incremented and stored with each successive written record starting at 1. */
    private int recordNumber = 1;
    /** Do we add a last header or trailer to file/buffer? */
    private boolean addTrailer;
    /** Do we add a record index to the trailer? */
    private boolean addTrailerIndex;

    /** Has close() been called? */
    private boolean closed;
    /** Has open() been called? */
    private boolean opened;

    /** List of record lengths interspersed with record event counts
     * to be optionally written in trailer. */
    private ArrayList<Integer> recordLengths = new ArrayList<Integer>(1500);

    /**
     * Default constructor.
     * <b>No</b> file is opened. Any file will have little endian byte order.
     */
    public Writer(){
        outputRecord = new RecordOutputStream();
        fileHeader = new FileHeader(true);
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
    public Writer(ByteOrder order, int maxEventCount, int maxBufferSize){
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
        this(hType, order, maxEventCount, maxBufferSize, null, null);
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
     */
    public Writer(HeaderType hType, ByteOrder order, int maxEventCount, int maxBufferSize,
                  String dictionary, byte[] firstEvent) {
        if (order != null) {
            byteOrder = order;
        }
        this.dictionary = dictionary;
        this.firstEvent = firstEvent;
        outputRecord = new RecordOutputStream(order, maxEventCount, maxBufferSize, 1);

        if ( (dictionary != null && dictionary.length() > 0) ||
             (firstEvent != null && firstEvent.length   > 0))  {
            dictionaryFirstEventBuffer = createDictionaryRecord();
        }

        if (hType == HeaderType.HIPO_FILE){
            fileHeader = new FileHeader(false);
        } else {
            fileHeader = new FileHeader(true);
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
     * Constructor with filename & byte order.
     * The output file will be created with no user header and therefore
     * {@link #open(String)} is called in this method and should not be called again.
     * LZ4 compression.
     * @param filename      output file name
     * @param order         byte order of written file or null for default (little endian)
     * @param maxEventCount max number of events a record can hold.
     *                      Value of O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes a record can hold.
     *                      Value of < 8MB results in default of 8MB.
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
        this(buf, buf.order(), 0, 0, null, null);
    }

    /**
     * Constructor with byte order.
     * This method places the dictionary and first event into the file header's user header.
     * No compression.
     *
     * @param buf           buffer in to which to write events and/or records.
     * @param order         byte order of data to be written. Little endian if null.
     * @param maxEventCount max number of events a record can hold.
     *                      Value of O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes a record can hold.
     *                      Value of < 8MB results in default of 8MB.
     * @param dictionary    string holding an evio format dictionary to be placed in userHeader.
     * @param firstEvent    byte array containing an evio event to be included in userHeader.
     *                      It must be in the same byte order as the order argument.
     */
    public Writer(ByteBuffer buf, ByteOrder order, int maxEventCount, int maxBufferSize,
                  String dictionary, byte[] firstEvent) {
// TODO: why are dict and firstEv defined if never used with buffer????
        if (order != null) {
            byteOrder = order;
        }
        buffer = buf;
        buf.order(byteOrder);
        
        this.dictionary = dictionary;
        this.firstEvent = firstEvent;
        outputRecord = new RecordOutputStream(order, maxEventCount, maxBufferSize, 0);

        if ( (dictionary != null && dictionary.length() > 0) ||
             (firstEvent != null && firstEvent.length   > 0))  {
            dictionaryFirstEventBuffer = createDictionaryRecord();
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

    /**
     * Get the internal record's header.
     * @return internal record's header.
     */
    public RecordHeader getRecordHeader() {return outputRecord.getHeader();}

    /**
     * Get the internal record used to add events to file.
     * @return internal record used to add events to file.
     */
    public RecordOutputStream getRecord() {return outputRecord;}

    /**
     * Does this writer add a trailer to the end of the file/buffer?
     * @return true if this writer adds a trailer to the end of the file/buffer, else false.
     */
    public boolean addTrailer() {return addTrailer;}

    /**
     * Set whether this writer adds a trailer to the end of the file/buffer.
     * @param addTrailer if true, at the end of file/buffer, add an ending header (trailer)
     *                   with no index of records and no following data.
     *                   Update the file header to contain a file offset to the trailer.
     */
    public void addTrailer(boolean addTrailer) {this.addTrailer = addTrailer;}

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

        ByteBuffer headerBuffer;

        // User header given as arg has precedent
        if (userHeader != null) {
            headerBuffer = createHeader(userHeader);
        }
        else {
            // If dictionary & firstEvent not defined and user header not given ...
            if (dictionary == null && firstEvent == null) {
                userHeader = new byte[0];
                headerBuffer = createHeader(userHeader);
            }
            // else treat buffer containing dictionary and/or firstEvent as user header
            else {
                headerBuffer = createHeader(dictionaryFirstEventBuffer);
            }
        }

//        System.out.println("open: fe bit = " + fileHeader.hasFirstEvent());
//        // Create complete file header here (general file header + index array + user header)
//        ByteBuffer headerBuffer = createHeader(userHeader);
//        System.out.println("open: after create Header, fe bit = " + fileHeader.hasFirstEvent());

        // Write this to file
        outStream = new RandomAccessFile(filename, "rw");
        fileChannel = outStream.getChannel();
        outStream.write(headerBuffer.array());

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

        if (opened) {
            throw new HipoException("currently open, call reset() first");
        }
        else if (toFile) {
            throw new HipoException("can only write to a file, call open(filename, userHeader)");
        }

        if (buf == null) {
            throw new HipoException("buf arg is null");
        }

        buffer = buf;
        buffer.order(byteOrder);
        this.userHeader = userHeader;
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
        RecordOutputStream record = new RecordOutputStream(byteOrder, 2, 0, 0);

        // How much data we got?
        int bytes=0;
        if (dictionary != null) {
            bytes += dictionary.length();
        }
        if (firstEvent != null) {
System.out.println("createRecord: add first event bytes " + firstEvent.length);
            bytes += firstEvent.length;
        }

        // If we have huge dictionary/first event ...
        if (bytes > record.getInternalBufferCapacity()) {
            record = new RecordOutputStream(byteOrder, 2, bytes, 0);
        }

        // Add dictionary to record
        if (dictionary != null) {
            try {record.addEvent(dictionary.getBytes("US-ASCII"));}
            catch (UnsupportedEncodingException e) {/* never happen */}
            // Also need to set bits in headers
            if (fileHeader != null)   fileHeader.hasDictionary(true);
            if (recordHeader != null) recordHeader.hasDictionary(true);
        }

        // Add first event to record
        if (firstEvent != null) {
System.out.println("createRecord: add first event to record");
            record.addEvent(firstEvent);
            if (fileHeader != null)   fileHeader.hasFirstEvent(true);
            if (recordHeader != null) recordHeader.hasFirstEvent(true);
        }

        // Make events into record. Pos = 0, limit = # valid bytes.
        record.build();

        // Ready-to-read buffer contains record data
        return record.getBinaryBuffer();
    }


    /**
     * Convenience method that sets compression type for the file.
     * The compression type is also set for internal record.
     * When writing to the file, record data will be compressed
     * according to the given type.
     * @param compression compression type
     * @return this object
     */
    public final Writer setCompressionType(int compression){
        outputRecord.getHeader().setCompressionType(compression);
        compressionType = outputRecord.getHeader().getCompressionType();
        return this;
    }

    /**
     * Convenience method that gets compression type for the file being written.
     * @return compression type for the file being written.
     */
    public int getCompressionType() {return outputRecord.getHeader().getCompressionType();}

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

        System.out.println("createHeader: IN, fe bit = " + fileHeader.hasFirstEvent());

        // Amount of user data in bytes
        int userHeaderBytes = 0;
        if (userHeader == null) {
            fileHeader.setUserHeaderLength(0);
        }
        else {
            userHeaderBytes = userHeader.length;
            fileHeader.setUserHeaderLength(userHeaderBytes);
            if (userHeaderBytes < 1) {
                userHeaderBytes = 0;
                fileHeader.setUserHeaderLength(0);
            }
        }

        System.out.println("createHeader: after set user header len, fe bit = " + fileHeader.hasFirstEvent());
        int totalLen = fileHeader.getLength();
        byte[] array = new byte[totalLen];
        ByteBuffer buffer = ByteBuffer.wrap(array);
        buffer.order(byteOrder);

        try {
            System.out.println("createHeader: will write file header into buffer: hasFE = " + fileHeader.hasFirstEvent());
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
        System.out.println("createHeader: IN, fe bit = " + fileHeader.hasFirstEvent());

        // Amount of user data in bytes
        int userHeaderBytes = 0;
        if (userHeader == null) {
            fileHeader.setUserHeaderLength(0);
        }
        else {
            userHeaderBytes = userHeader.remaining();
            fileHeader.setUserHeaderLength(userHeaderBytes);
            if (userHeaderBytes < 1) {
                userHeaderBytes = 0;
                fileHeader.setUserHeaderLength(0);
            }
        }

        System.out.println("createHeader: after set user header len, fe bit = " + fileHeader.hasFirstEvent());
        int totalLen = fileHeader.getLength();
        byte[] array = new byte[totalLen];
        ByteBuffer buffer = ByteBuffer.wrap(array);
        buffer.order(byteOrder);

        try {
            System.out.println("createHeader: will write file header into buffer: hasFE = " + fileHeader.hasFirstEvent());
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
     * @throws IOException if error writing to file.
     */
    public void writeTrailer(boolean writeIndex) throws IOException {

        // If we're NOT adding a record index, just write trailer
        if (!writeIndex) {
            try {
                RecordHeader.writeTrailer(headerArray, recordNumber, byteOrder, null);
                // TODO: not really necessary to keep track here?
                writerBytesWritten += RecordHeader.HEADER_SIZE_BYTES;
                if (toFile) {
                    outStream.write(headerArray, 0, RecordHeader.HEADER_SIZE_BYTES);
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

        // Create the index of record lengths & entries in proper byte order
        byte[] recordIndex = new byte[4*recordLengths.size()];
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

        try {
            // Place data into headerArray - both header and index
            RecordHeader.writeTrailer(headerArray, recordNumber,
                                      byteOrder, recordIndex);
            // TODO: not really necessary to keep track here?
            writerBytesWritten += dataBytes;
            if (toFile) {
                outStream.write(headerArray, 0, dataBytes);
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
     * @throws IOException if error writing to file.
     */
    public void writeRecord(RecordOutputStream record) throws IOException {
        RecordHeader header = record.getHeader();

        // Make sure given record is consistent with this writer
        header.setCompressionType(compressionType);
        header.setRecordNumber(recordNumber++);
        //System.out.println( " set compression type = " + compressionType);
        record.getHeader().setCompressionType(compressionType);
        record.setByteOrder(byteOrder);

        record.build();
        int bytesToWrite = header.getLength();
        // Record length of this record
        recordLengths.add(bytesToWrite);
        // Followed by events in record
        recordLengths.add(header.getEntries());
        writerBytesWritten += bytesToWrite;

        if (toFile) {
            outStream.write(record.getBinaryBuffer().array(), 0, bytesToWrite);
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
     * @throws IOException if cannot write to file.
     */
    public void addEvent(ByteBuffer buffer) throws IOException {
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

    /**
     * Write internal record with incremented record # to file or buffer.
     * @throws IOException if cannot write to file.
     */
    private void writeOutput() throws IOException {
        if (!toFile) {
            writeOutputToBuffer();
            return;
        }

        RecordHeader header = outputRecord.getHeader();
        header.setRecordNumber(recordNumber++);
        // --- Added on SEP 21 - gagik
        header.setCompressionType(compressionType);
        outputRecord.build();
        int bytesToWrite = header.getLength();
        //int wordsToWrite = bytesToWrite/4;
        //int remainder    = bytesToWrite%4;
        // Record length of this record
        recordLengths.add(bytesToWrite);
        // Followed by events in record
        recordLengths.add(header.getEntries());
        writerBytesWritten += bytesToWrite;
        //System.out.println(" bytes to write = " + bytesToWrite);
        if (outputRecord.getBinaryBuffer().hasArray()) {
            outStream.write(outputRecord.getBinaryBuffer().array(), 0, bytesToWrite);
        }
        else {
            // binary buffer is ready to read after build()
            fileChannel.write(outputRecord.getBinaryBuffer());
        }
        outputRecord.reset();
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
            // Create a buffer out of userHeader array or dictionary/first event
            // User header given as arg has precedent
            if (userHeader != null) {
                userHeaderBuffer = ByteBuffer.wrap(userHeader);
            }
            // If either dictionary or firstEvent is defined and user header not given ...
            else if (dictionary != null || firstEvent != null) {
                userHeaderBuffer = createDictionaryRecord();
            }

            // This will take care of any unpadded user header data
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

        if (addTrailer) {
            // Keep track of where we are right now which is just before trailer
            long trailerPosition = writerBytesWritten;

            // Write the trailer
            writeTrailer(addTrailerIndex);

            if (toFile) {
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
                    int bitInfo = fileHeader.hasTrailerWithIndex(true);
                    if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
                        outStream.writeInt(Integer.reverseBytes(bitInfo));
                    }
                    else {
                        outStream.writeInt(bitInfo);
                    }
                }
            }
        }

        if (toFile) {
            // Need to update the record count in file header
            outStream.seek(FileHeader.RECORD_COUNT_OFFSET);
            if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
                outStream.writeInt(Integer.reverseBytes(recordNumber - 1));
            }
            else {
                outStream.writeInt(recordNumber - 1);
            }

            outStream.close();
        }
        
        closed = true;
        //System.out.println("[writer] ---> bytes written " + writerBytesWritten);
    }
}
