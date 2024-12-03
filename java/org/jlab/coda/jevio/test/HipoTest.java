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

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class HipoTest extends TestBase {


    /** Writing to a buffer using CompactEventBuilder interface. */
    void testCompactEventCreation(int tag, int num) throws EvioException, IOException, HipoException {

        try {
            // Create ByteBuffer with EvioEvent in it
            buffer = createCompactEventBuffer(tag, num, ByteOrder.nativeOrder(), 200000, null);

            Utilities.printBytes(buffer, 0, 100, "BUFFER BYTES");

            System.out.println("\nBuffer -> \n" + buffer.toString());


            //
            // Write file
            //
            Writer writer = new Writer(HeaderType.EVIO_FILE, ByteOrder.nativeOrder(),
                    0, 0, null, null,
                    CompressionType.RECORD_UNCOMPRESSED, false);

            writer.open(writeFileName1);
            writer.addEvent(buffer);
            writer.close();


            Utilities.printBytes(writeFileName1, 0, 100, "WRITTEN FILE BYTES");

            System.out.println("\n\nAfter writer closed ... ");

            // Read event back out of file
            Reader reader = new Reader(writeFileName1);

            System.out.println("createCompactEvents: have dictionary? " + reader.hasDictionary());
            String xmlDict = reader.getDictionary();
            System.out.println("createCompactEvents: read dictionary ->\n\n" + xmlDict);
            System.out.println("\ncreateCompactEvents: try getting getNextEvent");

            if (reader.getEventCount() < 1) {
                System.out.println("createCompactEvents: no data events in file");
                return;
            }

            int len;
            byte[] bytes = reader.getNextEvent();
            EvioEvent ev = EvioReader.getEvent(bytes, 0, reader.getByteOrder());
            System.out.println("createCompactEvents: next evio event ->\n" + ev.treeToString(""));

            byte[] bytes2 = reader.getEvent(0);
            System.out.println("createCompactEvents: get event(0), size = " + bytes2.length);

            ByteBuffer bb1 = ByteBuffer.allocate(20000);
            reader.getEvent(bb1, 0);
            System.out.println("createCompactEvents: event 1,  bb1 limit ->\n" + bb1.limit());


            ByteBuffer bb2 = ByteBuffer.allocate(20000);
            reader.getEvent(bb2, 0);
            System.out.println("createCompactEvents: event 1,  bb2 limit ->\n" + bb2.limit());
        }
        catch (EvioException e) {
            e.printStackTrace();
        }
    }


    /** Writing to a buffer using original evio tree interface. */
    void testTreeEventCreation(int tag, int num) throws IOException {

        try {
            // Build event (bank of banks) with EventBuilder object
            EvioEvent event = createTreeEvent(tag, num);

            System.out.println("Event:\n" + event.treeToString(""));
            System.out.println("Event Header:\n" + event.getHeader().toString());

            // Take event & write it into buffer
            System.out.println("Write event to " + writeFileName1 + " as compressed LZ4");
            EventWriter writer = new EventWriter(writeFileName1, null, "runType", 1, 0L, 0, 0,
                    ByteOrder.nativeOrder(), dictionary, true, false, null, 1, 1, 1, 1,
                    CompressionType.RECORD_COMPRESSION_LZ4, 2, 16, 0);

            writer.setFirstEvent(event);
            writer.writeEvent(event);
            writer.close();

            // Read event back out of file
            EvioReader reader = new EvioReader(writeFileName1);

            System.out.println("    createObjectEvents: have dictionary? " + reader.hasDictionaryXML());
            String xmlDict = reader.getDictionaryXML();
            System.out.println("    createObjectEvents: read dictionary ->\n\n" + xmlDict);

            System.out.println("\n    createObjectEvents: have first event? " + reader.hasFirstEvent());
            EvioEvent fe = reader.getFirstEvent();
            System.out.println("    createObjectEvents: read first event ->\n\n" + fe.treeToString(""));

            System.out.println("\n    createObjectEvents: try getting ev #1");
            EvioEvent ev = reader.parseEvent(1);
            System.out.println("    createObjectEvents: event ->\n" + ev.treeToString(""));
        }
        catch (EvioException e) {
            e.printStackTrace();
        }
    }



    public static void main(String args[]) {
        try {
            HipoTest tester = new HipoTest();
            //tester.testCompactEventCreation(1,1);
            tester.testTreeEventCreation(1,1);
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }



}