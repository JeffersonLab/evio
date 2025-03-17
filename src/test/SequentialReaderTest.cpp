//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include <iostream>
#include <fstream>

#include "TestBase.h"
#include "eviocc.h"


namespace evio {


    /**
     * Test swapping evio data.
     *
     * @date 03/17/2025
     * @author timmer
     */
    class SequentialReaderTest : public TestBase {
    };
}

    int main(int argc, char **argv) {

        try {
            int counter = 1;

            // Write 3 events to file
            std::string fileName  = "/tmp/seqReadTest.evio";
            evio::SequentialReaderTest tester;
            auto ev = tester.createEventBuilderEvent(1,1);
            evio::EventWriter writer(fileName);
            writer.writeEvent(ev);
            writer.writeEvent(ev);
            writer.writeEvent(ev);
            writer.close();

            std::cout << "read ev file: " << fileName << std::endl;


            evio::EvioReader fileReader(fileName, false, true);

            std::cout << "count events ..." << std::endl;
            size_t eventCount = fileReader.getEventCount();
            std::cout << "done counting events, " << eventCount << std::endl;

            // Read sequentially

            while (fileReader.parseNextEvent() != nullptr) {
                std::cout <<  "parseNextEvent # " << counter++ << std::endl;
            }

            // Now read non-sequentially

            std::cout << "get ev #1" << std::endl;
            auto event = fileReader.getEvent(1);

            std::cout << "get ev #2" << std::endl;
            event = fileReader.getEvent(2);

            std::cout << "get ev #3" << std::endl;
            event = fileReader.getEvent(3);



            std::cout << "goto ev #1" << std::endl;
            event = fileReader.gotoEventNumber(1);

            std::cout << "goto ev #2" << std::endl;
            event = fileReader.gotoEventNumber(2);

            std::cout << "goto ev #3" << std::endl;
            event = fileReader.gotoEventNumber(3);



            std::cout << "parse ev #1" << std::endl;
            event = fileReader.parseEvent(1);

            std::cout << "parse ev #2" << std::endl;
            event = fileReader.parseEvent(2);

            std::cout << "parse ev #3" << std::endl;
            event = fileReader.parseEvent(3);


            // Rewind
            std::cout << "rewind file" << std::endl;
            fileReader.rewind();


            // Read sequentially again
            counter = 1;
            while (fileReader.parseNextEvent() != nullptr) {
                std::cout << "parseNextEvent # " << counter++ << std::endl;
            }

        }
        catch (evio::EvioException & e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
    }
