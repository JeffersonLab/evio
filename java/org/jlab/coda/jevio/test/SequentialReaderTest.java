package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.util.Arrays;
import java.util.List;

/**
 * Test program.
 * @author timmer
 * Date: Apr 14, 2015
 */
public class SequentialReaderTest {

    /** For testing only */
    public static void main(String args[]) {

        int counter = 1;

        String fileName  = "/home/timmer/evioTestFiles/hd_rawdata_002175_000_good.evio";
        File fileIn = new File(fileName);
        System.out.println("read ev file: " + fileName + " size: " + fileIn.length());

        try {
            // Read sequentially
            EvioReader fileReader = new EvioReader(fileName, false, true);

            while (fileReader.parseNextEvent() != null) {
                System.out.println("read ev # " + counter++);
            }
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }


}
