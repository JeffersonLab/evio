#include <iostream>
#include <string>

#include "eviocc.h"  // EVIO C++ API all inside

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
        // Open the HIPO file using EvioReader
        EvioReader reader(filename);

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
        std::cout << "Byte order: " << reader.getByteOrder().getName() << std::endl;
        std::cout << "Num blocks (aka records): " << reader.getBlockCount() << std::endl;


    } catch (const std::exception& e) {
        std::cerr << "Error: Unable to read HIPO file. " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
