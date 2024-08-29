package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.BitSet;

public class ReadWriteV4Test {



    static String  xmlDict =
            "<xmlDict>\n" +
            "  <bank name=\"HallD\"             tag=\"6-8\"  type=\"bank\" >\n" +
            "      <description format=\"New Format\" >hall_d_tag_range</description>\n" +
            "      <bank name=\"DC(%t)\"        tag=\"6\" num=\"4\" >\n" +
            "          <leaf name=\"xpos(%n)\"  tag=\"6\" num=\"5\" />\n" +
            "          <bank name=\"ypos(%n)\"  tag=\"6\" num=\"6\" />\n" +
            "      </bank >\n" +
            "      <bank name=\"TOF\"     tag=\"8\" num=\"0\" >\n" +
            "          <leaf name=\"x\"   tag=\"8\" num=\"1\" />\n" +
            "          <bank name=\"y\"   tag=\"8\" num=\"2\" />\n" +
            "      </bank >\n" +
            "      <bank name=\"BCAL\"      tag=\"7\" >\n" +
            "          <leaf name=\"x(%n)\" tag=\"7\" num=\"1-3\" />\n" +
            "      </bank >\n" +
            "  </bank >\n" +
            "  <dictEntry name=\"JUNK\" tag=\"5\" num=\"0\" />\n" +
            "  <dictEntry name=\"SEG5\" tag=\"5\" >\n" +
            "       <description format=\"Old Format\" >tag 5 description</description>\n" +
            "  </dictEntry>\n" +
            "  <bank name=\"Rangy\" tag=\"75 - 78\" >\n" +
            "      <leaf name=\"BigTag\" tag=\"76\" />\n" +
            "  </bank >\n" +
            "</xmlDict>";

//    static ByteBuffer buffer;

    static int[] int1;
    static byte[] byte1;
    static short[] short1;
    static long[] long1;
    static float[] float1;
    static double[] double1;
    static String[] string1;

    static int dataElementCount = 3;

    static void setDataSize(int elementCount) {

        int1 = new int[elementCount];
        byte1 = new byte[elementCount];
        short1 = new short[elementCount];
        long1 = new long[elementCount];
        float1 = new float[elementCount];
        double1 = new double[elementCount];
        string1 = new String[elementCount];


        for (int i = 0; i < elementCount; i++) {
            int1[i] = i + 1;
            byte1[i] = (byte) ((i + 1) % 255);
            short1[i] = (short) ((i + 1) % 16384);
            long1[i] = i + 1;
            float1[i] = (float) (i + 1);
            double1[i] = (i + 1);
            string1[i] = ("0x" + (i + 1));
        }
    }



    // Create a fake Evio Event
    static ByteBuffer generateEvioBuffer(ByteOrder order, int dataWords) {

        // Create an evio bank of banks, containing a bank of ints
        ByteBuffer evioDataBuf = ByteBuffer.allocate(16 + 4*dataWords);
        evioDataBuf.order(order);
        evioDataBuf.putInt(3+dataWords);  // event length in words

        int tag  = 0x1234;
        int type = 0x10;  // contains evio banks
        int num  = 0x12;
        int secondWord = tag << 16 | type << 8 | num;

        evioDataBuf.putInt(secondWord);  // 2nd evio header word

        // now put in a bank of ints
        evioDataBuf.putInt(1+dataWords);  // bank of ints length in words
        tag = 0x5678; type = 0x1; num = 0x56;
        secondWord = tag << 16 | type << 8 | num;
        evioDataBuf.putInt(secondWord);  // 2nd evio header word

        // Int data
        for (int i=0; i < dataWords; i++) {
            evioDataBuf.putInt(i);
        }

        evioDataBuf.flip();
        return evioDataBuf;
    }


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




    // Create a fake Evio Event
    static ByteBuffer generateEvioBuffer(ByteOrder order, int tag, int num) throws EvioException {

        setDataSize(dataElementCount);

        ByteBuffer buffer = ByteBuffer.allocate(200000);
        buffer.order(order);

        CompactEventBuilder builder = new CompactEventBuilder(buffer);

        // add top/event level bank of banks
        builder.openBank(tag, num, DataType.BANK);

        // add bank of banks
        builder.openBank(tag + 1, num + 1, DataType.BANK);

        // add bank of ints
        builder.openBank(tag + 2, num + 2, DataType.UINT32);
        builder.addIntData(int1);
        builder.closeStructure();

        // add bank of bytes
        builder.openBank(tag + 3, num + 3, DataType.UCHAR8);
        builder.addByteData(byte1);
        builder.closeStructure();

        // add bank of shorts
        builder.openBank(tag + 4, num + 4, DataType.USHORT16);
        builder.addShortData(short1);
        builder.closeStructure();

        // add bank of longs
        builder.openBank(tag + 40, num + 40, DataType.ULONG64);
        builder.addLongData(long1);
        builder.closeStructure();

        // add bank of banks
        builder.openBank(tag + 1000, num + 1000, DataType.BANK);

        // add bank of shorts
        builder.openBank(tag + 1200, num + 1200, DataType.USHORT16);
        builder.addShortData(short1);
        builder.closeStructure();

        builder.closeStructure();

        // add bank of floats
        builder.openBank(tag + 5, num + 5, DataType.FLOAT32);
        builder.addFloatData(float1);
        builder.closeStructure();

        // add bank of doubles
        builder.openBank(tag + 6, num + 6, DataType.DOUBLE64);
        builder.addDoubleData(double1);
        builder.closeStructure();

        // add bank of strings
        builder.openBank(tag + 7, num + 7, DataType.CHARSTAR8);
        builder.addStringData(string1);
        builder.closeStructure();

        // add bank of composite data
        builder.openBank(tag + 100, num + 100, DataType.COMPOSITE);

        // Now create some COMPOSITE data
        // Format to write 1 int and 1 float a total of N times
        String format = "N(I,F)";

        CompositeData.Data myData = new CompositeData.Data();
        myData.addN(2);
        myData.addInt(1);
        myData.addFloat(1.0F);
        myData.addInt(2);
        myData.addFloat(2.0F);

        // Create CompositeData object
        CompositeData[] cData = new CompositeData[1];
        cData[0] = new CompositeData(format, 1, myData,
                1, 1, order);

        // Add to bank
        builder.addCompositeData(cData);
        builder.closeStructure();

        // add bank of segs
        builder.openBank(tag + 14, num + 14, DataType.SEGMENT);

        // add seg of ints
        builder.openSegment(tag + 8, DataType.INT32);
        builder.addIntData(int1);
        builder.closeStructure();

        // add seg of bytes
        builder.openSegment(tag + 9, DataType.CHAR8);
        builder.addByteData(byte1);
        builder.closeStructure();

        // add seg of shorts
        builder.openSegment(tag + 10, DataType.SHORT16);
        builder.addShortData(short1);
        builder.closeStructure();

        // add seg of longs
        builder.openSegment(tag + 40, DataType.LONG64);
        builder.addLongData(long1);
        builder.closeStructure();

        // add seg of floats
        builder.openSegment(tag + 11, DataType.FLOAT32);
        builder.addFloatData(float1);
        builder.closeStructure();

        // add seg of doubles
        builder.openSegment(tag + 12, DataType.DOUBLE64);
        builder.addDoubleData(double1);
        builder.closeStructure();

        // add seg of strings
        builder.openSegment(tag + 13, DataType.CHARSTAR8);
        builder.addStringData(string1);
        builder.closeStructure();

        builder.closeStructure();


        // add bank of tagsegs
        builder.openBank(tag + 15, num + 15, DataType.TAGSEGMENT);

        // add tagseg of ints
        builder.openTagSegment(tag + 16, DataType.UINT32);
        builder.addIntData(int1);
        builder.closeStructure();

        // add tagseg of bytes
        builder.openTagSegment(tag + 17, DataType.UCHAR8);
        builder.addByteData(byte1);
        builder.closeStructure();

        // add tagseg of shorts
        builder.openTagSegment(tag + 18, DataType.USHORT16);
        builder.addShortData(short1);
        builder.closeStructure();

        // add tagseg of longs
        builder.openTagSegment(tag + 40, DataType.ULONG64);
        builder.addLongData(long1);
        builder.closeStructure();

        // add tagseg of floats
        builder.openTagSegment(tag + 19, DataType.FLOAT32);
        builder.addFloatData(float1);
        builder.closeStructure();

        // add tagseg of doubles
        builder.openTagSegment(tag + 20, DataType.DOUBLE64);
        builder.addDoubleData(double1);
        builder.closeStructure();

        // add tagseg of strings
        builder.openTagSegment(tag + 21, DataType.CHARSTAR8);
        builder.addStringData(string1);
        builder.closeStructure();

        builder.closeAll();

        // Make this call to set proper pos & lim
        return builder.getBuffer();
    }





    static void writeFile(String filename) {

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
        int bufSize = 0;

        try {
            EventWriterUnsyncV4 writer = new EventWriterUnsyncV4(
                    filename, directory, runType, runNum, split,
                    EventWriterUnsyncV4.DEFAULT_BLOCK_SIZE,
                    EventWriterUnsyncV4.DEFAULT_BLOCK_COUNT,
                    bufSize, order, xmlDict,
                    null, overWriteOK, append, firstEv);

//    public EventWriterUnsyncV4(String baseName, String directory, String runType,
//            int runNumber, long split,
//            int blockSizeMax, int blockCountMax, int bufferSize,
//            ByteOrder byteOrder, String xmlDictionary,
//                    BitSet bitInfo, boolean overWriteOK, boolean append,
//            EvioBank firstEvent)

//            int streamId = 0, int splitNumber = 0,
//            int splitIncrement= 1, int streamCount = 1)


            // Create an event with lots of stuff in it
            ByteBuffer evioDataBuf = generateEvioBuffer(order, 3, 4);
            // Create node from this buffer
            EvioNode node = EvioNode.extractEventNode(evioDataBuf, null, 0, 0, 0);

            // Create EvioBank
            int tag = 4567;
            int num = 123;
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

        int evCount = reader.getEventCount();
        System.out.println("Read in file " + finalFilename + ", got " + evCount + " events");

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
            System.out.println("\nFirst Event:\n" + pFE.toString());
        }

        System.out.println("Print out regular events:");
        int byteLen;

        for (int i=0; i < evCount; i++) {
            EvioEvent ev = reader.getEvent(i+1);
            System.out.println("\nEvent" + (i+1) + ":\n" + ev.toString());
        }
    }



    static void writeAndReadBuffer() throws IOException {

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
                    xmlDict,null, 0,
                    1, append, firstEv);
            
//    public EventWriterUnsyncV4(ByteBuffer buf, int blockSizeMax, int blockCountMax,
//            String xmlDictionary, BitSet bitInfo, int reserved1,
//            int blockNumber, boolean append, EvioBank firstEvent)


            // Create an event with lots of stuff in it
            ByteBuffer evioDataBuf = generateEvioBuffer(order, 3, 4);
            // Create node from this buffer
            EvioNode node = EvioNode.extractEventNode(evioDataBuf, null, 0, 0, 0);

            // Create EvioBank
            int tag = 4567;
            int num = 123;
            EvioBank bank = generateEvioBank(order, tag, num);

            // write as buffer
            writer.writeEvent(evioDataBuf, false);
            // write as node
            writer.writeEvent(node, false);
            // write as EvioBank
            writer.writeEvent(bank);

            writer.close();
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
        System.out.println("----------      READER1       --------------");
        System.out.println("--------------------------------------------");

        ByteBuffer dataBuf0 = null;

        try {
            EvioCompactReader reader1 = new EvioCompactReader(copy);

            int evCount2 = reader1.getEventCount();
            System.out.println("Read in buffer, got " + evCount2 + " events");

            String dict2 = reader1.getDictionaryXML();
            System.out.println("   Got dictionary = " + dict2);

            // Compact reader does not deal with first events, so skip over it

            System.out.println("Print out regular events:");

            for (int i = 0; i < reader1.getEventCount(); i++) {
                System.out.println("scanned event #" + i + " :");
                EvioNode compactNode = reader1.getScannedEvent(i + 1);
                System.out.println("node ->\n" + compactNode);

                ByteBuffer dataBuf = compactNode.getStructureBuffer(true);
//                        ByteBuffer buffie(4*compactNode->getDataLength());
//                        auto dataBuf = compactNode->getByteData(buffie,true);

                if (i==0) {
                    dataBuf0 = dataBuf;
                }

                Utilities.printBytes(dataBuf, dataBuf.position(), dataBuf.remaining(),
                        "  Event #" + i);
            }
        }
        catch (EvioException e) {
            System.out.println("PROBLEM: " + e.getMessage());
        }



        System.out.println("--------------------------------------------");
        System.out.println("----------      READER2       --------------");
        System.out.println("--------------------------------------------");


        boolean unchanged = true;
        int index = 0;

        byte[] dataVec = null;
        byte[] dataVec0 = null;

        try {
            EvioReader reader2 = new EvioReader(copy2);

            ///////////////////////////////////
            // Do a parsing listener test here
            EventParser parser = reader2.getParser();

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
            reader2.parseEvent(1);

            ///////////////////////////////////

            int evCount3 = reader2.getEventCount();
            System.out.println("Read in buffer, got " + evCount3 + " events");

            String dict3 = reader2.getDictionaryXML();
            System.out.println("   Got dictionary = " + dict3);

            EvioEvent pFE3 = reader2.getFirstEvent();
            if (pFE3 != null) {
                System.out.println("   First Event bytes = " + pFE3.getTotalBytes());
                System.out.println("   First Event values = \n" + "   ");
                for (int i = 0; i < pFE3.getRawBytes().length; i++) {
                    System.out.println((int) (pFE3.getRawBytes()[i]) + ",  ");
                }
                System.out.println("\n");
            }

            System.out.println("Print out regular events:");

            for (int i = 0; i < reader2.getEventCount(); i++) {
                EvioEvent ev = reader2.getEvent(i + 1);
                System.out.println("ev ->\n" + ev);

                dataVec = ev.getRawBytes();
                if (i==0) {
                    dataVec0 = dataVec;
                }
                Utilities.printBytes(dataVec, 0, dataVec.length, "  Event #" + i);
            }
        }
        catch (EvioException e) {
            System.out.println("PROBLEM: " + e.getMessage());
        }

        System.out.println("Comparing buffer data (lim = " + dataBuf0.limit() + ") with vector data (len = " + dataVec0.length + ")");
        for (int i=0; i < dataVec0.length; i++) {
            if ((/*data[i+8]*/ dataBuf0.array()[i+8] != dataVec0[i]) && (i > 3)) {
                unchanged = false;
                index = i;
                System.out.print("Reader different than EvioReader at byte #" + index);
                System.out.println(", 0x" + Integer.toHexString(dataBuf0.array()[i+8]) + " changed to 0x" +
                        Integer.toHexString(dataVec[i]));
                break;
            }
        }
        if (unchanged) {
            System.out.println("First data EVENT same whether using EvioCompactReader or EvioReader!");
        }


    }




    public static void main(String args[]) {

        try {

            //System.out.println("Dictionary len = " + xmlDict.length());
            //System.out.println("Dictionary = \n" + xmlDict);

            //String filename_c = "./evioTest.c.evio";
            String filename   = "./evioTest.java.evio";
            writeFile(filename);
            readFile(filename);

            //writeAndReadBuffer();
        }
        catch (Exception e) {
            e.printStackTrace();
        }
        System.out.println("----------------------------------------\n");


    }



};




