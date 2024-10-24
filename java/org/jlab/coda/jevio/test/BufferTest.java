package org.jlab.coda.jevio.test;

import org.jlab.coda.hipo.CompressionType;
import org.jlab.coda.jevio.*;

import java.nio.ByteBuffer;

/**
 * Test program.
 * @author timmer
 * Date: Oct 7, 2010
 */
public class BufferTest {


    /**
       3 block headers (first 2 have 2 extra words each, last has 1 extra word).
       First block has 2 events. Second has 3 events.
       Last is empty final block.
    */
    static int data1[] = {
        0x00000014,
        0x00000001,
        0x0000000A,
        0x00000002,
        0x00000000,
        0x00000004,
        0x00000000,
        0xc0da0100,
        0x00000003,
        0x00000002,

        0x00000004,
        0x00010101,
        0x00000001,
        0x00000001,
        0x00000001,

        0x00000004,
        0x00010101,
        0x00000002,
        0x00000002,
        0x00000002,

        0x00000019,
        0x00000002,
        0x0000000A,
        0x00000003,
        0x00000000,
        0x00000004,
        0x00000000,
        0xc0da0100,
        0x00000001,
        0x00000002,

        0x00000004,
        0x00010101,
        0x00000003,
        0x00000003,
        0x00000003,

        0x00000004,
        0x00010101,
        0x00000004,
        0x00000004,
        0x00000004,

        0x00000004,
        0x00010101,
        0x00000005,
        0x00000005,
        0x00000005,

        0x00000009,
        0x00000003,
        0x00000009,
        0x00000000,
        0x00000000,
        0x00000204,
        0x00000000,
        0xc0da0100,
        0x00000003,
    };

    /** One block header with 1 event. */
    static int data2[] = {
        0x0000000d,
        0x00000001,
        0x00000008,
        0x00000001,
        0x00000000,
        0x00000204,
        0x00000000,
        0xc0da0100,

        0x00000004,
        0x00010101,
        0x00000001,
        0x00000001,
        0x00000001
    };

    /** One block header with extra int, no events. */
    static int data3[] = {
        0x00000009,
        0x00000001,
        0x00000009,
        0x00000000,
        0x00000000,
        0x00000204,
        0x00000000,
        0xc0da0100,
        0x00000004,
    };

    /** 2 block headers, both empty & each with extra int. */
    static int data4[] = {
        0x00000009,
        0x00000001,
        0x00000009,
        0x00000000,
        0x00000000,
        0x00000004,
        0x00000000,
        0xc0da0100,
        0x00000004,

        0x00000009,
        0x00000002,
        0x00000009,
        0x00000000,
        0x00000000,
        0x00000204,
        0x00000000,
        0xc0da0100,
        0x00000004,
    };



    /** For testing only */
    public static void main(String args[]) {

        try {

            // Evio event = bank of int data
            EventBuilder eb = new EventBuilder(1, DataType.INT32, 1);
            EvioEvent ev = eb.getEvent();
            int[] dat = new int[] {1, 2, 3, 4};
            ev.appendIntData(dat);

            // create writer with max block size of 256 ints
            EventWriter evWriter = new EventWriter(ByteBuffer.allocate(64), 4*256, 20, null, 1,
                                                    CompressionType.RECORD_UNCOMPRESSED);
            evWriter.close();

            // create buffer to write to of size 274 ints (> 8 + 244 + 8 + 6 + 8)
            // which should hold 3 block headers and both events
            ByteBuffer buffer = ByteBuffer.allocate(4*274);
            buffer.clear();
            evWriter.setBuffer(buffer);

            // write first event - 244 ints long  (with block header = 252 ints)
            evWriter.writeEvent(ev);

            // this should leave room for 4 ints before writer starts a new block header -
            // not enough for the next event which will be 2 header ints + 4 data ints
            // for a total size of 6 ints
            EvioEvent ev2 = new EvioEvent(2, DataType.INT32, 2);
            int[] dat2 = new int[] {2, 2, 2, 2};
            ev2.appendIntData(dat2);

            // write second event - 6 ints long. This should force a second block header.
            evWriter.writeEvent(ev2);
            evWriter.close();

            // Lets's read what we've written
            buffer.flip();

            EvioReader reader = new EvioReader(buffer);
            long evCount = reader.getEventCount();
            System.out.println("Read buffer, got " + evCount + " events & " +
                    reader.getBlockCount() + " blocks\n");

            EvioEvent event;
            while ( (event = reader.parseNextEvent()) != null) {
                System.out.println("Event = " + event.toXML());
                System.out.println("\nevent count = " + reader.getEventCount() + "\n");
            }

        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }

    /** Create writer, close, change buffer, write event, read it. */
    public static void main1(String args[]) {

        try {
            // Evio event = bank of double data
            EventBuilder eb = new EventBuilder(1, DataType.DOUBLE64, 1);
            EvioEvent ev = eb.getEvent();
            double[] da = new double[] {1., 2., 3.};
            ev.appendDoubleData(da);

            EventWriter evWriter = new EventWriter(ByteBuffer.allocate(64), 4*550000, 200, null, 1,
                                                    CompressionType.RECORD_UNCOMPRESSED);
            evWriter.close();

            ByteBuffer buffer = ByteBuffer.allocate(4000);

            for (int j=0; j < 2; j++) {
                buffer.clear();
                evWriter.setBuffer(buffer);
                evWriter.writeEvent(ev);
                evWriter.close();
                buffer.flip();

                EvioReader reader = new EvioReader(buffer);
                long evCount = reader.getEventCount();
                System.out.println("Read buffer, got " + evCount + " number of events\n");

                EvioEvent event;
                while ( (event = reader.parseNextEvent()) != null) {
                   System.out.println("Event = " + event.toXML());
                    System.out.println("\nevent count = " + reader.getEventCount() + "\n");
                }
            }
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }


    /** Testing for padding bug in which zero values overwrite beginning of data array
     * instead of end for CHAR8 & UCHAR8. Found and fixed. */
    public static void main4(String args[]) {

        try {

            // Evio event = bank of double data
            EventBuilder eb = new EventBuilder(1, DataType.CHAR8, 1);
            EvioEvent ev = eb.getEvent();
            byte[] da = new byte[] {1,2,3,4,5,6,7};
            ev.appendByteData(da);

            ByteBuffer buffer = ByteBuffer.allocate(100);
            EventWriter evWriter = new EventWriter(ByteBuffer.allocate(64), 4*550000, 200, null, 1,
                                                   CompressionType.RECORD_UNCOMPRESSED);
            evWriter.writeEvent(ev);
            evWriter.close();

            buffer.flip();

            EvioReader reader = new EvioReader(buffer);
            long evCount = reader.getEventCount();
            System.out.println("Read file, got " + evCount + " number of events\n");

            EvioEvent event;
            while ( (event = reader.parseNextEvent()) != null) {
                System.out.println("Event = " + event.toXML());
                System.out.println("\nevent count = " + reader.getEventCount() + "\n");
            }

        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }



}
