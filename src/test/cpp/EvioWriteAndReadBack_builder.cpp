#include "EvioTestHelper.h"  // EVIO C++ API (EVIO 6 main branch)

using namespace evio;

// Forward declaration for a few functions
void evioReadStep(std::string filename);

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

    for (int i = 0; i < nEvents; ++i) {

        // Build a new event (top-level bank) with tag=1, type=BANK, num=1
        //-------------------------------------
        uint16_t tag = 1;
        uint8_t  num = 1;
        // if(i == 0) {
        std::shared_ptr<EventBuilder> builder = std::make_shared<EventBuilder>(tag, DataType::BANK, num);
        std::shared_ptr<EvioEvent> event = builder->getEvent();
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
        std::shared_ptr<evio::EvioBank> bankFloats = EvioBank::getInstance(tag+11, DataType::FLOAT32, num+11); 
        // // Write our data into bank
        builder->setFloatData(bankFloats, floatVec.data(), floatVec.size());
        builder->addChild(event, bankFloats);

        // Write the completed event to file
        writerV6->writeEvent(event);

        // event.reset();
        // bankFloats.reset();
        // builder.reset();
        // floatVec.clear();
    }

    
    writerV6->close();  // close file writer (flush remaining data)&#8203;:contentReference[oaicite:6]{index=6}
    std::cout << "Wrote " << nEvents << " events to file." << std::endl;
    
    evioReadStep(evioHelperObj->directory + "/" + evioHelperObj->baseNameV6);
    
    return 0;
}


void evioReadStep(std::string filename) {
    
    EvioReader reader(filename);
    
    // Find out how many events the file contains
    int32_t evCount = reader.getEventCount();
    int32_t eventsToCheck = 10; // Number of events to check with explicit output
    std::cout << "File has " << evCount << " events" << std::endl;

    // Does it contain a dictionary?
    if (reader.hasDictionaryXML()) {
        std::string dict = reader.getDictionaryXML();
        std::cout << "Dictionary = " << dict << std::endl;
    }

    // Does it contain a first event?
    std::shared_ptr<EvioEvent> fe = reader.getFirstEvent();
    if (fe != nullptr) std::cout << "First event size = " << fe->getTotalBytes() << " bytes" << std::endl;

    std::cout << "EVIO Ver: " << reader.getEvioVersion() << std::endl;
    std::cout << "File Size: " << reader.fileSize() << std::endl;
    std::cout << "Event count: " << reader.getEventCount() << std::endl;
    std::cout << "Has first event?: " << reader.hasFirstEvent() << std::endl;
    
    // Look at each event with random-access type method (start at 1)
    std::cout << "Print out regular events raw data:" << std::endl;
    for (int i = 0; i < evCount; i++) {

        if (i >= eventsToCheck) {
            std::cout << "Reached maximum number of events to read: " << eventsToCheck << std::endl;
            break;
        }

        auto ev = reader.parseEvent(i+1);
        std::cout << "      got & parsed ev " << (i+1) << std::endl;
        std::cout << "      event ->\n" << ev->toString() << std::endl;
        auto& dataVec = ev->getRawBytes();
        std::cout << "Event has tag = " << ev->getHeader()->getTag() << std::endl;
        std::cout << "Event structure type = " << ev->getStructureType().toString() << std::endl;
        std::vector< std::shared_ptr< BaseStructure > > & children = ev->getChildren();
        std::cout << "Event has " << children.size() << " children" << std::endl;

        // Now loop over children, if any
        for (size_t j = 0; j < children.size(); j++) {
            std::cout << "Child " << j << " tag = " << children[j]->getStructureType().toString() << std::endl;

            std::cout << "NChildren: " << children[j]->getChildCount() << std::endl;
            std::cout << "Num items stored: " << children[j]->getNumberDataItems() << std::endl;
            std::cout << "Data type: " << children[j]->getHeader()->getDataType().toString() << std::endl;
            if(children[j]->getHeader()->getDataType() == DataType::FLOAT32) {
                std::cout << "Data: ";
                std::vector<float> data_vec = children[j]->getFloatData();
                for (size_t l = 0; l < data_vec.size(); l++) {
                    std::cout << data_vec[l] << " ";
                }

                // for (size_t k = 0; k < children[j]->getChildCount(); k++) {
                //     std::cout << "Child " << j << ",  subchild " << k << " " << children[j]->getChildAt(k)->getStructureType().toString() << std::endl;
                //     std::cout << "Subchild " << k << " datatype: " << children[j]->getChildAt(k)->getHeader()->getDataType().toString() << std::endl;
                //     std::cout << "nsubchildren: " << children[j]->getChildAt(k)->getChildCount() << std::endl;
                //     std::cout << "num items stored: " << children[j]->getChildAt(k)->getNumberDataItems() << std::endl;
                //     // If data type is int, dump the data
                //     std::cout << "Data type: " << children[j]->getChildAt(k)->getHeader()->getDataType().toString() << std::endl;
                //     if(children[j]->getChildAt(k)->getHeader()->getDataType() == DataType::FLOAT32) {
                //         std::cout << "Data: ";
                //         std::vector<float> data_uint_vec = children[j]->getChildAt(k)->getFloatData();
                //         for (size_t l = 0; l < data_uint_vec.size(); l++) {
                //             std::cout << data_uint_vec[l] << " ";
                //         }
                //         std::cout << std::endl;
                //     }
                // }
            }
            std::cout << std::endl;
        
        
        reader.rewind();

        }
    }

    return;
}

    // Go back to beginning of file

    // // Use sequential access to events
    // std::shared_ptr<EvioEvent> ev = nullptr;
    // int counter = 0;
    // while ((ev = reader.parseNextEvent()) != nullptr) {
    //     counter++;
    //     if (counter >= eventsToCheck) break;
    //     // do something with event
    //     std::cout << "Event has tag = " << ev->getHeader()->getTag() << std::endl;
    // }    
