package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.*;
import java.nio.channels.FileChannel;

/**
 * Take test programs out of EvioCompactReader.
 * @author timmer
 * Date: Nov 15, 2012
 */
public class CompactReaderBugTest {


    /**
     * Evio version 6 format.
     * Create a record header which has a non-standard length (17 words instead of 14).
     * and contains 4 events.
     * First is bank with 1 char, second has bank with 2 chars,
     * third has bank with 3 chars, and the 4th has 4 chars.
     */
    static int wordData[] = {

            // Deliberately add words, allowed by evio-6/hipo format rules, but not used by evio

            // 17 + 4 + 2 + 4*3 = 35 words (0x21)

            0x00000023, // entire record word len inclusive, 35 words
            0x00000001, // rec #1
            0x00000011, // header word len, inclusive (should always be 14, but set to 17)
            0x00000004, // event count
            0x00000010, // index array len in bytes (4*4 = 16)
            0x00000206, // bit info word, evio version 6, is last record
            0x00000008, // user header byte len, 8
            0xc0da0100, // magic #
            0x00000048, // uncompressed data byte len (16 index + 8 + 4*12 events = 72 or 0x48)
            0x00000000, // compression type (0), compressed length (0)
            0x00000000, // user reg 1
            0x00000001, // user reg 1
            0x00000000, // user reg 2
            0x00000002, // user reg 2
            0x00000000, // extra header word, never normally here
            0x00000000, // extra header word, never normally here
            0x00000000, // extra header word, never normally here

            // array index (length in bytes of each event)
            0xc,
            0xc,
            0xc,
            0xc,

            // user header (should only be here if dictionary or first event defined, which they aren't)
            0x01020304,
            0x04030201,


            // event 1: num=1, tag=1, data type = 8 bit signed int, pad=3 (1 byte valid data)
            0x00000002,
            0x0001c601, // this pos = 100
            0x01020304,

            // event 2: num=2, tag=2, data type = 8 bit signed int, pad=2 (2 bytes valid data)
            0x00000002,
            0x00028602, // this pos = 112
            0x01020304,

            // event 3: num=3, tag=3, data type = 8 bit signed int, pad=1 (3 bytes valid data)
            0x00000002,
            0x00034603, // this pos = 124
            0x01020304,

            // event 4: num=4, tag=4, data type = 8 bit signed int, pad=0 (all 4 bytes are valid data)
            0x00000002,
            0x00040604, // this pos = 136
            0x01020304,
    };


    static int normalData[] = {

            // Normal evio 6 format

            // 14 + 4 + 4*3 = 30 words (0x1e)

            0x0000001e, // entire record word len inclusive, 35 words
            0x00000001, // rec #1
            0x0000000e, // header word len, inclusive (is always 14)
            0x00000004, // event count
            0x00000010, // index array len in bytes (4*4 = 16)
            0x00000206, // bit info word, evio version 6, is last record
            0x00000000, // user header byte len, 0
            0xc0da0100, // magic #
            0x00000040, // uncompressed data byte len (16 index + 4*12 events = 64 or 0x40)
            0x00000000, // compression type (0), compressed length (0)
            0x00000000, // user reg 1
            0x00000001, // user reg 1
            0x00000000, // user reg 2
            0x00000002, // user reg 2

            0xc,
            0xc,
            0xc,
            0xc,

            0x00000002,
            0x0001c601,
            0x01020304,

            0x00000002,
            0x00028602,
            0x01020304,

            0x00000002,
            0x00034603,
            0x01020304,

            0x00000002,
            0x00040604,
            0x01020304,
    };




    public static void ByteBufferTest() {
        byte[] array = new byte[] {(byte)1,(byte)2,(byte)3,(byte)4};

        ByteBuffer bb1 = ByteBuffer.wrap(array);

        System.out.println("Wrapped array: ");
        for (int i=0; i < array.length; i++) {
            System.out.println("array[" + i + "] = " + array[i]);
        }

        ByteBuffer bbDup = bb1.duplicate();
        bbDup.clear();


        System.out.println("\nDuplicate array: ");
        for (int i=0; i < bbDup.remaining(); i++) {
            System.out.println("array[" + i + "] = " + bbDup.get(i));
        }

        bb1.clear();
        ByteBuffer bbSlice = bb1.slice();
        bbSlice.clear();

        System.out.println("\nSlice array: ");
        for (int i=0; i < bbSlice.remaining(); i++) {
            System.out.println("array[" + i + "] = " + bbSlice.get(i));
        }

     //   bbDup.clear();
        bbDup.limit(3).position(1);
        bbSlice = bbDup.slice();
        bbSlice.clear();

        System.out.println("\nSlice of Duplicate array: ");
        for (int i=0; i <  bbSlice.remaining(); i++) {
            System.out.println("array[" + i + "] = " + bbSlice.get(i));
        }
    }




    public static void main(String args[]) {

        try {
            byte[] data = ByteDataTransformer.toBytes(wordData, ByteOrder.BIG_ENDIAN);
            ByteBuffer buf = ByteBuffer.wrap(data);

            EvioCompactReader reader = new EvioCompactReader(buf);
            int evCount = reader.getEventCount();
            for (int i=0; i < evCount; i++) {
                EvioNode node = reader.getEvent(i+1);
System.out.println("\nEvent " + (i+1) + ": tag=" + node.getTag() + ", num=" + node.getNum() +
                   ", dataPos=" + node.getDataPosition() +", type=" + node.getDataTypeObj() +
                   ", pad=" + node.getPad());
System.out.println("    = " + node);

                ByteBuffer bb = node.getByteData(false);
                System.out.println("Buf: limit = " + bb.limit() + ", cap = " +
                           bb.capacity() + ", pos = " + bb.position());

                // Two ways to print out data
//                for (int j=0; j < bb.remaining(); j++) {
//                    System.out.println("data["+j+"] = " + bb.get(bb.position() + j));
//                }

                byte[] array = ByteDataTransformer.toByteArray(bb);
                for (int j=0; j < array.length; j++) {
                    System.out.println("BDT data["+j+"] = " + (int)array[j]);
                }

            }

            System.out.println("\n\nByteBuffer test:\n\n" );
            ByteBufferTest();


        }
        catch (Exception e) {
            e.printStackTrace();
        }

    }


}
