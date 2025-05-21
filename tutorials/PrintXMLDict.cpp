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

    try {
        // Open the EVIO6 file using EvioReader
        EvioReader reader(filename);

        // Check for an embedded XML dictionary
        if (reader.hasDictionaryXML()) {
            // Retrieve the raw XML dictionary text
            std::string xmlDict = reader.getDictionaryXML();
            // Print the XML dictionary to standard output
            std::cout << xmlDict;
            if (!xmlDict.empty() && xmlDict.back() != '\n') {
                std::cout << std::endl;  // ensure a newline at end, if not already present
            }
        } else {
            // No dictionary present in the file
            std::cout << "No XML dictionary found." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: Unable to read EVIO file. " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
