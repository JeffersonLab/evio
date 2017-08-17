/*
 *   Copyright (c) 2016.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.hipo;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 *
 * @author gavalian
 */
public class RecordStream {
    
    
    private int      MAX_BUFFER_SIZE = 8*1024*1024;
    private int      MAX_EVENT_COUNT = 1024*1024;
    private final int    HEADER_SIZE = 16*4;
    
    private final int RECORD_UID_WORD_LE = 0x43455248;
    private final int RECORD_UID_WORD_BE = 0x48524543;
    
    ByteArrayOutputStream  recordStream = null;
    ByteBuffer              recordIndex = null;
    
    
    ByteBuffer      recordEvents         = null;
    ByteBuffer      recordData           = null;
    ByteBuffer      recordDataCompressed = null;
    ByteBuffer      recordBinary         = null;
    
    /**
     * Compression information. Compression types.
     */
    Compressor       dataCompressor = null;
    private int     compressionType = Compressor.RECORD_COMPRESSION_LZ4;
    
    /**
     * BLOCK INFORMATION to be written to the header
     * These words are part of the EVIO header format, and
     * are written into header exactly as it was in EVIO.
     */
    private int  blockNumber = 0;
    private int blockVersion = 6;
    private int blockBitInfo = 0;
    private int reservedWord = 0;
    private int reservedWordSecond = 0;
    /**
     *  UNIQUE identifiers part of the new HIPO Header. There are
     *  Two long words reserved to be used for tagging event records
     *  for fast search through the file.
     */
    private long recordHeaderUniqueWordFirst  = 0L;
    private long recordHeaderUniqueWordSecond = 0L;
    
    /**
     * Default constructor.
     */
    public RecordStream(){
        /*recordStream = new ByteArrayOutputStream(MAX_BUFFER_SIZE);
        byte[] index = new byte[MAX_EVENT_COUNT*4];
        recordIndex  = ByteBuffer.wrap(index);
        recordIndex.order(ByteOrder.LITTLE_ENDIAN);
        recordIndex.putInt(0, 0);*/
        allocate(this.MAX_BUFFER_SIZE);
        dataCompressor = new Compressor();
    }
    /**
     * creates a record stream object with desired maximum size.
     * @param size 
     */
    public RecordStream(int size){
        MAX_BUFFER_SIZE = size;
        recordStream = new ByteArrayOutputStream(MAX_BUFFER_SIZE);
        byte[] index = new byte[MAX_EVENT_COUNT*4];
        recordIndex  = ByteBuffer.wrap(index);
        recordIndex.order(ByteOrder.LITTLE_ENDIAN);
        recordIndex.putInt(0, 0);
    }
    /**
     * sets unique words for the record header, there are two LONG
     * words at the end of each record.
     * @param uw1 first unique word (LONG)
     * @param uw2 second unique word (LONG)
     */
    public void setUniqueWords(long uw1, long uw2){
       recordHeaderUniqueWordFirst  = uw1;
       recordHeaderUniqueWordSecond = uw2;
    }
    /**
     * Sets compression type. Available compressions are:
     * 0 - uncompressed
     * 1 - LZ4 fast compression
     * 2 - LZ4 best compression
     * 3 - GZIP compression
     * @param type compression type (0-4)
     */
    public void setCompressionType(int type){        
        compressionType = type;
        if(compressionType<0||compressionType>3){
            System.out.println("[WARNING !] unknown compression type "
            + type + ". using uncompressed buffers.");
            compressionType = 0;
        }
    }
    /**
     * Set the version word for the record. Default is 6
     * @param version version number
     */
    public void setVersion(int version){
        blockVersion = version;
    }
    /**
     * sets the bit info for the record, this will be written into 
     * the high 24 bits of the word #6 (starting count from #1), the
     * lower 8 bits are the version.
     * @param bitinfo bit information for the record
     */
    public void setBitInfo(int bitinfo){
        blockBitInfo = bitinfo;
    }
    /**
     * set block number, for checking the order of the blocks
     * that are coming in from DAQ.
     * @param blkn block number
     */
    public void setBlockNumber(int blkn){
        blockNumber = blkn;
    }
    /**
     * Sets the reserved word for the block header. It is written
     * to word #7 in the record header (counting from #1). 
     * @param rw reserved word (32 bits)
     */
    public void setReservedWord(int rw){
        this.reservedWord = rw;
    }
    /**
     * Allocates all buffers for constructing the record stream.
     * @param size 
     */
    private void allocate(int size){
        MAX_BUFFER_SIZE = size;
        
        byte[] ri = new byte[MAX_EVENT_COUNT*4];
        recordIndex = ByteBuffer.wrap(ri);
        recordIndex.order(ByteOrder.LITTLE_ENDIAN);
        recordIndex.putInt(0, 4);
        
        byte[] re = new byte[size];
        recordEvents = ByteBuffer.wrap(re);
        recordEvents.order(ByteOrder.LITTLE_ENDIAN);
        recordEvents.putInt(0, 4);
        
        byte[] rd = new byte[size];
        recordData = ByteBuffer.wrap(rd);
        recordData.order(ByteOrder.LITTLE_ENDIAN);
        
        byte[] rdc = new byte[size + 1024*1024];
        recordDataCompressed = ByteBuffer.wrap(rdc);
        recordDataCompressed.order(ByteOrder.LITTLE_ENDIAN);
        
        byte[] rb = new byte[size + 1024*1024];
        recordBinary = ByteBuffer.wrap(rb);
        recordBinary.order(ByteOrder.LITTLE_ENDIAN);
    }
    /**
     * adds the byte[] array into the record stream.
     * @param event
     * @param position
     * @param length
     * @return 
     */
    public boolean addEvent(byte[] event, int position, int length){
        int  size = length;
        
        int indexSize = recordIndex.getInt(0);
        int eventSize = recordEvents.getInt(0);
        int combinedSize = indexSize + eventSize + HEADER_SIZE;
        if( (combinedSize + length)>= this.MAX_BUFFER_SIZE){
            //System.out.println(" the record is FULL..... INDEX SIZE = " 
            //        + (indexSize/4) + " DATA SIZE = " + eventSize);
            return false;
        }
        
        int resultSize = eventSize + length;
        recordEvents.position(eventSize);
        recordEvents.put(event, position, length);
        recordEvents.putInt(0, resultSize);
        
        recordIndex.putInt(indexSize, length);
        recordIndex.putInt(0, indexSize+4);
        return true;
    }
    /**
     * Add byte[] to the output stream. Index array is updated
     * to include the size of the event, and counter is incremented.
     * @param event 
     * @return  true is the event was added, false if the buffer is full.
     */
    public boolean addEvent(byte[] event){
        return addEvent(event,0,event.length);
        /*int  size = event.length;
        int count = recordIndex.getInt(0);
        recordIndex.putInt( (count+1)*4,    size);
        recordIndex.putInt(           0, count+1);
        
        try {
            recordStream.write(event);
        } catch (IOException ex) {
            Logger.getLogger(RecordStream.class.getName()).log(Level.SEVERE, null, ex);
        }*/
    }
    /**
     * Reset internal buffers. The capacity of the ByteArray stream is set to 0.
     * and the first integer of index array is set to 0. The buffer is ready to 
     * receive new data.
     */
    public void reset(){
        recordIndex.putInt(   0, 4); // length of the index array is reset
        recordEvents.putInt(  0, 4); // the length of the data is reset
        recordBinary.putInt(  4, 0); // set the size of the binary output buffer to 0
    }
    /**
     * constructs the word that describes compression, includes the compression
     * type (upper 8 bits) and compressed data size (lower 24 bits).
     * @param ctype compression type
     * @param csize compressed buffer size
     * @return word with combined type and size
     */
    private int getCompressionWord(int ctype, int csize){
        int word = ((ctype<<24)&0xFF000000)|(csize&0x00FFFFFF);
        return word;
    }
    
    private int getVersionWord(){
        int versionWord = ((this.blockBitInfo<<8)&(0xFFFFFF00))|blockVersion;
        return versionWord;
    }
    /**
     * Builds the record. First compresses the data buffer.
     * Then the header is constructed.
     */
    public void build(){
        
        int indexSize = recordIndex.getInt(  0) - 4;
        int eventSize = recordEvents.getInt( 0) - 4;
        
        recordData.position(0);
        recordData.put(  recordIndex.array(), 4, indexSize);
        recordData.put( recordEvents.array(), 4, eventSize);
        
        int dataBufferSize = indexSize + eventSize;
        
        int compressedSize = dataCompressor.compressLZ4(recordData, dataBufferSize, 
                recordDataCompressed, recordDataCompressed.array().length);
        
        //System.out.println(" DATA SIZE = " + dataBufferSize + "  COMPRESSED SIZE = " + compressedSize);
        int nevents = recordIndex.getInt(0)/4;
        
        int recordWordCount = (compressedSize + this.HEADER_SIZE)/4;
        if( (compressedSize+this.HEADER_SIZE)%4!=0) recordWordCount+=1;
        
        
        recordBinary.position(0);
        recordBinary.putInt(   0, recordWordCount);
        recordBinary.putInt(   4, blockNumber);
        recordBinary.putInt(   8, 16);
        recordBinary.putInt(  12, nevents);
        recordBinary.putInt(  16, reservedWord);
        recordBinary.putInt(  20, getVersionWord());
        recordBinary.putInt(  24, reservedWordSecond);
        recordBinary.putInt(  28, RECORD_UID_WORD_LE);
        recordBinary.putInt(  32, dataBufferSize);
        recordBinary.putInt(  36, getCompressionWord(compressionType, compressedSize));
        recordBinary.putInt(  40, 0);
        recordBinary.putInt(  44, indexSize/4);
        recordBinary.putLong( 48, recordHeaderUniqueWordFirst);
        recordBinary.putLong( 56, recordHeaderUniqueWordSecond);
        
        recordBinary.position(HEADER_SIZE);
        recordBinary.put(recordDataCompressed.array(), 0, compressedSize);
        
        /*
        int size = recordIndex.getInt(0)*4 + recordStream.size();
        byte[] buffer = new byte[size];
        
        byte[] indexBuffer = recordIndex.array();
        byte[]  dataBuffer = recordStream.toByteArray();
        int     dataOffset = recordIndex.getInt(0)*4;
        System.arraycopy(  dataBuffer, 0, buffer, dataOffset, dataBuffer.length);
        System.arraycopy( indexBuffer, 4, buffer,          0, dataOffset);
        
        byte[] dataCompressedBuffer = Compressor.getCompressedBuffer(1, buffer);
        byte[]         recordBuffer = new byte[48+dataCompressedBuffer.length];
        
        System.arraycopy(dataCompressedBuffer, 0, recordBuffer, 48, dataCompressedBuffer.length);
        ByteBuffer byteBuffer = ByteBuffer.wrap(recordBuffer);
        byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
        byteBuffer.putInt(  0, RECORD_UID_WORD_LE);
        byteBuffer.putInt(  4, recordBuffer.length);
        byteBuffer.putInt(  8, dataBuffer.length);
        byteBuffer.putInt( 12, dataCompressedBuffer.length);
        byteBuffer.putInt( 16, recordIndex.getInt(0));
        byteBuffer.putInt( 20, 0);
        byteBuffer.putInt( 24, recordIndex.getInt(0)*4);
                
        return byteBuffer.array();
        */
    }
    public int getEventCount(){
        return this.recordIndex.getInt(0)/4;
    }
    
    public ByteBuffer getBinaryBuffer(){
        return this.recordBinary;
    }
}
