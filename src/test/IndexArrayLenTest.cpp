/**
 * Copyright (c) 2023, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 08/10/2023
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

// Used to test the reading of uncompressed evio buffer and file
// to see if changes made to handle a zero-length index array
// actually worked. Note that testing does NOT need to be done
// with compressed data since data in that form can only be
// written by the evio library which will always add an index array.

// 2 Records (2 events each) + Trailer
static uint32_t evioBuf[] = {
        // Record Header #1
        23,  // 14 header, 2 index, 0 user hdr, 3 word event, 4 word event
        1,  14,  2,  8,  6,  0,  0xc0da0100,  28,  0,  0,  0,  0,  0,

        // index array
        12, 16,

        // Event #1 len = 3, (tag 0, num 0, unsigned int), int val = 0
        2, 0x00100, 0,
        // Event #2 len = 4, (tag 1, num 1, unsigned int), int vals = 1
        3, 0x10101, 1, 1,

        // Record Header #2
        23,  // 14 header, 2 index, 0 user hdr, 3 word event, 4 word event
        2,  14,  2,  8,  6,  0,  0xc0da0100,  28,  0,  0,  0,  0,  0,

        // index array
        12, 16,

        // Event #1 len = 3, (tag 2, num 2, unsigned int), int val = 2
        2, 0x20102, 2,
        // Event #2 len = 4, (tag 3, num 3, unsigned int), int vals = 3
        3, 0x30103, 3, 3,

        // Trailer
        14,  // 14 header, 0 index, 0 user hdr, 0 events
        3,  14,  0,  0,  0x206,  0,  0xc0da0100,  0,  0,  0,  0,  0,  0
};

static size_t arrayLen = 4*60;

// 2 Records (2 events each) + Trailer ---> index array + user header
static uint32_t evioBuf3[] = {
        // Record Header #1
        24,  // 14 header, 2 index, 1 user hdr, 3 word event, 4 word event
        1,  14,  2,  8,  6,  4,  0xc0da0100,  28,  0,  0,  0,  0,  0,

        // index array
        12, 16,

        // user header
        1,

        // Event #1 len = 3, (tag 0, num 0, unsigned int), int val = 0
        2, 0x00100, 0,
        // Event #2 len = 4, (tag 1, num 1, unsigned int), int vals = 1
        3, 0x10101, 1, 1,

        // Record Header #2
        24,  // 14 header, 2 index, 1 user hdr, 3 word event, 4 word event
        2,  14,  2,  8,  6,  4,  0xc0da0100,  28,  0,  0,  0,  0,  0,

        // index array
        12, 16,

        // user header
        2,

        // Event #1 len = 3, (tag 2, num 2, unsigned int), int val = 2
        2, 0x20102, 2,
        // Event #2 len = 4, (tag 3, num 3, unsigned int), int vals = 3
        3, 0x30103, 3, 3,

        // Trailer
        14,  // 14 header, 0 index, 0 user hdr, 0 events
        3,  14,  0,  0,  0x206,  0,  0xc0da0100,  0,  0,  0,  0,  0,  0
};

static size_t arrayLen3 = 4*62;


// 2 Records (2 events each) + Trailer ---> no index array!
static uint32_t evioBuf2[] = {
        // Record #1
        21,  // 14 header, 0 index, 0 user hdr, 3 word event, 4 word event
        1,  14,  2,  0,  6,  0,  0xc0da0100,  28,  0,  0,  0,  0,  0,

        // index array, none

        // Event #1 len = 3, (tag 0, num 0, unsigned int), int val = 0
        2, 0x00100, 0,
        // Event #2 len = 4, (tag 1, num 1, unsigned int), int vals = 1
        3, 0x10101, 1, 1,

        // Record #2
        21,  // 14 header, 0 index, 0 user hdr, 3 word event, 4 word event
        2,  14,  2,  0,  6,  0,  0xc0da0100,  28,  0,  0,  0,  0,  0,

        // index array, none

        // Event #1 len = 3, (tag 2, num 2, unsigned int), int val = 2
        2, 0x20102, 2,
        // Event #2 len = 4, (tag 3, num 3, unsigned int), int vals = 3
        3, 0x30103, 3, 3,

        // Trailer
        14,  // 14 header, 0 index, 0 user hdr, 0 events
        3,  14,  0,  0,  0x206,  0,  0xc0da0100,  0,  0,  0,  0,  0,  0
};

static size_t arrayLen2 = 4*56;


// 2 Records (2 events each) + Trailer ---> no index array!
static uint32_t fileHdr[] = {
        // file header // non index, no user header, trailer pos = 4*(14+21+21)
        0x4556494F, 1, 14, 3, 0, 6, 0, 0xc0da0100, 0, 0,   224, 0,   0, 0
};

static size_t fileHdrLen = 4*14;



using namespace std;


namespace evio {


    class ReadWriteTest {


        string filename = "/tmp/indexArrayTest.evio";

    public:

        ReadWriteTest() {  }

        void readBuffer() {
            //auto sharedBuf = make_shared<ByteBuffer>(reinterpret_cast<uint8_t *>(evioBuf), arrayLen);
            //auto sharedBuf = make_shared<ByteBuffer>(reinterpret_cast<uint8_t *>(evioBuf3), arrayLen3);
            auto sharedBuf = make_shared<ByteBuffer>(reinterpret_cast<uint8_t *>(evioBuf2), arrayLen2);

            Reader reader(sharedBuf);
            ByteOrder order = reader.getByteOrder();

            int32_t evCount = reader.getEventCount();
            cout << "Read in buffer, got " << evCount << " events" << endl;

            cout << "Print out regular events:" << endl;
            uint32_t byteLen;

            for (int i=0; i < reader.getEventCount(); i++) {
                shared_ptr<uint8_t> data = reader.getEvent(i, &byteLen);
                Util::printBytes(data.get(), byteLen, "  Event #" + std::to_string(i));
            }
        }


        void readFile() {
            Reader reader(filename, true);
            ByteOrder order = reader.getByteOrder();

            int32_t evCount = reader.getEventCount();
            cout << "Read in buffer, got " << evCount << " events" << endl;

            cout << "Print out regular events:" << endl;
            uint32_t byteLen;

            for (int i=0; i < reader.getEventCount(); i++) {
                shared_ptr<uint8_t> data = reader.getEvent(i, &byteLen);
                Util::printBytes(data.get(), byteLen, "  Event #" + std::to_string(i));
            }
        }


        void writeFile() {
            // Open file and write header + records
            FILE *fp = fopen (filename.c_str(), "w");
            if (fp == nullptr) {
                printf("Failed to open statistics file %s, so don't keep any\n", filename.c_str());
            }

            size_t bytes = fwrite(fileHdr, fileHdrLen, 1, fp);
            bytes = fwrite(evioBuf2, arrayLen2, 1, fp);
            fclose(fp);
        }

    };


}




int main(int argc, char **argv) {



    evio::ReadWriteTest tester;

    //tester.readBuffer();
    tester.writeFile();
    tester.readFile();

    cout << endl << endl << "----------------------------------------" << endl << endl;

    return 0;
}

