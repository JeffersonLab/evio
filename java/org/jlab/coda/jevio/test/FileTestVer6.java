package org.jlab.coda.jevio.test;

import org.jlab.coda.hipo.*;
import org.jlab.coda.jevio.*;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.util.Arrays;
import java.util.BitSet;

/**
 * Test program for writing and reading evio version 6 files.
 * @author timmer
 * @since Nov 29, 2017
 */
public class FileTestVer6 {

    /**
      * Create a typical Hall D type of event in its structure.
      * Has 100 rocs of 20 entangled events with 200 bytes of
      * data from each roc, 30K per event.
      * @return  typical Hall D type of event.
      */
     static private ByteBuffer createEventBuffer(int ts) {

         try {
             //CompactEventBuilder b = new CompactEventBuilder(31000, ByteOrder.BIG_ENDIAN);
             CompactEventBuilder b = new CompactEventBuilder(ByteBuffer.allocateDirect(31000));
             int EBid = 1;
             int numRocs = 100;
             int numEvents = 20;
             long startingEventNumber = 2L;

             /////////////////////////////////////////////////////////////////////
             // Top level event, from PEB, 20 entangled events
             /////////////////////////////////////////////////////////////////////
             b.openBank(0xff50, numEvents, DataType.BANK);

             /////////////////////////////////////////////////////////////////////
             // Built trigger bank w/ timestamps, run # and runt type, 100 rocs
             /////////////////////////////////////////////////////////////////////
             b.openBank(0xff27, numRocs, DataType.SEGMENT);

             // 1st segment - unsigned longs
             b.openSegment(EBid, DataType.ULONG64);

             // timestamp for each event starting at 10
             long[] t = new long[numEvents+2];
             for (int i=0; i < numEvents; i++) {
                 t[i+1] = ts + i;
             }
             // first event number
             t[0] = startingEventNumber;
             // run #3, run type 4
             t[numEvents + 1] = (3L << 32) & 4L;
             b.addLongData(t);

             b.closeStructure();

             // 2nd segment - unsigned shorts
             b.openSegment(EBid, DataType.USHORT16);

             short[] s = new short[numEvents];
             Arrays.fill(s, (short)20);  // event types = 20 for all rocs
             b.addShortData(s);

             b.closeStructure();

             // Segment for each roc - unsigned ints.
             // Each roc has one timestamp for each event (10 + i)
             int[] rocTS = new int[numEvents];
             for (int i=0; i < numEvents; i++) {
                 rocTS[i] = 10 + i;
             }

             for (int i=0; i < numRocs; i++) {
                 int rocId = i;
                 b.openSegment(rocId, DataType.UINT32);
                 b.addIntData(rocTS);
                 b.closeStructure();
             }

             b.closeStructure();

             /////////////////////////////////////////////////////////////////////
             // End of trigger bank
             /////////////////////////////////////////////////////////////////////

             /////////////////////////////////////////////////////////////////////
             // One data bank for each roc
             /////////////////////////////////////////////////////////////////////

             int[] data = new int[50];     // 200 bytes
             Arrays.fill(data, 0xf0f0f0f0);
             data[0] = (int)startingEventNumber;
             data[1] = 10; // TS

             for (int i=0; i < numRocs; i++) {
                 int rocId = i;
                 // Data Bank
                 b.openBank(rocId, numEvents, DataType.BANK);

                 // Data Block Bank
                 int detectorId = 10*i;
                 b.openBank(detectorId, numEvents, DataType.INT32);
                 b.addIntData(data);
                 b.closeStructure();

                 b.closeStructure();
             }

             /////////////////////////////////////////////////////////////////////
             // End of event
             /////////////////////////////////////////////////////////////////////
             b.closeAll();

             // Returned buffer is read to read
             return b.getBuffer();
         }
         catch (EvioException e) {
             e.printStackTrace();
         }

         return null;
     }

    /**
     * Create a typical Hall D type of event in its structure.
     * Has 100 rocs of 20 entangled events with 200 bytes of
     * data from each roc, 30K per event.
     * @return  typical Hall D type of event.
     */
    static private EvioEvent createEvioEvent(int ts, int tag) {

        int EBid = 1;
        int numRocs = 100;
        int numEvents = 20;
        long startingEventNumber = 2L;

        try {

            /////////////////////////////////////////////////////////////////////
            // Top level event, from PEB, 20 entangled events
            /////////////////////////////////////////////////////////////////////
            EventBuilder builder = new EventBuilder(tag, DataType.BANK, numEvents);
            EvioEvent event = builder.getEvent();

            /////////////////////////////////////////////////////////////////////
            // Built trigger bank w/ timestamps, run # and runt type, 100 rocs
            /////////////////////////////////////////////////////////////////////
            // bank of segments
            EvioBank bankSegs = new EvioBank(0xff27, DataType.SEGMENT, numRocs);
            builder.addChild(event, bankSegs);

            // 1st segment - unsigned longs

            // timestamp for each event starting at 10
            long[] t = new long[numEvents+2];
            for (int i=0; i < numEvents; i++) {
                t[i+1] = ts + i;
            }
            // first event number
            t[0] = startingEventNumber;
            // run #3, run type 4
            t[numEvents + 1] = (3L << 32) & 4L;

            EvioSegment segment = new EvioSegment(EBid, DataType.ULONG64);
            segment.appendLongData(t);
            builder.addChild(bankSegs, segment);

            // 2nd segment - unsigned shorts

            short[] s = new short[numEvents];
            Arrays.fill(s, (short)20);  // event types = 20 for all rocs
            EvioSegment segment2 = new EvioSegment(EBid, DataType.USHORT16);
            segment2.appendShortData(s);
            builder.addChild(bankSegs, segment2);

            // Segment for each roc - unsigned ints.
            // Each roc has one timestamp for each event (10 + i)
            int[] rocTS = new int[numEvents];
            for (int i=0; i < numEvents; i++) {
                rocTS[i] = 10 + i;
            }

            for (int i=0; i < numRocs; i++) {
                int rocId = i;
                EvioSegment seg = new EvioSegment(rocId, DataType.UINT32);
                seg.appendIntData(rocTS);
                builder.addChild(bankSegs, seg);
            }

            /////////////////////////////////////////////////////////////////////
            // End of trigger bank
            /////////////////////////////////////////////////////////////////////

            /////////////////////////////////////////////////////////////////////
            // One data bank for each roc
            /////////////////////////////////////////////////////////////////////

            int[] data = new int[50];     // 200 bytes
            Arrays.fill(data, 0xf0f0f0f0);
            data[0] = (int)startingEventNumber;
            data[1] = 10; // TS

            for (int i=0; i < numRocs; i++) {
                int rocId = i;
                // Data Bank
                EvioBank dataBank = new EvioBank(rocId, DataType.BANK, numEvents);
                builder.addChild(event, dataBank);

                // Data Block Bank
                int detectorId = 10*i;
                EvioBank dataBlockBank = new EvioBank(detectorId, DataType.INT32, numEvents);
                dataBlockBank.appendIntData(data);
                builder.addChild(dataBank, dataBlockBank);
            }

            /////////////////////////////////////////////////////////////////////
            // End of event
            /////////////////////////////////////////////////////////////////////

            // Returned buffer is read to read
            return event;
        }
        catch (EvioException e) {
            e.printStackTrace();
        }

        return null;
    }


    // xml dictionary
    private static String xmlDictionary =
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


    // xml dictionary
    private static String dictionary =
            "<xmlDict>\n" +
                    "  <xmldumpDictEntry name=\"bank\"           tag=\"1\"   num=\"1\"/>\n" +
                    "  <xmldumpDictEntry name=\"bank of shorts\" tag=\"2\"   num=\"2\"/>\n" +
                    "  <xmldumpDictEntry name=\"shorts pad0\"    tag=\"3\"   num=\"3\"/>\n" +
                    "  <xmldumpDictEntry name=\"shorts pad2\"    tag=\"4\"   num=\"4\"/>\n" +
                    "  <xmldumpDictEntry name=\"bank of chars\"  tag=\"5\"   num=\"5\"/>\n" +
            "</xmlDict>";


    public static String curFileName;

    /** For WRITING a local file. */
    public static void main(String args[]) {

        String fileName  = "/Users/timmer/fileTest.ev.v6";
        String splitFileName  = "/Users/timmer/fileTest.ev.v6.0";
        File file = new File(fileName);
        file.delete();
        File sfile = new File(splitFileName);
        sfile.delete();

        ByteBuffer myBuf = null;

        // Do we overwrite or append?
        boolean append = false;

        // Do we write to file or buffer?
        boolean useFile = true;

        try {
            // Create an event writer to write out the test events to file
            EventWriter writer;
            int targetRecordBytes = 8*1024*1024; // 8 MB
            int splitBytes = 200000000;

            ByteOrder order = ByteOrder.BIG_ENDIAN;

            // Define a first event and write it into byte array
            EvioEvent eventFirst = createEvioEvent(100, 0xff50);
            byte[] efArray = new byte[eventFirst.getTotalBytes()];
            ByteBuffer efBuf = ByteBuffer.wrap(efArray);
            efBuf.order(order);
            eventFirst.write(efBuf);

            if (useFile) {
                System.out.println("FileTest, write to file " + fileName + "\n");
                writer = new EventWriter(fileName, null,
                        null, 0, splitBytes,
                        50000, 0,
                         ByteOrder.BIG_ENDIAN, xmlDictionary,
                        false, append, null,
                        0, 0, 1,
                        1, 0, 1,
                        8);
//
//                public EventWriter(String baseName, String directory, String runType,
//                         int runNumber, long split,
//                         int maxRecordSize, int maxEventCount,
//                         ByteOrder byteOrder, String xmlDictionary,
//                         boolean overWriteOK, boolean append,
//                         EvioBank firstEvent, int streamId,
//                         int splitNumber, int splitIncrement, int streamCount,
//                         int compressionType, int compressionThreads, int ringSize)

            }
            else {
                // Create an event writer to write to buffer
                myBuf = ByteBuffer.allocate(10000);
                myBuf.order(order);
                writer = new EventWriter(myBuf, targetRecordBytes,
                        10000, null,
                        1, null, 0);
//                public EventWriter(ByteBuffer buf, int maxRecordSize, int maxEventCount,
//                            String xmlDictionary, int recordNumber,
//                            EvioBank firstEvent, int compressionType)
            }


            for (int i = 0; i < 10; i++) {
                // Top level event
                EvioEvent event = createEvioEvent(10 * (i + 1), 0xff50 + i);
                // Write event to file
                writer.writeEvent(event);
                // How much room do I have left in the buffer now?
                if (!useFile) {
                    System.out.println("Buffer has " + myBuf.remaining() + " bytes left");
                }
                else {
                    System.out.println("Wrote event w/ ts = " + (10*(i+1)));
                }
            }

            if (true) {
                // All done writing
                System.out.println("FileTest, call close()\n\n");
                writer.close();
            }

            if (useFile) {
                curFileName = splitFileName;
                System.out.println("FileTest, current file name = " + curFileName + "\n");
                Utilities.printBytes(splitFileName, 0, 600, "File");
            }
            else {
                Utilities.printBytes(myBuf, 0, 600, "Buffer");
            }

            boolean useSequentialRead = false;


            // Since we're splitting the file, .0 is tacked onto the end by default
            fileName = splitFileName;
            

            if (useFile) {
                // Mess with the file by removing bytes off the end
                // to simulate incomplete writes due to crashes.
                //removeBytesFromFileEnd(fileName, 5);
                System.out.println("\nCALLING readWrittenDataOldAPI on " + fileName + "\n");
                readWrittenDataOldAPI(fileName, null, useSequentialRead);
            }
            else {
                //ByteBuffer buf = writer.getBuffer();
                // remove header + 1 word if closed, else just 1 word
                //buf.limit(buf.limit() - 84);
                //Utilities.bufferToFile(fileName+"out", buf, true, false);


                //myBuf.flip();
                readWrittenDataOldAPI(null, myBuf, useSequentialRead);
            }

        }
        catch (Exception e) {
            e.printStackTrace();
        }

    }



    /** For WRITING a local file. */
    public static void main1(String args[]) {

        String fileName  = "/daqfs/home/timmer/evioFiles/fileTestSmall.ev";
        File file = new File(fileName);
        file.delete();

        ByteBuffer myBuf = null;

        // Do we overwrite or append?
        boolean append = false;

        // Do we write to file or buffer?
        boolean useFile = false;


        EvioEvent eventFirst = null;

        try {
            // Create an event writer to write out the test events to file
            Writer writer;
            int targetBlockBytes = 8*1000*1000; // 8 MB
            int splitBytes = 200000000;
            int internalBufSize = 0;
            RecordOutputStream outputStream = new RecordOutputStream();

            ByteOrder order = ByteOrder.BIG_ENDIAN;

            // Define a first event and write it into byte array
            eventFirst = createEvioEvent(100, 0xff50);
            byte[] efArray = new byte[eventFirst.getTotalBytes()];
            ByteBuffer efBuf = ByteBuffer.wrap(efArray);
            efBuf.order(order);
            eventFirst.write(efBuf);

            if (useFile) {
                writer = new Writer(HeaderType.HIPO_FILE, order,
                                    10, targetBlockBytes, dictionary, efArray);

                writer.getFileHeader().setUserIntFirst(111);
                writer.getFileHeader().setUserIntSecond(222);
                writer.getFileHeader().setUserRegister(12121212);
                byte[] userHeader = null;
                //userHeader = new byte[20];
                //Arrays.fill(userHeader, (byte)4);
                writer.open(fileName, userHeader);
            }
            else {
                // Create an event writer to write to buffer
                myBuf = ByteBuffer.allocate(10000);
                myBuf.order(order);
                writer = new Writer(myBuf, order, targetBlockBytes, 10, null, null);
            }


            if (useFile) {
                for (int i = 0; i < 6; i++) {
                    // Top level event
                    EvioEvent event = createEvioEvent(10 * (i + 1), 0xff50 + i);
                    // Write event to file
                    writer.addEvent(event);
                    // How much room do I have left in the buffer now?
                    if (!useFile) {
                        System.out.println("Buffer has " + myBuf.remaining() + " bytes left");
                    }
                }
            }
            else {
                ByteBuffer record = Writer.createRecord(dictionary, efArray, writer.getByteOrder(), null, null);

                for (int i = 0; i < 6; i++) {
                    // Top level event
                    EvioEvent event = createEvioEvent(10 * (i + 1), 0xff50 + i);
                    // Write event to output stream
                    outputStream.addEvent(event);
                    // How much room do I have left in the buffer now?
                    System.out.println("Buffer has " + myBuf.remaining() + " bytes left");
                }
                outputStream.build(record);
                writer.writeRecord(outputStream);

            }


            if (true) {
                // All done writing
                System.out.println("FileTest, call close()");
                writer.close();
            }

//            if (xmlDictionary != null) {
//                EvioXMLDictionary dict = new EvioXMLDictionary(xmlDictionary);
//                NameProvider.setProvider(dict);
//            }

            if (useFile) {
                // Mess with the file by removing bytes off the end
                // to simulate incomplete writes due to crashes.
                //removeBytesFromFileEnd(fileName, 5);
                boolean useSequentialRead = false;
                readWrittenData(fileName, null, useSequentialRead);
            }
            else {
                ByteBuffer buf = writer.getBuffer();
                // remove header + 1 word if closed, else just 1 word
                buf.limit(buf.limit() - 84);
                Utilities.bufferToFile(fileName+"out", buf, true, false);
                readWrittenData(buf, false);
            }

        }
        catch (Exception e) {
            e.printStackTrace();
        }

    }


    private static void readWrittenData(ByteBuffer buf, boolean sequential)
            throws IOException, EvioException {

        try {
            Reader reader = new Reader(buf);
            System.out.println("read ev buffer: size = " + buf.remaining());

            // How many events in the file?
            int evCount = reader.getEventCount();
            System.out.println("Read buffer, got " + evCount + " events:\n");

            if (sequential) {
                // Use sequential access to look at events
                EvioNode node;
//                while ( (node = reader.getNextEventNode()) != null) {
//                    System.out.println("Event = " + node.toString());
//                }
                for (int i=0; i < evCount; i++) {
                    EvioNode ev = reader.getNextEventNode();
                    if (ev == null) {
                        System.out.println("Seq Event #" + i + " => node is null");
                    }
                    else {
                        System.out.println("Seq Event #" + i + " = " + ev.toString());
                    }
                }

            }
            else {
                // Use the "random access" capability to look at event
                for (int i=0; i < evCount; i++) {
                    EvioNode ev = reader.getEventNode(i);
                    if (ev == null) {
                        System.out.println("Event #" + i + " => node is null");
                    }
                    else {
                        System.out.println("Event #" + i + " = " + ev.toString());
                    }
                }
            }

        }
        catch (HipoException e) {
            e.printStackTrace();
        }


    }


    private static void readWrittenDataOldAPI(String fileName, ByteBuffer buf, boolean sequential)
            throws IOException, EvioException {

        try {

            File file;
            EvioReader reader;

            if (buf != null) {
                System.out.println("\nRead buffer:\n");

                reader = new EvioReader(buf);
                System.out.println("    buffer: pos = " + buf.position() + ", lim: " + buf.limit());
                // Buffer will not contain first event or dictionary.
            }
            else {
                System.out.println("\nRead file " + fileName + " as file:\n");
                file = new File(fileName);
                reader = new EvioReader(fileName, true);
                System.out.println("  file:  size = " + file.length());


                if (reader.hasDictionaryXML()) {
                    String xml = reader.getDictionaryXML();
                    System.out.println("  Dictionary = \n" + xml);
                }
                else {
                    System.out.println("  NO dictionary");
                }
            }

            // How many events in the file?
            int evCount = reader.getEventCount();
            System.out.println("Read file, got " + evCount + " events:\n");

            // When reading a file, EvioNodes will be null by definition
            if (sequential) {
                // Use sequential access to look at events
                for (int i = 0; i < evCount; i++) {
                    EvioEvent ev = reader.nextEvent();
                    if (ev == null) {
                        System.out.println("Seq Event #" + i + " => node is null");
                    }
                    else {
                        System.out.println("Seq Event #" + i + " = " + ev.toString());
                    }
                }

            }
            else {
                // Use the "random access" capability to look at event
                for (int i = 1; i <= evCount; i++) {
                    EvioEvent ev = reader.getEvent(i);
                    if (ev == null) {
                        System.out.println("Event #" + i + " => node is null");
                    }
                    else {
                        System.out.println("Event #" + i + " = " + ev.toString());
                    }
                }
            }


        }
        catch (EvioException e) {
            e.printStackTrace();
            Utilities.printBytes(buf, 0, 200, "Bad Event");
        }
    }



    private static void readWrittenData(String fileName, ByteBuffer buf, boolean sequential)
            throws IOException, EvioException {

        try {

            File file;
            Reader reader;

            if (buf != null) {
                System.out.println("\nRead buffer:\n");

                reader = new Reader(buf);
                System.out.println("    buffer: pos = " + buf.position() + ", lim: " + buf.limit());
                // Buffer will not contain first event or dictionary.
            }
            else {
                System.out.println("\nRead file " + fileName + " as file:\n");
                file = new File(fileName);
                reader = new Reader(fileName);
                FileHeader fh = reader.getFileHeader();
                System.out.println("  file header ->\n" + fh);
                System.out.println("  file:  size = " + file.length());

                if (reader.hasFirstEvent()) {
                    byte[] feArray = reader.getFirstEvent();
                    EvioEvent ev = EvioReader.parseEvent(feArray, 0, reader.getByteOrder());
                    System.out.println("  got first event = " + ev);
                }
                else {
                    System.out.println("  NO first event");
                }

                if (reader.hasDictionary()) {
                    String xml = reader.getDictionary();
                    System.out.println("  Dictionary = \n" + xml);
                }
                else {
                    System.out.println("  NO dictionary");
                }
            }

            // How many events in the file?
            int evCount = reader.getEventCount();
            System.out.println("Read file, got " + evCount + " events:\n");

            // When reading a file, EvioNodes will be null by definition
            if (buf != null) {
                if (sequential) {
                    // Use sequential access to look at events
                    for (int i = 0; i < evCount; i++) {
                        EvioNode ev = reader.getNextEventNode();
                        if (ev == null) {
                            System.out.println("Seq Event #" + i + " => node is null");
                        }
                        else {
                            System.out.println("Seq Event #" + i + " = " + ev.toString());
                        }
                    }

                }
                else {
                    // Use the "random access" capability to look at event
                    for (int i=1; i <= evCount; i++) {
                        EvioNode ev = reader.getEventNode(i);
                        if (ev == null) {
                            System.out.println("Event #" + i + " => node is null");
                        }
                        else {
                            System.out.println("Event #" + i + " = " + ev.toString());
                        }
                    }
                }
            }
            else {
                if (sequential) {
                    int i=0;
                    byte[] evBytes = reader.getPrevEvent();
                    System.out.println("Seq prev event = " + evBytes); i--;

                    evBytes = reader.getNextEvent();
                    System.out.println("Seq NEXT event = " + evBytes); i++;

                    evBytes = reader.getNextEvent();
                    System.out.println("Seq NEXT event = " + evBytes); i++;

                    evBytes = reader.getPrevEvent();
                    System.out.println("Seq prev event = " + evBytes); i--;

                    evBytes = reader.getNextEvent();
                    System.out.println("Seq NEXT event = " + evBytes); i++;

                    evBytes = reader.getNextEvent();
                    System.out.println("Seq NEXT event = " + evBytes); i++;

                    evBytes = reader.getNextEvent();
                    System.out.println("Seq NEXT event = " + evBytes); i++;

                    evBytes = reader.getNextEvent();
                    System.out.println("Seq NEXT event = " + evBytes); i++;

                    evBytes = reader.getNextEvent();
                    System.out.println("Seq NEXT event = " + evBytes); i++;

                    evBytes = reader.getNextEvent();
                    System.out.println("Seq NEXT event = " + evBytes); i++;

                    evBytes = reader.getNextEvent();
                    System.out.println("Seq NEXT event = " + evBytes); i++;

                    evBytes = reader.getNextEvent();
                    System.out.println("Seq NEXT event = " + evBytes); i++;

                    evBytes = reader.getPrevEvent();
                    System.out.println("Seq prev event = " + evBytes); i--;

                    evBytes = reader.getPrevEvent();
                    System.out.println("Seq prev event = " + evBytes); i--;

                    evBytes = reader.getPrevEvent();
                    System.out.println("Seq prev event = " + evBytes); i--;

                    evBytes = reader.getNextEvent();
                    System.out.println("Seq NEXT event = " + evBytes); i++;

                    evBytes = reader.getPrevEvent();
                    System.out.println("Seq prev event = " + evBytes); i--;

                    evBytes = reader.getPrevEvent();
                    System.out.println("Seq prev event = " + evBytes); i--;

                    evBytes = reader.getPrevEvent();
                    System.out.println("Seq prev event = " + evBytes); i--;

                    evBytes = reader.getPrevEvent();
                    System.out.println("Seq prev event = " + evBytes); i--;

                    evBytes = reader.getPrevEvent();
                    System.out.println("Seq prev event = " + evBytes); i--;

                    evBytes = reader.getPrevEvent();
                    System.out.println("Seq prev event = " + evBytes); i--;

                    evBytes = reader.getNextEvent();
                    System.out.println("Seq NEXT event = " + evBytes); i++;

//                    for (int i = 0; i < evCount; i++) {
//                        byte[] evBytes = reader.getNextEvent();
//
//                        if (evBytes == null) {
//                            System.out.println("Seq Event #" + i + " => node is null");
//                        }
//                        else {
//                            System.out.println("Seq Event #" + i + " = " + evBytes);
//                            Utilities.printBytes(evBytes, 0, 40, "seq event " + i);
//                        }
//                    }
                }
                else {
                    for (int i = 0; i < evCount; i++) {
                        byte[] evBytes = reader.getEvent(i);
                        if (evBytes == null) {
                            System.out.println("Event #" + i + " => node is null");
                        }
                        else {
                            System.out.println("Event #" + i + " = " + evBytes);
                            Utilities.printBytes(evBytes, 0, 40, "event " + i);
                        }
                    }
                }
            }

        }
        catch (HipoException e) {
            e.printStackTrace();
            Utilities.printBytes(buf, 0, 200, "Bad Event");
        }
    }




    private static void appendEvents(String fileName, EvioEvent ev, int count)
            throws IOException, EvioException {

        EventWriter writer = new EventWriter(fileName, null, null, 1, 0,
                                             4*16, 1000,
                                             ByteOrder.nativeOrder(), null,
                                             true, true, null,
                                             0, 0, 1, 1, 0, 1, 8);
        for (int i=0; i < count; i++) {
            // append event to file
            writer.writeEvent(ev);
            System.out.println("Appended event #" + (i+1) + ", Block #" + writer.getBlockNumber());
        }

        writer.close();
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
//EventWriter eventWriterNew = new  EventWriter(myBuf, 4*100, 3, dictionary, 1, null, 0);

            EventWriter eventWriterNew = new EventWriter(fileName, null, null, 1, 0,
                                                         4*1000, 3,
                                                         ByteOrder.BIG_ENDIAN, dictionary,
                                                         true, false, null,
                                                         0, 0, 1, 1, 0, 1, 8);

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
                                                             0, 0, 1, 1, 0, 1, 8);
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
                                                             0, 0, 1, 1, 0, 1, 8);

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
                                                              ByteOrder.BIG_ENDIAN, dictionary,
                                                              true, false, null,
                                                             0, 0, 1, 1, 0, 1, 8);
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
                                                             0, 0, 1, 1, 0, 1, 8);

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
