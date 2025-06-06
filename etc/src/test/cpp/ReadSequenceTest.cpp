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
    class ReadSequenceTest {

        public:

        // Create a simple Event
        static std::shared_ptr<EvioEvent> generateEvioEvent() {
            try {
                EventBuilder builder(0x1234, DataType::INT32, 0x12);
                auto ev = builder.getEvent();

                int data[] {1};
                builder.appendIntData(ev, data, 1);
                return ev;
            }
            catch (EvioException & e) {
                return nullptr;
            }
        }


        // Write file with 10 simple events in it
        static void writeFile(std::string filename) {

            try {
                EventWriter writer(filename);

                // Create an event containing a bank of ints
                auto ev = generateEvioEvent();

                if (ev == nullptr) {
                    throw new EvioException("bad event generation");
                }

                for (int i=0; i < 10; i++) {
                    std::cout << "Write event #" << i << std::endl;
                    writer.writeEvent(ev);
                }

                writer.close();
            }
            catch (EvioException & e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        }


        static void readFile(std::string filename) {
            try {
                EvioReader reader(filename);
                int evCount = reader.getEventCount();

                std::cout << "Read " << evCount << " events using sequential reading" << std::endl;
                for (int i=0; i < evCount; i++) {
                    auto ev = reader.parseNextEvent();
                    std::cout << "got event " << i << std::endl;
                }
            }
            catch (EvioException & e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        }

    };
}


int main(int argc, char **argv) {

    try {
        std::string filename = "/tmp/myTestFile";
        evio::ReadSequenceTest tester;
        tester.writeFile(filename);
        tester.readFile(filename);
    }
    catch (std::runtime_error & e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

}