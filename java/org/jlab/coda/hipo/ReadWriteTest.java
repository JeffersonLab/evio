/*
 *   Copyright (c) 2017.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.hipo;


import org.jlab.coda.jevio.ByteDataTransformer;
import org.jlab.coda.jevio.EvioException;

import java.nio.ByteOrder;

/**
 * Class used to test behavior of the RecordSupply & RecordRingItem classes.
 * @version 6.0
 * @since 6.0 9/21/17
 * @author timmer
 */
public class ReadWriteTest {

    /**
     * Write ints.
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
     * @param size number of SHORTS
     * @param order byte order of shorts in memory
     * @return
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

    static void testStreamRecordMT() {

        try {

            // Variables to track record build rate
            double freqAvg;
            long totalC = 0;
            long loops = 11;

            String fileName = "/dev/shm/hipoTest2.evio";

            // Create files
            String finalFilename = fileName + ".1";
            //        WriterMT writer1(finalFilename, ByteOrder::ENDIAN_LITTLE, 0, 0, Compressor::LZ4, 1);
            //cout << "Past creating writer1" << endl;
            finalFilename = fileName + ".2";
            //WriterMT writer2(ByteOrder::ENDIAN_LITTLE, 0, 0, Compressor::UNCOMPRESSED, 2);
            String dictionary = "This is a dictionary";
            //dictionary = "";
            byte firstEvent[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
            int firstEventLen = 10;
            ByteOrder order  =  ByteOrder.LITTLE_ENDIAN;
            WriterMT writer2 = new WriterMT(HeaderType.EVIO_FILE, order, 0, 0,
                                            dictionary, firstEvent, firstEventLen,
                                            CompressionType.RECORD_COMPRESSION_GZIP, 2, true, 16);

            byte[] userHdr = new byte[10];
            for (byte i = 0; i < 10; i++) {
                userHdr[i] = i;
            }

            //writer2.open(finalFilename, userHdr);
            writer2.open(finalFilename);
            System.out.println("Past creating writer2");
            //        finalFilename = fileName + ".3";
            //        WriterMT writer3(finalFilename, ByteOrder::ENDIAN_LITTLE, 0, 0, Compressor::LZ4, 1);
            //cout << "Past creating writer3" << endl;

            //byte[] buffer = generateSequentialInts(100, order);
            byte[] buffer = generateSequentialShorts(13, order);

            long t1 = System.currentTimeMillis();

            while (true) {
                // random data array
                //            writer1.addEvent(buffer, 0, 400);
                //writer2.addEvent(buffer, 0, 400);
                writer2.addEvent(buffer, 0, 26);
                //            writer3.addEvent(buffer, 0, 400);

                //cout << int(20000000 - loops) << endl;
                totalC++;

                if (--loops < 1) break;
            }

            long t2 = System.currentTimeMillis();
            long deltaT = (t2 - t1);

            freqAvg = (double) totalC / deltaT * 1000;

            System.out.println("Time = " + deltaT + " msec,  Hz = " + freqAvg);
            System.out.println("Finished all loops, count = " + totalC);


            //        writer1.addTrailer(true);
            //        writer1.addTrailerWithIndex(true);
            //        cout << "Past write 1" << endl;

            //      writer2.addTrailer(true);
            //      writer2.addTrailerWithIndex(true);

            //        writer3.addTrailer(true);
            //        writer3.addTrailerWithIndex(true);

            //        cout << "Past write 3" << endl;

            //        writer1.close();
            //        cout << "Past close 1" << endl;
            writer2.close();
            System.out.println("Past close 2");
            //        writer3.close();

            // Doing a diff between files shows they're identical!

            System.out.println("Finished writing files " + fileName + " + .1, .2, .3");
            System.out.println("Now read file " + fileName + " + .1, .2, .3");

            Reader reader = new Reader(finalFilename);

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
            return;
        }
    }


    public static void main(String[] args){

        testStreamRecordMT();

    }

}
