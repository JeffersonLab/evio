package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.*;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Take test programs out of EvioCompactReader.
 * @author timmer
 * Date: Nov 15, 2012
 */
public class Writer_Reader_Test {


    public static void main(String args[]) {
        try {
            EventWriterTest();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }


        // Test the BaseStructure's tree methods
        static void EventWriterTest() throws EvioException, IOException {

            //---------------------------------------------
            // Use CompactEventBuilder to create an event
            //---------------------------------------------

            int bufSize = 1000;
            CompactEventBuilder ceb = new CompactEventBuilder(bufSize, ByteOrder.BIG_ENDIAN, false);

            ceb.openBank(1, 1, DataType.BANK);
            ceb.openBank(2, 2, DataType.DOUBLE64);
            double[] dd = {1.1, 2.2, 3.3};
            ceb.addDoubleData(dd);
            ceb.closeStructure();

            ceb.openBank(3, 3, DataType.SEGMENT);
            ceb.openSegment(4, DataType.UINT32);
            int[] ui = {4, 5, 6};
            ceb.addIntData(ui);
            ceb.closeStructure();
            ceb.closeStructure();

            ceb.openBank(5, 5, DataType.TAGSEGMENT);
            ceb.openTagSegment(6, DataType.USHORT16);
            short[] us = {7, 8, 9};
            ceb.addShortData(us);
            ceb.closeStructure();
            ceb.closeStructure();

            ceb.openBank(7, 7, DataType.COMPOSITE);
            // Now create some COMPOSITE data
            // Format to write 1 int and 1 float a total of N times
            String format = "N(I,F)";

            CompositeData.Data myData = new CompositeData.Data();
            myData.addN(2);
            myData.addInt(1);
            myData.addFloat(1.0F);
            myData.addInt(2);
            myData.addFloat(2.0F);

            // Create CompositeData object
            CompositeData cData = new CompositeData(format,1,  myData, 1, 1);
            CompositeData[] cDataArray = new CompositeData[1];
            cDataArray[0] = cData;

            // Add to bank
            ceb.addCompositeData(cDataArray);

            ceb.closeAll();

            ByteBuffer cebEvbuf = ceb.getBuffer();

            //Util::printBytes(cebEvbuf, 0 , 200, "From CompactEventBuilder");

            // Write into a buffer
            ByteBuffer newBuf = ByteBuffer.allocate(1000);
            EventWriter writer = new EventWriter(newBuf);
            writer.writeEvent(cebEvbuf);
            writer.close();
            ByteBuffer writerBuf = writer.getByteBuffer();
            System.out.println("Buffer from writer lim = " + writerBuf.limit() + ", pos = " + writerBuf.position());

            //Util::printBytes(newBuf, 0 , 260, "From EventWriter");

            // Read event back out of buffer
            EvioReader reader = new EvioReader(writerBuf);
            System.out.println("Created reader from buf ");
            EvioEvent cebEv = reader.parseEvent(1);

            System.out.println("Event:\n" + cebEv);

        }



    }



