//
// Copyright 2025, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100

package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.io.IOException;
import java.util.List;

public class XmlTest extends TestBase {

    private static String stripWhitespace(String input) {
        return input.replaceAll("\\s+", "");
    }

    void writeAndReadXmlFile() throws EvioException, IOException {

        String fname  = "/tmp/data.evio";
        String fname2  = "/tmp/data2.evio";

        String xfname  = "/tmp/data.xml";
        String xfname2 = "/tmp/data2.xml";

        try {
            EventWriter writer = new EventWriter(fname);

            // Create an evio event, get it in xml format
            EvioEvent event = createEventBuilderEvent(0, 0);
            String evXML = event.toXML(false);
            System.out.println("Wrote the following (xml) event to file in evio format:\n" + evXML);
            writer.writeEvent(event);
            writer.close();

            System.out.println("READ evio file with 1 event");
            EvioReader reader = new EvioReader(fname);

            System.out.println("convert evio to xml file");
            WriteStatus status = reader.toXMLFile(xfname);
            if (status == WriteStatus.SUCCESS) {
                System.out.println("  Success converting evio file to XML!!");
            }

            // Print out the file
            System.out.println("read & print out xml file");
            try {
                Files.lines(Paths.get(xfname)).forEach(System.out::println);
            }
            catch (IOException e) {
                System.err.println("Error reading the file: " + e.getMessage());
            }

            // Read in xml file and convert to evio event
            String xml = new String(Files.readAllBytes(Paths.get(xfname)));
            List<EvioEvent> evList = Utilities.toEvents(xml, 1, 0, null, false);
            System.out.println("\n\n\nevent list len = " + evList.size());

            // Write these events back to another evio file
            EventWriter writer2 = new EventWriter(fname2);
            int counter = 1;
            for (EvioEvent ev : evList) {
                writer2.writeEvent(ev);
                System.out.println("Event " + counter++ + ":\n" + ev.toXML(false));
            }
            writer2.close();

            System.out.println("convert 2nd evio to 2nd xml file");
            System.out.println("READ evio file with 1 event");
            EvioReader reader2 = new EvioReader(fname2);
            status = reader2.toXMLFile(xfname2);
            if (status == WriteStatus.SUCCESS) {
                System.out.println("  Success converting 2nd evio file to 2nd XML!!");
            }


            try {
                // Read all lines from both files
                List<String> lines1 = Files.readAllLines(Paths.get(xfname));
                List<String> lines2 = Files.readAllLines(Paths.get(xfname2));

                // Remove first 2 lines if they exist (since the 2nd line is a comment which differs from file to file)
                String content1 = String.join("\n", lines1.subList(Math.min(2, lines1.size()), lines1.size()));
                String content2 = String.join("\n", lines2.subList(Math.min(2, lines2.size()), lines2.size()));

                if (content1.equals(content2)) {
                    System.out.println("Files are identical.");
                } else if (stripWhitespace(content1).equals(stripWhitespace(content2))) {
                    System.out.println("Files differ only in whitespace.");
                } else {
                    System.out.println("Files differ.");
                }
            } catch (IOException e) {
                System.err.println("Error reading files: " + e.getMessage());
            }
        }
        catch (EvioException e) {
            System.out.println("PROBLEM: " + e.getMessage());
        }


        // Delete files
        
        try {
            Path path = Paths.get(fname);
            Files.delete(path);
            System.out.println("Successfully deleted " + path);
        }
        catch (IOException e) {}

        try {
            Path path = Paths.get(fname2);
            Files.delete(path);
            System.out.println("Successfully deleted " + path);
        }
        catch (IOException e) {}

        try {
            Path path = Paths.get(xfname);
            Files.delete(path);
            System.out.println("Successfully deleted " + path);
        }
        catch (IOException e) {}

        try {
            Path path = Paths.get(xfname2);
            Files.delete(path);
            System.out.println("Successfully deleted " + path);
        }
        catch (IOException e) {}

    }



    public static void main(String args[]) {

        XmlTest tester = new XmlTest();

        try {
            tester.writeAndReadXmlFile();
        }
        catch (Exception e) {
            e.printStackTrace();
        }
        System.out.println("----------------------------------------\n");
    }



};




