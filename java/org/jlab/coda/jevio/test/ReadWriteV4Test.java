package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class ReadWriteV4Test extends TestBase {



    static EvioBank generateEvioBank(ByteOrder order, int tag, int num) {
        // Event, traditional bank of banks
        EventBuilder builder = new EventBuilder(tag, DataType.BANK, num);
        EvioEvent ev = builder.getEvent();

        // add a bank of doubles
        EvioBank bank1 = new EvioBank(22, DataType.DOUBLE64, 0);
        double[] dData = new double[1000];
        for (int i = 0; i < 1000; i++) {
            dData[i] = i + 1.;
        }

        try {
            builder.appendDoubleData(bank1, dData);
System.out.println("  generate Evio Bank, bank1 len = " + bank1.getTotalBytes());
            builder.addChild(ev, bank1);
System.out.println("  generate Evio Bank, ev len = " + ev.getTotalBytes());
        }
        catch (EvioException e) {/* never happen */}

        return ev;
    }



    void writeFile(String filename, int tag, int num) {

        ByteOrder order = ByteOrder.LITTLE_ENDIAN;

        // Create a "first event"
        int[] firstEventData = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        EventBuilder builder = new EventBuilder(1, DataType.UINT32, 2);
        EvioEvent firstEv = builder.getEvent();
        try {
            firstEv.appendIntData(firstEventData);
        }
        catch (EvioException e) {/* never happen */}

        // Create files
        String directory = null;
        String runType = null;
        int runNum = 1;
        long split = 0;
        boolean overWriteOK = true;
        boolean append = false;
        int bufSize = 10000;

        try {

            EventWriterV4 writer = new EventWriterV4 (
                    filename, directory, runType, runNum, split,
                    EventWriterUnsyncV4.DEFAULT_BLOCK_SIZE,
                    EventWriterUnsyncV4.DEFAULT_BLOCK_COUNT,
                    bufSize, order, dictionary,
                    null, overWriteOK, append, firstEv);

//    public EventWriterUnsyncV4(String baseName, String directory, String runType,
//            int runNumber, long split,
//            int blockSizeMax, int blockCountMax, int bufferSize,
//            ByteOrder byteOrder, String xmlDictionary,
//                    BitSet bitInfo, boolean overWriteOK, boolean append,
//            EvioBank firstEvent)
//
//            int streamId = 0, int splitNumber = 0,
//            int splitIncrement= 1, int streamCount = 1)


            // Create an event with lots of stuff in it
            ByteBuffer evioDataBuf = createCompactEventBuffer(tag, num, order, bufSize, null);

            // Create node from this buffer
            EvioNode node = EvioNode.extractEventNode(evioDataBuf, null, 0, 0, 0);

            // Create EvioBank
            EvioBank bank = generateEvioBank(order, tag, num);

            // write as buffer
            writer.writeEvent(evioDataBuf, false);
System.out.println("  Wrote evio buffer, len = " + evioDataBuf.limit());
            // write as node
            writer.writeEvent(node, false);
System.out.println("  Wrote evio node, total bytes = " + node.getTotalBytes());
            // write as EvioBank
            writer.writeEvent(bank);
System.out.println("  Wrote evio bank, total bytes = " + bank.getTotalBytes());
System.out.println("  Wrote evio bank, header len in bytes = " + (4*(bank.getHeader().getLength() + 1)));

            writer.close();
System.out.println("Finished writing file " + writer.getCurrentFilename() + ", now read it in");
        }
        catch (EvioException | IOException e) {
            e.printStackTrace();
        }

    }


    static void readFile(String finalFilename) throws IOException, EvioException {
        EvioReader reader = new EvioReader(finalFilename);
        ByteOrder order = reader.getByteOrder();

        System.out.println("Read in file " + finalFilename + " of byte order " + order);
        int evCount = reader.getEventCount();
        System.out.println("Got " + evCount + " events");

        String dict = reader.getDictionaryXML();
        if (dict == null) {
            System.out.println("\nNo dictionary");
        }
        else {
            System.out.println("\nGot dictionary:\n" + dict);
        }

        int feBytes;
        EvioEvent pFE = reader.getFirstEvent();
        if (pFE != null) {
            System.out.println("\nGot first Event:\n" + pFE.toString());
        }
        else {
            System.out.println("\nNo first event");
        }

        System.out.println("\nPrint out regular events:");

        for (int i=0; i < evCount; i++) {
            EvioEvent ev = reader.getEvent(i+1);
            System.out.println("\nEvent" + (i+1) + ":\n" + ev.toString());
        }
    }



    void writeAndReadBuffer(int tag, int num) throws IOException {

        ByteOrder order = ByteOrder.LITTLE_ENDIAN;
        ByteBuffer buffer = ByteBuffer.allocate(200000);
        buffer.order(order);

        // Create a "first event"
        int[] firstEventData = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        EventBuilder builder = new EventBuilder(1, DataType.INT32, 2);
        EvioEvent firstEv = builder.getEvent();
        try {
            firstEv.appendIntData(firstEventData);
        }
        catch (EvioException e) {/* never happen */}

        boolean append = false;

        try {
            EventWriterUnsyncV4 writer = new EventWriterUnsyncV4(
                    buffer,
                    EventWriterUnsyncV4.DEFAULT_BLOCK_SIZE,
                    EventWriterUnsyncV4.DEFAULT_BLOCK_COUNT,
                    dictionary,null, 0,
                    1, append, firstEv);


            // Create an event with lots of stuff in it
            ByteBuffer evioDataBuf = createCompactEventBuffer(tag, num, order, bufSize, null);
            // Create same event as EvioEvent object
            EvioEvent evioEv = createEventBuilderEvent(tag, num);
            // Create node from this buffer
            EvioNode node = EvioNode.extractEventNode(evioDataBuf, null, 0, 0, 0);


            // write as buffer
            writer.writeEvent(evioDataBuf, false);
            // write as EvioEvent
            writer.writeEvent(evioEv, false);
            // write as node
            writer.writeEvent(node, false);

            writer.close();
            // Get ready-to-read buffer
            buffer = writer.getByteBuffer();

        }
        catch (EvioException | IOException e) {
            e.printStackTrace();
        }


        ByteBuffer copy = ByteBuffer.allocate(buffer.capacity());
        ByteBuffer copy2 = ByteBuffer.allocate(buffer.capacity());

        System.arraycopy(buffer.array(), 0, copy.array(), 0, buffer.capacity());
        System.arraycopy(buffer.array(), 0, copy2.array(), 0, buffer.capacity());

        copy.limit(buffer.limit());
        copy2.limit(buffer.limit());


        // Compare original with copy


        System.out.println("--------------------------------------------");
        System.out.println("----------  EvioCompactReader  -------------");
        System.out.println("--------------------------------------------");

        ByteBuffer dataBuf0 = null;

        try {
            EvioCompactReader reader1 = new EvioCompactReader(copy);

            int evCount2 = reader1.getEventCount();
            System.out.println("   Got " + evCount2 + " events");

            String dict2 = reader1.getDictionaryXML();
            System.out.println("   Got dictionary = \n" + dict2);

            // Compact reader does not deal with first events, so skip over it

            System.out.println("\n   Print out events (includes first event if evio version 4) :");

            for (int i = 0; i < reader1.getEventCount(); i++) {
                System.out.println("      scanned event #" + (i+1) + " :");
                EvioNode compactNode = reader1.getScannedEvent(i + 1);
                System.out.println("      node ->\n         " + compactNode);

                ByteBuffer dataBuf = compactNode.getStructureBuffer(true);

                if (i==0) {
                    dataBuf0 = dataBuf;

                    Utilities.printBytes(dataBuf, dataBuf.position(), dataBuf.remaining(),"      Event #" + (i+1));
                }
            }
        }
        catch (EvioException e) {
            System.out.println("PROBLEM: " + e.getMessage());
        }



        System.out.println("--------------------------------------------");
        System.out.println("----------     EvioReader     --------------");
        System.out.println("--------------------------------------------");


        boolean unchanged = true;
        int index = 0;

        byte[] dataVec = null;
        byte[] dataVec0 = null;

        try {
            EvioReader reader2 = new EvioReader(copy2);


            int evCount3 = reader2.getEventCount();
            System.out.println("   Got " + evCount3 + " events");

            String dict3 = reader2.getDictionaryXML();
            System.out.println("   Got dictionary = \n" + dict3);

            System.out.println("\n   Got first event = " + reader2.hasFirstEvent());

            System.out.println("\n   Print out events (includes first event if evio version 4) :");

            for (int i = 0; i < evCount3; i++) {
                EvioEvent ev = reader2.parseEvent(i + 1);
                System.out.println("      got & parsed ev " + (i+1));
                System.out.println("      event ->\n" + ev);

                dataVec = ev.getRawBytes();
                if (i==0) {
                    dataVec0 = dataVec;
                }
            }
        }
        catch (EvioException e) {
            System.out.println("PROBLEM: " + e.getMessage());
        }

        System.out.println("   Comparing buffer data (lim = " + dataBuf0.limit() + ") with vector data (len = " + dataVec0.length + ")");
        for (int i=0; i < dataVec0.length; i++) {
            if ((/*data[i+8]*/ dataBuf0.array()[i+8] != dataVec0[i]) && (i > 3)) {
                unchanged = false;
                index = i;
                System.out.print("      Compact reader different than EvioReader at byte #" + index);
                System.out.println(", 0x" + Integer.toHexString(dataBuf0.array()[i+8]) + " changed to 0x" +
                        Integer.toHexString(dataVec0[i]));
                break;
            }
        }
        if (unchanged) {
            System.out.println("First data EVENT same whether using EvioCompactReader or EvioReader!");
        }


    }




    public static void main(String args[]) {

        try {

            String filename_c = "./evioTest.c.evio";
            String filename_j = "./evioTest.java.evio";

            ReadWriteV4Test tester = new ReadWriteV4Test();

            //tester.writeFile(filename_j, 1, 1);
            tester.readFile(filename_j);

//            File cFile = new File(filename_c);
//            if (cFile.exists()) {
//                tester.readFile(filename_c);
//            }

            //tester.writeAndReadBuffer(1, 1);
        }
        catch (Exception e) {
            e.printStackTrace();
        }
        System.out.println("\n\n----------------------------------------\n");


    }



};




