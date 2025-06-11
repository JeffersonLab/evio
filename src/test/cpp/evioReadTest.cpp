// Description: Test program to read an evio file and print its contents.
// This taken mostly from ReadWriteV4Test.cpp
#include <iostream>
#include <string>
#include <EvioReader.h>
#include <ByteOrder.h>


using namespace std;
using namespace evio;

void readFile(string finalFilename) {

    std::cout << std::endl;
    std::cout << "--------------------------------------------" << std::endl;
    std::cout << "----------   Read from file   --------------" << std::endl;
    std::cout << "--------------------------------------------" << std::endl;

    EvioReader reader(finalFilename);
    ByteOrder & order = reader.getByteOrder();

    cout << "Read in file " << finalFilename  << " of byte order " << order.getName() << endl;
    cout << "Evio version: " << reader.getEvioVersion() << endl;
    int32_t evCount = reader.getEventCount();
    cout << "Got " << evCount << " events" << endl;

    string dict = reader.getDictionaryXML();
    if (dict.empty()) {
        cout << "\nNo dictionary" << endl;
    }
    else {
        cout << "\nGot dictionary:\n" << dict << endl;
    }

    auto pFE = reader.getFirstEvent();
    if (pFE != nullptr) {
        cout << "\nGot first Event:\n" << pFE->toString() << endl << endl;
    }
    else {
        cout << "\nNo first event" << endl;
    }

    cout << "Print out regular events:" << endl;

    for (int i=0; i < evCount; i++) {
        auto ev = reader.getEvent(i+1);
        cout << "\nEvent" << (i+1) << ":\n" << ev->toString() << endl;
    }
}

int main(int argc, char **argv) {

    if( argc > 1 ) {
        string filename = argv[1];
        readFile(filename);
    }else{
        std::cout << "Usage: evioReadTest <filename>" << std::endl;
        std::cout << "  where <filename> is the path to an evio file" << std::endl;
    }

    return 0;
}
