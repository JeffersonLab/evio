package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.io.IOException;
import java.nio.*;
import java.util.Arrays;
import java.util.List;

/**
 * Take test programs out of EvioCompactReader.
 * @author timmer
 * Date: Nov 15, 2012
 */
public class CompactStructureHandlerTest {


    static void printBytes(ByteBuffer buf) {
        for (int i=buf.position(); i < buf.capacity(); i++) {
            System.out.println("byte = " + buf.get(i));
        }
        System.out.println("\n");
    }



    static EvioEvent createSingleEvent(int tag) {

        // Top level event
        EvioEvent event = null;

        // data
        int[] intData1 = new int[1];
        Arrays.fill(intData1, 7);

        int[] intData2 = new int[1];
        Arrays.fill(intData2, 8);

        int[] intData3 = new int[1];
        Arrays.fill(intData3, 9);

        int[] intData4 = new int[1];
        Arrays.fill(intData3, 10);

        try {

            // Build event (bank of banks) with EventBuilder object
            EventBuilder builder = new EventBuilder(tag, DataType.BANK, 1);
            event = builder.getEvent();

            // bank of ints
            EvioBank bankInts = new EvioBank(tag+1, DataType.INT32, 2);
            builder.setIntData(bankInts, intData1);
            builder.addChild(event, bankInts);

            // bank of banks
            EvioBank bankBanks = new EvioBank(tag+2, DataType.BANK, 3);
            builder.addChild(event, bankBanks);

                // bank of ints
                EvioBank bankInts2 = new EvioBank(tag+19, DataType.INT32, 20);
                builder.setIntData(bankInts2, intData2);
                builder.addChild(bankBanks, bankInts2);

            // bank of ints
            EvioBank bankInts3 = new EvioBank(tag+3, DataType.INT32, 4);
            builder.setIntData(bankInts3, intData3);
            builder.addChild(event, bankInts3);

            // bank of ints
            EvioBank bankInts4 = new EvioBank(tag+5, DataType.INT32, 5);
            builder.setIntData(bankInts4, intData4);
            builder.addChild(event, bankInts4);

        }
        catch (EvioException e) {
            e.printStackTrace();
        }

        return event;
    }



    static ByteBuffer createAddBuffer(int tag, int num) {

        ByteBuffer bb = null;
        try {
            CompactEventBuilder builder = new CompactEventBuilder(4*5, ByteOrder.LITTLE_ENDIAN);
            builder.openBank(tag, num, DataType.BANK);
            builder.openBank(tag+1, num+1, DataType.INT32);
            builder.addIntData(new int[] {6});
            builder.closeAll();
            bb = builder.getBuffer();
        }
        catch (EvioException e) {
            e.printStackTrace();
        }

        return bb;
     }



    /** For creating a local evio buffer, put into EvioCompactStructureHandler,
     *  remove a node, then examine resulting buffer. */
    public static void main(String args[]) {

        try {
            int tag = 1;
            int num = 1;

            ByteBuffer buf = ByteBuffer.allocate(1024);
            buf.order(ByteOrder.LITTLE_ENDIAN);
            EvioEvent event = createSingleEvent(tag);

            // Evio 6 format (buf cleared (pos -> 0, lim->cap) before writing.
            // Note: header = 14 int + 1 int / event (index) = 4*(14 + 1) bytes = 60 bytes.
            // For evio 6, position just past header = 60 (1 event in buf)
            EventWriter writer = new EventWriter(buf);
            writer.writeEvent(event);
            writer.close();

            ByteBuffer finishedBuf = writer.getByteBuffer();

            Utilities.printBytes(finishedBuf, 0, finishedBuf.limit(), "Finished Buffer");

            buf.limit(60 + 4*16).position(60);
            Utilities.printBytes(buf, 60, 4*16, "Full Event");

            EvioCompactStructureHandler handler =
                    new EvioCompactStructureHandler(buf, DataType.BANK);
            List<EvioNode> list = handler.getNodes();

            // Remove last node
            handler.removeStructure(list.get(list.size() - 1));

            // Add buffer containing event
            ByteBuffer bb = createAddBuffer(tag+4,tag+4);
            ByteBuffer newBuf = handler.addStructure(bb);

            Utilities.printBytes(bb, 0, bb.limit(), "New event");


            // Search event for tag = 2, num = 2
            List<EvioNode> foundList = handler.searchStructure(2, 2);
            for (EvioNode n: foundList) {
                // Look at each node (structure)
                System.out.println("Found struct with tag = 2, num = 2");
                Utilities.printBytes(n, n.getTotalBytes(), "Found node");
            }


            list = handler.getNodes();
            System.out.println("Got " + list.size() + " nodes after everything");
            int i=0;
            for (EvioNode n : list) {
                // Look at each node (structure)
                ByteBuffer dat = handler.getStructureBuffer(list.get(i));
                Utilities.printBytes(dat, dat.position(), dat.limit(), "Struct for node " + (i + 1));

                // Look at data for each node (structure)
                ByteBuffer dd = handler.getData(list.get(i));
                Utilities.printBytes(dd, dd.position(), dd.limit(), "Data for node " + (i + 1));
                i++;
            }
        }
        catch (EvioException e) {
            e.printStackTrace();
        }
        catch (IOException e) {
            e.printStackTrace();
        }



    }




}
