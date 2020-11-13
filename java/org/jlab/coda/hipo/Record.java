/*
 *   Copyright (c) 2016.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.hipo;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;

/**
 *  <pre>
 * RECORD HEADER STRUCTURE ( 48 bytes, 12 integers (32 bit) )

 * 
 * +----------------------------------+
 * |       ID word (0x43455248)       |
 * +----------------------------------+
 * +         Record Length            |
 * +----------------------------------+
 * +    Data Length Uncompressed      |
 * +------+---------------------------+
 * +  CT  | Data Length Compressed    | // CT = compression type (8 bits)
 * +------+---------------------------+
 * +  Number of events in the record  |
 * +----------------------------------+
 * +      Record Header length        |
 * +----------------------------------+
 * +  Index Description Length        |
 * +----------------------------------+
 * +           Reserved               |
 * +----------------------------------+
 * +        UID FIRST LOW             |
 * +----------------------------------+
 * +        UID FIRST HIGH            |
 * +----------------------------------+
 * +        UID SECOND LOW            |
 * +----------------------------------+
 * +        UID SECOND HIGH           |
 * +----------------------------------+
 *
 * </pre>
 *
 * <p></p><b>NOTE: THIS CLASS IS NOT USED IN EVIO!</b></p>
 *
 * HIPO Record Class that creates records of
 * data buffers (agnostic to what the data represents).
 * @author gavalian
 * @since 08/10/2017
 * @deprecated
 */
public class Record {
    /**
     * Constants providing Unique ID that is written at the beginning 
     * of each record for verification purposes (0x43455248).
     * The record header length is fixed (48 bytes) and is described 
     * in the beginning of this file.
     */
    private final Integer        recordUID = 0x43455248;
    private final Integer recordHeaderSize = 48;
    /**
     * Two Unique IDs written at the end of each record header.
     * the purpose of this is to keep track of the files in 
     * multi-threaded reconstruction environment where the order
     * of the records within a file can be changed. The 
     * meaning of the UIDs is user defined.
     */
    private Long   UniqueID_First = (long) 0;
    private Long  UniqueID_Second = (long) 0;
    /**
     * List containing all the events added to the record.
     */
    private final List<byte[]> recordEvents = new ArrayList<byte[]>();
    /**
     * The MAXIMUM_BYTES controls the maximum number of bytes
     * to be written to the record. The addEvent function will
     * return false if writing new buffer will exceed the maximum 
     * allowed size of the record. BytesWritten will increase
     * each time a buffer is added.
     */
    private Integer  bytesWritten = 0;
    private Integer MAXIMUM_BYTES = 8388608;
    /**
     * Compression type will determine how the data buffer will
     * be compressed. Available options are:
     * 0 - leave uncompressed, 1 - use GZIP compression, 
     * 2 - use LZ4 FAST compression, 3 - use LZ4 BEST compression.
     * GZIP leads to highest compression, but it's extremely slow.
     * LZ4 fast can be used where the speed is required.
     * LZ4 BEST can be used for better compression.
     * In general it is recommended to use one of the LZ4 compressions
     * since decompression speed for LZ4 is far superior to GZIP.
     */
    private CompressionType compressionType = CompressionType.RECORD_UNCOMPRESSED;
    
    /** Header for this record. */
    private RecordHeader recordHeader;

    
    public Record() {
       recordHeader = new RecordHeader();
    }

    /**
     * Sets compression type for the record. It will be used when
     * the build() method is called.
     * @param compression compression type as enum.
     * @return this object.
     */
    public Record setCompressionType(CompressionType compression) {
        this.compressionType = compression;
        return this;
    }
    
    /**
     * Sets compression type for the record. It will be used when
     * the build() method is called.
     * @param compression compression type as int.
     * @return this object.
     */
    public Record setCompressionType(int compression) {
        this.compressionType = CompressionType.getCompressionType(compression);
        return this;
    }

    /**
     * Gets the compression type for the record.
     * @return  compression type for the record.
     */
    public CompressionType getCompressionType(){
        return this.compressionType;
    }

    /**
     * Sets the maximum capacity of the record. No data
     * will be added if the this limit is reached.
     * @param max_capacity maximum capacity of the record.
     */
    public void setMaximumCapacity(int max_capacity){
        MAXIMUM_BYTES = max_capacity;
    }
    /**
     * add new byte[] array into the record. if the array
     * added to the record will exceed the maximum capacity
     * array will not be added and function returns false.
     * @param buffer array to add to the record
     * @return true is array is added, false otherwise
     */
    public boolean addEvent(byte[] buffer){
        if(buffer.length + bytesWritten > MAXIMUM_BYTES){
            return false;
        } else {
            recordEvents.add(buffer);
            bytesWritten += buffer.length;
        }
        return true;
    }
    
    /**
     * Returns a reference to the byte[] array
     * stored at position index.
     * @param index array position
     * @return  byte array
     */
    public byte[] getEventNoCopy(int index){
        return this.recordEvents.get(index);
    }
    /**
     * returns a copy of the event with index = index.
     * @param index order in the array
     * @return byte array
     */
    public byte[] getEvent(int index){
        int size = this.recordEvents.get(index).length;
        byte[] event = new byte[size];
        System.arraycopy(recordEvents.get(index), 0, event, 0, size);
        return event;
    }    
    /**
     * Returns byte[] array where all the buffers of the events
     * are written back to back. total size is equal to bytesWritten
     * @return buffer with all events.
     */
    private byte[] getDataBuffer(){
        byte[] dataBuffer = new byte[this.bytesWritten];
        int offset = 0;
        for(int entry = 0; entry < this.recordEvents.size(); entry++){
            byte[] item = recordEvents.get(entry);
            System.arraycopy(item, 0, dataBuffer, offset, item.length);
            offset+= item.length;
        }
        return dataBuffer;
    }
    
    public void build(ByteBuffer userHeader){
        int userHeaderSize = userHeader.array().length;
        //int indexArraySize = 
    }

    /**
     * Builds a byte[] array of the record. The record header, index
     * array and compressed data are written out. NOTE ! the buffer
     * is LITTLE_ENDIAN independent of the platform.
     * @return array representation of the record.
     */
    public ByteBuffer build() {
        
        int   record_size = this.recordHeaderSize + 4*this.recordEvents.size();
        int data_size_unc = this.bytesWritten;
        
        //byte[] buffer = new byte[data_size_unc+record_size];
        
        int indexOffset = this.recordHeaderSize;
        int  dataOffset = this.recordHeaderSize + 4*this.recordEvents.size();
        
        byte[] dataBuffer = getDataBuffer();
        byte[] compressedData = Compressor.getCompressedBuffer(compressionType, dataBuffer);
        //System.out.println(" DATA SIZE = " + dataBuffer.length + "  COMPRESSED SIZE = " + compressedData.length);
        byte[] recordBuffer = new byte[record_size+compressedData.length];
        
        ByteBuffer byteBuffer = ByteBuffer.wrap(recordBuffer);
        byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
        
        //System.arraycopy(compressedData, 0, recordBuffer, dataOffset, compressedData.length);
        System.arraycopy(compressedData, 0, recordBuffer, dataOffset, compressedData.length);
        
        byteBuffer.putInt(  0,  recordUID);
        byteBuffer.putInt(  4,  recordBuffer.length);
        byteBuffer.putInt(  8,  dataBuffer.length);
        byteBuffer.putInt( 12,  Record.encodeCompressionWord(compressionType.getValue(), compressedData.length));
        byteBuffer.putInt( 16,  recordEvents.size());
        byteBuffer.putInt( 20,  0);
        byteBuffer.putInt( 24,  4*recordEvents.size());
        
        /*Adding the Unique IDs into the record header*/
        byteBuffer.putLong(32, this.UniqueID_First);
        byteBuffer.putLong(40, this.UniqueID_Second);
        
        for(int entry = 0; entry < recordEvents.size(); entry++) 
            byteBuffer.putInt(indexOffset+entry*4, recordEvents.get(entry).length);
        
        return byteBuffer;
    }

    /**
     * Resets the content of the record. sets bytes writing counter to zero.
     */
    public void reset(){
        this.recordEvents.clear();
        this.bytesWritten = 0;
    }

    /**
     * returns number of arrays in the record.
     * @return number of arrays in the record.
     */
    public int getEntries(){
        return this.recordEvents.size();
    }
    
    public static int encodeCompressionWord(int compression, int length){
        int word = (compression << 24) & 0xFF000000;
        word = (word|length);
        return word;
    }
    
    public static int decodeCompressionType(int word){
        int type = (word&0xFF000000)>>24;
        return type;
    }
    
    public static int decodeCompressionLength(int word){
        int length = (word&0x00FFFFFF);
        return length;
    }
    
    public static Record initBinary(byte[] buffer){
        
        Record record = new Record();
        ByteBuffer byteBuffer = ByteBuffer.wrap(buffer);
        byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
        
        Integer   recordLength = byteBuffer.getInt(4);
        Integer recordUniqueID = byteBuffer.getInt(0);
        /*if(recordUniqueID!=record.recordUID){
            System.out.println("[record:error] ---> error initializing record. wrong UID");
            return null;
        }*/
        Integer   dataLength = byteBuffer.getInt(  8);
        Integer   uncompWord = byteBuffer.getInt( 12);
        Integer   eventCount = byteBuffer.getInt( 16);
        Integer  indexLength = byteBuffer.getInt( 24);
        
        Integer compressionLength = Record.decodeCompressionLength(uncompWord);
        CompressionType compression = CompressionType.getCompressionType(Record.decodeCompressionType(uncompWord));
        
        record.setCompressionType(compression);
        
        int  dataOffset = record.recordHeaderSize + indexLength;
        int indexOffset = record.recordHeaderSize;

        byte[] data = new byte[compressionLength];

        //System.out.println("data length ----> " + data.length + " " + dataLength +  "  " + indexLength + " "
        //        + uncompWord + "  " + compression + "  " + compressionLength + "  offset = " + dataOffset);
        System.arraycopy(buffer, dataOffset, data, 0, data.length);                
        
        byte[] dataUncompressed = Compressor.getUnCompressedBuffer(compression, data);
        
        
        
        int position = 0;
        for(int entry = 0; entry < eventCount; entry++){
            Integer eventLength = byteBuffer.getInt(indexOffset+4*entry);
            byte[]        event = new byte[eventLength];
            System.arraycopy(dataUncompressed, position, event, 0, eventLength);
            record.addEvent(event);
            position += eventLength;
        }
        return record;
    }
    
    public void show(){
        System.out.println("RECORD:: LENGTH = " + this.bytesWritten + "  EVENTS = " + this.recordEvents.size());
        for(int i = 0; i < this.recordEvents.size(); i++){
            System.out.println(String.format("Entry %8d : Length = %8d", i,recordEvents.get(i).length));
        }
    }
    
}
