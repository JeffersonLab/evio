package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;
import org.jlab.coda.jevio.EvioCompressedReader;
import org.jlab.coda.jevio.EvioEvent;


import java.io.File;

/**
 * Test program.
 * @author timmer
 * Date: Oct 7, 2010
 */
public class FileReaderTest {

    /** For reading file. */
    public static void main(String args[]) {

        int evCount;
        EvioEvent event;

        String fileV2 = "/home/timmer/evioTestFiles/evioV2format.ev";        // 2.8M
        String fileV4 = "/home/timmer/evioTestFiles/evioV4format.ev";        // 2.8M
        String fileLz4 = "/home/timmer/evioTestFiles/evioV4format.ev.lz4";   // 1.5M
        String fileLz4B = "/home/timmer/evioTestFiles/evioV4format.ev.blocks.lz4";   // 1.5M, multiple blocks
        String fileV4Big = "/home/timmer/evioTestFiles/bigFileV4.ev";         // 6G
        String fileV4RealData = "/home/timmer/evioTestFiles/hd_rawdata_002175_000_good.evio";   // 20G

        String fileName = fileLz4B;

        File fileIn = new File(fileName);
        System.out.println("FileReaderTest: Read file " + fileName + ", size: " + fileIn.length());


        try {

            boolean sequential  = true;
            boolean checkNumSeq = false;

            EvioCompressedReader fileReader;



            if (true) {

                System.out.println("FileReaderTest: Seq = " + sequential);

                fileReader = new EvioCompressedReader(fileName, checkNumSeq, sequential);

                System.out.println("has 1st ev = " +
                       ( (BlockHeaderV4) (fileReader.getFirstBlockHeader())).hasFirstEvent());

                evCount = fileReader.getEventCount();

                System.out.println("\nnum ev = " + evCount);
                System.out.println("blocks = " + fileReader.getBlockCount());
                System.out.println("dictionary = " + fileReader.getDictionaryXML() + "\n");




                for (int i=1; i <= 260; i++) {
                    event = fileReader.parseNextEvent();
                    System.out.println("  FileReaderTest: got event " + i);
                    //event = fileReader.getEvent(i);

                    if (event == null) {
                        System.out.println("\nFileReaderTest: We got a NULL event !!!");
                        return;
                    }

                    System.out.println("FileReaderTest: event size = " + event.getTotalBytes());
                }

                System.out.println("FileReaderTest: Rewind ...");
                fileReader.rewind();
            }

            if (true) {
                sequential  = false;
                System.out.println("FileReaderTest: Seq = " + sequential);

                fileReader = new EvioCompressedReader(fileName, checkNumSeq, sequential);

                for (int i=1; i <= 260; i++) {
                   // System.out.println("  FileReaderTest: try getting next event " + i);
                    //event = fileReader.parseNextEvent();
                    event = fileReader.getEvent(i);
                    System.out.println("  FileReaderTest: got event " + i);

                    if (event == null) {
                        System.out.println("\nFileReaderTest: We got a NULL event !!!");
                        return;
                    }

                   // System.out.println("FileReaderTest: event size = " + event.getTotalBytes());
                }

                System.out.println("FileReaderTest: Rewind ...");
                fileReader.rewind();
            }


        }
        catch (Exception e) {
            e.printStackTrace();
        }

        System.out.println("FileReaderTest: done");

    }



}
