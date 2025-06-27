/*
 *   Copyright (c) 2025.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */

import static org.junit.jupiter.api.Assertions.assertEquals;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Tag;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.TestInfo;

import java.io.*;
import org.jlab.coda.jevio.*;
import org.jlab.coda.jevio.unit_tests.EvioTestHelper; // helper class for unit tests, also contained in java src files

@Tag("fast") // Run when selecting "fast" category of tests

class EvioTestReadback_builder  {

    @Test
    @DisplayName("Evio Write and Read Back Test")
    void DoReadbackTest(TestInfo testInfo) throws IOException, EvioException {
        
        int nEvents = 100000; // Number of events to write
        
        evioWriteStep(nEvents); // Write events
        evioReadStep(nEvents); // Readback with asserts
    }


    void evioWriteStep(int nEvents) throws IOException, EvioException {

        EvioTestHelper h = new EvioTestHelper();

        // Create a new EvioWriter
        EventWriterUnsync writer = EvioTestHelper.defaultEventWriter();

        EventBuilder builder = null;
        EvioEvent event = null;

        // Generate and write events
        for (int i = 0; i < nEvents; i++) {

            // Build a new event (top-level bank) with tag=1, type=BANK, num=1
            //-------------------------------------
            int tag = 1;
            int num = 1;
            
            float[] floatVec = h.genXYZT(i); // generate pseudo x, y, z, time values

            builder = new EventBuilder(tag, DataType.BANK, num);
            event = builder.getEvent();
            
            // (SUB)BANK 1 OF 1
            // Create first (& only) child of bank of banks = bank of floats
            EvioBank bankFloats = new EvioBank(tag+11, DataType.FLOAT32, num+11);
            bankFloats.appendFloatData(floatVec); // add float data to bank of floats
            builder.addChild(event, bankFloats);

            try {
                // Write the completed event to file
                writer.writeEvent(event, false);
            }
            catch (EvioException e) {
                System.out.println("EvioException: " + e.toString());
            }
        }
        // End of writing events
        try {
            writer.close();
        } 
        catch (Exception e) {
            System.out.println("EvioException: " + e.toString());
            e.printStackTrace();
        }

        System.out.println(" Wrote  " + nEvents + " events to file. ");
        
    }
    

    void evioReadStep(int nEvents) throws IOException, EvioException {

        String filename = EvioTestHelper.directory + "/" + EvioTestHelper.baseNameV6;
        
        try {
            EvioReader reader = new EvioReader(filename);
            int evCount = reader.getEventCount();
            int eventsToCheck = 10; // Number of events to check with explicit output

            System.out.println("Read " + evCount + " events from file: " + filename);
            assertEquals(nEvents, evCount, "Event count mismatch after reading back");

            // Does it contain a dictionary?
            // if (reader.hasDictionaryXML()) {
            //     String dict = reader.getDictionaryXML();
            //     System.out.println("Dictionary = \n" + dict);
            // }            

            // Look at each event with random-access type method (starts at 1)
            // System.out.println("Print out regular events’ raw data:");
            // for (int i = 0; i < eventsToCheck; i++) {
            //     EvioEvent ev = reader.parseEvent(i + 1);
            //     byte[] byteData = ev.getByteData();
            //     System.out.println("First ev size = " + ev.getTotalBytes() + " bytes");
            //     System.out.println(" Event # " + i + "  length = " + byteData.length + " ints");
            //     Utilities.printBytes(byteData, 0, byteData.length, " Event #" + i);
            // }            

            reader.rewind(); // Rewind to the beginning of the file
            // for (int i = 0; i < eventsToCheck; i++) {
            //     EvioEvent ev = reader.parseNextEvent();
            //     byte[] byteData = ev.getByteData();
            //     System.out.println("First ev size = " + ev.getTotalBytes() + " bytes");
            //     // System.out.println(" Event # " + i + "  length = " + byteData.length + " ints");
            //     Utilities.printBytes(byteData, 0, byteData.length, " Event #" + i);
            // }            

            

            // reader.rewind(); // Rewind to the beginning of the file
            // // Look at each event with sequential type method (starts at 0)
            // for (int i = 0; i < evCount; i++) {
            //     EvioEvent ev = reader.parseNextEvent();
            //     byte[] byteData = ev.getByteData();
            //     Utilities.printBytes(byteData, 0, byteData.length, " Event #" + i);
                
            //     // Check if the event is a bank
            //     if (ev.getStructureType() == StructureType.BANK) {
            //         List<EvioBank> banks = ev.getChildBanks();
            //         assertEquals(1, banks.size(), "Expected 1 child bank in event " + i);
            // // Look at each event with sequential type method (starts at 0)
            // for (int i = 0; i < evCount; i++) {
            //     EvioEvent ev = reader.parseNextEvent();
            //     byte[] byteData = ev.getByteData();
            //     Utilities.printBytes(byteData, 0, byteData.length, " Event #" + i);
                
            //     // Check if the event is a bank
            //     if (ev.getStructureType() == StructureType.BANK) {
            //         List<EvioBank> banks = ev.getChildBanks();
            //         assertEquals(1, banks.size(), "Expected 1 child bank in event " + i);
                    
            //         EvioBank bank = banks.get(0);
            //         assertEquals(1, bank.getNum(), "Expected num=1 for bank in event " + i);
                    
            //         // Check the float data
            //         float[] data = bank.getFloatData();
            //         assertEquals(4, data.length, "Expected 4 float values in bank of event " + i);
            //         // Check the float data
            //         float[] data = bank.getFloatData();
            //         assertEquals(4, data.length, "Expected 4 float values in bank of event " + i);
                    
            //         // Validate the data
            //         // float[] expectedData = EvioTestHelper.genXYZT(i);
            //         assertEquals(expectedData[0], data[0], 0.001, "X value mismatch in event " + i);
            //         assertEquals(expectedData[1], data[1], 0.001, "Y value mismatch in event " + i);
            //         assertEquals(expectedData[2], data[2], 0.001, "Z value mismatch in event " + i);
            //         assertEquals(expectedData[3], data[3], 0.001, "T value mismatch in event " + i);
                // }
            // }
    // }            
            //         // Validate the data
            //         float[] expectedData = EvioTestHelper.genXYZT(i);
            //         assertEquals(expectedData[0], data[0], 0.001, "X value mismatch in event " + i);
            //         assertEquals(expectedData[1], data[1], 0.001, "Y value mismatch in event " + i);
            //         assertEquals(expectedData[2], data[2], 0.001, "Z value mismatch in event " + i);
            //         assertEquals(expectedData[3], data[3], 0.001, "T value mismatch in event " + i);
            //     }
            // }
        }            
        catch (Exception e) {
            e.printStackTrace();
        }
    }
}