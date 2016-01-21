package org.jlab.coda.jevio.test;


import org.jlab.coda.jevio.DataType;
import org.jlab.coda.jevio.EvioBank;

import java.util.Arrays;

/**
 * Test program made to test the method which unpacks a byte array into string data.
 *
 * @author timmer
 * Date: Dec 17, 2015
 */
public class UnpackStringTest {

    /* e, v, i, o, null, 4, 4, 4*/
    static private byte[] good1 = new byte[] {(byte)101, (byte)118, (byte)105, (byte)111,
                                              (byte)0,   (byte)4,   (byte)4,   (byte)4};

    /* e, v, i, o, null, H, E, Y, H, O, null, 4*/
    static private byte[] good2 = new byte[] {(byte)101, (byte)118, (byte)105, (byte)111,
                                              (byte)0,   (byte)72,  (byte)69,  (byte)89,
                                              (byte)72,  (byte)79,  (byte)0,   (byte)4};

    /* e, v, i, o, null, x, y, z*/
    static private byte[] good3 = new byte[] {(byte)101, (byte)118, (byte)105, (byte)111,
                                              (byte)0,   (byte)120, (byte)121, (byte)122};

    /* e, v, i, o, -, x, y, z*/
    static private byte[] bad1 = new byte[] {(byte)101, (byte)118, (byte)105, (byte)111,
                                             (byte)45,  (byte)120, (byte)121, (byte)122};

    /* e, v, i, o, 3, 4, 4, 4*/  /* bad format 1 */
    static private byte[] bad2  = new byte[] {(byte)101, (byte)118, (byte)105, (byte)111,
                                              (byte)3,   (byte)4,   (byte)4,   (byte)4};

    /* e, v, i, o, 0, 4, 3, 3, 3, 3, 3, 4*/  /* bad format 2 */
    static private byte[] bad3  = new byte[] {(byte)101, (byte)118, (byte)105, (byte)111,
                                              (byte)0,   (byte)4,   (byte)3,   (byte)3,
                                              (byte)3,   (byte)3,   (byte)3,   (byte)4};

     /* e, v, i, o, null, H, E, Y, H, O, 3, 4*/  /* bad format 4 */
    static private byte[] bad4  = new byte[] {(byte)101, (byte)118, (byte)105, (byte)111,
                                              (byte)0,   (byte)72,  (byte)69,  (byte)89,
                                              (byte)72,  (byte)79,  (byte)3,   (byte)4};

    /* e, v, i, o, null, 4, 4, 4*/  /* bad format 3 */
    static private byte[] bad5 = new byte[] {(byte)101, (byte)118, (byte)105, (byte)111,
                                              (byte)0,   (byte)4,   (byte)3,   (byte)4};

    /* whitespace chars - only printing ws chars are 9,10,13*/
    static private byte[] ws = new byte[] {(byte)9,   (byte)10,  (byte)11,  (byte)12,
                                           (byte)13,  (byte)28,  (byte)29,  (byte)30,
                                           (byte)31};

    /* e, v, i, o, ws, ws, ws, ws, ws, ws, ws, ws, null, 4, 4 (test whitespace chars) */
    static private byte[] bad6 = new byte[] {(byte)101, (byte)118, (byte)105, (byte)111,
                                              (byte)9,   (byte)10,  (byte)11,  (byte)12,
                                              (byte)13,  (byte)28,  (byte)29,  (byte)30,
                                              (byte)31,  (byte)0,   (byte)4,   (byte)4};

    /* e, 0, 4*/  /* too small */
    static private byte[] bad7 = new byte[] {(byte)101, (byte)0,   (byte)4};




    /** Test unpackRawBytes method. */
    public static void main(String args[]) {

// Whitespace test
//        for (byte b : ws) {
//            System.out.println(b+"--" + ((char)b) + "--");
//        }

        // To do this test, various methods used below need to be made public temporarily
        // and uncommented below.

        try {
            EvioBank bank = new EvioBank(1, DataType.CHARSTAR8, 2);
            bank.setRawBytes(bad7);
            //bank.unpackRawBytesToStrings();
            String[] strings = new String[] {"hey", "ho"};
            bank.appendStringData(strings);

            String[] sa = bank.getStringData();
            int i = 1;
            if (sa != null) {
                System.out.println("Decoded strings:");
                for (String s : sa) {
                    System.out.print("string " + (i++) + ": ");
                    System.out.println(s);
                }
            }
            else {
                System.out.println("No decoded strings");
            }
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }


    /** Compare performance of unpackRawBytes method between old and new versions. */
    public static void main1(String args[]) {

        byte[] b = new byte[400];
        Arrays.fill(b, 0, 396, (byte)101);
        b[396] = (byte)0;
        b[397] = (byte)4;
        b[398] = (byte)4;
        b[399] = (byte)4;

        EvioBank bank = new EvioBank(1, DataType.CHARSTAR8, 2);
        bank.setRawBytes(b);

        long t2, t1 = System.currentTimeMillis();

        for (int i=0; i < 2000000; i++) {
            // make public first, then uncomment next line
            //bank.unpackRawBytesToStrings();
        }

        t2 = System.currentTimeMillis();

        System.out.println("Time = " + (t2 - t1) + " millisec");

    }



}
