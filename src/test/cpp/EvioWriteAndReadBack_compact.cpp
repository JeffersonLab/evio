#include "EvioTestHelper.h"  

using namespace evio;

int main(int argc, char** argv) {

    // Boilerplate
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <number_of_events>\n";
        return 1;
    }
    int nEvents = std::stoi(argv[1]);
    std::cout << "Writing " << nEvents << " events to files..." << std::endl;
    EvioTestHelper* evioHelperObj = new EvioTestHelper();

    // std::shared_ptr<EventWriterV4> writerV4   = evioHelperObj->defaultEventWriterV4();
    std::shared_ptr<EventWriter>   writerV6   = evioHelperObj->defaultEventWriter();
    // std::shared_ptr<EventWriter>   writerHipo = evioHelperObj->defaultEventWriterHIPO();

    for (int i = 0; i < nEvents; ++i) {


        // Data to write stored in these arrays
        // int8_t byteArray[8] = {0, 1, 2, 3, 4, 5, 6, 7};
        std::vector<float> floatArr = evioHelperObj->genXYZT(i); // generate pseudo x, y, z, time values;
        // Create the buffer to hold everything
        ByteBuffer buffer = ByteBuffer(8192);
        // Build event (bank of banks) with CompactEventBuilder object
        int tag = 1, num = 1;
        CompactEventBuilder* builder = new CompactEventBuilder(8192,ByteOrder::nativeOrder());
        //-------------------------------------
        // add top/event level bank of banks
        builder->openBank(tag, DataType::BANK, num);
        // add bank of banks to event
        builder->openBank(tag+1, DataType::BANK, num+1);
        // add bank of ints to bank of banks
        builder->openBank(tag+11, DataType::FLOAT32, num+11);
        // done with bank of ints, go to enclosing structure
        builder->addFloatData(floatArr.data(),floatArr.size());

        builder->closeStructure();
        // done with bank of banks
        builder->closeStructure();
        builder->closeAll();
        // There is no way to remove any structures
        // Get ready-to-read buffer from builder
        // (this sets the proper pos and lim in buffer)
        std::shared_ptr<ByteBuffer>& bytebbuffer = builder->getBuffer();

        // Create an event as a bank containing four 32-bit floats
        std::shared_ptr<EvioBank> event = EvioBank::getInstance(1, DataType::FLOAT32, 1);
        // Fill the bank's float data (x, y, z, time) with 0.0
        auto &floatData = event->getFloatData();
        floatData = evioHelperObj->genXYZT(i); // generate pseudo x, y, z, time values
        event->updateFloatData();  // update internal length counters&#8203;:contentReference[oaicite:3]{index=3}

        // Write the event to each file format
        // writerV4->writeEvent(event);
        writerV6->writeEvent(event);
        // writerHipo->writeEvent(event);
    }

    // Close the writers, flush buffers
    // writerV4->close();
    writerV6->close();
    // writerHipo->close();

    std::cout << "Wrote " << nEvents << " events to EVIO4, EVIO6, and HIPO format files." << std::endl;
    delete evioHelperObj;
    return 0;
}
