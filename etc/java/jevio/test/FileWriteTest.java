package org.jlab.coda.jevio.test;

import org.jlab.coda.hipo.CompressionType;
import org.jlab.coda.jevio.*;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Test program. Mirrors the FileWriteTest.cpp file.
 *
 * @author timmer
 * Date: Dec 2, 2024
 */
public class FileWriteTest {



    void eventWriterTest() throws EvioException, IOException {

        //---------------------------------------------
        // Use CompactEventBuilder to create an event
        //---------------------------------------------

        // Create room for entire big event
        int bufSize = 1300000;
        CompactEventBuilder ceb = new CompactEventBuilder(bufSize, ByteOrder.nativeOrder(), true);
        ceb.openBank(1, 1, DataType.INT32);
        int[] dd = new int[250000];
        ceb.addIntData(dd, 0, 250000);
        ceb.closeAll();
        ByteBuffer bigEvt = ceb.getBuffer();

        // Little event
        bufSize = 1000;
        CompactEventBuilder eb = new CompactEventBuilder(bufSize, ByteOrder.nativeOrder(), true);
        eb.openBank(1, 1, DataType.INT32);
        eb.addIntData(dd, 0, 3);
        eb.closeAll();
        ByteBuffer littleEvt = eb.getBuffer();

        System.out.println("Buf pos = " + bigEvt.position() + ", lim = " + bigEvt.limit() + ", cap = " + bigEvt.capacity());

        // Write into a file
        int targetRecordBytes = 900000; // 900KB
        int bufferBytes = 1000000; // 1MB

        String fname = "./codaFileTestCC.ev";

        int evioVersion = 6;

        if (evioVersion == 4) {
            EventWriterV4 writer = new EventWriterV4(fname, null, null, 1, 0, targetRecordBytes,
                    100000, bufferBytes, ByteOrder.nativeOrder(), null, null, true,
                    false, null, 0, 0, 1, 1);

            System.out.println("Write little event 1, buf pos = " + littleEvt.position() + ", lim = " + littleEvt.limit());
            writer.writeEvent(littleEvt, false);
            //writer.writeEventToFile(nullptr, littleEvt, false);
            // Delay between writes
            //std::this_thread::sleep_for(std::chrono::seconds(2));
            System.out.println("Write BIG event 1");
            writer.writeEvent(bigEvt, false);
            //std::this_thread::sleep_for(std::chrono::seconds(2));
            System.out.println("Write little event 2, buf pos = " + littleEvt.position() + ", lim = " + littleEvt.limit());
            writer.writeEvent(littleEvt, true);
            //std::this_thread::sleep_for(std::chrono::seconds(2));
            System.out.println("Write BIG event 2");
            writer.writeEvent(bigEvt, true);
            System.out.println("WRITER CLOSE");
            writer.close();


            EvioReader reader = new EvioReader(fname);
            EvioEvent ev;
            while ( (ev = reader.parseNextEvent()) != null) {

            }
        }
        else {
            EventWriter writer = new EventWriter(fname, null, null, 1, 0, targetRecordBytes,
                    100000, ByteOrder.nativeOrder(), null, true, false,
                    null, 0, 0, 1, 1, CompressionType.RECORD_UNCOMPRESSED,
                    0, 0, bufferBytes);

            System.out.println("Write little event 1, buf pos = " + littleEvt.position() + ", lim = " + littleEvt.limit());
            writer.writeEvent(littleEvt);
            //writer.writeEventToFile(littleEvt, false, false);
            // Delay between writes
            //std::this_thread::sleep_for(std::chrono::seconds(2));
            System.out.println("Write BIG event 1");
            writer.writeEvent(bigEvt);
            //writer.writeEventToFile(bigEvt, false, false);
            //std::this_thread::sleep_for(std::chrono::seconds(2));
            System.out.println("Write little event 2, buf pos = " + littleEvt.position() + ", lim = " + littleEvt.limit());
            writer.writeEvent(littleEvt);
            //writer.writeEventToFile(littleEvt, false, true);
            //std::this_thread::sleep_for(std::chrono::seconds(2));
            System.out.println("Write BIG event 2");
            writer.writeEvent(bigEvt);
            //writer.writeEventToFile(bigEvt, false, true);
            System.out.println("WRITER CLOSE");
            writer.close();
        }
    }



    /**
     * Run as a stand-alone application.
     */
    public static void main(String[] args) {
        FileWriteTest receiver = new FileWriteTest();
        try {
            receiver.eventWriterTest();
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }



}
