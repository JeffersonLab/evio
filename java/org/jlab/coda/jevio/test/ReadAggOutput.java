package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.io.File;
import java.io.IOException;
import java.nio.ByteOrder;

public class ReadAggOutput {


    static String fileName = "muFile";

    static int readFile(String finalFilename) throws Exception {

        try {
            EvioReader reader = new EvioReader(new File(finalFilename), false, true, false);
            ByteOrder order = reader.getByteOrder();

            int evCount = reader.getEventCount();
            System.out.println("Read in file " + finalFilename + ", got " + evCount + " events");

            // Loop through all events (skip first 2 which are prestart and go)
            for (int i=1; i <= evCount; i++) {

                System.out.println("Event " + i + ":");
                EvioEvent ev = reader.parseEvent(i);
                int evTag = ev.getHeader().getTag();
                if (evTag == 0xffd1) {
                    System.out.println("Skip over PRESTART event");
                    continue;
                }
                else if (evTag == 0xffd2) {
                    System.out.println("Skip over GO event");
                    continue;
                }
                else if (evTag == 0xffd4) {
                    System.out.println("Hit END event, quitting");
                    break;
                }

                if (evTag == 0xff60) {
                    System.out.println("Found built streaming event");
                }

                // Go one level down ->
                int childCount = ev.getChildCount();
                if (childCount < 2) {
                    throw new Exception("Problem: too few child for event (" + childCount + ")");
                }
                System.out.println("Event has " + childCount + " child structures");

                // First bank is Time Info Bank (TIB) with frame and timestamp
                EvioBank b = (EvioBank) ev.getChildAt(0);
                int[] intData = b.getIntData();
                int frame = intData[0];
                long timestamp = ((((long)intData[1]) & 0x00000000ffffffffL) +
                                  (((long)intData[2]) << 32));
                System.out.println("  Frame = " + frame + ", TS = " + timestamp);

                // Loop through all ROC Time Slice Banks (TSB) which come after TIB
                for (int j=1; j < childCount; j++) {
                    // ROC Time SLice Bank
                    EvioBank rocTSB = (EvioBank)ev.getChildAt(j);
                    int kids = rocTSB.getChildCount();
                    if (kids < 2) {
                        throw new Exception("Problem: too few child for TSB (" + childCount + ")");
                    }

                    // Another level down, each TSB has a Stream Info Bank (SIB) which comes first,
                    // followed by data banks

                    // Skip over SIB by starting at 1
                    for (int k=1; k < kids; k++) {
                        EvioBank dataBank = (EvioBank) rocTSB.getChildAt(k);
                        // Vardan, here is where you get data.
                        // Ignore the data type (currently the improper value of 0xf).
                        // Just get the data as bytes
                        byte[] byteData = dataBank.getRawBytes();

                    }
                }
            }
        }
        catch (IOException e) {
            e.printStackTrace();
        }
        catch (EvioException e) {
            e.printStackTrace();
        }

        return 0;
    }





    public static void main(String args[]) {

        try {
            readFile(fileName);
        }
        catch (Exception e) {
            e.printStackTrace();
        }
        System.out.println("----------------------------------------\n");


    }




};




