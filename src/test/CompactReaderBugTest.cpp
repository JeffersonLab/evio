/**
 * Copyright (c) 2025, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 02/17/2025
 * @author timmer
 */


#include <cstdint>
#include <memory>
#include <limits>
#include <fstream>

#include "eviocc.h"


namespace evio {

    class CompactReaderBugTest {

    public:

        uint32_t wordDataIntCount = 35;

        /**
         * Evio version 6 format.
         * Create a record header which has a non-standard length (17 words instead of 14).
         * and contains 4 events.
         * First is bank with 1 char, second has bank with 2 chars,
         * third has bank with 3 chars, and the 4th has 4 chars.
         */
        uint32_t  wordData[35] {

                // Deliberately add words, allowed by evio-6/hipo format rules, but not used by evio

                // 17 + 4 + 2 + 4*3 = 35 words (0x21)

                0x00000023, // entire record word len inclusive, 35 words
                0x00000001, // rec #1
                0x00000011, // header word len, inclusive (should always be 14, but set to 17)
                0x00000004, // event count
                0x00000010, // index array len in bytes (4*4 = 16)
                0x00000206, // bit info word, evio version 6, is last record
                0x00000008, // user header byte len, 8
                0xc0da0100, // magic #
                0x00000048, // uncompressed data byte len (16 index + 8 + 4*12 events = 72 or 0x48)
                0x00000000, // compression type (0), compressed length (0)
                0x00000000, // user reg 1
                0x00000001, // user reg 1
                0x00000000, // user reg 2
                0x00000002, // user reg 2
                0x00000000, // extra header word, never normally here
                0x00000000, // extra header word, never normally here
                0x00000000, // extra header word, never normally here

                // array index (length in bytes of each event)
                0xc,
                0xc,
                0xc,
                0xc,

                // user header (should only be here if dictionary or first event defined, which they aren't)
                0x01020304,
                0x04030201,


                // event 1: num=1, tag=1, data type = 8 bit signed int, pad=3 (1 byte valid data)
                0x00000002,
                0x0001c601, // this pos = 100
                0x01020304,

                // event 2: num=2, tag=2, data type = 8 bit signed int, pad=2 (2 bytes valid data)
                0x00000002,
                0x00028602, // this pos = 112
                0x01020304,

                // event 3: num=3, tag=3, data type = 8 bit signed int, pad=1 (3 bytes valid data)
                0x00000002,
                0x00034603, // this pos = 124
                0x01020304,

                // event 4: num=4, tag=4, data type = 8 bit signed int, pad=0 (all 4 bytes are valid data)
                0x00000002,
                0x00040604, // this pos = 136
                0x01020304,
        };



        uint32_t normalDataIntCount = 30;

        uint32_t normalData[30] {

                // Normal evio 6 format

                // 14 + 4 + 4*3 = 30 words (0x1e)

                0x0000001e, // entire record word len inclusive, 30 words
                0x00000001, // rec #1
                0x0000000e, // header word len, inclusive (is always 14)
                0x00000004, // event count
                0x00000010, // index array len in bytes (4*4 = 16)
                0x00000206, // bit info word, evio version 6, is last record
                0x00000000, // user header byte len, 0
                0xc0da0100, // magic #
                0x00000040, // uncompressed data byte len (16 index + 4*12 events = 64 or 0x40)
                0x00000000, // compression type (0), compressed length (0)
                0x00000000, // user reg 1
                0x00000001, // user reg 1
                0x00000000, // user reg 2
                0x00000002, // user reg 2

                0xc,
                0xc,
                0xc,
                0xc,

                0x00000002,
                0x0001c601,
                0x01020304,

                0x00000002,
                0x00028602,
                0x01020304,

                0x00000002,
                0x00034603,
                0x01020304,

                0x00000002,
                0x00040604,
                0x01020304,
        };


        void ByteBufferTest() {

            uint8_t* array = new uint8_t[]{(uint8_t) 1, (uint8_t) 2, (uint8_t) 3, (uint8_t) 4};
            auto bb1 = std::make_shared<ByteBuffer>(array, 4);

            std::cout << "Wrapped array: " << std::endl;
            for (int i = 0; i < 4; i++) {
                std::cout << "array[" << i << "] = " << +array[i] << std::endl;
            }

            auto bbDup = bb1->duplicate();
            bbDup->clear();


            std::cout << "\nDuplicate array: " << std::endl;
            for (int i = 0; i < bbDup->remaining(); i++) {
                std::cout << "array[" << i << "] = " << +bbDup->getByte(i) << std::endl;
            }

            bb1->clear();
            auto bbSlice = bb1->slice();
            bbSlice->clear();

            std::cout << "\nSlice array: " << std::endl;
            for (int i = 0; i < bbSlice->remaining(); i++) {
                std::cout << "array[" << i << "] = " << +bbSlice->getByte(i) << std::endl;
            }

            //   bbDup.clear();
            bbDup->limit(3).position(1);
            bbSlice = bbDup->slice();
            bbSlice->clear();

            std::cout << "\nSlice of Duplicate array: " << std::endl;
            for (int i = 0; i < bbSlice->remaining(); i++) {
                std::cout << "array[" << i << "] = " << +bbSlice->getByte(i) << std::endl;
            }
        }
    };

}



int main(int argc, char **argv) {

    try {
        evio::CompactReaderBugTest tester;

        // Convert array of ints to array of bytes, then into a shared_ptr<ByteBuffer>
        size_t byteLen = 4*tester.wordDataIntCount;
        auto byteData = new uint8_t[byteLen];
        evio::Util::toByteArray(tester.wordData, tester.wordDataIntCount,
                                evio::ByteOrder::ENDIAN_BIG, byteData);
        auto buf = std::make_shared<evio::ByteBuffer>(byteData, byteLen);

        evio::EvioCompactReader reader(buf);
        int evCount = reader.getEventCount();
        for (int i=0; i < evCount; i++) {
            auto node = reader.getEvent(i+1);
            std::cout << "\nEvent " << (i+1) << ": tag=" << node->getTag() << ", num=" << node->getNum() <<
                               ", dataPos=" << node->getDataPosition() << ", type=" << node->getDataTypeObj().toString() <<
                               ", pad=" << node->getPad() << std::endl;
            std::cout << "    = " << node->toString() << std::endl;

            auto bb = node->getByteData(false);
            std::cout << "Buf: limit = " << bb->limit() << ", cap = " <<
                               bb->capacity() << ", pos = " << bb->position() << std::endl;

            for (int j=0; j < bb->remaining(); j++) {
                std::cout << "data[" << j << "] = " << +(bb->getByte(bb->position() + j)) << std::endl;
            }
        }

        std::cout << "\n\nByteBuffer test:\n\n" << std::endl;
        tester.ByteBufferTest();

    }
    catch (std::runtime_error & e) {
        std::cout << "PROBLEM: " << e.what() << std::endl;
    }

}


