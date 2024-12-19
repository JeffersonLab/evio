//
// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


package org.jlab.coda.jevio.test;

import org.jlab.coda.hipo.*;
import org.jlab.coda.jevio.*;

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;

public class HipoTest extends TestBase {


    /** Writing to a buffer using CompactEventBuilder interface. */
    void testCompactEventCreation(int tag, int num) throws EvioException, IOException, HipoException {

        try {
            // Create ByteBuffer with EvioEvent in it
            buffer = createCompactEventBuffer(tag, num, ByteOrder.nativeOrder(), 200000, null);

            Utilities.printBytes(buffer, 0, buffer.limit(), "BUFFER BYTES");
            System.out.println("\nBuffer -> \n" + buffer.toString());

            CompressionType compressionType = CompressionType.RECORD_COMPRESSION_LZ4_BEST;
            CompressionType compressionType2 = CompressionType.RECORD_UNCOMPRESSED;

            //------------------------------
            // Create record to test writer.writeRecord(recOut);
            // This will not change position of buffer.
            //------------------------------
            RecordOutputStream recOut = new RecordOutputStream(order, 0, 0, compressionType2);
            recOut.addEvent(buffer, 0);
            //------------------------------

            //
            // Write file
            //

            //WriterMT writer = new WriterMT();

//            WriterMT writer = new WriterMT(order, 1000, 1000000, compressionType, 3, 4);

            WriterMT writer = new WriterMT(HeaderType.EVIO_FILE, order, 1000, 1000000,
                    dictionary, buffer.array(), buffer.limit(), compressionType,
                    3, false, 4);

//
//            Writer writer = new Writer(HeaderType.EVIO_FILE, ByteOrder.nativeOrder(),
//                    0, 0, dictionary, buffer.array(), buffer.limit(), compressionType, false);

            writer.open(writeFileName1, null, true);
            writer.addEvent(buffer);
            writer.close();
            File f = new File (writeFileName1);
            System.out.println("File size of " + writeFileName1 + " is " + f.length());
            Utilities.printBytes(writeFileName1, 0, 200, "WRITTEN FILE BYTES");


            writer.open(writeFileName1, null, true);
            System.out.println("\nCall open again, rewrite 3 events to file");
            writer.addEvent(buffer);
            writer.addEvent(buffer);
            writer.addEvent(buffer);
            System.out.println("add entire record");
            writer.writeRecord(recOut);
            writer.close();

            f = new File (writeFileName1);
            System.out.println("File size of " + writeFileName1 + " is now " + f.length());

            Utilities.printBytes(writeFileName1, 0, 200, "WRITTEN FILE BYTES 2");

            System.out.println("\n\nRead file ...");

            // Read event back out of file
            Reader reader = new Reader(writeFileName1);

            System.out.println("have dictionary? " + reader.hasDictionary());
            String xmlDict = reader.getDictionary();
            if (reader.hasDictionary()) {
                System.out.println("dictionary ->\n\n" + xmlDict + "\n");
            }

            System.out.println("have first event? " + reader.hasFirstEvent());
            if (reader.hasFirstEvent()) {
                byte[] firstEvt = reader.getFirstEvent();
                if (firstEvt != null) {
                    System.out.println("first event len = " + firstEvt.length);
                }
            }

            System.out.println("\ntry getting getNextEvent");
            if (reader.getEventCount() < 1) {
                System.out.println("no data events in file");
                return;
            }

            byte[] bytes = reader.getNextEvent();
            EvioEvent ev = EvioReader.getEvent(bytes, 0, reader.getByteOrder());
            if (bytes != null) {
                System.out.println("next evio event ->\n" + ev.treeToString(""));
            }

            bytes = reader.getEvent(0);
            if (bytes != null) {
                System.out.println("getEvent(0), size = " + bytes.length);
            }

            bytes = reader.getEvent(1);
            if (bytes != null) {
                System.out.println("getEvent(1), size = " + bytes.length);
            }

            bytes = reader.getEvent(2);
            if (bytes != null) {
                System.out.println("getEvent(2), size = " + bytes.length);
            }

            bytes = reader.getEvent(3);
            if (bytes != null) {
                System.out.println("getEvent(3), size = " + bytes.length);
            }

            // This event was added with reader.recordWrite()
            bytes = reader.getEvent(4);
            if (bytes != null) {
                System.out.println("getEvent(4), size = " + bytes.length);
            }

            bytes = reader.getEvent(20);
            if (bytes != null) {
                System.out.println("getEvent(20), size = " + bytes.length);
            }
            else {
                System.out.println("getEvent(20), no such event!");
            }

            ByteBuffer bb1 = ByteBuffer.allocate(20000);
            reader.getEvent(bb1, 0);
            System.out.println("event 1,  ByteBuffer limit = " + bb1.limit());
        }
        catch (EvioException e) {
            e.printStackTrace();
        }
    }


    /** Writing to a buffer using original evio tree interface. */
    void testTreeEventCreation(int tag, int num) throws IOException {

        try {
            // Build an event using original evio tree interface.
            // NOTE: doing things this way means that event.getRawBytes() returns null! */
            EvioEvent event = createTreeEvent(tag, num);

            // Create node from this event
            int size = event.getTotalBytes();
            ByteBuffer evioDataBuf = ByteBuffer.allocate(size);
            evioDataBuf.order(ByteOrder.nativeOrder());
            event.write(evioDataBuf);
            evioDataBuf.flip();

            EvioNode node = EvioNode.extractEventNode(evioDataBuf, null, 0, 0, 0);


            System.out.println("\n\nEvent (created by tree methods):\n" + event.treeToString(""));
            System.out.println("Event Header:\n" + event.getHeader().toString());

            // Take event & write it into buffer
            System.out.println("Write event to " + writeFileName2 + " as compressed LZ4");
            EventWriter writer = new EventWriter(writeFileName2, null, "runType", 1, 0L, 0, 0,
                    ByteOrder.nativeOrder(), dictionary, true, false, null, 1, 1, 1, 1,
                    CompressionType.RECORD_COMPRESSION_LZ4, 2, 16, 0);

            writer.setFirstEvent(event);
            writer.writeEvent(event);

            // Test double sync
            writer.writeEvent(node, false, false);

            System.out.println("    call writer.close()");
            writer.close();

            // Read event back out of file
            System.out.println("    create EvioReader");
            EvioReader reader = new EvioReader(writeFileName2);

            System.out.println("    have dictionary? " + reader.hasDictionaryXML());
            if (reader.hasDictionaryXML()) {
                String xmlDict = reader.getDictionaryXML();
                System.out.println("    read dictionary ->\n\n" + xmlDict);
            }

            System.out.println("\n    have first event? " + reader.hasFirstEvent());
            if (reader.hasFirstEvent()) {
                EvioEvent fe = reader.getFirstEvent();
                System.out.println("    read first event ->\n\n" + fe.treeToString(""));
            }

            System.out.println("\n    try getting ev #1");
            EvioEvent ev = reader.parseEvent(1);
            System.out.println("    event ->\n" + ev.treeToString(""));

            System.out.println("\n    try getting ev #2");
            ev = reader.parseEvent(2);
            System.out.println("    event ->\n" + ev.treeToString(""));
        }
        catch (EvioException e) {
            e.printStackTrace();
        }
    }

    /**
     * Copy ByteBuffer.
     * @param original buffer to copy
     * @return copy of original
     */
    public static ByteBuffer deepCopy(ByteBuffer original) {
        // Create a new ByteBuffer with the same capacity
        ByteBuffer copy = ByteBuffer.allocate(original.capacity());
        // Duplicate the contents (avoid changing pos/lim of original)
        ByteBuffer duplicate = original.duplicate().limit(original.capacity()).position(0);
        copy.put(duplicate);
        copy.limit(original.limit()).position(original.position());
        return copy;
    }


    void writeAndReadBuffer() {

        System.out.println();
        System.out.println();
        System.out.println("--------------------------------------------");
        System.out.println("--------------------------------------------");
        System.out.println("------------- NOW BUFFERS ------------------");
        System.out.println("--------------------------------------------");
        System.out.println("--------------------------------------------");
        System.out.println();
        System.out.println();

        // Create Buffer
        ByteOrder order = ByteOrder.nativeOrder();
        int bufSize = 3000;
        ByteBuffer buffer = ByteBuffer.allocate(bufSize);
        buffer.order(order);

        // user header data
        byte[] userHdr = new byte[10];
        for (int i = 0; i < 10; i++) {
            userHdr[i] = (byte) (i + 16);
        }

        // No compression allowed with buffers
        ByteBuffer copy = null;
        ByteBuffer copy2 = null;

        try {
            //Writer writer = new Writer(buffer, 0, 0, dictionary, userHdr, 10);
            Writer writer = new Writer(buffer, userHdr);

            // Create an evio bank of ints
            ByteBuffer evioDataBuf = createEventBuilderBuffer(0, 0, order, 200000);
            // Create node from this buffer
            EvioNode node = EvioNode.extractEventNode(evioDataBuf, null, 0, 0, 0);

            writer.addEvent(node);
            writer.close();

            // Get ready-to-read buffer
            buffer = writer.getBuffer();

            copy = deepCopy(buffer);
            copy2 = deepCopy(buffer);

            System.out.println("Finished buffer ->\n" + buffer.toString());
            System.out.println("COPY1 ->\n" + copy.toString());
            System.out.println("COPY2 ->\n" + copy2.toString());
            System.out.println("Past close, now read it");

            Utilities.printBytes(buffer, 0, buffer.limit(), "Buffer Bytes");

            System.out.println("--------------------------------------------");
            System.out.println("------------------ Reader ------------------");
            System.out.println("--------------------------------------------");
        }
        catch (Exception e) {
            e.printStackTrace();
            System.exit(1);
        }

        boolean unchanged;
        int index;
        byte[] data = null;


        try {
            Reader reader = new Reader(buffer);

            // Compare original with copy
            unchanged = true;
            index = 0;
            for (int i = 0; i < buffer.capacity(); i++) {
                if (buffer.array()[i] != copy.array()[i]) {
                    unchanged = false;
                    index = i;
                    System.out.println("Orig buffer CHANGED at byte #" + index);
                    System.out.println(", 0x" + Integer.toHexString(copy.array()[i] & 0xf) + " changed to 0x" +
                            Integer.toHexString(buffer.array()[i] & 0xf) );
                    Utilities.printBytes(buffer, 0, 200, "Buffer Bytes");
                    break;
                }
            }
            if (unchanged) {
                System.out.println("ORIGINAL buffer Unchanged!");
            }

            int evCount = reader.getEventCount();
            System.out.println("   Got " + evCount + " events");

            String dict = reader.getDictionary();
            System.out.println("   Have dictionary = " + reader.hasDictionary());

            byte[] pFE = reader.getFirstEvent();
            if (pFE != null) {
                int feBytes = pFE.length;
                System.out.println("   First Event bytes = " + feBytes);
                System.out.println("   First Event values = \n   ");
                for (int i = 0; i < feBytes; i++) {
                    System.out.println(((pFE[i]) & 0xf) + ",  ");
                }
                System.out.println();
            }

            System.out.println("   Print out regular events:");
            int byteLen;
            data = null;
            for (int i = 0; i < evCount; i++) {
                // Because this is a Reader object, it does not parse evio, it only gets a bunch of bytes.
                // For parsing evio, use EvioCompactReader or EvioReader.
                data = reader.getEvent(i);
                byteLen = data.length;
                System.out.println("      Event " + (i+1) + " len = " + byteLen + " bytes");
            }
        }
        catch (HipoException e) {
            e.printStackTrace();
            System.exit(1);
        }


        System.out.println("--------------------------------------------");
        System.out.println("------------ EvioCompactReader -------------");
        System.out.println("--------------------------------------------");

        ByteBuffer dataBuf = null;

        try {
            EvioCompactReader reader2 = new EvioCompactReader(copy);

            int evCount2 = reader2.getEventCount();
            System.out.println("   Got " + evCount2 + " events");

            String dict2 = reader2.getDictionaryXML();
            System.out.println("   Have dictionary = " + reader2.hasDictionary());

            // Compact reader does not deal with first events, so skip over it

            System.out.println("   Print out regular events:");

            for (int i = 0; i < evCount2; i++) {
                EvioNode compactNode = reader2.getScannedEvent(i + 1);

                // This node and possibly other nodes have the same underlying buffer.
                // Get this node's portion of the underlying buffer into its own buffer.
                dataBuf = compactNode.getByteData(true);
                // Get the byte order right.
                dataBuf.order(order);

                System.out.println("      Event " + (i+1) + " len = " + compactNode.getTotalBytes() + " bytes");
//                Utilities.printBytes(dataBuf, dataBuf.position(), dataBuf.remaining(),
//                        "      Event #" + (i+1) + " at pos " + dataBuf.position());
            }

            // Comparing last events together, one from reader, the other from reader2
            unchanged = true;
            for (int i=0; i < dataBuf.limit(); i++) {
                if ((data[i+8] != dataBuf.array()[i])) {
                    unchanged = false;
                    index = i;
                    System.out.println("Reader different than EvioCompactReader at byte #" + index);
                    System.out.println(", 0x" + Integer.toHexString(data[i] & 0xf) + " changed to 0x" +
                            Integer.toHexString(buffer.array()[i] & 0xf) );
                    break;
                }
            }
            if (unchanged) {
                System.out.println("\nLast event same whether using Reader or EvioCompactReader!");
            }
        }
        catch (EvioException e) {
            e.printStackTrace();
            System.exit(1);

        }


        System.out.println("--------------------------------------------");
        System.out.println("-------------- EvioReader ------------------");
        System.out.println("--------------------------------------------");

        try {
            EvioReader reader3 = new EvioReader(copy2);

            ///////////////////////////////////
            // Do a parsing listener test here
            EventParser parser = reader3.getParser();


            IEvioListener myListener1 = new IEvioListener() {
                public void startEventParse(BaseStructure structure) {
                    System.out.println("  START: parsing event 1 = " + structure.toString());
                }

                public void endEventParse(BaseStructure structure) {
                    System.out.println("  END: parsing event 1 = " + structure.toString());
                }

                public void gotStructure(BaseStructure topStructure, IEvioStructure structure) {
                    System.out.println("  GOT: struct 1 = " + structure.toString());
                }
            };

            IEvioListener myListener2 = new IEvioListener() {
                public void startEventParse(BaseStructure structure) {
                    System.out.println("  START: parsing event 2 = " + structure.toString() + "\n");
                }

                public void endEventParse(BaseStructure structure) {
                    System.out.println("  END: parsing event 2 = " + structure.toString() + "\n");
                }

                public void gotStructure(BaseStructure topStructure, IEvioStructure structure) {
                    System.out.println("  GOT: struct 2 = " + structure.toString() + "\n");
                }
            };

            // Add the listener to the parser
            parser.addEvioListener(myListener2);
            parser.addEvioListener(myListener1);

            IEvioFilter myFilter = new IEvioFilter() {
                public boolean accept(StructureType type, IEvioStructure struct) {
                    return true;
                }
            };

            // Add the filter to the parser
            parser.setEvioFilter(myFilter);

            // Now parse some event
            System.out.println("Run custom filter and listener, placed in reader's parser, on first event:\n");
            EvioEvent ev = reader3.parseEvent(1);

            ///////////////////////////////////

            int evCount3 = reader3.getEventCount();
            System.out.println("   Got " + evCount3 + " events");

            String dict3 = reader3.getDictionaryXML();
            System.out.println("   Got dictionary = " + reader3.hasDictionaryXML());

            EvioEvent fe = reader3.getFirstEvent();
            if (fe != null) {
                System.out.println("   First Event bytes = " + fe.getTotalBytes());
                System.out.println("   First Event values = \n");
                byte[] rawBytes = fe.getRawBytes();
                for (int i = 0; i < rawBytes.length; i++) {
                    System.out.println((int) (rawBytes[i]) + ",  ");
                }
                System.out.println();
            }

            // Remove all listeners & filter
            parser.removeEvioListener(myListener2);
            parser.removeEvioListener(myListener1);
            parser.setEvioFilter(null);

            byte[] dataVec = null;

            System.out.println("   Print out regular events:");
            for (int i = 0; i < evCount3; i++) {
                ev = reader3.parseEvent(i + 1);
                System.out.println("      Event " + (i+1) + " len = " + ev.getTotalBytes() + " bytes");

                dataVec = ev.getRawBytes();
//                Utilities.printBytes(dataVec, 0, dataVec.length, "  Event #" + i);
                }

            System.out.println("\nComparing data with dataVec");
            unchanged = true;
            for (int i=0; i < dataVec.length; i++) {
                if ((data[i+8] != dataVec[i]) && (i > 3)) {
                    unchanged = false;
                    index = i;
                    System.out.println("Reader different than EvioReader at byte #" + index);
                    System.out.println(", 0x" + Integer.toHexString(data[i] & 0xf) + " changed to 0x" +
                            Integer.toHexString(dataVec[i] & 0xf) );
                    break;
                }
            }
            if (unchanged) {
                System.out.println("EVENT same whether using Reader or EvioReader!\n");
            }

        }
        catch (Exception e) {
            e.printStackTrace();
            System.exit(1);
        }
    }
    

    public static void main(String args[]) {
        try {
            HipoTest tester = new HipoTest();

            // FILES
            tester.testCompactEventCreation(1,1);
            tester.testTreeEventCreation(1,1);

            // BUFFERS
            tester.writeAndReadBuffer();
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }



}