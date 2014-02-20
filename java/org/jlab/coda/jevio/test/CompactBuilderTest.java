package org.jlab.coda.jevio.test;


import org.jlab.coda.jevio.*;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.List;

/**
 * Test program.
 * @author timmer
 * Feb 14, 2014
 */
public class CompactBuilderTest {

     int[]    int1;
     byte[]   byte1;
     short[]  short1;
     long[]   long1;
     float[]  float1;
     double[] double1;
     String[] string1;

     int runLoops = 2;
     int bufferLoops = 20000;
     int dataElementCount = 3;

     boolean oldEvio = false;
     boolean useBuf = false;

    ByteBuffer buffer;

         // files for input & output
     String writeFileName1 = "/daqfs/home/timmer/coda/compactEvioBuild.ev";
     String writeFileName2 = "/daqfs/home/timmer/coda/compactEvioNode.ev";
     ByteOrder order = ByteOrder.BIG_ENDIAN;


    /**
     * Method to decode the command line used to start this application.
     * @param args command line arguments
     */
    private void decodeCommandLine(String[] args) {

        // loop over all args
        for (int i = 0; i < args.length; i++) {

            if (args[i].equalsIgnoreCase("-h")) {
                usage();
                System.exit(-1);
            }
            else if (args[i].equalsIgnoreCase("-count")) {
                dataElementCount = Integer.parseInt(args[i + 1]);
                i++;
            }
            else if (args[i].equalsIgnoreCase("-loops")) {
                bufferLoops = Integer.parseInt(args[i + 1]);
                i++;
            }
            else if (args[i].equalsIgnoreCase("-runs")) {
                runLoops = Integer.parseInt(args[i + 1]);
                i++;
            }
            else if (args[i].equalsIgnoreCase("-little")) {
                order = ByteOrder.LITTLE_ENDIAN;
            }
            else if (args[i].equalsIgnoreCase("-old")) {
                oldEvio = true;
            }
            else if (args[i].equalsIgnoreCase("-buf")) {
                useBuf = true;
            }
            else {
                usage();
                System.exit(-1);
            }
        }

        return;
    }


    /** Method to print out correct program command line usage. */
    private static void usage() {
        System.out.println("\nUsage:\n\n" +
            "   java CompactBuilderTest\n" +
            "        [-count <elements>]  number of data elements of each type\n"+
            "        [-loops <loops>]     number of times to loop\n" +
            "        [-runs <runs>]       number of runs\n" +
            "        [-little]            use little endian buffer\n" +
            "        [-old]               use old (orig) evio interface\n" +
            "        [-buf]               use buffer (not array) in new interface\n" +
            "        [-h]                 print this help\n");
    }




    public CompactBuilderTest(String args[]) {

        int tag=1, num=1, bufSize=200000;
        byte[] array = new byte[bufSize];
        buffer = ByteBuffer.wrap(array);
        buffer.order(order);
        //ByteBuffer buffer = ByteBuffer.allocateDirect(bufSize);

        decodeCommandLine(args);

        setDataSize(dataElementCount);

        EvioNode node = readFile(writeFileName1, 2, 2);
        if (node != null) {
            insertEvioNode(node, tag, num, useBuf);
            return;
        }

        if (oldEvio) {
            createObjectEvents(tag, num);
        }
        else {
            createCompactEvents(tag, num, useBuf);
        }
    }


    /** For WRITING a local file. */
    public static void main(String args[]) {

        new CompactBuilderTest(args);
    }


     void setDataSize(int elementCount) {

        int1    = new int   [elementCount];
        byte1   = new byte  [elementCount];
        short1  = new short [elementCount];
        long1   = new long  [elementCount];
        float1  = new float [elementCount];
        double1 = new double[elementCount];
        string1 = new String[elementCount];

        for (int i=0; i < elementCount; i++) {
            int1[i]    = i+1;
            byte1[i]   = (byte)  ((i+1) % Byte.MAX_VALUE);
            short1[i]  = (short) ((i+1) % Short.MAX_VALUE);
            long1[i]   = i+1;
            float1[i]  = (float)  (i+1);
            double1[i] = (double) (i+1);
            string1[i] = String.valueOf(i+1);
        }
    }


    public EvioNode readFile(String filename, int tag, int num) {

        List<EvioNode> returnList;
        EvioNode node = null;

        try {
            EvioCompactReader reader = new EvioCompactReader(filename);

            // search event 1 for struct with tag, num
            returnList = reader.searchEvent(1, tag, num);
            if (returnList.size() < 1) {
                System.out.println("GOT NOTHING IN SEARCH for ev 1, tag = " + tag +
                        ", num = " + num);
                return null;
            }
            else {
                System.out.println("Found " + returnList.size() + " structs");
            }

            return returnList.get(0);

        }
        catch (IOException e) {
            e.printStackTrace();
        }
        catch (EvioException e) {
            e.printStackTrace();
        }

        return null;
    }

    /** Writing to a buffer using new interface. */
    public  void insertEvioNode(EvioNode node, int tag, int num, boolean useBuf) {
        try {

            for (int j=0; j<runLoops; j++) {
                long t2, t1 = System.currentTimeMillis();

                for (int i=0; i< bufferLoops; i++) {

                    CompactEventBuilder builder = new CompactEventBuilder(buffer, useBuf);

                    // add top/event level bank of banks
                    builder.openBank(tag, num, DataType.BANK);

//                    // add bank of banks
//                    builder.openBank(tag+1, num+1, DataType.BANK);
//
//                    // add bank of ints
//                    builder.openBank(tag+2, num+2, DataType.INT32);
//                    builder.addIntData(int1);
//                    builder.closeStructure();
//
//                    // add bank of bytes
//                    builder.openBank(tag + 3, num + 3, DataType.CHAR8);
//                    builder.addByteData(byte1);
//                    builder.closeStructure();
//
//                    // add bank of shorts
//                    builder.openBank(tag + 4, num + 4, DataType.SHORT16);
//                    builder.addShortData(short1);
//                    builder.closeStructure();
//
//                    // add bank of longs
//                    builder.openBank(tag + 40, num + 40, DataType.LONG64);
//                    builder.addLongData(long1);
//                    builder.closeStructure();
//
//                    // add bank of floats
//                    builder.openBank(tag+5, num+5, DataType.FLOAT32);
//                    builder.addFloatData(float1);
//                    builder.closeStructure();
//
//                    // add bank of doubles
//                    builder.openBank(tag+6, num+6, DataType.DOUBLE64);
//                    builder.addDoubleData(double1);
//                    builder.closeStructure();
//
//                    // add bank of strings
//                    builder.openBank(tag+7, num+7, DataType.CHARSTAR8);
//                    builder.addStringData(string1);
//                    builder.closeStructure();
//
//                    builder.closeStructure();

                    builder.addEvioNode(node);

                    builder.closeAll();
//                    if (i == 0) builder.toFile(writeFileName2);
                }

                t2 = System.currentTimeMillis();
                System.out.println("Time = " + (t2-t1) + " milliseconds");
            }

        }
        catch (EvioException e) {
            e.printStackTrace();
        }
    }



    /** Writing to a buffer using new interface. */
    public  void createCompactEvents(int tag, int num, boolean useBuf) {
        try {

            for (int j=0; j<runLoops; j++) {
                long t2, t1 = System.currentTimeMillis();

                for (int i=0; i< bufferLoops; i++) {

                    CompactEventBuilder builder = new CompactEventBuilder(buffer, useBuf);

                    // add top/event level bank of banks
                    builder.openBank(tag, num, DataType.BANK);

                    // add bank of banks
                    builder.openBank(tag+1, num+1, DataType.BANK);

                    // add bank of ints
                    builder.openBank(tag+2, num+2, DataType.INT32);
                    builder.addIntData(int1);
                    builder.closeStructure();

                    // add bank of bytes
                    builder.openBank(tag + 3, num + 3, DataType.CHAR8);
                    builder.addByteData(byte1);
                    builder.closeStructure();

                    // add bank of shorts
                    builder.openBank(tag + 4, num + 4, DataType.SHORT16);
                    builder.addShortData(short1);
                    builder.closeStructure();

                    // add bank of longs
                    builder.openBank(tag + 40, num + 40, DataType.LONG64);
                    builder.addLongData(long1);
                    builder.closeStructure();

                    // add bank of floats
                    builder.openBank(tag+5, num+5, DataType.FLOAT32);
                    builder.addFloatData(float1);
                    builder.closeStructure();

                    // add bank of doubles
                    builder.openBank(tag+6, num+6, DataType.DOUBLE64);
                    builder.addDoubleData(double1);
                    builder.closeStructure();

//                    // add bank of strings
//                    builder.openBank(tag+7, num+7, DataType.CHARSTAR8);
//                    builder.addStringData(string1);
//                    builder.closeStructure();

                    builder.closeStructure();


                    // add bank of segs
                    builder.openBank(tag+14, num+14, DataType.SEGMENT);

                    // add seg of ints
                    builder.openSegment(tag+8, DataType.INT32);
                    builder.addIntData(int1);
                    builder.closeStructure();

                    // add seg of bytes
                    builder.openSegment(tag+9, DataType.CHAR8);
                    builder.addByteData(byte1);
                    builder.closeStructure();

                    // add seg of shorts
                    builder.openSegment(tag+10, DataType.SHORT16);
                    builder.addShortData(short1);
                    builder.closeStructure();

                    // add seg of longs
                    builder.openSegment(tag+40, DataType.LONG64);
                    builder.addLongData(long1);
                    builder.closeStructure();

                    // add seg of floats
                    builder.openSegment(tag+11, DataType.FLOAT32);
                    builder.addFloatData(float1);
                    builder.closeStructure();

                    // add seg of doubles
                    builder.openSegment(tag+12, DataType.DOUBLE64);
                    builder.addDoubleData(double1);
                    builder.closeStructure();

//                    // add seg of strings
//                    builder.openSegment(tag+13, DataType.CHARSTAR8);
//                    builder.addStringData(string1);
//                    builder.closeStructure();

                    builder.closeStructure();


                    // add bank of tagsegs
                    builder.openBank(tag+15, num+15, DataType.TAGSEGMENT);

                    // add tagseg of ints
                    builder.openTagSegment(tag + 16, DataType.INT32);
                    builder.addIntData(int1);
                    builder.closeStructure();

                    // add tagseg of bytes
                    builder.openTagSegment(tag + 17, DataType.CHAR8);
                    builder.addByteData(byte1);
                    //builder.addByteData(byte1);
                    builder.closeStructure();

                    // add tagseg of shorts
                    builder.openTagSegment(tag + 18, DataType.SHORT16);
                    builder.addShortData(short1);
                    builder.closeStructure();

                    // add tagseg of longs
                    builder.openTagSegment(tag + 40, DataType.LONG64);
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

//                    // add tagseg of strings
//                    builder.openTagSegment(tag + 21, DataType.CHARSTAR8);
//                    builder.addStringData(string1);
//                    builder.closeStructure();

                    builder.closeAll();
//                    if (i == 0) builder.toFile(writeFileName1);
                }

                t2 = System.currentTimeMillis();
                System.out.println("Time = " + (t2-t1) + " milliseconds");
            }

        }
        catch (EvioException e) {
            e.printStackTrace();
        }
    }


    /** Writing to a buffer using original evio interface. */
    public  void createObjectEvents(int tag, int num) {

        // Top level event
        EvioEvent event;

        try {

            for (int j=0; j<runLoops; j++) {
                long t2, t1 = System.currentTimeMillis();

                for (int i=0; i< bufferLoops; i++) {

                    // Build event (bank of banks) with EventBuilder object
                    EventBuilder builder = new EventBuilder(tag, DataType.BANK, num);
                    event = builder.getEvent();



                    // bank of banks
                    EvioBank bankBanks = new EvioBank(tag+1, DataType.BANK, num+1);
                    builder.addChild(event, bankBanks);

                    // bank of ints
                    EvioBank bankInts = new EvioBank(tag+2, DataType.INT32, num+2);
                    bankInts.appendIntData(int1);
                    //bankInts.appendIntData(int1);
                    builder.addChild(bankBanks, bankInts);

                    // bank of bytes
                    EvioBank bankBytes = new EvioBank(tag+3, DataType.CHAR8, num+3);
                    bankBytes.appendByteData(byte1);
                    //bankBytes.appendByteData(byte1);
                    builder.addChild(bankBanks, bankBytes);

                    // bank of shorts
                    EvioBank bankShorts = new EvioBank(tag+4, DataType.SHORT16, num+4);
                    bankShorts.appendShortData(short1);
                    //bankShorts.appendShortData(short1);
                    builder.addChild(bankBanks, bankShorts);

                    // bank of longs
                    EvioBank bankLongs = new EvioBank(tag+40, DataType.LONG64, num+40);
                    bankLongs.appendLongData(long1);
                    //bankLongs.appendLongData(long1);
                    builder.addChild(bankBanks, bankLongs);

                    // bank of floats
                    EvioBank bankFloats = new EvioBank(tag+5, DataType.FLOAT32, num+5);
                    bankFloats.appendFloatData(float1);
                    //bankFloats.appendFloatData(float1);
                    builder.addChild(bankBanks, bankFloats);

                    // bank of doubles
                    EvioBank bankDoubles = new EvioBank(tag+6, DataType.DOUBLE64, num+6);
                    bankDoubles.appendDoubleData(double1);
                    //bankDoubles.appendDoubleData(double1);
                    builder.addChild(bankBanks, bankDoubles);

//                    // bank of strings
//                    EvioBank bankStrings = new EvioBank(tag+7, DataType.CHARSTAR8, num+7);
//                    for (String s : string1)
//                        bankStrings.appendStringData(s);
//                    //for (String s : string1)
//                    //    bankStrings.appendStringData(s);
//                    builder.addChild(bankBanks, bankStrings);



                    // bank of segments
                    EvioBank bankSegs = new EvioBank(tag+14, DataType.SEGMENT, num+14);
                    builder.addChild(event, bankSegs);

                    // seg of ints
                    EvioSegment segInts = new EvioSegment(tag+8, DataType.INT32);
                    segInts.appendIntData(int1);
                    //segInts.appendIntData(int1);
                    builder.addChild(bankSegs, segInts);

                    // seg of bytes
                    EvioSegment segBytes = new EvioSegment(tag+9, DataType.CHAR8);
                    segBytes.appendByteData(byte1);
                    //segBytes.appendByteData(byte1);
                    builder.addChild(bankSegs, segBytes);

                    // seg of shorts
                    EvioSegment segShorts = new EvioSegment(tag+10, DataType.SHORT16);
                    segShorts.appendShortData(short1);
                    //segShorts.appendShortData(short1);
                    builder.addChild(bankSegs, segShorts);

                    // seg of longs
                    EvioSegment segLongs = new EvioSegment(tag+40, DataType.LONG64);
                    segLongs.appendLongData(long1);
                    //segLongs.appendLongData(long1);
                    builder.addChild(bankSegs, segLongs);

                    // seg of floats
                    EvioSegment segFloats = new EvioSegment(tag+11, DataType.FLOAT32);
                    segFloats.appendFloatData(float1);
                    //segFloats.appendFloatData(float1);
                    builder.addChild(bankSegs, segFloats);

                    // seg of doubles
                    EvioSegment segDoubles = new EvioSegment(tag+12, DataType.DOUBLE64);
                    segDoubles.appendDoubleData(double1);
                    //segDoubles.appendDoubleData(double1);
                    builder.addChild(bankSegs, segDoubles);

//                    // seg of strings
//                    EvioSegment segStrings = new EvioSegment(tag+13, DataType.CHARSTAR8);
//                    for (String s : string1)
//                        segStrings.appendStringData(s);
//                    //for (String s : string1)
//                    //    segStrings.appendStringData(s);
//                    builder.addChild(bankSegs, segStrings);



                    // bank of tagsegments
                    EvioBank bankTsegs = new EvioBank(tag+15, DataType.TAGSEGMENT, num+15);
                    builder.addChild(event, bankTsegs);

                    // tagsegments of ints
                    EvioTagSegment tsegInts = new EvioTagSegment(tag+16, DataType.INT32);
                    tsegInts.appendIntData(int1);
                    //tsegInts.appendIntData(int1);
                    builder.addChild(bankTsegs, tsegInts);

                    // tagsegments of bytes
                    EvioTagSegment tsegBytes = new EvioTagSegment(tag+17, DataType.CHAR8);
                    tsegBytes.appendByteData(byte1);
                    //tsegBytes.appendByteData(byte1);
                    builder.addChild(bankTsegs, tsegBytes);

                    // tagsegments of shorts
                    EvioTagSegment tsegShorts = new EvioTagSegment(tag+18, DataType.SHORT16);
                    tsegShorts.appendShortData(short1);
                    //tsegShorts.appendShortData(short1);
                    builder.addChild(bankTsegs, tsegShorts);

                    // tagsegments of longs
                    EvioTagSegment tsegLongs = new EvioTagSegment(tag+40, DataType.LONG64);
                    tsegLongs.appendLongData(long1);
                    //tsegLongs.appendLongData(long1);
                    builder.addChild(bankTsegs, tsegLongs);

                    // tagsegments of floats
                    EvioTagSegment tsegFloats = new EvioTagSegment(tag+19, DataType.FLOAT32);
                    tsegFloats.appendFloatData(float1);
                    //tsegFloats.appendFloatData(float1);
                    builder.addChild(bankTsegs, tsegFloats);

                    // tagsegments of doubles
                    EvioTagSegment tsegDoubles = new EvioTagSegment(tag+20, DataType.DOUBLE64);
                    tsegDoubles.appendDoubleData(double1);
                    //tsegDoubles.appendDoubleData(double1);
                    builder.addChild(bankTsegs, tsegDoubles);

//                    // tagsegments of strings
//                    EvioTagSegment tsegStrings = new EvioTagSegment(tag+21, DataType.CHARSTAR8);
//                    for (String s : string1)
//                        tsegStrings.appendStringData(s);
//                    //for (String s : string1)
//                    //    tsegStrings.appendStringData(s);
//                    builder.addChild(bankTsegs, tsegStrings);


//
//                    // bank of segments
//                    EvioBank bankSegs2 = new EvioBank(tag+30, DataType.SEGMENT, num+30);
//                    builder.addChild(event, bankSegs2);
//
//                    // seg of tagsegments
//                    EvioSegment segTsegs = new EvioSegment(tag+31, DataType.TAGSEGMENT);
//                    builder.addChild(bankSegs2, segTsegs);
//
//                    // tagsegments of doubles
//                    EvioTagSegment tsegDoubles2 = new EvioTagSegment(tag+32, DataType.DOUBLE64);
//                    tsegDoubles2.appendDoubleData(double1);
//                    builder.addChild(segTsegs, tsegDoubles2);
//

                    // Take objects & write them into buffer
                    event.write(buffer);
                    buffer.clear();
                }

                t2 = System.currentTimeMillis();
                System.out.println("Time = " + (t2-t1) + " milliseconds");
            }
        }
        catch (EvioException e) {
            e.printStackTrace();
        }

    }


}
