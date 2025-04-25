#include <iostream>
#include <memory>
#include <random>

#include "eviocc.h"
#include "EvioTestHelper.h"  // EVIO C++ API (EVIO 6 main branch)

using namespace evio;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <number_of_events>\n";
        return 1;
    }
    int nEvents = std::stoi(argv[1]);

    EvioTestHelper evioHelperObj();

    // Output file names for each format
    std::string fileNameEvio4 = "events_v4.ev";   // EVIO version 4 file
    std::string fileNameEvio6 = "events_v6.ev";   // EVIO version 6 file
    std::string fileNameHipo  = "events.hipo";    // HIPO format file

    std::shared_ptr<EventWriterV4> writerV4   = evioHelperObj().defaultEventWriterV4();
    std::shared_ptr<EventWriter>   writerV6   = evioHelperObj().defaultEventWriter();
    std::shared_ptr<EventWriter>   writerHipo = evioHelperObj().defaultEventWriterHIPO();


    // **2. Loop over events and write each one.**
    for (int i = 0; i < nEvents; ++i) {
        // Create an event as a bank containing four 32-bit floats
        std::shared_ptr<EvioBank> event = EvioBank::getInstance(1, DataType::FLOAT32, 1);
        // Fill the bank's float data (x, y, z, time) with 0.0
        auto &floatData = event->getFloatData();
        floatData.assign(4, 0.0f);
        floatData = evioHelperObj().genXYZT(i); // generate pseudo x, y, z, time values
        event->updateFloatData();  // update internal length counters&#8203;:contentReference[oaicite:3]{index=3}

        // Write the event to each file format
        writerV4->writeEvent(event);
        writerV6->writeEvent(event);
        writerHipo->writeEvent(event);
    }

    // **3. Close all files to flush buffers.**
    writerV4->close();
    writerV6->close();
    writerHipo->close();

    std::cout << "Wrote " << nEvents << " events to EVIO4, EVIO6, and HIPO format files." << std::endl;
    return 0;
}
