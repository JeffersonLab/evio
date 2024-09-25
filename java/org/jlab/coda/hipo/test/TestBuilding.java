/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package org.jlab.coda.hipo.test;

import org.jlab.coda.hipo.*;
import java.io.RandomAccessFile;

/**
 * Class used to test the speed of the RecordOutputStream.build()
 * method. Help to speed up LZ4 data compression.
 * @author timmer
 */
public class TestBuilding {


    /** Amount of real data bytes in hallDdata array. */
    static private int arrayBytes;

    /** Put extracted real data in here. */
    static private byte[] realData;


    /**
     * Method to get real data from an existing file of data previously extracted from
     * a Hall D data file.
     *
     * @return true if hall D data found and available, else false.
     */
    static boolean getRealData() {

        int num = 1;

        // First check to see if we already have some data in a file
        String filename = System.getenv("CODA");
        if (filename != null) {
            filename += "/common/bin/hallDdata" + num + ".bin";
        }
        else {
            filename = "/Users/timmer/coda/coda3/common/bin/hallDdata" + num + ".bin";
        }

        try {
            RandomAccessFile file = new RandomAccessFile(filename, "rw");
            arrayBytes = (int) file.length();
            realData = new byte[arrayBytes];
            file.read(realData, 0, arrayBytes);
            file.close();
        }
        catch (Exception e) {
            // No file to be read, so try creating our own
            System.out.println("getRealData: cannot open data file " + filename);
            return false;
        }

System.out.println("getRealData: successfully read in file " + filename);
        return true;
    }





    /**
     * Example method using RecordOutputStream class.
     */
    public static void testStreamRecord() {

        // Variables to track record build rate
        double freqAvg;
        long t1, t2, deltaT, totalC=0;
        // Ignore the first N values found for freq in order
        // to get better avg statistics. Since the JIT compiler in java
        // takes some time to analyze & compile code, freq may initially be low.
        long ignore = 100;
        long loops  = 1100;

        // Create file
        RecordOutputStream ros = new RecordOutputStream();
        ros.getHeader().setCompressionType(CompressionType.RECORD_COMPRESSION_LZ4);


        // position in realData to get data from
        int offset = 0;
        int eventLength = 10000;

        getRealData();

        // Fill object with events
        while (true) {
            // Go through real data array
            if (offset + eventLength > arrayBytes) {
                offset = 0;
            }

            boolean added = ros.addEvent(realData, offset, eventLength);
            if (!added) {
                // record output stream is full, time to build
                break;
            }

            offset += eventLength;
        }



        t1 = System.currentTimeMillis();

        do {
            ros.build();

            // Ignore beginning loops to remove JIT compile time
            if (ignore-- > 0) {
                t1 = System.currentTimeMillis();
            }
            else {
                totalC++;
            }    
        } while (--loops > 0);




        t2 = System.currentTimeMillis();
        deltaT = t2 - t1; // millisec
        freqAvg = (double) totalC / deltaT * 1000;

        System.out.println("Time = " + deltaT + " msec,  Hz = " + freqAvg);
        
        System.out.println("Finished all loops, count = " + totalC);

    }



    /**
     * Example use of this class.
     * @param args unused.
     */
    public static void main(String[] args){

        testStreamRecord();

    }
}
