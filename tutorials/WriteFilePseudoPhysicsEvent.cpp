#include <iostream>
#include <memory>
#include <random>

#include "eviocc.h"  // EVIO C++ API (EVIO 6 main branch)

using namespace evio;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <number_of_events>\n";
        return 1;
    }
    int nEvents = std::stoi(argv[1]);

    // XML dictionary defining labels for the event structure
    std::string xmlDictionary = R"(
        <xmlDict>
          <bank name="floatBank" tag="10" num="1" type="float32">
            <leaf name="X"/>
            <leaf name="Y"/>
            <leaf name="Z"/>
            <leaf name="time"/>
            <leaf/>
          </bank>
          <dictEntry name="jzint" tag="11" num="2" type="int32" />
          <dictEntry name="example" tag="12" num="3" type="charstar8" />
        </xmlDict>
        )";


    // Output file names for each format
    std::string fileNameEvio4 = "events_v4.ev";   // EVIO version 4 file
    std::string fileNameEvio6 = "events_v6.ev";   // EVIO version 6 file
    std::string fileNameHipo  = "events.hipo";    // HIPO format file

    // Parameters for file writing (file split thresholds, etc.)
    uint32_t maxRecordBytes    = 1000000;  // max bytes per record (e.g., ~1 MB)
    uint32_t maxEventsPerRecord = 1000;    // max number of events per record
    uint32_t bufferBytes       = 1000000;  // internal buffer size in bytes

    // **1. Create writers for each format.** 
    // EVIO version 4 writer (using EventWriterV4)
    EventWriterV4 writerV4(fileNameEvio4,              // file path
                           "", "",                     // (directory, runType not used here)
                           1, 0,                       // runNumber=1, splitNumber=0 (no file splitting)
                           maxRecordBytes, maxEventsPerRecord,
                           ByteOrder::ENDIAN_LOCAL,    // use local endian byte order
                           xmlDictionary, true, false,            // give dictionary, overwrite existing, no append
                           nullptr, 0, 0,              // no “first event” provided
                           1, 1,                       // stream id=1, starting block number=1
                           bufferBytes);

    // EVIO version 6 writer (EventWriter) – outputs EVIO format by default&#8203;:contentReference[oaicite:0]{index=0}
    EventWriter writerV6(fileNameEvio6,                // file path
                         "", "",                       // (directory, runType)
                         1, 0,                         // runNumber=1, no file splitting
                         maxRecordBytes, maxEventsPerRecord,
                         ByteOrder::ENDIAN_LOCAL, 
                         xmlDictionary, true, false,              // dictionary, overwrite, no append
                         nullptr, 1, 0, 1, 1,          // no first event, stream id=1, block=1
                         Compressor::CompressionType::UNCOMPRESSED, 
                         0, 0,                         // no compression, so 0 threads and default level
                         bufferBytes);

    // HIPO format writer (also uses EventWriter, but with HIPO file header)&#8203;:contentReference[oaicite:1]{index=1}
    // We enable HIPO by specifying a compression type (e.g., LZ4) – the EVIO library will 
    // then produce a HIPO file header (ID = 0x43455248 for "HIPO") instead of "EVIO"&#8203;:contentReference[oaicite:2]{index=2}.
    EventWriter writerHipo(fileNameHipo, 
                           "", "", 
                           1, 0, 
                           maxRecordBytes, maxEventsPerRecord,
                           ByteOrder::ENDIAN_LOCAL, 
                           xmlDictionary, true, false, 
                           nullptr, 1, 0, 1, 1,
                           Compressor::CompressionType::UNCOMPRESSED,  // use LZ4 compression to trigger HIPO format
                           0, 0, 
                           bufferBytes);

    // Random number generator for Gaussian (mean=0, sigma=0.1)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> gauss(0.0, 0.1);


    // **2. Loop over events and write each one.**
    for (int i = 0; i < nEvents; ++i) {
        // Create an event as a bank containing four 32-bit floats
        std::shared_ptr<EvioBank> event = EvioBank::getInstance(1, DataType::FLOAT32, 1);
        // Fill the bank's float data (x, y, z, time) with 0.0
        auto &floatData = event->getFloatData();
        floatData.assign(5, 0.0f);
        floatData[0] = gauss(gen);
        floatData[1] = gauss(gen);
        floatData[2] = 0.;
        floatData[3] = i*2.008f; // to mock up rf time
        event->updateFloatData();  // update internal length counters&#8203;:contentReference[oaicite:3]{index=3}

        // Write the event to each file format
        writerV4.writeEvent(event);
        writerV6.writeEvent(event);
        writerHipo.writeEvent(event);
    }

    // **3. Close all files to flush buffers.**
    writerV4.close();
    writerV6.close();
    writerHipo.close();

    std::cout << "Wrote " << nEvents << " events to EVIO4, EVIO6, and HIPO format files." << std::endl;
    return 0;
}
