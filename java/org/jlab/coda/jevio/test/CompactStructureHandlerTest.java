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

        try {

            // Build event (bank of banks) with EventBuilder object
            EventBuilder builder = new EventBuilder(tag, DataType.BANK, 1);
            event = builder.getEvent();

            // bank of ints
            EvioBank bankInts = new EvioBank(tag+1, DataType.INT32, 2);
            bankInts.appendIntData(intData1);
            builder.addChild(event, bankInts);

            // bank of banks
            EvioBank bankBanks = new EvioBank(tag+2, DataType.BANK, 3);
            builder.addChild(event, bankBanks);

                // bank of ints
                EvioBank bankInts2 = new EvioBank(tag+19, DataType.INT32, 20);
                bankInts2.appendIntData(intData2);
                builder.addChild(bankBanks, bankInts2);

            // bank of ints
            EvioBank bankInts3 = new EvioBank(tag+3, DataType.INT32, 4);
            bankInts3.appendIntData(intData3);
            builder.addChild(event, bankInts3);

        }
        catch (EvioException e) {
            e.printStackTrace();
        }

        return event;

    }



    static ByteBuffer createBuffer(int tag, int num) {

         EvioEvent bank = createSingleEvent(tag);
         int byteSize = bank.getTotalBytes();

         ByteBuffer bb = ByteBuffer.allocate(byteSize);
         bank.write(bb);
         bb.flip();

         return bb;
    }


    static ByteBuffer createAddBuffer(int tag, int num) {

        ByteBuffer bb = null;
        try {
            CompactEventBuilder builder = new CompactEventBuilder(4*5, ByteOrder.BIG_ENDIAN);
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
            ByteBuffer buf = ByteBuffer.allocate(1024);
            EvioEvent event = createSingleEvent(1);
            EventWriter writer = new EventWriter(buf);
            writer.writeEvent(event);

            System.out.println("After writer, buf pos = " + buf.position() +
                               ", lim = " + buf.limit());

            // Position buffer to after block header
            buf.limit(32 + 4*13).position(32);
            Utilities.printBuffer(buf, 0, 29, "EVENT");

            EvioCompactStructureHandler handler =
                    new EvioCompactStructureHandler(buf, DataType.BANK);
            List<EvioNode> list = handler.getNodes();

            // Remove  node
            handler.removeStructure(list.get(4));

            // buffer to add
            ByteBuffer bb = createAddBuffer(6,6);
            ByteBuffer newBuf = handler.addStructure(bb);

            Utilities.printBuffer(newBuf, 0, 29, "New event");

            list = handler.getNodes();
            System.out.println("Got " + list.size() + " nodes after everything");
            int i=0;
            for (EvioNode n : list) {
                // Look at data for 3rd node
                ByteBuffer dd = handler.getStructureBuffer(list.get(i));
                Utilities.printBuffer(dd, 0, 29, "Struct buf for node " + (++i));
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
