package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.nio.*;
import java.util.Arrays;
import java.util.List;

/**
 * Take test programs out of EvioCompactReader.
 * @author timmer
 * Date: Nov 15, 2012
 */
public class CompactTest {


    static EvioEvent createSimpleSingleEvent(int tag) {

        // Top level event
        EvioEvent event = null;

        // data
        int[] intData = new int[21];
        Arrays.fill(intData, 1);

        try {
            // Build event (bank of banks) with EventBuilder object
            EventBuilder builder = new EventBuilder(tag, DataType.BANK, tag);
            event = builder.getEvent();

            // bank of ints
            EvioBank bankInts = new EvioBank(tag+1, DataType.INT32, tag+1);
            bankInts.appendIntData(intData);
            builder.addChild(event, bankInts);

        }
        catch (EvioException e) {
            e.printStackTrace();
        }

        return event;

    }


    static EvioEvent createSingleEvent(int tag) {

        // Top level event
        EvioEvent event = null;

        // data  (1500 bytes)
        int[] intData = new int[1];
        Arrays.fill(intData, 1);

        byte[] byteData = new byte[4];
        Arrays.fill(byteData, (byte)2);

        short[] shortData = new short[2];
        Arrays.fill(shortData, (short)3);

        double[] doubleData = new double[1];
        Arrays.fill(doubleData, 4.);

        try {

            // Build event (bank of segs) with EventBuilder object
            EventBuilder builder = new EventBuilder(tag, DataType.SEGMENT, tag);
            event = builder.getEvent();

            // segment of shorts
            EvioSegment segShorts = new EvioSegment(8, DataType.SHORT16);
            segShorts.appendShortData(shortData);
            builder.addChild(event, segShorts);


            // segment of doubles
            EvioSegment segDoubles = new EvioSegment(9, DataType.DOUBLE64);
            segDoubles.appendDoubleData(doubleData);
            builder.addChild(event, segDoubles);

        }
        catch (EvioException e) {
            e.printStackTrace();
        }

        return event;

    }





    static ByteBuffer createSingleSegment(int tag) {

        EvioSegment seg = new EvioSegment(tag, DataType.INT32);

        int[] intData = new int[10];
        Arrays.fill(intData, 456);

        try {
            seg.appendIntData(intData);
        }
        catch (EvioException e) {
            e.printStackTrace();
        }

        ByteBuffer bb = ByteBuffer.allocate(4*12);
        seg.write(bb);
        bb.flip();

        return bb;
    }



    static ByteBuffer createSingleBank(int tag, int num) {

         EvioEvent bank = createSimpleSingleEvent(tag);
         int byteSize = bank.getTotalBytes();

         ByteBuffer bb = ByteBuffer.allocate(byteSize);
         bank.write(bb);
         bb.flip();

         return bb;
     }


    static ByteBuffer createBuffer(int eventCount) {

        String xmlDict = null;

        // Create a buffer
        ByteBuffer myBuf = ByteBuffer.allocate(1000 * eventCount);

        try {
            // Create an event writer to write into "myBuf" with 10000 ints or 100 events/block max
            EventWriter writer = new EventWriter(myBuf, 10000, 100, xmlDict, null);

            EvioEvent ev = createSimpleSingleEvent(1);

            for (int i=0; i < eventCount; i++) {
                // Write event to buffer
                writer.writeEvent(ev);
            }

            writer.close();
        }
        catch (Exception e) {
            e.printStackTrace();
        }

        // Get ready to read
        myBuf.flip();

        return myBuf;
    }




    /** For WRITING a local file. */
    public static void main(String args[]) {

        // Create buffer with 1 events
        int count = 1, evCount = 0, loops = 1, adds = 1;

        // Create buffer containing bank ONLY to add dynamically to
        ByteBuffer bankBuf = createSingleBank(66, 99);
        ByteBuffer addBuf  = createSingleBank(77, 88);

        // keep track of time
        long t1, t2, time;
        double rate;

        t1 = System.currentTimeMillis();

        // Doing things the new way with EvioCompactReader
        for (int i=0; i < loops; i++) {
            try {
                EvioCompactStructureHandler handler =
                        new EvioCompactStructureHandler(bankBuf, DataType.BANK);

                List<EvioNode> nodes = handler.getNodes();

                System.out.println("Original struct has " + bankBuf.remaining() + " bytes");
                System.out.println("Original struct has " + nodes.size() + " nodes");
                ByteBuffer newBuf = null;

                for (int j=0; j < adds; j++) {
System.out.println("Insert buffer of " + addBuf.remaining() + " bytes");
                    newBuf = handler.addStructure(addBuf);
                    nodes = handler.getNodes();
                }

                System.out.println("New struct has " + nodes.size() + " nodes");


                System.out.println("bank buf pos = " + addBuf.position() +
                                    ", limit = " + addBuf.limit());

                System.out.println("new buf pos = " + newBuf.position() +
                                    ", limit = " + newBuf.limit());

                EvioCompactStructureHandler handler2 =
                        new EvioCompactStructureHandler(newBuf, DataType.BANK);
                System.out.println("Reconstructed struct has " + newBuf.remaining() + " bytes");
                nodes = handler2.getNodes();
                System.out.println("Reconstructed struct has " + nodes.size() + " nodes");
            }
            catch (EvioException e) {
                e.printStackTrace();
                System.exit(-1);
            }

        }

        // calculate the event rate
        t2 = System.currentTimeMillis();
        time = t2 - t1;
        rate = 1000.0 * ((double) loops*evCount) / time;
        System.out.println("rate = " + String.format("%.3g", rate) + " Hz");
        System.out.println("time = " + (time) + " milliseconds");


    }


}
