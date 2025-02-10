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

  //               builder.setAllHeaderLengths();

         }
         catch (EvioException e) {
             e.printStackTrace();
         }

         return event;
     }



    static ByteBuffer createComplexBuffer() {

        // Create a buffer
        ByteBuffer myBuf = ByteBuffer.allocate(4000);

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
Utilities.printBuffer(origBuf, 0, origBuf.limit()/4, "ORIG  BEFORE");

            //--------------------------------------------------
            // Take a look at what "should be" a modified node
            //--------------------------------------------------
            ByteBuffer newBuf = reader.addStructure(1, addBuf);
            reader.setBuffer(newBuf);
            node1 = reader.getScannedEvent(1);
            node2 = reader.getScannedEvent(2);

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


            // Remove 2nd event
            System.out.println("Node2 is obsolete? " + node2.isObsolete());
            ByteBuffer removedBuf = reader.removeStructure(node2);
            Utilities.printBuffer(removedBuf, 0, removedBuf.limit()/4, "REMOVED BUFFER");


            // Reread new buffer which should be same as original
            EvioCompactReader reader2 = new EvioCompactReader(removedBuf);
            System.out.println("New reader shows " + reader2.getEventCount() + " events");

            node1 = reader2.getScannedEvent(1);
         //   node2 = reader2.getScannedEvent(2);

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



        }
        catch (Exception e) {
            e.printStackTrace();
        }

    }



    /** Add bank w/ addStructure() & remove it w/ removeStructure(). Compare. */
    public static void main1(String args[]) {

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
            Utilities.printBuffer(origBuf, 0, origBuf.limit()/4, "ORIG  BEFORE");

            //--------------------------------------------------
            // Take a look at what "should be" a modified node
            //--------------------------------------------------
            ByteBuffer newBuf = reader.addStructure(1, addBuf);
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

        }
        catch (Exception e) {
            e.printStackTrace();
        }

    }


    /** Test for addStructure() */
    public static void main2(String args[]) {

        try {

            boolean useFile = false;
            EvioCompactReader reader;
            ArrayList<EvioNode> kids;


            if (useFile) {
                reader = new EvioCompactReader("/tmp/removeTest.evio");
            }
            else {
                // ready-to-read buf with 2 events
                ByteBuffer buf = createComplexBuffer();
                reader = new EvioCompactReader(buf);
            }

            System.out.println("Using file = " + reader.isFile());
            System.out.println("# of events = " + reader.getEventCount());

            EvioNode node1 = reader.getScannedEvent(1);
            EvioNode node2 = reader.getScannedEvent(2);
            reader.toFile("/tmp/junk1");

            int i=0;
            System.out.println("1st event all:");
            for (EvioNode n : node1.getAllNodes()) {
                i++;
                System.out.println("node " + i + ": " + n);
            }

            System.out.println("1st event children:");
            i=0;
            kids = node1.getChildNodes();
            if (kids != null) {
                for (EvioNode n : kids) {
                    i++;
                    System.out.println("child node " + i + ": " + n);
                }
            }

            i=0;
            System.out.println("2nd event all:");
            for (EvioNode n : node2.getAllNodes()) {
                i++;
                System.out.println("node " + i + ": " + n);
            }


//            if (!useFile) reader.toFile("/tmp/removeTest.evio");

            System.out.println("node 1 has all-node-count = " + node1.getAllNodes().size());

            // Remove 3rd bank structure here (1st bank of ints)
//System.out.println("removing node = " + node1.getAllNodes().get(2));
//            reader.removeStructure(node1.getAllNodes().get(2));

            // The second node (tag/num = 2/2) has a child (4/4). So both get removed.
System.out.println("removing node = " + node1.getAllNodes().get(1));
            /* buf = */ reader.removeStructure(node1.getAllNodes().get(1));

//System.out.println("removing node = " + node2.getAllNodes().get(2));
//            reader.removeStructure(node2.getAllNodes().get(2));
//System.out.println("Using file (after node removal) = " + reader.isFile());

            System.out.println("Old node1 object is obsolete? = " + node1.isObsolete());
            // Get the new node resulting from a rescan in removeStructure
            node1 = reader.getScannedEvent(1);
            System.out.println("node 1 now has all-node-count = " + node1.getAllNodes().size());

            //System.out.println("\n\n ******************* removing event # 2\n");
            //ByteBuffer newBuf = reader.removeEvent(2);
            //reader.removeStructure(node2);

//            EvioCompactReader reader2 = new EvioCompactReader(newBuf);
//
//            node1 = reader2.getScannedEvent(1);
//            node2 = reader2.getScannedEvent(2);

            System.out.println("REMOVE node 1");
            ByteBuffer newBuf = reader.removeEvent(1);
            System.out.println("Re-get scanned events 1 & 2");
            node1 = reader.getScannedEvent(1);
            reader.removeStructure(node1.getChildAt(0));
            node2 = reader.getScannedEvent(2);

            reader.toFile("/tmp/junk2");

            i=0;
            if (node1 == null) {
                System.out.println("event 1 = null");
            }
            else {

                if (node1.isObsolete()) {
                    System.out.println("OBSOLETE event 1");
                }
                else {
                    System.out.println("1st event after:");
                    for (EvioNode n : node1.getAllNodes()) {
                        i++;
                        if (n.isObsolete()) {
                            System.out.println("OBSOLETE node " + i + ": " + n);
                        }
                        else {
                            System.out.println("node " + i + ": " + n);
                        }
                    }
                    System.out.println("1st event children after:");
                    i = 0;
                    kids = node1.getChildNodes();
                    if (kids != null) {
                        for (EvioNode n : kids) {
                            i++;
                            System.out.println("child node: " + i + ": " + n);
                        }
                    }
                }
            }


            i=0;
            if (node2 == null) {
                System.out.println("event 2 = null");
            }
            else {
                if (node2.isObsolete()) {
                    System.out.println("OBSOLETE event 2");
                }
                else {
                    System.out.println("2nd event after:");
                    for (EvioNode n : node2.getAllNodes()) {
                        i++;
                        System.out.println("node " + i + ": " + n);
                    }
                }
            }

            if (node1 != null) System.out.println("\nBlock 1 after: " + node1.blockNode + "\n");
            if (node2 != null) System.out.println("\nBlock 2 after: " + node2.blockNode + "\n");


            System.out.println("\n\n\nReanalyze");
            System.out.println("# of events = " + reader.getEventCount());

            // Reanalyze the buffer and compare
            EvioCompactReader reader2 = new EvioCompactReader(newBuf);

            node1 = reader2.getScannedEvent(1);
            node2 = reader2.getScannedEvent(2);

            i=0;
            if (node1 == null) {
                System.out.println("event 1 is NULL");
            }
            else if (node1.isObsolete()) {
                System.out.println("event 1 is obsolete");
            }
            else {
                System.out.println("1st event all:");
                for (EvioNode n : node1.getAllNodes()) {
                    i++;
                    System.out.println("node " + i + ": " + n);
                }

                System.out.println("1st event children:");
                i = 0;
                kids = node1.getChildNodes();
                if (kids != null) {
                    for (EvioNode n : kids) {
                        i++;
                        System.out.println("child node " + i + ": " + n);
                    }
                }
                System.out.println("\nBlock 1: " + node1.blockNode + "\n");
            }

             i=0;
            if (node2 == null) {
                System.out.println("event 2 is NULL");
            }
            else if (node2.isObsolete()) {
                System.out.println("event 2 is obsolete");
            }
            else {
                System.out.println("2nd event all:");
                for (EvioNode n : node2.getAllNodes()) {
                    i++;
                    System.out.println("node " + i + ": " + n);
                }
                System.out.println("\nBlock 2: " + node2.blockNode + "\n");
            }

            // Write to file for viewing
//            Utilities.bufferToFile("/tmp/result2.evio", newBuf, true, false);

        }
        catch (Exception e) {
            e.printStackTrace();
        }

    }



}
