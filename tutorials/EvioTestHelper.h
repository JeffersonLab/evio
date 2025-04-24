//
// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100

#ifndef EVIO_TEST_HELPER_H
#define EVIO_TEST_HELPER_H

#include <string>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <memory>
#include <limits>
#include <unistd.h>

#include "eviocc.h"


namespace evio {


    class EvioTestHelper {

    protected:


    public:

        explicit EvioTestHelper();


        EventWriterV4 defaultEventWriterV4();

        EventWriter   defaultEventWriter() {
            return EventWriter(
                baseName,   
                directory,
                runType,
                runNumber,
                split,
                maxRecordSize,
                maxEventCount,
                byteOrder,
                xmlDictionary,
                overWriteOK,
                append,
                firstEvent,
                streamId,
                splitNumber,
                splitIncrement,
                streamCount,
                compressionType,
                compressionThreads,
                ringSize,
                bufferSize 
            );
        }

        EventWriter   makeEventWriterHIPO();
        
        float*        genXYZT();


    private:
        std::string dictionary;
        std::string baseName;
        const std::string & directory;
        const std::string & runType;
        uint32_t runNumber = 1;
        uint64_t split = 0;
        uint32_t maxRecordSize = 4194304;
        uint32_t maxEventCount = 10000;
        const ByteOrder & byteOrder = ByteOrder::nativeOrder();
        const std::string & xmlDictionary = "";
        bool     overWriteOK = true;
        bool     append = false;
        std::shared_ptr< EvioBank >firstEvent = nullptr;
        uint32_t streamId = 1;
        uint32_t splitNumber = 0;
        uint32_t splitIncrement = 1;
        uint32_t streamCount = 1;
        Compressor::CompressionType compressionType = Compressor::UNCOMPRESSED;
        uint32_t compressionThreads = 1;
        uint32_t ringSize = 0;
        size_t   bufferSize = 32100000;
    };

}


#endif //EVIO_TEST_HELPER_H
