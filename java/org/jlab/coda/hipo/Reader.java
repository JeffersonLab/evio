/*
 *   Copyright (c) 2016.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.hipo;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * Reader class that reads files stored in the HIPO
 * format. 
 * @author gavalian
 * @date 08/10/2017
 */
public class Reader {
    /**
     * List of records in the file. The array is initialized
     * at the open() method when the entire file is scanned
     * to read out positions of each record in the file.
     * 
     */    
    
    private final List<RecordPosition>  recordPositions = new ArrayList<RecordPosition>();
    /**
     * Input binary file stream.
     */
    FileInputStream  inputStream = null;
    
    private          RandomAccessFile  inStreamRandom = null;
    private final RecordInputStream inputRecordStream = new RecordInputStream();
    
    
    private int currentRecordLoaded = 0;
    /**
     * Default constructor. Does nothing. If instance is created
     * with default constructor the open() method has to be used 
     * to open the input stream.
     */
    public Reader(){
        
    }
    /**
     * Constructor with filename. creates instance and opens 
     * the input stream with given name
     * @param filename input file name
     */
    public Reader(String filename){
        open(filename);
        scanFile();
    }
    /**
     * Opens an input stream in binary mode. Scans for
     * records in the file and stores record information
     * in internal array. Each record can be read from the file.
     * @param filename input file name
     */
    public final void open(String filename){
        if(inStreamRandom != null){
           if( inStreamRandom.getChannel().isOpen() == true) {
               try {
                   System.out.println("[READER] ---> closing current file : " + inStreamRandom.getFilePointer());
                   inStreamRandom.close();
               } catch (IOException ex) {
                   Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
               }               
           }
        }
        try {
            //inputStream = new FileInputStream(new File(filename));
            //this.scanFile();
            System.out.println("[READER] ----> openning current file : " + filename);
            inStreamRandom = new RandomAccessFile(filename,"r");            
            System.out.println("[READER] ---> open successfull, size : " + inStreamRandom.length());
        } catch (FileNotFoundException ex) {
            Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
        } catch (IOException ex) {
            Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
        }
    }
    
    public byte[] getEvent(int index){
        return inputRecordStream.getEvent(index);
    }
    
    public void getEvent(ByteBuffer buffer, int index){
        try {
            inputRecordStream.getEvent(buffer, index);
        } catch (HipoException ex) {
            /** If the sizes are not right**/
            System.out.println(ex.getMessage());
        }
    }
    
    public int getEventCount(){
        return inputRecordStream.getEntries();
    }
    /**
     * Reads record from the file from position index.
     * @param index record index
     * @return decoded record from the file
     * @throws HipoException if file not in hipo format
     */
    public boolean readRecord(int index) throws HipoException {
        if(index>=0&&index<recordPositions.size()){
            RecordPosition pos = recordPositions.get(index);
            inputRecordStream.readRecord(inStreamRandom, pos.getPosition());
            currentRecordLoaded = index;
            return true;
        }
        return false;
    }
    /**
     * Returns the number of records recovered from the file.
     * @return 
     */
    public int getRecordCount(){
        return recordPositions.size();
    }
    /**
     * returns the index of the record that has been loaded.
     * @return index of loaded record
     */
    public int getCurrentRecord(){
        return this.currentRecordLoaded;
    }
    /**
     * Scans the file to index all the record positions.
     */
    private void scanFile(){
        
        byte[]     fileHeader = new byte[RecordHeader.HEADER_SIZE_BYTES];
        FileHeader header = new FileHeader();
        RecordHeader recordHeader = new RecordHeader();

        try {
            
            inStreamRandom.getChannel().position(0L);
            inStreamRandom.read(fileHeader);
            
            ByteBuffer buffer = ByteBuffer.wrap(fileHeader);
            buffer.order(ByteOrder.LITTLE_ENDIAN);
            
            header.readHeader(buffer);
            //System.out.println(header.toString());
            
            int userHeaderWords = header.getUserHeaderLengthWords();            
            long recordPosition = header.getHeaderLength() + userHeaderWords*4;

            //System.out.println(" FIRST RECORD POSITION = " + recordPosition);
            long fileSize = inStreamRandom.length();
            
            long    maximumSize = fileSize - RecordHeader.HEADER_SIZE_BYTES;
            int     numberOfRecords = 0;
            recordPositions.clear();
            
            while(recordPosition < maximumSize){
                inStreamRandom.getChannel().position(recordPosition);
                inStreamRandom.read(fileHeader);
                ByteBuffer recordBuffer = ByteBuffer.wrap(fileHeader);
                recordBuffer.order(ByteOrder.LITTLE_ENDIAN);
                recordHeader.readHeader(recordBuffer);
                //System.out.println(">>>>>==============================================");
                //System.out.println(recordHeader.toString());
                int offset = recordHeader.getLength();
                RecordPosition pos = new RecordPosition(recordPosition);
                pos.setLength(offset);
                pos.setCount(recordHeader.getEntries());
                this.recordPositions.add(pos);
                recordPosition += offset;
                numberOfRecords++;
            }
            //System.out.println("NUMBER OF RECORDS " + recordPositions.size());
            //System.out.println(" recovered records = " + readerRecordEntries.size());
        } catch (IOException ex) {
            Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
        } catch (HipoException ex) {
            Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
        }
    }
    
    public void show(){
        System.out.println(" ***** FILE: (info), RECORDS = "
                + recordPositions.size() + " *****");
        for(RecordPosition entry : this.recordPositions){
            System.out.println(entry);
        }
    }
    
    /**
     * Internal class to keep track of the records in the file.
     * Each entry keeps record position in the file, length of
     * the record and number of entries contained.
     */
    public static class RecordPosition {
        
        private long position;
        private int  length;
        private int  count;
        private int  firstEvent;
        
        public RecordPosition(long _pos){
            position = _pos;
        }
        
        public RecordPosition setPosition(long _pos){ position = _pos; return this; }
        public RecordPosition setLength(int _len)  { length = _len;   return this; }
        public RecordPosition setCount( int  _cnt)  { count = _cnt;   return this; }
        
        public long getPosition(){ return position;}
        public int  getLength(){   return   length;}
        public int  getCount(){    return    count;}
        
        @Override
        public String toString(){
            return String.format(" POSITION = %16d, LENGTH = %12d, COUNT = %8d", position, length, count);
        }
    }
    
    public static void main(String[] args){
        Reader reader = new Reader("converted_000810.evio");
        
        //reader.show();
        byte[] eventarray = new byte[1024*1024];
        ByteBuffer eventBuffer = ByteBuffer.wrap(eventarray);
        
        for(int i = 0; i < reader.getRecordCount(); i++){
            try {
                reader.readRecord(i);
                Thread.sleep(100);
            } catch (Exception ex) {
                Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
            }
            int nevents = reader.getEventCount();
            for(int k = 0; k < nevents ; k++){
                reader.getEvent(eventBuffer,k);
            }
            System.out.println("---> read record " + i);
        }
        
        //reader.open("test.evio");
        /*reader.readRecord(0);
        int nevents = reader.getEventCount();
        System.out.println("-----> events = " + nevents);
        for(int i = 0; i < 10 ; i++){
            byte[] event = reader.getEvent(i);
            System.out.println("---> events length = " + event.length);
            ByteBuffer buffer = ByteBuffer.wrap(event);
            buffer.order(ByteOrder.LITTLE_ENDIAN);
            String data = DataUtils.getStringArray(buffer, 10,30);
            System.out.println(data);
        }*/
    }
}
