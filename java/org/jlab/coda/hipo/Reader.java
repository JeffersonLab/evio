/*
 *   Copyright (c) 2016.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.hipo;

import org.jlab.coda.jevio.ByteDataTransformer;
import org.jlab.coda.jevio.EvioException;
import org.jlab.coda.jevio.EvioXMLDictionary;

import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
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
     */
    private final List<RecordPosition>  recordPositions = new ArrayList<RecordPosition>();
//    private final List<RecordPosition>  eventPositions  = new ArrayList<RecordPosition>();
    /** Input binary file stream. */
    private FileInputStream  inputStream;
    /** Fastest way to read/write files. */
    private RandomAccessFile  inStreamRandom;
    /** Keep one record for reading in data record-by-record. */
    private final RecordInputStream inputRecordStream = new RecordInputStream();
    
    /** Number or position of record to be read next. */
    private int currentRecordLoaded;

    /**
     * In order of priority:
     * If a trailer with index info exists, use that instead of scanning file.
     * If not and a header with index info exists, use that except if this flag
     * is true. In that case, do a scan and ignore header.
     */
    private boolean forceScan;

    private boolean sequential = true;

    /** When doing a sequential read, used to assign a transient
     * number [1..n] to events as they are being read. */
    private int currentEventNumber;

    /** Files may have an xml format dictionary in the user header of the file header. */
    private String dictionaryXML;

    /**
     * Default constructor. Does nothing. If instance is created
     * with default constructor the {@link #open(String)} method has to be used
     * to open the input stream.
     */
    public Reader() {}

    /**
     * Constructor with filename. creates instance and opens 
     * the input stream with given name
     * @param filename input file name
     */
    public Reader(String filename){
        open(filename);
        scanFile(forceScan);
    }

    /**
     * Constructor with filename. creates instance and opens
     * the input stream with given name
     * @param filename input file name
     * @param forceScan force a scan of file even if index info exists in header.
     */
    public Reader(String filename, boolean forceScan){
        this.forceScan = forceScan;
        open(filename);
        scanFile(forceScan);
    }

    /**
     * Opens an input stream in binary mode. Scans for
     * records in the file and stores record information
     * in internal array. Each record can be read from the file.
     * @param filename input file name
     */
    public final void open(String filename){
        if(inStreamRandom != null && inStreamRandom.getChannel().isOpen()) {
            try {
                System.out.println("[READER] ---> closing current file : " + inStreamRandom.getFilePointer());
                inStreamRandom.close();
            } catch (IOException ex) {
                Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
            }
        }

        try {
            //inputStream = new FileInputStream(new File(filename));
            //scanFile();
            System.out.println("[READER] ----> opening current file : " + filename);
            inStreamRandom = new RandomAccessFile(filename,"r");
            System.out.println("[READER] ---> open successful, size : " + inStreamRandom.length());
        } catch (FileNotFoundException ex) {
            Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
        } catch (IOException ex) {
            Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
        }
    }

    // Methods for file in general
//    /**
//    * Get the XML format dictionary is there is one.
//    *
//    * @return XML format dictionary, else null.
//    */
//   public EvioXMLDictionary getDictionary() {
//       return dictionaryXML;
//   }

    /**
     * Get the XML format dictionary if there is one.
     * @return XML format dictionary, else null.
     */
    public String getDictionaryXML() {
        if (hasDictionary() && dictionaryXML != null) return dictionaryXML;
        try {
            RecordInputStream userRecord = inputRecordStream.getUserHeaderAsRecord(null, 0);
        }
        catch (HipoException e) {/* never happen */}

        return dictionaryXML;
    }

    /**
     * Does this evio file have an associated XML dictionary?
     *
     * @return <code>true</code> if this evio file has an associated XML dictionary,
     *         else <code>false</code>
     */
    public boolean hasDictionary() {return dictionaryXML != null;}

    /**
     * Get a byte array representing the next event while sequentially reading.
     * @return byte array representing the next event.
     */
    public byte[] getNextEvent() {
        return inputRecordStream.getEvent(currentEventNumber++);
    }

    // Methods for current record

    /**
     * Get a byte array representing the specified event from the current record.
     * @param index index of specified event within current record,
     *              contiguous starting at 0.
     * @return byte array representing the specified event.
     */
    public byte[] getEvent(int index) {return inputRecordStream.getEvent(index);}

    /**
     * Get data representing the specified event from the current record
     * and place it in given buffer.
     * @param buffer buffer in which to place event data.
     * @param index  index of specified event within current record,
     *               contiguous starting at 0.
     * @throws HipoException if buffer has insufficient space to contain event
     *                       (buffer.capacity() < event size).
     */
    public void getEvent(ByteBuffer buffer, int index) throws HipoException {
        inputRecordStream.getEvent(buffer, index);
    }

    /**
     * Get the number of events in current record.
     * @return number of events in current record.
     */
    public int getEventCount() {return inputRecordStream.getEntries();}

    /**
     * Get the index of the current record.
     * @return index of the current record.
     */
    public int getCurrentRecord() {return currentRecordLoaded;}

    /**
     * Get the current record stream.
     * @return current record stream.
     */
    public RecordInputStream getCurrentRecordStream() {return inputRecordStream;}

    /**
     * Reads record from the file at the given record index.
     * @param index record index  (starting at 0).
     * @return true if valid index and successful reading record, else false.
     * @throws HipoException if file not in hipo format
     */
    public boolean readRecord(int index) throws HipoException {
        if (index >= 0 && index < recordPositions.size()) {
            RecordPosition pos = recordPositions.get(index);
            inputRecordStream.readRecord(inStreamRandom, pos.getPosition());
            currentRecordLoaded = index;
            return true;
        }
        return false;
    }

    //-----------------------------------------------------------------

    /**
     * Get the number of records read from the file.
     * @return number of records read from the file.
     */
    public int getRecordCount() {return recordPositions.size();}

    /** Scans the file to index all the record positions. */
    private void scanFileOrig(){

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

    /**
     * Scans the file to index all the record positions.
     * It takes advantage of any existing indexes in file.
     * @param force if true, force a file scan even except if header has index info.
     */
    private void scanFile(boolean force) {
        
        byte[] headerBytes      = new byte[RecordHeader.HEADER_SIZE_BYTES];
        ByteBuffer headerBuffer = ByteBuffer.wrap(headerBytes);

        FileHeader fileHeader     = new FileHeader();
        RecordHeader recordHeader = new RecordHeader();

        try {
            FileChannel channel = inStreamRandom.getChannel().position(0L);

            // Read and parse file header
            inStreamRandom.read(headerBytes);
            fileHeader.readHeader(headerBuffer);
            //System.out.println(header.toString());

            // First record position (past file's header + index + user header)
            long recordPosition = fileHeader.getLength();

            // Is there an existing record length index?
            // Index in trailer gets first priority.
            // Index in file header gets next priority but
            // only if told not to force a scan.
            boolean fileHasIndex = fileHeader.hasTrailerWithIndex() || (fileHeader.hasIndex() && !force);

            // If there is an index, use that instead of generating a list by scanning
            if (fileHasIndex) {
                int indexLength;
               
                // If we have a trailer with indexes ...
                if (fileHeader.hasTrailerWithIndex()) {
                    // Position read right before trailing header
                    channel.position(fileHeader.getTrailerPosition());

                    // Read trailer
                    inStreamRandom.read(headerBytes);
                    recordHeader.readHeader(headerBuffer);
                    indexLength = recordHeader.getIndexLength();
                }
                else {
                    // If index immediately follows file header,
                    // we're already in position to read it.
                    indexLength = fileHeader.getIndexLength();
                }

                // Read indexes
                byte[] index = new byte[indexLength];
                inStreamRandom.read(index);
                int len;

                try {
                    // Turn bytes into record lengths & event counts
                    int[] intData = ByteDataTransformer.toIntArray(index, fileHeader.getByteOrder());
                    // Turn record lengths into file positions and store in list
                    recordPositions.clear();
                    for (int i=0; i < intData.length; i += 2) {
                        len = intData[i];
                        RecordPosition pos = new RecordPosition(recordPosition, len, intData[i+1]);
                        recordPositions.add(pos);
                        recordPosition += len;
                    }
                }
                catch (EvioException e) {/* never happen */}

                return;
            }

            // If here, there is either no valid index or only one in header but
            // force flag was set, so scan file by
            // reading each record header and storing its position,
            // length, and event count.
            int indexLength = fileHeader.getIndexLength();
            long fileSize = inStreamRandom.length();
            long maximumSize = fileSize - RecordHeader.HEADER_SIZE_BYTES - indexLength;
            recordPositions.clear();
            int  offset;

            while (recordPosition < maximumSize) {
                channel.position(recordPosition);
                inStreamRandom.read(headerBytes);
                recordHeader.readHeader(headerBuffer);
                //System.out.println(">>>>>==============================================");
                //System.out.println(recordHeader.toString());
                offset = recordHeader.getLength();
                RecordPosition pos = new RecordPosition(recordPosition, offset,
                                                        recordHeader.getEntries());
                recordPositions.add(pos);
                recordPosition += offset;
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
    private static class RecordPosition {
        /** Position in file. */
        private long position;
        /** Length in bytes. */
        private int  length;
        /** Number of entries in record. */
        private int  count;

        RecordPosition(long pos) {position = pos;}
        RecordPosition(long pos, int len, int cnt) {
            position = pos; length = len; count = cnt;
        }

        public RecordPosition setPosition(long _pos){ position = _pos; return this; }
        public RecordPosition setLength(int _len)   { length = _len;   return this; }
        public RecordPosition setCount( int _cnt)   { count = _cnt;    return this; }
        
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
                int nevents = reader.getEventCount();
                for(int k = 0; k < nevents ; k++){
                    reader.getEvent(eventBuffer,k);
                }
                System.out.println("---> read record " + i);
            } catch (Exception ex) {
                Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
            }
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
