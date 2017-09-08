/*
 *   Copyright (c) 2016.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.hipo;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Class which handles the creation and use of Evio & HIPO Records.<p>
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
 * @since 6.0 9/6/17
 * @author gavalian
 * @author timmer
 */
public class RecordOutputStream {
    
    /** Maximum number of events per record. */
    private static final int MAX_EVENT_COUNT = 1024*1024;

    /** Maximum size of some internal buffers in bytes. */
    private int MAX_BUFFER_SIZE = 8*1024*1024;

    /** This buffer stores event lengths ONLY. */
    private ByteBuffer recordIndex;
    
    /** This buffer stores event data ONLY. */
    private ByteBuffer recordEvents;
    
    /** This buffer stores data that will be compressed. */
    private ByteBuffer recordData;

    /** Buffer in which to put constructed (& compressed) binary record. */
    private ByteBuffer recordBinary;
    
    /** Compression information & type. */
    private Compressor dataCompressor;

    /** Header of this Record. */
    private RecordHeader header;

    /** Number of events written to this Record. */
    private int eventCount;

    /** Number of valid bytes in recordIndex buffer */
    private int indexSize;
    
    /** Number of valid bytes in recordEvents buffer. */
    private int eventSize;

    /** Byte order of record byte arrays to build. */
    private ByteOrder byteOrder;

    
    /** Default, no-arg constructor. */
    public RecordOutputStream(){
        allocate();
        dataCompressor = new Compressor();
        header = new RecordHeader();
        header.setCompressionType(Compressor.RECORD_COMPRESSION_LZ4);
        byteOrder = ByteOrder.LITTLE_ENDIAN;
    }

    /**
     * Constructor with byte order of built record byte arrays.
     * @param order byte order of built record byte arrays.
     */
    public RecordOutputStream(ByteOrder order){
        this();
        byteOrder = order;
    }

    /**
     * Get the general header of this record.
     * @return general header of this record.
     */
    public RecordHeader getHeader() {return header;}

    /**
     * Get the number of events written so far into the buffer
     * @return number of events written so far into the buffer
     */
    public int getEventCount() {return eventCount;}

    /**
     * Get the internal ByteBuffer used to construct binary representation of this record.
     * @return internal ByteBuffer used to construct binary representation of this record.
     */
    public ByteBuffer getBinaryBuffer() {return recordBinary;}

    /** Allocates all buffers for constructing the record stream. */
    private void allocate(){

        recordIndex = ByteBuffer.wrap(new byte[MAX_EVENT_COUNT*4]);
        recordIndex.order(byteOrder);

        recordEvents = ByteBuffer.wrap(new byte[MAX_BUFFER_SIZE]);
        recordEvents.order(byteOrder);

        recordData = ByteBuffer.wrap(new byte[MAX_BUFFER_SIZE]);
        recordData.order(byteOrder);

        // Trying to compress random data will expand it, so create a cushion
        recordBinary = ByteBuffer.wrap(new byte[MAX_BUFFER_SIZE + (1024*1024)]);
        recordBinary.order(byteOrder);
    }
    
    /**
     * Adds an event's byte[] array into the record.
     * <b>The byte order of event's byte array must
     * match the byte order given in constructor!</b>
     * 
     * @param event    event's byte array
     * @param position offset into event byte array from which to begin reading
     * @param length   number of bytes from byte array to add
     * @return true if event was added, false if the buffer is full or
     *         event count limit exceeded
     */
    public boolean addEvent(byte[] event, int position, int length){

        if( (eventCount + 1 > MAX_EVENT_COUNT) ||
            ((indexSize + eventSize + RecordHeader.HEADER_SIZE_BYTES + length) >= MAX_BUFFER_SIZE)) {
            //System.out.println(" the record is FULL..... INDEX SIZE = "
            //        + (indexSize/4) + " DATA SIZE = " + eventSize);
            return false;
        }

        //recordEvents.position(eventSize);
        // TODO: check length if len%4 = 0

        // Add event data (position of recordEvents buffer is incremented)
        recordEvents.put(event, position, length);
        eventSize += length;

        // Add 1 more index
        recordIndex.putInt(indexSize, length);
        indexSize += 4;

        eventCount++;

        return true;
    }

    /**
     * Adds an event's byte[] array into the record.
     * <b>The byte order of event's byte array must
     * match the byte order given in constructor!</b>
     * 
     * @param event event's byte array
     * @return true if event was added, false if the buffer is full.
     */
    public boolean addEvent(byte[] event){
        return addEvent(event,0, event.length);
    }

    /**
     * Reset internal buffers. The capacity of the ByteArray stream is set to 0.
     * and the first integer of index array is set to 0. The buffer is ready to 
     * receive new data.
     */
    public void reset(){
        indexSize  = 0;
        eventSize  = 0;
        eventCount = 0;

        recordData.clear();
        recordIndex.clear();
        recordEvents.clear();
        recordBinary.clear();

        header.reset();
    }

    /**
     * Builds the record. Compresses data, header is constructed,
     * then header & data written into internal buffer.
     */
    public void build(){

        // Write index & event arrays into a single, temporary buffer
        recordData.position(0);
        recordData.put(  recordIndex.array(), 0, indexSize);
        recordData.put( recordEvents.array(), 0, eventSize);

        // Since hipo/evio data is padded, all data to be written is already padded
        int dataBufferSize = indexSize + eventSize;

        // Compress that temporary buffer into destination buffer
        // (skipping over where record header will be written).
        int compressedSize = 0;
        try {
            compressedSize = dataCompressor.compressLZ4(
                                        recordData.array(), 0, dataBufferSize,
                                        recordBinary.array(), RecordHeader.HEADER_SIZE_BYTES,
                                        (recordBinary.array().length -
                                                RecordHeader.HEADER_SIZE_BYTES));
        }
        catch (HipoException e) {/* should not happen */}
        //System.out.println(" DATA SIZE = " + dataBufferSize + "  COMPRESSED SIZE = " + compressedSize);

        // Set header values
        header.setEntries(eventCount);
        header.setDataLength(eventSize);
        header.setIndexLength(indexSize);
        header.setCompressedDataLength(compressedSize);
        header.setLength(compressedSize + RecordHeader.HEADER_SIZE_BYTES); // record byte length

        // Go back and write header into destination buffer
        recordBinary.position(0);
        header.writeHeader(recordBinary);
    }

    /**
     * Builds the record. Compresses data, header is constructed,
     * then header & data written into internal buffer.
     * If user header is not padded to 4-byte boundary, it's done here.
     *
     * @param userHeader user's ByteBuffer which must be ready to read
     */
    public void build(ByteBuffer userHeader){

        // Arg check
        if (userHeader == null) {
            build();
            return;
        }

        //int userhSize = userHeader.array().length; // May contain unused bytes at end!
        // How much user-header data do we actually have?
        int userHeaderSize = userHeader.remaining();

//        System.out.println("  INDEX = 0 " + indexSize + "  " + (indexSize + userHeaderSize)
//                + "  DIFF " + userHeaderSize);

        // Into a single, temporary buffer, place the following:

        // 1) uncompressed index array
        recordData.position(0);
        // Note, put() will increment position
        recordData.put(recordIndex.array(), 0, indexSize);
        int dataBufferSize = indexSize;

        // 2) user header array
        recordData.put(userHeader.array(), 0, userHeaderSize);
        // Account for unpadded user header.
        // This will find the user header length in words & account for padding.
        header.setUserHeaderLength(userHeaderSize);
        // Hop over padded user header length
        dataBufferSize += 4*header.getUserHeaderLengthWords();
        recordData.position(dataBufferSize);

        // 3) data array (hipo/evio data is already padded)
        recordData.put(recordEvents.array(), 0, eventSize);
        dataBufferSize += eventSize;

        //System.out.println(" TOTAL SIZE = " + dataBufSize);

        // Compress that temporary buffer into destination buffer
        // (skipping over where record header will be written).
        int compressedSize = 0;
        try {
            compressedSize = dataCompressor.compressLZ4(
                                        recordData.array(), 0, dataBufferSize,
                                        recordBinary.array(), RecordHeader.HEADER_SIZE_BYTES,
                                        (recordBinary.array().length -
                                                RecordHeader.HEADER_SIZE_BYTES));
        }
        catch (HipoException e) {/* should not happen */}

        // Set header values (user header length already set above)
        header.setEntries(eventCount);
        header.setDataLength(eventSize);
        header.setIndexLength(indexSize);
        header.setCompressedDataLength(compressedSize);
        header.setLength(compressedSize + RecordHeader.HEADER_SIZE_BYTES);

        // Go back and write header into destination buffer
        recordBinary.position(0);
        header.writeHeader(recordBinary);
    }
}
