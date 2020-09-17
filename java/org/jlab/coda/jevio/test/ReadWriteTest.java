package org.jlab.coda.jevio.test;

import org.jlab.coda.hipo.*;
import org.jlab.coda.jevio.*;
import proguard.io.JarWriter;

import javax.swing.tree.DefaultMutableTreeNode;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.nio.ShortBuffer;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;

public class ReadWriteTest {



    static String  mlDict =
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
            "</xmlDict>\n";


    /**
     * Write shorts.
     * @param size number of SHORTS
     * @param order byte order of shorts in memory
     * @return
     */
    static byte[] generateSequentialShorts(int size, ByteOrder order) {
        short[] buffer = new short[size];

        for (int i = 0; i < size; i++) {
            buffer[i] = (short)i;
        }

        byte[] bArray = null;
        try {
            bArray = ByteDataTransformer.toBytes(buffer, order);
        }
        catch (EvioException e) {
            e.printStackTrace();
        }

        return bArray;
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
        int secondWord = tag << 16 | type << 4 | num;

        evioDataBuf.putInt(secondWord);  // 2nd evio header word

        // now put in a bank of ints
        evioDataBuf.putInt(1+dataWords);  // bank of ints length in words
        tag = 0x5678; type = 0x1; num = 0x56;
        secondWord = tag << 16 | type << 4 | num;
        evioDataBuf.putInt(secondWord);  // 2nd evio header word

        // Int data
        for (int i=0; i < dataWords; i++) {
            evioDataBuf.putInt(i);
        }

        evioDataBuf.flip();
        return evioDataBuf;
    }





    static String eventWriteFileMT(String filename) {

        // Variables to track record build rate
        double freqAvg;
        long totalC = 0;
        long loops = 6;


        String dictionary = null;

        byte[] firstEvent = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        int firstEventLen = 10;
        boolean addTrailerIndex = true;
        ByteOrder order = ByteOrder.BIG_ENDIAN;
        //CompressionType compType = CompressionType.RECORD_COMPRESSION_GZIP;
        CompressionType compType = CompressionType.RECORD_UNCOMPRESSED;

        // Create files
        String directory = null;
        int runNum = 123;
        long split = 0; // 2 MB
        int maxRecordSize = 0; // 0 -> use default
        int maxEventCount = 2; // 0 -> use default
        boolean overWriteOK = true;
        boolean append = true;
        int streamId = 3;
        int splitNumber = 2;
        int splitIncrement = 1;
        int streamCount = 2;
        int compThreads = 2;
        int ringSize = 16;
        int bufSize = 1;
        DefaultMutableTreeNode m;

        try {
            EventWriterUnsync writer = new EventWriterUnsync(filename, directory, "runType",
                                                             runNum, split, maxRecordSize, maxEventCount,
                                                             order, dictionary, overWriteOK, append,
                                                             null, streamId, splitNumber, splitIncrement, streamCount,
                                                             compType, compThreads, ringSize, bufSize);

//            String firstEv = "This is the first event";
//            ByteBuffer firstEvBuf = ByteBuffer.wrap(firstEv.getBytes(StandardCharsets.US_ASCII));
//            writer.setFirstEvent(firstEvBuf);

            byte[] dataArray = generateSequentialShorts(13, order);
            ByteBuffer dataBuffer = ByteBuffer.wrap(dataArray);
            //  When appending, it's possible the byte order gets switched
            order = writer.getByteOrder();

            // Create an event containing a bank of ints
            ByteBuffer evioDataBuf = generateEvioBuffer(order, 10);

            // Create node from this buffer
            EvioNode node = EvioNode.extractEventNode(evioDataBuf, null, 0, 0, 0);

            while (true) {
                System.out.println("Write event ~ ~ ~");
                // event in evio format
                writer.writeEvent(evioDataBuf);

                totalC++;

                if (--loops < 1) break;
            }

            //writer.writeEvent(node, false);

            writer.close();
            System.out.println("Past close");
            System.out.println("Finished writing file "+ writer.getCurrentFilename() + ", now read it in");

            return writer.getCurrentFilename();
        }
        catch (EvioException e) {
            e.printStackTrace();
        }
        catch (IOException e) {
            e.printStackTrace();
        }

        return null;
    }



    static void readFile(String finalFilename) {

        try {
            Reader reader1 = new Reader(finalFilename);
            ByteOrder order = reader1.getByteOrder();

            int evCount = reader1.getEventCount();
            System.out.println("Read in file " + finalFilename + ", got " + evCount + " events");

            String dict = reader1.getDictionary();
            System.out.println("   Got dictionary = " + dict);

            byte[] pFE = reader1.getFirstEvent();
            if (pFE != null) {
                int feBytes = pFE.length;
                System.out.println("   First Event bytes = " + feBytes);
                System.out.println("   First Event values = \n   ");
                for (int i = 0; i < feBytes; i++) {
                    System.out.println((int)(pFE[i]) + ",  ");
                }
                System.out.println("");
            }

            System.out.println("reader.getEvent(0)");
            byte[] data = reader1.getEvent(0);
            System.out.println("got event");

            int wordLen = data.length/2;
            if (data != null) {
                ByteBuffer dataBuf = ByteBuffer.wrap(data);
                dataBuf.order(order);
                ShortBuffer shortBuffer = dataBuf.asShortBuffer();

                System.out.println("   Event #0, values =\n   ");
                for (int i=0; i < wordLen; i++) {
                    System.out.println(shortBuffer.get(i) + ",  ");
                    if ((i+1)%5 == 0) System.out.println();
                }
            }
        }
        catch (IOException e) {
            e.printStackTrace();
        }
        catch (HipoException e) {
            e.printStackTrace();
        }

    }


    static void readFile2(String finalFilename) {

        try {
            Reader reader1 = new Reader(finalFilename);
            ByteOrder order = reader1.getByteOrder();

            int evCount = reader1.getEventCount();
            System.out.println("Read in file " + finalFilename + ", got " + evCount + " events");

            String dict = reader1.getDictionary();
            System.out.println("   Got dictionary = " + dict);

            String feString = null;
            byte[] pFE = reader1.getFirstEvent();
            if (pFE != null) {
                feString = new String(pFE);
            }

            System.out.println("First event = " + feString);




            System.out.println("reader.getEvent(0)");
            byte[] data = reader1.getEvent(0);
            System.out.println("got event");

            int wordLen = data.length/4;
            if (data != null) {
                ByteBuffer dataBuf = ByteBuffer.wrap(data);
                dataBuf.order(order);
                IntBuffer intBuffer = dataBuf.asIntBuffer();

                System.out.println("   Event #0, values =\n   ");
                for (int i=0; i < wordLen; i++) {
                    System.out.print(intBuffer.get(i) + ",  ");
                    if ((i+1)%5 == 0) System.out.println();
                }
            }
        }
        catch (IOException e) {
            e.printStackTrace();
        }
        catch (HipoException e) {
            e.printStackTrace();
        }
    }


    static ByteBuffer buffer;

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
    static ByteBuffer generateEvioBuffer(ByteOrder order, int tag, int num) throws EvioException {

        setDataSize(dataElementCount);

        buffer = ByteBuffer.allocate(200000);
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
        cData[0] = new CompositeData(format, 1, myData, 1, 1);

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


    static void writeAndReadBuffer() throws EvioException, HipoException, IOException {

        int loops = 3;

        byte firstEvent[] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29};
        int firstEventLen = 10;

        ByteOrder order = ByteOrder.BIG_ENDIAN;
        CompressionType compType = CompressionType.RECORD_UNCOMPRESSED;

        // Possible user header data
        byte[] userHdr = new byte[10];
        for (byte i = 0; i < 10; i++) {
            userHdr[i] = (byte)(i+16);
        }

        // Create Buffer
        ByteBuffer buffer = ByteBuffer.allocate(3000);
        buffer.order(order);

        //Writer writer(buffer, 0, 0, "", firstEvent, firstEventLen);
        Writer writer = new Writer(buffer, userHdr);
System.out.println("Past creating Writer object");

        // Calling the following method makes a shared pointer out of dataArray, so don't delete
        ByteBuffer dataBuffer = ByteBuffer.allocate(20);
        for (short i=0; i < 10; i++) {
            dataBuffer.putShort(i);
        }
        dataBuffer.flip();

System.out.println("Data buffer ->\n" + dataBuffer.toString());

        // Create an evio bank of ints
        ByteBuffer evioDataBuf = generateEvioBuffer(order, 0, 0);
        // Create node from this buffer
        EvioNode node = EvioNode.extractEventNode(evioDataBuf, null, 0,0,0);

            while (true) {
                // random data array
                //writer.addEvent(dataArray, 0, 20);
System.out.println("add event of len = " + dataBuffer.remaining());
                writer.addEvent(dataBuffer);

                if (--loops < 1) break;
            }

System.out.println("add event of node,  data type = " + node.getDataTypeObj().toString() +
                   ", bytes = " + node.getTotalBytes());
        writer.addEvent(node);
        writer.addEvent(node);

            //------------------------------
            // Add entire record at once, 2x
            //------------------------------

            RecordOutputStream recOut = new RecordOutputStream(order, 0, 0, CompressionType.RECORD_UNCOMPRESSED);
            ByteBuffer dataBuffer2 = ByteBuffer.allocate(40);
            for (short i=0; i < 20; i++) {
                dataBuffer2.putShort(i);
            }
            dataBuffer2.flip();
System.out.println("add entire record (containing " + dataBuffer2.remaining() + " bytes of data");
            recOut.addEvent(dataBuffer2.array(), 0, 40);
            writer.writeRecord(recOut);

System.out.println("add the previous record again, but with one more event added  ... ");
            recOut.addEvent(dataBuffer2.array(), 0, 40);
            writer.writeRecord(recOut);

            //------------------------------
            // Add last event
            //------------------------------
System.out.println("once more, add event of len = " + dataBuffer.remaining());
            writer.addEvent(dataBuffer);

        //------------------------------

System.out.println("Past writes");

        writer.close();

        // Get ready-to-read buffer
        buffer = writer.getBuffer();
System.out.println("Finished buffer ->\n" + buffer);
System.out.println("Past close, now read it");

Utilities.printBytes(buffer, 0, 200, "Buffer Bytes");
ByteBuffer copy = ByteBuffer.allocate(buffer.capacity());
System.arraycopy(buffer.array(), 0, copy.array(), 0, buffer.capacity());

        //------------------------------
        //---- READ --------------------
        //------------------------------

        int readerType = 0;

        if (readerType == 0) {
            Reader reader = new Reader(buffer);
System.out.println("PAST creating Reader object");

            // COmpare original with copy
            boolean unchanged = true;
            int index = 0;
            for (int i=0; i < buffer.capacity(); i++) {
                if (buffer.array()[i] != copy.array()[i]) {
                    unchanged = false;
                    index = i;
                    System.out.print("Orig buffer CHANGED at byte #" + index);
                    System.out.println(", " + buffer.array()[i] + " changed to " + copy.array()[i]);
                    Utilities.printBytes(buffer, 0, 200, "Buffer Bytes");
                    break;
                }
            }
            if (unchanged) {
                System.out.println("ORIGINAL buffer Unchanged!");
            }

            int evCount = reader.getEventCount();
            System.out.println("Read in buffer, got " + evCount + " events");

            String dict = reader.getDictionary();
            System.out.println("   Got dictionary = " + dict);

            byte[] pFE = reader.getFirstEvent();
            if (pFE != null) {
                System.out.println("   First Event bytes = " + pFE.length);
                System.out.println("   First Event values = \n" + "   ");
                for (int i = 0; i < pFE.length; i++) {
                    System.out.println((int) (pFE[i]) + ",  ");
                }
                System.out.println("\n");
            }

            System.out.println("Print out regular events:");

            for (int i = 0; i < reader.getEventCount(); i++) {
                byte[] data = reader.getEvent(i);
                Utilities.printBytes(data, 0, data.length, "  Event #" + i);
            }
        }
        // Use evio reader to see what happens when we have non-evio events ....
        else if (readerType == 1) {
            try {
                EvioCompactReader reader = new EvioCompactReader(buffer);

                int evCount = reader.getEventCount();
                System.out.println("Read in buffer, got " + evCount + " events");

                String dict = reader.getDictionaryXML();
                System.out.println("   Got dictionary = " + dict);

                // Compact reader does not deal with first events, so skip over it

                System.out.println("Print out regular events:");

                for (int i = 0; i < reader.getEventCount(); i++) {
                    EvioNode compactNode = reader.getScannedEvent(i + 1);
                    System.out.println("node ->\n" + compactNode);

                    ByteBuffer dataBuf = compactNode.getByteData(true);
//                        ByteBuffer buffie(4*compactNode->getDataLength());
//                        auto dataBuf = compactNode->getByteData(buffie,true);

                    Utilities.printBytes(dataBuf, dataBuf.position(), dataBuf.remaining(),
                            "  Event #" + i);
                }
            }
            catch (EvioException e) {
                System.out.println("PROBLEM: " + e.getMessage());
            }
        }
        else if (readerType == 2) {
            try {
                EvioReader reader = new EvioReader(buffer);

                int evCount = reader.getEventCount();
                System.out.println("Read in buffer, got " + evCount + " events");

                String dict = reader.getDictionaryXML();
                System.out.println("   Got dictionary = " + dict);

                EvioEvent pFE = reader.getFirstEvent();
                if (pFE != null) {
                    System.out.println("   First Event bytes = " + pFE.getTotalBytes());
                    System.out.println("   First Event values = \n" + "   ");
                    for (int i = 0; i < pFE.getRawBytes().length; i++) {
                        System.out.println((int) (pFE.getRawBytes()[i]) + ",  ");
                    }
                    System.out.println("\n");
                }

                System.out.println("Print out regular events:");

                for (int i = 0; i < reader.getEventCount(); i++) {
                    EvioEvent ev = reader.getEvent(i + 1);
                    //System.out.println("ev ->\n" + ev);

                    byte[] dataVec = ev.getRawBytes();
                    Utilities.printBytes(dataVec, 0, dataVec.length, "  Event #" + i);
                }
            }
            catch (EvioException e) {
                System.out.println("PROBLEM: " + e.getMessage());
            }
        }

        // COmpare original with copy
        boolean unchanged = true;
        for (int i=0; i < buffer.capacity(); i++) {
            if (buffer.array()[i] != copy.array()[i]) {
                unchanged = false;
                break;
            }
        }
        if (unchanged) {
            System.out.println("ORIGINAL BUFFER Unchanged!");
        }
        else {
            System.out.println("ORIGINAL BUFFER CHANGED!!!!");
        }

    }



    public static void main(String args[]) {
//        String filename = "/dev/shm/EventWriterTest.evio";
//        System.out.println("Try writing " + filename);

//        // Write files
//        String actualFilename = ReadWriteTest.eventWriteFileMT(filename);
//        System.out.println("Finished writing, now try reading n" + actualFilename);
//
//        // Read files just written
//        ReadWriteTest.readFile2(actualFilename);

        try {
            writeAndReadBuffer();
        }
        catch (Exception e) {
            e.printStackTrace();
        }
        System.out.println("----------------------------------------\n");


    }


//    public static void main1(String args[]) {
//        String filename   = "/dev/shm/hipoTest.evio";
//        String filenameMT = "/dev/shm/hipoTestMT.evio";
////    String filename   = "/dev/shm/hipoTest-j.evio";
////    String filenameMT = "/dev/shm/hipoTestMT-j.evio";
//
//        // Write files
//        ReadWriteTest.writeFile(filename);
//        ReadWriteTest.writeFileMT(filenameMT);
//
//        // Read files just written
//        ReadWriteTest.readFile(filename);
//        System.out.println("----------------------------------------\n");
//        ReadWriteTest.readFile(filenameMT);
//    }



};




