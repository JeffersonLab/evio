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
    
    
    private int MAX_BUFFER_SIZE = 8*1024*1024;
    private int MAX_EVENT_COUNT = 1024*1024;
    
    private final int RECORD_UID_WORD_LE = 0x43455248;
    private final int RECORD_UID_WORD_BE = 0x48524543;
    
    ByteArrayOutputStream  recordStream = null;
    ByteBuffer              recordIndex = null;
    
    public RecordStream(){
        recordStream = new ByteArrayOutputStream(MAX_BUFFER_SIZE);
        byte[] index = new byte[MAX_EVENT_COUNT*4];
        recordIndex  = ByteBuffer.wrap(index);
        recordIndex.order(ByteOrder.LITTLE_ENDIAN);
        recordIndex.putInt(0, 0);
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
    
    public void addEvent(byte[] event, int position, int length){
        int  size = length;
        int count = recordIndex.getInt(0);
        recordIndex.putInt( (count+1)*4,    size);
        recordIndex.putInt(           0, count+1);
        try {
            recordStream.write(event);
        } catch (IOException ex) {
            Logger.getLogger(RecordStream.class.getName()).log(Level.SEVERE, null, ex);
        }
    }
    /**
     * Add byte[] to the output stream. Index array is updated
     * to include the size of the event, and counter is incremented.
     * @param event 
     */
    public void addEvent(byte[] event){
        addEvent(event,0,event.length);
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
        recordStream.reset();
        recordIndex.putInt(0, 0);
    }
    /**
     * Builds the record. First compresses the data buffer.
     * Then the header is constructed.
     * @return 
     */
    public byte[] build(){
        
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
    }
    
}
