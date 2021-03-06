package org.jlab.coda.jevio.test;

import org.jlab.coda.hipo.CompressionType;
import org.jlab.coda.jevio.*;

import java.io.File;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;

/**
 * Program for testing speed of writing to disk.
 * Writes events typical of Hall D on-line.
 *
 * @author timmer
 * Date: May 5, 2017
 */
public class FileWritingSpeedTest {

    static String filename;
    static boolean unsync;


    /**
     * Method to decode the command line used to start this application.
     * @param args command line arguments
     */
    static private void decodeCommandLine(String[] args) {

        // loop over all args
        for (int i = 0; i < args.length; i++) {

            if (args[i].equalsIgnoreCase("-h")) {
                usage();
                System.exit(-1);
            }
            else if (args[i].equalsIgnoreCase("-f")) {
                filename = args[i+1];
                i++;
            }
            else if (args[i].equalsIgnoreCase("-unsync")) {
                unsync = true;
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
             "   java BigFileWrite\n" +
             "        [-f <filename>]     name of file to write\n" +
             "        [-unsync]           use unsynchronized writer\n" +
             "        [-h]                print this help\n");
     }



    /** For WRITING a local file. */
    public static void main(String args[]) {

        decodeCommandLine(args);

        if (filename == null) {
            filename = "/scratch/timmer/fileWritingSpeedTest.ev";
        }
        File file = new File(filename);

        // Do we overwrite or append?
        EventWriter writer = null;
        EventWriterUnsync writerUnsync = null;

        try {
            // Create an event writer to write out the test events to file

            long splitBytes = 20000000000L; // 20 GB split

            if (unsync) {
                writerUnsync = new EventWriterUnsync(filename, null, null, 1, splitBytes,
                                                     4*16, 1000,
                                                     ByteOrder.BIG_ENDIAN, null,
                                                     true, false, null,
                                                     0, 0, 1, 1, CompressionType.RECORD_UNCOMPRESSED, 0, 0, 0);
            }
            else {
                writer = new EventWriter(filename, null, null, 1, splitBytes,
                                         4*16, 1000,
                                         ByteOrder.BIG_ENDIAN, null,
                                         true, false, null,
                                         0, 0, 1, 1, CompressionType.RECORD_UNCOMPRESSED, 0, 0, 0);
            }

            ByteBuffer eventBuf = createEventBuffer();
            int eventSize = eventBuf.remaining();

            System.out.println("event size = " + eventSize);


            int  skip=2, printPeriod = 5000;
            long oldVal=0L, totalT=0L, deltaCount, totalCount=0L;
            long t1, t2, deltaT, counter=0L;
            long byteCountTotal = 0L;


            t1 = t2 = System.currentTimeMillis();

            while (true) {
                if (unsync) {
                    writerUnsync.writeEvent(eventBuf);
                }
                else {
                    writer.writeEvent(eventBuf);
                }
                eventBuf.flip();
                byteCountTotal += eventSize;

                // Don't get the time each loop since it's quite expensive.
                //
                // The following is the equivalent of the mod operation
                // but is much faster (x mod 2^n == x & (2^n - 1)).
                // Thus (counter % 256) -> (255 & counter)
                if ((255 & counter++) == 0) {
                    t2 = System.currentTimeMillis();
                }
                deltaT = t2 - t1;

                if (deltaT > printPeriod) {
                    if (skip-- < 1) {
                        totalT += deltaT;
                        deltaCount = byteCountTotal - oldVal;
                        totalCount += deltaCount;

                        System.out.println("File writing: byte rate = " +
                                                   String.format("%.3g", (deltaCount*1000./deltaT) ) +
                                                   ",  avg = " +
                                                   String.format("%.3g", (totalCount*1000.)/totalT) + " Hz" );
                    }
                    else {
                        System.out.println("File writing: byte rate = " +
                                                   String.format("%.3g", ((byteCountTotal-oldVal)*1000./deltaT) ) + " Hz");
                    }
                    t1 = t2;
                    oldVal = byteCountTotal;
                }

            }

        }
        catch (Exception e) {
            e.printStackTrace();
        }
        finally {
            // All done writing
            System.out.println("FileTest, call close()");
            if (unsync) {
                writerUnsync.close();
            }
            else {
                writer.close();
            }
        }

    }


    /**
     * Create a typical Hall D type of event in its structure.
     * Has 100 rocs of 20 entangled events with 200 bytes of
     * data from each roc, 30K per event.
     * @return  typical Hall D type of event.
     */
    static private ByteBuffer createEventBuffer() {

        try {
            //CompactEventBuilder b = new CompactEventBuilder(31000, ByteOrder.BIG_ENDIAN);
            CompactEventBuilder b = new CompactEventBuilder(ByteBuffer.allocateDirect(31000));
            int EBid = 1;
            int numRocs = 100;
            int numEvents = 20;
            long startingEventNumber = 2L;

            /////////////////////////////////////////////////////////////////////
            // Top level event, from PEB, 20 entangled events
            /////////////////////////////////////////////////////////////////////
            b.openBank(0xff50, numEvents, DataType.BANK);

            /////////////////////////////////////////////////////////////////////
            // Built trigger bank w/ timestamps, run # and runt type, 100 rocs
            /////////////////////////////////////////////////////////////////////
            b.openBank(0xff27, numRocs, DataType.SEGMENT);

            // 1st segment - unsigned longs
            b.openSegment(EBid, DataType.ULONG64);

            // timestamp for each event starting at 10
            long[] t = new long[numEvents+2];
            for (int i=0; i < numEvents; i++) {
                t[i+1] = 10 + i;
            }
            // first event number
            t[0] = startingEventNumber;
            // run #3, run type 4
            t[numEvents + 1] = (3L << 32) & 4L;
            b.addLongData(t);

            b.closeStructure();

            // 2nd segment - unsigned shorts
            b.openSegment(EBid, DataType.USHORT16);

            short[] s = new short[numEvents];
            Arrays.fill(s, (short)20);  // event types = 20 for all rocs
            b.addShortData(s);

            b.closeStructure();

            // Segment for each roc - unsigned ints.
            // Each roc has one timestamp for each event (10 + i)
            int[] rocTS = new int[numEvents];
            for (int i=0; i < numEvents; i++) {
                rocTS[i] = 10 + i;
            }

            for (int i=0; i < numRocs; i++) {
                int rocId = i;
                b.openSegment(rocId, DataType.UINT32);
                b.addIntData(rocTS);
                b.closeStructure();
            }

            b.closeStructure();

            /////////////////////////////////////////////////////////////////////
            // End of trigger bank
            /////////////////////////////////////////////////////////////////////

            /////////////////////////////////////////////////////////////////////
            // One data bank for each roc
            /////////////////////////////////////////////////////////////////////

            int[] data = new int[50];     // 200 bytes
            Arrays.fill(data, 0xf0f0f0f0);
            data[0] = (int)startingEventNumber;
            data[1] = 10; // TS

            for (int i=0; i < numRocs; i++) {
                int rocId = i;
                // Data Bank
                b.openBank(rocId, numEvents, DataType.BANK);

                // Data Block Bank
                int detectorId = 10*i;
                b.openBank(detectorId, numEvents, DataType.INT32);
                b.addIntData(data);
                b.closeStructure();

                b.closeStructure();
            }

            /////////////////////////////////////////////////////////////////////
            // End of event
            /////////////////////////////////////////////////////////////////////
            b.closeAll();

            // Returned buffer is read to read
            return b.getBuffer();
        }
        catch (EvioException e) {
            e.printStackTrace();
        }

        return null;
    }


}
