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
 *  Class which reads data to create an Evio or HIPO Record.<p>
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
 *    |                                  |            |                  |    Pad 3      |
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
    private Compressor  compressor = Compressor.getInstance();
    
    /** Number of event in record. */
    private int  nEntries;

    /** Offset, in uncompressed dataBuffer, from just past header to user header
     *  (past index). */
    private int  userHeaderOffset;

    /** Offset, in uncompressed dataBuffer, from just past header to event data
     *  (past index + user header). */
    private int  eventsOffset;

    /** Byte order of internal ByteBuffers. */
    private ByteOrder byteOrder;


    /** Default, no-arg constructor. */
    public RecordInputStream(){
        byteOrder = ByteOrder.LITTLE_ENDIAN;
        allocate(DEFAULT_BUF_SIZE);
        header = new RecordHeader();
    }

    /**
     * Constructor with arguments.
     * @param order byte order of record byte arrays.
     */
    public RecordInputStream(ByteOrder order) {
        byteOrder = order;
        allocate(DEFAULT_BUF_SIZE);
        header = new RecordHeader();
    }

    /** Allocates all buffers for constructing the record stream. */
    private void allocate(int size){
        byte[] arrayData = new byte[size];
        dataBuffer = ByteBuffer.wrap(arrayData);
        dataBuffer.order(byteOrder);

        byte[] arrayRecord = new byte[size];
        recordBuffer = ByteBuffer.wrap(arrayRecord);
        recordBuffer.order(byteOrder);

        // byte[] arrayHeader = new byte[72];
        byte[] arrayHeader = new byte[RecordHeader.HEADER_SIZE_BYTES];
        headerBuffer = ByteBuffer.wrap(arrayHeader);
        headerBuffer.order(byteOrder);
    }

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
     *               set to last index.
     * @return byte array containing event.
     */
    public byte[] getEvent(int index){
        int firstPosition = 0;
        if (index > 0) {
            if (index >= header.getEntries()) {
                index = header.getEntries() - 1;
            }
            firstPosition = dataBuffer.getInt( (index-1)*4 );
        }
        int lastPosition = dataBuffer.getInt(index*4);

        int length = lastPosition - firstPosition;
        byte[] event = new byte[length];
        int   offset = eventsOffset + firstPosition;

        if (dataBuffer.hasArray()) {
            System.arraycopy(dataBuffer.array(), offset, event, 0, length);
        }
        else {
            dataBuffer.limit(length+offset).position(offset);
            dataBuffer.get(event, 0, length);
        }

        //System.out.println(" reading from " + offset + "  length = " + event.length);
        return event;
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
     *                       (buffer.capacity() - bufOffset < event size).
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
     * Any given byte buffer has to be large enough to contain user header,
     * but the buffer.limit() is ignored & reset.
     * If no buffer is given (arg is null), create a buffer internally and return it.
     * Buffer's byte order is set to that of the internal buffers.
     *
     * @param buffer    buffer to be filled with user header. If null create one and
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
// TODO: use capacity NOT array.length since backing array may be huge. Make changes elsewhere.
        else if ((bufOffset < 0) || (bufOffset + length > buffer.capacity())) {
            if (bufOffset < 0) {
                throw new HipoException("negative offset arg");
            }
            throw new HipoException("buffer with offset " + bufOffset +
                                    " is smaller than the user header.");
        }
        
        buffer.order(byteOrder);

        if (buffer.hasArray() && dataBuffer.hasArray()) {
// TODO: be sure to use arrayOffset() since buffer may represent only part of array
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
     * Parse the user header into the returned recordInputStream object.
     * The byte buffer has to be large enough to contain it.
     * Warning, buffer.limit() is ignored & reset.
     * @param buffer    buffer to be filled with user header
     * @param bufOffset offset into buffer to place user header.
     * @return record parsed from user header or null if no user header exists.
     * @throws HipoException if buffer has insufficient space to contain user header
     *                       (buffer.capacity() - bufOffset < user header size).
     *                       If buffer null or offset negative.
     */
    public RecordInputStream getUserHeaderAsRecord(ByteBuffer buffer, int bufOffset)
                    throws HipoException {

        // Read user header into given buffer, ready to read & with proper byte order
        if (getUserHeader(buffer, bufOffset) == null) {
            // If no user header ...
            return null;
        }

        // Parse user header into record
        RecordInputStream record = new RecordInputStream(byteOrder);
        record.readRecord(buffer, bufOffset);
        return record;
    }

    /**
     * Reads record from the file at given position.
     * Any compressed data is decompressed.
     * Memory is allocated as needed.
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
            // That is, everything except the header. Don't forget padding.
            int neededSpace =   header.getIndexLength() +
                              4*header.getUserHeaderLengthWords() +
                              4*header.getDataLengthWords();

            // Handle rare situation in which compressed data takes up more room
            neededSpace = neededSpace < cLength ? cLength : neededSpace;

            // Make room to handle all data to be read & uncompressed
            if (dataBuffer.capacity() < neededSpace) {
                allocate(neededSpace);
            }

            //System.out.println(header);
            //System.out.println(" READING FROM POSITION "
            //        + (position) + "  DATA SIZE = " + (recordLengthWords));
            //int padding = header.getCompressedDataLengthPadding();
            //System.out.println(" compressed size = " + cLength + "  padding = " + padding);

            // Go here to read rest of record
            channel.position(position + headerLength);

            // Decompress data
            switch (header.getCompressionType()) {
                case 1:
                case 2:
                    // LZ4
                    // Read compressed data
                    file.read(recordBuffer.array(), 0, cLength);
                    //file.read(recordBuffer.array(), 0, (recordLengthWords - headerLength));
                    compressor.uncompressLZ4(recordBuffer, cLength, dataBuffer);
                    break;

                case 3:
                    // GZIP
                    file.read(recordBuffer.array(), 0, cLength);
                    byte[] unzipped = compressor.uncompressGZIP(recordBuffer.array(), 0, cLength);
                    dataBuffer.put(unzipped);
                    break;

                case 0:
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
            eventsOffset     = userHeaderOffset + header.getUserHeaderLengthWords()*4;

            //showIndex();
            // Overwrite event lengths with event offsets
            int event_pos = 0;
            for(int i = 0; i < nEntries; i++){
                int   size = dataBuffer.getInt(i*4);
                event_pos += size;
                dataBuffer.putInt(i*4, event_pos);
            }
            //showIndex();

        } catch (IOException ex) {
            Logger.getLogger(RecordInputStream.class.getName()).log(Level.SEVERE, null, ex);
        } catch (HipoException ex) {
            Logger.getLogger(RecordInputStream.class.getName()).log(Level.SEVERE, null, ex);
        }
    }

    /**
     * Reads a record from the buffer at the given offset.
     * Any compressed data is decompressed.
     * Memory is allocated as needed.
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
            setByteOrder(headerBuffer.order());

            int recordLengthWords = header.getLength();
            int headerLength      = header.getHeaderLength();
            int cLength           = header.getCompressedDataLength();

            int compDataOffset = offset + headerLength;

            // How many bytes will the expanded record take?
            // That is, everything except the header. Don't forget padding.
            int neededSpace =   header.getIndexLength() +
                              4*header.getUserHeaderLengthWords() +
                              4*header.getDataLengthWords();

            // Handle rare situation in which compressed data takes up more room
            neededSpace = neededSpace < cLength ? cLength : neededSpace;

            // Make room to handle all data to be read & uncompressed
            if (dataBuffer.capacity() < neededSpace) {
                allocate(neededSpace);
            }

            // Decompress data
            switch (header.getCompressionType()) {
                case 1:
                case 2:
                    // Read LZ4 compressed data
                    if (buffer.hasArray() && recordBuffer.hasArray()) {
                        System.arraycopy(buffer.array(), buffer.arrayOffset() + compDataOffset,
                                         recordBuffer.array(), 0, cLength);
                    }
                    else {
                        buffer.limit(compDataOffset + cLength).position(compDataOffset);
                        recordBuffer.put(buffer);
                    }

                    compressor.uncompressLZ4(recordBuffer, cLength, dataBuffer);
                    break;

                case 3:
                    // Read GZIP compressed data
                    if (buffer.hasArray() && recordBuffer.hasArray()) {
                        System.arraycopy(buffer.array(), buffer.arrayOffset() + compDataOffset,
                                         recordBuffer.array(), 0, cLength);
                    }
                    else {
                        buffer.limit(compDataOffset + cLength).position(compDataOffset);
                        recordBuffer.put(buffer);
                    }

                    byte[] unzipped = compressor.uncompressGZIP(recordBuffer.array(), 0, cLength);
                    dataBuffer.put(unzipped);
                    break;

                case 0:
                default:
                    // Read uncompressed data - rest of record
                    int len = recordLengthWords - headerLength;
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

            // Overwrite event lengths with event offsets
            int event_pos = 0;
            for(int i = 0; i < nEntries; i++){
                int   size = dataBuffer.getInt(i*4);
                event_pos += size;
                dataBuffer.putInt(i*4, event_pos);
            }
        }
        catch (HipoException ex) {
            Logger.getLogger(RecordInputStream.class.getName()).log(Level.SEVERE, null, ex);
        }
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
