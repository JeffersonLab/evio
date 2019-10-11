/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package org.jlab.coda.hipo;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 *
 *  Class which reads data to create an Evio or HIPO Record.
 *  This class is NOT thread safe!<p>
 *
 * <pre>
 * RECORD STRUCTURE:
 *
 *               Uncompressed                                      Compressed
 *
 *    +----------------------------------+            +----------------------------------+
 *    |       General Record Header      |            |       General Record Header      |
 *    +----------------------------------+            +----------------------------------+
 *
 *    +----------------------------------+ ---------> +----------------------------------+
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
 * </pre>
 *
 * @version 6.0
 * @since 6.0 10/13/17
 * @author gavalian
 */
public class RecordInputStream {

    /** Default internal buffer size in bytes. */
    private static final int DEFAULT_BUF_SIZE = 8*1024*1024;

    /** General header of this record. */
    private RecordHeader  header;

    /** This buffer contains uncompressed data consisting of, in order,
     *  1) index array, 2) user header, 3) events. */
    private ByteBuffer  dataBuffer;

    /** This buffer contains compressed data. */
    private ByteBuffer  recordBuffer;

    /** Record's header is read into this buffer. */
    private ByteBuffer  headerBuffer;

    /** Singleton used to decompress data. */
    private static Compressor compressor = Compressor.getInstance();
    
    /** Number of event in record. */
    private int  nEntries;

    /** Offset, in uncompressed dataBuffer, from just past header to user header
     *  (past index). */
    private int  userHeaderOffset;

    /** Offset, in uncompressed dataBuffer, from just past header to event data
     *  (past index + user header). */
    private int  eventsOffset;

    /** Length in bytes of uncompressed data (events) in dataBuffer, not including
     * header, index or user header. */
    private int  uncompressedEventsLength;

    /** Byte order of internal ByteBuffers. */
    private ByteOrder byteOrder;


    /** Default, no-arg constructor. */
    public RecordInputStream(){
        byteOrder = ByteOrder.LITTLE_ENDIAN;
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
     * @param index  index of event starting at 0. If index too large, it's
     *               set to last index. If negative, it's set to 0.
     * @return byte array containing event.
     */
    public byte[] getEvent(int index){
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
// TODO: Allocating memory here!!!
        byte[] event = new byte[length];
        int   offset = eventsOffset + firstPosition;

        if (dataBuffer.hasArray()) {
            // NOTE: dataBuffer.arrayOffset() is always 0
            System.arraycopy(dataBuffer.array(), offset, event, 0, length);
        }
        else {
// TODO: Do changing limit & pos affect other things???
            dataBuffer.limit(length+offset).position(offset);
            dataBuffer.get(event, 0, length);
        }

//System.out.println("getEvent: reading from " + offset + "  length = " + event.length);
        return event;
    }

    /**
     * Returns the length of the event with given index.
     * @param index index of the event
     * @return length of the data in bytes.
     */
    public int getEventLength(int index){
        if(index<0||index>=getEntries()) return 0;
        int firstPosition = dataBuffer.getInt((index - 1) * 4);
        int lastPosition = dataBuffer.getInt(index * 4);
        int length = lastPosition - firstPosition;
        return length;
    }
    
    /**
     * Get the event at the given index and write it into the given byte buffer.
     * The given byte buffer has to be large enough to receive all the event's data,
     * but the buffer.limit() is ignored & reset.
     * If no buffer is given (arg is null), create a buffer internally and return it.
     * Buffer's byte order is set to that of the internal buffers.
     *
     * @param buffer buffer to be filled with event starting at position = 0
     * @param index  index of event starting at 0. Negative value = 0.
     * @return buffer or newly allocated buffer if arg is null.
     * @throws HipoException if buffer has insufficient space to contain event
     *                       (buffer.capacity() < event size).
     */
    public ByteBuffer getEvent(ByteBuffer buffer, int index) throws HipoException {
        return getEvent(buffer, 0, index);
    }

    /**
     * Get the event at the given index and write it into the given byte buffer.
     * The given byte buffer has to be large enough to receive all the event's data,
     * but the buffer.limit() is ignored & reset.
     * If no buffer is given (arg is null), create a buffer internally and return it.
     * Buffer's byte order is set to that of the internal buffers.
     *
     * @param buffer    buffer to be filled with event.
     * @param bufOffset offset into buffer to place event.
     * @param index     index of event starting at 0. Negative value = 0.
     * @return buffer or newly allocated buffer if arg is null.
     * @throws HipoException if buffer has insufficient space to contain event
     *                       (buffer.capacity() - bufOffset < event size).
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
            // Read data starting at offset for length # of bytes
            dataBuffer.limit(offset + length).position(offset);
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
     * Note that the buffer.limit() is ignored & reset.
     * If no buffer is given (arg is null), create a buffer internally and return it.
     * Buffer's byte order is set to that of the internal buffers.
     *
     * @param buffer    buffer to be filled with user header. If null, create one and
     *                  return that.
     * @param bufOffset offset into buffer to place user header.
     * @return buffer (or newly allocated buffer if arg is null) if header exists
     *         and successfully copied into buffer, or
     *         null if no user header exists.
     * @throws HipoException if buffer has insufficient space to contain user header
     *                       (buffer.capacity() - bufOffset < user header size).
     *                       If buffer is not null and offset negative.
     */
    public ByteBuffer getUserHeader(ByteBuffer buffer, int bufOffset) throws HipoException {

        int length = header.getUserHeaderLength();
        if (length < 1) {
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
     * Warning, buffer.limit() is ignored & reset.
     * If no buffer is given (arg is null), create a buffer internally and use it.
     * Parse the user header into the returned recordInputStream object.
     * @param buffer    buffer to be filled with user header
     * @param bufOffset offset into buffer to place user header.
     * @return record parsed from user header or null if no user header exists.
     * @throws HipoException if non-null buffer has insufficient space to contain user header
     *                       (buffer.capacity() - bufOffset < user header size).
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

            int recordLengthWords = header.getLength();
            int headerLength      = header.getHeaderLength();
            int cLength           = header.getCompressedDataLength();

            // How many bytes will the expanded record take?
            // Just data:
            uncompressedEventsLength = 4*header.getDataLengthWords();
            // Everything except the header & don't forget padding:
            int neededSpace =   header.getIndexLength() +
                              4*header.getUserHeaderLengthWords() +
                                uncompressedEventsLength;

            // Handle rare situation in which compressed data takes up more room
            neededSpace = neededSpace < cLength ? cLength : neededSpace;

            // Make room to handle all data to be read & uncompressed
            if (dataBuffer.capacity() < neededSpace) {
                allocate(neededSpace);
            }
            dataBuffer.clear();

            // Go here to read rest of record
            channel.position(position + headerLength);

            // Decompress data
            switch (header.getCompressionType()) {
                case RECORD_COMPRESSION_LZ4:
                case RECORD_COMPRESSION_LZ4_BEST:
                    // LZ4
                    // Read compressed data
                    file.read(recordBuffer.array(), 0, cLength);
                    //file.read(recordBuffer.array(), 0, (recordLengthWords - headerLength));
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
                    // None
                    // Read uncompressed data - rest of record
                    file.read(dataBuffer.array(), 0, (recordLengthWords - headerLength));
            }

            // Number of entries in index
            nEntries = header.getEntries();
            // Offset from just past header to user header (past index)
            userHeaderOffset = nEntries*4;
            // Offset from just past header to data (past index + user header)
            eventsOffset = userHeaderOffset + header.getUserHeaderLengthWords()*4;

            // Overwrite event lengths with event offsets
            int event_pos = 0;
            for(int i = 0; i < nEntries; i++){
                int   size = dataBuffer.getInt(i*4);
                event_pos += size;
                dataBuffer.putInt(i*4, event_pos);
            }

        } catch (IOException ex) {
            Logger.getLogger(RecordInputStream.class.getName()).log(Level.SEVERE, null, ex);
        } catch (HipoException ex) {
            Logger.getLogger(RecordInputStream.class.getName()).log(Level.SEVERE, null, ex);
        }
    }

    /**
     * Reads a record from the buffer at the given offset. Call this method or
     * {@link #readRecord(RandomAccessFile, long)} before calling any other.
     * Any compressed data is decompressed. Memory is allocated as needed.
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

            // Make sure all internal buffers have the same byte order
            setByteOrder(buffer.order());

            int recordLengthBytes = header.getLength();
            int headerLength      = header.getHeaderLength();
            int cLength           = header.getCompressedDataLength();

            int compDataOffset = offset + headerLength;

            // How many bytes will the expanded record take?
            // Just data:
            uncompressedEventsLength = 4*header.getDataLengthWords();
            // Everything except the header & don't forget padding:
            int neededSpace =   header.getIndexLength() +
                              4*header.getUserHeaderLengthWords() +
                                uncompressedEventsLength;

            // Make room to handle all data to be read & uncompressed
            dataBuffer.clear();
            if (dataBuffer.capacity() < neededSpace) {
                allocate(neededSpace);
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
                    int len = recordLengthBytes - headerLength;
                    if (buffer.hasArray() && dataBuffer.hasArray()) {
                        System.arraycopy(buffer.array(), buffer.arrayOffset() + compDataOffset,
                                         dataBuffer.array(), 0, len);
                    }
                    else {
                        buffer.limit(compDataOffset + len).position(compDataOffset);
                        dataBuffer.put(buffer);
                    }
            }

            // Number of entries in index
            nEntries = header.getEntries();
            // Offset from just past header to user header (past index)
            userHeaderOffset = nEntries*4;
            // Offset from just past header to data (past index + user header)
            eventsOffset = userHeaderOffset + header.getUserHeaderLengthWords()*4;

// TODO: How do we handle trailers???
            // Overwrite event lengths with event offsets
            int event_pos = 0;
            for(int i = 0; i < nEntries; i++){
                int   size = dataBuffer.getInt(i*4);
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
     * Be aware that the position & limit of srcBuf may be changed.
     * The limit of dstBuf may be changed. The position of dstBuf will
     * be set to just after the user-header and just before the data.
     * 
     * @param srcBuf buffer containing record data.
     * @param srcOff offset into buffer to beginning of record data.
     * @param dstBuf buffer into which the record is uncompressed.
     * @param arrayBacked true if both srcBuf and dstBuf are backed by arrays.
     * @param header RecordHeader object to be used to read the record header in srcBuf.
     * @return the original record size in srcBuf (bytes).
     * @throws HipoException if srcBuf is null, contains too little data,
     *                       is not in proper format, or version earlier than 6.
     */
    static int  uncompressRecord(ByteBuffer srcBuf, int srcOff, ByteBuffer dstBuf,
                                boolean arrayBacked, RecordHeader header)
            throws HipoException {

//        if (srcBuf == null || srcOff < 0 || destBuf == null || destOff < 0 || header == null) {
//            throw new HipoException("bad argument(s)");
//        }

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

        // Reset the header length
        dstBuf.putInt(dstOff + RecordHeader.RECORD_LENGTH_OFFSET, uncompressedRecordLength);
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
     * returns number of the events packed in the record.
     * @return 
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
     * @param args 
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
