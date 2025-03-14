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


    /**
     * Created by IntelliJ IDEA.
     * User: timmer
     * Date: 4/12/11
     * Time: 10:21 AM
     * To change this template use File | Settings | File Templates.
    */
    class CompositeTester {

        static  constexpr double PI = 3.141592653589793;

        /**
         * Equivalent to Java's Double.doubleToLongBits(double value).
         * @param value double to convert.
         * @return long representation of double.
         */
        static uint64_t doubleToLongBits(double value) {
            uint64_t bits;
            std::memcpy(&bits, &value, sizeof(double));  // Copy memory without reinterpret_cast
            return bits;
        }

        /**
         * Equivalent to Java's Float.floatToIntBits(float value).
         * @param value float to convert.
         * @return int representation of float.
         */
        static uint32_t floatToIntBits(float value) {
            uint32_t bits;
            std::memcpy(&bits, &value, sizeof(float));  // Copy memory safely
            return bits;
        }


    public:

        /** For testing only */
        static void test1() {


            uint32_t bank[24];

            //**********************/
            // bank of tagsegments */
            //**********************/
            bank[0] = 23;                       // bank length
            bank[1] = 6 << 16 | 0xF << 8 | 3;   // tag = 6, bank contains composite type, num = 3

            // Here follows the actual CompositeData element stored in the bank

            // N(I,D,F,2S,8a)
            // first part of composite type (for format) = tagseg (tag & type ignored, len used)
            bank[2]  = 5 << 20 | 0x3 << 16 | 4; // tag = 5, seg has char data, len = 4
            // ASCII chars values in latest evio string (array) format, N(I,D,F,2S,8a) with N=2
            bank[3]  = 0x4E << 24 | 0x28 << 16 | 0x49 << 8 | 0x2C;    // N ( I ,
            bank[4]  = 0x44 << 24 | 0x2C << 16 | 0x46 << 8 | 0x2C;    // D , F ,
            bank[5]  = 0x32 << 24 | 0x53 << 16 | 0x2C << 8 | 0x38 ;   // 2 S , 8
            bank[6]  = 0x61 << 24 | 0x29 << 16 | 0x00 << 8 | 0x04 ;   // a ) \0 \4

            // second part of composite type (for data) = bank (tag, num, type ignored, len used)
            bank[7]  = 16;
            bank[8]  = 6 << 16 | 0xF << 8 | 1;
            bank[9]  = 0x2; // N
            bank[10] = 0x00001111; // I
            // Double
            double d = PI * (-1.e-100);
            uint64_t  dl = doubleToLongBits(d);
            bank[11] = (uint32_t) (dl >> 32) & 0xffffffff;    // higher 32 bits
            bank[12] = (uint32_t)  dl;                        // lower 32 bits
            // Float
            float f = (float)(PI*(-1.e-24));
            uint32_t  fi = floatToIntBits(f);
            bank[13] = fi;

            bank[14] = 0x11223344; // 2S

            bank[15]  = 0x48 << 24 | 0x49 << 16 | 0x00 << 8 | 0x48;    // H  I \0  H
            bank[16]  = 0x4F << 24 | 0x00 << 16 | 0x04 << 8 | 0x04;    // 0 \ 0 \4 \4

            // duplicate data
            for (int i=0; i < 7; i++) {
                bank[17+i] = bank[10+i];
            }

            // all composite including headers
            uint32_t allData[22];
            for (int i=0; i < 22; i++) {
                allData[i] = bank[i+2];
            }

            // analyze format string
            std::string format = "N(I,D,F,2S,8a)";

            try {
                std::cout << "\n_________ TEST 1 _________\n" << std::endl;

                // change int array into byte array
                uint8_t *byteArray = new uint8_t[4*22];
                Util::toByteArray(allData, 22, ByteOrder::ENDIAN_BIG, byteArray);

                // wrap bytes in ByteBuffer for ease of printing later
                auto buf = std::make_shared<ByteBuffer>(byteArray, 4*22);
                buf->order(ByteOrder::ENDIAN_BIG);


                // print double swapped data
                std::cout << "ORIGINAL DATA:" << std::endl;
                for (int i=0; i < 22; i++) {
                    std::cout << std::showbase << std::hex << "     " << buf->getInt(i*4) << std::endl;
                }
                std::cout << std::endl;

                // swap
                std::cout << "CALL CompositeData::swapAll(), buf pos = " << buf->position() << std::endl;
                CompositeData::swapAll(buf, 0, 22);

                // print swapped data
                std::cout << "SWAPPED DATA:" << std::endl;
                for (int i=0; i < 22; i++) {
                    std::cout << "     " << buf->getInt(i*4) << std::endl;
                }
                std::cout << std::endl;

                // swap again
                std::cout << "Call CompositeData.swapAll()" << std::endl;
                CompositeData::swapAll(buf, 0, 22);

                // print double swapped data
                std::cout << "DOUBLE SWAPPED DATA:" << std::endl;
                for (int i=0; i < 22; i++) {
                    std::cout << "     " << buf->getInt(i*4) << std::endl;
                }
                std::cout << std::endl;

                // Check for differences
                std::cout << "CHECK FOR DIFFERENCES:" << std::endl;
                bool goodSwap = true;
                for (int i=0; i < 22; i++) {
                    if (buf->getInt(4*i) != bank[i+2]) {
                        std::cout << "orig = " << allData[i] << ", double swapped = " << buf->getInt(4*i) << std::endl;
                        goodSwap = false;
                    }
                }
                std::cout << "good swap = " << goodSwap << std::endl;

                // Create composite object
                auto cData = CompositeData::getInstance(byteArray,ByteOrder::ENDIAN_BIG);
                std::cout << "cData object = " + cData->toString("",false) + "\n\n" << std::endl;

                // print out general data
                std::cout << "format = " + format << std::endl;
                printCompositeDataObject(cData);

                // use alternative method to print out
                cData->index(0);
                std::cout << std::showbase << std::hex << "\nNValue = " << cData->getNValue() << std::endl;
                std::cout << "  Int  = " << cData->getInt() << std::endl;
                std::cout << "Double = " << cData->getDouble() << std::endl;
                std::cout << "Float  = " << cData->getFloat() << std::endl;
                std::cout << "Short  = " << cData->getShort() << std::endl;
                std::cout << "Short  = " << cData->getShort() << std::endl;
                std::vector<std::string> strs = cData->getStrings();
                for (std::string s : strs) std::cout << "String = " + s << std::endl;

                // use toString() method to print out
                std::cout << "\ntoString =\n" + cData->toString("     ", true) << std::dec << std::endl;

            }
            catch (EvioException & e) {
                std::cout << "PROBLEM: " << e.what() << std::endl;
            }
            std::cout << std::dec;
        }


        /**
         * Simple example of providing a format string and some data
         * in order to create a CompositeData object.
         */
        static void test2() {

            // Create a CompositeData object ...
            std::cout << "\n_________ TEST 2 _________\n" << std::endl;


//                    // Format to write an int and a string
//                    // To get the right format code for the string, use a helper method.
//                    // String can be at most 10 characters long, since when converted to
//                    // evio format, this will take 12 chars (max from composite data lib/rule is 15).
//                    std::string myString = "stringWhic";
//                    std::vector<std::string> strings;
//                    strings.push_back(myString);
//                    std::string stringFormat = CompositeData::stringsToFormat(strings);
//
//                    // Put the 2 formats together
//                    std::string format = "I," + stringFormat;
//
//                    std::cout << "format = " << format << std::endl;
//
//                    // Now create some data
//                    CompositeData::Data myData;
//                    myData.addInt(2);
//                    // Underneath, the string is converted to evio format for string array
//                    myData.addString(myString);



            // Format to write an int and a string
            // To get the right format code for the string, use a helper method.
            // All Strings together (including 1 between each element of array)
            // can be at most 10 characters long, since when converted to
            // evio format, this will take 12 chars (max from composite data lib/rule is 15).
            std::string myString1 = "st1__";
            std::string myString2 = "st2_";
            std::vector<std::string> both;
            both.push_back(myString1);
            both.push_back(myString2);
            std::string stringFormat = CompositeData::stringsToFormat(both);

            // Put the 2 formats together
            std::string format = "I," + stringFormat;

            std::cout << "Array of two strings:\n" << std::endl;
            std::cout << "format = " << format << std::endl;

            // Now create some data
            CompositeData::Data myData;
            myData.addInt(2);
            // Underneath, the string is converted to evio format for string array
            myData.addString(both);

            // Create CompositeData object
            std::shared_ptr<CompositeData> cData = nullptr;
            try {
                cData = CompositeData::getInstance(format, myData, 1, 0 , 0, ByteOrder::ENDIAN_BIG);
            }
            catch (EvioException & e) {
                std::cout << "PROBLEM: " << e.what() << std::endl;
                exit(-1);
            }


            // Print it out
            printCompositeDataObject(cData);


            // Format to write an int and a string
            // To get the right format code for the string, use a helper method.
            // An array of strings, when treated as a single item
            // in the format, can be at most 10 characters long.
            // To get around this restriction, each string must be
            // treated as its own entry in the format when.
            std::string myStr1 = "stringOf10";
            std::string myStr2 = "another_10";
            std::vector<std::string> s1;
            std::vector<std::string> s2;
            s1.push_back(myStr1);
            s2.push_back(myStr2);
            std::string stringFormat1 = CompositeData::stringsToFormat(s1);
            std::string stringFormat2 = CompositeData::stringsToFormat(s2);

            // Put the 2 formats together
            std::string format2 = "I," + stringFormat1 + "," + stringFormat2;

            std::cout << "\n\nTwo strings separately:\n" << std::endl;
            std::cout << "format = " << format2 << std::endl;

            // Now create some data
            CompositeData::Data myData2;
            myData2.clear();
            myData2.addInt(2);
            // Underneath, the string is converted to evio format for string array
            myData2.addString(myStr1);
            myData2.addString(myStr2);

            // Create CompositeData object
            std::shared_ptr<CompositeData> cData2 = nullptr;
            try {
                cData2 = CompositeData::getInstance(format2, myData2, 1, 0 , 0, ByteOrder::ENDIAN_BIG);
            }
            catch (EvioException e) {
                std::cout << "PROBLEM: " << e.what() << std::endl;
                exit(-1);
            }

            // Print it out
            printCompositeDataObject(cData2);
        }


        /**
         * More complicated example of providing a format string and some data
         * in order to create a CompositeData object.
         */
        static void test3() {

            std::cout << "\n_________ TEST 3 _________\n" << std::endl;
            // Create a CompositeData object ...

            // Format to write a m shorts, 1 float, 1 double a total of N times
            std::string format = "N(mS,F,D)";

            std::cout << "format = " << format << std::endl;

            // Now create some data (in the proper order!)
            // This has a padding of 2 bytes.
            CompositeData::Data myData;
            myData.addN(2);
            myData.addm(1);

            // use array as an example
            std::vector<int16_t> v;
            v.push_back(1);
            myData.addShort(v);

            myData.addFloat(1.0F);
            myData.addDouble(PI);
            myData.addm(1);
            myData.addShort(4);
            myData.addFloat(2.0F);
            myData.addDouble(2.*PI);

            // Create CompositeData object
            std::shared_ptr<CompositeData> cData = nullptr;
            try {
                cData = CompositeData::getInstance(format, myData, 1, 1 , 0, ByteOrder::nativeOrder());
            }
            catch (EvioException e) {
                std::cout << "PROBLEM: " << e.what() << std::endl;
                exit(-1);
            }


            // Now create more data
            // This has a padding of 3 bytes.
            CompositeData::Data myData2;
            myData2.addN(1);
            myData2.addm(2);
            std::vector<int16_t> v2;
            v2.push_back(1);
            v2.push_back(2);
            myData2.addShort(v2);
            myData2.addFloat(1.0F);
            myData2.addDouble(PI);

            // Create CompositeData object
            std::shared_ptr<CompositeData> cData2 = nullptr;
            try {
                cData2 = CompositeData::getInstance(format, myData2, 2, 2 , 0, ByteOrder::nativeOrder());
            }
            catch (EvioException e) {
                std::cout << "PROBLEM: " << e.what() << std::endl;
                exit(-1);
            }


            // Now create more data

            std::string format3 = "N(NS,F,D)";
            std::cout << "format3 = " << format3 << std::endl;

            // This has a padding of 3 bytes.
            CompositeData::Data myData3;
            myData3.addN(1);
            myData3.addN(2);
            std::vector<int16_t> v3;
            v3.push_back(1);
            v3.push_back(2);
            myData3.addShort(v3);
            myData3.addFloat(1.0F);
            myData3.addDouble(PI);

            // Create CompositeData object
            std::shared_ptr<CompositeData> cData3 = nullptr;
            try {
                cData3 = CompositeData::getInstance(format3, myData3, 3, 3 , 0, ByteOrder::nativeOrder());
            }
            catch (EvioException e) {
                std::cout << "PROBLEM: " << e.what() << std::endl;
                exit(-1);
            }


            // Print it out
            std::cout << "1st composite data item:" << std::endl;
            printCompositeDataObject(cData);
            std::cout << "2nd composite data item:" << std::endl;
            printCompositeDataObject(cData2);
            std::cout << "3rd composite data item:" << std::endl;
            printCompositeDataObject(cData3);

            try {
                std::shared_ptr<EvioEvent> ev = EvioEvent::getInstance(0, DataType::COMPOSITE, 0);
                std::vector<std::shared_ptr<CompositeData>> & compData = ev->getCompositeData();
                ev->setByteOrder(ByteOrder::ENDIAN_BIG);
                compData.push_back(cData);
                compData.push_back(cData2);
                ev->updateCompositeData();

                std::cout << "\nDOUBLE SWAP:" << std::endl;
                std::vector<uint8_t> & src = ev->getRawBytes();
                if (src.empty()) {
                    std::cout << "raw bytes is empty !!! " << std::endl;
                }
                else {
                    int srcLen = src.size();
                    uint8_t *srcOrig = new uint8_t[srcLen];
                    std::memcpy(srcOrig, src.data(), srcLen);
                    uint8_t *dest = new uint8_t[srcLen];

                    auto origOrder = ev->getByteOrder();
                    auto swappedOrder = origOrder == ByteOrder::ENDIAN_BIG ? ByteOrder::ENDIAN_LITTLE : ByteOrder::ENDIAN_BIG;

                    bool useBuffers = true;

                    // Both methods below are tested and work
                    if (useBuffers) {
                        // This shared pointer grabs ownership of srcOrig memory
                        std::shared_ptr<ByteBuffer> srcOrigBuffer = std::make_shared<ByteBuffer>(srcOrig, srcLen);
                        srcOrigBuffer->order(origOrder);
                        std::shared_ptr<ByteBuffer> destBuffer = std::make_shared<ByteBuffer>(dest, srcLen);
                        destBuffer->order(swappedOrder);

                        std::cout << "swap #1 buffer" << std::endl;
                        CompositeData::swapAll(srcOrigBuffer, destBuffer,
                                              0, 0, srcLen/4);

                        std::cout << "swap #2 buffer" << std::endl;
                        CompositeData::swapAll(destBuffer, srcOrigBuffer,
                                              0, 0, srcLen/4);

                        for (int i = 0; i < srcLen; i++) {
                            if (src.at(i) != srcOrigBuffer->getByte(i)) {
                                std::cout << "Double swapped item is different at pos " << i << ", src = " << +src[i] <<
                                          ", dble = " << +(srcOrigBuffer->getByte(i)) << std::endl;
                            }
                        }

                    }
                    else {
                        std::cout << "swap #1" << std::endl;
                        CompositeData::swapAll(src.data(), dest, srcLen / 4, ev->getByteOrder().isLocalEndian());
                        std::cout << "swap #2" << std::endl;
                        CompositeData::swapAll(dest, src.data(), srcLen / 4,
                                               ev->getByteOrder().getOppositeEndian().isLocalEndian());
                        std::cout << "past swap #2" << std::endl;

                        for (int i = 0; i < srcLen; i++) {
                            if (src[i] != srcOrig[i]) {
                                std::cout << "Double swapped item is different at pos " << i << ", src = " << +srcOrig[i] <<
                                          ", dble = " << +src[i] << std::endl;
                            }
                        }

                    }


                    if (!useBuffers) {
                        delete[](srcOrig);
                        delete[](dest);
                    }

                    std::cout << "DOUBLE SWAP DONE" << std::endl;

                }

                // Write it to this file
                std::string fileName  = "./composite.dat";

                std::cout << "\nWrite above Composite data to file\n" << std::endl;
                EventWriter writer(fileName, ByteOrder::ENDIAN_BIG);
                writer.writeEvent(ev);
                writer.close();

                std::cout << "Read file and print\n" << std::endl;
                EvioReader reader(fileName);
                auto evR = reader.parseNextEvent();
                auto h = evR->getHeader();
                std::cout << "event: tag = " << h->getTag() <<
                                   ", type = " << h->getDataTypeName() << ", len = " << h->getLength() << std::endl;
                if (evR != nullptr) {
                    auto cDataR = evR->getCompositeData();
                    for (auto & cd : cDataR) {
                        std::cout << "\nCD:" << std::endl;
                        printCompositeDataObject(cd);
                    }
                }

            }
            catch (std::runtime_error & e) {
                std::cout << "PROBLEM: " << e.what() << std::endl;
            }
        }




        /**
         * More complicated example of providing a format string and some data
         * in order to create a CompositeData object.
         */
        static void test4() {

            std::cout << "\n_________ TEST 4 _________\n" << std::endl;


            // Format to write 1 int and 1 float a total of N times
            std::string format1 = "N(I,F)";
            std::cout << "format = " + format1 << std::endl;

            // Now create some data
            CompositeData::Data myData1;
            myData1.addN(1);
            myData1.addInt(1); // use array for convenience
            myData1.addFloat(1.0F);
            // Create CompositeData object
            std::shared_ptr<CompositeData> cData1 = nullptr;
            try {
                cData1 = CompositeData::getInstance(format1, myData1, 1, 1, 1, ByteOrder::ENDIAN_BIG);
            }
            catch (EvioException e) {
                std::cout << "PROBLEM: " << e.what() << std::endl;
                exit(-1);
            }

            // Now create some data
            CompositeData::Data myData2;
            myData2.addN(1);
            myData2.addInt(2); // use array for convenience
            myData2.addFloat(2.0F);
            std::shared_ptr<CompositeData> cData2 = nullptr;
            try {
                cData2 = CompositeData::getInstance(format1, myData2, 2, 2, 2, ByteOrder::ENDIAN_BIG);
            }
            catch (EvioException e) {
                std::cout << "PROBLEM: " << e.what() << std::endl;
                exit(-1);
            }

            // Now create some data
            CompositeData::Data myData3;
            myData3.addN(1);
            myData3.addInt(3); // use array for convenience
            myData3.addFloat(3.0F);
            std::shared_ptr<CompositeData> cData3 = nullptr;
            try {
                cData3 = CompositeData::getInstance(format1, myData3, 3, 3, 3, ByteOrder::ENDIAN_BIG);
            }
            catch (EvioException e) {
                std::cout << "PROBLEM: " << e.what() << std::endl;
                exit(-1);
            }

            std::cout << "Create 3d composite data objects" << std::endl;

            // Print it out
            std::cout << "Print 3 composite data objects" << std::endl;
            printCompositeDataObject(cData1);
            printCompositeDataObject(cData2);
            printCompositeDataObject(cData3);

            std::cout << "  composite data object 1:\n" << std::endl;
            Util::printBytes(cData1->getRawBytes().data(), cData1->getRawBytes().size(), "RawBytes 1");
            std::cout << "  composite data object 2:\n" << std::endl;
            Util::printBytes(cData2->getRawBytes().data(), cData2->getRawBytes().size(), "RawBytes 2");
            std::cout << "  composite data object 3:\n" << std::endl;
            Util::printBytes(cData3->getRawBytes().data(), cData3->getRawBytes().size(), "RawBytes 3");

            try {
                std::shared_ptr<EvioEvent> ev = EvioEvent::getInstance(0, DataType::COMPOSITE, 0);
                ev->setByteOrder(ByteOrder::ENDIAN_BIG);
                std::vector<std::shared_ptr<CompositeData>> & compData = ev->getCompositeData();
                compData.push_back(cData1);
                compData.push_back(cData2);
                compData.push_back(cData3);
                ev->updateCompositeData();

                std::cout << "Print event raw bytes of composite array:\n" << std::endl;
                Util::printBytes(ev->getRawBytes().data(), ev->getRawBytes().size(), "Array rawBytes");

                // Write it to this file
                std::string fileName  = "./composite.dat";

                std::cout << "WRITE FILE:" << std::endl;
                EventWriter writer(fileName, ByteOrder::ENDIAN_LITTLE);
                writer.writeEvent(ev);
                writer.close();

                Util::printBytes(fileName, 0, 1000, "FILE read back in");


                // Read it from file
                std::cout << "READ FILE & PRINT CONTENTS:" << std::endl;
                EvioReader reader(fileName);
                auto evR = reader.parseNextEvent();
                auto h = evR->getHeader();
                std::cout << "event: tag = " << h->getTag() <<
                                   ", type = " << h->getDataTypeName() << ", len = " << h->getLength() << std::endl;
                if (evR != nullptr) {
                    auto cDataR = evR->getCompositeData();
                    for (auto & cd : cDataR) {
                        printCompositeDataObject(cd);
                    }
                }

            }
            catch (std::runtime_error & e) {
                std::cout << "PROBLEM: " << e.what() << std::endl;
            }
        }



        /**
         * Print the data from a CompositeData object in a user-friendly form.
         * @param cData CompositeData object
         */
       static void printCompositeDataObject(std::shared_ptr<CompositeData> & cData) {

            // Get lists of data items & their types from composite data object
            std::vector<CompositeData::DataItem> & items = cData->getItems();
            std::vector<DataType> & types = cData->getTypes();

            // Use these list to print out data of unknown format
            size_t len = items.size();

            std::cout << std::showbase << std::hex;

            for (int i=0; i < len; i++) {
                CompositeData::DataItem &dataItem = items[i];
                evio::DataType & type = types[i];
                std::printf("type = %9s, val = ", type.toString().c_str());

                if (type == evio::DataType::UINT32) {
                    uint32_t j = dataItem.item.ui32;
                    std::cout << j << std::endl;
                }
                else if (type == evio::DataType::NVALUE ||
                         type == evio::DataType::INT32) {
                    int32_t j = dataItem.item.i32;
                    std::cout << j << std::endl;
                }
                else if (type == evio::DataType::ULONG64) {
                    uint64_t j = dataItem.item.ul64;
                    std::cout << j << std::endl;
                }
                else if (type == evio::DataType::LONG64) {
                    int64_t j = dataItem.item.l64;
                    std::cout << j << std::endl;
                }
                else if (type == evio::DataType::nVALUE ||
                         type == evio::DataType::SHORT16) {
                    int16_t j = dataItem.item.s16;
                    std::cout << j << std::endl;
                }
                else if (type == evio::DataType::USHORT16) {
                    int16_t j = dataItem.item.us16;
                    std::cout << j << std::endl;
                }
                else if (type == evio::DataType::mVALUE ||
                         type == evio::DataType::CHAR8) {
                    int8_t j = dataItem.item.b8;
                    std::cout << +j << std::endl;
                }
                else if (type == evio::DataType::UCHAR8) {
                    uint8_t j = dataItem.item.ub8;
                    std::cout << +j << std::endl;
                }
                else if (type == evio::DataType::FLOAT32) {
                    float j = dataItem.item.flt;
                    std::cout << j << std::endl;
                }
                else if (type == evio::DataType::DOUBLE64) {
                    double j = dataItem.item.dbl;
                    std::cout << j << std::endl;
                }
                else if (type == evio::DataType::CHARSTAR8) {
                    try {
                        auto &strs = dataItem.strVec;
                        for (size_t j = 0; j < strs.size(); j++) {
                            std::cout << strs[j];
                            if (j < strs.size() - 1) {
                                std::cout << ", ";
                            }
                        }
                        std::cout << std::endl;
                    }
                    catch (std::runtime_error & e) {
                        std::cout << "Error in printCompositeDataObject: " << e.what() << std::endl;
                    }
                }
            }
            std::cout << std::dec;
        }

    };

}

int main(int argc, char **argv) {

    try {
        evio::CompositeTester tester;
        tester.test1();
        tester.test2();
        tester.test3();
        tester.test4();
    }
    catch (std::runtime_error & e) {
        std::cout << "PROBLEM: " << e.what() << std::endl;
    }
    return 0;

}
