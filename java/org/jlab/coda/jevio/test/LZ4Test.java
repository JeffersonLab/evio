package org.jlab.coda.jevio.test;


import org.jlab.coda.jevio.*;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Test program.
 * @author timmer
 * June 14, 2017
 */
public class LZ4Test {

    ByteBuffer buffer;

    // files for input & output
    String fileInName  = "/home/timmer/evioTestFiles/evioV4format.ev";
    String fileOutName = "/home/timmer/evioTestFiles/evioV4format.ev.lz4";

    ByteOrder order = ByteOrder.BIG_ENDIAN;


    /**
     * Method to decode the command line used to start this application.
     * @param args command line arguments
     */
    private void decodeCommandLine(String[] args) {

        // loop over all args
        for (int i = 0; i < args.length; i++) {

            if (args[i].equalsIgnoreCase("-h")) {
                usage();
                System.exit(-1);
            }
            else if (args[i].equalsIgnoreCase("-f")) {
                fileInName = args[i + 1];
                fileOutName = fileInName;
                i++;
            }
            else {
                usage();
                System.exit(-1);
            }
        }

        return;
    }


    /** Method to print out correct program command line usage. */
    private static void usage() {
        System.out.println("\nUsage:\n\n" +
            "   java LZ4Test\n" +
            "        [-f <file>]   read from <file>, output to <file>.lz4\n" +
            "        [-h]          print this help\n");
    }


    /** For WRITING a local file. */
    public static void main(String args[]) {
        new LZ4Test(args);
    }


    public LZ4Test(String args[]) {

        decodeCommandLine(args);

        try {
            EvioCompactReader reader = new EvioCompactReader(fileInName);
            EventWriter writer = new EventWriter(fileOutName);

            int evCount = reader.getEventCount();
            System.out.println("Got " + evCount + " events in file");

            ByteBuffer evBuf;

            for (int i=1; i <= evCount; i++) {
                evBuf = reader.getEventBuffer(i, true);
                System.out.println("Read event " + i + ", buf pos = " +
                evBuf.position() + ", lim = " + evBuf.limit());

                System.out.println("Write event " + i);
                writer.writeEvent(evBuf);
                
                Thread.sleep(1);
            }

            reader.close();
            writer.close();
            System.out.println("Closed all, read it back in now");


            EvioCompressedReader reader2 = new EvioCompressedReader(fileOutName);

            EvioEvent ev;
            int j=1;

            while ((ev = reader2.parseNextEvent()) != null) {
                System.out.println("Read & parsed event " + j);
                j++;
            }

            reader2.close();

            return;
        }
        catch (Exception e) {
            e.printStackTrace();
        }




    }




}
