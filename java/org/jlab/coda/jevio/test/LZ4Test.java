package org.jlab.coda.jevio.test;


import org.jlab.coda.jevio.*;

import java.io.File;
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
    String fileOutName = "/home/timmer/evioTestFiles/evioV4format.ev.blocks.lz4";

    ByteOrder order = ByteOrder.BIG_ENDIAN;

    String xmlDict =
            "<xmlDict>"  +
                    "<xmldumpDictEntry name='a' tag= '1' num = '1' />"  +
                    "<xmldumpDictEntry name='b' tag= '2' num = '2' />"  +
            "</xmlDict>";

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


            // Build first event
            EventBuilder builder = new EventBuilder(100, DataType.INT32, 100);
            EvioEvent eventFirst = builder.getEvent();

            // bank of ints
            int[] intFirstData = new int[6];
            for (int i=0; i < intFirstData.length; i++) {
                intFirstData[i] = 0x100*(i+1);
            }
            eventFirst.appendIntData(intFirstData);


            EvioCompactReader reader = new EvioCompactReader(fileInName);
            //EventWriter writer = new EventWriter(fileOutName);
            File ff = new File(fileOutName);

            // Orig file has 998 events. Limiting each block to 100 events
            // forces it to write multiple blocks - good for testing.
//            EventWriter writer = new EventWriter(ff, 4194304,
//                                                100, ByteOrder.LITTLE_ENDIAN,
//                                                xmlDict, null);

            EventWriter writer = new  EventWriter(fileOutName, null, null,
                                                  1, 0L, 4194304,
                                                  100, 0,
                                                   ByteOrder.LITTLE_ENDIAN, xmlDict,
                                                  null, true, false,
                                                  true, true,
                                                   eventFirst, 0);


            System.out.println("Test int <-> byte conversion:");

//            writer.eventSizes = new int[] {125, 15000, 2000000, 200000000, 400000000, 122, 100};
//            writer.eventSizesCount = 7;
//            writer.toByteSizes();
//            Utilities.printBuffer(ByteBuffer.wrap(writer.blockIndexes), 0, 12, "bytes");
//            int[] sizesReclaimed = writer.toIntegerSizes(writer.blockIndexes, writer.blockIndexBytes);
//            for (int i=0; i < 7; i++) {
//                System.out.println("size reclaimed = " + sizesReclaimed[i]);
//            }
//
//            System.out.println("blockIndexBytes = " + writer.blockIndexBytes +
//            ", eventSizesCount = " + writer.eventSizesCount);

//            for (int i=0; i < 20; i++) {
//                System.out.println("i = " + i + ", result = " + writer.getEventLengthsArraySize(i));
//            }

            int evCount = reader.getEventCount();
            int eventsUsed = evCount > 8 ? 8 : evCount;
            System.out.println("Got " + evCount + " events in file, but only look at " + eventsUsed);




            ByteBuffer evBuf;

            for (int i=1; i <= eventsUsed; i++) {
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

            int j=1;

            while ((reader2.parseNextEvent()) != null) {
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
