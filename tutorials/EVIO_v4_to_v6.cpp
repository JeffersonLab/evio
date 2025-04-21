#include <iostream>
#include <string>

// Include EVIO6 C++ headers (adjust the include paths as needed for your installation)
#include "eviocc.h"  // EVIO C++ API (EVIO 6 main branch)

// Use the EVIO namespace for convenience
using namespace evio;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <evio_file>" << std::endl;
        return 1;
    }
    std::string f_in  = argv[1];
    std::string f_out = argv[2];

    // Read / write parameters
    uint32_t maxRecordBytes     = 1000000;
    uint32_t maxEventsPerRecord = 1000;
    uint32_t bufferBytes        = 1000000;
    

    try {
        // Open the EVIO6 file using EvioReader
        EvioReader reader(f_in);
        // EvioCompactReader reader(f_in);

        EventWriter   writerV6(f_out, "", "", 1, 0,
            maxRecordBytes, maxEventsPerRecord,
            ByteOrder::ENDIAN_LOCAL, "", true, false,
            nullptr, 1, 0, 1, 1,
            Compressor::CompressionType::UNCOMPRESSED,
            0, 0, bufferBytes);

        for(int32_t i = 0; i < reader.getEventCount(); ++i) {

            // **Create a top-level event (bank of banks)** with tag=1, num=1
            EventBuilder builder(1, DataType::BANK, 1);
            std::shared_ptr<EvioEvent> event = builder.getEvent();
            
            auto ev = reader.parseEvent(i + 1);
            auto & dataVec = ev->getRawBytes();
            Util::printBytes(dataVec.data(), dataVec.size()," Event #" + std::to_string(i));

        }

    }
        



     catch (const std::exception& e) {
        std::cerr << "Error: could not open and convert " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
