package org.jlab.coda.jevio.test;

import org.jlab.coda.hipo.CompressionType;
import org.jlab.coda.jevio.*;

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Test program.
 * @author timmer
 * Date: Sep 16, 2015
 */
public class FirstEventTest {

    /** For testing first event defined in constructor. */
    public static void test1() {

        boolean useBuffer = false, append = false;

        //create an event writer to write out the test events.
        String fileName = "/tmp/firstEventTestJava.ev";
        File file = new File(fileName);
        ByteBuffer myBuf = ByteBuffer.allocate(800);

        // xml dictionary
        String dictionary =
                "<xmlDict>\n" +
                        "  <dictEntry name=\"regular event\" tag=\"1\"   num=\"1\"/>\n" +
                        "  <dictEntry name=\"FIRST EVENT\"   tag=\"2\"   num=\"2\"/>\n" +
                "</xmlDict>";

        // data
        int[] intData1 = new int[] {1,2,3,4,5,6,7};
        int[] intData2 = new int[] {8,9,10,11,12,13,14};

        EvioEvent eventFirst, event;

        try {
            // First event - bank of ints
            EventBuilder EB = new EventBuilder(2, DataType.UINT32, 2);
            eventFirst = EB.getEvent();
            eventFirst.appendIntData(intData1);

            // Regular event - bank of ints
            EventBuilder EB2 = new EventBuilder(1, DataType.UINT32, 1);
            event = EB2.getEvent();
            event.appendIntData(intData2);

            //int split = 312;
            int split = 100;
            System.out.println("FirstEventTest: create EventWriter for file");

            EventWriter ER = new EventWriter(fileName, null, "runType", 1, split,
                    4*1000, 4,
                    ByteOrder.BIG_ENDIAN, dictionary,
                    true, false, eventFirst,
                    0, 0, 1, 1, CompressionType.RECORD_UNCOMPRESSED, 0, 0, 0);

            System.out.println("FirstEventTest: write event #1");
            ER.writeEvent(event);
            System.out.println("FirstEventTest: write event #2");
            ER.writeEvent(event);
            System.out.println("FirstEventTest: write event #3");
            ER.writeEvent(event);

            // All done writing
            ER.close();

            // Now read it back
            EvioReader reader = new EvioReader(fileName + ".0");
            EvioEvent fe = reader.getFirstEvent();
            byte[] feData = fe.getRawBytes();
            int[] feIntData = ByteDataTransformer.toIntArray(feData, ByteOrder.BIG_ENDIAN);
            // Compare
            if (feIntData.length != intData1.length) {
                System.out.println("FirstEventTest: orig first event, and the one read back are of different lengths");
            }
            else {
                boolean bad = false;
                for (int i=0; i < feIntData.length; i++) {
                    if (feIntData[i] != intData1[i]) {
                        bad = true;
                        System.out.println("FirstEventTest: element " + i + " differs between orig and read first ev");
                    }
                }
                if (!bad) {
                    System.out.println("FirstEventTest: orig first event and the one read back are the SAME");
                }
            }


            if (append) {
                split = 0;

                // Append 1 event onto 1st file
                System.out.println("FirstEventTest: opening first file");
                ER = new EventWriter(fileName + ".0", null, "runType", 1, split,
                        4*1000, 4,
                        ByteOrder.BIG_ENDIAN, null,
                        true, true, null,
                        0, 0, 1, 1, CompressionType.RECORD_UNCOMPRESSED, 0, 0, 0);

                System.out.println("FirstEventTest: append event #1");
                ER.writeEvent(event);
                // All done appending
                ER.close();
            }

        }
        catch (IOException e) {
            e.printStackTrace();
        }
        catch (EvioException e) {
            e.printStackTrace();
        }
    }


    public static void main(String args[]) {
        test1();
    }


}
