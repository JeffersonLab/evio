package org.jlab.coda.jevio.test;

import org.jlab.coda.hipo.CompressionType;
import org.jlab.coda.jevio.*;

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


/**
 * Test program.
 * @author timmer
 * Date: Oct 7, 2010
 */
public class FileTest {



    /**
     * Test how the AsynchronousFileChannel affect its ByteBuffer arg
     * when reading and writing. Does buffer position advance on write?
     * on read?
     *
     * @param args
     */
    public static void main(String args[]) throws IOException {
        String fileName = "/tmp/fileTest";

        Path currentFilePath = Paths.get(fileName);
        File currentFile = currentFilePath.toFile();

        ByteBuffer buffer = ByteBuffer.allocate(64);

        buffer.putInt(1);
        buffer.flip();

        System.out.println("Before write pos = " + buffer.position() + ", remaining = " + buffer.remaining());

        // For reading existing file and preparing for stream writes
        AsynchronousFileChannel asyncFileChannel = AsynchronousFileChannel.open(currentFilePath,
                StandardOpenOption.READ,
                StandardOpenOption.WRITE);


        Future<Integer> future = asyncFileChannel.write(buffer, 0);

        try {
            int partial = future.get();
        }
        catch (Exception e) {
            throw new IOException(e);
        }

        asyncFileChannel.write(buffer, 0);
        asyncFileChannel.close();

        System.out.println("After write pos = " + buffer.position());
    }



    /** For WRITING a local file. */
    public static void main4(String args[]) {
        File f = new File("/dev/shm/someTestFile");
        long freeSpace = f.getFreeSpace();
        File f2 = new File("/dev/shm");
        long freeSpace2 = f2.getFreeSpace();
        File f3 = new File("/tmp");
        long freeSpace3 = f3.getFreeSpace();
        System.out.println("Free space on partition for " + f.getName() + " = " + freeSpace);
        System.out.println("Free space on partition for " + f2.getName() + " = " + freeSpace2);
        System.out.println("Free space on partition for " + f3.getName() + " = " + freeSpace3);

    }


    /**
     * For WRITING an event too large for the desired record size.
     */
    public static void main5(String args[]) {

        // String fileName  = "./myData.ev";
        String fileName  = "./codaFileTest.ev";
        File file = new File(fileName);

        String xmlDictionary = null;

        // data, 4 bytes / event
        int[] littleIntData  = new int[1];
        littleIntData[0] = 123;


        try {

            int targetRecordBytes = 910000; // 910KB
            int bufferBytes = 1000000; // 1MB

            EventWriter writer = new EventWriter(
                    file.getPath(), null, null, 1, 0,
                    targetRecordBytes, 100000,
                    ByteOrder.nativeOrder(), xmlDictionary,
                    true, false, null,
                    0, 0, 1, 1,
                    CompressionType.RECORD_UNCOMPRESSED, 0, 0,
                    bufferBytes);

            // Build little event
            EventBuilder builder2 = new EventBuilder(100, DataType.INT32, 100);
            EvioEvent event2 = builder2.getEvent();
            event2.setIntData(littleIntData);


            // Write events to test if writer's internal buffer can handle 1 event > that size
            boolean wroteIt = writer.writeEvent(event2, false, true);
            System.out.println("FileTest, little event, wroteIt = " + wroteIt);

            wroteIt = writer.writeEvent(event2, false, true);
            System.out.println("FileTest, little event again, wroteIt = " + wroteIt);

            wroteIt = writer.writeEvent(event2, false, true);
            System.out.println("FileTest, little event again, wroteIt = " + wroteIt);

            wroteIt = writer.writeEvent(event2, false, true);
            System.out.println("FileTest, little event again, wroteIt = " + wroteIt);

            // All done writing
            writer.close();
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }


    /**
     * For WRITING an event too large for the desired record size.
     */
    public static void main6(String args[]) {

        // String fileName  = "./myData.ev";
        String fileName  = "./codaFileTest.ev";
        File file = new File(fileName);
        ByteBuffer myBuf = null;

        String xmlDictionary = null;

        // data, 1M data bytes for each complete event
        int wordSize = 250000;
        int[] bigIntData  = new int[wordSize];
        for (int i=0; i < wordSize; i++) {
            bigIntData[i] = i;
        }

        // data, 4 bytes / event
        int[] littleIntData  = new int[1];
        littleIntData[0] = 123;

        // data, 300k data bytes for each complete event
        int medWordSize = 75000;
        int[] medIntData  = new int[medWordSize];
        for (int i=0; i < medWordSize; i++) {
            medIntData[i] = i;
        }

        try {

            int targetRecordBytes = 910000; // 910KB
            int bufferBytes = 1000000; // 1MB

            EventWriterUnsync writer = new EventWriterUnsync(
                    file.getPath(), null, null, 1, 0,
                    targetRecordBytes, 100000,
                    ByteOrder.nativeOrder(), xmlDictionary,
                    true, false, null,
                    0, 0, 1, 1,
                    CompressionType.RECORD_UNCOMPRESSED, 0, 0,
                    bufferBytes);

            // Build big event
            EventBuilder builder = new EventBuilder(10, DataType.INT32, 10);
            EvioEvent event = builder.getEvent();
            event.setIntData(bigIntData);

            // Build little event
            EventBuilder builder2 = new EventBuilder(100, DataType.INT32, 100);
            EvioEvent event2 = builder2.getEvent();
            event2.setIntData(littleIntData);

            // Build medium event
            EventBuilder builder3 = new EventBuilder(200, DataType.INT32, 200);
            EvioEvent event3 = builder3.getEvent();
            event3.setIntData(medIntData);

            // Write events to test if writer's internal buffer can handle 1 event > that size
//            boolean wroteIt = writer.writeEventToFile(event2, null, false);
//            System.out.println("FileTest, little event, wroteIt = " + wroteIt);
//
//            wroteIt = writer.writeEventToFile(event, null, false);
//            System.out.println("FileTest, big event, wroteIt = " + wroteIt);
//
//            wroteIt = writer.writeEventToFile(event2, null, false);
//            System.out.println("FileTest, little event again, wroteIt = " + wroteIt);
//
//            wroteIt = writer.writeEventToFile(event, null, false);
//            System.out.println("FileTest, big event again, wroteIt = " + wroteIt);
//
//            wroteIt = writer.writeEventToFile(event2, null, false);
//            System.out.println("FileTest, little event again, wroteIt = " + wroteIt);
//
//            wroteIt = writer.writeEventToFile(event, null, false);
//            System.out.println("FileTest, big event again, wroteIt = " + wroteIt);
//
//            wroteIt = writer.writeEventToFile(event2, null, false);
//            System.out.println("FileTest, little event again, wroteIt = " + wroteIt);
//
//            wroteIt = writer.writeEventToFile(event, null, false);
//            System.out.println("FileTest, big event again, wroteIt = " + wroteIt);


            // Test to see the resulting record sizes in the written file.
            // 3 medium size events should be all that fits into 1, 9.1kB record.

            boolean wroteIt = writer.writeEventToFile(event3, null, false, false);
            System.out.println("FileTest, med event, wroteIt = " + wroteIt);

            wroteIt = writer.writeEventToFile(event3, null, false, false);
            System.out.println("FileTest, med event again, wroteIt = " + wroteIt);
            wroteIt = writer.writeEventToFile(event3, null, false, false);
            System.out.println("FileTest, med event again, wroteIt = " + wroteIt);
            wroteIt = writer.writeEventToFile(event3, null, false, false);
            System.out.println("FileTest, med event again, wroteIt = " + wroteIt);
            wroteIt = writer.writeEventToFile(event3, null, false, false);
            System.out.println("FileTest, med event again, wroteIt = " + wroteIt);
            wroteIt = writer.writeEventToFile(event3, null, false, false);
            System.out.println("FileTest, med event again, wroteIt = " + wroteIt);
            wroteIt = writer.writeEventToFile(event3, null, false, false);
            System.out.println("FileTest, med event again, wroteIt = " + wroteIt);

            wroteIt = writer.writeEventToFile(event, null, false, false);
            System.out.println("FileTest, big event, wroteIt = " + wroteIt);


            // All done writing
            writer.close();
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }


    /**
     * For WRITING a single event repeatedly to a local file in order to
     * test the file structure to see if record sizes are correct.
     */
    public static void main7(String args[]) {

        // String fileName  = "./myData.ev";
        String fileName  = "./codaFileTest.ev";
        File file = new File(fileName);
        ByteBuffer myBuf = null;

        String xmlDictionary = null;

        // data, 10,000 data bytes for each complete event
        int wordSize = 2500 - 2;
        int[] bigIntData  = new int[wordSize];
        for (int i=0; i < wordSize; i++) {
            bigIntData[i] = i;
        }

        try {

            int targetRecordBytes = 0; // 8MB
            int bufferBytes = 64000000; // 64MB

            EventWriterUnsync writer = new EventWriterUnsync(
                    file.getPath(), null, null, 1, 0,
                    targetRecordBytes, 100000,
                    ByteOrder.nativeOrder(), xmlDictionary,
                    true, false, null,
                    0, 0, 1, 1,
                    CompressionType.RECORD_UNCOMPRESSED, 0, 0,
                    bufferBytes);

            // Build event
            EventBuilder builder = new EventBuilder(10, DataType.INT32, 10);
            EvioEvent event = builder.getEvent();
            event.appendIntData(bigIntData);

            // Write 200MB file
            for (int i=0; i < 20000; i++) {
                // Write events to file
                writer.writeEvent(event);
            }

                // All done writing
            System.out.println("FileTest, call close()");
            writer.close();
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }



    /** For WRITING a local file. */
    public static void main8(String args[]) {

        // String fileName  = "./myData.ev";
        String fileName  = "/home/timmer/fileTestSmall.ev";
        File file = new File(fileName);
        ByteBuffer myBuf = null;

        // xml dictionary
        String xmlDictionary =
            "<xmlDict>\n" +
            "  <bank name=\"My Event\"       tag=\"1\"   num=\"1\">\n" +
            "     <bank name=\"Ints\"    tag=\"2\"   num=\"2\">\n" +
            "       <leaf name=\"My Shorts\" tag=\"3\"   />\n" +
            "     </bank>\n" +
            "     <bank name=\"Banks\"       tag=\"4\"   num=\"4\">\n" +
            "       <leaf name=\"My chars\"  tag=\"5\"   num=\"5\"/>\n" +
            "     </bank>\n" +
            "  </bank>\n" +
            "  <dictEntry name=\"First Event\" tag=\"100\"  num=\"100\"/>\n" +
            "  <dictEntry name=\"Test Bank\" tag=\"1\" />\n" +
            "</xmlDict>";
        //xmlDictionary = null;
        // data
        byte[]  byteData1 = new byte[]  {1,2,3,4,5};
        int[]   intData1  = new int[]   {1,2,3,4};
        //final int bigSize =  549990;
        //int[]   bigIntData  = new int[bigSize];
        int[]   intData2  = new int[]   {7,8,9};
        short[] shortData = new short[] {11,22,33};
        double[] doubleData = new double[] {1.1,2.2,3.3};


//        for (int i=0; i < bigSize; i++) {
//            bigIntData[i] = i;
//        }

        // Do we overwrite or append?
        boolean append = false;

        // Do we write to file or buffer?
        boolean useFile = true;

        // Top level event
        EvioEvent event = null;

        try {
            // Create an event writer to write out the test events to file
            EventWriter writer;

//            // Build event (bank of banks) with EventBuilder object
//            EventBuilder builder = new EventBuilder(1, DataType.BANK, 1);
//            event = builder.getEvent();
//
//            // bank of ints
//            EvioBank bankInts = new EvioBank(2, DataType.INT32, 2);
//            bankInts.appendIntData(intData1);
//            builder.addChild(event, bankInts);

            // Build event (bank of ints) with EventBuilder object
            EventBuilder builder = new EventBuilder(1, DataType.INT32, 1);
            event = builder.getEvent();
            event.appendIntData(intData1);


            // Build first event
            EventBuilder builder3 = new EventBuilder(100, DataType.INT32, 100);
            EvioEvent eventFirst = builder3.getEvent();

            // bank of ints
            int[] intFirstData = new int[6];
            for (int i=0; i < intFirstData.length; i++) {
                intFirstData[i] = 0x100*(i+1);
            }
            eventFirst.appendIntData(intFirstData);



            if (useFile) {
// Include first event
                int splitBytes = 160;
                int targetBlockSize = 29;

                writer = new EventWriter(file.getPath(), null, null, 1, splitBytes,
                                         4*targetBlockSize, 1000,
                                         ByteOrder.nativeOrder(), xmlDictionary,
                                         true, append, eventFirst,
                                         0, 0, 1, 1, CompressionType.RECORD_UNCOMPRESSED, 0, 0, 0);
            }
            else {
                // Create an event writer to write to buffer
                myBuf = ByteBuffer.allocate(10000);
                //myBuf.order(ByteOrder.LITTLE_ENDIAN);
                writer = new EventWriter(myBuf, 4*28, 1000, null, 1,
                                          CompressionType.RECORD_UNCOMPRESSED);
            }


//            // Build bigger event
//            EventBuilder builder2 = new EventBuilder(10, DataType.BANK, 10);
//            EvioEvent event2 = builder2.getEvent();
//
//            // bank of int
//            int[] intData = new int[5];
//            for (int i=0; i < intData.length; i++) {
//                intData[i] = i;
//            }
//            EvioBank bankInts2 = new EvioBank(20, DataType.INT32, 20);
//            bankInts2.appendIntData(intData);
//            builder2.addChild(event2, bankInts2);

            // Build bigger event
            EventBuilder builder2 = new EventBuilder(10, DataType.INT32, 10);
            EvioEvent event2 = builder2.getEvent();

            // bank of int
            int[] intData = new int[5];
            for (int i=0; i < intData.length; i++) {
                intData[i] = 2*i;
            }
            event2.appendIntData(intData);



//            // Write event to file
//            writer.writeEvent(event);
//            System.out.println("Event #1, Block #" + writer.getBlockNumber());
//
//            writer.writeEvent(event2);
//            System.out.println("Event #2, Block #" + writer.getBlockNumber());
//
//            writer.writeEvent(event);
//            System.out.println("Event #3, Block #" + writer.getBlockNumber());



            for (int i=0; i < 6; i++) {
                // Write event to file
                writer.writeEvent(event);

                System.out.println("Event #" + (i+1) + ", Block #" + writer.getBlockNumber());

                // How much room do I have left in the buffer now?
                if (!useFile) {
                    System.out.println("Buffer has " + myBuf.remaining() + " bytes left");
                }
            }

            writer.writeEvent(event2);
            System.out.println("Event #7, Block #" + writer.getBlockNumber());
         //   writer.flush();
         //   writer.flush();
         //   writer.flush();
         //   writer.flush();

            writer.writeEvent(event);
            System.out.println("Event #8, Block #" + writer.getBlockNumber());

            if (true) {
                // All done writing
                System.out.println("FileTest, call close()");
                writer.close();
            }

            if (xmlDictionary != null) {
                EvioXMLDictionary dict = new EvioXMLDictionary(xmlDictionary);
                NameProvider.setProvider(dict);
            }

//            if (useFile) {
//                // Mess with the file by removing bytes off the end
//                // to simulate incomplete writes due to crashes.
//                //removeBytesFromFileEnd(fileName, 5);
//
//                boolean useSequentialRead = false;
//                //readWrittenData(fileName+"out", useSequentialRead);
//                readWrittenData(fileName, useSequentialRead);
//            }
//            else {
//                ByteBuffer buf = writer.getByteBuffer();
//                // remove header + 1 word if closed, else just 1 word
//                buf.limit(buf.limit() - 84);
//                Utilities.bufferToFile(fileName+"out", buf, true, false);
//                readWrittenData(buf);
//            }
//
//            // Append a few more events and reread
//            System.out.println("call appendEvents");
//            appendEvents(fileName, event, 3);
//
//            readWrittenData(fileName, false);
//

        }
        catch (Exception e) {
            e.printStackTrace();
        }

    }


    static private void removeBytesFromFileEnd(String fileName, int byteCount) {

        try {
            // Random file access used to read existing file
            RandomAccessFile rafIn = new RandomAccessFile(fileName, "rw");

            // Random file access used to read existing file
            RandomAccessFile rafOut = new RandomAccessFile(fileName+"out", "rw");

            // Size of existing file
            long fileSize = rafIn.length();

            // Size of file to write
            if (byteCount < 0) byteCount *= -1;
            long newSize = fileSize - byteCount;
            System.out.println("File size = " + fileSize + ", reduce to " + newSize);

            for (long l=0L; l < newSize; l++) {
                //System.out.println("long l = " + l + ", read from in and write to out");
                rafOut.writeByte(rafIn.readByte());
            }
            rafIn.close();
            rafOut.close();
        }
        catch (IOException e) {
            e.printStackTrace();
        }

    }

    private static void readWrittenData(ByteBuffer buf)
            throws IOException, EvioException {

        EvioReader evioReader = new EvioReader(buf);

        // How many events in the file?
        int evCount = evioReader.getEventCount();
        System.out.println("\nRead buffer, got " + evCount + " events");

        // Use the "random access" capability to look at last event (starts at 1)
        EvioEvent ev = evioReader.parseEvent(evCount);
        System.out.println("Last event = " + ev.toString());

        // Print out any data in the last event.
        //
        // In the writing example, the data for this event was set to
        // be little endian so we need to read it in that way too
        int[] intData = ev.getIntData();
        if (intData != null) {
            for (int i=0; i < 4; i++) {
                System.out.println("intData[" + i + "] = " + intData[i]);
            }
        }

    }


    private static void readWrittenData(String fileName, boolean sequential)
            throws IOException, EvioException {

        File file = new File(fileName);
        EvioReader evioReader = new EvioReader(fileName, false, sequential);
        System.out.println("read ev file: " + fileName + ", size: " + file.length());

        // How many events in the file?
        int evCount = evioReader.getEventCount();
        System.out.println("Read file, got " + evCount + " events:\n");

        // Use the "random access" capability to look at last event (starts at 1)
        EvioEvent ev = evioReader.parseEvent(evCount);
        System.out.println("Last event = " + ev.toString());

        // Print out any data in the last event.
        //
        // In the writing example, the data for this event was set to
        // be little endian so we need to read it in that way too
        //ev.setByteOrder(ByteOrder.LITTLE_ENDIAN);
        int[] intData = ev.getIntData();
        if (intData != null) {
            for (int i=0; i < 4; i++) {
                System.out.println("intData[" + i + "] = " + intData[i]);
            }
        }

        // Use sequential access to events
//        while ( (ev = evioReader.parseNextEvent()) != null) {
//            System.out.println("Event = " + ev.toString());
//        }
    }


    private static void appendEvents(String fileName, EvioEvent ev, int count)
            throws IOException, EvioException {

        EventWriter writer = new EventWriter(fileName, null, null, 1, 0,
                                 4*16, 1000,
                                 ByteOrder.nativeOrder(), null,
                                 true, true, null,
                                             0, 0, 1, 1, CompressionType.RECORD_UNCOMPRESSED, 0, 0, 0);

        for (int i=0; i < count; i++) {
            // append event to file
            writer.writeEvent(ev);
            System.out.println("Appended event #" + (i+1) + ", Block #" + writer.getBlockNumber());
        }

        writer.close();
    }



    private static void readWrittenDataOrig(String fileName, File file, ByteBuffer myBuf,
                                            boolean useFile, EvioEvent event) {

        try {
            EvioReader evioReader;
            if (useFile) {
                System.out.println("read ev file: " + fileName + ", size: " + file.length());
                evioReader = new EvioReader(fileName);
            }
            else {
                myBuf.flip();
                evioReader = new EvioReader(myBuf);
            }
            EventParser parser = evioReader.getParser();

                IEvioListener listener = new IEvioListener() {
                    public void gotStructure(BaseStructure topStructure, IEvioStructure structure) {
                        System.out.println("Parsed structure of type " + structure.getStructureType());
                    }

                    public void startEventParse(BaseStructure structure) {

                        System.out.println("Starting event parse");
                    }

                    public void endEventParse(BaseStructure structure) {

                        System.out.println("Ended event parse");
                    }
                };

                parser.addEvioListener(listener);

            // Get any existing dictionary (should be the same as "xmlDictionary")
            String xmlDictString = evioReader.getDictionaryXML();
            EvioXMLDictionary dictionary = null;

            if (xmlDictString == null) {
                System.out.println("Ain't got no dictionary!");
            }
            else {
                // Create dictionary object from xml string
                dictionary = new EvioXMLDictionary(xmlDictString);
                System.out.println("Got a dictionary:\n" + dictionary.toString());
            }

            // How many events in the file?
            int evCount = evioReader.getEventCount();
            System.out.println("Read file, got " + evCount + " events:\n");

            // Use the "random access" capability to look at last event (starts at 1)
            EvioEvent ev = evioReader.parseEvent(evCount);
            System.out.println("Last event = " + ev.toString());

            // Print out any data in the last event.
            //
            // In the writing example, the data for this event was set to
            // be little endian so we need to read it in that way too
            ev.setByteOrder(ByteOrder.LITTLE_ENDIAN);
            int[] intData = ev.getIntData();
            if (intData != null) {
                for (int i=0; i < intData.length; i++) {
                    System.out.println("intData[" + i + "] = " + intData[i]);
                }
            }

            // Use the dictionary
            if (dictionary != null) {
                String eventName = dictionary.getName(ev);
                System.out.println("Name of last event = " + eventName);
            }

            // Use sequential access to events
            while ( (ev = evioReader.parseNextEvent()) != null) {
                System.out.println("Event = " + ev.toString());
            }

            // Go back to the beginning of file/buffer
            evioReader.rewind();

            // Search for banks/segs/tagsegs with a particular tag & num pair of values
            int tag=1, num=1;
            List<BaseStructure> list = StructureFinder.getMatchingBanks(
                    (ev = evioReader.parseNextEvent()), tag, num);
            System.out.println("Event = " + ev.toString());
            for (BaseStructure s : list) {
                if (dictionary == null)
                    System.out.println("Evio structure named \"" + s.toString() + "\" has tag=1 & num=1");
                else
                    System.out.println("Evio structure named \"" + dictionary.getName(s) + "\" has tag=1 & num=1");
            }

            // ------------------------------------------------------------------
            // Search for banks/segs/tagsegs with a custom set of search criteria
            // ------------------------------------------------------------------

            // This filter selects Segment structures that have odd numbered tags.
            class myEvioFilter implements IEvioFilter {
                public boolean accept(StructureType structureType, IEvioStructure struct) {
                    return (structureType == StructureType.SEGMENT &&
                            (struct.getHeader().getTag() % 2 == 1));
                }
            };

            myEvioFilter filter = new myEvioFilter();
            list = StructureFinder.getMatchingStructures(event, filter);
            if (list != null) {
                System.out.println("list size = " + list.size());
                for (BaseStructure s : list) {
                    if (dictionary == null)
                        System.out.println("Evio structure named \"" + s.toString() + "\" has tag=1 & num=1");
                    else
                        System.out.println("Evio structure named " + dictionary.getName(s) +
                                                   " is a segment with an odd numbered tag");
                }
            }

            // ------------------------------------------------------------------
        }
        catch (EvioException e) {
            e.printStackTrace();
        }
        catch (IOException e) {
            e.printStackTrace();
        }
    }



    /** For testing only */
    public static void main9(String args[]) {

        //create an event writer to write out the test events.
        String fileName  = "/daqfs/home/timmer/coda/jevio-4.3/testdata/BigOut2.ev";
        File file = new File(fileName);

        ByteBuffer myBuf = ByteBuffer.allocate(800);

        // xml dictionary
        String dictionary =
                "<xmlDict>\n" +
                        "  <xmldumpDictEntry name=\"bank\"           tag=\"1\"   num=\"1\"/>\n" +
                        "  <xmldumpDictEntry name=\"bank of shorts\" tag=\"2\"   num=\"2\"/>\n" +
                        "  <xmldumpDictEntry name=\"shorts pad0\"    tag=\"3\"   num=\"3\"/>\n" +
                        "  <xmldumpDictEntry name=\"shorts pad2\"    tag=\"4\"   num=\"4\"/>\n" +
                        "  <xmldumpDictEntry name=\"bank of chars\"  tag=\"5\"   num=\"5\"/>\n" +
                "</xmlDict>";

        // data
        short[] shortData1  = new short[] {11,22};
        short[] shortData2  = new short[] {11,22,33};

        EvioEvent event = null;

        try {
            EventWriter eventWriterNew = new EventWriter(fileName, null, null, 1, 0,
                                                         4*1000, 3,
                                                         ByteOrder.BIG_ENDIAN, dictionary,
                                                         true, false, null,
                                                         0, 0, 1, 1, CompressionType.RECORD_UNCOMPRESSED, 0, 0, 0);

            // event - bank of banks
            EventBuilder eventBuilder2 = new EventBuilder(1, DataType.BANK, 1);
            event = eventBuilder2.getEvent();

            // bank of short banks
            EvioBank bankBanks = new EvioBank(2, DataType.BANK, 2);

            // 3 shorts
            EvioBank shortBank1 = new EvioBank(3, DataType.SHORT16, 3);
            shortBank1.appendShortData(shortData1);
            eventBuilder2.addChild(bankBanks, shortBank1);

            EvioBank shortBank2 = new EvioBank(3, DataType.SHORT16, 3);
            shortBank2.appendShortData(shortData2);
            eventBuilder2.addChild(bankBanks, shortBank2);

            eventBuilder2.addChild(event, bankBanks);
            eventWriterNew.writeEvent(event);

            // all done writing
            eventWriterNew.close();
        }
        catch (IOException e) {
            e.printStackTrace();
        }
        catch (EvioException e) {
            e.printStackTrace();
        }


//        // parse bytes
//        class myListener implements IEvioListener {
//
//            public void startEventParse(EvioEvent evioEvent) { }
//
//            public void endEventParse(EvioEvent evioEvent) { }
//
//            public void gotStructure(EvioEvent evioEvent, IEvioStructure structure) {
//
//                BaseStructureHeader header = structure.getHeader();
//
//                System.out.println("------------------");
//                System.out.println("" + structure);
//
//                switch (header.getDataTypeEnum()) {
//                    case FLOAT32:
//                        System.out.println("        FLOAT VALS");
//                        float floatdata[] = structure.getFloatData();
//                        for (float f : floatdata) {
//                            System.out.println("         " + f);
//                        }
//                        break;
//
//                    case DOUBLE64:
//                        System.out.println("        DOUBLE VALS");
//                        double doubledata[] = structure.getDoubleData();
//                        for (double d : doubledata) {
//                            System.out.println("         " + d);
//                        }
//                        break;
//
//                    case SHORT16:
//                        System.out.println("        SHORT VALS");
//                        short shortdata[] = structure.getShortData();
//                        for (short i : shortdata) {
//                            System.out.println("        0x" + Integer.toHexString(i));
//                        }
//                        break;
//
//                    case INT32:
//                    case UINT32:
//                        System.out.println("        INT VALS");
//                        int intdata[] = structure.getIntData();
//                        for (int i : intdata) {
//                            System.out.println("        0x" + Integer.toHexString(i));
//                        }
//                        break;
//
//                    case LONG64:
//                        System.out.println("        LONG VALS");
//                        long longdata[] = structure.getLongData();
//                        for (long i : longdata) {
//                            System.out.println("        0x" + Long.toHexString(i));
//                        }
//                        break;
//
//                    case CHAR8:
//                    case UCHAR8:
//                        System.out.println("        BYTE VALS");
//                        byte bytedata[] = structure.getByteData();
//                        for (byte i : bytedata) {
//                            System.out.println("         " + i);
//                        }
//                        break;
//
//                    case CHARSTAR8:
//                        System.out.println("        STRING VALS");
//                        String stringdata[] = structure.getStringData();
//                        for (String str : stringdata) {
//                            System.out.println("         " + str);
//                        }
//                        break;
//                }
//            }
//        }


         // print event
        System.out.println("Event = \n" + event.toXML());

         //now read it back in

         File fileIn = new File(fileName);
         System.out.println("read ev file: " + fileName + " size: " + fileIn.length());

         try {
             EvioReader fileReader = new EvioReader(fileName);
             System.out.println("\nnum ev = " + fileReader.getEventCount());
             System.out.println("dictionary = " + fileReader.getDictionaryXML() + "\n");



             event = fileReader.parseNextEvent();
             if (event == null) {
                 System.out.println("We got a NULL event !!!");
                 return;
             }
             System.out.println("Event = \n" + event.toXML());
             while ( (event = fileReader.parseNextEvent()) != null) {
                System.out.println("Event = " + event.toString());
             }
         }
         catch (IOException e) {
             e.printStackTrace();
         }
         catch (EvioException e) {
             e.printStackTrace();
         }
     }


    /**
       3 block headers (first 2 have 2 extra words each, last has 1 extra word).
       First block has 2 events. Second has 3 events.
       Last is empty final block.
    */
    static int data1[] = {
        0x00000014,
        0x00000001,
        0x0000000A,
        0x00000002,
        0x00000000,
        0x00000004,
        0x00000000,
        0xc0da0100,
        0x00000003,
        0x00000002,

        0x00000004,
        0x00010101,
        0x00000001,
        0x00000001,
        0x00000001,

        0x00000004,
        0x00010101,
        0x00000002,
        0x00000002,
        0x00000002,

        0x00000019,
        0x00000002,
        0x0000000A,
        0x00000003,
        0x00000000,
        0x00000004,
        0x00000000,
        0xc0da0100,
        0x00000001,
        0x00000002,

        0x00000004,
        0x00010101,
        0x00000003,
        0x00000003,
        0x00000003,

        0x00000004,
        0x00010101,
        0x00000004,
        0x00000004,
        0x00000004,

        0x00000004,
        0x00010101,
        0x00000005,
        0x00000005,
        0x00000005,

        0x00000009,
        0x00000003,
        0x00000009,
        0x00000000,
        0x00000000,
        0x00000204,
        0x00000000,
        0xc0da0100,
        0x00000003,
    };


    /** For writing out a 5GByte file. */
    public static void main22(String args[]) {
        // xml dictionary
        String dictionary =
                "<xmlDict>\n" +
                        "\t<xmldumpDictEntry name=\"bank\"           tag=\"1\"   num=\"1\"/>\n" +
                        "\t<xmldumpDictEntry name=\"bank of shorts\" tag=\"2\"   num=\"2\"/>\n" +
                        "\t<xmldumpDictEntry name=\"shorts pad0\"    tag=\"3\"   num=\"3\"/>\n" +
                        "\t<xmldumpDictEntry name=\"shorts pad2\"    tag=\"4\"   num=\"4\"/>\n" +
                        "\t<xmldumpDictEntry name=\"bank of chars\"  tag=\"5\"   num=\"5\"/>\n" +
                "</xmlDict>";


        // write evio file that has extra words in headers
        if (false) {
            try {
                byte[] be  = ByteDataTransformer.toBytes(data1, ByteOrder.BIG_ENDIAN);
                ByteBuffer buf = ByteBuffer.wrap(be);
                String fileName  = "/local/scratch/HeaderFile.ev";
                File file = new File(fileName);
                FileOutputStream fileOutputStream = new FileOutputStream(file);
                FileChannel fileChannel = fileOutputStream.getChannel();
                fileChannel.write(buf);
                fileChannel.close();
            }
            catch (Exception e) {
                e.printStackTrace();  //To change body of catch statement use File | Settings | File Templates.
            }

        }

        //create an event writer to write out the test events.
        String fileName  = "/local/scratch/gagik.evio";
        //String fileName  = "/local/scratch/BigFile3GBig.ev";
        //String fileName  = "/local/scratch/BigFile1GLittle.ev";
        //String fileName  = "/local/scratch/LittleFile.ev";
        //String fileName  = "/local/scratch/HeaderFile.ev";
        File file = new File(fileName);
        EvioEvent event;

        //large file with big events (10MB)
        if (false) {
            // data
            int[] intData  = new int[2500000];
            Arrays.fill(intData, 23);


            try {
                EventWriter eventWriterNew = new EventWriter(fileName, null, null, 1, 0,
                                                             4*1000, 3,
                                                             ByteOrder.BIG_ENDIAN, null,
                                                             true, false, null,
                                                             0, 0, 1, 1, CompressionType.RECORD_UNCOMPRESSED, 0, 0, 0);

                // event - bank of banks
                EventBuilder eventBuilder2 = new EventBuilder(1, DataType.INT32, 1);
                event = eventBuilder2.getEvent();
                event.appendIntData(intData);

                // 300 ev/file * 10MB/ev = 3GB/file
                for (int i=0; i < 300; i++) {
                    eventWriterNew.writeEvent(event);
                    System.out.print(".");
                }
                System.out.println("\nDONE");

                // all done writing
                eventWriterNew.close();
            }
            catch (IOException e) {
                e.printStackTrace();
            }
            catch (EvioException e) {
                e.printStackTrace();
            }
        }

        // large file with small events (1kB)
        if (false) {
            // data
            int[] intData  = new int[250];
            Arrays.fill(intData, 25);


            try {
                EventWriter eventWriterNew = new EventWriter(fileName, null, null, 1, 0,
                                                             4*10000000, 100000,
                                                             ByteOrder.BIG_ENDIAN, null,
                                                             true, false, null,
                                                             0, 0, 1, 1, CompressionType.RECORD_UNCOMPRESSED, 0, 0, 0);

                // event - bank of banks
                EventBuilder eventBuilder2 = new EventBuilder(1, DataType.INT32, 1);
                event = eventBuilder2.getEvent();
                event.appendIntData(intData);

                // 3000000 ev/file * 1kB/ev = 3GB/file
                for (int i=0; i < 3000000; i++) {
                    eventWriterNew.writeEvent(event);
                    if (i%100000== 0) System.out.print(i+" ");
                }
                System.out.println("\nDONE");

                // all done writing
                eventWriterNew.close();
            }
            catch (IOException e) {
                e.printStackTrace();
            }
            catch (EvioException e) {
                e.printStackTrace();
            }
        }

        // 1G file with small events (1kB)
        if (false) {
            // data
            int[] intData  = new int[63];
            Arrays.fill(intData, 25);


            try {
                EventWriter eventWriterNew = new EventWriter(fileName, null, null, 1, 0,
                                                              4*10000000, 6535,
                                                              ByteOrder.BIG_ENDIAN, null,
                                                              true, false, null,
                                                             0, 0, 1, 1, CompressionType.RECORD_UNCOMPRESSED, 0, 0, 0);

                // event - bank of banks
                EventBuilder eventBuilder2 = new EventBuilder(1, DataType.INT32, 1);
                event = eventBuilder2.getEvent();
                event.appendIntData(intData);

                // 4000000 ev/file * 260/ev = 1GB/file
                for (int i=0; i < 4000000; i++) {
                    eventWriterNew.writeEvent(event);
                    if (i%200000== 0) System.out.print(i+" ");
                }
                System.out.println("\nDONE");

                // all done writing
                eventWriterNew.close();
            }
            catch (IOException e) {
                e.printStackTrace();
            }
            catch (EvioException e) {
                e.printStackTrace();
            }
        }

        // small file
        if (false) {
            // data
            int[] intData  = new int[1];


            try {
                EventWriter eventWriterNew = new EventWriter(fileName, null, null, 1, 0,
                                                              4*1000, 3,
                                                              ByteOrder.BIG_ENDIAN, dictionary,
                                                              true, false, null,
                                                             0, 0, 1, 1, CompressionType.RECORD_UNCOMPRESSED, 0, 0, 0);

                // event - bank of banks
                EventBuilder eventBuilder2 = new EventBuilder(1, DataType.INT32, 1);
                event = eventBuilder2.getEvent();
                event.appendIntData(intData);

                // 300 ev/file * 4bytes/ev = 1.2kB/file
                for (int i=0; i < 300; i++) {
                    eventWriterNew.writeEvent(event);
                    System.out.print(".");
                }
                System.out.println("\nDONE");

                // all done writing
                eventWriterNew.close();
            }
            catch (IOException e) {
                e.printStackTrace();
            }
            catch (EvioException e) {
                e.printStackTrace();
            }
        }

        if (true) {
            System.out.println("\nTRY READING");
            int evCount;
            File fileIn = new File(fileName);
            System.out.println("read ev file: " + fileName + " size: " + fileIn.length());
            try {
                EvioReader fileReader = new EvioReader(fileName, false, true);
                evCount = fileReader.getEventCount();
                System.out.println("\nnum ev = " + evCount);
                System.out.println("blocks = " + fileReader.getBlockCount());
                System.out.println("dictionary = " + fileReader.getDictionaryXML() + "\n");


                int counter = 0;

                long t2, t1 = System.currentTimeMillis();

                while ( (event = fileReader.parseNextEvent()) != null) {
                    if (event == null) {
                        System.out.println("We got a NULL event !!!");
                        return;
                    }

//                    if (counter++ %10 ==0) {
//                        System.out.println("Event #" + counter + " =\n" + event);
//                        int[] d = event.getIntData();
//                        System.out.println("Data[0] = " + d[0] + ", Data[last] = " + d[d.length-1]);
//                        //            System.out.println("Event = \n" + event.toXML());
//                        //            while ( (event = fileReader.parseNextEvent()) != null) {
//                        //               System.out.println("Event = " + event.toString());
//                        //            }
//                    }
                }

                t2 = System.currentTimeMillis();
                System.out.println("Sequential Time = " + (t2-t1) + " milliseconds");




               fileReader.rewind();

                t1 = System.currentTimeMillis();

                for (int i=1; i <= evCount; i++) {
                    event = fileReader.parseEvent(i);
                    if (event == null) {
                        System.out.println("We got a NULL event !!!");
                        return;
                    }

//                    if (i %10 == 0) {
//                        System.out.println("Event #" + i + " =\n" + event);
//                        int[] d = event.getIntData();
//                        System.out.println("Data[0] = " + d[0] + ", Data[last] = " + d[d.length-1]);
//                    }

                }

                t2 = System.currentTimeMillis();
                System.out.println("Random access Time = " + (t2-t1) + " milliseconds");



            }
            catch (IOException e) {
                e.printStackTrace();
            }
            catch (EvioException e) {
                e.printStackTrace();
            }
        }


        if (false) {
            System.out.println("\nTRY READING");

            File fileIn = new File(fileName);
            System.out.println("read ev file: " + fileName + " size: " + fileIn.length());
            try {
                EvioReader fileReader = new EvioReader(fileName);
                System.out.println("\nev count= " + fileReader.getEventCount());
                System.out.println("dictionary = " + fileReader.getDictionaryXML() + "\n");


                System.out.println("ev count = " + fileReader.getEventCount());

            }
            catch (IOException e) {
                e.printStackTrace();
            }
            catch (EvioException e) {
                e.printStackTrace();
            }
        }


        System.out.println("DONE READING");

    }



}
