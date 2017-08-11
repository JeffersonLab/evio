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
    private List<RecordEntry>  readerRecordEntries = 
            new ArrayList<RecordEntry>();
    /**
     * Input binary file stream.
     */
    FileInputStream  inputStream = null;
    
    
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
    }
    /**
     * Opens an input stream in binary mode. Scans for
     * records in the file and stores record information
     * in internal array. Each record can be read from the file.
     * @param filename input file name
     */
    public final void open(String filename){
        try {
            inputStream = new FileInputStream(new File(filename));
            this.scanFile();
        } catch (FileNotFoundException ex) {
            Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
        }
    }
    
    /**
     * Reads record from the file from position index.
     * @param index record index
     * @return decoded record from the file
     */
    public Record readRecord(int index){
        Long   position = readerRecordEntries.get(index).getPosition();
        Integer    size = readerRecordEntries.get(index).getLength();
        byte[] buffer = new byte[size];
        //System.out.println(" READ RECORD SIZE = " + buffer.length);
        try {
            inputStream.getChannel().position(position);
            inputStream.read(buffer);
        } catch (IOException ex) {
            Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
        }
        
        Record rec = Record.initBinary(buffer);
        return rec;
    }
    /**
     * Returns the number of records recovered from the file.
     * @return 
     */
    public int getRecordCount(){
        return this.readerRecordEntries.size();
    }
    
    private void scanFile(){
        byte[]  fileHeader   = new byte[Writer.FILE_HEADER_LENGTH];
        
        try {
            inputStream.read(fileHeader);
            ByteBuffer buffer = ByteBuffer.wrap(fileHeader);
            buffer.order(ByteOrder.LITTLE_ENDIAN);
            Integer    headerLength = buffer.getInt(16);
            
            Long       recordPosition = (long) Writer.FILE_HEADER_LENGTH + headerLength;
            Long      inputStreamSize = inputStream.getChannel().size();
            byte[]  recordBuffer = new byte[48];
            readerRecordEntries.clear();
            //System.out.println("--------->  RECORD POSITION " + recordPosition 
            //        + "  FILE SIZE = " + inputStreamSize);
            while( (recordPosition + 48) < inputStreamSize ){
                inputStream.getChannel().position(recordPosition);
                inputStream.read(recordBuffer);
                ByteBuffer header = ByteBuffer.wrap(recordBuffer);
                header.order(ByteOrder.LITTLE_ENDIAN);
                Integer recordLength = header.getInt(4);
                Integer   eventCount = header.getInt(16);
                RecordEntry entry = new RecordEntry(recordPosition,recordLength,eventCount);
                readerRecordEntries.add(entry);
                recordPosition += recordLength;
            }
            //System.out.println(" recovered records = " + readerRecordEntries.size());
        } catch (IOException ex) {
            Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
        }
    }
    
    public void show(){
        System.out.println("FILE: (info)");
        for(RecordEntry entry : this.readerRecordEntries){
            System.out.println(entry);
        }
    }
    /**
     * Internal class to keep track of the records in the file.
     * Each entry keeps record position in the file, length of
     * the record and number of entries contained.
     */
    public static class RecordEntry {
        
        Long     position = (long) 0;
        Integer    length = 0;
        Integer   entries = 0;
        
        public RecordEntry(long _pos, int _l, int _e){
            position = _pos; length = _l; entries = _e;
        }
        public Long getPosition(){ return position;}
        public Integer getLength(){ return length;}
        public Integer getEntries(){return entries;}
        
        @Override
        public String toString(){
            StringBuilder str = new StringBuilder();
            str.append(String.format("\t position : %9d, buffer count : %8d, length : %8d",
                    position,entries,length));
            return str.toString();
        }
    }
}
