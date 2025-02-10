package org.jlab.coda.jevio.test;

import org.jlab.coda.hipo.*;
import org.jlab.coda.jevio.*;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class ReadWriteTest extends TestBase {


    void writeAndReadBuffer() throws EvioException, HipoException, IOException {

        // Create Buffer
        int bufSize = 3000;
        ByteOrder order = ByteOrder.nativeOrder();
        ByteBuffer buffer = ByteBuffer.allocate(bufSize);
        buffer.order(order);

        // Possible user header data
        byte[] userHdr = new byte[10];
        for (byte i = 0; i < 10; i++) {
            userHdr[i] = (byte) (i + 16);
        }

        Writer writer = new Writer(buffer, userHdr);

        // Calling the following method makes a shared pointer out of dataArray, so don't delete
        ByteBuffer dataBuffer = ByteBuffer.allocate(20);
        for (short i = 0; i < 10; i++) {
            dataBuffer.putShort(i);
        }
        dataBuffer.flip();

        //System.out.println("Data buffer ->\n" + dataBuffer.toString());

        // Create an evio bank of ints
        ByteBuffer evioDataBuf = createEventBuilderBuffer(0, 0, order, bufSize);
        // Create node from this buffer
        EvioNode node = EvioNode.extractEventNode(evioDataBuf, null, 0, 0, 0);

//        System.out.println("add event of node,  data type = " + node.getDataTypeObj().toString() +
//                ", bytes = " + node.getTotalBytes());
        writer.addEvent(node);

        System.out.println("Wrote Buffer w/ user header");

        writer.close();

        // Get ready-to-read buffer
        buffer = writer.getBuffer();

        ByteBuffer copy  = ByteBuffer.allocate(buffer.capacity());
        ByteBuffer copy2 = ByteBuffer.allocate(buffer.capacity());
        System.arraycopy(buffer.array(), 0, copy.array(), 0, buffer.capacity());
        System.arraycopy(buffer.array(), 0, copy2.array(), 0, buffer.capacity());
        copy.limit(buffer.limit());
        copy2.limit(buffer.limit());

        System.out.println("Finished buffer ->\n" + buffer.toString());
        System.out.println("COPY1 ->\n" + copy.toString());
        System.out.println("COPY2 ->\n" + copy2.toString());

        Utilities.printBytes(buffer, 0, bufSize, "Buffer Bytes");

        //------------------------------
        //----  READER ----------------
        //------------------------------


        Reader reader = new Reader(buffer);
        System.out.println("Past Reader's constructor");
        // Compare original with copy
        boolean unchanged = true;
        int index = 0;

        // Compare original with copy
        for (int i = 0; i < buffer.capacity(); i++) {
            if (buffer.array()[i] != copy.array()[i]) {
                unchanged = false;
                index = i;
                System.out.print("Orig buffer CHANGED at byte #" + index);
                System.out.println(", 0x" + Integer.toHexString(copy.array()[i]) + " changed to 0x" +
                        Integer.toHexString(buffer.array()[i]));
                Utilities.printBytes(buffer, 0, 200, "Buffer Bytes");
                break;
            }
        }
        if (unchanged) {
            System.out.println("ORIGINAL buffer Unchanged!");
        }

        ByteBuffer usrHeadBuf = reader.readUserHeader();
        if (usrHeadBuf.remaining() < 1) {
            System.out.println("NO user header in buffer");
        }
        else {
            Utilities.printBytes(usrHeadBuf, 0, usrHeadBuf.limit(), "User Header exists! ");
        }

        int evCount = reader.getEventCount();
        System.out.println("\nRead in buffer, got " + evCount + " events");

        System.out.println("   Got dictionary = " + reader.hasDictionary());
        System.out.println("   Got first evt  = " + reader.hasFirstEvent());

        System.out.println("Print out regular events:");
        byte[] data = null;

        for (int i = 0; i < reader.getEventCount(); i++) {
            data = reader.getEvent(i);
            Utilities.printBytes(data, 0, data.length, "  Event #" + (i+1));
        }


        System.out.println("--------------------------------------------");
        System.out.println("----------      READER2       --------------");
        System.out.println("--------------------------------------------");

        ByteBuffer dataBuf = null;

        try {
            EvioCompactReader reader2 = new EvioCompactReader(copy);

            int evCount2 = reader2.getEventCount();
            System.out.println("Read in buffer, got " + evCount2 + " events");

            System.out.println("   Got dictionary = " + reader2.hasDictionary());

            // Compact reader does not deal with first events, so skip over it

            System.out.println("Print out regular events:");

            for (int i = 0; i < evCount2; i++) {
                System.out.println("scanned event #" + (i+1) + " :");
                EvioNode compactNode = reader2.getScannedEvent(i + 1);
                System.out.println("node ->\n" + compactNode);

                dataBuf = compactNode.getStructureBuffer(true);

                Utilities.printBytes(dataBuf, dataBuf.position(), dataBuf.remaining(),
                        "  Event #" + (i+1) + " at pos " + dataBuf.position());
            }
        }
        catch (EvioException e) {
            System.out.println("PROBLEM: " + e.getMessage());
        }

        for (int i=0; i < dataBuf.limit(); i++) {
            if ((data[i] != dataBuf.array()[i]) && (i > 3)) {
                unchanged = false;
                index = i;
                System.out.print("Reader different than EvioCompactReader at byte #" + index);
                System.out.println(", 0x" + Integer.toHexString(data[i]) + " changed to 0x" +
                        Integer.toHexString(dataBuf.array()[i]));
                break;
            }
        }
        if (unchanged) {
            System.out.println("EVENT same whether using Reader or EvioCompactReader!");
        }


        System.out.println("--------------------------------------------");
        System.out.println("----------      READER3       --------------");
        System.out.println("--------------------------------------------");


        byte[] dataVec = null;
        try {
            EvioReader reader3 = new EvioReader(copy2);

            ///////////////////////////////////
            // Do a parsing listener test here
            EventParser parser = reader3.getParser();

            IEvioListener myListener = new IEvioListener() {
                public void startEventParse(BaseStructure structure) {
                    System.out.println("  START parsing event = " + structure.toString());
                }

                public void endEventParse(BaseStructure structure) {
                    System.out.println("  END parsing event = " + structure.toString());
                }

                public void gotStructure(BaseStructure topStructure, IEvioStructure structure) {
                    System.out.println("  GOT struct = " + structure.toString());
                }
            };

            IEvioListener myListener2 = new IEvioListener() {
                public void startEventParse(BaseStructure structure) {
                    System.out.println("  START parsing event 2 = " + structure.toString());
                }

                public void endEventParse(BaseStructure structure) {
                    System.out.println("  END parsing event 2 = " + structure.toString());
                }

                public void gotStructure(BaseStructure topStructure, IEvioStructure structure) {
                    System.out.println("  GOT struct 2 = " + structure.toString());
                }
            };

            // Add the listener to the parser
            parser.addEvioListener(myListener);
            parser.addEvioListener(myListener2);

            IEvioFilter myFilter = new IEvioFilter() {
                public boolean accept(StructureType type, IEvioStructure struct) {
                    return true;
                }
            };

            // Add the filter to the parser
            parser.setEvioFilter(myFilter);

            // Now parse some event
            System.out.println("Run custom filter and listener, placed in reader's parser, on first event:");
            reader3.parseEvent(1);

            ///////////////////////////////////

            int evCount3 = reader3.getEventCount();
            System.out.println("Read in buffer, got " + evCount3 + " events");

            System.out.println("   Got dictionary = " + reader3.hasDictionaryXML());
            System.out.println("   Got first evt  = " + reader3.hasFirstEvent());

            System.out.println("Print out regular events:");

            for (int i = 0; i < evCount3; i++) {
                EvioEvent ev = reader3.getEvent(i + 1);
                System.out.println("ev ->\n" + ev);

                dataVec = ev.getRawBytes();
                Utilities.printBytes(dataVec, 0, dataVec.length, "  Event #" + (i+1));
            }
        }
        catch (EvioException e) {
            System.out.println("PROBLEM: " + e.getMessage());
        }

        System.out.println("COmparing data (len = " +data.length + ") with dataVec (len = " + dataVec.length + ")");
        // data has whole event, dataVec only data, so have event bank skip over header
        for (int i=0; i < dataVec.length; i++) {
            if ((data[i+8] != dataVec[i])) {
                unchanged = false;
                index = i;
                System.out.print("Reader different than EvioReader at byte #" + index);
                System.out.println(", 0x" + Integer.toHexString(data[i]) + " changed to 0x" +
                        Integer.toHexString(dataVec[i]));
                break;
            }
        }
        if (unchanged) {
            System.out.println("EVENT same whether using Reader or EvioReader!");
        }


    }



    public static void main(String args[]) {

        ReadWriteTest tester = new ReadWriteTest();

        try {
            tester.writeAndReadBuffer();
        }
        catch (Exception e) {
            e.printStackTrace();
        }
        System.out.println("----------------------------------------\n");


    }



};




