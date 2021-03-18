/**
 * Copyright (c) 2019, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 07/05/2019
 * @author timmer
 */


#include <functional>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <thread>
#include <memory>
#include <regex>
#include <iterator>
#include <fstream>


#include "eviocc.h"


using namespace std;


namespace evio {


    class ReadWriteTest {


    public:


        ReadWriteTest() {  }


        void readFile(string finalFilename) {

            Reader reader(finalFilename);
            ByteOrder order = reader.getByteOrder();

            int32_t evCount = reader.getEventCount();
            cout << "Read in file " << finalFilename << ", got " << evCount << " events" << endl;

            cout << "Print out regular events:" << endl;
            uint32_t byteLen;

            for (int i=0; i < reader.getEventCount(); i++) {
                shared_ptr<uint8_t> data = reader.getEvent(i, &byteLen);
                Util::printBytes(data.get(), byteLen, "  Event #" + std::to_string(i));
            }

        }

    };


}




int main(int argc, char **argv) {


    string filename  = "/Users/timmer/coda/evio/evio.dat";

    evio::ReadWriteTest tester;

    tester.readFile(filename);

    cout << endl << endl << "----------------------------------------" << endl << endl;

    return 0;
}

