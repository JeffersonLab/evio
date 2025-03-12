package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;

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
    static      byte[]   byteData   = new byte[]   {(byte)0xff, 0, (byte)0x80};
    static      short[]  shortData  = new short[]  {(short)0xffff, 0, (short)0x8000};
    static      int[]    intData    = new int[]    {0xffffffff, 0, 0x80000000};
    static      long[]   longData   = new long[]   {0xffffffffffffffffL, 0L, 0x8000000000000000L};
    static      float[]  floatData  = new float[]  {Float.MAX_VALUE, 0.F, Float.MIN_VALUE};
    static      double[] doubleData = new double[] {Double.MAX_VALUE, 0., Double.MIN_VALUE};
    static      String[] stringData = new String[] {"123", "456", "789"};




    static CompositeData[] createCompositeData() {
           // Create a CompositeData object ...

        // Format to write a N shorts, 1 float, 1 double a total of N times
        String format1 = "N(NS,F,D)";

        // Now create some data
        CompositeData.Data myData1 = new CompositeData.Data();
        myData1.addN(2);
        myData1.addN(3);
        myData1.addShort(new short[]{1, 2, 3}); // use array for convenience
        myData1.addFloat(Float.MAX_VALUE);
        myData1.addDouble(Double.MAX_VALUE);
        myData1.addN(1);
        myData1.addShort((short) 4); // use array for convenience
        myData1.addFloat(Float.MIN_VALUE);
        myData1.addDouble(Double.MIN_VALUE);

        // ROW 2
        myData1.addN(1);
        myData1.addN(1);
        myData1.addShort((short)4);
        myData1.addFloat(4.0F);
        myData1.addDouble(4.);

        // Format to write an unsigned int, unsigned char, and N number of
        // M (int to be found) ascii characters & 1 64-bit int. We need to
        // wait before we can create this format string because we don't know
        // yet how many String characters (M) we have to determine the "Ma" term.
        // String format2 = "i,c,N(Ma,L)";
        // If an integer replaces "M", it cannot be greater than 15.
        // If M is written as "N", it can be any positive integer.

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

        // Now create some data
        CompositeData.Data myData3 = new CompositeData.Data();

        myData3.addChar(byteData[0]);
        myData3.addChar(byteData[1]);
        myData3.addChar(byteData[2]);

        myData3.addUchar(byteData[0]);
        myData3.addUchar(byteData[1]);
        myData3.addUchar(byteData[2]);

        myData3.addShort(shortData[0]);
        myData3.addShort(shortData[1]);
        myData3.addShort(shortData[2]);

        myData3.addUshort(shortData[0]);
        myData3.addUshort(shortData[1]);
        myData3.addUshort(shortData[2]);

        myData3.addInt(intData[0]);
        myData3.addInt(intData[1]);
        myData3.addInt(intData[2]);

        myData3.addUint(intData[0]);
        myData3.addUint(intData[1]);
        myData3.addUint(intData[2]);

        myData3.addLong(longData[0]);
        myData3.addLong(longData[1]);
        myData3.addLong(longData[2]);

        myData3.addUlong(longData[0]);
        myData3.addUlong(longData[1]);
        myData3.addUlong(longData[2]);

        String format3 = "3C,3c,3S,3s,3I,3i,3L,3l";

//        // Now create some data
//        CompositeData.Data myData4 = new CompositeData.Data();
////        myData4.addInt(88);
////        myData4.addInt(99);
//         // Define our ascii data
//        String ss[] = new String[1];
//        ss[0] = "Reallyreallylongstring";
//        // Find out how big the evio representation of this string is
//        int strLen = BaseStructure.stringsToRawSize(ss);
//        System.out.println("2nd string evio format len = " + strLen);
//        myData4.addN(strLen);
//        myData4.addString(ss);
//        // Format to write a long, and N number of ascii characters.
//        String format4 = "Na";

        // Format to write a N shorts, 1 float, 1 double a total of N times
        String format5 = "N(NS,4I)";

        // Now create some data
        CompositeData.Data myData5 = new CompositeData.Data();
        myData5.addN(2);
        myData5.addN(3);
        myData5.addShort(new short[]{1, 2, 3}); // use array for convenience
        myData5.addInt(1);
        myData5.addInt(2);
        myData5.addInt(3);
        myData5.addInt(4);
        myData5.addN(1);
        myData5.addShort((short) 4); // use array for convenience
        myData5.addInt(3);
        myData5.addInt(4);
        myData5.addInt(5);
        myData5.addInt(6);

        // ROW 2
        myData5.addN(1);
        myData5.addN(1);
        myData5.addShort((short) 4);
        myData5.addInt(5);
        myData5.addInt(6);
        myData5.addInt(7);
        myData5.addInt(8);

        // Format to test how values are written on a line
        String format6 = "D,2D,3D,3F,4F,5F,5S,6S,7S,7C,8C,9C";

        // Now create some data
        CompositeData.Data myData6 = new CompositeData.Data();
        myData6.addDouble(Double.MIN_VALUE);

        myData6.addDouble(0.);
        myData6.addDouble(Double.MAX_VALUE);

        myData6.addDouble(3.);
        myData6.addDouble(3.);
        myData6.addDouble(3.);


        myData6.addFloat((float)3.e-10);
        myData6.addFloat((float)3.e10);
        myData6.addFloat((float)3.e10);

        myData6.addFloat(Float.MIN_VALUE);
        myData6.addFloat((float)0.);
        myData6.addFloat((float)4.e11);
        myData6.addFloat(Float.MAX_VALUE);

        myData6.addFloat(5.F);
        myData6.addFloat(5.F);
        myData6.addFloat(5.F);
        myData6.addFloat(5.F);
        myData6.addFloat(5.F);

        short sh = 5;
        myData6.addShort(sh);
        myData6.addShort(sh);
        myData6.addShort(sh);
        myData6.addShort(sh);
        myData6.addShort(sh);
        sh = 6;
        myData6.addShort(sh);
        myData6.addShort(sh);
        myData6.addShort(sh);
        myData6.addShort(sh);
        myData6.addShort(sh);
        myData6.addShort(sh);
        sh = 7;
        myData6.addShort(sh);
        myData6.addShort(sh);
        myData6.addShort(sh);
        myData6.addShort(sh);
        myData6.addShort(sh);
        myData6.addShort(sh);
        myData6.addShort(sh);

        byte b = 8;
        myData6.addChar(b);
        myData6.addChar(b);
        myData6.addChar(b);
        myData6.addChar(b);
        myData6.addChar(b);
        myData6.addChar(b);
        myData6.addChar(b);
        b = 9;
        myData6.addChar(b);
        myData6.addChar(b);
        myData6.addChar(b);
        myData6.addChar(b);
        myData6.addChar(b);
        myData6.addChar(b);
        myData6.addChar(b);
        myData6.addChar(b);
        b = 10;
        myData6.addChar(b);
        myData6.addChar(b);
        myData6.addChar(b);
        myData6.addChar(b);
        myData6.addChar(b);
        myData6.addChar(b);
        myData6.addChar(b);
        myData6.addChar(b);
        myData6.addChar(b);

        // Create CompositeData array
        CompositeData[] cData = new CompositeData[5];
        try {
            ByteOrder order = ByteOrder.BIG_ENDIAN;
            cData[0] = new CompositeData(format1, 1, myData1, 1, 1, order);
            cData[1] = new CompositeData(format2, 2, myData2, 2, 2, order);
            cData[2] = new CompositeData(format3, 3, myData3, 3, 3, order);
//            cData[3] = new CompositeData(format4, 4, myData4, 4, 4, order);
            cData[3] = new CompositeData(format5, 5, myData5, 5, 5, order);
            cData[4] = new CompositeData(format6, 6, myData6, 6, 6, order);
        }
        catch (EvioException e) {
            e.printStackTrace();
            System.exit(-1);
        }

        return cData;
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

                    // add bank of unsigned ints
                    builder.openBank(tag+2, num+32, DataType.UINT32);
                    builder.addIntData(intData);
                    builder.closeStructure();

                    // add bank of bytes
                    builder.openBank(tag+3, num+3, DataType.CHAR8);
                    builder.addByteData(byteData);
                    builder.closeStructure();

                    // add bank of unsigned bytes
                    builder.openBank(tag+3, num+33, DataType.UCHAR8);
                    builder.addByteData(byteData);
                    builder.closeStructure();

                    // add bank of shorts
                    builder.openBank(tag+4, num+4, DataType.SHORT16);
                    builder.addShortData(shortData);
                    builder.closeStructure();

                    // add bank of unsigned shorts
                    builder.openBank(tag+4, num+34, DataType.USHORT16);
                    builder.addShortData(shortData);
                    builder.closeStructure();

                    // add bank of longs
                    builder.openBank(tag+5, num+5, DataType.LONG64);
                    builder.addLongData(longData);
                    builder.closeStructure();

                    // add bank of unsigned longs
                    builder.openBank(tag+5, num+35, DataType.ULONG64);
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



    /** Create event and swap it twice. */
    public static void main(String args[]) {

        try {

            ByteBuffer buffie = createCompactSingleEvent(1);
            int byteSize = buffie.limit();

            ByteBuffer swappedBuffie = ByteBuffer.allocate(byteSize);
            ByteBuffer origBuffie = ByteBuffer.allocate(byteSize);

            // Take buffer and swap it
            ByteDataTransformer.swapEvent(buffie, swappedBuffie, 0, 0);
            // Take and swap the swapped buffer
            ByteDataTransformer.swapEvent(swappedBuffie, origBuffie, 0, 0);

            boolean goodSwap = true;
            for (int i=0; i < byteSize; i++) {
                if (buffie.get(i) != origBuffie.get(i)) {
                    System.out.println("SwapTest: data differs at index = " + i);
                    goodSwap = false;
                }
            }

            if (goodSwap) {
                System.out.println("SwapTest: double swap successful!!");
            }
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }

}
