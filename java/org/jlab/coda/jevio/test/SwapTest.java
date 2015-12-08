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

                    // bank of bytes
                    EvioBank bankBytes = new EvioBank(tag+3, DataType.UCHAR8, 4);
                    //bankBytes.appendByteData(byteData);
                    builder.setByteData(bankBytes, byteData);
                    builder.addChild(bankBanks, bankBytes);

                    // bank of shorts
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


                // Bank of segs
                EvioBank bankBanks2 = new EvioBank(tag+9, DataType.SEGMENT, 10);
                builder.addChild(event, bankBanks2);

                    // segment of ints
                    EvioSegment segInts = new EvioSegment(tag+10, DataType.INT32);
                    segInts.appendIntData(intData);
                    builder.addChild(bankBanks2, segInts);

                    // segment of shorts
                    EvioSegment segShorts = new EvioSegment(tag+11, DataType.SHORT16);
                    segShorts.appendShortData(shortData);
                    builder.addChild(bankBanks2, segShorts);


                    // segment of segments
                    EvioSegment segSegments = new EvioSegment(tag+12, DataType.SEGMENT);
                    builder.addChild(bankBanks2, segSegments);

                        // segment of bytes
                        EvioSegment segBytes = new EvioSegment(tag+13, DataType.CHAR8);
                        segBytes.appendByteData(byteData);
                        builder.addChild(segSegments, segBytes);

                        // segment of doubles
                        EvioSegment segDoubles = new EvioSegment(tag+14, DataType.DOUBLE64);
                        segDoubles.appendDoubleData(doubleData);
                        builder.addChild(segSegments, segDoubles);


                // Bank of tag segs
                EvioBank bankBanks4 = new EvioBank(tag+15, DataType.TAGSEGMENT, 16);
                builder.addChild(event, bankBanks4);

                    // tag segment of bytes
                    EvioTagSegment tagSegBytes = new EvioTagSegment(tag+16, DataType.CHAR8);
                    tagSegBytes.appendByteData(byteData);
                    builder.addChild(bankBanks4, tagSegBytes);

                    // tag segment of shorts
                    EvioTagSegment tagSegShorts = new EvioTagSegment(tag+17, DataType.SHORT16);
                    tagSegShorts.appendShortData(shortData);
                    builder.addChild(bankBanks4, tagSegShorts);

                    // tag seg of longs
                    EvioTagSegment tagsegLongs = new EvioTagSegment(tag+18, DataType.LONG64);
                    tagsegLongs.appendLongData(longData);
                    builder.addChild(bankBanks4, tagsegLongs);

        }
        catch (EvioException e) {
            e.printStackTrace();
        }

        return event;

    }

    static EvioEvent createSimpleEvent(int tag) {

        // Top level event
        EvioEvent event = null;

        try {
            // Build event (bank of banks) with EventBuilder object
            EventBuilder builder = new EventBuilder(tag, DataType.BANK, 1);
            event = builder.getEvent();

//            // bank of bytes
//            EvioBank bankBytes = new EvioBank(tag+1, DataType.UCHAR8, 2);
//            bankBytes.appendByteData(byteData);
//            builder.addChild(event, bankBytes);
//
//            // bank of shorts
//            EvioBank bankShorts = new EvioBank(tag+2, DataType.USHORT16, 3);
//            bankShorts.appendShortData(shortData);
//            builder.addChild(event, bankShorts);
//
//            // bank of ints
//            EvioBank bankInts = new EvioBank(tag+3, DataType.INT32, 4);
//            bankInts.appendIntData(intData);
//            builder.addChild(event, bankInts);
//
//            // bank of longs
//            EvioBank bankLongs = new EvioBank(tag+4, DataType.LONG64, 5);
//            bankLongs.appendLongData(longData);
//            builder.addChild(event, bankLongs);
//
//            // bank of floats
//            EvioBank bankFloats = new EvioBank(tag+5, DataType.FLOAT32, 6);
//            bankFloats.appendFloatData(floatData);
//            builder.addChild(event, bankFloats);

            // bank of doubles
            EvioBank bankDoubles = new EvioBank(tag+6, DataType.DOUBLE64, 7);
            bankDoubles.appendDoubleData(doubleData);
            builder.addChild(event, bankDoubles);

//            // bank of string array
//            EvioBank bankStrings = new EvioBank(tag+7, DataType.CHARSTAR8, 8);
//            for (int i=0; i < stringData.length; i++) {
//                bankStrings.appendStringData(stringData[i]);
//            }
//            builder.addChild(event, bankStrings);

            // bank of composite data
            CompositeData.Data cData = new CompositeData.Data();
            cData.addShort((short) 1);
            cData.addInt(1);
            cData.addLong(1L);
            cData.addFloat(1);
            cData.addDouble(1.);

            cData.addShort((short) 2);
            cData.addInt(2);
            cData.addLong(2L);
            cData.addFloat(2);
            cData.addDouble(2.);

            CompositeData cd = new CompositeData("S,I,L,F,D", 1, cData, 2, 3);
            System.out.println("CD:\n" +  cd.toString());

            EvioBank bankComposite = new EvioBank(tag+8, DataType.COMPOSITE, 9);
            bankComposite.appendCompositeData(new CompositeData[]{cd});
            builder.addChild(event, bankComposite);

        }
        catch (EvioException e) {
            e.printStackTrace();
        }

        return event;
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

        try {
            EvioEvent bank = createSingleEvent(1);
            int byteSize = bank.getTotalBytes();

            ByteBuffer bb1 = ByteBuffer.allocate(byteSize + 2*(32));
            ByteBuffer bb2 = ByteBuffer.allocate(byteSize + 2*(32));

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
