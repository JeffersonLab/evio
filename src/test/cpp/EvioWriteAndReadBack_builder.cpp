#include "EvioTestHelper.h"  // EVIO C++ API (EVIO 6 main branch)

using namespace evio;

int main(int argc, char* argv[]) {

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

    std::shared_ptr<EventBuilder> builder = NULL;
    std::shared_ptr<EvioEvent> event = NULL;

    for (int i = 0; i < nEvents; ++i) {

        // Build a new event (top-level bank) with tag=1, type=BANK, num=1
        //-------------------------------------
        uint16_t tag = 1;
        uint8_t  num = 1;
        // if(i == 0) {
        builder = std::make_shared<EventBuilder>(tag, DataType::BANK, num);
        event = builder->getEvent();
        // }
        // else {
        //     builder = std::make_shared<EventBuilder>(event);
        // }
        
        std::vector<float> floatVec = evioHelperObj->genXYZT(i); // generate pseudo x, y, z, time values
        // std::vector<float> floatVec = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f}; // Example data for testing

        // Now to start defining event

        // // THE OVERBANK
        // // First child of event = bank of banks
        // auto bankBanks = EvioBank::getInstance(tag+1, DataType::BANK, num+1);
        // builder->addChild(event, bankBanks);

        // // (SUB)BANK 1 OF 1
        // // Create first (& only) child of bank of banks = bank of floats
        auto bankFloats = EvioBank::getInstance(tag+11, DataType::FLOAT32, num+11); 
        // // Write our data into bank
        builder->setFloatData(bankFloats, floatVec.data(), floatVec.size());
        builder->addChild(event, bankFloats);

        // // Write the completed event to file
        // writerV6->writeEvent(event);
    }

    writerV6->close();  // close file writer (flush remaining data)&#8203;:contentReference[oaicite:6]{index=6}
    std::cout << "Wrote " << nEvents << " events to file." << std::endl;
    return 0;

}
