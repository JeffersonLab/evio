package org.jlab.coda.jevio.unit_tests;
import java.util.Random;
import java.nio.ByteOrder;

import org.jlab.coda.hipo.CompressionType;
// Import evio
import org.jlab.coda.jevio.*;

public class EvioTestHelper {
    private final Random random;

    // Parameters (simplified here for brevity)
    private final String baseNameV4 = "testEventsV4.evio"; // base name of file to be created. If split > 1, this is the base name of all files created. If split < 1, this is the name of the only file created.
    private final String baseNameV6 = "testEventsV6.evio"; // base name of file to be created. If split > 1, this is the base name of all files created. If split < 1, this is the name of the only file created.
    private final String baseNameHIPO = "testEventsHIPO.hipo"; // base name of file to be created. If split > 1, this is the base name of all files created. If split < 1, this is the name of the only file created.
    private final String directory  = ""; // directory in which file is to be placed
    private final String runType  = "";  // name of run type configuration to be used in naming files
    private final int runNumber  = 1;    // arbitrary, usually experiment-specific
    private final long split  = 0; // if < 1, do not split file, write to only one file of unlimited size. Else this is max size in bytes to make a file before closing it and starting writing another. 
    private final int maxRecordSize = 33554432; // (32 MiB) max number of uncompressed data bytes each record can hold. Value of < 8MB results in default of 8MB. The size of the record will not be larger than this size unless a single event itself is larger
    private final int maxEventCount = 10000;  // max number of events each RECORD can hold. Value <= O means use default (1M).
    private final ByteOrder byteOrder = ByteOrder.nativeOrder(); // options: BIG_ENDIAN, LITTLE_ENDIAN, nativeOrder()
    private final String XMLdictionary = 
            "<xmlDict>\n" +
            "  <bank name=\"floatBank\" tag=\"10\" num=\"1\" type=\"float32\">\n" +
            "    <leaf name=\"X\"/>\n" +
            "    <leaf name=\"Y\"/>\n" +
            "    <leaf name=\"Z\"/>\n" +
            "    <leaf name=\"time\"/>\n" +
            "    <leaf/>\n" +
            "  </bank>\n" +
            "  <dictEntry name=\"jzint\" tag=\"11\" num=\"2\" type=\"int32\" />\n" +
            "  <dictEntry name=\"example\" tag=\"12\" num=\"3\" type=\"charstar8\" />\n" +
            "</xmlDict>\n";
        private final boolean      overWriteOK = true;
        private final boolean      append = false;
        private final EvioBank firstEvent = null;  // The first event written into each file (after any dictionary) including all split files; may be null. Useful for adding common, static info into each split file
        private final int streamId = 1;  // streamId number (100 > id > -1) for file name
        private final int splitNumber = 0;  // number at which to start the split numbers
        private final int splitIncrement = 1; // amount to increment split number each time another file is created
        private final int streamCount = 1; // total number of streams in DAQ
        private final CompressionType compressionType = CompressionType.RECORD_UNCOMPRESSED;
        private final CompressionType compressionTypeHIPO = CompressionType.RECORD_COMPRESSION_LZ4;
        private final int compressionThreads = 1; // number of threads doing compression simultaneously
        private final int ringSize = 0;  // number of records in supply ring. If set to < compressionThreads, it is forced to equal that value and is also forced to be a multiple of 2, rounded up.
        private final int   bufferSize = 33554432; // (32 MiB) number of bytes to make each internal buffer which will be storing events before writing them to a file. 9MB = default if bufferSize = 0.

    // Constructor
    public EvioTestHelper() {
        this.random = new Random();
    }

    // Generate a float array similar to C++ vector<float>
    public float[] genXYZT(int i) {
        float[] x4 = new float[5]; // matches C++ size
        x4[0] = (float) random.nextGaussian() * 0.1f;
        x4[1] = (float) random.nextGaussian() * 0.1f;
        x4[2] = 0.0f;
        x4[3] = i * 2.008f;
        return x4;
    }

    // Simulated writer method (stub for example purposes)
    public EventWriter defaultEventWriter() {

        EventWriter w = null;

        try {
            w = new EventWriter(
                baseNameV6, 
                directory, 
                runType, 
                runNumber,
                split, 
                maxRecordSize, 
                maxEventCount, 
                byteOrder, 
                XMLdictionary,
                overWriteOK,
                append,
                firstEvent,
                streamId,
                splitNumber,
                splitIncrement,
                streamCount,
                compressionType,
                compressionThreads,
                ringSize,
                bufferSize);
        } 
        catch (EvioException e) {
            System.err.println("Error creating writer. Stack trace:\n ");
            e.printStackTrace();
        }
        return w;
    }
}
