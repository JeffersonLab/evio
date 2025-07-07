/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package org.jlab.coda.hipo.test;

import java.io.FileInputStream;
import java.io.IOException;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;

import org.jlab.coda.hipo.*;

/**
 *
 * @author gavalian
 */
public class TestWriter {

    /**
     * Generate a byte array of random data.
     * @return byte array of random data.
     */
    public static byte[] generateBuffer(){
        int size =  (int) (Math.random()*35.0);
        size+= 480;
        byte[] buffer = new byte[size];
        for(int i = 0; i < buffer.length; i++){
            buffer[i] =  (byte) (Math.random()*126.0);
        }
        return buffer;
    }

    /**
     * Generate a byte array of random data.
     * @param size number of bytes in array.
     * @return byte array of random data.
     */
    public static byte[] generateBuffer(int size){
        byte[] buffer = new byte[size];
        for(int i = 0; i < buffer.length; i++){
            buffer[i] =  (byte) (Math.random()*125.0 + 1.0);
        }
        return buffer;
    }

    /**
     * Print out given byte array.
     * @param array array to print out.
     */
    public static void print(byte[] array){
        StringBuilder str = new StringBuilder();
        int wrap = 20;
        for(int i = 0; i < array.length; i++){
            str.append(String.format("%3X ", array[i]));
            if((i+1)%wrap==0) str.append("\n");
        }
        System.out.println(str.toString());
    }


    /**
     * Test the writing speed of compressed, random data to a file.
     * @throws IOException for problems writing file
     * @throws HipoException for problems writing file
     */
    public static void testStreamRecord() throws IOException, HipoException {

        // Variables to track record build rate
        double freqAvg;
        long t1, t2, deltaT, totalC=0;
        // Ignore the first N values found for freq in order
        // to get better avg statistics. Since the JIT compiler in java
        // takes some time to analyze & compile code, freq may initially be low.
        long ignore = 10000;
        long loops  = 2000000;

        // Create file
        Writer writer = new Writer();
        writer.setCompressionType(CompressionType.RECORD_COMPRESSION_LZ4);
        writer.open("./exampleFile.v6.evio");


        byte[] buffer = TestWriter.generateBuffer(400);

        t1 = System.currentTimeMillis();

        while (true) {
            // random data array
            writer.addEvent(buffer);

//System.out.println(""+ (20000000 - loops));
            // Ignore beginning loops to remove JIT compile time
            if (ignore-- > 0) {
                t1 = System.currentTimeMillis();
            }
            else {
                totalC++;
            }

            if (--loops < 1) break;
        }

        t2 = System.currentTimeMillis();
        deltaT = t2 - t1; // millisec
        freqAvg = (double) totalC / deltaT * 1000;

        System.out.println("Time = " + deltaT + " msec,  Hz = " + freqAvg);
        
        System.out.println("Finished all loops, count = " + totalC);

//        // Create our own record
//        RecordOutputStream myRecord = new RecordOutputStream(writer.getByteOrder());
//        buffer = TestWriter.generateBuffer(200);
//        myRecord.addEvent(buffer);
//        myRecord.addEvent(buffer);
//        writer.writeRecord(myRecord);

//        writer.addTrailer(true);
//        writer.addTrailerWithIndex(true);
        writer.close();

        System.out.println("Finished writing file");


    }

    /**
     * Compare the writing of identical data to both Writer and WriterMT
     * to see if resulting files are identical.
     *
     * @throws IOException for problems writing file
     * @throws HipoException for problems writing file
     */
    public static void testWriterVsWriterMT() throws IOException, HipoException {

        // Variables to track record build rate
        double freqAvg;
        long t1, t2, deltaT, totalC=0;
        // Ignore the first N values found for freq in order
        // to get better avg statistics. Since the JIT compiler in java
        // takes some time to analyze & compile code, freq may initially be low.
        long ignore = 0;
        long loops  = 200000;

        // Create 1 file with Writer
        Writer writer = new Writer(ByteOrder.BIG_ENDIAN, 100000, 8000000);
        writer.setCompressionType(CompressionType.RECORD_COMPRESSION_LZ4);
        String file1 = "./dataFile.v6.writer";
        writer.open(file1);

        // Create 1 file with WriterMT

        WriterMT writerN = new WriterMT(ByteOrder.BIG_ENDIAN, 100000, 8000000,
                CompressionType.RECORD_COMPRESSION_LZ4, 1, 8);
        String file2 = "./dataFile.v6.writerMT";
        writerN.open(file2);

        // Buffer with random dta
        byte[] buffer = TestWriter.generateBuffer(400);

        t1 = System.currentTimeMillis();

        while (true) {
            // write data with Writer class
            writer.addEvent(buffer);

            // write data with WriterNew class
            writerN.addEvent(buffer);

            // Ignore beginning loops to remove JIT compile time
            if (ignore-- > 0) {
                t1 = System.currentTimeMillis();
            }
            else {
                totalC++;
            }

            if (--loops < 1) break;
        }

        t2 = System.currentTimeMillis();
        deltaT = t2 - t1; // millisec
        freqAvg = (double) totalC / deltaT * 1000;

        System.out.println("Time = " + deltaT + " msec,  Hz = " + freqAvg);

        System.out.println("Finished all loops, count = " + totalC);

        writer.close();
        writerN.close();

        System.out.println("Finished writing 2 files, now compare");
        
        FileInputStream fileStream1 = new FileInputStream(file1);
        FileChannel fileChannel = fileStream1.getChannel();
        long fSize = fileChannel.size();

        FileInputStream fileStream2 = new FileInputStream(file2);


        for (long l=0; l < fSize; l++) {
            if (fileStream1.read() != fileStream2.read()) {
                System.out.println("Files differ at byte = " + l);
            }
        }

    }


    /**
     * Write 3 files, all using WriterMT, comparing using 1, 2, or 3 compression threads.
     * It's up to the caller to compare the output files to each other to see if they're
     * identical.
     */
    public static void testStreamRecordMT() {

        try {
            // Variables to track record build rate
            double freqAvg;
            long t1, t2, deltaT, totalC=0;
            // Ignore the first N values found for freq in order
            // to get better avg statistics. Since the JIT compiler in java
            // takes some time to analyze & compile code, freq may initially be low.
            long ignore = 0;
            long loops  = 6;

            String fileName = "./exampleFile.v6.evio";

            // Create files
            WriterMT writer1 = new WriterMT(fileName + ".1", ByteOrder.LITTLE_ENDIAN, 0, 0,
                                            CompressionType.RECORD_COMPRESSION_LZ4, 1, 8);
            WriterMT writer2 = new WriterMT(fileName + ".2", ByteOrder.LITTLE_ENDIAN, 0, 0,
                                            CompressionType.RECORD_COMPRESSION_LZ4, 2, 8);
            WriterMT writer3 = new WriterMT(fileName + ".3", ByteOrder.LITTLE_ENDIAN, 0, 0,
                                            CompressionType.RECORD_COMPRESSION_LZ4, 3, 8);

            byte[] buffer = TestWriter.generateBuffer(400);

            t1 = System.currentTimeMillis();

            while (true) {
                // random data array
                writer1.addEvent(buffer);
                writer2.addEvent(buffer);
                writer3.addEvent(buffer);

    //System.out.println(""+ (20000000 - loops));
                // Ignore beginning loops to remove JIT compile time
                if (ignore-- > 0) {
                    t1 = System.currentTimeMillis();
                }
                else {
                    totalC++;
                }

                if (--loops < 1) break;
            }

            t2 = System.currentTimeMillis();
            deltaT = t2 - t1; // millisec
            freqAvg = (double) totalC / deltaT * 1000;

            System.out.println("Time = " + deltaT + " msec,  Hz = " + freqAvg);

            System.out.println("Finished all loops, count = " + totalC);


            writer1.addTrailer(true);
            writer1.addTrailerWithIndex(true);

            writer2.addTrailer(true);
            writer2.addTrailerWithIndex(true);

            writer3.addTrailer(true);
            writer3.addTrailerWithIndex(true);


            writer1.close();
            writer2.close();
            writer3.close();

            // Doing a diff between files shows they're identical!

            System.out.println("Finished writing files");
        }
        catch (HipoException e) {
            e.printStackTrace();
        }

    }


    /**
     * Test the RecordOutputStream by writing an array of random data to it,
     * then building the stream, then resetting it, in an infinite loop.
     */
    public static void streamRecord() {
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

    /**
     * Test writing random, compressed data to file.
     * @throws IOException problems with file I/O.
     */
    public static void writerTest() throws IOException {
        Writer writer = new Writer("compressed_file.evio",
                                   ByteOrder.BIG_ENDIAN, 0, 0);
        //byte[] array = TestWriter.generateBuffer();
        for(int i = 0; i < 340000; i++){
            byte[] array = TestWriter.generateBuffer();
            writer.addEvent(array);
        }
        writer.close();
    }

    /**
     * Test writing random, compressed data to file along with a user header.
     * @throws IOException for problems writing file
     * @throws HipoException for problems writing file
     */
    public static void writerTestWithUserHdr() throws IOException, HipoException {
       String userHeader = "Example of creating a new header file.......?";
       System.out.println("STRING LENGTH = " + userHeader.length());
       Writer writer = new Writer();
       writer.open("example_file.evio", userHeader.getBytes());
       for(int i = 0; i < 5; i++){
           byte[] array = TestWriter.generateBuffer();
           writer.addEvent(array);
       }

       writer.close();
    }

    /**
     * Example use of this class.
     * @param args unused.
     */
    public static void main(String[] args){

        testStreamRecordMT();
        //testWriterVsWriterMT();
        //testStreamRecord();
        //streamRecord();
        //writerTest();
        ///writerTestWithUserHdr();
    }
}
