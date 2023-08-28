/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package org.jlab.coda.hipo;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.BufferOverflowException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 *
 *  Class which reads data to create an Evio or HIPO Record.
 *  This class is NOT thread safe!
 *
 * <pre><code>
 * RECORD STRUCTURE:
 *
 *               Uncompressed                                      Compressed
 *
 *    +----------------------------------+            +----------------------------------+
 *    |       General Record Header      |            |       General Record Header      |
 *    +----------------------------------+            +----------------------------------+
 *
 *    +----------------------------------+ ---------&gt; +----------------------------------+
 *    |           Index Array            |            |        Compressed Data           |
 *    +----------------------------------+            |             Record               |
 *                                                    |                                  |
 *    +----------------------------------+            |                                  |
 *    |           User Header            |            |                  ----------------|
 *    |           (Optional)             |            |                  |    Pad 3      |
 *    |                  ----------------|            +----------------------------------+
 *    |                  |    Pad 1      |           ^
 *    +----------------------------------+          /
 *                                                 /
 *    +----------------------------------+       /
 *    |           Data Record            |     /
 *    |                                  |    /
 *    |                  ----------------|   /
 *    |                  |    Pad 2      | /
 *    +----------------------------------+
 *
 *
 *
 *
 * GENERAL RECORD HEADER STRUCTURE ( see RecordHeader.java )
 *
 *    +----------------------------------+
 *  1 |         Record Length            | // 32bit words, inclusive
 *    +----------------------------------+
 *  2 +         Record Number            |
 *    +----------------------------------+
 *  3 +         Header Length            | // 14 (words)
 *    +----------------------------------+
 *  4 +       Event (Index) Count        |
 *    +----------------------------------+
 *  5 +      Index Array Length          | // bytes
 *    +-----------------------+---------+
 *  6 +       Bit Info        | Version  | // version (8 bits)
 *    +-----------------------+----------+
 *  7 +      User Header Length          | // bytes
 *    +----------------------------------+
 *  8 +          Magic Number            | // 0xc0da0100
 *    +----------------------------------+
 *  9 +     Uncompressed Data Length     | // bytes
 *    +------+---------------------------+
 * 10 +  CT  |  Data Length Compressed   | // CT = compression type (4 bits)
 *    +----------------------------------+
 * 11 +        General Register 1        | // UID 1st (64 bits)
 *    +--                              --+
 * 12 +                                  |
 *    +----------------------------------+
 * 13 +        General Register 2        | // UID 2nd (64 bits)
 *    +--                              --+
 * 14 +                                  |
 *    +----------------------------------+
 * </code></pre>
 *
 * @version 6.0
 * @since 6.0 10/13/17
 * @author gavalian
 */
public class RecordInputStream {

    /** Default internal buffer size in bytes. */
    private static final int DEFAULT_BUF_SIZE = 8*1024*1024;

    /** Number of event in record. */
    private int  nEntries;

    /** Offset, in uncompressed dataBuffer, from just past header to user header
     *  (past index) bytes. */
    private int  userHeaderOffset;

    /** Offset, in uncompressed dataBuffer, from just past header to event data
     *  (past index + user header) bytes. */
    private int  eventsOffset;

    /** Length in bytes of uncompressed data (events) in dataBuffer, not including
     * header, index or user header. */
    private int  uncompressedEventsLength;

    /** Byte order of internal ByteBuffers. */
    private ByteOrder byteOrder = ByteOrder.LITTLE_ENDIAN;

    /** General header of this record. */
    private RecordHeader  header;

    /**
     * This buffer contains uncompressed data consisting of, in order,
     *      1) index array, 2) user header, 3) events.
     *  It's important to know that the index array is rewritten in readRecord().
     *  Initially each word int the array contained the size of the next event.
     *  It was overwritten to berthe offset to the next event so the position of
     *  each event does not have to be calculated each time is data is accessed.
     *  This offset is from the beginning of event data (after record header,
     *  index array, and user header + padding). First offset = 0.
     *  The second offset = # of bytes to beginning of second event, etc.
     */
    private ByteBuffer  dataBuffer;

    /** This buffer contains compressed data. */
    private ByteBuffer  recordBuffer;

    /** Record's header is read into this buffer. */
    private ByteBuffer  headerBuffer;

    /** Singleton used to decompress data. */
    private static Compressor compressor = Compressor.getInstance();
    


    /** Default, no-arg constructor. */
    public RecordInputStream(){
        header = new RecordHeader();
        allocate(DEFAULT_BUF_SIZE);

        // Allocate buffer to read header into
        headerBuffer = ByteBuffer.wrap(new byte[RecordHeader.HEADER_SIZE_BYTES]);
        headerBuffer.order(byteOrder);
    }

    /**
     * Constructor with arguments.
     * @param order byte order of record byte arrays.
     */
    public RecordInputStream(ByteOrder order) {
        byteOrder = order;
        header = new RecordHeader();
        allocate(DEFAULT_BUF_SIZE);

        // Allocate buffer to read header into
        headerBuffer = ByteBuffer.wrap(new byte[RecordHeader.HEADER_SIZE_BYTES]);
        headerBuffer.order(byteOrder);
    }

    /** Allocates data & record buffers for constructing the record stream. */
    private void allocate(int size){
        dataBuffer = ByteBuffer.wrap(new byte[size]);
        dataBuffer.order(byteOrder);

        recordBuffer = ByteBuffer.wrap(new byte[size]);
        recordBuffer.order(byteOrder);
    }

    /**
     * Get the header of this record.
     * @return header of this record.
     */
    public RecordHeader getHeader() {return header;}

    /**
     * Get the byte order of the internal buffers.
     * @return byte order of the internal buffers.
     */
    public ByteOrder getByteOrder() {return byteOrder;}

    /**
     * Set the byte order of the internal buffers.
     * @param order desired byte order of internal buffers.
     */
    private void setByteOrder(ByteOrder order) {
        byteOrder = order;
        dataBuffer.order(order);
        recordBuffer.order(order);
        headerBuffer.order(order);
    }

    /**
     * Get the buffer with all uncompressed data in it.
     * It's position and limit are set to read only event data. That means no
     * header, index, or user-header.
     * @return  the buffer with uncompressed event data in it.
     */
    public ByteBuffer getUncompressedDataBuffer() {
        dataBuffer.limit(eventsOffset + uncompressedEventsLength).position(eventsOffset);
        return dataBuffer;
    }

    /**
     * Does this record contain an event index?
     * @return true if record contains an event index, else false.
     */
    public boolean hasIndex() {return (header.getIndexLength() > 0);}

    /**
     * Does this record contain a user header?
     * @return true if record contains a user header, else false.
     */
    public boolean hasUserHeader() {return (header.getUserHeaderLength() > 0);}

    /**
     * Get the event at the given index and return it in an allocated array.
     * Not threadsafe as internal buffer's pos & limit may change momentarily,
     * but are unchanged on return.
     * This method allocates memory and thus creates garbage.
     *
     * @param index  index of event starting at 0. If index too large, it's
     *               set to last index. If negative, it's set to 0.
     * @return byte array containing event.
     */
    public byte[] getEvent(int index) {
        int firstPosition = 0;

        if (index > 0) {
            if (index >= header.getEntries()) {
                index = header.getEntries() - 1;
            }
            // Remember, the index array of events lengths (at beginning of dataBuffer)
            // was overwritten in readRecord() to contain offsets to events.
            firstPosition = dataBuffer.getInt( (index-1)*4 );
        }
        else {
            index = 0;
        }

        int lastPosition = dataBuffer.getInt(index*4);
        int length = lastPosition - firstPosition;
        int offset = eventsOffset + firstPosition;

        // Allocating memory here!!!
        byte[] event = new byte[length];

        if (dataBuffer.hasArray()) {
            // NOTE: dataBuffer.arrayOffset() is always 0
            System.arraycopy(dataBuffer.array(), offset, event, 0, length);
        }
        else {
            // To be consistent, whether dataBuffer has a backing array or not,
            // the position & limit should remain unchanged. This is done, HOWEVER,
            // the method will not be threadsafe.
            int lim = dataBuffer.limit();
            int pos = dataBuffer.position();

            dataBuffer.limit(length+offset).position(offset);
            dataBuffer.get(event, 0, length);

            // Set things back to the way they were
            dataBuffer.limit(lim).position(pos);
        }

//System.out.println("RecordInputStream: getEvent: reading from " + offset + "  length = " + event.length);
        return event;
    }

    /**
     * Get the event at the given index and return it in the given array.
     * Not threadsafe as internal buffer's pos & limit may change momentarily,
     * but are unchanged on return.
     *
     * @param event byte array in which to place event.
     * @param off   offset in event to place event. Negative values set to 0.
     * @param index index of event starting at 0. If index too large, it's
     *              set to last index. If negative, it's set to 0.
     * @return number of data bytes written into event array.
     * @throws BufferOverflowException if event array too small.
     *
     */
    public int getEvent(byte[] event, int off, int index) throws BufferOverflowException {
        int firstPosition = 0;

        if (index > 0) {
            if (index >= header.getEntries()) {
                index = header.getEntries() - 1;
            }
            // Remember, the index array of events lengths (at beginning of dataBuffer)
            // was overwritten in readRecord() to contain offsets to events.
            firstPosition = dataBuffer.getInt( (index-1)*4 );
        }
        else {
            index = 0;
        }

        if (off < 0) off = 0;

        int lastPosition = dataBuffer.getInt(index*4);
        int length = lastPosition - firstPosition;
        int offset = eventsOffset + firstPosition;

        if (length + off > event.length) {
            // not enough mem to store event
            throw new BufferOverflowException();
        }

        if (dataBuffer.hasArray()) {
            // NOTE: dataBuffer.arrayOffset() is always 0
            System.arraycopy(dataBuffer.array(), offset, event, off, length);
        }
        else {
            // To be consistent, whether dataBuffer has a backing array or not,
            // the position & limit should remain unchanged. This is done, HOWEVER,
            // the method will not be threadsafe.
            int lim = dataBuffer.limit();
            int pos = dataBuffer.position();

            dataBuffer.limit(length+offset).position(offset);
            dataBuffer.get(event, off, length);

            // Set things back to the way they were
            dataBuffer.limit(lim).position(pos);
        }

        return length;
    }

    /**
     * Returns the length of the event with given index.
     * @param index index of the event
     * @return length of the data in bytes.
     */
    public int getEventLength(int index) {
        if (index < 0 || index >= getEntries()) return 0;

        int firstPosition = 0;
        if (index > 0) {
            if (index >= header.getEntries()) {
                index = header.getEntries() - 1;
            }
            // Remember, the index array of events lengths (at beginning of dataBuffer)
            // was overwritten in readRecord() to contain offsets to events.
            firstPosition = dataBuffer.getInt( (index-1)*4 );
        }

        int lastPosition = dataBuffer.getInt(index*4);
        return lastPosition - firstPosition;
    }
    
    /**
     * Get the event at the given index and write it into the given byte buffer.
     * The given byte buffer has to be large enough to receive all the event's data,
     * but the buffer.limit() is ignored and reset.
     * If no buffer is given (arg is null), create a buffer internally and return it.
     * Buffer's byte order is set to that of the internal buffers.
     * Buffer pos & lim are ready to read on return.
     * Not threadsafe as internal buffer's pos & limit may change momentarily,
     * but are unchanged on return.
     *
     * @param buffer buffer to be filled with event starting at position = 0
     * @param index  index of event starting at 0. Negative value = 0.
     * @return buffer or newly allocated buffer if arg is null.
     * @throws HipoException if buffer has insufficient space to contain event
     *                       (buffer.capacity() &lt; event size).
     */
    public ByteBuffer getEvent(ByteBuffer buffer, int index) throws HipoException {
        return getEvent(buffer, 0, index);
    }

    /**
     * Get the event at the given index and write it into the given byte buffer.
     * The given byte buffer has to be large enough to receive all the event's data,
     * but the buffer.limit() is ignored and reset.
     * If no buffer is given (arg is null), create a buffer internally and return it.
     * Buffer's byte order is set to that of the internal buffers.
     * Buffer pos & lim are ready to read on return.
     * Not threadsafe as internal buffer's pos & limit may change momentarily,
     * but are unchanged on return.
     *
     * @param buffer    buffer to be filled with event.
     * @param bufOffset offset into buffer to place event.
     * @param index     index of event starting at 0. Negative value = 0.
     * @return buffer or newly allocated buffer if arg is null.
     * @throws HipoException if buffer has insufficient space to contain event
     *                       (buffer.capacity() - bufOffset &lt; event size).
     */
    public ByteBuffer getEvent(ByteBuffer buffer, int bufOffset, int index) throws HipoException {
        int firstPosition = 0;
        if (index > 0) {
            if (index >= header.getEntries()) {
                throw new HipoException("index too large");
            }
            firstPosition = dataBuffer.getInt((index - 1) * 4);
        }
        int lastPosition = dataBuffer.getInt(index * 4);
        int length = lastPosition - firstPosition;
        int offset = eventsOffset + firstPosition;

        if (buffer == null) {
            buffer = ByteBuffer.wrap(new byte[length]);
            bufOffset = 0;
        }
        else if ((bufOffset < 0) || (bufOffset + length > buffer.capacity())) {
            if (bufOffset < 0) {
                throw new HipoException("negative offset arg");
            }
            throw new HipoException("buffer with offset " + bufOffset +
                                    " is smaller than the event.");
        }

        buffer.order(byteOrder);

        if (buffer.hasArray() && dataBuffer.hasArray()) {
            System.arraycopy(dataBuffer.array(), offset, buffer.array(),
                             buffer.arrayOffset() + bufOffset, length);
        }
        else {
            // To be consistent, whether arrays have backing arrays or not,
            // the position & limit should remain unchanged. This is done, HOWEVER,
            // the method will not be threadsafe.
            int lim = dataBuffer.limit();
            int pos = dataBuffer.position();

            // Read data starting at offset for length # of bytes
            dataBuffer.limit(offset + length).position(offset);
            // Reset limit to capacity
            buffer.clear();
            buffer.position(bufOffset);
            buffer.put(dataBuffer);

            // Set things back to the way they were
            dataBuffer.limit(lim).position(pos);
        }

        // Make buffer ready to read.
        // Always set limit first, else you can cause exception.
        buffer.limit(bufOffset + length).position(bufOffset);

        return buffer;
    }

    /**
     * Get the user header contained in this record.
     * @return the user header contained in this record, or null if none.
     */
    public byte[] getUserHeader() {
        int length = header.getUserHeaderLength();
        if (length < 1) {
            return null;
        }

        byte[] userHeader = new byte[length];

        if (dataBuffer.hasArray()) {
            System.arraycopy(dataBuffer.array(), userHeaderOffset, userHeader, 0, length);
        }
        else {
            dataBuffer.limit(length + userHeaderOffset).position(userHeaderOffset);
            dataBuffer.get(userHeader, 0, length);
        }
        return userHeader;
    }

    /**
     * Get any existing user header and write it into the given byte buffer.
     * The given byte buffer must be large enough to contain user header.
     * If no buffer is given (arg is null), create a buffer internally and return it.
     * Buffer's byte order is set to that of the internal buffers.
     * Buffer's position is set to bufOffset and limit is set to bufOffset +
     * userHeader size.
     *
     * @param buffer    buffer to be filled with user header. If null, create one and
     *                  return that.
     * @param bufOffset offset into buffer to place user header.
     * @return buffer (or newly allocated buffer if arg is null) if header exists
     *         and successfully copied into buffer, or
     *         null if no user header exists.
     * @throws HipoException if buffer has insufficient space to contain user header
     *                       (buffer.capacity() - bufOffset &lt; user header size).
     *                       If buffer is not null and offset negative.
     */
    public ByteBuffer getUserHeader(ByteBuffer buffer, int bufOffset) throws HipoException {

        int length = header.getUserHeaderLength();
        if (length < 1) {
            if (buffer != null) {
                buffer.limit(bufOffset + length).position(bufOffset);
            }
            return null;
        }

        if (buffer == null) {
            buffer = ByteBuffer.wrap(new byte[length]);
            bufOffset = 0;
        }
        else if ((bufOffset < 0) || (bufOffset + length > buffer.capacity())) {
            if (bufOffset < 0) {
                throw new HipoException("negative offset arg");
            }
            throw new HipoException("buffer with offset " + bufOffset +
                                    " is smaller than the user header.");
        }
        
        buffer.order(byteOrder);

        if (buffer.hasArray() && dataBuffer.hasArray()) {
            System.arraycopy(dataBuffer.array(), userHeaderOffset, buffer.array(),
                             buffer.arrayOffset() + bufOffset, length);
        }
        else {
            // Read data starting at offset for length # of bytes
            dataBuffer.limit(userHeaderOffset+length).position(userHeaderOffset);
            // Reset limit to capacity
            buffer.clear();
            buffer.position(bufOffset);
            buffer.put(dataBuffer);
        }

        // Make buffer ready to read.
        // Always set limit first, else you can cause exception.
        buffer.limit(bufOffset + length).position(bufOffset);

        return buffer;
    }

    /**
     * Get any existing user header and write it into the given byte buffer.
     * The byte buffer must be large enough to contain it.
     * Warning, buffer.limit() is ignored and reset.
     * If no buffer is given (arg is null), create a buffer internally and use it.
     * Parse the user header into the returned recordInputStream object.
     * @param buffer    buffer to be filled with user header
     * @param bufOffset offset into buffer to place user header.
     * @return record parsed from user header or null if no user header exists.
     * @throws HipoException if non-null buffer has insufficient space to contain user header
     *                       (buffer.capacity() - bufOffset &lt; user header size).
     *                       If offset negative.
     */
    public RecordInputStream getUserHeaderAsRecord(ByteBuffer buffer, int bufOffset)
                    throws HipoException {

        // Read user header into given buffer, ready to read & with proper byte order.
        // If buffer is null, use internally created buffer with 0 offset.
        ByteBuffer buf = getUserHeader(buffer, bufOffset);
        if (buf == null) {
            // If no user header ...
            return null;
        }

        // If we had to create buffer internally ...
        if (buffer == null) {
            bufOffset = 0;
        }

        // Parse user header into record
        RecordInputStream record = new RecordInputStream(byteOrder);
        record.readRecord(buf, bufOffset);
        return record;
    }

    /**
     * Reads record from the file at given position. Call this method or
     * {@link #readRecord(ByteBuffer, int)} before calling any other.
     * Any compressed data is decompressed. Memory is allocated as needed.
     * First the header is read, then the length of
     * the record is read from header, then
     * following bytes are read and decompressed.
     * Handles the case in which index array length = 0.
     *
     * @param file opened file descriptor
     * @param position position in the file
     * @throws HipoException if file not in hipo format or if bad argument(s)
     */
    public void readRecord(RandomAccessFile file, long position) throws HipoException {

        if (file == null || position < 0L) {
            throw new HipoException("bad argument(s)");
        }

        try {
            FileChannel channel = file.getChannel();

            // Read header
            channel.position(position);
            file.read(headerBuffer.array());
            // This will switch headerBuffer to proper byte order
            header.readHeader(headerBuffer);
            // Make sure all internal buffers have the same byte order
            setByteOrder(headerBuffer.order());

            int recordLengthBytes = header.getLength();
            int headerLength      = header.getHeaderLength();
            nEntries              = header.getEntries();     // # of events
            int indexLen          = header.getIndexLength(); // bytes
            int cLength           = header.getCompressedDataLength();
            int userHdrLen        = 4*header.getUserHeaderLengthWords(); // bytes + padding

            // Handle the case of len = 0 for index array in header:
            // The necessary memory to hold such an index = 4*nEntries bytes.
            // In this case it must be reconstructed here by scanning the record.
            boolean findEvLens = false;
            if (indexLen == 0) {
                findEvLens = true;
                indexLen = 4*nEntries;
            }
            else if (indexLen != 4*nEntries) {
                // Header info is goofed up
                throw new HipoException("Record header index array len " + indexLen +
                        " does not match 4*(event cnt) " + (4*nEntries));
            }

            // How many bytes will the expanded record take?
            // Just data:
            uncompressedEventsLength = 4*header.getDataLengthWords();
            // Everything except the header & don't forget padding:
            int neededSpace = indexLen + userHdrLen + uncompressedEventsLength;

            // Handle rare situation in which compressed data takes up more room.
            // This also determines size of recordBuffer in which compressed
            // data is first read.
            neededSpace = neededSpace < cLength ? cLength : neededSpace;

            // Make room to handle all data to be read & uncompressed
            if (dataBuffer.capacity() < neededSpace) {
                allocate(neededSpace);
            }
            dataBuffer.clear();
            if (findEvLens) {
                // Leave room in front for reconstructed index array
                dataBuffer.position(indexLen);
            }

            // Go here to read rest of record
            channel.position(position + headerLength);

            // Decompress data
            switch (header.getCompressionType()) {
                case RECORD_COMPRESSION_LZ4:
                case RECORD_COMPRESSION_LZ4_BEST:
                    // LZ4
                    // Read compressed data
                    file.read(recordBuffer.array(), 0, cLength);
                    compressor.uncompressLZ4(recordBuffer, cLength, dataBuffer);
                    break;

                case RECORD_COMPRESSION_GZIP:
                    // GZIP
                    file.read(recordBuffer.array(), 0, cLength);
                    byte[] unzipped = compressor.uncompressGZIP(recordBuffer.array(), 0, cLength);
                    dataBuffer.put(unzipped);
                    break;

                case RECORD_UNCOMPRESSED:
                default:
                    // Read uncompressed data - rest of record
                    int len = recordLengthBytes - headerLength;
                    if (findEvLens) {
                        // If we still have to find event lengths to reconstruct missing index array,
                        // leave room for it in dataBuffer.
                        file.read(dataBuffer.array(), indexLen, len);
                    }
                    else {
                        file.read(dataBuffer.array(), 0, len);
                    }
            }

            // Offset in dataBuffer past index array, to user header
            userHeaderOffset = indexLen;
            // Offset in dataBuffer just past index + user header, to events
            eventsOffset = indexLen + userHdrLen;

// TODO: How do we handle trailers???

            // Overwrite event lengths with event offsets.
            // First offset is to beginning of 2nd (starting at 1) event, etc.
            int event_pos = 0, read_pos = eventsOffset;

            for (int i = 0; i < nEntries; i++) {
                int size;
                if (findEvLens) {
                    // In the case there is no index array, evio MUST be the format!
                    // Reconstruct index - the first bank word = len - 1 words.
                    size = 4*(dataBuffer.getInt(read_pos) + 1);
                    read_pos += size;
                }
                else {
                    // ev size in index array
                    size = dataBuffer.getInt(i * 4);
                }
                event_pos += size;
                dataBuffer.putInt(i*4, event_pos);
            }

        }
        catch (IOException | HipoException ex) {
            Logger.getLogger(RecordInputStream.class.getName()).log(Level.SEVERE, null, ex);
        }
    }

    /**
     * Reads a record from the buffer at the given offset. Call this method or
     * {@link #readRecord(RandomAccessFile, long)} before calling any other.
     * Any compressed data is decompressed. Memory is allocated as needed.
     * Handles the case in which index array length = 0.
     *
     * @param buffer buffer containing record data.
     * @param offset offset into buffer to beginning of record data.
     * @throws HipoException if buffer not in hipo format or if bad argument(s)
     */
    public void readRecord(ByteBuffer buffer, int offset) throws HipoException {

        if (buffer == null || offset < 0) {
            throw new HipoException("bad argument(s)");
        }

        try {
            // This will switch buffer to proper byte order
            header.readHeader(buffer, offset);
//System.out.println("readRecord: header = \n" + header.toString());

            // Make sure all internal buffers have the same byte order
            setByteOrder(buffer.order());

            int recordLengthBytes = header.getLength();
            int headerLength      = header.getHeaderLength();
            nEntries              = header.getEntries();     // # of events
            int indexLen          = header.getIndexLength(); // bytes
            int cLength           = header.getCompressedDataLength();
            int userHdrLen        = 4*header.getUserHeaderLengthWords(); // bytes + padding

            int compDataOffset    = offset + headerLength;

            // Handle the case of len = 0 for index array in header:
            // The necessary memory to hold such an index = 4*nEntries bytes.
            // In this case it must be reconstructed here by scanning the record.
            boolean findEvLens = false;
            if (indexLen == 0) {
                findEvLens = true;
                indexLen = 4*nEntries;
            }
            else if (indexLen != 4*nEntries) {
                // Header info is goofed up
                throw new HipoException("Record header index array len " + indexLen +
                        " does not match 4*(event cnt) " + (4*nEntries));
            }

            // How many bytes will the expanded record take?
            // Just data:
            uncompressedEventsLength = 4*header.getDataLengthWords();
            // Everything except the header & don't forget padding:
            int neededSpace = indexLen + userHdrLen + uncompressedEventsLength;

            // Make room to handle all data to be read & uncompressed
            if (dataBuffer.capacity() < neededSpace) {
                allocate(neededSpace);
            }
            dataBuffer.clear();
            if (findEvLens) {
                // Leave room in front for reconstructed index array
                dataBuffer.position(indexLen);
            }

            // Decompress data
            switch (header.getCompressionType()) {
                case RECORD_COMPRESSION_LZ4:
                case RECORD_COMPRESSION_LZ4_BEST:
                    // Read LZ4 compressed data (WARNING: this does set limit on dataBuffer!)
                    compressor.uncompressLZ4(buffer, compDataOffset, cLength, dataBuffer);
                    break;

                case RECORD_COMPRESSION_GZIP:
                    // Read GZIP compressed data
                    buffer.limit(compDataOffset + cLength).position(compDataOffset);
                    byte[] unzipped = compressor.uncompressGZIP(buffer);
                    dataBuffer.put(unzipped);
                    break;

                case RECORD_UNCOMPRESSED:
                default:
// TODO: See if we can avoid this unnecessary copy!
                    // Read uncompressed data - rest of record
//System.out.println("readRecord: recordLengthBytes = " + recordLengthBytes + ", headerLength = " + headerLength);
                    int len = recordLengthBytes - headerLength;
                    if (buffer.hasArray() && dataBuffer.hasArray()) {
//System.out.println("readRecord: start reading at buffer pos = " + (buffer.arrayOffset() + compDataOffset) +
//        ", buffer limit = " + buffer.limit() + ", databuf limit = " + dataBuffer.limit() +
//        ", dataBuf pos = " + dataBuffer.position() + ", LEN ==== " + len);
                        if (findEvLens) {
                            // If we still have to find event lengths to reconstruct missing index array,
                            // leave room for it in dataBuffer.
                            System.arraycopy(buffer.array(), buffer.arrayOffset() + compDataOffset,
                                    dataBuffer.array(), indexLen, len);
                        }
                        else {
                            System.arraycopy(buffer.array(), buffer.arrayOffset() + compDataOffset,
                                    dataBuffer.array(), 0, len);
                        }
                    }
                    else {
                        buffer.limit(compDataOffset + len).position(compDataOffset);
                        dataBuffer.put(buffer);
                    }
            }

            // Offset in dataBuffer past index array, to user header
            userHeaderOffset = indexLen;
            // Offset in dataBuffer just past index + user header, to events
            eventsOffset = indexLen + userHdrLen;

// TODO: How do we handle trailers???

            // Overwrite event lengths with event offsets.
            // First offset is to beginning of 2nd (starting at 1) event, etc.
            int event_pos = 0, read_pos = eventsOffset;

            for (int i = 0; i < nEntries; i++) {
                int size;
                if (findEvLens) {
                    // In the case there is no index array, evio MUST be the format!
                    // Reconstruct index - the first bank word = len - 1 words.
                    size = 4*(dataBuffer.getInt(read_pos) + 1);
                    read_pos += size;
                }
                else {
                    // ev size in index array
                    size = dataBuffer.getInt(i * 4);
                }
                event_pos += size;
                dataBuffer.putInt(i*4, event_pos);
            }
        }
        catch (HipoException ex) {
            ex.printStackTrace();
            Logger.getLogger(RecordInputStream.class.getName()).log(Level.SEVERE, null, ex);
        }
    }

    /**
     * Uncompress the data of a record from the source buffer at the given offset
     * into the destination buffer.
     * Be aware that the position and limit of srcBuf may be changed.
     * The limit of dstBuf may be changed. The position of dstBuf will
     * be set to just after the user-header and just before the data.
     * 
     * @param srcBuf buffer containing record data.
     * @param srcOff offset into buffer to beginning of record data.
     * @param dstBuf buffer into which the record is uncompressed.
     * @param arrayBacked true if both srcBuf and dstBuf are backed by arrays.
     * @param header RecordHeader object to be used to read the record header in srcBuf.
     * @return the original record size in srcBuf (bytes).
     * @throws HipoException if srcBuf/dstBuf/header is null, contains too little data,
     *                       is not in proper format, or version earlier than 6.
     */
    static int  uncompressRecord(ByteBuffer srcBuf, int srcOff, ByteBuffer dstBuf,
                                boolean arrayBacked, RecordHeader header)
            throws HipoException {

        if (srcBuf == null || srcOff < 0 || dstBuf == null || header == null) {
            throw new HipoException("bad argument(s)");
        }

        int dstOff = dstBuf.position();

        // Read in header. This will switch srcBuf to proper byte order.
        header.readHeader(srcBuf, srcOff);
//System.out.println("\nuncompressRecord: header --> \n" + header);

        CompressionType compressionType = header.getCompressionType();
        int headerBytes              = header.getHeaderLength();
        int origRecordBytes          = header.getLength();
        int compressedDataLength     = header.getCompressedDataLength();
        int uncompressedRecordLength = header.getUncompressedRecordLength();

        int compressedDataOffset = srcOff + headerBytes;
        int indexLen = header.getIndexLength();
        int userLen  = 4*header.getUserHeaderLengthWords();  // padded

        // Make sure destination buffer has the same byte order
        //dstBuf.order(srcBuf.order());

        if (compressionType.isCompressed()) {
            // Copy (uncompressed) general record header to destination buffer
            if (arrayBacked) {
                System.arraycopy(srcBuf.array(), srcOff + srcBuf.arrayOffset(),
                                 dstBuf.array(), dstOff + dstBuf.arrayOffset(),
                                 headerBytes);
                dstBuf.position(dstOff + headerBytes);
            }
            else {
                srcBuf.limit(srcOff + headerBytes).position(srcOff);
                dstBuf.position(dstOff);
                dstBuf.put(srcBuf);
                // Get ready to read data
                srcBuf.limit(srcBuf.capacity());
            }
        }
        else {
            // Since everything is uncompressed, copy it all over as is
            int copyBytes = indexLen + userLen + 4*header.getDataLengthWords();  // padded
            if (arrayBacked) {
                System.arraycopy(srcBuf.array(), srcOff + srcBuf.arrayOffset(),
                                 dstBuf.array(), dstOff + dstBuf.arrayOffset(),
                                 headerBytes  + copyBytes);
                dstBuf.position(dstOff + headerBytes);
            }
            else {
                srcBuf.limit(compressedDataOffset + copyBytes).position(srcOff);
                dstBuf.position(dstOff);
                dstBuf.put(srcBuf);
                srcBuf.limit(srcBuf.capacity());
            }
        }

        // Decompress data
        switch (compressionType) {
            case RECORD_COMPRESSION_LZ4:
            case RECORD_COMPRESSION_LZ4_BEST:
                // Read LZ4 compressed data
                compressor.uncompressLZ4(srcBuf, compressedDataOffset,
                                         compressedDataLength, dstBuf);
                dstBuf.limit(dstBuf.capacity());
                break;

            case RECORD_COMPRESSION_GZIP:
                // Read GZIP compressed data
                srcBuf.limit(compressedDataOffset + compressedDataLength).position(compressedDataOffset);
                byte[] unzipped = compressor.uncompressGZIP(srcBuf);
                dstBuf.put(unzipped);
                break;

            case RECORD_UNCOMPRESSED:
            default:
                // Everything copied over above
        }

        srcBuf.limit(srcBuf.capacity());

        // Position dstBuf just before the data so it can be scanned for EvioNodes.
        // This takes user header padding into account.
        dstBuf.position(dstOff + headerBytes + indexLen + userLen);

        // Reset the compression type and length in header to 0
        dstBuf.putInt(dstOff + RecordHeader.COMPRESSION_TYPE_OFFSET, 0);
        header.setCompressionType(CompressionType.RECORD_UNCOMPRESSED).setCompressedDataLength(0);
//        // The previous call updated the bitinfo word in header. Write this into buf:
//        dstBuf.putInt(dstOff + RecordHeader.BIT_INFO_OFFSET, header.getBitInfoWord());

        // Reset the header length
        dstBuf.putInt(dstOff + RecordHeader.RECORD_LENGTH_OFFSET, uncompressedRecordLength/4);
        header.setLength(uncompressedRecordLength);

//            // If there is an index, change lengths to event offsets
//            if (header.getIndexLength() > 0) {
//                // Number of entries in index
//                int nEntries = header.getEntries();
//
//                // Overwrite event lengths with event offsets
//                int index, eventPos = 0;
//                int off = dstOff + headerBytes;
//
//                for (int i = 0; i < nEntries; i++) {
//                    index = off + 4*i;
//                    int size = dstBuf.getInt(index);
//                    eventPos += size;
//                    dstBuf.putInt(index, eventPos);
//                }
//            }

        return origRecordBytes;
    }

    /**
     * Returns number of the events packed in the record.
     * @return number of the events packed in the record.
     */
    public int getEntries(){return nEntries;}

    /**
     * prints on the screen the index array of the record
     */
    private void showIndex(){
        for(int i = 0; i < nEntries; i++){
            System.out.printf("%3d  ",dataBuffer.getInt(i*4));
        }
        System.out.println();
    }
    
    /**
     * test main program
     * @param args args.
     */
    public static void main(String[] args){
        
        try {
            RandomAccessFile outStreamRandom = new RandomAccessFile("example_file.evio","r");
            long position = (14+12)*4;
            RecordInputStream istream = new RecordInputStream();
            istream.readRecord(outStreamRandom, position);
            
            int nevents = istream.getEntries();
            
            for(int i = 0; i < nevents; i++){
                byte[] event = istream.getEvent(i);
                System.out.printf("%4d : size = %8d\n",i,event.length);
            }

        } catch (FileNotFoundException ex) {
            Logger.getLogger(RecordInputStream.class.getName()).log(Level.SEVERE, null, ex);
        }
        catch (HipoException ex) {
            Logger.getLogger(RecordInputStream.class.getName()).log(Level.SEVERE, null, ex);
        }
    }
}
