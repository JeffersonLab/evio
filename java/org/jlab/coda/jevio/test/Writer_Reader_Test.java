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
                // L0 child 1
                ceb.openBank(1, 1, DataType.BANK);
                    // L1 child 1
                    ceb.openBank(1, 1, DataType.BANK);
                        // L2 child 1
                        ceb.openBank(2, 2, DataType.DOUBLE64);
                        double[] dd = {1.1, 2.2, 3.3};
                        ceb.addDoubleData(dd);
                        ceb.closeStructure();
                    ceb.closeStructure();
                ceb.closeStructure();

                // L0 child 2
                ceb.openBank(3, 3, DataType.SEGMENT);
                    // L1 child 2
                    ceb.openSegment(4, DataType.UINT32);
                    int[] ui = {4, 5, 6};
                    ceb.addIntData(ui);
                    ceb.closeStructure();
                ceb.closeStructure();

                // L0 child 3
                ceb.openBank(5, 5, DataType.TAGSEGMENT);
                    // L1 child 3
                    ceb.openTagSegment(6, DataType.USHORT16);
                    short[] us = {7, 8, 9};
                    ceb.addShortData(us);
                    ceb.closeStructure();
                ceb.closeStructure();

                // L0 child 4
                ceb.openBank(1, 1, DataType.BANK);
                    // L1 child 4
                    ceb.openBank(1, 1, DataType.BANK);
                        // L2 child 2
                        ceb.openBank(4, 4, DataType.UINT32);
                        ceb.addIntData(ui);
                        ceb.closeStructure();
                    ceb.closeStructure();

                    // L1 child 5
                    ceb.openBank(1, 1, DataType.BANK);
                        // L2 child 3
                        ceb.openBank(1, 1, DataType.BANK);
                            // L3 child 1
                            ceb.openBank(1, 1, DataType.BANK);
                                // L4 child 1
                                ceb.openBank(4, 4, DataType.UINT32);
                                ceb.addIntData(ui);
                                ceb.closeStructure();
                            ceb.closeStructure();

                            // L3 child 2
                            ceb.openBank(1, 1, DataType.BANK);
                                // L4 child 2
                                ceb.openBank(4, 4, DataType.UINT32);
                                ceb.addIntData(ui);
                                ceb.closeStructure();
                            ceb.closeStructure();

                            // L3 child 3
                            ceb.openBank(1, 1, DataType.BANK);
                                // L4 child 3
                                ceb.openBank(4, 4, DataType.UINT32);
                                ceb.addIntData(ui);
                                ceb.closeStructure();

                                // L4 child 4
                                ceb.openBank(4, 4, DataType.UINT32);
                                ceb.addIntData(ui);
                                ceb.closeStructure();
                            ceb.closeStructure();

                        ceb.closeStructure();
                    ceb.closeStructure();
                ceb.closeStructure();

            ceb.closeAll();

            ByteBuffer cebEvbuf = ceb.getBuffer();


            // Write into a buffer
            ByteBuffer newBuf = ByteBuffer.allocate(1000);
            EventWriter writer = new EventWriter(newBuf);
            writer.writeEvent(cebEvbuf);
            writer.close();
            ByteBuffer writerBuf = writer.getByteBuffer();

            // Read event back out of buffer
            EvioCompactReader reader = new EvioCompactReader(writerBuf);
            EvioNode node = reader.getScannedEvent(1);
            int level0 = node.getChildCount(0);
            System.out.println();
            int level1 = node.getChildCount(1);
            System.out.println();
            int level2 = node.getChildCount(2);
            System.out.println();
            int level3 = node.getChildCount(3);
            System.out.println();
            int level4 = node.getChildCount(4);

            System.out.println("Children at L0 = " + level0);
            System.out.println("Children at L1 = " + level1);
            System.out.println("Children at L2 = " + level2);
            System.out.println("Children at L3 = " + level3);
            System.out.println("Children at L4 = " + level4);


        }


//        // Test the BaseStructure's tree methods
//        static void EventWriterTest() throws EvioException, IOException {
//
//            //---------------------------------------------
//            // Use CompactEventBuilder to create an event
//            //---------------------------------------------
//
//            int bufSize = 1000;
//            CompactEventBuilder ceb = new CompactEventBuilder(bufSize, ByteOrder.BIG_ENDIAN, false);
//
//            ceb.openBank(1, 1, DataType.BANK);
//            ceb.openBank(2, 2, DataType.DOUBLE64);
//            double[] dd = {1.1, 2.2, 3.3};
//            ceb.addDoubleData(dd);
//            ceb.closeStructure();
//
//            ceb.openBank(3, 3, DataType.SEGMENT);
//            ceb.openSegment(4, DataType.UINT32);
//            int[] ui = {4, 5, 6};
//            ceb.addIntData(ui);
//            ceb.closeStructure();
//            ceb.closeStructure();
//
//            ceb.openBank(5, 5, DataType.TAGSEGMENT);
//            ceb.openTagSegment(6, DataType.USHORT16);
//            short[] us = {7, 8, 9};
//            ceb.addShortData(us);
//            ceb.closeStructure();
//            ceb.closeStructure();
//
//            ceb.openBank(7, 7, DataType.COMPOSITE);
//            // Now create some COMPOSITE data
//            // Format to write 1 int and 1 float a total of N times
//            String format = "N(I,F)";
//
//            CompositeData.Data myData = new CompositeData.Data();
//            myData.addN(2);
//            myData.addInt(1);
//            myData.addFloat(1.0F);
//            myData.addInt(2);
//            myData.addFloat(2.0F);
//
//            // Create CompositeData object
//            CompositeData cData = new CompositeData(format,1,  myData, 1, 1);
//            CompositeData[] cDataArray = new CompositeData[1];
//            cDataArray[0] = cData;
//
//            // Add to bank
//            ceb.addCompositeData(cDataArray);
//
//            ceb.closeAll();
//
//            ByteBuffer cebEvbuf = ceb.getBuffer();
//
//            //Util::printBytes(cebEvbuf, 0 , 200, "From CompactEventBuilder");
//
//            // Write into a buffer
//            ByteBuffer newBuf = ByteBuffer.allocate(1000);
//            EventWriter writer = new EventWriter(newBuf);
//            writer.writeEvent(cebEvbuf);
//            writer.close();
//            ByteBuffer writerBuf = writer.getByteBuffer();
//            System.out.println("Buffer from writer lim = " + writerBuf.limit() + ", pos = " + writerBuf.position());
//
//            //Util::printBytes(newBuf, 0 , 260, "From EventWriter");
//
//            // Read event back out of buffer
//            EvioReader reader = new EvioReader(writerBuf);
//            System.out.println("Created reader from buf ");
//            EvioEvent cebEv = reader.parseEvent(1);
//
//            System.out.println("Event:\n" + cebEv);
//
//        }

    }



