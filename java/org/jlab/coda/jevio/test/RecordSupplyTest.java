/**
 * Copyright (c) 2025, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 01/10/2025
 * @author timmer
 */

package org.jlab.coda.jevio.test;


//-------------------------------------------------------------
// Class used to test RecordSupply and RecordRingItem classes.
// These are never used by the user directly.
//-------------------------------------------------------------



/////////////////////////////////////////////////////////////////////////////////////////


import org.jlab.coda.hipo.CompressionType;
import org.jlab.coda.hipo.RecordRingItem;
import org.jlab.coda.hipo.RecordSupply;

import java.nio.ByteOrder;
import java.util.ArrayList;

public class RecordSupplyTest {

    /**
     * Class used to get compressed items, "write" them, and put them back.
     * Last barrier on ring.
     * It is an interruptable thread from the boost library, and only 1 exists.
     */
    class Writer2 extends Thread {


        /** Supply of RecordRingItems. */
        RecordSupply supply;
        /** Thread which does the file writing. */
        Thread thd;


        /**
         * Constructor.
         * @param recSupply RecordSupply object.
         */
        public Writer2(RecordSupply recSupply) {
            supply = recSupply;
            thd = this;
        }

        /** Stop the thread. */
        public void stopThread() throws InterruptedException {
            // Send signal to interrupt this writing thread (not calling thread)
            thd.interrupt();
            // Wait for it to stop
            thd.join();
        }

        /** Run this method in thread. */
        public void run() {
            try {
                while (true) {
                    // Get the next record for this thread to write
                    RecordRingItem item = supply.getToWrite();
                    System.out.println("   W : v" + item.getId());
                    supply.releaseWriterSequential(item);
                }
            }
            catch (Exception e) {
                e.printStackTrace();
            }

        }

    }


//////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Class used to take items from ring buffer, "compress" them, and place them back.
     */
    class Compressor2 extends Thread {

        /** Supply of RecordRingItems. */
        RecordSupply supply;
        /** Thread which does the file writing. */
        Thread thd;
        /** Keep track of this thread with id number. */
        int threadNumber;


        /**
         * Constructor.
         * @param threadNum thread id number.
         * @param recSupply RecordSupply object.
         */
        public Compressor2(int threadNum, RecordSupply recSupply) {
            supply = recSupply;
            threadNumber = threadNum;
            thd = this;
        }


        /** Stop the thread. */
        public void stopThread() throws InterruptedException {
            // Send signal to interrupt this compression thread (not calling thread)
            thd.interrupt();
            // Wait for it to stop
            thd.join();
        }

        /** Method to run in the thread. */
        public void run() {

            try {

                // The first time through, we need to release all records coming before
                // our first in case there are < threadNumber records before close() is called.
                // This way close() is not waiting for thread #12 to get and subsequently
                // release items 0 - 11 when there were only 5 records total.
                // (threadNumber starts at 0).
                if (threadNumber > 0) {
                    supply.release(threadNumber, threadNumber - 1);
                }

                while (true) {
                    // Get the next record for this thread to compress
                    RecordRingItem item = supply.getToCompress(threadNumber);
                    System.out.println("   C" + threadNumber + ": v" + item.getId());

                    // Release back to supply
                    supply.releaseCompressor(item);

                    Thread.sleep(2000);
                }
            }
            catch (Exception  e) {
                e.printStackTrace();
            }
        }
    }


////////////////////////////////////////////////////////////////////////////////////////////


    public void recordSupplyTest() {

        // Number of threads doing compression simultaneously
        int compressionThreadCount = 2;

        // Number of records held in this supply
        int ringSize = 32;

        // The byte order in which to write a file or buffer
        ByteOrder byteOrder = ByteOrder.LITTLE_ENDIAN;

        CompressionType compressionType = CompressionType.RECORD_UNCOMPRESSED;

        // Fast supply of record items for filling, compressing and writing
        RecordSupply supply = new RecordSupply(ringSize, byteOrder,
                compressionThreadCount,0, 0, compressionType);

        // Threads used to compress data
        ArrayList<Compressor2> compressorThreads = new ArrayList<>(compressionThreadCount);

        // Create & start compression threads
        for (int i=0; i < compressionThreadCount; i++) {
            Compressor2 c = new Compressor2(i, supply);
            compressorThreads.add(c);
            c.start();
        }

        // Thread used to write data to file/buffer.
        // Easier to use list here so we don't have to construct it immediately.
        Writer2 writerThread = new Writer2(supply);

        // Start writing thread
        writerThread.start();

        int counter = 0;

        while (true) {
            // Producer gets next available record
            RecordRingItem item = supply.get();
            item.setId(counter++);
            System.out.println("P->" + item.getId());
            supply.publish(item);
        }
    }


    public static void main(String[] args) {
        RecordSupplyTest tester = new RecordSupplyTest();
        tester.recordSupplyTest();
    }

}


