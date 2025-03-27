//
// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100

#ifndef EVIO_TESTBASE_H
#define EVIO_TESTBASE_H

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


    class TestBase {

    protected:

        int32_t *int1;
        char    *byte1;
        int16_t *short1;
        int64_t *long1;

        uint32_t *uint1;
        unsigned char  *ubyte1;
        uint16_t *ushort1;
        uint64_t *ulong1;

        float *float1;
        double *double1;

        std::vector<int32_t> intVec;
        std::vector<char>    byteVec;
        std::vector<int16_t> shortVec;
        std::vector<int64_t> longVec;

        std::vector<uint32_t> uintVec;
        std::vector<unsigned char>  ubyteVec;
        std::vector<uint16_t> ushortVec;
        std::vector<uint64_t> ulongVec;

        std::vector<float> floatVec;
        std::vector<double> doubleVec;
        std::vector<std::string> stringsVec;

        std::vector<std::shared_ptr<CompositeData>> cDataVec;

        int runLoops = 1;
        int bufferLoops = 1;
        int dataElementCount = 3;
        int skip = 0;
        int bufSize = 200000;


        std::shared_ptr<ByteBuffer> buffer;

        // files for input & output
        std::string writeFileName0 = "./compactEvioBuildOld.ev";
        std::string writeFileName1 = "./compactEvioBuild.ev";
        std::string writeFileName2 = "./treeEvioBuild.ev";

        ByteOrder order{ByteOrder::ENDIAN_LOCAL};

        std::string dictionary;

    public:

        explicit TestBase();

        TestBase(size_t bufSize, ByteOrder const & order);

        std::shared_ptr<ByteBuffer> createCompactEventBuffer(uint16_t tag, uint8_t num,
                                                             ByteOrder const & order = ByteOrder::ENDIAN_LOCAL,
                                                             size_t bufSize = 200000,
                                                             std::shared_ptr<CompactEventBuilder> builder = nullptr);

        std::shared_ptr<ByteBuffer> createEventBuilderBuffer(uint16_t tag, uint8_t num,
                                                             ByteOrder const & order = ByteOrder::ENDIAN_LOCAL,
                                                             size_t bufSize = 200000);

        std::shared_ptr<EvioEvent> createEventBuilderEvent(uint16_t tag, uint8_t num);

        std::shared_ptr<ByteBuffer> createTreeBuffer(uint16_t tag, uint8_t num,
                                                     ByteOrder const & byteOrder = ByteOrder::ENDIAN_LOCAL,
                                                     size_t bSize = 200000);

        std::shared_ptr<EvioEvent> createTreeEvent(uint16_t tag, uint8_t num);


    private:

        void setDataSize(int elementCount);


    };

}


#endif //EVIO_TESTBASE_H
