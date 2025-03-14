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
#include <limits>

#include "TestBase.h"
#include "eviocc.h"


namespace evio {


    /**
     * Test swapping evio data.
     *
     * @date 03/12/2025
     * @author timmer
     */
    class SwapTest {

    private:

        // data
        int8_t   byteData[3];
        uint8_t  ubyteData[3];
        int16_t  shortData[3];
        uint16_t ushortData[3];
        int32_t  intData[3];
        uint32_t uintData[3];
        int64_t  longData[3];
        uint64_t ulongData[3];
        float    floatData[3];
        double   doubleData[3];

        std::vector<std::string> stringData;
        std::vector<std::shared_ptr<CompositeData>> cData;

        ByteOrder order = ByteOrder::ENDIAN_BIG;

    public:

        SwapTest() {
            byteData[0] = std::numeric_limits<int8_t>::max();
            byteData[1] = 0;
            byteData[2] = std::numeric_limits<int8_t>::min();

            ubyteData[0] = std::numeric_limits<uint8_t>::max();
            ubyteData[1] = 0;
            ubyteData[2] = std::numeric_limits<uint8_t>::min();

            shortData[0] = std::numeric_limits<int16_t>::max();
            shortData[1] = 0;
            shortData[2] = std::numeric_limits<int16_t>::min();

            ushortData[0] = std::numeric_limits<uint16_t>::max();
            ushortData[1] = 0;
            ushortData[2] = std::numeric_limits<uint16_t>::min();

            intData[0] = std::numeric_limits<int32_t>::max();
            intData[1] = 0;
            intData[2] = std::numeric_limits<int32_t>::min();

            uintData[0] = std::numeric_limits<uint32_t>::max();
            uintData[1] = 0;
            uintData[2] = std::numeric_limits<uint32_t>::min();

            longData[0] = std::numeric_limits<int64_t>::max();
            longData[1] = 0;
            longData[2] = std::numeric_limits<int64_t>::min();

            ulongData[0] = std::numeric_limits<uint64_t>::max();
            ulongData[1] = 0;
            ulongData[2] = std::numeric_limits<uint64_t>::min();

            floatData[0] = std::numeric_limits<float>::max();
            floatData[1] = 0.;
            floatData[2] = std::numeric_limits<float>::min();

            doubleData[0] = std::numeric_limits<double>::max();
            doubleData[1] = 0.;
            doubleData[2] = std::numeric_limits<double>::min();

            stringData.push_back("123");
            stringData.push_back("456");
            stringData.push_back("789");
        }


        void createCompositeData() {
            // Create a CompositeData object ...

            try {
                // Format to write a N shorts, 1 float, 1 double a total of N times
            std::string format1 = "N(NS,F,D)";

            // Now create some data
            CompositeData::Data myData1;
            myData1.addN(2);
            myData1.addN(3);

            std::vector<int16_t> shorts;
            shorts.push_back(1);
            shorts.push_back(2);
            shorts.push_back(3);
            myData1.addShort(shorts);

            myData1.addFloat(std::numeric_limits<float>::max());
            myData1.addDouble(std::numeric_limits<double>::max());
            myData1.addN(1);
            myData1.addShort((int16_t) 4);
            myData1.addFloat(std::numeric_limits<float>::min());
            myData1.addDouble(std::numeric_limits<double>::min());

            // ROW 2
            myData1.addN(1);
            myData1.addN(1);
            myData1.addShort((int16_t) 4);
            myData1.addFloat(4.0F);
            myData1.addDouble(4.);

            // Format to write an unsigned int, unsigned char, and N number of
            // M (int to be found) ascii characters & 1 64-bit int. We need to
            // wait before we can create this format string because we don't know
            // yet how many string characters (M) we have to determine the "Ma" term.
            // std::string format2 = "i,c,N(Ma,L)";
            // If an integer replaces "M", it cannot be greater than 15.
            // If M is written as "N", it can be any positive integer.

            // Now create some data
            CompositeData::Data myData2;
            myData2.addUInt(21);
            myData2.addUChar((uint8_t) 22);
            myData2.addN(1);

            // Define our ascii data
            std::vector<std::string> s;
            s.push_back("str1");
            s.push_back("str2");

            // Find out how what the composite format representation of this string is
            std::string asciiFormat = CompositeData::stringsToFormat(s);
            // Format to write an unsigned int, unsigned char, and N number of
            // M ascii characters & 1 64-bit int.
            // std::cout << "ascii format = " << asciiFormat << std::endl;
            std::string format2 = "i,c,N(" + asciiFormat + ",L)";
            myData2.addString(s);
            myData2.addLong(24L);

            // Now create some data
            CompositeData::Data myData3;

            myData3.addChar(byteData[0]);
            myData3.addChar(byteData[1]);
            myData3.addChar(byteData[2]);

            myData3.addUChar(byteData[0]);
            myData3.addUChar(byteData[1]);
            myData3.addUChar(byteData[2]);

            myData3.addShort(shortData[0]);
            myData3.addShort(shortData[1]);
            myData3.addShort(shortData[2]);

            myData3.addUShort(shortData[0]);
            myData3.addUShort(shortData[1]);
            myData3.addUShort(shortData[2]);

            myData3.addInt(intData[0]);
            myData3.addInt(intData[1]);
            myData3.addInt(intData[2]);

            myData3.addUInt(intData[0]);
            myData3.addUInt(intData[1]);
            myData3.addUInt(intData[2]);

            myData3.addLong(longData[0]);
            myData3.addLong(longData[1]);
            myData3.addLong(longData[2]);

            myData3.addULong(longData[0]);
            myData3.addULong(longData[1]);
            myData3.addULong(longData[2]);

            std::string format3 = "3C,3c,3S,3s,3I,3i,3L,3l";

            //            // Now create some data
            //            CompositeData::Data myData4;
            //            //myData4.addInt(88);
            //            //myData4.addInt(99);
            //            // Define our ascii data
            //            std::vector<std::string> ss;
            //            ss.push_back("Reallyreallylongstring");
            //            // Find out how big the evio representation of this string is
            //            int strLen = BaseStructure::stringsToRawSize(ss);
            //            //std::cout << "2nd string evio format len = " << strLen << std::endl;
            //            myData4.addN(strLen);
            //            myData4.addString(ss);
            //            // Format to write a long, and N number of ascii characters.
            //            std::string format4 = "Na";

            // Format to write a N shorts, 1 float, 1 double a total of N times
            std::string format5 = "N(NS,4I)";

            // Now create some data
            CompositeData::Data myData5;
            myData5.addN(2);
            myData5.addN(3);
            myData5.addShort(shorts); // use vector for convenience
            myData5.addInt(1);
            myData5.addInt(2);
            myData5.addInt(3);
            myData5.addInt(4);
            myData5.addN(1);
            myData5.addShort(4);
            myData5.addInt(3);
            myData5.addInt(4);
            myData5.addInt(5);
            myData5.addInt(6);

            // ROW 2
            myData5.addN(1);
            myData5.addN(1);
            myData5.addShort(4);
            myData5.addInt(5);
            myData5.addInt(6);
            myData5.addInt(7);
            myData5.addInt(8);

            // Format to test how values are written on a line
            std::string format6 = "D,2D,3D,3F,4F,5F,5S,6S,7S,7C,8C,9C";

            // Now create some data
            CompositeData::Data myData6;
            myData6.addDouble(std::numeric_limits<double>::min());

            myData6.addDouble(0.);
            myData6.addDouble(std::numeric_limits<double>::max());

            myData6.addDouble(3.);
            myData6.addDouble(3.);
            myData6.addDouble(3.);

            myData6.addFloat((float) 3.e-10);
            myData6.addFloat((float) 3.e10);
            myData6.addFloat((float) 3.e10);

            myData6.addFloat(std::numeric_limits<float>::min());
            myData6.addFloat((float) 0.);
            myData6.addFloat((float) 4.e11);
            myData6.addFloat(std::numeric_limits<float>::max());

            myData6.addFloat(5.F);
            myData6.addFloat(5.F);
            myData6.addFloat(5.F);
            myData6.addFloat(5.F);
            myData6.addFloat(5.F);

            short sh = 5;
            myData6.addShort(sh);
            myData6.addShort(sh);
            myData6.addShort(sh);
            myData6.addShort(sh);
            myData6.addShort(sh);
            sh = 6;
            myData6.addShort(sh);
            myData6.addShort(sh);
            myData6.addShort(sh);
            myData6.addShort(sh);
            myData6.addShort(sh);
            myData6.addShort(sh);
            sh = 7;
            myData6.addShort(sh);
            myData6.addShort(sh);
            myData6.addShort(sh);
            myData6.addShort(sh);
            myData6.addShort(sh);
            myData6.addShort(sh);
            myData6.addShort(sh);

            int8_t b = 8;
            myData6.addChar(b);
            myData6.addChar(b);
            myData6.addChar(b);
            myData6.addChar(b);
            myData6.addChar(b);
            myData6.addChar(b);
            myData6.addChar(b);
            b = 9;
            myData6.addChar(b);
            myData6.addChar(b);
            myData6.addChar(b);
            myData6.addChar(b);
            myData6.addChar(b);
            myData6.addChar(b);
            myData6.addChar(b);
            myData6.addChar(b);
            b = 10;
            myData6.addChar(b);
            myData6.addChar(b);
            myData6.addChar(b);
            myData6.addChar(b);
            myData6.addChar(b);
            myData6.addChar(b);
            myData6.addChar(b);
            myData6.addChar(b);
            myData6.addChar(b);


            // Create CompositeData array
                cData.push_back(CompositeData::getInstance(format1, myData1, 1, 1, 1, order));
                cData.push_back(CompositeData::getInstance(format2, myData2, 2, 2, 2, order));
                cData.push_back(CompositeData::getInstance(format3, myData3, 3, 3, 3, order));
                //cData.push_back(CompositeData::getInstance(format4, myData4, 4, 4, 4, order);
                cData.push_back(CompositeData::getInstance(format5, myData5, 5, 5, 5, order));
                cData.push_back(CompositeData::getInstance(format6, myData6, 6, 6, 6, order));
            }
            catch (EvioException &e) {
                std::cerr << "Error in createCompositeData: " << e.what() << std::endl;
                exit(-1);
            }
        }


        /** Build the same event as above but with a CompactEventBuilder instead of an EventBuilder. */
        std::shared_ptr<ByteBuffer> createCompactSingleEvent(int tag) {

            try {
                // Buffer to fill
            std::shared_ptr<ByteBuffer> buf = std::make_shared<ByteBuffer>(1024);
            buf->order(order);
            auto builder = std::make_shared<CompactEventBuilder>(buf);

            int num = tag;



                // add top/event level bank of banks
                builder->openBank(tag, DataType::BANK, num);

                    // add bank of banks
                    builder->openBank(tag + 1, DataType::BANK, num + 1);

                        // add bank of ints
                        builder->openBank(tag + 2, DataType::INT32, num + 2);
                        builder->addIntData(intData, 3);
                        builder->closeStructure();

                        // add bank of unsigned ints
                        builder->openBank(tag + 2, DataType::UINT32, num + 32);
                        builder->addUIntData(uintData, 3);
                        builder->closeStructure();

                        // add bank of bytes
                        builder->openBank(tag + 3, DataType::CHAR8, num + 3);
                        builder->addByteData(byteData, 3);
                        builder->closeStructure();

                        // add bank of unsigned bytes
                        builder->openBank(tag + 3, DataType::UCHAR8, num + 33);
                        builder->addUByteData(ubyteData, 3);
                        builder->closeStructure();

                        // add bank of shorts
                        builder->openBank(tag + 4, DataType::SHORT16, num + 4);
                        builder->addShortData(shortData, 3);
                        builder->closeStructure();

                        // add bank of unsigned shorts
                        builder->openBank(tag + 4, DataType::USHORT16, num + 34);
                        builder->addUShortData(ushortData, 3);
                        builder->closeStructure();

                        // add bank of longs
                        builder->openBank(tag + 5, DataType::LONG64, num + 5);
                        builder->addLongData(longData, 3);
                        builder->closeStructure();

                        // add bank of unsigned longs
                        builder->openBank(tag + 5, DataType::ULONG64, num + 35);
                        builder->addULongData(ulongData, 3);
                        builder->closeStructure();

                        // add bank of floats
                        builder->openBank(tag + 6, DataType::FLOAT32, num + 6);
                        builder->addFloatData(floatData, 3);
                        builder->closeStructure();

                        // add bank of doubles
                        builder->openBank(tag + 7, DataType::DOUBLE64, num + 7);
                        builder->addDoubleData(doubleData, 3);
                        builder->closeStructure();

                        // add bank of strings
                        builder->openBank(tag + 8, DataType::CHARSTAR8, num + 8);
                        builder->addStringData(stringData);
                        builder->closeStructure();

                        // bank of composite data array
                        createCompositeData();
                        builder->openBank(tag + 9, DataType::COMPOSITE, num + 9);
                        builder->addCompositeData(cData);
                        builder->closeStructure();

                    builder->closeStructure();


                    // add bank of segs
                    builder->openBank(tag + 10, DataType::SEGMENT, num + 10);

                        // add seg of ints
                        builder->openSegment(tag + 11, DataType::INT32);
                        builder->addIntData(intData, 3);
                        builder->closeStructure();

                        // add seg of shorts
                        builder->openSegment(tag + 12, DataType::SHORT16);
                        builder->addShortData(shortData, 3);
                        builder->closeStructure();

                        // add seg of segs
                        builder->openSegment(tag + 13, DataType::SEGMENT);

                            // add seg of bytes
                            builder->openSegment(tag + 14, DataType::CHAR8);
                            builder->addByteData(byteData, 3);
                            builder->closeStructure();

                            // add seg of doubles
                            builder->openSegment(tag + 15, DataType::DOUBLE64);
                            builder->addDoubleData(doubleData, 3);
                            builder->closeStructure();

                        builder->closeStructure();

                    builder->closeStructure();


                    // add bank of tagsegs
                    builder->openBank(tag + 16, DataType::TAGSEGMENT, num + 16);

                        // add tagseg of bytes
                        builder->openTagSegment(tag + 17, DataType::CHAR8);
                        builder->addByteData(byteData, 3);
                        builder->closeStructure();

                        // add tagseg of shorts
                        builder->openTagSegment(tag + 18, DataType::SHORT16);
                        builder->addShortData(shortData, 3);
                        builder->closeStructure();

                        // add tagseg of longs
                        builder->openTagSegment(tag + 19, DataType::LONG64);
                        builder->addLongData(longData, 3);
                        builder->closeStructure();

               builder->closeAll();
                return builder->getBuffer();

            }
            catch (EvioException & e) {
                std::cout << "Error in createCompactSingleEvent: " << e.what() << std::endl;
                throw EvioException(e);
            }

        }





        /**
         * Create a test Evio Event in ByteBuffer form using CompactEventBuilder.
         * @param tag tag of event.
         * @param num num of event.
         * @param byteOrder byte order of resulting ByteBuffer.
         * @param bSize if builder is null, size of internal buf to create
         * @param builder object to build EvioEvent with, if null, create in method.
         * @return ByteBuffer containing EvioEvent.
         * @throws EvioException error in CompactEventBuilder object.
         */
        std::shared_ptr<ByteBuffer> createCompactEventBuffer(uint16_t tag, uint8_t num,
                                                             ByteOrder const & byteOrder,
                                                             size_t bSize,
                                                             std::shared_ptr<CompactEventBuilder> builder) {
            try {

                if (builder == nullptr) {
                    std::shared_ptr<ByteBuffer> buf = std::make_shared<ByteBuffer>(bSize);
                    buf->order(byteOrder);
                    builder = std::make_shared<CompactEventBuilder>(buf);
                }

                int dataElementCount = 3;

                // add top/event level bank of banks
                builder->openBank(tag, DataType::BANK, num);

                // add bank of banks
                builder->openBank(tag + 200, DataType::BANK, num + 200);

                // add bank of ints
                builder->openBank(tag + 2, DataType::INT32, num + 2);
                builder->addIntData(intData, dataElementCount);
                builder->closeStructure();

                // add bank of bytes
                builder->openBank(tag + 3, DataType::CHAR8, num + 3);
                builder->addByteData(byteData, dataElementCount);
                builder->closeStructure();

                // add bank of shorts
                builder->openBank(tag + 4, DataType::SHORT16, num + 4);
                builder->addShortData(shortData, dataElementCount);
                builder->closeStructure();

                // add bank of longs
                builder->openBank(tag + 40, DataType::LONG64, num + 40);
                builder->addLongData(longData, dataElementCount);
                builder->closeStructure();

                // add bank of floats
                builder->openBank(tag + 5, DataType::FLOAT32, num + 5);
                builder->addFloatData(floatData, dataElementCount);
                builder->closeStructure();

                // add bank of doubles
                builder->openBank(tag + 6, DataType::DOUBLE64, num + 6);
                builder->addDoubleData(doubleData, dataElementCount);
                builder->closeStructure();

                // add bank of strings
                builder->openBank(tag + 7, DataType::CHARSTAR8, num + 7);
                builder->addStringData(stringData);
                builder->closeStructure();

                // add bank of composite data
                builder->openBank(tag + 100, DataType::COMPOSITE, num + 100);
                builder->addCompositeData(cData);
                builder->closeStructure();

                builder->closeStructure();



                // add bank of segs
                builder->openBank(tag + 201, DataType::SEGMENT, num + 201);

                // add seg of ints
                builder->openSegment(tag + 8, DataType::INT32);
                builder->addIntData(intData, dataElementCount);
                builder->closeStructure();

                // add seg of bytes
                builder->openSegment(tag + 9, DataType::CHAR8);
                builder->addByteData(byteData, dataElementCount);
                builder->closeStructure();

                // add seg of shorts
                builder->openSegment(tag + 10, DataType::SHORT16);
                builder->addShortData(shortData, dataElementCount);
                builder->closeStructure();

                // add seg of longs
                builder->openSegment(tag + 40, DataType::LONG64);
                builder->addLongData(longData, dataElementCount);
                builder->closeStructure();

                // add seg of floats
                builder->openSegment(tag + 11, DataType::FLOAT32);
                builder->addFloatData(floatData, dataElementCount);
                builder->closeStructure();

                // add seg of doubles
                builder->openSegment(tag + 12, DataType::DOUBLE64);
                builder->addDoubleData(doubleData, dataElementCount);
                builder->closeStructure();

                // add seg of strings
                builder->openSegment(tag + 13, DataType::CHARSTAR8);
                builder->addStringData(stringData);
                builder->closeStructure();

                builder->closeStructure();


                // add bank of tagsegs
                builder->openBank(tag + 202, DataType::TAGSEGMENT, num + 202);

                // add tagseg of ints
                builder->openTagSegment(tag + 16, DataType::INT32);
                builder->addIntData(intData, dataElementCount);
                builder->closeStructure();

                // add tagseg of bytes
                builder->openTagSegment(tag + 17, DataType::CHAR8);
                builder->addByteData(byteData, dataElementCount);
                builder->closeStructure();

                // add tagseg of shorts
                builder->openTagSegment(tag + 18, DataType::SHORT16);
                builder->addShortData(shortData, dataElementCount);
                builder->closeStructure();

                // add tagseg of longs
                builder->openTagSegment(tag + 40, DataType::LONG64);
                builder->addLongData(longData, dataElementCount);
                builder->closeStructure();

                // add tagseg of floats
                builder->openTagSegment(tag + 19, DataType::FLOAT32);
                builder->addFloatData(floatData, dataElementCount);
                builder->closeStructure();

                // add tagseg of doubles
                builder->openTagSegment(tag + 20, DataType::DOUBLE64);
                builder->addDoubleData(doubleData, dataElementCount);
                builder->closeStructure();

                // add tagseg of strings
                builder->openTagSegment(tag + 21, DataType::CHARSTAR8);
                builder->addStringData(stringData);
                builder->closeStructure();

                builder->closeAll();

                // Make this call to set proper pos & lim
                return builder->getBuffer();
            }
            catch (EvioException &e) {
                std::cout << "ERROR: " << e.what() << std::endl;
                throw EvioException(e);
            }

        }


    };

}



    /** Create event and swap it twice. */
    int main(int argc, char **argv) {

        try {
            evio::SwapTest tester;


      //      auto buffie = tester.createCompactEventBuffer(1, 1, evio::ByteOrder::ENDIAN_BIG, 1024, nullptr);
            auto buffie = tester.createCompactSingleEvent(1);
            int byteSize = buffie->limit();
            std::cout << "SwapTest: buffie lim = " << byteSize << ", pos = " << buffie->position() << ", cap = " << buffie->capacity() << std::endl;

            auto swappedBuffie = std::make_shared<evio::ByteBuffer>(byteSize);
            auto origBuffie = std::make_shared<evio::ByteBuffer>(byteSize);
            std::vector<std::shared_ptr<evio::EvioNode>> nodeList;

            // Take buffer and swap it

//            static void swapEvent(std::shared_ptr<ByteBuffer> & srcBuffer,
//                                  std::shared_ptr<ByteBuffer> & destBuffer,
//                                  std::vector<std::shared_ptr<EvioNode>> & nodeList,
//                                  bool storeNodes=false, bool swapData=true,
//                                  size_t srcPos=0, size_t destPos=0) {


            std::cout << "SwapTest: before swap, buffie len = " << buffie->remaining() << std::endl;

            evio::EvioSwap::swapEvent(buffie, swappedBuffie, nodeList, true);
            std::cout << "SwapTest: after swap, buffie len = " << buffie->remaining() << ",  swapped data len = " << swappedBuffie->remaining() << std::endl;
            // Take and swap the swapped buffer
            nodeList.clear();
            evio::EvioSwap::swapEvent(swappedBuffie, origBuffie, nodeList, true);
            std::cout << "SwapTest: after another swap, swapped data len = " << swappedBuffie->remaining() << ",  d-swapped data len = " << origBuffie->remaining() << std::endl;

            evio::Util::printBytes(buffie, 0, byteSize, "buffie");
            evio::Util::printBytes(swappedBuffie, 0, byteSize, "swappedBuffie");
            evio::Util::printBytes(origBuffie, 0, byteSize, "origBuffie");

            bool goodSwap = true;
            for (int i = 0; i < byteSize; i++) {
                uint8_t one = buffie->getByte(i);
                uint8_t two = origBuffie->getByte(i);

                if (one != two) {
                    std::cout << "SwapTest: data differs at index = " << i << ",  orig = " << +one << ", double swapped = " << +two << std::endl;
                    goodSwap = false;
                }
            }

            if (goodSwap) {
                std::cout << "SwapTest: double swap successful!!" << std::endl;
            }
        }
        catch (std::runtime_error &e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
        return 0;
    }






