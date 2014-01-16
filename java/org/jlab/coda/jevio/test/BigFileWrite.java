package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.io.File;
import java.io.IOException;

/**
 * Test program.
 * @author timmer
 * Date: Oct 7, 2010
 */
public class BigFileWrite {



    /** Write big file of about 6GB for testing purposes.  */
    public static void main(String args[]) {

        // Create an event writer to write out the test events.
        String fileName  = "/daqfs/home/timmer/coda/jevio-4.3/testdata/bigFileV4.ev";
        File file = new File(fileName);

        // data
        int[] intData = new int[3750];

        int count = 0;
        long t1=0, t2=0, time, totalT=0, totalCount=0;
        double rate, avgRate;

        try {
            EventWriter eventWriter = new EventWriter(file);

            // event -> bank of bytes
            // each event (including header) is 100 bytes
            EventBuilder eventBuilder = new EventBuilder(1, DataType.INT32, 1);
            EvioEvent ev = eventBuilder.getEvent();

            // keep track of time
            t1 = System.currentTimeMillis();


            for (int j=0; j < 400000; j++) {
                intData[0] = j;
                ev.setIntData(intData);
                eventWriter.writeEvent(ev);
                count++;
            }

            // all done writing
            eventWriter.close();

            // calculate the event rate
            t2 = System.currentTimeMillis();
            time = t2 - t1;
            rate = 1000.0 * ((double) count) / time;
            totalCount += count;
            totalT += time;
            avgRate = 1000.0 * ((double) totalCount) / totalT;
            System.out.println("rate = " + String.format("%.3g", rate) +
                                       " Hz,  avg = " + String.format("%.3g", avgRate));
            System.out.println("time = " + (time) + " milliseconds");
        }
        catch (IOException e) {
            e.printStackTrace();
        }
        catch (EvioException e) {
            e.printStackTrace();
        }


    }




}
