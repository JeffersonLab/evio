package org.jlab.coda.jevio.test;

import org.jlab.coda.hipo.Reader;
import org.jlab.coda.jevio.*;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

/**
 * Test program made to work with 2 file produced by the SwapTest (one of the mainN() methods).
 * This test the readers both regular and compact to see if they parse this single event
 * properly.
 *
 * @author timmer
 * Date: Dec 3, 2015
 */
public class HipoReaderTest {

    static String fileEvio6  =  "/daqfs//home/timmer/hd_rawdata_030888_000_short_EVIO6.evio";


    /** Reading files to get EvioNode objects & print out XML representation. */
    public static void main4(String args[]) {

        try {

            String xml = new String(Files.readAllBytes(Paths.get(fileEvio6)));
            System.out.println("\nXML:\n" + xml);

            System.out.println("Convert XML to EvioEvent:");
            List<EvioEvent> evList = Utilities.toEvents(xml);

            for (EvioEvent ev : evList) {
                System.out.println("\n\nEvioEvent ---> XML:\n" + ev.toXML());
            }
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }


    /** Reading files to get EvioNode objects & print out XML representation. */
    public static void main3(String args[]) {

        EvioNode node;
        boolean hex = false;

        try {
            EvioCompactReader fileReader = new EvioCompactReader(fileEvio6);


            int evNum = 1;
            while ( (node = fileReader.getScannedEvent(evNum++)) != null) {
                String xml = Utilities.toXML(node, hex);
                System.out.println("\nXML:\n" + xml);


                System.out.println("Convert XML to EvioEvent:");
                List<EvioEvent> evList = Utilities.toEvents(xml);

                System.out.println("\n\nev to XML:\n" + evList.get(0).toXML());

                System.out.println("\n\n");
            }

        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }


    /** Reading files to get EvioNode objects & print out XML representation. */
    public static void main(String args[]) {

 //       EvioNode node;
 //       boolean hex=false, compact=false;

        try {
//            if (!compact) {
                Reader reader = new Reader(fileEvio6);
                int evNum = 0;
                byte[] byteArray;

            byteArray = reader.getEvent(evNum);
            Utilities.printBytes(byteArray, 0, 80, "array for event # " + (evNum + 1));


                while ( (byteArray = reader.getEvent(evNum)) != null) {
                    System.out.println("array = " + byteArray);
                    Utilities.printBytes(byteArray, 0, 80, "event " + (evNum + 1));
                    evNum++;
                }
//            }
//            else {
//                EvioCompactReader fileReader = new EvioCompactReader(ver2File);
//                node = fileReader.getScannedEvent(1);
//                String xml = Utilities.toXML(node, hex);
//                System.out.println("\nXML:\n" + xml);
//            }

        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }


}
