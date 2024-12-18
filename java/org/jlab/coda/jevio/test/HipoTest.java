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

public class HipoTest extends TestBase {


    /** Writing to a buffer using CompactEventBuilder interface. */
    void testCompactEventCreation(int tag, int num) throws EvioException, IOException, HipoException {

        try {
            // Create ByteBuffer with EvioEvent in it
            buffer = createCompactEventBuffer(tag, num, ByteOrder.nativeOrder(), 200000, null);

            Utilities.printBytes(buffer, 0, buffer.limit(), "BUFFER BYTES");
            System.out.println("\nBuffer -> \n" + buffer.toString());

            //
            // Write file
            //

            //WriterMT writer = new WriterMT();

//            WriterMT writer = new WriterMT(order, 1000, 1000000,
//                    CompressionType.RECORD_COMPRESSION_LZ4_BEST,
//                    3, 4);

            WriterMT writer = new WriterMT(HeaderType.EVIO_FILE, order, 1000, 1000000,
                    dictionary, buffer.array(), buffer.limit(),
                    CompressionType.RECORD_COMPRESSION_LZ4_BEST,
                    3, false, 4);

//
//            Writer writer = new Writer(HeaderType.EVIO_FILE, ByteOrder.nativeOrder(),
//                    0, 0, dictionary, buffer.array(), buffer.limit(),
//                    CompressionType.RECORD_COMPRESSION_LZ4_BEST, false);

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
            System.out.println("next evio event ->\n" + ev.treeToString(""));

            byte[] bytes2 = reader.getEvent(0);
            System.out.println("get event(0), size = " + bytes2.length);

            bytes2 = reader.getEvent(1);
            System.out.println("get event(1), size = " + bytes2.length);

            bytes2 = reader.getEvent(2);
            System.out.println("get event(2), size = " + bytes2.length);

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



    public static void main(String args[]) {
        try {
            HipoTest tester = new HipoTest();
            tester.testCompactEventCreation(1,1);
            tester.testTreeEventCreation(1,1);
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }



}