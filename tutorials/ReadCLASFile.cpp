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
    std::string filename = argv[1];

    int maxEvents = 10;  // Maximum number of events to read

    try {
        // Open the EVIO6 file using EvioReader
        EvioReaderV4 reader(filename);

        // Check for an embedded XML dictionary
        if (reader.hasDictionaryXML()) {
            // Retrieve the raw XML dictionary text
            std::string xmlDict = reader.getDictionaryXML();
            // Print the XML dictionary to standard output
            std::cout << xmlDict;
            if (!xmlDict.empty() && xmlDict.back() != '\n') std::cout << std::endl;  // ensure a newline at end, if not already present
        } else {
            // No dictionary present in the file
            std::cout << "No XML dictionary found." << std::endl;
        }

        std::cout << "EVIO Ver: " << reader.getEvioVersion() << std::endl;
        std::cout << "File Size: " << reader.fileSize() << std::endl;
        std::cout << "Event count: " << reader.getEventCount() << std::endl;
        std::cout << "Has first event?: " << reader.hasFirstEvent() << std::endl;

        for (uint32_t i = 0; i < reader.getEventCount(); i++) {
            if(i+1 >= maxEvents) {
                std::cout << "Reached maximum number of events to read: " << maxEvents << std::endl;
                break;
            }
            std::shared_ptr<EvioEvent> ev = reader.parseEvent(i+1);
            std::cout << "      got & parsed ev " << (i+1) << std::endl;
            std::cout << "      event ->\n" << ev->toString() << std::endl;
            auto& dataVec = ev->getRawBytes();
            std::cout << "Event has tag = " << ev->getHeader()->getTag() << std::endl;
            std::cout << "Event structure type = " << ev->getStructureType().toString() << std::endl;
            std::vector< std::shared_ptr< BaseStructure > > & children = ev->getChildren();
            std::cout << "Event has " << children.size() << " children" << std::endl;
            std::shared_ptr< BaseStructure > child1 = children[0];
            for (size_t j = 0; j < children.size(); j++) {
                std::cout << "Child " << j << " tag = " << children[j]->getStructureType().toString() << std::endl;
                std::cout << "NChildren: " << children[j]->getChildCount() << std::endl;

                for (size_t k = 0; k < children[j]->getChildCount(); k++) {
                    std::cout << "Child " << j << ",  subchild " << k << " " << children[j]->getChildAt(k)->getStructureType().toString() << std::endl;
                    std::cout << "Subchild " << k << " datatype: " << children[j]->getChildAt(k)->getHeader()->getDataType().toString() << std::endl;
                    std::cout << "nsubchildren: " << children[j]->getChildAt(k)->getChildCount() << std::endl;
                    std::cout << "num items stored: " << children[j]->getChildAt(k)->getNumberDataItems() << std::endl;
                    if(children[j]->getChildAt(k)->getHeader()->getDataType() == DataType::UINT32) {
                        std::cout << "Data: ";
                        std::vector< uint32_t > data_uint_vec = children[j]->getChildAt(k)->getUIntData();
                        for (size_t l = 0; l < data_uint_vec.size(); l++) {
                            std::cout << data_uint_vec[l] << " ";
                        }
                        std::cout << std::endl;
                    }
                }
                std::cout << std::endl;
            }
            
            
            // Util::printBytes(dataVec.data(), dataVec.size()," Event #" + std::to_string(i+1));
        }



    } catch (const std::exception& e) {
        std::cerr << "Error: Unable to read EVIO file. " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
