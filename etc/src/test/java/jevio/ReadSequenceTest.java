package org.jlab.coda.jevio.test;

import org.jlab.coda.hipo.*;
import org.jlab.coda.jevio.*;

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.nio.ShortBuffer;

public class ReadSequenceTest {

    // Create a simple Event
    static EvioEvent generateEvioEvent() {
        try {
            EventBuilder builder = new EventBuilder(0x1234, DataType.INT32, 0x12);
            EvioEvent ev = builder.getEvent();

            int[] data = {1};
            builder.appendIntData(ev, data);
            return ev;
        } catch (EvioException e) {
            return null;
        }
    }


    // Write file with 10 simple events in it
    static void writeFile(String filename) {

        try {
            EventWriterUnsync writer = new EventWriterUnsync(filename);

            // Create an event containing a bank of ints
            EvioEvent ev = generateEvioEvent();

            if (ev == null) {
                throw new EvioException("bad event generation");
            }

            for (int i=0; i < 10; i++) {
                System.out.println("Write event #" + i);
                writer.writeEvent(ev);
            }

            writer.close();
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }


    static void readFile(String filename) {

        try {
            EvioReader reader = new EvioReader(filename);
            int evCount = reader.getEventCount();

            System.out.println("Read " + evCount + " events using sequential reading");
            for (int i=0; i < evCount; i++) {
                EvioEvent ev = reader.parseNextEvent();
                System.out.println("got event " + i);
            }
        }
        catch (Exception e) {
            e.printStackTrace();
        }

    }


    public static void main(String args[]) {

        try {
            String filename = "/tmp/myTestFile";
            writeFile(filename);
            readFile(filename);
        }
        catch (Exception e) {
            e.printStackTrace();
        }

    }

};




