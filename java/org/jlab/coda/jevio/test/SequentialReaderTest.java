package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.io.File;


/**
 * Test program.
 * @author timmer
 * Date: Apr 14, 2015
 */
public class SequentialReaderTest extends TestBase {

    /** For testing only */
    public static void main(String args[]) {


        try {
            int counter = 1;

            // Write 3 events to file
            String fileName  = "/tmp/seqReadTest.evio";
            SequentialReaderTest tester = new SequentialReaderTest();
            EvioEvent ev = tester.createEventBuilderEvent(1,1);
            EventWriter writer = new EventWriter(fileName);
            writer.writeEvent(ev);
            writer.writeEvent(ev);
            writer.writeEvent(ev);
            writer.close();

            File fileIn = new File(fileName);
            System.out.println("read ev file: " + fileName + " size: " + fileIn.length());


            EvioReader fileReader = new EvioReader(fileName, false, true);

            System.out.println("count events ...");
            int eventCount = fileReader.getEventCount();
            System.out.println("done counting events, " + eventCount);

            // Read sequentially

            while (fileReader.parseNextEvent() != null) {
                System.out.println("parseNextEvent # " + counter++);
            }

            // Now read non-sequentially

            System.out.println("get ev #1");
            EvioEvent event = fileReader.getEvent(1);

            System.out.println("get ev #2");
            event = fileReader.getEvent(2);

            System.out.println("get ev #3");
            event = fileReader.getEvent(3);



            System.out.println("goto ev #1");
            event = fileReader.gotoEventNumber(1);

            System.out.println("goto ev #2");
            event = fileReader.gotoEventNumber(2);

            System.out.println("goto ev #3");
            event = fileReader.gotoEventNumber(3);



            System.out.println("parse ev #1");
            event = fileReader.parseEvent(1);

            System.out.println("parse ev #2");
            event = fileReader.parseEvent(2);

            System.out.println("parse ev #3");
            event = fileReader.parseEvent(3);


            // Rewind
            System.out.println("rewind file");
            fileReader.rewind();


            // Read sequentially again
            counter = 1;
            while (fileReader.parseNextEvent() != null) {
                System.out.println("parseNextEvent # " + counter++);
            }

        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }


}
