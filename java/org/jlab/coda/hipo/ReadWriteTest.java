/*
 *   Copyright (c) 2017.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.hipo;


import org.jlab.coda.jevio.ByteDataTransformer;
import org.jlab.coda.jevio.EvioException;
import org.jlab.coda.jevio.EvioNode;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Class used to test behavior of the RecordSupply and RecordRingItem classes.
 * @version 6.0
 * @since 6.0 9/21/17
 * @author timmer
 */
public class ReadWriteTest {

    /**
     * Write ints.
     *
     * @param size number of INTS
     * @return byte array
     */
    static byte[] generateSequentialInts(int size, ByteOrder order) {
        try {
            int[] buffer = new int[size];
            for (int i = 0; i < size; i++) {
                buffer[i] = i;
                //buffer[i] = 1;
            }
            return ByteDataTransformer.toBytes(buffer, order);
        }
        catch (EvioException e) {
            e.printStackTrace();
        }
        return null;
    }

    /**
     * Write shorts.
     *
     * @param size  number of SHORTS
     * @param order byte order of shorts in memory
     */
    static byte[] generateSequentialShorts(int size, ByteOrder order) {
        try {
            short[] buffer = new short[size];
            for (short i = 0; i < size; i++) {
                buffer[i] = i;
                //buffer[i] = (short)1;
            }
            return ByteDataTransformer.toBytes(buffer, order);
        }
        catch (EvioException e) {
            e.printStackTrace();
        }
        return null;
    }


    static ByteBuffer generateEvioBuffer(ByteOrder order) {
        // Create an evio bank of ints
        ByteBuffer evioDataBuf = ByteBuffer.allocate(20).order(order); // 5 ints
        evioDataBuf.putInt(0, 4);  // length in words
        evioDataBuf.putInt(4, 0xffd10100);  // 2nd evio header word   (prestart event)
        evioDataBuf.putInt(8, 0x1234);  // time
        evioDataBuf.putInt(12, 0x5);  // run #
        evioDataBuf.putInt(16, 0x6);  // run type
        return evioDataBuf;
    }


    static void writeFile(String finalFilename) {

        try {

            // Variables to track record build rate
            double freqAvg;
            long totalC = 0;
            long loops = 3;

            // Create files
            String dictionary = "This is a dictionary";
            //dictionary = "";
            boolean addTrailerIndex = true;
            //CompressionType compType = CompressionType.RECORD_COMPRESSION_GZIP;
            CompressionType compType = CompressionType.RECORD_UNCOMPRESSED;

            byte firstEvent[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
            int firstEventLen = 10;
            ByteOrder order = ByteOrder.LITTLE_ENDIAN;

            Writer writer = new Writer(HeaderType.EVIO_FILE, order, 0, 0,
                                       dictionary, firstEvent, compType,
                                       addTrailerIndex);

            byte[] userHdr = new byte[10];
            for (byte i = 0; i < 10; i++) {
                userHdr[i] = i;
            }

            writer.open(finalFilename, userHdr);
            //writer.open(finalFilename);
            System.out.println("Past creating writer");

            //byte[] buffer = generateSequentialInts(100, order);
            byte[] dataArray = generateSequentialShorts(13, order);
            ByteBuffer dataBuffer = ByteBuffer.wrap(dataArray).order(order);

            // Create an evio bank of ints
            ByteBuffer evioDataBuf = generateEvioBuffer(order);
            // Create node from this buffer
            EvioNode node = EvioNode.extractEventNode(evioDataBuf, null, 0, 0, 0);

            long t1 = System.currentTimeMillis();

            while (true) {
                // random data array
                //writer.addEvent(dataArray, 0, 26);
                writer.addEvent(dataBuffer);

                //cout << int(20000000 - loops) << endl;
                totalC++;

                if (--loops < 1) break;
            }

            writer.addEvent(node);

            long t2 = System.currentTimeMillis();
            long deltaT = (t2 - t1);

            freqAvg = (double) totalC / deltaT * 1000;

            System.out.println("Time = " + deltaT + " msec,  Hz = " + freqAvg);
            System.out.println("Finished all loops, count = " + totalC);

            //------------------------------
            // Add entire record at once
            //------------------------------

            RecordOutputStream recOut = new RecordOutputStream();
            recOut.addEvent(dataArray, 0, 26);
            writer.writeRecord(recOut);


            //        writer1.addTrailer(true);
            //        writer1.addTrailerWithIndex(true);
            //        cout << "Past write 1" << endl;

            //      writer.addTrailer(true);
            //      writer.addTrailerWithIndex(true);

            //        writer3.addTrailer(true);
            //        writer3.addTrailerWithIndex(true);

            //        cout << "Past write 3" << endl;

            //        writer1.close();
            //        cout << "Past close 1" << endl;
            writer.close();
            System.out.println("Past close 2");
            //        writer3.close();

            // Doing a diff between files shows they're identical!

            System.out.println("Finished writing files " + finalFilename + ", now read it in");
        }
        catch(Exception e) {
            e.printStackTrace();
        }
    }



    static void writeFileMT(String finalFilename) {

        try {
            // Variables to track record build rate
            double freqAvg;
            long totalC = 0;
            long loops = 3;

            // Create files
            String dictionary = "This is a dictionary";
            //dictionary = "";
            boolean addTrailerIndex = true;
            //CompressionType compType = CompressionType.RECORD_COMPRESSION_GZIP;
            CompressionType compType = CompressionType.RECORD_UNCOMPRESSED;

            byte firstEvent[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
            int firstEventLen = 10;
            ByteOrder order = ByteOrder.LITTLE_ENDIAN;
            WriterMT writer = new WriterMT(HeaderType.EVIO_FILE, order, 0, 0,
                                           dictionary, firstEvent, firstEventLen,
                                           compType, 2, addTrailerIndex, 16);

            byte[] userHdr = new byte[10];
            for (byte i = 0; i < 10; i++) {
                userHdr[i] = i;
            }

            writer.open(finalFilename, userHdr);
            //writer.open(finalFilename);
            System.out.println("Past creating writer");

            //byte[] buffer = generateSequentialInts(100, order);
            byte[] dataArray = generateSequentialShorts(13, order);
            ByteBuffer dataBuffer = ByteBuffer.wrap(dataArray).order(order);

            // Create an evio bank of ints
            ByteBuffer evioDataBuf = generateEvioBuffer(order);
            // Create node from this buffer
            EvioNode node = EvioNode.extractEventNode(evioDataBuf, null, 0, 0, 0);

            long t1 = System.currentTimeMillis();

            while (true) {
                // random data array
                //writer.addEvent(dataArray, 0, 26);
                writer.addEvent(dataBuffer);

                //cout << int(20000000 - loops) << endl;
                totalC++;

                if (--loops < 1) break;
            }

            writer.addEvent(node);

            long t2 = System.currentTimeMillis();
            long deltaT = (t2 - t1);

            freqAvg = (double) totalC / deltaT * 1000;

            System.out.println("Time = " + deltaT + " msec,  Hz = " + freqAvg);
            System.out.println("Finished all loops, count = " + totalC);

            //------------------------------
            // Add entire record at once
            //------------------------------

            RecordOutputStream recOut = new RecordOutputStream();
            recOut.addEvent(dataArray, 0, 26);
            writer.writeRecord(recOut);

            writer.close();
            System.out.println("Past close ");

            // Doing a diff between files shows they're identical!

            System.out.println("Finished writing files " + finalFilename + ", now read it in");
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }

    static void readFile(String finalFilename) {

        try {
            Reader reader = new Reader(finalFilename);
            ByteOrder order = reader.getByteOrder();

            int evCount = reader.getEventCount();
            System.out.println("Read in file " + finalFilename + ", got " + evCount + " events");

            String dict = reader.getDictionary();
            System.out.println("   Got dictionary = " + dict);

            byte[] pFE = reader.getFirstEvent();
            if (pFE != null) {
                int feBytes = pFE.length;
                System.out.println("   First Event bytes = " + feBytes);
                System.out.print("   First Event values = \n   ");
                for (int i = 0; i < feBytes; i++) {
                    System.out.print(pFE[i] + ",  ");
                }
                System.out.println();
            }

            byte[] data = reader.getEvent(0);

            if (data != null) {
//                int[] intData = ByteDataTransformer.toIntArray(data, order);
//                System.out.print("   Event #0, values =\n   ");
//                int pos = 0;
//                for (int i: intData) {
//                    System.out.print(i + ",  ");
//                    if ((++pos)%5 == 0) System.out.println();
//                }


                short[] sData = ByteDataTransformer.toShortArray(data, order);
                System.out.print("   Event #0, values =\n   ");
                int pos = 0;
                for (short i: sData) {
                    System.out.print(i + ",  ");
                    if ((++pos)%5 == 0) System.out.println();
                }

                System.out.println();
            }

        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }


    /**
     * Main
     * @param args args
     */
    public static void main(String[] args){

//        String filename   = "/dev/shm/hipoTest.evio";
//        String filenameMT = "/dev/shm/hipoTest.evio";
        String filename   = "/dev/shm/hipoTest-j.evio";
        String filenameMT = "/dev/shm/hipoTestMT-j.evio";

        // Write files
        writeFile(filename);
        writeFileMT(filenameMT);

        // Now read the written files
        readFile(filename);
        System.out.println("\n\n----------------------------------------\n\n");
        readFile(filenameMT);
    }

}
