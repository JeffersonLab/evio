//
// Copyright 2022, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


/**
 * <p>
 * @file Read the example of (truncated) hall D data file and convert to event files.
 * </p>
 */


#include <unistd.h>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <cinttypes>

// EVIO reading
#include "eviocc.h"


using namespace evio;

#define INPUT_LENGTH_MAX 256



/**
 * Doing things this way is like reading a buffer bit-by-bit
 * and passing it off to the parser bit-by-bit
 * @param argc
 * @param argv
 * @return
 */
int main(int argc, char **argv) {

    bool debug = false;


    std::string filename = "/daqfs/home/timmer/evioDataFiles/clas_006586.evio.00001";
    std::string basename = "ev_";

    fprintf(stderr, "Create reading object for file = %s\n", filename.c_str());
    fflush(stderr);

    auto reader = EvioReader(filename);


    uint64_t totalBytes = 0L;
    int avgBufBytes = 0;


    int counter = 0;
    int byteSize = 0;
    int bufsSent = 0;
    int index = 0, evCount = 0;
    int bigEventCnt = 0;
    uint64_t totalBigEvBytes = 0L;

    fprintf(stderr, "Start reading file = %s\n", filename.c_str());
    fflush(stderr);


//    try {

        while (true) {

            auto ev = reader.nextEvent();
            if (ev == nullptr) {
                break;
            }
            std::vector <uint8_t> dataVec = ev->getRawBytes();


            uint32_t bytes = ev->getTotalBytes();
            totalBytes += bytes;

            if (bytes >= 30000) {
                bigEventCnt++;
                totalBigEvBytes += bytes;
            }

            counter++;

            fprintf(stderr, "event %d, size %u, total size = %" PRIu64 "\n", counter, bytes, totalBytes);
            fflush(stderr);

        }
//    }
//    catch (std::runtime_error &ex) {
//        std::cerr << "Exception reading events = " << ex.what() << std::endl;
//    }

    reader.rewind();

    avgBufBytes = totalBytes / counter;
    std::cerr << "Data byte total = " << totalBytes << ", processed events = " << counter << ", avg buf size = " << avgBufBytes << std::endl;
    std::cerr << "Big ev data byte total = " << totalBigEvBytes << ", big events = " << bigEventCnt << std::endl;

    bufsSent = 0;



    while (false) {

        if (bufsSent >= counter) {
            break;
        }

        auto ev = reader.nextEvent();
        if (ev == nullptr) {
            break;
        }
        std::vector <uint8_t> dataVec = ev->getRawBytes();


        uint32_t bytes = ev->getTotalBytes();
        bufsSent++;


        char *buf = (char *)(&dataVec[0]);

        // Write out file with all times and levels
        std::string fileName = basename + std::to_string(bufsSent);
        FILE *fp = fopen (fileName.c_str(), "w");
        fwrite(buf,bytes,1,fp);
        fclose(fp);

        // Do this for the /daqfs/java/clas_005038.1231.hipo  file
        if (bufsSent > 100) {
            break;
        }
        fprintf(stderr, "wrote event %d, size %u to file %s\n", bufsSent, bytes, fileName.c_str());

    }

    return 0;
}
