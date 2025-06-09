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
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.AsynchronousFileChannel;
import java.nio.channels.FileChannel;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardOpenOption;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Future;

import org.jlab.coda.hipo.*;
import org.jlab.coda.jevio.*;
import org.jlab.coda.jevio.unit_tests.EvioTestHelper; // helper class for unit tests, also contained in java src files

@Tag("fast") // Run when selecting "fast" category of tests

class EvioWriteAndReadBack_builder  {

    @Test
    @DisplayName("Evio Write and Read Back Test")

    // void evioWriteAndReadBack(TestInfo testInfo) throws IOException, EvioException {
    //     evioWriteAndReadBack(testInfo, 10);
    // }

    void evioWriteAndReadBack(TestInfo testInfo) throws IOException, EvioException {

        int nEvents = 100000; // 10 M events

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

            // Now to start defining event
            // if(i == 0) {
            builder = new EventBuilder(tag, DataType.BANK, num);
            event = builder.getEvent();
            // }
            
            // THE OVERBANK
            // First child of event = bank of banks
            // EvioBank bankBanks = new EvioBank(tag+1, DataType.BANK, num+1);
            // builder.addChild(event, bankBanks);

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
}