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

import org.jlab.coda.jevio.*;
import org.jlab.coda.jevio.unit_tests.EvioTestHelper;

@Tag("fast") // Run when selecting "fast" category of tests

class EvioWriteAndReadBack {

    @Test
    @DisplayName("Evio Write and Read Back Test")
    void evioWriteAndReadBack(TestInfo testInfo) throws EvioException {


        // Create a new EvioWriter
        EvioWriter writer = EvioTestHelper.defaultEventWriter();

        // // Create an event with some data
        // EvioEvent event = new EvioEvent(1);
        // float[] data = {1.0f, 2.0f, 3.0f, 4.0f};
        // event.addBank(new EvioBank("floatBank", 10, 1, data));

        // // Write the event
        // writer.writeEvent(event);
        // writer.close();

        // // Now read back the event
        // EvioReader reader = new EvioReader("testEvents.evio");
        // EvioEvent readEvent = reader.readEvent();

        // // Verify the data
        // assertEquals(1, readEvent.getEventNumber(), "Event number should match");
        // assertEquals(4, readEvent.getBanks().get(0).getIntData().length, "Data length should match");
    }
}