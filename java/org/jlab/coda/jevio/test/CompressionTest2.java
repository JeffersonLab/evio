package org.jlab.coda.jevio.test;

import org.jlab.coda.hipo.Compressor;
import org.jlab.coda.hipo.HipoException;
import org.jlab.coda.jevio.*;

import java.io.File;
import java.nio.ByteBuffer;


/**
 * Test program.
 * @author timmer
 * Date: Oct 21, 2013
 */


public class CompressionTest2 {


    /** For Reading Evio version 6 data from a file. */
    public static void main(String args[]) {

        try {
            String fileName = "/tmp/test526.0";
            File file = new File(fileName);

            Utilities.printBytes(fileName, 0, 3000, fileName);
            EvioReader reader = new EvioReader(file);
            EvioEvent ev = reader.getEvent(3);
            int[] data = ev.getIntData();
            for (int i=0; i < data.length; i++) {
                System.out.println("data["+i+"] = " + data[i]);
            }
        }
        catch (Exception e) {
            e.printStackTrace();
        }


    }



    /** For WRITING-to / READING-from a buffer. */
    public static void main2(String args[]) {

        Compressor comp = Compressor.getInstance();

        // Destination array
        byte[] dest = new byte[120];
        byte[] uncomp = new byte[120];

        // Source array
        byte[] src = new byte[100];
        for (int i=0; i < src.length; i++) {
            src[i] = (byte) i;
        }

        try {
            int compressedSize = comp.compressLZ4(src, 0, 100, dest, 0, 120);

            int uncompressedSize = comp.uncompressLZ4(dest, 0, compressedSize, uncomp, 0);

            // Compare original to decompressed data
            for (int i=0; i < 100; i++) {
                if (src[i] != uncomp[i]) {
                    System.out.println("PROBLEM WITH DECOMPRESSION !!!");
                    System.exit(-1);
                }
            }
            System.out.println("DECOMPRESSION was fine using arrays");

            ByteBuffer srcBuf = ByteBuffer.wrap(src);
            ByteBuffer destBuf = ByteBuffer.wrap(dest);
            ByteBuffer uncompBuf = ByteBuffer.wrap(uncomp);

            compressedSize = comp.compressLZ4(srcBuf, 0, 100, destBuf, 0, 120);

            uncompressedSize = comp.uncompressLZ4(destBuf, 0, compressedSize, uncompBuf);

            // Compare original to decompressed data
            for (int i=0; i < 100; i++) {
                if (srcBuf.get(i) != uncompBuf.get(i)) {
                    System.out.println("PROBLEM WITH DECOMPRESSION !!!");
                    System.exit(-1);
                }
            }

            System.out.println("DECOMPRESSION was fine using buffers");

        }
        catch (HipoException e) {
            e.printStackTrace();
        }

    }

}
