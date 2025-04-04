/*
 *   Copyright (c) 2017.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.hipo.test;


import org.jlab.coda.hipo.*;
import java.nio.ByteOrder;

/**
 * Class used to test behavior of the RecordSupply and RecordRingItem classes.
 * @version 6.0
 * @since 6.0 9/21/17
 * @author timmer
 */
public class RecordSupplyTester {

    /** Supply of RecordSupplyItem objects each of which encapsulate a Record. */
    RecordSupply supply;
    /** Number of compression threads. */
    int compressThreadCount = 1;
    /** Endianness of each created record. */
    ByteOrder order = ByteOrder.LITTLE_ENDIAN;

    /** Class for getting new RecordRingItem from the RecordSupply and publishing them
     * so they're accessible to the compressor. */
    class Producer extends Thread {
        @Override
        public void run() {
            while (true) {
                RecordRingItem item = supply.get();
                //System.out.println("Producer: got item");
                //try {Thread.sleep(1500);} catch (InterruptedException e) {}
                supply.publish(item);
                //System.out.println("Producer: published item");
            }

        }
    }

    /** Class for getting an item from the supply in order to compress data. */
    class Compressor extends Thread {
        /** ID number of the thread used to compress. */
        int num;

        /**
         * Constructor.
         * @param threadNumber ID number of the thread used to compress.
         */
        Compressor(int threadNumber) {
            num = threadNumber;
        }

        @Override
        public void run() {
            try {
                while (true) {
                    //System.out.println("  Compressor " + num + ": waiting 4 item");
                    RecordRingItem item = supply.getToCompress(num);
                    Thread.sleep(3000);
                    supply.releaseCompressor(item);
                    //System.out.println("  Compressor " + num + ": released item");
                }
            }
            catch (Exception e) {
                e.printStackTrace();
            }
        }
    }

    /** Class to get an item with compressed data from
     *  the supply in order to write it somewhere. */
    class Writerr extends Thread {
        @Override
        public void run() {
            try {
                while (true) {
                    //System.out.println("    Writer: waiting 4 item");
                    RecordRingItem item = supply.getToWrite();
                    //Thread.sleep(1500);
                    supply.releaseWriter(item);
                    System.out.print(".");
                    //System.out.println("    Writer: released item");
                }
            }
            catch (Exception e) {
                e.printStackTrace();
            }
        }
    }


    /**
     * Constructor.
     * @param threadCount number of compression threads.
     */
    RecordSupplyTester(int threadCount) {

        supply = new RecordSupply(8, order, threadCount,
                                  0, 0, CompressionType.RECORD_COMPRESSION_LZ4);
        compressThreadCount = threadCount;

        Writerr writer = new Writerr();
        writer.start();

        for (int i=0; i < threadCount; i++) {
            System.out.println("Starting compressor #" + (i+1));
            Compressor comp = new Compressor(i);
            comp.start();
        }

        Producer producer = new Producer();
        producer.start();
    }


    /**
     * Example use of this class.
     * @param args unused.
     */
    public static void main(String[] args){

        RecordSupplyTester tester = new RecordSupplyTester(8);

    }

}
