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
#include <random>
#include <iostream>

#include "eviocc.h"


using namespace evio;


    class EvioTestHelper {

    protected:


    public:

        // Constructor
        explicit EvioTestHelper() {}

        std::shared_ptr<EventWriterV4> defaultEventWriterV4() {
            return std::make_shared<EventWriterV4>(
                baseNameV4,
                directory,
                runType,
                runNumber,
                split,
                maxRecordSize,
                maxEventCount,
                byteOrder,
                XMLdictionary,
                overWriteOK,
                append,
                firstEvent,
                streamId,
                splitNumber,
                splitIncrement,
                streamCount,
                bufferSize,
                bitInfo
            );
        } 

        std::shared_ptr<EventWriter>   defaultEventWriter(std::string baseName = "") {
            
            if (baseName.empty()) {
                baseName = baseNameV6; // Default to V6 if no base name is provided
            }
            
            return  std::make_shared<EventWriter>(
                baseName,   
                directory,
                runType,
                runNumber,
                split,
                maxRecordSize,
                maxEventCount,
                byteOrder,
                XMLdictionary,
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
        std::shared_ptr<EventWriter>   defaultEventWriterHIPO() {
            return std::make_shared<EventWriter>(
                baseNameHIPO,   
                directory,
                runType,
                runNumber,
                split,
                maxRecordSize,
                maxEventCount,
                byteOrder,
                XMLdictionary,
                overWriteOK,
                append,
                firstEvent,
                streamId,
                splitNumber,
                splitIncrement,
                streamCount,
                compressionTypeHIPO,
                compressionThreads,
                ringSize,
                bufferSize 
            );
        }
        
        std::vector<float> genXYZT(int i) {
            std::vector<float> x4(4); // 5th entry for pyevio bugfix
            x4[0] = gauss(gen);
            x4[1] = gauss(gen);
            x4[2] = 0.0f;
            x4[3] = i*2.008f;
            return x4;
        }
        std::string baseNameV4 = "testEventsV4_cppAPI.evio"; // base name of file to be created. If split > 1, this is the base name of all files created. If split < 1, this is the name of the only file created.
        std::string baseNameV6 = "testEventsV6_cppAPI.evio"; // base name of file to be created. If split > 1, this is the base name of all files created. If split < 1, this is the name of the only file created.
        std::string baseNameHIPO = "testEventsHIPO_cppAPI.hipo"; // base name of file to be created. If split > 1, this is the base name of all files created. If split < 1, this is the name of the only file created.
        std::string directory = "/home/jzarling/super_evio_dev/evio/tmp";     // directory in which file is to be placed

    private:

        // Random number generator for Gaussian (mean=0, sigma=0.1)
        std::random_device my_rd;
        std::mt19937 gen{ my_rd() };
        std::normal_distribution<float> gauss{0.0f, 0.1f};

            
        const std::string runType = "";       // name of run type configuration to be used in naming files
        uint32_t runNumber = 1;
        uint64_t split = 0;                 // if < 1, do not split file, write to only one file of unlimited size. Else this is max size in bytes to make a file before closing it and starting writing another. 
        uint32_t maxRecordSize = 33554432;  // (32 MiB) max number of uncompressed data bytes each record can hold. Value of < 8MB results in default of 8MB. The size of the record will not be larger than this size unless a single event itself is larger
        uint32_t maxEventCount = 10000;     // max number of events each record can hold. Value <= O means use default (1M).
        const ByteOrder & byteOrder = ByteOrder::nativeOrder();
        const std::string XMLdictionary = R"(
            <xmlDict>
              <bank name="floatBank" tag="10" num="1" type="float32">
                <leaf name="X"/>
                <leaf name="Y"/>
                <leaf name="Z"/>
                <leaf name="time"/>
                <leaf/>
              </bank>
              </xmlDict>
              )";
            //   <dictEntry name="jzint" tag="11" num="2" type="int32" />
            //   <dictEntry name="example" tag="12" num="3" type="charstar8" />
        bool     overWriteOK = true;
        bool     append = false;
        std::shared_ptr< EvioBank >firstEvent = nullptr;  // The first event written into each file (after any dictionary) including all split files; may be null. Useful for adding common, static info into each split file
        uint32_t streamId = 1;  // streamId number (100 > id > -1) for file name
        uint32_t splitNumber = 0;  // number at which to start the split numbers
        uint32_t splitIncrement = 1; // amount to increment split number each time another file is created
        uint32_t streamCount = 1; // total number of streams in DAQ
        Compressor::CompressionType compressionType = Compressor::UNCOMPRESSED;
        Compressor::CompressionType compressionTypeHIPO = Compressor::LZ4;
        uint32_t compressionThreads = 1; // number of threads doing compression simultaneously
        uint32_t ringSize = 0;  // number of records in supply ring. If set to < compressionThreads, it is forced to equal that value and is also forced to be a multiple of 2, rounded up.
        size_t   bufferSize = 33554432; // (32 MiB) number of bytes to make each internal buffer which will be storing events before writing them to a file. 9MB = default if bufferSize = 0.
        std::bitset<24>* bitInfo = nullptr; // EventWriterV4 only, set of bits to include in first block header
    };

#endif //EVIO_TEST_HELPER_H
