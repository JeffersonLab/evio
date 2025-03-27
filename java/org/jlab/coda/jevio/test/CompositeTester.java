package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.util.List;

/**
 * Created by IntelliJ IDEA.
 * User: timmer
 * Date: 4/12/11
 * Time: 10:21 AM
 * To change this template use File | Settings | File Templates.
 */
public class CompositeTester {



    /** For testing only */
    public static void test1() {


        int[] bank = new int[24];

        //**********************/
        // bank of tagsegments */
        //**********************/
        bank[0] = 23;                       // bank length
        bank[1] = 6 << 16 | 0xF << 8 | 3;   // tag = 6, bank contains composite type, num = 3

        // Here follows the actual CompositeData element stored in the bank

        // N(I,D,F,2S,8a)
        // first part of composite type (for format) = tagseg (tag & type ignored, len used)
        bank[2]  = 5 << 20 | 0x3 << 16 | 4; // tag = 5, seg has char data, len = 4
        // ASCII chars values in latest evio string (array) format, N(I,D,F,2S,8a) with N=2
        bank[3]  = 0x4E << 24 | 0x28 << 16 | 0x49 << 8 | 0x2C;    // N ( I ,
        bank[4]  = 0x44 << 24 | 0x2C << 16 | 0x46 << 8 | 0x2C;    // D , F ,
        bank[5]  = 0x32 << 24 | 0x53 << 16 | 0x2C << 8 | 0x38 ;   // 2 S , 8
        bank[6]  = 0x61 << 24 | 0x29 << 16 | 0x00 << 8 | 0x04 ;   // a ) \0 \4

        // second part of composite type (for data) = bank (tag, num, type ignored, len used)
        bank[7]  = 16;
        bank[8]  = 6 << 16 | 0xF << 8 | 1;
        bank[9]  = 0x2; // N
        bank[10] = 0x00001111; // I
        // Double
        double d = Math.PI * (-1.e-100);
        long  dl = Double.doubleToLongBits(d);
        bank[11] = (int) (dl >>> 32);    // higher 32 bits
        bank[12] = (int)  dl;            // lower 32 bits
        // Float
        float f = (float)(Math.PI*(-1.e-24));
        int  fi = Float.floatToIntBits(f);
        bank[13] = fi;

        bank[14] = 0x11223344; // 2S

        bank[15]  = 0x48 << 24 | 0x49 << 16 | 0x00 << 8 | 0x48;    // H  I \0  H
        bank[16]  = 0x4F << 24 | 0x00 << 16 | 0x04 << 8 | 0x04;    // 0 \ 0 \4 \4

        // duplicate data
        for (int i=0; i < 7; i++) {
            bank[17+i] = bank[10+i];
        }

        // all composite including headers
        int[] allData = new int[22];
        for (int i=0; i < 22; i++) {
            allData[i] = bank[i+2];
        }

        // analyze format string
        String format = "N(I,D,F,2S,8a)";

        try {
            System.out.println("\n_________ TEST 1 _________\n");

            // change int array into byte array
            byte[] byteArray = ByteDataTransformer.toBytes(allData, ByteOrder.BIG_ENDIAN);

            // wrap bytes in ByteBuffer for ease of printing later
            ByteBuffer buf = ByteBuffer.wrap(byteArray);
            buf.order(ByteOrder.BIG_ENDIAN);


            // print double swapped data
            System.out.println("ORIGINAL DATA:");
            IntBuffer iBuf = buf.asIntBuffer();
            for (int i=0; i < allData.length; i++) {
                System.out.println("     0x"+Integer.toHexString(iBuf.get(i)));
            }
            System.out.println();

            // swap
System.out.println("CALL CompositeData.swapAll()");
            CompositeData.swapAll(byteArray, 0, null, 0, allData.length, ByteOrder.BIG_ENDIAN);

            // print swapped data
            System.out.println("SWAPPED DATA:");
            //iBuf = buf.asIntBuffer();
            for (int i=0; i < allData.length; i++) {
                System.out.println("     0x"+Integer.toHexString(iBuf.get(i)));
            }
            System.out.println();


            // swap again
System.out.println("Call CompositeData.swapAll()");
            CompositeData.swapAll(byteArray, 0, null, 0, allData.length, ByteOrder.LITTLE_ENDIAN);

            // print double swapped data
            System.out.println("DOUBLE SWAPPED DATA:");
            for (int i=0; i < allData.length; i++) {
                System.out.println("     0x"+Integer.toHexString(iBuf.get(i)));
            }
            System.out.println();

            // Check for differences
            System.out.println("CHECK FOR DIFFERENCES:");
            boolean goodSwap = true;
            for (int i=0; i < 22; i++) {
                if (iBuf.get(i) != bank[i+2]) {
                    System.out.println("orig = " + allData[i] + ", double swapped = " + iBuf.get(i) );
                    goodSwap = false;
                }
            }
            System.out.println("good swap = " + goodSwap);

            // Create composite object
            CompositeData cData = new CompositeData(byteArray, ByteOrder.BIG_ENDIAN);
            System.out.println("cData object = " + cData.toString() + "\n\n");

            // print out general data
            System.out.println("format = " + format);
            printCompositeDataObject(cData);

            // use alternative method to print out
            cData.index(0);
            System.out.println("\nNvalue = 0x" + Integer.toHexString(cData.getNValue()));
            System.out.println("Int    = 0x" + Integer.toHexString(cData.getInt()));
            System.out.println("Double = " + cData.getDouble());
            System.out.println("Float  = " + cData.getFloat());
            System.out.println("Short  = 0x" + Integer.toHexString(cData.getShort()));
            System.out.println("Short  = 0x" + Integer.toHexString(cData.getShort()));
            String[] strs = cData.getStrings();
            for (String s : strs) System.out.println("String = " + s);

            // use toString() method to print out
            System.out.println("\ntoXmlString =\n" + cData.toXML(true));

        }
        catch (EvioException e) {
            e.printStackTrace();
        }

    }


    /**
     * Simple example of providing a format string and some data
     * in order to create a CompositeData object.
     */
    public static void test2() {

        // Create a CompositeData object ...
        System.out.println("\n_________ TEST 2 _________\n");




//        // Format to write an int and a string
//        // To get the right format code for the string, use a helper method.
//        // String can be at most 10 characters long, since when converted to
//        // evio format, this will take 12 chars (max from composite data lib/rule is 15).
//        String myString = "stringWhic";
//        String stringFormat = CompositeData.stringsToFormat(new String[] {myString});
//
//        // Put the 2 formats together
//        String format = "I," + stringFormat;
//
//        System.out.println("format = " + format);
//
//        // Now create some data
//        CompositeData.Data myData = new CompositeData.Data();
//        myData.addInt(2);
//        // Underneath, the string is converted to evio format for string array
//        myData.addString(myString);



        // Format to write an int and a string
        // To get the right format code for the string, use a helper method.
        // All Strings together (including 1 between each element of array)
        // can be at most 10 characters long, since when converted to
        // evio format, this will take 12 chars (max from composite data lib/rule is 15).
        String myString1 = "st1__";
        String myString2 = "st2_";
        String[] both = new String[2];
        both[0] = myString1;
        both[1] = myString2;
        String stringFormat = CompositeData.stringsToFormat(both);

        // Put the 2 formats together
        String format = "I," + stringFormat;

        System.out.println("Array of two strings:\n");
        System.out.println("format = " + format);

        // Now create some data
        CompositeData.Data myData = new CompositeData.Data();
        myData.addInt(2);
        // Underneath, the string is converted to evio format for string array
        myData.addString(both);

        // Create CompositeData object
        CompositeData cData = null;
        try {
            cData = new CompositeData(format, 1, myData, 0 ,0,
                    ByteOrder.BIG_ENDIAN);
        }
        catch (EvioException e) {
            e.printStackTrace();
            System.exit(-1);
        }

        // Print it out
        printCompositeDataObject(cData);


        // Format to write an int and a string
        // To get the right format code for the string, use a helper method.
        // An array of strings, when treated as a single item
        // in the format, can be at most 10 characters long.
        // To get around this restriction, each string must be
        // treated as its own entry in the format when.
        myString1 = "stringOf10";
        myString2 = "another_10";
        String stringFormat1 = CompositeData.stringsToFormat(new String[] {myString1});
        String stringFormat2 = CompositeData.stringsToFormat(new String[] {myString2});

        // Put the 2 formats together
        format = "I," + stringFormat1 + "," + stringFormat2;

        System.out.println("\n\nTwo strings separately:\n");
        System.out.println("format = " + format);

        // Now create some data
        myData = new CompositeData.Data();
        myData.addInt(2);
        // Underneath, the string is converted to evio format for string array
        myData.addString(myString1);
        myData.addString(myString2);

        // Create CompositeData object
        try {
            cData = new CompositeData(format, 1, myData, 0 ,0,
                    ByteOrder.BIG_ENDIAN);
        }
        catch (EvioException e) {
            e.printStackTrace();
            System.exit(-1);
        }

        // Print it out
        printCompositeDataObject(cData);
    }


    /**
     * More complicated example of providing a format string and some data
     * in order to create a CompositeData object.
     */
    public static void test3() {

        System.out.println("\n_________ TEST 3 _________\n");
        // Create a CompositeData object ...

        // Format to write a m shorts, 1 float, 1 double a total of N times
        String format = "N(mS,F,D)";

        System.out.println("format = " + format);

        // Now create some data (in the proper order!)
        // This has a padding of 2 bytes.
        CompositeData.Data myData = new CompositeData.Data();
        myData.addN(2);
        myData.addm((byte)1);
        myData.addShort(new short[] {1}); // use array for convenience
        myData.addFloat(1.0F);
        myData.addDouble(Math.PI);
        myData.addm((byte)1);
        myData.addShort((short)4); // use array for convenience
        myData.addFloat(2.0F);
        myData.addDouble(2.*Math.PI);

        // Create CompositeData object
        CompositeData cData = null;
        try {
            cData = new CompositeData(format, 1, myData, 1 ,0,
                    ByteOrder.LITTLE_ENDIAN);
        }
        catch (EvioException e) {
            e.printStackTrace();
            System.exit(-1);
        }



        // Now create more data
        // This has a padding of 3 bytes.
        CompositeData.Data myData2 = new CompositeData.Data();
        myData2.addN(1);
        myData2.addm((byte)2);
        myData2.addShort(new short[] {1,2}); // use array for convenience
        myData2.addFloat(1.0F);
        myData2.addDouble(Math.PI);

        // Create CompositeData object
        CompositeData cData2 = null;
        try {
            cData2 = new CompositeData(format, 2, myData2, 2 ,0,
                    ByteOrder.LITTLE_ENDIAN);
        }
        catch (EvioException e) {
            e.printStackTrace();
            System.exit(-1);
        }


        String format3 = "N(NS,F,D)";

        System.out.println("format3 = " + format3);

        // Now create more data
        // This has a padding of 3 bytes.
        CompositeData.Data myData3 = new CompositeData.Data();
        myData3.addN(1);
        myData3.addN((byte)2);
        myData3.addShort(new short[] {1,2}); // use array for convenience
        myData3.addFloat(1.0F);
        myData3.addDouble(Math.PI);

        // Create CompositeData object
        CompositeData cData3 = null;
        try {
            cData3 = new CompositeData(format3, 3, myData3, 3 ,0,
                    ByteOrder.LITTLE_ENDIAN);
        }
        catch (EvioException e) {
            e.printStackTrace();
            System.exit(-1);
        }


        // Print it out
        System.out.println("1st composite data item:");
        printCompositeDataObject(cData);
//        System.out.println("\n2nd composite data item:");
//        printCompositeDataObject(cData2);
        System.out.println("\n3rd composite data item:");
        printCompositeDataObject(cData2);

        try {
            EvioEvent ev = new EvioEvent(0, DataType.COMPOSITE, 0);
            // This MUST BE THE SAME ENDIAN as EventWriter!
            ev.setByteOrder(ByteOrder.BIG_ENDIAN);
            ev.appendCompositeData(new CompositeData[] {cData,cData2});

            System.out.println("\nDOUBLE SWAP:");
            byte[] src = ev.getRawBytes();
            if (src == null) {
                System.out.println("raw bytes is NULL !!! ");
            }
            else {
                int srcLen = src.length;
                byte[] srcOrig = new byte[srcLen];
                System.arraycopy(src, 0, srcOrig, 0, srcLen);
                byte[] dest = new byte[srcLen];

                // Both methods below are tested and work
                if (true) {
                    ByteBuffer srcOrigBuffer = ByteBuffer.wrap(srcOrig);
                    srcOrigBuffer.order(ByteOrder.BIG_ENDIAN);
                    ByteBuffer destBuffer = ByteBuffer.wrap(dest);
                    destBuffer.order(ByteOrder.LITTLE_ENDIAN);
                    System.out.println("swap #1 buffer");
                    CompositeData.swapAll(srcOrigBuffer, destBuffer,
                            0, 0, srcLen/4, false);

                    System.out.println("swap #2 buffer");
                    CompositeData.swapAll(destBuffer, srcOrigBuffer,
                            0, 0, srcLen/4, false);
                }
                else {
                    ByteOrder origOrder = ev.getByteOrder();
                    ByteOrder swappedOrder = origOrder == ByteOrder.BIG_ENDIAN ? ByteOrder.LITTLE_ENDIAN : ByteOrder.BIG_ENDIAN;
                    System.out.println("swap #1");
                    CompositeData.swapAll(src, 0, dest, 0, srcLen / 4, ev.getByteOrder());
                    System.out.println("swap #2");
                    CompositeData.swapAll(dest, 0, src, 0, srcLen / 4, swappedOrder);
                }

                for (int i = 0; i < srcLen; i++) {
                    if (src[i] != srcOrig[i]) {
                        System.out.println("Double swapped item is different at pos " + i);
                    }
                }
                System.out.println("DOUBLE SWAP DONE");

            }

            // An event containing composite data types should never have padding != 0.
            // This is because the composite type is tagseg followed by a bank, both of
            // which are defined to end on 4 byte boundaries. However, the tagseg and bank of a
            // single Composite data item each may have a padding that is != 0.
            // In this case, the composite data is defined so it is NOT an integral # of words.

            System.out.println("\n\nEvent containing comp data is " + ev.getTotalBytes() +
                               " bytes, padding = " + ev.getHeader().getPadding());

            // Write it to this file
            String fileName  = "./composite.dat";

            System.out.println("\nWrite above Composite data to file\n");
            EventWriter writer = new EventWriter(fileName, false, ByteOrder.LITTLE_ENDIAN);
            writer.writeEvent(ev);
            writer.close();

            System.out.println("Read file and print\n");
            EvioReader reader = new EvioReader(fileName);
            EvioEvent evR = reader.parseNextEvent();
            BaseStructureHeader h = evR.getHeader();
            System.out.println("event: tag = " + h.getTag() +
                    ", type = " + h.getDataTypeName() + ", len = " + h.getLength());
            if (evR != null) {
                CompositeData[] cDataR = evR.getCompositeData();
                for (CompositeData cd : cDataR) {
                    System.out.println("\nCD: \n");
                            printCompositeDataObject(cd);
                }
            }
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }




    /**
     * More complicated example of providing a format string and some data
     * in order to create a CompositeData object.
     */
    public static void test4() {

        System.out.println("\n_________ TEST 4 _________\n");


        // Format to write 1 int and 1 float a total of N times
        String format = "N(I,F)";
System.out.println("format = " + format);

        // Now create some data
        CompositeData.Data myData1 = new CompositeData.Data();
        myData1.addN(1);
        myData1.addInt(1); // use array for convenience
        myData1.addFloat(1.0F);

        // Now create some data
        CompositeData.Data myData2 = new CompositeData.Data();
        myData2.addN(1);
        myData2.addInt(2); // use array for convenience
        myData2.addFloat(2.0F);

        // Now create some data
        CompositeData.Data myData3 = new CompositeData.Data();
        myData3.addN(1);
        myData3.addInt(3); // use array for convenience
        myData3.addFloat(3.0F);


System.out.println("Create 3 composite data objects");

        // Create CompositeData object
        CompositeData[] cData = new CompositeData[3];
        try {
            cData[0] = new CompositeData(format, 1, myData1, 1 ,1,
                    ByteOrder.BIG_ENDIAN);
            cData[1] = new CompositeData(format, 2, myData2, 2 ,2,
                    ByteOrder.BIG_ENDIAN);
            cData[2] = new CompositeData(format, 3, myData3, 3 ,3,
                    ByteOrder.BIG_ENDIAN);
        }
        catch (EvioException e) {
            e.printStackTrace();
            System.exit(-1);
        }

        // Print it out
        System.out.println("Print 3 composite data objects");
        printCompositeDataObject(cData[0]);
        printCompositeDataObject(cData[1]);
        printCompositeDataObject(cData[2]);

        System.out.println("  composite data object 1:\n");
        Utilities.printBytes(cData[0].getRawBytes(), 0, cData[0].getRawBytes().length, "RawBytes 1");
        System.out.println("  composite data object 2:\n");
        Utilities.printBytes(cData[1].getRawBytes(), 0, cData[1].getRawBytes().length, "RawBytes 2");
        System.out.println("  composite data object 3:\n");
        Utilities.printBytes(cData[2].getRawBytes(), 0, cData[2].getRawBytes().length, "RawBytes 3");

        try {
            EvioEvent ev = new EvioEvent(0, DataType.COMPOSITE, 0);
            ev.setByteOrder(ByteOrder.BIG_ENDIAN);
            ev.appendCompositeData(cData);

            System.out.println("Print event raw bytes of composite array:\n");
            Utilities.printBytes(ev.getRawBytes(), 0, ev.getRawBytes().length, "Array rawBytes");

            // Write it to this file
            String fileName  = "./composite.dat";

System.out.println("WRITE FILE:");
            EventWriter writer = new EventWriter(fileName, false, ByteOrder.LITTLE_ENDIAN);
            writer.writeEvent(ev);
            writer.close();

            Utilities.printBytes(fileName, 0, 1000, "FILE read back in");


                // Read it from file
System.out.println("READ FILE & PRINT CONTENTS:");
            EvioReader reader = new EvioReader(fileName);
            EvioEvent evR = reader.parseNextEvent();
            BaseStructureHeader h = evR.getHeader();
            System.out.println("event: tag = " + h.getTag() +
                                ", type = " + h.getDataTypeName() + ", len = " + h.getLength());
            if (evR != null) {
                CompositeData[] cDataR = evR.getCompositeData();
                for (CompositeData cd : cDataR) {
                    printCompositeDataObject(cd);
                }
            }

        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }


    /**
     * More complicated example of providing a format string and some data
     * in order to create a CompositeData object.
     */
    public static void main(String args[]) {
        test1();
        test2();
        test3();
        test4();
    }



        /**
         * Print the data from a CompositeData object in a user-friendly form.
         * @param cData CompositeData object
         */
    public static void printCompositeDataObject(CompositeData cData) {

        // Get lists of data items & their types from composite data object
        List<Object> items = cData.getItems();
        List<DataType> types = cData.getTypes();

        // Use these list to print out data of unknown format
        DataType type;
        int len = items.size();

        for (int i=0; i < len; i++) {
            type =  types.get(i);
            System.out.print(String.format("type = %9s, val = ", type));
            switch (type) {
                case NVALUE:
                case INT32:
                case UINT32:
                case UNKNOWN32:
                    int k = (Integer)items.get(i);
                    System.out.println("0x"+Integer.toHexString(k));
                    break;
                case LONG64:
                case ULONG64:
                    long l = (Long)items.get(i);
                    System.out.println("0x"+Long.toHexString(l));
                    break;
                case nVALUE:
                case SHORT16:
                case USHORT16:
                    short s = (Short)items.get(i);
                    System.out.println("0x"+Integer.toHexString(s));
                    break;
                case mVALUE:
                case CHAR8:
                case UCHAR8:
                    byte b = (Byte)items.get(i);
                    System.out.println("0x"+Integer.toHexString(b));
                    break;
                case FLOAT32:
                    float ff = (Float)items.get(i);
                    System.out.println(""+ff);
                    break;
                case DOUBLE64:
                    double dd = (Double)items.get(i);
                    System.out.println(""+dd);
                    break;
                case CHARSTAR8:
                    String[] strs = (String[])items.get(i);
                    for (int j = 0; j < strs.length; j++) {
                        System.out.print(strs[j]);
                        if (j < strs.length - 1) {
                            System.out.print(", ");
                        }
                    }
                    System.out.println();
                    break;
                default:
            }
        }
    }



}
