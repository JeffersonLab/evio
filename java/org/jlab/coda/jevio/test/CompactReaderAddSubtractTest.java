/*
 * Copyright (c) 2016, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 */

package org.jlab.coda.jevio.test;

import org.jlab.coda.hipo.CompressionType;
import org.jlab.coda.jevio.*;

import java.nio.*;
import java.util.ArrayList;
import java.util.Arrays;

/**
 * Take test programs out of EvioCompactReader.
 * @author timmer
 * Date: Nov 15, 2012
 */
public class CompactReaderAddSubtractTest {


    static EvioEvent createSingleEvent(int tag) {

        // Top level event
        EvioEvent event = null;

        // data
        int[] intData = new int[2];
        Arrays.fill(intData, tag);

        try {

            // Build event (bank of banks) with EventBuilder object
            EventBuilder builder = new EventBuilder(tag, DataType.BANK, 1);
            event = builder.getEvent();

                // bank of banks
                EvioBank bankBanks = new EvioBank(tag+1, DataType.BANK, 2);
                builder.addChild(event, bankBanks);

                    // bank of ints
                    EvioBank bankInts = new EvioBank(tag+2, DataType.INT32, 3);
                    bankInts.appendIntData(intData);
                    builder.addChild(bankBanks, bankInts);
    //        builder.setAllHeaderLengths();
        }
        catch (EvioException e) {
            e.printStackTrace();
        }

        return event;
    }


    static EvioEvent createComplexEvent(int tag) {

         // Top level event
         EvioEvent event = null;

         // data
        int[] intData = new int[2];
        Arrays.fill(intData, tag);

        int[] intData2 = new int[2];
        Arrays.fill(intData2, tag*10);

         try {

             // Build event (bank of banks) with EventBuilder object
             EventBuilder builder = new EventBuilder(tag, DataType.BANK, 1);
             event = builder.getEvent();

                 // bank of banks
                 EvioBank bankBanks = new EvioBank(tag+1, DataType.BANK, 2);
                 builder.addChild(event, bankBanks);

                     // bank of ints
                     EvioBank bankInts = new EvioBank(tag+2, DataType.INT32, 3);
                     bankInts.appendIntData(intData);
                     builder.addChild(bankBanks, bankInts);

             // bank of banks
                 EvioBank bankBanks2 = new EvioBank(tag+3, DataType.BANK, 4);
                 builder.addChild(event, bankBanks2);

             // bank of ints
                      EvioBank bankInts2 = new EvioBank(tag+4, DataType.INT32, 5);
                      bankInts2.appendIntData(intData2);
                      builder.addChild(bankBanks2, bankInts2);
         }
         catch (EvioException e) {
             e.printStackTrace();
         }

         return event;
     }



    static ByteBuffer createComplexBuffer() {

        // Create a buffer
        ByteBuffer myBuf = ByteBuffer.allocate(4100);

        try {
            // Create an event writer to write into "myBuf"
            // When writing a buffer, only 1 (ONE) record is used,
            // so maxEventCount (3rd arg) must be set to accommodate all events to be written!
            EventWriter writer = new EventWriter(myBuf, 4000, 2, null, 1,
                                                  CompressionType.RECORD_UNCOMPRESSED);

            EvioEvent ev1 = createComplexEvent(1);
//System.out.println("\ncreateComplexBuffer: complex event = " + ev1);
            EvioEvent ev2 = createSingleEvent(100);
//System.out.println("\ncreateComplexBuffer: simple event = " + ev2);

            // Write events to buffer
            boolean added = writer.writeEvent(ev1);
//System.out.println("\ncreateComplexBuffer: added complex event = " + added);
            added = writer.writeEvent(ev2);
//System.out.println("\ncreateComplexBuffer: added simple event = " + added);


            // All done writing
            writer.close();

            int eventsWritten = writer.getEventsWritten();
//System.out.println("\ncreateComplexBuffer: events written = " + eventsWritten + "\n\n");

            myBuf = writer.getByteBuffer();
        }
        catch (Exception e) {
            e.printStackTrace();
        }

        // Get ready to read

        return myBuf;
    }



    /** Add bank w/ addStructure() & remove it w/ removeStructure(). Compare. */
    public static void main(String args[]) {

        try {

            EvioCompactReader reader;

            // ready-to-read buf with 2 events
            ByteBuffer buf = createComplexBuffer();
            reader = new EvioCompactReader(buf);

            System.out.println("# of events = " + reader.getEventCount());

            EvioNode node1 = reader.getScannedEvent(1);
            EvioNode node2 = reader.getScannedEvent(2);

            int i=0;
            System.out.println("1st event all:");
            for (EvioNode n : node1.getAllNodes()) {
                i++;
                System.out.println("  node " + i + ": " + n);
            }

            System.out.println("\n1st event children:");
            i=0;
            ArrayList<EvioNode> kids = node1.getChildNodes();
            if (kids != null) {
                for (EvioNode n : kids) {
                    i++;
                    System.out.println("  child node " + i + ": " + n);
                }
            }

            i=0;
            System.out.println("\n2nd event all:");
            for (EvioNode n : node2.getAllNodes()) {
                i++;
                System.out.println("  node " + i + ": " + n);
            }

            System.out.println("\nBlock 1: " + node1.blockNode);
            System.out.println("Block 2: " + node2.blockNode + "\n");

            System.out.println("node 1 has all-node-count = " + node1.getAllNodes().size());

            // Add bank of ints to node 1
            int data1[] = {
                    0x00000002,
                    0x00060b06,
                    0x1,
            };
            ByteBuffer addBuf = ByteBuffer.wrap(ByteDataTransformer.toBytes(data1, ByteOrder.BIG_ENDIAN));
            ByteBuffer origBuf = reader.getByteBuffer();
            System.out.println("  origBuf = " + origBuf.toString());
            Utilities.printBuffer(origBuf, 0, origBuf.limit()/4, "ORIG  BEFORE");

            //--------------------------------------------------
            // Take a look at what "should be" a modified node
            //--------------------------------------------------
            ByteBuffer newBuf = reader.addStructure(1, addBuf);
            Utilities.printBuffer(newBuf, 0, newBuf.limit()/4, "AFTER ADDED BUF");
            reader.setBuffer(newBuf);
            node1 = reader.getScannedEvent(1);

            System.out.println("1st event after:");
            i = 0;
            for (EvioNode n : node1.getAllNodes()) {
                i++;
                System.out.println("node " + i + ": " + n);
            }

            System.out.println("reader.byteBuffer = " + reader.getByteBuffer());

            if (reader.getByteBuffer() == node1.getBuffer()) {
                System.out.println("reader and node have same buffer");
            }
            else {
                System.out.println("reader and node have DIFFERENT buffer");
            }

            System.out.println("\n\nTry removing 2nd event");

            int nodeKidCount = node1.getChildCount();
            System.out.println("node1 has " + nodeKidCount + " kids");
            EvioNode kidToRemove = node1.getChildAt(nodeKidCount-1);
            System.out.println("node to remove = " + kidToRemove);


            // Remove last child node of first event (the one we just added above)
            ByteBuffer removedBuf = reader.removeStructure(kidToRemove);
            Utilities.printBuffer(removedBuf, 0, removedBuf.limit()/4, "REMOVED BUFFER");
            // Reread new buffer which should be same as original
            EvioCompactReader reader2 = new EvioCompactReader(removedBuf);

            node1 = reader2.getScannedEvent(1);
            node2 = reader2.getScannedEvent(2);

            i=0;
            System.out.println("1st event all:");
            for (EvioNode n : node1.getAllNodes()) {
                i++;
                System.out.println("  node " + i + ": " + n);
            }

            System.out.println("\n1st event children:");
            i=0;
            kids = node1.getChildNodes();
            if (kids != null) {
                for (EvioNode n : kids) {
                    i++;
                    System.out.println("  child node " + i + ": " + n);
                }
            }

            i=0;
            System.out.println("\n2nd event all:");
            for (EvioNode n : node2.getAllNodes()) {
                i++;
                System.out.println("  node " + i + ": " + n);
            }





            // Remove 2nd event
            System.out.println("Node2 is obsolete? " + node2.isObsolete());
            removedBuf = reader2.removeStructure(node2);
            Utilities.printBuffer(removedBuf, 0, removedBuf.limit()/4, "REMOVED BUFFER");


            // Reread new buffer which should be same as original
            EvioCompactReader reader3 = new EvioCompactReader(removedBuf);
            System.out.println("New reader shows " + reader3.getEventCount() + " events");

            node1 = reader3.getScannedEvent(1);
            //   node2 = reader3.getScannedEvent(2);

            i=0;
            System.out.println("1st event all:");
            for (EvioNode n : node1.getAllNodes()) {
                i++;
                System.out.println("  node " + i + ": " + n);
            }

            System.out.println("\n1st event children:");
            i=0;
            kids = node1.getChildNodes();
            if (kids != null) {
                for (EvioNode n : kids) {
                    i++;
                    System.out.println("  child node " + i + ": " + n);
                }
            }

            System.out.println("\n\n----------------------------------------\n");


        }
        catch (Exception e) {
            e.printStackTrace();
        }

    }



}
