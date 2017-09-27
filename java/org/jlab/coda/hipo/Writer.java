/*
 *   Copyright (c) 2016.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */

package org.jlab.coda.hipo;

import org.jlab.coda.jevio.*;

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

public class Writer implements AutoCloseable {

    /** Do we write to a file or a buffer? */
    private boolean toFile = true;

    // If writing to file ...

    /** Object for writing file. */
    private RandomAccessFile  outStream;
    /** The file channel, used for writing a file, derived from outStream. */
    private FileChannel  fileChannel;
    /** Header to write to file. */
    private RecordHeader  fileHeader;

    // If writing to buffer ...
    
    /** The buffer being written to. */
    private ByteBuffer buffer;

    // For both files & buffers

    /** Byte order of data to write to file/buffer. */
    private ByteOrder byteOrder = ByteOrder.LITTLE_ENDIAN;
    /** Internal Record. */
    private RecordOutputStream  outputRecord;
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
     * Default constructor.
     * <b>No</b> file is opened. Any file will have little endian byte order.
     */
    public Writer(){
        outputRecord = new RecordOutputStream();
        fileHeader   = new RecordHeader(HeaderType.EVIO_FILE);
    }

    /**
     * Constructor with byte order.
     * <b>No</b> file is opened.
     * Any dictionary will be placed in the user header which will create a conflict if
     * user tries to call {@link #open(String, byte[])} with another user header array.
     *
     * @param order byte order of written file
     * @param maxEventCount max number of events a record can hold.
     *                      Value of O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes a record can hold.
     *                      Value of < 8MB results in default of 8MB.
     */
    public Writer(ByteOrder order, int maxEventCount, int maxBufferSize){
        if (order != null) {
            byteOrder = order;
        }
        outputRecord = new RecordOutputStream(order, maxEventCount, maxBufferSize, 1);
        fileHeader   = new RecordHeader(HeaderType.EVIO_FILE);
    }

    /**
     * Constructor with filename.
     * The output file will be created with no user header.
     * File byte order is little endian.
     * @param filename output file name
     */
    public Writer(String filename){
        this();
        open(filename);
    }

    /**
     * Constructor with filename & byte order.
     * The output file will be created with no user header. LZ4 compression.
     * @param filename      output file name
     * @param order         byte order of written file or null for default (little endian)
     * @param maxEventCount max number of events a record can hold.
     *                      Value of O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes a record can hold.
     *                      Value of < 8MB results in default of 8MB.
     */
    public Writer(String filename, ByteOrder order, int maxEventCount, int maxBufferSize){
        this(order, maxEventCount, maxBufferSize);
        open(filename);
    }

    /**
     * Constructor for writing to a ByteBuffer. Byte order is taken from the buffer.
     * LZ4 compression.
     * @param buf buffer in to which to write events and/or records.
     * @param maxEventCount max number of events a record can hold.
     *                      Value of O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes a record can hold.
     *                      Value of < 8MB results in default of 8MB.
     */
    public Writer(ByteBuffer buf, int maxEventCount, int maxBufferSize) {
        buffer = buf;
        byteOrder = buf.order();
        outputRecord = new RecordOutputStream(byteOrder, maxEventCount, maxBufferSize, 1);
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
    public RecordHeader getFileHeader() {return fileHeader;}

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
            Logger.getLogger(Writer.class.getName()).log(Level.SEVERE, null, ex);
        } catch (IOException ex) {
            Logger.getLogger(Writer.class.getName()).log(Level.SEVERE, null, ex);
        }

        writerBytesWritten = (long) (fileHeader.getLength());
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

        fileHeader.writeFileHeader(buffer, 0);
        System.arraycopy(userHeader, 0, array,
                         RecordHeader.HEADER_SIZE_BYTES, userHeaderBytes);

        return buffer;
    }

    /**
     * Write a general header as the last "header" or trailer in the file
     * optionally followed by an index of all record lengths.
     * @param writeIndex if true, write an index of all record lengths in trailer.
     */
    public void writeTrailer(boolean writeIndex){

        // If we're NOT adding a record index, just write trailer
        if (!writeIndex) {
            RecordHeader.writeTrailer(headerArray, recordNumber, byteOrder, null);
            try {
                // TODO: not really necessary to keep track here?
                writerBytesWritten += RecordHeader.HEADER_SIZE_BYTES;
                outStream.write(headerArray, 0, RecordHeader.HEADER_SIZE_BYTES);
            } catch (IOException ex) {
                Logger.getLogger(Writer.class.getName()).log(Level.SEVERE, null, ex);
            }
            return;
        }

        // Create the index of record lengths in proper byte order
        byte[] recordIndex = new byte[4* recordLengths.size()];
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
        RecordHeader.writeTrailer(headerArray, recordNumber,
                                  byteOrder, recordIndex);
        try {
            // TODO: not really necessary to keep track here?
            writerBytesWritten += dataBytes;
            outStream.write(headerArray, 0, dataBytes);
        } catch (IOException ex) {
            Logger.getLogger(Writer.class.getName()).log(Level.SEVERE, null, ex);
        }

        return;
    }

    /**
     * Appends the record to the file.
     * Using this method in conjunction with addEvent() is not thread-safe.
     * @param record record object
     */
    public void writeRecord(RecordOutputStream record) {
        RecordHeader header = record.getHeader();

        // Make sure given record is consistent with this writer
        header.setCompressionType(compressionType);
        header.setRecordNumber(recordNumber++);
        //System.out.println( " set compresstion type = " + compressionType);
        record.getHeader().setCompressionType(compressionType);
        record.setByteOrder(byteOrder);

        record.build();
        int bytesToWrite = header.getLength();
        // Record length of this record
        recordLengths.add(bytesToWrite);
        writerBytesWritten += bytesToWrite;

        try {
            outStream.write(record.getBinaryBuffer().array(), 0, bytesToWrite);
        } catch (IOException ex) {
            Logger.getLogger(Writer.class.getName()).log(Level.SEVERE, null, ex);
        }
    }

    // Use internal outputRecordStream to write individual events

    /**
     * Add a byte array to the internal record. If the length of
     * the buffer exceeds the maximum size of the record, the record
     * will be written to the file (compressed if the flag is set).
     * Internal record will be reset to receive new buffers.
     * Using this method in conjunction with writeRecord() is not thread-safe.
     *
     * @param buffer array to add to the file.
     * @param offset offset into array from which to start writing data.
     * @param length number of bytes to write from array.
     */
    public void addEvent(byte[] buffer, int offset, int length){
        boolean status = outputRecord.addEvent(buffer, offset, length);
        if(!status){
            writeOutput();
            outputRecord.addEvent(buffer, offset, length);
        }
    }

    /**
     * Add a byte array to the internal record. If the length of
     * the buffer exceeds the maximum size of the record, the record
     * will be written to the file (compressed if the flag is set).
     * Internal record will be reset to receive new buffers.
     * Using this method in conjunction with writeRecord() is not thread-safe.
     *
     * @param buffer array to add to the file.
     */
    public void addEvent(byte[] buffer){
        addEvent(buffer,0,buffer.length);
    }

    /** Write internal record with incremented record # to file. */
    private void writeOutput(){
        RecordHeader header = outputRecord.getHeader();
        header.setRecordNumber(recordNumber++);
        // --- Added on SEP 21 - gagik
        header.setCompressionType(compressionType);
        outputRecord.build();
        int bytesToWrite = header.getLength();
        // Record length of this record
        recordLengths.add(bytesToWrite);
        writerBytesWritten += bytesToWrite;
        //System.out.println(" bytes to write = " + bytesToWrite);
        try {
            if (outputRecord.getBinaryBuffer().hasArray()) {
                outStream.write(outputRecord.getBinaryBuffer().array(), 0, bytesToWrite);
            }
            else {
                // binary buffer is ready to read after build()
                fileChannel.write(outputRecord.getBinaryBuffer());
            }
            outputRecord.reset();
        } catch (IOException ex) {
            Logger.getLogger(Writer.class.getName()).log(Level.SEVERE, null, ex);
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
        addTrailer = false;
    }

    /**
     * Close opened file. If the output record contains events,
     * they will be flushed to file. Trailer and its optional index
     * written if requested.
     */
    public void close(){
        if (outputRecord.getEventCount() > 0) {
            writeOutput();
        }

        try {
            if (addTrailer) {
                // Keep track of where we are right now which is just before trailer
                long trailerPosition = writerBytesWritten;

                // Write the trailer
                writeTrailer(addTrailerIndex);

                // Find & update file header's trailer position word
                outStream.seek(RecordHeader.TRAILER_POSITION_OFFSET);
                if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
                    outStream.writeLong(Long.reverseBytes(trailerPosition));
                }
                else {
                    outStream.writeLong(trailerPosition);
                }

                // Find & update file header's bit-info word
                if (addTrailerIndex) {
                    outStream.seek(RecordHeader.BIT_INFO_OFFSET);
                    int bitInfo = fileHeader.setBitInfoForFile(false,
                                                               false,
                                                               true);
                    if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
                        outStream.writeInt(Integer.reverseBytes(bitInfo));
                    }
                    else {
                        outStream.writeInt(bitInfo);
                    }
                }
            }

            outStream.close();
            //System.out.println("[writer] ---> bytes written " + writerBytesWritten);
        } catch (IOException ex) {
            Logger.getLogger(Writer.class.getName()).log(Level.SEVERE, null, ex);
        }
    }
}
