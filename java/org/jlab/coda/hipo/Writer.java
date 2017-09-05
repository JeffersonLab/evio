/*
 *   Copyright (c) 2016.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */

package org.jlab.coda.hipo;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 *  Writer class for CODA data files.
 * @author gavalian
 * @date 08/10/2017
 */

public class Writer {
    /*
     * Internal constants used in the FILE header
     */
    // TODO: shouldn't this be 14*4 = 56?
    public final static int    FILE_HEADER_LENGTH = 72;
    public final static int    FILE_UNIQUE_WORD   = 0x4F504948;//0x4849504F;
    public final static int    FILE_VERSION_WORD  = 0x322E3056;//0x56302E32;
    public final static int    VERSION_NUMBER     = 6;
    public final static int    MAGIC_WORD_LE      = 0xc0da0100;
    public final static int    MAGIC_WORD_BE      = 0x0001dac0;
    
    /**
     * BYTE ORDER OF THE FILE
     */
    
    private ByteOrder  byteOrderFile = ByteOrder.LITTLE_ENDIAN;
    /**
     * output stream used for writing binary data to the file.
     */
    private       FileOutputStream  outStream = null;
    private RandomAccessFile  outStreamRandom = null;
    private       Record         outputRecord = null;
    private RecordOutputStream   outputRecordStream = null;
    /**
     * header byte buffer is stored when object is created.
     * and all subsequent files will have same header byte buffer.
     * the header is user defined and can be anything.
     */
    private byte[]  writerHeaderBuffer = null;    
    private Integer    compressionType = 0;
    private Long    writerBytesWritten = 0L;
    
    
    public Writer(String filename, ByteOrder order){
        byteOrderFile = order;
        outputRecordStream = new RecordOutputStream();
        outputRecordStream.reset();
        open(filename);
    }
    
    /**
     * default constructor only the internal record is initialized
     * no file will be opened.
     */
    public Writer(){
        outputRecord = new Record();
        outputRecordStream = new RecordOutputStream();
    }
    /**
     * constructor with filename, the output file will be initialized.
     * the header of the file will have zero length.
     * @param filename output file name
     */
    public Writer(String filename){
        outputRecord = new Record();
        outputRecordStream = new RecordOutputStream();
        this.open(filename);
    }
    /**
     * open a file with byte array that represents the header
     * @param filename output file name
     * @param header header array
     */
    public Writer(String filename, byte[] header){
        outputRecord = new Record();
        outputRecordStream = new RecordOutputStream();
        this.writerHeaderBuffer = header;
        this.open(filename, header);
    }
    /**
     * Open a file with given header and compression type
     * @param filename output file name
     * @param header header byte array
     * @param compression compression type
     */
    public Writer(String filename, byte[] header, int compression){
        outputRecord = new Record();
        this.writerHeaderBuffer = header;
        this.open(filename, header);
        setCompressionType(compression);
    }
    /**
     * Opens a file for writing with header array initialized from 
     * string. The header string can be an XML dictionary.
     * @param filename output file name
     * @param header string header object 
     * @param compression compression type
     */
    public Writer(String filename, String header, int compression){
        outputRecord = new Record();
        this.writerHeaderBuffer = header.getBytes();
        open(filename, header.getBytes());
        setCompressionType(compression);
    }    
    /**
     * open a new file with no header.
     * @param filename output file name
     */
    public final void open(String filename){
        this.open(filename, new byte[]{});
        
    }
    /**
     * Open a file with given header byte array.
     * @param filename disk file name
     * @param header byte array representing the header
     */
    public final void open(String filename, byte[] header){
        this.writerBytesWritten = (long) (Writer.FILE_HEADER_LENGTH + header.length);
        try {
            //outStream = new FileOutputStream(new File(filename));
            outStreamRandom = new RandomAccessFile(filename,"rw");
            /*
            byte[] headerBuffer = new byte[Writer.FILE_HEADER_LENGTH+header.length];
            
            ByteBuffer byteBuffer = ByteBuffer.wrap(headerBuffer);
            byteBuffer.order(ByteOrder.LITTLE_ENDIAN);
            
            byteBuffer.putInt(  0, Writer.FILE_UNIQUE_WORD);
            byteBuffer.putInt(  4, Writer.FILE_VERSION_WORD);
            byteBuffer.putInt(  8, 0x2);
            byteBuffer.putInt( 12, 0);
            byteBuffer.putInt( 16, header.length);
            byteBuffer.putInt( 20, VERSION_NUMBER);
            byteBuffer.putInt( 28, MAGIC_WORD_LE);
            if(byteOrderFile == ByteOrder.BIG_ENDIAN) byteBuffer.putInt(28, MAGIC_WORD_BE);
            
            System.arraycopy(header, 0, headerBuffer, Writer.FILE_HEADER_LENGTH, header.length);
            */
            ByteBuffer headerBuffer = this.createHeader(header);
            outStreamRandom.write(headerBuffer.array());
            
        } catch (FileNotFoundException ex) {
            Logger.getLogger(Writer.class.getName()).log(Level.SEVERE, null, ex);
        } catch (IOException ex) {
            Logger.getLogger(Writer.class.getName()).log(Level.SEVERE, null, ex);
        }
    }
    /**
     * sets compression type for the file. The compression type
     * is also set for internal record. When writing to the file
     * record data will be compressed according to the type set.
     * @param compression compression type
     * @return this object
     */
    public final Writer setCompressionType(int compression){
        this.outputRecord.setCompressionType(compression);
        this.compressionType = outputRecord.getCompressionType();
        return this;
    }
    
    public ByteBuffer createHeader(byte[] userHeader){
        
        int uhsize = userHeader.length;
        RecordHeader header = new RecordHeader();
        
        header.setCompressedDataLength(0).setDataLength(0);
        header.setCompressionType(0).setIndexLength(0);
        header.setUserHeaderLength(uhsize);
        header.setHeaderLength(14);
        int words = 14 + header.getUserHeaderLengthWords();
        header.setLength(words);
        
        byte[] array = new byte[words*4];
        ByteBuffer buffer = ByteBuffer.wrap(array);
        buffer.order(ByteOrder.LITTLE_ENDIAN);
        
        header.writeHeader(buffer, 0);
        System.arraycopy(userHeader, 0, array, 14*4, userHeader.length);
        
        buffer.putInt(0, 0x4F495645);
        
        /*
        int size = userHeader.length;
        int uhWords = (size)/4;
        
        if(userHeader.length%4!=0) uhWords++;
        
        System.out.println(" SIZE = " + size + "  words = " + uhWords);
        byte[]  fileHeader = new byte[64+uhWords*4];

        System.arraycopy(userHeader, 0, fileHeader, 64, userHeader.length);
        ByteBuffer headerBuffer = ByteBuffer.wrap(fileHeader);
        headerBuffer.order(ByteOrder.LITTLE_ENDIAN);
        
        headerBuffer.putInt(  0, 16+uhWords);
        headerBuffer.putInt(  4, 0);
        headerBuffer.putInt(  8, 16);
        headerBuffer.putInt( 12, 0);
        headerBuffer.putInt( 16, 0);
        headerBuffer.putInt( 20, 6);
        headerBuffer.putInt( 24, 0);
        
        headerBuffer.putInt( 28, MAGIC_WORD_LE);
        headerBuffer.putInt( 32, Writer.FILE_UNIQUE_WORD);
        headerBuffer.putInt( 36, Writer.FILE_VERSION_WORD);
        headerBuffer.putInt( 40, userHeader.length);
        headerBuffer.putInt( 44, 0);
        
        */
        return buffer;
    }
    /**
     * Appends the record to the file.
     * @param record record object
     */
    private void writeRecord(Record record){
        ByteBuffer buffer = record.build();        
        try {
            byte[] bufferArray = buffer.array();
            this.writerBytesWritten += bufferArray.length;
            this.outStream.write(buffer.array());
        } catch (IOException ex) {
            Logger.getLogger(Writer.class.getName()).log(Level.SEVERE, null, ex);
        }
    }
    
    public void addEvent(byte[] buffer, int offset, int length){
        boolean status = outputRecordStream.addEvent(buffer, offset,length);
        if(status==false){
            writeOutput();
            outputRecordStream.addEvent(buffer, offset,length);
        }
    }
    /**
     * add an byte buffer to the internal record. if the length of
     * the buffer exceeds the maximum size of the record, the record
     * will be written to the file (compressed if the flag is set), then
     * it will be reset to receive new buffers.
     * @param buffer array to add to the file.
     */
    public void addEvent(byte[] buffer){
        addEvent(buffer,0,buffer.length);
        /*
        boolean status = outputRecord.addEvent(buffer);
        if(status==false){
            //ByteBuffer outbytes = outputRecord.build();
            writeRecord(outputRecord);
            outputRecord.reset();
            outputRecord.addEvent(buffer);
        }*/
    }
    
    private void writeOutput(){
        
        byte[] header = new byte[233];
        ByteBuffer userHeader = ByteBuffer.wrap(header);

        outputRecordStream.getHeader().setRecordNumber(234);
        outputRecordStream.build(userHeader);
        
        ByteBuffer buffer = outputRecordStream.getBinaryBuffer();
        int bufferSize = buffer.getInt(0)*4;
        
        try {
            outStreamRandom.write(buffer.array(), 0, bufferSize);
            outputRecordStream.reset();
        } catch (IOException ex) {
            Logger.getLogger(Writer.class.getName()).log(Level.SEVERE, null, ex);
        }
    }
    /**
     * Close opened file. if the output record contains events.
     * the events will be flushed out.
     */
    public void close(){
        if(outputRecordStream.getEventCount()>0){
            writeOutput();
        }
        
        try {
            outStreamRandom.close();
            /*
            if(outputRecord.getEntries()>0){
            System.out.println("[writer] ---> closing file. flashing " +
            outputRecord.getEntries() + " events.");
            this.writeRecord(outputRecord);
            }
            try {
            this.outStream.close();
            System.out.println("[writer] ---> bytes written " + this.writerBytesWritten);
            } catch (IOException ex) {
            Logger.getLogger(Writer.class.getName()).log(Level.SEVERE, null, ex);
            }*/
        } catch (IOException ex) {
            Logger.getLogger(Writer.class.getName()).log(Level.SEVERE, null, ex);
        }
    }
}
