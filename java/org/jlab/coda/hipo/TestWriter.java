/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package org.jlab.coda.hipo;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.jlab.coda.jevio.EvioCompactReader;
import org.jlab.coda.jevio.EvioException;

/**
 *
 * @author gavalian
 */
public class TestWriter {
    
    public static byte[] generateBuffer(){
        int size =  (int) (Math.random()*35.0);
        size+= 480;
        byte[] buffer = new byte[size];
        for(int i = 0; i < buffer.length; i++){
            buffer[i] =  (byte) (Math.random()*126.0);
        }
        return buffer;
    }
    
    public static byte[] generateBuffer(int size){
        byte[] buffer = new byte[size];
        for(int i = 0; i < buffer.length; i++){
            buffer[i] =  (byte) (Math.random()*125.0 + 1.0);
        }
        return buffer;
    }
    
    public static void print(byte[] array){
        StringBuilder str = new StringBuilder();
        int wrap = 20;
        for(int i = 0; i < array.length; i++){
            str.append(String.format("%3X ", array[i]));
            if((i+1)%wrap==0) str.append("\n");
        }
        System.out.println(str.toString());
    }
    
    
    public static void byteStream(){
        
        ByteArrayOutputStream stream = new ByteArrayOutputStream(4*1024);
        
        System.out.println(" STREAM SIZE = " + stream.size());
        
        byte[] array = TestWriter.generateBuffer();
        System.out.println("ARRAY SIZE = " + array.length);
        int compression = 2345;
        stream.write(compression);
        
        System.out.println(" STREAM SIZE AFTER WRITE = " + stream.size());
        
        byte[] buffer = stream.toByteArray();
        System.out.println( " OBTAINED ARRAY LENGTH = " + buffer.length + "  " + buffer[0]);
    }
    
    public static void streamRecord(){
        RecordOutputStream stream = new RecordOutputStream();
        byte[] buffer = TestWriter.generateBuffer();
        while(true){
            //byte[] buffer = TestWriter.generateBuffer();
            boolean flag = stream.addEvent(buffer);
            if(flag==false){
                stream.build();
                stream.reset();
            }
        }

    }
    
    public static void writerTest(){
        Writer writer = new Writer("compressed_file.evio",ByteOrder.BIG_ENDIAN);
        //byte[] array = TestWriter.generateBuffer();
        for(int i = 0; i < 340000; i++){
            byte[] array = TestWriter.generateBuffer();
            writer.addEvent(array);
        }
        writer.close();
    }
    
    
    public static void convertor() {
        String filename = "/Users/gavalian/Work/Software/project-1a.0.0/clas_000810.evio.324";
        try {
            EvioCompactReader  reader = new EvioCompactReader(filename);
            int nevents = reader.getEventCount();
            String userHeader = "File is written with new version=6 format";
            Writer writer = new Writer("converted_000810.evio",userHeader.getBytes());
            
            System.out.println(" OPENED FILE EVENT COUNT = " + nevents);
            
            byte[] myHeader = new byte[233];
            ByteBuffer header = ByteBuffer.wrap(myHeader);
            
            for(int i = 1; i < nevents; i++){
                ByteBuffer buffer = reader.getEventBuffer(i,true); 
                writer.addEvent(buffer.array());
                
                //System.out.println(" EVENT # " + i + "  size = " + buffer.array().length );                
            }
            writer.close();
        } catch (EvioException ex) {
            Logger.getLogger(TestWriter.class.getName()).log(Level.SEVERE, null, ex);
        } catch (IOException ex) {
            Logger.getLogger(TestWriter.class.getName()).log(Level.SEVERE, null, ex);
        }
        
    }
    
    
    public static void createEmptyFile(){
       String userHeader = "Example of creating a new header file.......?";
       System.out.println("STRING LENGTH = " + userHeader.length());
       Writer writer = new Writer();
       writer.open("example_file.evio", userHeader.getBytes());
       byte[] array = TestWriter.generateBuffer(4500);
       writer.addEvent(array);
       writer.close();
    }
    
    public static void main(String[] args){
        
        TestWriter.createEmptyFile();
        
        /*Writer writer = new Writer();
        
        writer.open("new_header_test.evio",new byte[]{'a','b','c','d','e'});
        writer.close(); */
        
        //writer.createHeader(new byte[17]);
        
        //TestWriter.convertor();
        
        //TestWriter.writerTest();
        
        //TestWriter.streamRecord();
        
        //TestWriter.byteStream();
        
        /*
        byte[] header = TestWriter.generateBuffer(32);
        
        Writer writer = new Writer("compressed.evio", header,1);
        
        for(int i = 0 ; i < 425; i++){
            byte[] array = TestWriter.generateBuffer();
            writer.addEvent(array);
        }
        
        writer.close();
        
        Reader reader = new Reader();
        reader.open("compressed.evio");
        reader.show();
        Record record = reader.readRecord(0);
        record.show();
        */
        
        
        /*
        Record record = new Record();
        for(int i = 0; i < 6; i++){
            byte[] bytes = TestWriter.generateBuffer();
            record.addEvent(bytes);            
        }
        
        record.show();
        record.setCompressionType(0);
        byte[] data = record.build().array();
        System.out.println(" BUILD BUFFER SIZE = " + data.length);
        TestWriter.print(data);
        
        Record record2 = Record.initBinary(data);
        
        record2.show();*/
    }
}
