package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.nio.channels.FileChannel;

/**
 * Created by IntelliJ IDEA.
 * User: timmer
 * Date: 4/12/11
 * Time: 10:21 AM
 * To change this template use File | Settings | File Templates.
 */
public class SwapTest {

    private static void printDoubleBuffer(ByteBuffer byteBuffer) {
        byteBuffer.flip();
        System.out.println();
        for (int i=0; i < byteBuffer.limit()/8 ; i++) {
            System.out.print(byteBuffer.getDouble() + " ");
            if ((i+1)%8 == 0) System.out.println();
        }
        System.out.println();
    }

    private static void printIntBuffer(ByteBuffer byteBuffer) {
        byteBuffer.flip();
        System.out.println();
        for (int i=0; i < byteBuffer.limit()/4 ; i++) {
            System.out.print(byteBuffer.getInt() + " ");
            if ((i+1)%16 == 0) System.out.println();
        }
        System.out.println();
    }


    /**
      * This method prints out a portion of a given ByteBuffer object
      * in hex representation of ints.
      *
      * @param buf buffer to be printed out
      * @param lenInInts length of data in ints to be printed
      */
     private static void printBuffer(ByteBuffer buf, int lenInInts) {
         IntBuffer ibuf = buf.asIntBuffer();
         lenInInts = lenInInts > ibuf.capacity() ? ibuf.capacity() : lenInInts;
         for (int i=0; i < lenInInts; i++) {
             System.out.println("  Buf(" + i + ") = 0x" + Integer.toHexString(ibuf.get(i)));
         }
     }


    // data
//    static      byte[]   byteData   = new byte[]   {};
//    static      short[]  shortData  = new short[]  {};
//    static      int[]    intData    = new int[]    {};
//    static      long[]   longData   = new long[]   {};
//    static      float[]  floatData  = new float[]  {};
//    static      double[] doubleData = new double[] {};
//    static      String[] stringData = new String[] {};

//    static      byte[]   byteData   = new byte[]   {1};
//    static      short[]  shortData  = new short[]  {1};
//    static      int[]    intData    = new int[]    {1};
//    static      long[]   longData   = new long[]   {1};
//    static      float[]  floatData  = new float[]  {1.F};
//    static      double[] doubleData = new double[] {1.};
//    static      String[] stringData = new String[] {"1"};

    static      byte[]   byteData   = new byte[]   {1,2,3};
    static      short[]  shortData  = new short[]  {1,2,3};
    static      int[]    intData    = new int[]    {1,2,3};
    static      long[]   longData   = new long[]   {1L,2L,3L};
    static      float[]  floatData  = new float[]  {1.F,2.F,3.F};
    static      double[] doubleData = new double[] {1.,2.,3.};
    static      String[] stringData = new String[] {"123", "456", "789"};


    static ByteBuffer firstBlockHeader = ByteBuffer.allocate(32);
    static ByteBuffer emptyLastHeader  = ByteBuffer.allocate(32);

    static {
        emptyLastHeader.putInt(0,  8);
        emptyLastHeader.putInt(4,  2);
        emptyLastHeader.putInt(8,  8);
        emptyLastHeader.putInt(12, 0);
        emptyLastHeader.putInt(16, 0);
        emptyLastHeader.putInt(20, 0x204);
        emptyLastHeader.putInt(24, 0);
        emptyLastHeader.putInt(28, 0xc0da0100);
    }


    static void setFirstBlockHeader(int words, int count) {
        firstBlockHeader.putInt(0, words + 8);
        firstBlockHeader.putInt(4,  1);
        firstBlockHeader.putInt(8,  8);
        firstBlockHeader.putInt(12, count);
        firstBlockHeader.putInt(16, 0);
        firstBlockHeader.putInt(20, 4);
        firstBlockHeader.putInt(24, 0);
        firstBlockHeader.putInt(28, 0xc0da0100);
    }


    static void swapBlockHeader(ByteBuffer buf, int index) {
        buf.putInt(index,    Integer.reverseBytes(buf.getInt(index)));
        buf.putInt(index+4,  Integer.reverseBytes(buf.getInt(index + 4)));
        buf.putInt(index+8,  Integer.reverseBytes(buf.getInt(index + 8)));
        buf.putInt(index+12, Integer.reverseBytes(buf.getInt(index + 12)));
        buf.putInt(index+16, Integer.reverseBytes(buf.getInt(index + 16)));
        buf.putInt(index+20, Integer.reverseBytes(buf.getInt(index + 20)));
        buf.putInt(index+24, Integer.reverseBytes(buf.getInt(index + 24)));
        buf.putInt(index+28, Integer.reverseBytes(buf.getInt(index + 28)));
    }


    static CompositeData[] createCompositeData() {
           // Create a CompositeData object ...

        // Format 1 to write a N shorts, 1 float, 1 double a total of N times
        String format1 = "N(NS,F,D)";

        // Now create some data
        CompositeData.Data myData1 = new CompositeData.Data();
        myData1.addN(2);
        myData1.addN(3);
        myData1.addShort(new short[]{1, 2, 3}); // use array for convenience
        myData1.addFloat(1.0F);
        myData1.addDouble(Math.PI);
        myData1.addN(1);
        myData1.addShort((short) 4); // use array for convenience
        myData1.addFloat(2.0F);
        myData1.addDouble(2. * Math.PI);

        // Format 2 to write an unsigned int, unsigned char, and N number of
        // M (int to be found) ascii characters & 1 64-bit int. We need to
        // wait before we can create this format string because we don't know
        // yet how many String characters (M) we have to determine the "Ma" term.
        // String format2 = "i,c,N(Ma,L)";

        // Now create some data
        CompositeData.Data myData2 = new CompositeData.Data();
        myData2.addUint(21);
        myData2.addUchar((byte) 22);
        myData2.addN(1);
        // Define our ascii data
        String s[] = new String[2];
        s[0] = "str1";
        s[1] = "str2";
        // Find out how what the composite format representation of this string is
        String asciiFormat = CompositeData.stringsToFormat(s);
        // Format to write an unsigned int, unsigned char, and N number of
        // M ascii characters & 1 64-bit int.
        System.out.println("ascii format = " + asciiFormat);
        String format2 = "i,c,N(" + asciiFormat + ",L)";
        myData2.addString(s);
        myData2.addLong(24L);

        // Create CompositeData array
        CompositeData[] cData = new CompositeData[2];
        try {
            cData[0] = new CompositeData(format1, 1, myData1, 1, 1);
            cData[1] = new CompositeData(format2, 2, myData2, 2 ,2);
        }
        catch (EvioException e) {
            e.printStackTrace();
            System.exit(-1);
        }

        return cData;
    }


    /** Build an event with an EventBuilder. */
    static EvioEvent createSingleEvent(int tag) {

        // Top level event
        EvioEvent event = null;

        try {
            // Build event (bank of banks) with EventBuilder object
            EventBuilder builder = new EventBuilder(tag, DataType.BANK, 1);
            event = builder.getEvent();


                // bank of banks
                EvioBank bankBanks = new EvioBank(tag+1, DataType.BANK, 2);
                builder.addChild(event, bankBanks);

                    // bank of ints
                    EvioBank bankInts = new EvioBank(tag+2, DataType.INT32, 3);
                    //bankInts.appendIntData(intData);
                    builder.setIntData(bankInts, intData);
                    builder.addChild(bankBanks, bankInts);

                    // bank of unsigned bytes
                    EvioBank bankBytes = new EvioBank(tag+3, DataType.UCHAR8, 4);
                    //bankBytes.appendByteData(byteData);
                    builder.setByteData(bankBytes, byteData);
                    builder.addChild(bankBanks, bankBytes);

                    // bank of unsigned shorts
                    EvioBank bankShorts = new EvioBank(tag+4, DataType.USHORT16, 5);
                    bankShorts.appendShortData(shortData);
                    builder.addChild(bankBanks, bankShorts);

                    // bank of longs
                    EvioBank bankLongs = new EvioBank(tag+5, DataType.LONG64, 6);
                    bankLongs.appendLongData(longData);
                    builder.addChild(bankBanks, bankLongs);

                    // bank of floats
                    EvioBank bankFloats = new EvioBank(tag+6, DataType.FLOAT32, 7);
                    bankFloats.appendFloatData(floatData);
                    builder.addChild(bankBanks, bankFloats);

                    // bank of doubles
                    EvioBank bankDoubles = new EvioBank(tag+7, DataType.DOUBLE64, 8);
                    bankDoubles.appendDoubleData(doubleData);
                    builder.addChild(bankBanks, bankDoubles);

                    // bank of strings
                    EvioBank bankStrings = new EvioBank(tag+8, DataType.CHARSTAR8, 9);
                    bankStrings.appendStringData(stringData);
                    builder.addChild(bankBanks, bankStrings);

                    // bank of composite data array
                    CompositeData[] cdata = createCompositeData();
                    EvioBank bankComps = new EvioBank(tag+9, DataType.COMPOSITE, 10);
                    bankComps.appendCompositeData(cdata);
                    builder.addChild(bankBanks, bankComps);


                // Bank of segs
                EvioBank bankBanks2 = new EvioBank(tag+10, DataType.SEGMENT, 11);
                builder.addChild(event, bankBanks2);

                    // segment of ints
                    EvioSegment segInts = new EvioSegment(tag+11, DataType.INT32);
                    segInts.appendIntData(intData);
                    builder.addChild(bankBanks2, segInts);

                    // segment of shorts
                    EvioSegment segShorts = new EvioSegment(tag+12, DataType.SHORT16);
                    segShorts.appendShortData(shortData);
                    builder.addChild(bankBanks2, segShorts);


                    // segment of segments
                    EvioSegment segSegments = new EvioSegment(tag+13, DataType.SEGMENT);
                    builder.addChild(bankBanks2, segSegments);

                        // segment of bytes
                        EvioSegment segBytes = new EvioSegment(tag+14, DataType.CHAR8);
                        segBytes.appendByteData(byteData);
                        builder.addChild(segSegments, segBytes);

                        // segment of doubles
                        EvioSegment segDoubles = new EvioSegment(tag+15, DataType.DOUBLE64);
                        segDoubles.appendDoubleData(doubleData);
                        builder.addChild(segSegments, segDoubles);


                // Bank of tag segs
                EvioBank bankBanks4 = new EvioBank(tag+16, DataType.TAGSEGMENT, 17);
                builder.addChild(event, bankBanks4);

                    // tag segment of bytes
                    EvioTagSegment tagSegBytes = new EvioTagSegment(tag+17, DataType.CHAR8);
                    tagSegBytes.appendByteData(byteData);
                    builder.addChild(bankBanks4, tagSegBytes);

                    // tag segment of shorts
                    EvioTagSegment tagSegShorts = new EvioTagSegment(tag+18, DataType.SHORT16);
                    tagSegShorts.appendShortData(shortData);
                    builder.addChild(bankBanks4, tagSegShorts);

                    // tag seg of longs
                    EvioTagSegment tagsegLongs = new EvioTagSegment(tag+19, DataType.LONG64);
                    tagsegLongs.appendLongData(longData);
                    builder.addChild(bankBanks4, tagsegLongs);

        }
        catch (EvioException e) {
            e.printStackTrace();
        }

        return event;
    }


    /** Build the same event as above but with a CompactEventBuilder instead of an EventBuilder. */
    static ByteBuffer createCompactSingleEvent(int tag) {

        // Buffer to fill
        ByteBuffer buffer = ByteBuffer.allocate(1048);
        CompactEventBuilder builder = null;
        int num = tag;

        try {
            builder = new CompactEventBuilder(buffer);

            // add top/event level bank of banks
            builder.openBank(tag, num, DataType.BANK);

                // add bank of banks
                builder.openBank(tag+1, num+1, DataType.BANK);

                    // add bank of ints
                    builder.openBank(tag+2, num+2, DataType.INT32);
                    builder.addIntData(intData);
                    builder.closeStructure();

                    // add bank of unsigned bytes
                    builder.openBank(tag+3, num+3, DataType.UCHAR8);
                    builder.addByteData(byteData);
                    builder.closeStructure();

                    // add bank of unsigned shorts
                    builder.openBank(tag+4, num+4, DataType.USHORT16);
                    builder.addShortData(shortData);
                    builder.closeStructure();

                    // add bank of longs
                    builder.openBank(tag+5, num+5, DataType.LONG64);
                    builder.addLongData(longData);
                    builder.closeStructure();

                    // add bank of floats
                    builder.openBank(tag+6, num+6, DataType.FLOAT32);
                    builder.addFloatData(floatData);
                    builder.closeStructure();

                    // add bank of doubles
                    builder.openBank(tag+7, num+7, DataType.DOUBLE64);
                    builder.addDoubleData(doubleData);
                    builder.closeStructure();

                    // add bank of strings
                    builder.openBank(tag+8, num+8, DataType.CHARSTAR8);
                    builder.addStringData(stringData);
                    builder.closeStructure();

                    // bank of composite data array
                    CompositeData[] cdata = createCompositeData();
                    builder.openBank(tag+9, num+9, DataType.COMPOSITE);
                    builder.addCompositeData(cdata);
                    builder.closeStructure();

                builder.closeStructure();


                // add bank of segs
                builder.openBank(tag+10, num+10, DataType.SEGMENT);

                    // add seg of ints
                    builder.openSegment(tag+11, DataType.INT32);
                    builder.addIntData(intData);
                    builder.closeStructure();

                    // add seg of shorts
                    builder.openSegment(tag+12, DataType.SHORT16);
                    builder.addShortData(shortData);
                    builder.closeStructure();

                    // add seg of segs
                    builder.openSegment(tag+13, DataType.SEGMENT);

                        // add seg of bytes
                        builder.openSegment(tag+14, DataType.CHAR8);
                        builder.addByteData(byteData);
                        builder.closeStructure();

                        // add seg of doubles
                        builder.openSegment(tag+15, DataType.DOUBLE64);
                        builder.addDoubleData(doubleData);
                        builder.closeStructure();

                    builder.closeStructure();
                builder.closeStructure();


            // add bank of tagsegs
            builder.openBank(tag+16, num+16, DataType.TAGSEGMENT);

                // add tagseg of bytes
                builder.openTagSegment(tag+17, DataType.CHAR8);
                builder.addByteData(byteData);
                builder.closeStructure();

                // add tagseg of shorts
                builder.openTagSegment(tag+18, DataType.SHORT16);
                builder.addShortData(shortData);
                builder.closeStructure();

                // add tagseg of longs
                builder.openTagSegment(tag+19, DataType.LONG64);
                builder.addLongData(longData);
                builder.closeStructure();

            builder.closeAll();
        }
        catch (EvioException e) {
            e.printStackTrace();
        }

        return builder.getBuffer();
    }


    /** For testing only */
    public static void main1(String args[]) {

        try {
            EvioEvent bank = createSingleEvent(1);
            int byteSize = bank.getTotalBytes();

            ByteBuffer bb1 = ByteBuffer.allocate(byteSize);
            ByteBuffer bb2 = ByteBuffer.allocate(byteSize);

            // Write events
            bank.write(bb1);

            // Get ready to read buffer
            bb1.flip();

            // Get JIT compiler to speed things up first
            for (int i=0; i < 2000000; i++) {
                ByteDataTransformer.swapEvent(bb1, bb2, 0, 0);
                ByteDataTransformer.swapEvent(bb2, bb1, 0, 0);
            }


            long t1 = System.currentTimeMillis();

            for (int i=0; i < 2000000; i++) {
                ByteDataTransformer.swapEvent(bb1, bb2, 0, 0);
                ByteDataTransformer.swapEvent(bb2, bb1, 0, 0);
            }

            long t2 = System.currentTimeMillis();

            System.out.println("Time = " + (t2 - t1) + " millisec");

//            List<EvioNode> nodeList =  new ArrayList<EvioNode>(20);
//            ByteDataTransformer.swapEvent(bb1, bb2, 0, 0, nodeList);
//
//            for (EvioNode node : nodeList) {
//                System.out.println("node: " + node.getDataTypeObj());
//            }
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }



    /** Write event to one file and it swapped version to another file. */
    public static void main(String args[]) {
        boolean useEventBuilder = false;
        ByteBuffer bb1, bb2;
        int byteSize = 0;

        try {

            if (useEventBuilder) {
                EvioEvent bank = createSingleEvent(1);
                byteSize = bank.getTotalBytes();

                bb1 = ByteBuffer.allocate(byteSize + 2 * (32));
                bb2 = ByteBuffer.allocate(byteSize + 2 * (32));

                // Write first block header
                setFirstBlockHeader(byteSize / 4, 1);
                bb1.put(firstBlockHeader);
                firstBlockHeader.position(0);

                // Write events
                bank.write(bb1);
            }
            // if using CompactEventBuilder ...
            else {
                ByteBuffer buffie = createCompactSingleEvent(1);
                byteSize = buffie.limit();

                bb1 = ByteBuffer.allocate(byteSize + 2 * (32));
                bb2 = ByteBuffer.allocate(byteSize + 2 * (32));

                // Write first block header
                setFirstBlockHeader(byteSize / 4, 1);
                bb1.put(firstBlockHeader);
                firstBlockHeader.position(0);

                // Write events
                bb1.put(buffie);
            }

            // Write last block header
            bb1.put(emptyLastHeader);
            emptyLastHeader.position(0);

            // Get ready to read buffer
            bb1.flip();

            // Take buffer and swap it
            ByteDataTransformer.swapEvent(bb1, bb2, 32, 32);

            // Be sure to get evio block headers right so we
            // can read swapped event with an EvioReader
            bb2.position(0);
            bb2.put(firstBlockHeader);
            firstBlockHeader.position(0);
            swapBlockHeader(bb2, 0);

            bb2.position(32 + byteSize);
            bb2.put(emptyLastHeader);
            emptyLastHeader.position(0);
            swapBlockHeader(bb2, 32 + byteSize);

            bb2.position(0);

            File evFile = new File("/dev/shm/regularEvent.evio");
            FileOutputStream fileOutputStream = new FileOutputStream(evFile);
            FileChannel fileChannel = fileOutputStream.getChannel();
            fileChannel.write(bb1);
            fileChannel.close();

            File evFile2 = new File("/dev/shm/swappedEvent.evio");
            fileOutputStream = new FileOutputStream(evFile2);
            fileChannel = fileOutputStream.getChannel();
            fileChannel.write(bb2);
            fileChannel.close();

        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }


    /** For testing only */
    public static void main2(String args[]) {

        try {
            EvioEvent bank = createSingleEvent(1);
            int byteSize = bank.getTotalBytes();

            ByteBuffer bb1 = ByteBuffer.allocate(byteSize + 2*(32));
            ByteBuffer bb2 = ByteBuffer.allocate(byteSize + 2*(32));
            ByteBuffer bb3 = ByteBuffer.allocate(byteSize + 2*(32));

            // Write first block header
            setFirstBlockHeader(byteSize/4, 1);
            bb1.put(firstBlockHeader);
            firstBlockHeader.position(0);

            // Write events
            bank.write(bb1);

            // Write last block header
            bb1.put(emptyLastHeader);
            emptyLastHeader.position(0);

            // Get ready to read buffer
            bb1.flip();

            System.out.println("bb1 limit = " + bb1.limit() + ", pos = " + bb1.position() + ", cap = " + bb1.capacity());

            System.out.println("XML:\n" + bank.toXML());

            System.out.println("\n*************\n");

//            // Change byte buffer back into an event
//            EvioReader reader = new EvioReader(bb1);
//            EvioEvent ev = reader.parseEvent(1);
//            bb1.position(0);
//
//            System.out.println("\n\n reconstituted XML:\n" + ev.toXML());

            // Take buffer and swap it
            ByteDataTransformer.swapEvent(bb1, bb2, 32, 32);

            // Be sure to get evio block headers right so we
            // can read swapped event with an EvioReader
            bb2.position(0);
            bb2.put(firstBlockHeader);
            firstBlockHeader.position(0);
            swapBlockHeader(bb2, 0);

            bb2.position(32 + byteSize);
            bb2.put(emptyLastHeader);
            emptyLastHeader.position(0);
            swapBlockHeader(bb2, 32 + byteSize);

//            // Change byte buffer back into an event
//            EvioReader reader = new EvioReader(bb2);
//            EvioEvent ev = reader.parseEvent(1);
//            bb2.position(0);
//
//            System.out.println("\n\n reconstituted XML:\n" + ev.toXML());


            // Take swapped buffer and swap it again
            ByteDataTransformer.swapEvent(bb2, bb3, 32, 32);

            // Be sure to get evio block headers right so we
            // can read swapped event with an EvioReader
            bb3.position(0);
            bb3.put(firstBlockHeader);
            firstBlockHeader.position(0);
            swapBlockHeader(bb3, 0);
            swapBlockHeader(bb3, 0);

            bb3.position(32 + byteSize);
            bb3.put(emptyLastHeader);
            emptyLastHeader.position(0);
            swapBlockHeader(bb3, 32 + byteSize);
            swapBlockHeader(bb3, 32 + byteSize);

            // Change byte buffer back into an event
            EvioReader reader = new EvioReader(bb3);
            EvioEvent ev = reader.parseEvent(1);
            bb3.position(0);


            System.out.println("\n\n reconstituted XML:\n" + ev.toXML());

            System.out.println("bb1 limit = " + bb1.limit() + ", pos = " + bb1.position() + ", cap = " + bb1.capacity());
            System.out.println("bb3 limit = " + bb3.limit() + ", pos = " + bb3.position() + ", cap = " + bb3.capacity());

            IntBuffer ibuf1 = bb1.asIntBuffer();
            IntBuffer ibuf2 = bb3.asIntBuffer();
            int lenInInts = ibuf1.limit() < ibuf1.capacity() ? ibuf1.limit() : ibuf1.capacity();
            System.out.println("ibuf1 limit = " + ibuf1.limit() + ", cap = " + ibuf1.capacity());
            System.out.println("ibuf2 limit = " + ibuf2.limit() + ", cap = " + ibuf2.capacity());
            System.out.println("bb1           bb2\n---------------------------");
            for (int i=0; i < lenInInts; i++) {
                if (ibuf1.get(i) != ibuf2.get(i)) {
                    System.out.println("index " + i + ": 0x" + Integer.toHexString(ibuf1.get(i)) +
                                               " swapped to 0x" +Integer.toHexString(ibuf2.get(i)));
                }
//                System.out.println("0x" + Integer.toHexString(ibuf1.get(i)) +
//                                           "    0x" + Integer.toHexString(ibuf1.get(i)));
            }
        }
        catch (Exception e) {
            e.printStackTrace();
        }


    }

}
