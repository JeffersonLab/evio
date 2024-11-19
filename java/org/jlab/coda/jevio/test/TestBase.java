//
// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100

package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;


public class TestBase {


    int[]       int1;
    byte[]     byte1;
    short[]   short1;
    long[]     long1;
    float[]   float1;
    double[] double1;
    String[] string1;
    CompositeData[] cData;


    void setDataSize(int elementCount) {

        int1    = new int[elementCount];
        byte1   = new byte[elementCount];
        short1  = new short[elementCount];
        long1   = new long[elementCount];
        float1  = new float[elementCount];
        double1 = new double[elementCount];
        string1 = new String[elementCount];
        cData   = new CompositeData[elementCount];


        for (int i = 0; i < elementCount; i++) {
            int1[i]    = i + 1;
            byte1[i]   = (byte) ((i + 1) % 255);
            short1[i]  = (short) ((i + 1) % 16384);
            long1[i]   = i + 1;
            float1[i]  = (float) (i + 1);
            double1[i] = (double) (i + 1);
            string1[i] = ("0x" + (i + 1));

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
            try {
                cData[i] = new CompositeData(format, 1, myData,1, 1, order);
            }
            catch (EvioException e) {/* never happen */}
        }
    }


    int runLoops = 1;
    int bufferLoops = 1;
    int dataElementCount = 3;
    int skip = 0;
    int bufSize = 200000;


    ByteBuffer buffer;

    // files for input & output
    String writeFileName1 = "./compactEvioBuild.ev";
    String writeFileName0 = "./compactEvioBuildOld.ev";
    String writeFileName2 = "./rawEvioStructure.ev";

    ByteOrder order = ByteOrder.nativeOrder();

    String dictionary;



    public TestBase() {
        this(200000, ByteOrder.nativeOrder());
    }


    /**
     * Put boiler plate code for doing tests here.
     * The evio events created by all methods have the same structure and data.
     * In other words, they're identical.
     *
     * @param bufSize size in bytes of internal ByteBuffer.
     * @param byteOrder byte order of internal ByteBuffer.
     */
    public TestBase(int bufSize, ByteOrder byteOrder) {

        order = byteOrder;
        buffer = ByteBuffer.allocate(bufSize);
        buffer.order(order);

        boolean printOut = false;

        if (printOut) {
            System.out.println("Running with:");
            System.out.println(" data elements = " + dataElementCount);
            System.out.println("       bufSize = " + bufSize);
            System.out.println("         loops = " + bufferLoops);
            System.out.println("          runs = " + runLoops);
        }

        setDataSize(dataElementCount);

        // Warning: some of the following dictionary entries are based on the event created given
        // tag = 1, num = 1 in the args.


        dictionary =
                "<xmlDict>\n" +
                "<bank name=\"Top\"                tag=\"1\"  num=\"1\" type=\"bank\" >\n" +
                "  <bank name=\"2ndLevel\"         tag=\"201-203\"      type=\"bank\" >\n" +
                "      <description format=\"New Format\" >example_tag_range</description>\n" +
                "      <leaf name=\"BankUints\"     tag=\"3\" num=\"3\" />\n" +
                "      <leaf name=\"SegInts\"       tag=\"9\"  />\n" +
                "      <leaf name=\"TagSegUints\"   tag=\"17\" />\n" +
                "      <leaf name=\"InBank\"        tag=\"3-8\" />\n" +
                "      <leaf name=\"InSeg\"         tag=\"9-14\" />\n" +
                "      <leaf name=\"InTagSeg\"      tag=\"18-22\" />\n" +
                "  </bank >\n" +
                "</bank >\n" +
                "  <dictEntry name=\"CompositeData\" tag=\"101\"  type=\"composite\" />\n" +
                "  <dictEntry name=\"JUNK\" tag=\"4\" num=\"4\" />\n" +
                "  <dictEntry name=\"SEG5\" tag=\"5\" >\n" +
                "       <description format=\"Old Format\" >tag 5 description</description>\n" +
                "  </dictEntry>\n" +
                "</xmlDict>";


        if (printOut) System.out.println("Const: dictionary = \n" + dictionary);
    }



    /**
     * Create a test Evio Event in ByteBuffer form using CompactEventBuilder.
     * @param tag tag of event.
     * @param num num of event.
     * @param byteOrder byte order of resulting ByteBuffer.
     * @param bSize if builder is null, size of internal buf to create
     * @param builder object to build EvioEvent with, if null, create in method.
     * @return ByteBuffer containing EvioEvent.
     * @throws EvioException error in CompactEventBuilder object.
     */
    ByteBuffer createCompactEventBuffer(int tag, int num, ByteOrder byteOrder,
                                        int bSize, CompactEventBuilder builder) throws EvioException {

        if (builder == null) {
            ByteBuffer buf = ByteBuffer.allocate(bSize);
            buf.order(byteOrder);
            builder = new CompactEventBuilder(buf);
        }

        // add top/event level bank of banks
        builder.openBank(tag, num, DataType.BANK);

        // add bank of banks
        builder.openBank(tag + 200, num + 200, DataType.BANK);

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
        builder.addCompositeData(cData);
        builder.closeStructure();

        builder.closeStructure();




        // add bank of segs
        builder.openBank(tag + 201, num + 201, DataType.SEGMENT);

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
        builder.openBank(tag + 202, num + 202, DataType.TAGSEGMENT);

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




    /** Writing to a buffer using EventBuilder interface. */
    ByteBuffer createEventBuilderBuffer(int tag, int num,
                                         ByteOrder byteOrder, int bSize) {

        ByteBuffer buf = ByteBuffer.allocate(bSize);
        buf.order(byteOrder);

        EvioEvent event = createEventBuilderEvent(tag, num);
        // Take event & write it into buffer
        event.write(buf);
        buf.flip();
        return buf;
    }


    /** Build an event with an EventBuilder. */
    EvioEvent createEventBuilderEvent(int tag, int num) {

        // Top level event
        EvioEvent event = null;

        try {
            // Build event (bank of banks) with EventBuilder object
            EventBuilder builder = new EventBuilder(tag, DataType.BANK, num);
            event = builder.getEvent();


            // bank of banks
            EvioBank bankBanks = new EvioBank(tag+200, DataType.BANK, num+200);
            builder.addChild(event, bankBanks);

            // bank of ints
            EvioBank bankInts = new EvioBank(tag+2, DataType.UINT32, num+2);
            builder.setIntData(bankInts, int1);
            builder.addChild(bankBanks, bankInts);

            // bank of unsigned bytes
            EvioBank bankUBytes = new EvioBank(tag+3, DataType.UCHAR8, num+3);
            builder.setByteData(bankUBytes, byte1);
            builder.addChild(bankBanks, bankUBytes);

            // bank of unsigned shorts
            EvioBank bankUShorts = new EvioBank(tag+4, DataType.USHORT16, num+4);
            builder.setShortData(bankUShorts, short1);
            builder.addChild(bankBanks, bankUShorts);

            // bank of unsigned longs
            EvioBank bankULongs = new EvioBank(tag+40, DataType.ULONG64, num+40);
            builder.setLongData(bankULongs, long1);
            builder.addChild(bankBanks, bankULongs);

            // bank of floats
            EvioBank bankFloats = new EvioBank(tag+5, DataType.FLOAT32, num+5);
            builder.setFloatData(bankFloats, float1);
            builder.addChild(bankBanks, bankFloats);

            // bank of doubles
            EvioBank bankDoubles = new EvioBank(tag+6, DataType.DOUBLE64, num+6);
            builder.appendDoubleData(bankDoubles, double1);
            builder.addChild(bankBanks, bankDoubles);

            // bank of strings
            EvioBank bankStrings = new EvioBank(tag+7, DataType.CHARSTAR8, num+7);
            builder.setStringData(bankStrings, string1);
            builder.addChild(bankBanks, bankStrings);

            // bank of composite data array
            EvioBank bankComps = new EvioBank(tag+100, DataType.COMPOSITE, num+100);
            builder.appendCompositeData(bankComps, cData);
            builder.addChild(bankBanks, bankComps);




            // Bank of segs
            EvioBank bankBanks2 = new EvioBank(tag+201, DataType.SEGMENT, num+201);
            builder.addChild(event, bankBanks2);

            // segment of ints
            EvioSegment segInts = new EvioSegment(tag+8, DataType.INT32);
            builder.appendIntData(segInts, int1);
            builder.addChild(bankBanks2, segInts);

            // segment of bytes
            EvioSegment segBytes = new EvioSegment(tag+9, DataType.CHAR8);
            builder.appendByteData(segBytes, byte1);
            builder.addChild(bankBanks2, segBytes);

            // segment of shorts
            EvioSegment segShorts = new EvioSegment(tag+10, DataType.SHORT16);
            builder.appendShortData(segShorts, short1);
            builder.addChild(bankBanks2, segShorts);

            // segment of longs
            EvioSegment segLongs = new EvioSegment(tag+40, DataType.LONG64);
            builder.appendLongData(segLongs, long1);
            builder.addChild(bankBanks2, segLongs);

            // segment of floats
            EvioSegment segFloats = new EvioSegment(tag+11, DataType.FLOAT32);
            builder.appendFloatData(segFloats, float1);
            builder.addChild(bankBanks2, segFloats);

            // segment of doubles
            EvioSegment segDoubles = new EvioSegment(tag+12, DataType.DOUBLE64);
            builder.appendDoubleData(segDoubles, double1);
            builder.addChild(bankBanks2, segDoubles);

            // segment of strings
            EvioSegment segStrings = new EvioSegment(tag+13, DataType.CHARSTAR8);
            builder.appendStringData(segStrings, string1);
            builder.addChild(bankBanks2, segStrings);




            // Bank of tag segs
            EvioBank bankBanks4 = new EvioBank(tag+202, DataType.TAGSEGMENT, num+202);
            builder.addChild(event, bankBanks4);

            // tag segment of ints
            EvioTagSegment tagSegInts = new EvioTagSegment(tag+16, DataType.UINT32);
            builder.appendIntData(tagSegInts, int1);
            builder.addChild(bankBanks4, tagSegInts);

            // tag segment of bytes
            EvioTagSegment tagSegBytes = new EvioTagSegment(tag+17, DataType.UCHAR8);
            builder.appendByteData(tagSegBytes, byte1);
            builder.addChild(bankBanks4, tagSegBytes);

            // tag segment of shorts
            EvioTagSegment tagSegShorts = new EvioTagSegment(tag+18, DataType.USHORT16);
            builder.appendShortData(tagSegShorts, short1);
            builder.addChild(bankBanks4, tagSegShorts);

            // tag seg of longs
            EvioTagSegment tagSegLongs = new EvioTagSegment(tag+40, DataType.ULONG64);
            builder.appendLongData(tagSegLongs, long1);
            builder.addChild(bankBanks4, tagSegLongs);

            // segment of floats
            EvioTagSegment tagSegFloats = new EvioTagSegment(tag+19, DataType.FLOAT32);
            builder.appendFloatData(tagSegFloats, float1);
            builder.addChild(bankBanks4, tagSegFloats);

            // segment of doubles
            EvioTagSegment tagSegDoubles = new EvioTagSegment(tag+20, DataType.DOUBLE64);
            builder.appendDoubleData(tagSegDoubles, double1);
            builder.addChild(bankBanks4, tagSegDoubles);

            // segment of strings
            EvioTagSegment tagSegStrings = new EvioTagSegment(tag+21, DataType.CHARSTAR8);
            builder.appendStringData(tagSegStrings, string1);
            builder.addChild(bankBanks4, tagSegStrings);
        }
        catch (EvioException e) {
            e.printStackTrace();
        }

        return event;
    }



    /** Writing to a buffer using EventBuilder interface. */
    ByteBuffer createTreeBuffer(int tag, int num,
                                ByteOrder byteOrder,
                                int bSize) {

        ByteBuffer buf = ByteBuffer.allocate(bSize);
        buf.order(byteOrder);

        EvioEvent event = createTreeEvent(tag, num);
        // Take event & write it into buffer
        event.write(buf);
        buf.flip();
        return buf;
    }



    /** Writing to a buffer using original evio tree interface. */
    EvioEvent createTreeEvent(int tag, int num) {

        try {
            // Use event constructor and insert() calls
            EvioEvent event = new EvioEvent(tag, DataType.BANK, num);

            // bank of banks
            EvioBank bankBanks = new EvioBank(tag + 200, DataType.BANK, num + 200);
            event.insert(bankBanks, 0);

            // bank of ints
            EvioBank bankInts = new EvioBank(tag + 2, DataType.UINT32, num + 2);
            bankInts.setIntData(int1);
            bankBanks.insert(bankInts, 0);

            // bank of unsigned bytes
            EvioBank bankUBytes = new EvioBank(tag+3, DataType.UCHAR8, num+3);
            bankUBytes.setByteData(byte1);
            bankBanks.insert(bankUBytes, 1);

            // bank of unsigned shorts
            EvioBank bankUShorts = new EvioBank(tag+4, DataType.USHORT16, num+4);
            bankUShorts.setShortData(short1);
            bankBanks.insert(bankUShorts, 2);

            // bank of unsigned longs
            EvioBank bankULongs = new EvioBank(tag+40, DataType.ULONG64, num+40);
            bankULongs.setLongData(long1);
            bankBanks.insert(bankULongs, 3);

            // bank of floats
            EvioBank bankFloats = new EvioBank(tag+5, DataType.FLOAT32, num+5);
            bankFloats.setFloatData(float1);
            bankBanks.insert(bankFloats, 4);

            // bank of doubles
            EvioBank bankDoubles = new EvioBank(tag+6, DataType.DOUBLE64, num+6);
            bankDoubles.appendDoubleData(double1);
            bankBanks.insert(bankDoubles, 5);

            // bank of strings
            EvioBank bankStrings = new EvioBank(tag+7, DataType.CHARSTAR8, num+7);
            bankStrings.setStringData(string1);
            bankBanks.insert(bankStrings, 6);

            // bank of composite data array
            EvioBank bankComps = new EvioBank(tag+100, DataType.COMPOSITE, num+100);
            bankComps.appendCompositeData(cData);
            bankBanks.insert(bankComps, 7);



            // Bank of segs
            EvioBank bankBanks2 = new EvioBank(tag+201, DataType.SEGMENT, 201);
            event.insert(bankBanks2, 1);

            // segment of ints
            EvioSegment segInts = new EvioSegment(tag+8, DataType.INT32);
            segInts.appendIntData(int1);
            bankBanks2.insert(segInts, 0);

            // segment of bytes
            EvioSegment segBytes = new EvioSegment(tag+9, DataType.CHAR8);
            segBytes.appendByteData(byte1);
            bankBanks2.insert(segBytes, 1);

            // segment of shorts
            EvioSegment segShorts = new EvioSegment(tag+10, DataType.SHORT16);
            segShorts.appendShortData(short1);
            bankBanks2.insert(segShorts, 2);

            // segment of longs
            EvioSegment segLongs = new EvioSegment(tag+40, DataType.LONG64);
            segLongs.appendLongData(long1);
            bankBanks2.insert(segLongs, 3);

            // segment of floats
            EvioSegment segFloats = new EvioSegment(tag+11, DataType.FLOAT32);
            segFloats.appendFloatData(float1);
            bankBanks2.insert(segFloats, 4);

            // segment of doubles
            EvioSegment segDoubles = new EvioSegment(tag+12, DataType.DOUBLE64);
            segDoubles.appendDoubleData(double1);
            bankBanks2.insert(segDoubles, 5);

            // segment of strings
            EvioSegment segStrings = new EvioSegment(tag+13, DataType.CHARSTAR8);
            segStrings.appendStringData(string1);
            bankBanks2.insert(segStrings, 5);



            // Bank of tag segs
            EvioBank bankBanks4 = new EvioBank(tag+202, DataType.TAGSEGMENT, 202);
            event.insert(bankBanks4, 2);

            // tag segment of ints
            EvioTagSegment tagSegInts = new EvioTagSegment(tag+16, DataType.UINT32);
            tagSegInts.appendIntData(int1);
            bankBanks4.insert(tagSegInts, 0);

            // tag segment of bytes
            EvioTagSegment tagSegBytes = new EvioTagSegment(tag+17, DataType.UCHAR8);
            tagSegBytes.appendByteData(byte1);
            bankBanks4.insert(tagSegBytes, 1);

            // tag segment of shorts
            EvioTagSegment tagSegShorts = new EvioTagSegment(tag+18, DataType.USHORT16);
            tagSegShorts.appendShortData(short1);
            bankBanks4.insert(tagSegShorts, 2);

            // tag seg of longs
            EvioTagSegment tagSegLongs = new EvioTagSegment(tag+40, DataType.ULONG64);
            tagSegLongs.appendLongData(long1);
            bankBanks4.insert(tagSegLongs, 3);

            // segment of floats
            EvioTagSegment tagSegFloats = new EvioTagSegment(tag+19, DataType.FLOAT32);
            tagSegFloats.appendFloatData(float1);
            bankBanks4.insert(tagSegFloats, 4);

            // segment of doubles
            EvioTagSegment tagSegDoubles = new EvioTagSegment(tag+20, DataType.DOUBLE64);
            tagSegDoubles.appendDoubleData(double1);
            bankBanks4.insert(tagSegDoubles, 5);

            // segment of strings
            EvioTagSegment tagSegStrings = new EvioTagSegment(tag+21, DataType.CHARSTAR8);
            tagSegStrings.appendStringData(string1);
            bankBanks4.insert(tagSegStrings, 6);

            event.setAllHeaderLengths();

            return event;
        }
        catch (EvioException e) {
            e.printStackTrace();
        }

        return null;
    }


}
