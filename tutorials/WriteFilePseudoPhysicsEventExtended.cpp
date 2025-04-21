#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include "eviocc.h"

using namespace evio;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <number_of_events>\n";
        return 1;
    }
    int nEvents = std::stoi(argv[1]);

    // Output file names for EVIO4, EVIO6, and HIPO
    std::string fileNameEvio4 = "events_v4.ev";
    std::string fileNameEvio6 = "events_v6.ev";
    std::string fileNameHipo  = "events.hipo";

    // Writing parameters (similar to minimal version)
    uint32_t maxRecordBytes     = 1000000;
    uint32_t maxEventsPerRecord = 1000;
    uint32_t bufferBytes        = 1000000;

    // Create writers for EVIO4, EVIO6, and HIPO formats (same setup as minimal example)
    EventWriterV4 writerV4(fileNameEvio4, "", "", 1, 0, // string baseName, string &directory, string &runType, uint32_t runNumber, uint64_t split
                           maxRecordBytes, maxEventsPerRecord,
                           ByteOrder::ENDIAN_LOCAL, "", true, false, //const ByteOrder &byteOrder, string &xmlDictionary, bool overWriteOK, bool append
                           nullptr, 0, 0, 1, 1, bufferBytes); // shared_ptr< EvioBank > firstEvent, uint32_t streamId, uint32_t splitNumber, uint32_t splitIncrement, uint32_t streamCount, Compressor::CompressionType compressionType, uint32_t compressionThreads, uint32_t ringSize, uint32_t bufferSize

    EventWriter   writerV6(fileNameEvio6, "", "", 1, 0,
                           maxRecordBytes, maxEventsPerRecord,
                           ByteOrder::ENDIAN_LOCAL, "", true, false,
                           nullptr, 1, 0, 1, 1,
                           Compressor::CompressionType::UNCOMPRESSED,
                           0, 0, bufferBytes);

    EventWriter   writerHipo(fileNameHipo, "", "", 1, 0,
                             maxRecordBytes, maxEventsPerRecord,
                             ByteOrder::ENDIAN_LOCAL, "", true, false,
                             nullptr, 1, 0, 1, 1,
                             Compressor::CompressionType::UNCOMPRESSED,
                             0, 0, bufferBytes);

    // Loop to build and write events
    for (int i = 0; i < nEvents; ++i) {
        // **Create a top-level event (bank of banks)** with tag=1, num=1
        EventBuilder builder(1, DataType::BANK, 1);
        std::shared_ptr<EvioEvent> event = builder.getEvent();

        // Valid data types: int32, unint32, long64, ulong64, short16, ushort16, char8, uchar8, charstar8, float32, double64, bank, segment, tagsegment, composite, unknown32

        // **Bank 1: Floats** – create a bank holding 4 float32 values
        auto bankFloats = EvioBank::getInstance(10, DataType::FLOAT32, 1);
        auto &fData = bankFloats->getFloatData();
        fData.assign(4, 0.0f);           // four floats initialized to 0.0
        bankFloats->updateFloatData();   // update length for float data&#8203;:contentReference[oaicite:8]{index=8}
        builder.addChild(event, bankFloats);  // add this bank to the event&#8203;:contentReference[oaicite:9]{index=9}

        // **Bank 2: Integers** – create a bank holding some int32 values
        auto bankInts = EvioBank::getInstance(11, DataType::INT32, 2);
        auto &iData = bankInts->getIntData();
        iData.push_back(42);            // add an integer value 42
        iData.push_back(42);            // (for example, add twice to illustrate multiple entries)
        bankInts->updateIntData();      // update length for int data
        builder.addChild(event, bankInts);   // add this bank to the event&#8203;:contentReference[oaicite:10]{index=10}

        // **Bank 3: Strings** – create a bank holding one or more strings
        auto bankStrings = EvioBank::getInstance(12, DataType::CHARSTAR8, 3);
        auto &sData = bankStrings->getStringData();
        sData.emplace_back("example");  // add a string value "example"
        // (multiple strings can be added to this vector as needed)
        bankStrings->updateStringData();      // update length for string data
        builder.addChild(event, bankStrings); // add this bank to the event

        // At this point, `event` contains three child banks (float, int, string).
        // **Write the constructed event to all three output files:**
        writerV4.writeEvent(event);
        writerV6.writeEvent(event);
        writerHipo.writeEvent(event);
    }

    // Close writers to flush and finalize files
    writerV4.close();
    writerV6.close();
    writerHipo.close();

    std::cout << "Wrote " << nEvents 
              << " composite events (floats+ints+strings) to EVIO4, EVIO6, and HIPO files.\n";
    return 0;
}
