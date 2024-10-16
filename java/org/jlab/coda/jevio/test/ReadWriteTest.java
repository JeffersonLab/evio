package org.jlab.coda.jevio.test;

import org.jlab.coda.hipo.*;
import org.jlab.coda.jevio.*;

import javax.swing.tree.DefaultMutableTreeNode;
import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.nio.ShortBuffer;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

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
        cData[0] = new CompositeData(format, 1, myData, 1, 1,
                ByteOrder.BIG_ENDIAN);

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

    static void treeTest() {


        try {
            // Create some banks
            EvioBank topBank   = new EvioBank(0, DataType.BANK, 0);
            EvioBank midBank   = new EvioBank(1, DataType.BANK, 1);
            EvioBank midBank2  = new EvioBank(2, DataType.SEGMENT, 2);
            EvioBank childBank = new EvioBank(4, DataType.FLOAT32, 4);

            // Child bank's float data
            float[] fData = new float[] {0.F, 1.F};
            childBank.setFloatData(fData);

            // Create tree
            topBank.insert(midBank);
            topBank.insert(midBank2);
            midBank.insert(childBank);

            // Create more structures
            EvioSegment childSeg1 = new EvioSegment(5, DataType.INT32);
            EvioSegment childSeg2 = new EvioSegment(6, DataType.INT32);
            EvioSegment childSeg3 = new EvioSegment(7, DataType.SHORT16);

            // Children data
            int[] iData = new int[] {3, 4};
            childSeg1.setIntData(iData);

            int[] iData2 = new int[] {5, 6};
            childSeg2.setIntData(iData2);

            short[] sData = new short[] {7, 8};
            childSeg3.setShortData(sData);

            // Add segments to tree
            midBank2.insert(childSeg1);
            midBank2.insert(childSeg2);
            midBank2.insert(childSeg3);

            //--------------------
            // Print out tree info
            //--------------------
            System.out.println("midBank2 has " + midBank2.getChildCount() + " children");
            System.out.println("Remove childSeg2 from midBank2");
            midBank2.remove(childSeg2);
            System.out.println("midBank2 now has " + midBank2.getChildCount() + " children");

            System.out.println("iterate thru topBank children:");
            for (BaseStructure kid : topBank.getChildrenList()) {
                System.out.println("  kid = " + kid.toString());
            }

            System.out.println("childSeg1 is a Leaf? " + childSeg1.isLeaf());

        } catch (EvioException e) {
            e.printStackTrace();
        }

    }

    static void writeAndReadBuffer() throws EvioException, HipoException, IOException {

        // Create Buffer
        int bufSize = 3000;
        ByteOrder order = ByteOrder.BIG_ENDIAN;
        ByteBuffer buffer = ByteBuffer.allocate(bufSize);
        buffer.order(order);

        boolean compressed = false;

        if (!compressed) {

            int loops = 3;

            byte firstEvent[] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29};
            int firstEventLen = 10;

            CompressionType compType = CompressionType.RECORD_UNCOMPRESSED;

            // Possible user header data
            byte[] userHdr = new byte[10];
            for (byte i = 0; i < 10; i++) {
                userHdr[i] = (byte) (i + 16);
            }

            //Writer writer(buffer, 0, 0, "", firstEvent, firstEventLen);
            Writer writer = new Writer(buffer, userHdr);
            System.out.println("Past creating Writer object");

            // Calling the following method makes a shared pointer out of dataArray, so don't delete
            ByteBuffer dataBuffer = ByteBuffer.allocate(20);
            for (short i = 0; i < 10; i++) {
                dataBuffer.putShort(i);
            }
            dataBuffer.flip();

            System.out.println("Data buffer ->\n" + dataBuffer.toString());

            // Create an evio bank of ints
            ByteBuffer evioDataBuf = generateEvioBuffer(order, 0, 0);
            // Create node from this buffer
            EvioNode node = EvioNode.extractEventNode(evioDataBuf, null, 0, 0, 0);

//            while (true) {
//                // random data array
//                //writer.addEvent(dataArray, 0, 20);
//System.out.println("add event of len = " + dataBuffer.remaining());
//                writer.addEvent(dataBuffer);
//
//                if (--loops < 1) break;
//            }

            System.out.println("add event of node,  data type = " + node.getDataTypeObj().toString() +
                    ", bytes = " + node.getTotalBytes());
            writer.addEvent(node);
            writer.addEvent(node);

//            //------------------------------
//            // Add entire record at once, 2x
//            //------------------------------
//
//            RecordOutputStream recOut = new RecordOutputStream(order, 0, 0, CompressionType.RECORD_UNCOMPRESSED);
//            ByteBuffer dataBuffer2 = ByteBuffer.allocate(40);
//            for (short i=0; i < 20; i++) {
//                dataBuffer2.putShort(i);
//            }
//            dataBuffer2.flip();
//System.out.println("add entire record (containing " + dataBuffer2.remaining() + " bytes of data");
//            recOut.addEvent(dataBuffer2.array(), 0, 40);
//            writer.writeRecord(recOut);
//
//System.out.println("add the previous record again, but with one more event added  ... ");
//            recOut.addEvent(dataBuffer2.array(), 0, 40);
//            writer.writeRecord(recOut);
//
//            //------------------------------
//            // Add last event
//            //------------------------------
//System.out.println("once more, add event of len = " + dataBuffer.remaining());
//            writer.addEvent(dataBuffer);

            //------------------------------

            System.out.println("Past writes");

            writer.close();

            // Get ready-to-read buffer
            buffer = writer.getBuffer();

        }
        else {

            int loops = 3;

            byte firstEvent[] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29};
            int firstEventLen = 10;

            CompressionType compType = CompressionType.RECORD_COMPRESSION_GZIP;

            // Possible user header data
            byte[] userHdr = new byte[10];
            for (byte i = 0; i < 10; i++) {
                userHdr[i] = (byte) (i + 16);
            }

            // We cannot write compressed data into a buffer directly, but we can write it to a file
            // and then read the file back into a buffer (minus the file header).
            Writer writer = new Writer(HeaderType.EVIO_FILE, order, 0, 0,
            null, null, compType, false);
            writer.open("./temp");


            //Writer writer(buffer, 0, 0, "", firstEvent, firstEventLen);
            //Writer writer = new Writer(buffer, userHdr);

            // Calling the following method makes a shared pointer out of dataArray, so don't delete
            ByteBuffer dataBuffer = ByteBuffer.allocate(20);
            for (short i = 0; i < 10; i++) {
                dataBuffer.putShort(i);
            }
            dataBuffer.flip();

            System.out.println("Data buffer ->\n" + dataBuffer.toString());

            // Create an evio bank of ints
            ByteBuffer evioDataBuf = generateEvioBuffer(order, 0, 0);
            // Create node from this buffer
            EvioNode node = EvioNode.extractEventNode(evioDataBuf, null, 0, 0, 0);

//            while (true) {
//                // random data array
//                //writer.addEvent(dataArray, 0, 20);
//System.out.println("add event of len = " + dataBuffer.remaining());
//                writer.addEvent(dataBuffer);
//
//                if (--loops < 1) break;
//            }

            System.out.println("add event of node,  data type = " + node.getDataTypeObj().toString() +
                    ", bytes = " + node.getTotalBytes());
            writer.addEvent(node);
            writer.addEvent(node);

//            //------------------------------
//            // Add entire record at once, 2x
//            //------------------------------
//
//            RecordOutputStream recOut = new RecordOutputStream(order, 0, 0, CompressionType.RECORD_UNCOMPRESSED);
//            ByteBuffer dataBuffer2 = ByteBuffer.allocate(40);
//            for (short i=0; i < 20; i++) {
//                dataBuffer2.putShort(i);
//            }
//            dataBuffer2.flip();
//System.out.println("add entire record (containing " + dataBuffer2.remaining() + " bytes of data");
//            recOut.addEvent(dataBuffer2.array(), 0, 40);
//            writer.writeRecord(recOut);
//
//System.out.println("add the previous record again, but with one more event added  ... ");
//            recOut.addEvent(dataBuffer2.array(), 0, 40);
//            writer.writeRecord(recOut);
//
//            //------------------------------
//            // Add last event
//            //------------------------------
//System.out.println("once more, add event of len = " + dataBuffer.remaining());
//            writer.addEvent(dataBuffer);

            //------------------------------

            System.out.println("Past writes");

            writer.close();

            Utilities.printBytes("./temp", 0, 1000, "file bytes");

            System.out.println("PAST printBytes");

            // Find out size of file.
            // "ate" mode flag will go immediately to file's end (do this to get its size)
            File file = new File("./temp");
            bufSize = (int)file.length();
            System.out.println("Compressed file has byte length = " + bufSize);

            buffer.limit(bufSize);
            Utilities.readBytes("./temp", buffer);
            buffer.position(FileHeader.HEADER_SIZE_BYTES);
        }

System.out.println("Past close, now read it");

Utilities.printBytes(buffer, 0, bufSize, "Buffer Bytes");

        ByteBuffer copy = ByteBuffer.allocate(buffer.capacity());
        ByteBuffer copy2 = ByteBuffer.allocate(buffer.capacity());
        System.arraycopy(buffer.array(), 0, copy.array(), 0, buffer.capacity());
        System.arraycopy(buffer.array(), 0, copy2.array(), 0, buffer.capacity());
        copy.limit(buffer.limit()).position(FileHeader.HEADER_SIZE_BYTES);
        copy2.limit(buffer.limit()).position(FileHeader.HEADER_SIZE_BYTES);

        //------------------------------
        //----  READER1 ----------------
        //------------------------------


        Reader reader = new Reader(buffer);

        // Compare original with copy
        boolean unchanged = true;
        if (!compressed) {
            int index = 0;
            for (int i = 0; i < buffer.capacity(); i++) {
                if (buffer.array()[i] != copy.array()[i]) {
                    unchanged = false;
                    index = i;
                    System.out.print("Orig buffer CHANGED at byte #" + index);
                    System.out.println(", 0x" + Integer.toHexString(copy.array()[i]) + " changed to 0x" +
                            Integer.toHexString(buffer.array()[i]));
                    Utilities.printBytes(buffer, 0, 1600, "Buffer Bytes");
                    break;
                }
            }
            if (unchanged) {
                System.out.println("ORIGINAL buffer Unchanged!");
            }
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
        byte[] data = null;

        for (int i = 0; i < reader.getEventCount(); i++) {
            data = reader.getEvent(i);
            Utilities.printBytes(data, 0, data.length, "  Event #" + i);
        }


        System.out.println("--------------------------------------------");
        System.out.println("----------      READER2       --------------");
        System.out.println("--------------------------------------------");

        ByteBuffer dataBuf = null;

        try {
            EvioCompactReader reader2 = new EvioCompactReader(copy);

            int evCount2 = reader2.getEventCount();
            System.out.println("Read in buffer, got " + evCount2 + " events");

            String dict2 = reader2.getDictionaryXML();
            System.out.println("   Got dictionary = " + dict2);

            // Compact reader does not deal with first events, so skip over it

            System.out.println("Print out regular events:");

            for (int i = 0; i < reader2.getEventCount(); i++) {
                System.out.println("scanned event #" + i + " :");
                EvioNode compactNode = reader2.getScannedEvent(i + 1);
                System.out.println("node ->\n" + compactNode);

                dataBuf = compactNode.getStructureBuffer(true);
//                        ByteBuffer buffie(4*compactNode->getDataLength());
//                        auto dataBuf = compactNode->getByteData(buffie,true);

                Utilities.printBytes(dataBuf, dataBuf.position(), dataBuf.remaining(),
                        "  Event #" + i);
            }
        }
        catch (EvioException e) {
            System.out.println("PROBLEM: " + e.getMessage());
        }

        int index;
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

            String dict3 = reader3.getDictionaryXML();
            System.out.println("   Got dictionary = " + dict3);

            EvioEvent pFE3 = reader3.getFirstEvent();
            if (pFE3 != null) {
                System.out.println("   First Event bytes = " + pFE3.getTotalBytes());
                System.out.println("   First Event values = \n" + "   ");
                for (int i = 0; i < pFE3.getRawBytes().length; i++) {
                    System.out.println((int) (pFE3.getRawBytes()[i]) + ",  ");
                }
                System.out.println("\n");
            }

            System.out.println("Print out regular events:");

            for (int i = 0; i < reader3.getEventCount(); i++) {
                EvioEvent ev = reader3.getEvent(i + 1);
                System.out.println("ev ->\n" + ev);

                dataVec = ev.getRawBytes();
                Utilities.printBytes(dataVec, 0, dataVec.length, "  Event #" + i);
            }
        }
        catch (EvioException e) {
            System.out.println("PROBLEM: " + e.getMessage());
        }

        System.out.println("COmparing data (len = " +data.length + ") with dataVec (len = " + dataVec.length + ")");
        for (int i=0; i < dataVec.length; i++) {
            if ((data[i+8] != dataVec[i]) && (i > 3)) {
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




