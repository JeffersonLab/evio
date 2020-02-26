package org.jlab.coda.jevio.test;

import org.jlab.coda.hipo.*;
import org.jlab.coda.jevio.*;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.nio.ShortBuffer;
import java.nio.charset.StandardCharsets;

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



    public static void main(String args[]) {
        String filename = "/dev/shm/EventWriterTest.evio";
        System.out.println("Try writing " + filename);

        // Write files
        String actualFilename = ReadWriteTest.eventWriteFileMT(filename);
        System.out.println("Finished writing, now try reading n" + actualFilename);

        // Read files just written
        ReadWriteTest.readFile2(actualFilename);
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




