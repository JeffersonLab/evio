package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.io.File;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.Arrays;

/**
 * Test program.
 * @author timmer
 * Date: Nov 2, 2016
 */
public class DictionaryTest {


    static String xmlDict =
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
            "</xmlDict>"
            ;


    static EvioEvent createSingleEvent() {

        // Top level event
        EvioEvent event = null;

        // data
        int[] intData = new int[2];
        Arrays.fill(intData, 1);

        byte[] byteData = new byte[9];
        Arrays.fill(byteData, (byte)2);

        short[] shortData = new short[3];
        Arrays.fill(shortData, (short)3);

        double[] doubleData = new double[1];
        Arrays.fill(doubleData, 4.);

        try {

            // Build event (bank of banks) with EventBuilder object
            EventBuilder builder = new EventBuilder(6, DataType.BANK, 0);
            event = builder.getEvent();

                // bank of ints
                EvioBank bankInts = new EvioBank(6, DataType.INT32, 4);
                bankInts.appendIntData(intData);
                builder.addChild(event, bankInts);

                // bank of shorts
                EvioBank bankShorts = new EvioBank(6, DataType.SHORT16, 5);
                bankShorts.appendShortData(shortData);
                builder.addChild(event, bankShorts);

                // bank of doubles
                EvioBank bankDoubles = new EvioBank(6, DataType.DOUBLE64, 6);
                bankDoubles.appendDoubleData(doubleData);
                builder.addChild(event, bankDoubles);

                // bank of ints
                EvioBank bankInts2 = new EvioBank(7, DataType.INT32, 1);
                bankInts2.appendIntData(intData);
                builder.addChild(event, bankInts2);

                // bank of ints
                EvioBank bankInts3 = new EvioBank(7, DataType.INT32, 2);
                bankInts3.appendIntData(intData);
                builder.addChild(event, bankInts3);

                // Bank of banks
                EvioBank bankBanks = new EvioBank(8, DataType.BANK, 0);
                builder.addChild(event, bankBanks);

                    // bank of chars
                    EvioBank bankBytes = new EvioBank(8, DataType.CHAR8, 2);
                    bankBytes.appendByteData(byteData);
                    builder.addChild(bankBanks, bankBytes);

                // Bank of segs
                EvioBank bankSegs = new EvioBank(5, DataType.SEGMENT, 0);
                builder.addChild(event, bankSegs);

                    // Seg of shorts
                    EvioSegment segShorts = new EvioSegment(5, DataType.SHORT16);
                    segShorts.appendShortData(shortData);
                    builder.addChild(bankSegs, segShorts);

                // Bank of tagsegs
                EvioBank bankTSegs = new EvioBank(75, DataType.TAGSEGMENT, 0);
                builder.addChild(event, bankTSegs);

                    // Seg of bytes
                    EvioTagSegment tsegShorts = new EvioTagSegment(76, DataType.CHAR8);
                    tsegShorts.appendByteData(byteData);
                    builder.addChild(bankTSegs, tsegShorts);

        }
        catch (EvioException e) {
            e.printStackTrace();
        }

        return event;

    }


    /** For WRITING a local file. */
    public static void main(String args[]) {

        String fileName  = "/tmp/newDictTest.ev";
        String dictFileName  = "/tmp/newDict.xml";
        File file = new File(fileName);

        try {
            // Write xml to file containing only dictionary
            byte[] b = xmlDict.getBytes(Charset.forName("ASCII"));
            ByteBuffer buf = ByteBuffer.wrap(b);

            Utilities.bufferToFile(dictFileName, buf, true, false);

            // Create an event writer to write out the test events to file
            EventWriter writer = new EventWriter(file, xmlDict, false);

            EvioEvent ev = createSingleEvent();

            // Write 10 events to file
            writer.writeEvent(ev);
            writer.writeEvent(ev);
            writer.writeEvent(ev);
            writer.writeEvent(ev);
            writer.writeEvent(ev);
            writer.writeEvent(ev);
            writer.writeEvent(ev);
            writer.writeEvent(ev);
            writer.writeEvent(ev);
            writer.writeEvent(ev);

            // All done writing
            writer.close();



            EvioReader reader = new EvioReader(fileName, false, true);
            //int count = reader.getEventCount();
            int count = 2;
            System.out.println("Count = " + count);
            int j=0;
            while ( (ev = reader.parseNextEvent()) != null) {
                System.out.println("Event* " + (++j) + " = " + ev);
            }

            reader.rewind();

            j=0;
            while ( (ev = reader.parseNextEvent()) != null) {
                System.out.println("Event@ " + (++j) + " = " + ev);
            }



//            for (int i=1; i <= count; i++) {
//                ev = reader.getEvent(i);
//                System.out.println("Event " + i + " = " + ev);
//            }
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }



}
