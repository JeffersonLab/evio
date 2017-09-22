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
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 *
 * @author gavalian
 */
public class RecordInputStream {
    
    private RecordHeader        header = null;    
    private ByteBuffer    dataBuffer   = null;
    private ByteBuffer    recordBuffer = null;
    private ByteBuffer    headerBuffer = null;    
    private Compressor    compressor   = new Compressor();
    /**
     * Set offsets to the uncompressed buffer for retrieving the
     * user header and the rest of the data.
     */
    private int           nEntries = 0;
    private int   userHeaderOffset = 0;
    private int       eventsOffset = 0;
    
    public RecordInputStream(){
        allocate(8*1024*1024); 
        header = new RecordHeader();
    }
    
    private void allocate(int size){
        byte[] arrayData = new byte[size];
        dataBuffer = ByteBuffer.wrap(arrayData);
        dataBuffer.order(ByteOrder.LITTLE_ENDIAN);
        
        byte[] arrayRecord = new byte[size];
        recordBuffer = ByteBuffer.wrap(arrayRecord);
        recordBuffer.order(ByteOrder.LITTLE_ENDIAN);
        
        byte[] arrayHeader = new byte[72];
        headerBuffer = ByteBuffer.wrap(arrayHeader);
        headerBuffer.order(ByteOrder.LITTLE_ENDIAN);
    }
    
    public byte[] getEvent(int index){
        int lastPosition  = dataBuffer.getInt(index*4);
        int firstPosition = 0;
        if(index>0) firstPosition = dataBuffer.getInt( (index-1)*4 );
        byte[] event = new byte[lastPosition-firstPosition];
        int   offset = eventsOffset + firstPosition;
        //System.out.println(" reading from " + offset + "  length = " + event.length);
        System.arraycopy(dataBuffer.array(), offset, event, 0, event.length);
        return event;
    }
    /**
     * Writes event with index into a byte buffer, The byte buffer has 
     * to have proper size.
     * @param buffer
     * @param index
     * @return 
     * @throws org.jlab.coda.hipo.HipoException 
     */
    public boolean getEvent(ByteBuffer buffer, int index) throws HipoException {        
        int lastPosition  = dataBuffer.getInt(index*4);
        int firstPosition = 0;
        if(index>0) firstPosition = dataBuffer.getInt( (index-1)*4 );
        int length = lastPosition - firstPosition;
        int offset = eventsOffset + firstPosition;
        if(buffer.hasArray()==true){
            if(buffer.array().length>=length){
                System.arraycopy(dataBuffer.array(), offset, buffer.array(), 0, length);
                buffer.position(0);
                buffer.limit(length);
                return true;
            } else {
                throw new HipoException("ByteBuffer is smaller than the event.");
            }
        }
        return false;
    }
    /**
     * Reads the record from the file from given position and given
     * length.
     * @param file opened file descriptor
     * @param position position in the file
     * @param length the length of the record
     */
    public void readRecord(RandomAccessFile file, long position, int length){
        try {
            file.getChannel().position(position);
            
        } catch (IOException ex) {
            Logger.getLogger(RecordInputStream.class.getName()).log(Level.SEVERE, null, ex);
        }
    }
    /**
     * Reads record from the file from given position. First the header
     * is read then the length of the record is read from header, then
     * following bytes are read and decompressed.
     * @param file opened file descriptor
     * @param position position in the file
     */
    public void readRecord(RandomAccessFile file, long position) {
        try {
            file.getChannel().position(position);
            file.read(headerBuffer.array());
            header.readHeader(headerBuffer);
            //System.out.println(header);
            int recordLengthWords = header.getLength();
            int headerLength      = header.getHeaderLength();
            //System.out.println(" READIN FROM POSITION " 
            //        + (position) + "  DATA SIZE = " + (recordLengthWords));
            file.getChannel().position(position+headerLength);
            file.read(recordBuffer.array(), 0, recordLengthWords);
            int cLength = header.getCompressedDataLength();
            int padding = header.getCompressedDataLengthPadding();
            //System.out.println(" compressed size = " + cLength + "  padding = " + padding);
            int uncSize = 0;
            try {
                uncSize = compressor.uncompressLZ4(recordBuffer,
                        cLength, dataBuffer);
            }
            catch (HipoException e) {
                System.out.println("*** something went wrong ***");
                /* should not happen */
            }

            nEntries = header.getEntries();
            userHeaderOffset = nEntries*4;
            eventsOffset     = userHeaderOffset + header.getUserHeaderLengthWords()*4;
            //showIndex();
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
     * returns number of the events packed in the record.
     * @return 
     */
    public int getEntries(){ return nEntries;}
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
    }
}
