package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.io.File;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Random;

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

            long splitBytes = 2000000000L; // 2GB split

            if (unsync) {
                writerUnsync = new EventWriterUnsync(file.getPath(), null, null, 0,
                                                     splitBytes, ByteOrder.BIG_ENDIAN, null);
            }
            else {
                writer = new EventWriter(file.getPath(), null, null, 0,
                                         splitBytes, ByteOrder.BIG_ENDIAN, null);
            }

//            // Read in 1000 real HallD events and place into ByteBufferSupply
//
//            // File containing events (20G)
//            String dataFilename = "/home/timmer/evioTestFiles/hallD/hd_rawdata_031347_000.evio";
//
//            EvioCompactReader reader = new EvioCompactReader(dataFilename);
//
//            int evNumber = 1;
//            EvioNode node = reader.getEvent(evNumber);
//            ByteBuffer buf = node.getStructureBuffer(true);


            int numEvents = 1000;
            ByteBuffer[] eventBufs = new ByteBuffer[numEvents];
            for (int i=0; i < numEvents; i++) {
                eventBufs[i] = createEventBuffer();
            }
            int eventSize = eventBufs[0].remaining();

            System.out.println("event size = " + eventSize);


            int  index=0, skip=2, printPeriod = 5000;
            long oldVal=0L, totalT=0L, deltaCount, totalCount=0L;
            long t1, t2, deltaT, counter=0L;
            long byteCountTotal = 0L;
            ByteBuffer eventBuf;

            t1 = t2 = System.currentTimeMillis();

            while (true) {
                eventBuf = eventBufs[index];
                index = (index + 1) % numEvents;

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

            Random random = new Random(System.currentTimeMillis());
            long firstTimestamp = random.nextInt(123456789);
            long startingEventNumber = random.nextInt(2000000000);

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

            // timestamp for each event starting at 123456789
            long[] t = new long[numEvents+2];
            for (int i=0; i < numEvents; i++) {
                t[i+1] = firstTimestamp + 10*i;
            }
            // first event number
            t[0] = startingEventNumber;
            // run #3, run type 4
            t[numEvents + 1] = (3333L << 32) & 444L;
            b.addLongData(t);

            b.closeStructure();

            // 2nd segment - unsigned shorts - event types
            b.openSegment(EBid, DataType.USHORT16);

            short[] s = new short[numEvents];
            for (int i=0; i < numEvents; i++) {
                s[i] = (short) random.nextInt(32768);
            }
            b.addShortData(s);

            b.closeStructure();

            // Segment for each roc - unsigned ints.
            // Each roc has one timestamp for each event
            int[] rocTS = new int[numEvents];
            for (int i=0; i < numEvents; i++) {
                rocTS[i] = (int)firstTimestamp + 10*i;
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

            int[] data = new int[50]; // 200 bytes
            data[0] = (int)startingEventNumber;
            data[1] = (int)firstTimestamp; // TS

            for (int i=0; i < numRocs; i++) {
                int rocId = i;
                // Data Bank
                b.openBank(rocId, numEvents, DataType.BANK);

                // Data Block Bank
                int detectorId = 10*i;
                b.openBank(detectorId, numEvents, DataType.INT32);

                // change data for each roc
                for (int j=2; j < 50; j++) {
                    data[j] = random.nextInt(2147483647);
                }

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
