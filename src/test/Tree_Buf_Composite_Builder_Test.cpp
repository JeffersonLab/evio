//
// Created by timmer on 2/19/20.
//

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


#include <string>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <thread>
#include <memory>
#include <regex>
#include <limits>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <sys/mman.h>

#include "eviocc.h"


using namespace std;


namespace evio {


    // Test the BaseStructure's tree methods
    static void TreeTest() {

        // check handling of nullptr
        EvioSwap::swapBank(nullptr, false, nullptr);

        // check tree structure stuff
        auto topBank = EvioBank::getInstance(0, DataType::BANK, 0);
        auto midBank = EvioBank::getInstance(1, DataType::BANK, 1);
        auto midBank2 = EvioBank::getInstance(2, DataType::BANK, 2);
        auto childBank = EvioBank::getInstance(4, DataType::FLOAT32, 4);

        // Child's float data
        auto &fData = childBank->getFloatData();
        fData.push_back(0.);
        fData.push_back(1.);
        fData.push_back(2.);
        std::cout << "EvioBank: local intData size = " << fData.size() << std::endl;
        childBank->updateFloatData();

        // Create tree
        topBank->add(midBank);
        topBank->add(midBank2);
        // add it again should make no difference
        topBank->add(midBank2);
        midBank->add(childBank);


        std::cout << std::endl << "TopBank = " << topBank->toString() << std::endl;
        std::cout << std::boolalpha;
        std::cout << "Is child descendant of Top bank? " << topBank->isNodeDescendant(childBank) << std::endl;
        std::cout << "Is Top bank ancestor of child? " << childBank->isNodeAncestor(topBank) << std::endl;
        std::cout << "Depth at Top bank = " << topBank->getDepth() << std::endl << std::endl;
        std::cout << "Depth at Mid bank = " << midBank->getDepth() << std::endl << std::endl;
        std::cout << "Depth at Child bank = " << childBank->getDepth() << std::endl << std::endl;
        std::cout << "Level at top bank = " << topBank->getLevel() << std::endl;
        std::cout << "Level at child = " << childBank->getLevel() << std::endl;

        std::cout << "Remove child from midBank:" << std::endl;
        midBank->remove(childBank);
        std::cout << "midBank = " << midBank->toString() << std::endl;
        std::cout << "Is child descendant of top bank? " << topBank->isNodeDescendant(childBank) << std::endl;
        std::cout << "Is top bank ancestor of child? " << childBank->isNodeAncestor(topBank) << std::endl;

        // add child again
        midBank->add(childBank);
        std::cout << std::endl << "midBank = " << midBank->toString() << std::endl;
        midBank->removeAllChildren();
        std::cout << "Remove all children from bank:" << std::endl;
        std::cout << "midBank = " << midBank->toString() << std::endl;

        // add child again
        midBank->add(childBank);
        std::cout << std::endl << "midBank = " << midBank->toString() << std::endl;
        childBank->removeFromParent();
        std::cout << "Remove child from parent:" << std::endl;
        std::cout << "midBank = " << midBank->toString() << std::endl;

        // add child again
        midBank->add(childBank);
        std::cout << "Level at top bank = " << topBank->getLevel() << std::endl;
        std::cout << "Level at child = " << childBank->getLevel() << std::endl;
        std::cout << "Level at mid bank 1 = " << midBank->getLevel() << std::endl;

        std::cout << std::endl << "CALL sharedAncestor for both mid banks" << std::endl;
        auto strc = midBank2->getSharedAncestor(midBank);
        if (strc != nullptr) {
            std::cout << std::endl << "shared ancestor of midBank 1&2 = " << strc->toString() << std::endl << std::endl;
        }
        else {
            std::cout << std::endl << "shared ancestor of midBank 1&2 = NONE" << std::endl << std::endl;
        }

        auto path = childBank->getPath();
        std::cout << "Path of child bank:" << std::endl;
        for (auto str : path) {
            std::cout << "     -  " << str->toString() << std::endl;
        }

        uint32_t kidCount = topBank->getChildCount();
        std::cout << std::endl << "topBank has " << kidCount << " children" << std::endl;
        for (uint32_t i=0; i < kidCount; i++) {
            std::cout << "   child at index " << i << " = " << topBank->getChildAt(i)->toString() << std::endl;
            std::cout << "       child getIndex = " << topBank->getIndex(topBank->getChildAt(i)) << std::endl;
        }

        std::cout << std::endl << "insert another child into topBank at index = 2" << std::endl;
        auto midBank3 = EvioBank::getInstance(3, DataType::BANK, 3);
        topBank->insert(midBank3, 2);
        std::cout << std::endl << "topBank = " << topBank->toString() << std::endl;

        try {
            std::cout << std::endl << "insert another child into topBank at index = 4" << std::endl;
            auto midBank33 = EvioBank::getInstance(33, DataType::BANK, 33);
            topBank->insert(midBank33, 4);
            std::cout << std::endl << "topBank = " << topBank->toString() << std::endl;
        }
        catch (std::out_of_range & e) {
            std::cout << "ERROR: " << e.what() << std::endl;
        }

        std::cout << std::endl << "iterate thru topBank children" << std::endl;
        auto beginIter = topBank->childrenBegin();
        auto endIter = topBank->childrenEnd();
        for (; beginIter != endIter; beginIter++) {
            auto kid = *beginIter;
            std::cout << "  kid = " << kid->toString() << std::endl;
        }

            std::cout << std::endl << "Remove topBank's first child" << std::endl;
            topBank->remove(0);
            std::cout << "    topBank has " << kidCount << " children" << std::endl;
            std::cout << "    topBank = " << topBank->toString() << std::endl;
            // reinsert
            topBank->insert( midBank, 0);

            auto parent = topBank->getParent();
            if (parent == nullptr) {
                std::cout << std::endl << "Parent of topBank is = nullptr" << std::endl;
            }
            else {
                std::cout << std::endl << "Parent of topBank is = " << topBank->getParent()->toString() << std::endl;
            }

            parent = childBank->getParent();
            if (parent == nullptr) {
                std::cout << std::endl << "Parent of childBank is = nullptr" << std::endl;
            }
            else {
                std::cout << std::endl << "Parent of childBank is = " << parent->toString() << std::endl;
            }

            auto root = childBank->getRoot();
            std::cout << std::endl << "Root of childBank is = " << root->toString() << std::endl;
            root = topBank->getRoot();
            std::cout << "Root of topBank is = " << root->toString() << std::endl;

            std::cout << std::endl << "Is childBank root = " << childBank->isRoot() << std::endl;
            std::cout << "Is topBank root = " << topBank->isRoot() << std::endl << std::endl;

            std::shared_ptr<BaseStructure> node = topBank;
            std::cout << std::endl << "Starting from root:" << std::endl;
            do {
                node = node->getNextNode();
                if (node == nullptr) {
                    std::cout << "  next node = nullptr" << std::endl;
                }
                else {
                    std::cout << "  next node = " << node->toString() << std::endl;
                }
            } while (node != nullptr);


            node = midBank2;
            std::cout << std::endl << "Starting from midBank2:" << std::endl;
            do {
                node = node->getNextNode();
                if (node == nullptr) {
                    std::cout << "  next node = nullptr" << std::endl;
                }
                else {
                    std::cout << "  next node = " << node->toString() << std::endl;
                }
            } while (node != nullptr);


            node = midBank3;
            std::cout << std::endl << "Starting from midBank3:" << std::endl;
            do {
                node = node->getPreviousNode();
                if (node == nullptr) {
                    std::cout << "  prev node = nullptr" << std::endl;
                }
                else {
                    std::cout << "  prev node = " << node->toString() << std::endl;
                }
            } while (node != nullptr);

            std::cout << std::endl << "is childBank child of topBank = " << topBank->isNodeChild(childBank) << std::endl;
            std::cout << "is midBank3 child of topBank = " << topBank->isNodeChild(midBank3) << std::endl;

            std::cout << std::endl << "first child of topBank = " << topBank->getFirstChild()->toString() << std::endl;
            std::cout << "last child of topBank = " << topBank->getLastChild()->toString() << std::endl;
            std::cout << "child after midBank2 = " << topBank->getChildAfter(midBank2)->toString() << std::endl;
            std::cout << "child before midBank3 = " << topBank->getChildBefore(midBank3)->toString() << std::endl;

            std::cout << std::endl << "is midBank sibling of midBank3 = " << midBank->isNodeSibling(midBank3) << std::endl;
            std::cout << "sibling count of midBank3 = " << midBank3->getSiblingCount() << std::endl;
            std::cout << "next sibling of midBank = " << midBank->getNextSibling()->toString() << std::endl;
            std::cout << "prev sibling of midBank2 = " << midBank2->getPreviousSibling()->toString() << std::endl;
            std::cout << "prev sibling of midBank = " << midBank->getPreviousSibling() << std::endl;


            std::cout << std::endl << "Add 2 children to midBank2 & and 1 child to 3" << std::endl;
            auto childBank2 = EvioBank::getInstance(5, DataType::INT32, 5);
            auto childBank3 = EvioBank::getInstance(6, DataType::INT32, 6);
            auto childBank4 = EvioBank::getInstance(7, DataType::SHORT16, 7);

            // Child's data
            auto &iData = childBank2->getIntData();
            iData.push_back(3);
            iData.push_back(4);
            iData.push_back(5);
            childBank2->updateIntData();

            auto &iData2 = childBank3->getIntData();
            iData2.push_back(6);
            iData2.push_back(7);
            iData2.push_back(8);
            childBank3->updateIntData();

            auto &sData = childBank4->getShortData();
            sData.push_back(10);
            sData.push_back(11);
            sData.push_back(12);
            childBank4->updateShortData();

            // add to tree
            midBank2->add(childBank2);
            midBank2->add(childBank3);
            midBank3->add(childBank4);


            std::cout << std::endl << "childBank isLeaf = " << childBank->isLeaf() << std::endl;
            std::cout << "topBank isLeaf = " << topBank->isLeaf() << std::endl;
            std::cout << "topBank leaf count = " << topBank->getLeafCount() << std::endl;
            std::cout << "midBank2 leaf count = " << midBank2->getLeafCount() << std::endl;
            std::cout << "topBank first Leaf = " << topBank->getFirstLeaf()->toString() << std::endl;
            std::cout << "topBank last Leaf = " << topBank->getLastLeaf()->toString() << std::endl;
            std::cout << "midBank2 next Leaf = " << midBank2->getNextLeaf()->toString() << std::endl;
            std::cout << "childBank2 prev Leaf = " << childBank2->getPreviousLeaf()->toString() << std::endl;
            std::cout << "childBank prev Leaf = " << childBank->getPreviousLeaf() << std::endl << std::endl;


            std::cout << std::endl << "Add 1 child to topBank with same tag (4) as first leaf but num = 20" << std::endl;
            auto midBank4 = EvioBank::getInstance(4, DataType::BANK, 20);
            topBank->add(midBank4);

            //////////////////////////////////////////////////////
            // FINDING STRUCTURES
            //////////////////////////////////////////////////////

            std::cout << "Search for all banks of tag = 4 Using StructureFinder, got the following:" << std::endl;
            uint16_t tag = 4;
            uint8_t  num = 4;
            std::vector<std::shared_ptr<BaseStructure>> vec;

            StructureFinder::getMatchingBanks(topBank, tag, num, vec);
            for (auto n : vec) {
                std::cout << "  bank = " << n->toString() << std::endl;
            }
            vec.clear();


            std::cout << "Search for all banks of tag = 4, got the following:" << std::endl;
            class myFilter : public IEvioFilter {
                uint16_t tag;
                //uint8_t num = 0;
            public:
                myFilter(uint16_t tag) : tag(tag) {}
                bool accept(StructureType const & type, std::shared_ptr<BaseStructure> struc) override {
                    return ((type == StructureType::STRUCT_BANK) &&
                            (tag == struc->getHeader()->getTag()));
                }
            };

            auto filter = std::make_shared<myFilter>(4);
            topBank->getMatchingStructures(filter, vec);
            for (auto n : vec) {
                std::cout << "  bank = " << n->toString() << std::endl;
            }

            std::cout<< std::endl << "Search again for all banks of tag = 4, and execute listener:" << std::endl;
            class myListener : public IEvioListener {
            public:
                void gotStructure(std::shared_ptr<BaseStructure> topStructure,
                                  std::shared_ptr<BaseStructure> structure) {
                    std::cout << "  TOP struct = " << topStructure->toString() << std::endl;
                    std::cout << "  GOT struct = " << structure->toString() << std::endl << std::endl;
                }

                // We're not parsing so these are not used ...
                void startEventParse(std::shared_ptr<BaseStructure> structure) {
                    std::cout << "  start parsing struct = " << structure->toString() << std::endl;
                }
                void endEventParse(std::shared_ptr<BaseStructure> structure) {
                    std::cout << "  end parsing struct = " << structure->toString() << std::endl;
                }
            };

            auto listener = std::make_shared<myListener>();
            topBank->visitAllStructures(listener, filter);


        }



     // Test the ByteBuffer's slice() method
     static void ByteBufferTest1() {

        ByteBuffer b(24);
         b.putInt(0, 1);
         b.putInt(4, 2);
         b.putInt(8, 3);
         b.putInt(12, 4);
         b.putInt(16, 5);
         b.putInt(20, 6);

         Util::printBytes(b, 0, 24, "original");
         std::cout << "orig buf: pos = " << b.position() << ", lim = " <<  b.limit() << ", cap = " << b.capacity() <<
            ", off = " << b.arrayOffset() << std::endl << std::endl;

         // Make the slice start at 3rd int and limit is right after that
         b.position(8);
         b.limit(20);
         auto sl = b.slice();

         // change slice data
         sl->putInt(0, 0x33);
         sl->putInt(4, 0x44);
         sl->putInt(8, 0x55);

         // print slice
         Util::printBytes(sl, sl->position(), sl->capacity(), "slice1");
         std::cout << "slice: pos = " << sl->position() << ", lim = " << sl->limit() << ", cap = " << sl->capacity() <<
                   ", off = " << sl->arrayOffset() << std::endl << std::endl;

         // Make a slice of a slice
         sl->position(4);
         sl->limit(12);
         auto sl2 = sl->slice();
         sl2->putInt(0, 0x444);
         sl2->putInt(4, 0x555);

         // print slice2
         Util::printBytes(sl2, sl2->position(), sl2->capacity(), "slice2");
         std::cout << "slice2: pos = " << sl2->position() << ", lim = " << sl2->limit() << ", cap = " << sl2->capacity() <<
                   ", off = " << sl2->arrayOffset() << std::endl << std::endl;


         // print original buf again
         b.clear();
         Util::printBytes(b, 0, 24, "original again");
         std::cout << "orig buf again: pos = " << b.position() << ", lim = " <<  b.limit() << ", cap = " << b.capacity() <<
                      ", off = " << b.arrayOffset() << std::endl << std::endl;

    }



    // Test the ByteBuffer's use with memory mapped file
    static void ByteBufferTest2() {

        ByteBuffer b(24);
        b.putInt(0, 1);
        b.putInt(4, 2);
        b.putInt(8, 3);
        b.putInt(12, 4);
        b.putInt(16, 5);
        b.putInt(20, 6);

        // Write this into a file
        size_t fileSz = 4*6;
        std::string fileName = "./myByteBufferTest2.dat";
        Util::writeBytes( fileName, b);

        // Create a read-write memory mapped file
        int fd;
        void *pmem;

        if ((fd = ::open(fileName.c_str(), O_RDWR)) < 0) {
            throw EvioException("file does NOT exist");
        }
        else {
            // set shared mem size
            if (::ftruncate(fd, (off_t) fileSz) < 0) {
                ::close(fd);
                throw EvioException("fail to open file");
            }
        }

        // map file to process space
        if ((pmem = ::mmap((caddr_t) 0, fileSz, PROT_READ | PROT_WRITE,
                           MAP_SHARED, fd, (off_t) 0)) == MAP_FAILED) {
            ::close(fd);
            throw EvioException("fail to map file");
        }

        // close fd for mapped mem since no longer needed
        ::close(fd);

        // Change the mapped memory into a ByteBuffer for ease of handling ...
        ByteBuffer readBuf(static_cast<char *>(pmem), fileSz, true);

        // print original buf again
        Util::printBytes(readBuf, 0, 24, "read mapped file");
        std::cout << "mmapped buf: pos = " << readBuf.position() << ", lim = " << readBuf.limit() <<
                     ", cap = " << readBuf.capacity() << ", off = " << readBuf.arrayOffset() << std::endl << std::endl;

        // Write to the ByteBuffer which is linked to the memory mapped file
        readBuf.putInt(4, 0x22);
        readBuf.putInt(8, 0x33);


        // NOW, define another ByteBuffer which reads from that file and see if the data changed
        // Change the mapped memory into a ByteBuffer for ease of handling ...
        ByteBuffer readBuf2(static_cast<char *>(pmem), fileSz, true);

        // print file again
        Util::printBytes(readBuf2, 0, 24, "read mapped file again");
        std::cout << "read mmapped file again: pos = " << readBuf2.position() << ", lim = " << readBuf2.limit() <<
                     ", cap = " << readBuf2.capacity() << ", off = " << readBuf2.arrayOffset() << std::endl << std::endl;
    }



    /** Contains methods to test Composite Data.  */
    class CompositeTester {

    public:

        static int test1() {

            uint32_t bank[24];

            //**********************/
            // bank of tagsegments */
            //**********************/
            bank[0] = 23;                       // bank length
            bank[1] = 6 << 16 | 0xF << 8 | 3;   // tag = 6, bank contains composite type, num = 3


            // N(I,D,F,2S,8a)
            // first part of composite type (for format) = tagseg (tag & type ignored, len used)
            bank[2]  = 5 << 20 | 0x3 << 16 | 4; // tag = 5, seg has char data, len = 4
            // ASCII chars values in latest evio string (array) format, N(I,D,F,2S,8a) with N=2
            if (ByteOrder::isLocalHostBigEndian()) {
                bank[3] = 0x4E << 24 | 0x28 << 16 | 0x49 << 8 | 0x2C;    // N ( I ,
                bank[4] = 0x44 << 24 | 0x2C << 16 | 0x46 << 8 | 0x2C;    // D , F ,
                bank[5] = 0x32 << 24 | 0x53 << 16 | 0x2C << 8 | 0x38;   // 2 S , 8
                bank[6] = 0x61 << 24 | 0x29 << 16 | 0x00 << 8 | 0x04;   // a ) \0 \4
            }
            else {
                bank[3] = 0x4E | 0x28 << 8 | 0x49 << 16 | 0x2C << 24;    // N ( I ,
                bank[4] = 0x44 | 0x2C << 8 | 0x46 << 16 | 0x2C << 24;    // D , F ,
                bank[5] = 0x32 | 0x53 << 8 | 0x2C << 16 | 0x38 << 24;   // 2 S , 8
                bank[6] = 0x61 | 0x29 << 8 | 0x00 << 16 | 0x04 << 24;   // a ) \0 \4
            }

            // second part of composite type (for data) = bank (tag, num, type ignored, len used)
            bank[7]  = 16;
            bank[8]  = 6 << 16 | 0xF << 8 | 1;   // tag = 6, num = 1
            bank[9]  = 0x2; // N
            bank[10] = 0x1111; // I
            // Double
            double d = 3.14159;
            uint64_t l = *(reinterpret_cast<uint64_t *>(&d));
            if (!ByteOrder::isLocalHostBigEndian()) {
                bank[11] = l & 0xffffffff;    // lower 32 bits
                bank[12] = ((l >> 32) & 0xffffffff);    // higher 32 bits
            }
            else {
                bank[11] = ((l >> 32) & 0xffffffff);    // higher 32 bits
                bank[12] = l & 0xffffffff;   // lower 32 bits
            }
            // Float
            float f = (float)(3.14159);
            bank[13] = *(reinterpret_cast<int32_t *>(&f));

            if (ByteOrder::isLocalHostBigEndian()) {
                bank[14] = 0x22114433; // 2S
            }
            else {
                bank[14] = 0x33441122; // 2S
            }

            if (ByteOrder::isLocalHostBigEndian()) {
                bank[15] = 0x48 << 24 | 0x49 << 16 | 0x00 << 8 | 0x48;    // H  I \0  H
                bank[16] = 0x4F << 24 | 0x00 << 16 | 0x04 << 8 | 0x04;    // 0 \ 0 \4 \4
            }
            else {
                bank[15] = 0x48 | 0x49 << 8 | 0x00 << 16 | 0x48 << 24;    // H  I \0  H
                bank[16] = 0x4F | 0x00 << 8 | 0x04 << 16 | 0x04 << 24;    // 0 \ 0 \4 \4
            }

            // duplicate data
            for (int i=0; i < 7; i++) {
                bank[17+i] = bank[10+i];
            }

            // all composite including headers
            uint32_t * allData = new uint32_t[22];
            for (int i=0; i < 22; i++) {
                allData[i] = bank[i+2];
            }

            // analyze format string
            std::string format = "N(I,D,F,2S,8a)";

            try {

                int swapper = 3;

                if (swapper == 1) {
                    // change int array into byte array
                    auto byteArray = reinterpret_cast<uint8_t *>(allData);

                    std::cout << "Go from bytes to CDs:" << std::endl;
                    std::vector<std::shared_ptr<CompositeData>> cdList;
                    // This makes a copy of the raw bytes in byteArray
                    CompositeData::parse(byteArray, 4 * 22, ByteOrder::ENDIAN_LOCAL, cdList);

                    // Wrap bytes in ByteBuffer for ease of printing later.
                    // From now on, byteArray pointer cannot be used!!!
                    ByteBuffer buf(byteArray, 4 * 22, false);

                    Util::printBytes(buf, 0, 4 * 22, "Orig Data:");

                    std::cout << "Print CD orig:" << std::endl;
                    printCompositeDataObject(cdList[0]);

                    // Swap raw bytes in this object
                    cdList[0]->swap();
                    auto &swappedBytes = cdList[0]->getRawBytes();
                    Util::printBytes(swappedBytes.data(), 4 * 22, "Swapped Data:");

                    // Swap data again
                    cdList[0]->swap();
                    auto &swappedBytes2 = cdList[0]->getRawBytes();
                    Util::printBytes(swappedBytes2.data(), 4 * 22, "Double swapped Data:");

                    // Wrap bytes in ByteBuffer for ease of printing later
                    // Cannot make the underlying vector memory part of a ByteBuffer!!!
                    uint8_t *dsData = new uint8_t[4 * 22];
                    std::memcpy(dsData, swappedBytes2.data(), 4 * 22);
                    ByteBuffer doubleSwapBuf(dsData, 4 * 22, false);

                    // Check for differences
                    std::cout << "CHECK FOR DIFFERENCES:" << std::endl;
                    bool goodSwap = true;
                    for (int i = 0; i < 22; i++) {
                        if (buf.getInt(i) != doubleSwapBuf.getInt(i)) {
                            std::cout << std::hex << std::showbase << "orig = " << doubleSwapBuf.getInt(i) <<
                                      ", double swapped = " << buf.getInt(i) << std::endl;
                            goodSwap = false;
                        }
                    }
                    std::cout << "good swap = " << goodSwap << std::endl;

                    // Recreate the composite object from a ByteBuffer of double swapped data
                    auto cData = CompositeData::getInstance(doubleSwapBuf);
                    std::cout << "cData object = " << cData->toString() << std::endl << std::endl;
                    printCompositeDataObject(*(cData.get()));
                }
                else if (swapper == 2) {
                    // change int array into byte array
                    auto byteArray = reinterpret_cast<uint8_t *>(allData);

                    std::cout << "Go from bytes to CDs:" << std::endl;
                    std::vector<std::shared_ptr<CompositeData>> cdList;
                    // This makes a copy of the raw bytes in byteArray
                    CompositeData::parse(byteArray, 4 * 22, ByteOrder::ENDIAN_LOCAL, cdList);

                    // Wrap bytes in ByteBuffer for ease of printing later.
                    // From now on, byteArray pointer cannot be used!!!
                    ByteBuffer buf(byteArray, 4 * 22, false);

                    Util::printBytes(buf, 0, 4 * 22, "Orig Data:");

                    // Swap raw bytes in this object
                    std::cout << "CALL CompositeData::swapAll()" << std::endl;
                    CompositeData::swapAll(byteArray, nullptr, 22, true);
                    Util::printBytes(byteArray, 4 * 22, "Swapped Data:");


                    // Swap data again
                    std::cout << "CALL CompositeData::swapAll() again" << std::endl;
                    CompositeData::swapAll(byteArray, nullptr, 22, false);
                    Util::printBytes(byteArray, 4 * 22, "Double Swapped Data:");


                    // Wrap bytes in ByteBuffer for ease of printing later
                    // Cannot make the underlying vector memory part of a ByteBuffer!!!
                    uint8_t *dsData = new uint8_t[4 * 22];
                    std::memcpy(dsData, byteArray, 4 * 22);
                    ByteBuffer doubleSwapBuf(dsData, 4 * 22, false);

                    // Check for differences
                    std::cout << "CHECK FOR DIFFERENCES:" << std::endl;
                    bool goodSwap = true;
                    for (int i = 0; i < 22; i++) {
                        if (buf.getInt(i) != doubleSwapBuf.getInt(i)) {
                            std::cout << std::hex << std::showbase << "orig = " << doubleSwapBuf.getInt(i) <<
                                      ", double swapped = " << buf.getInt(i) << std::endl;
                            goodSwap = false;
                        }
                    }
                    std::cout << "good swap = " << goodSwap << std::endl;

                    // Recreate the composite object from a ByteBuffer of double swapped data
                    auto cData = CompositeData::getInstance(doubleSwapBuf);
                    std::cout << "cData object = " << cData->toString() << std::endl << std::endl;
                    printCompositeDataObject(*(cData.get()));
                }
                else if (swapper == 3) {
                    // change int array into byte array
                    auto byteArray = reinterpret_cast<uint8_t *>(allData);

                    std::cout << "Go from bytes to CDs:" << std::endl;
                    std::vector<std::shared_ptr<CompositeData>> cdList;
                    // This makes a copy of the raw bytes in byteArray
                    CompositeData::parse(byteArray, 4 * 22, ByteOrder::ENDIAN_LOCAL, cdList);

                    // Wrap bytes in ByteBuffer for ease of printing later.
                    // From now on, byteArray pointer cannot be used!!!
                    ByteBuffer buf(byteArray, 4 * 22, false);
                    // COpy data for later comparison to double swapped data
                    ByteBuffer bufCopy(buf);

                    Util::printBytes(buf, 0, 4 * 22, "Orig Data:");

                    // Swap raw bytes in this object
                    std::cout << "CALL CompositeData::swapAll()" << std::endl;
                    CompositeData::swapAll(buf, 0, 22);
                    Util::printBytes(buf, 0, 4 * 22, "Swapped Data:");

                    ByteBuffer doubleSwapBuf( 4 * 22);
                    doubleSwapBuf.order(ByteOrder::ENDIAN_LOCAL);

                    // Swap data again
                    std::cout << "CALL CompositeData::swapAll() again" << std::endl;
                    CompositeData::swapAll(buf, doubleSwapBuf, 0, 0, 22);
                    Util::printBytes(doubleSwapBuf, 0, 4 * 22, "Double Swapped Data");

                    // Check for differences
                    std::cout << "CHECK FOR DIFFERENCES:" << std::endl;
                    bool goodSwap = true;
                    for (int i = 0; i < 22; i++) {
                        if (bufCopy.getInt(i) != doubleSwapBuf.getInt(i)) {
                            std::cout << std::hex << std::showbase << "orig = " << doubleSwapBuf.getInt(i) <<
                                      ", double swapped = " << bufCopy.getInt(i) << std::endl;
                            goodSwap = false;
                        }
                    }
                    std::cout << "good swap = " << goodSwap << std::endl;

                    // Recreate the composite object from a ByteBuffer of double swapped data
                    auto cData = CompositeData::getInstance(doubleSwapBuf);
                    std::cout << "cData object = " << cData->toString() << std::endl << std::endl;
                    printCompositeDataObject(*(cData.get()));
                }
            }
            catch (EvioException & e) {
                std::cout << e.what() << std::endl;
            }

            return 0;
        }


        /**
         * Simple example of providing a format string and some data
         * in order to create a CompositeData object.
         */
        static int test2() {

            // Create a CompositeData object ...

            // Format to write an int and a string
            // To get the right format code for the string, use a helper method
            std::string myString = "string";
            std::vector<std::string> strings;
            strings.push_back(myString);
            std::string stringFormat = CompositeData::stringsToFormat(strings);

            // Put the 2 formats together
            std::string format = "I," + stringFormat;
            std::cout << "format = " << format << std::endl;

            // Now create some data
            CompositeData::Data myData;
            myData.addInt(2);
            myData.addString(myString);

            // Create CompositeData object
            auto cData = CompositeData::getInstance(format, myData, 1, 0 ,0);

            // Print it out
            printCompositeDataObject(cData);
            return 0;
        }


        /**
         * More complicated example of providing a format string and some data
         * in order to create a CompositeData object.
         */
        static int test3() {

            // Create a CompositeData object ...

            // Format to write a N shorts, 1 float, 1 double a total of N times
            std::string format = "N(NS,F,D)";

            std::cout << "format = " << format << std::endl;

            // Now create some data
            CompositeData::Data myData;
            myData.addN(2);
            myData.addN(3);
            std::vector<short> shorts;
            shorts.push_back(1);
            shorts.push_back(2);
            shorts.push_back(3);
            myData.addShort(shorts); // use vector for convenience
            myData.addFloat(1.0F);
            myData.addDouble(3.14159);
            myData.addN(1);
            myData.addShort((short)4);
            myData.addFloat(2.0F);
            myData.addDouble(2.*3.14159);

            // Create CompositeData object
            auto cData = CompositeData::getInstance(format, myData, 12, 22 ,33);
            std::cout << "created CD object with " << cData->getRawBytes().size() << " raw bytes\n";
            std::cout << "created CD object with " << cData->getNValues().size() << " N values\n";
            // Print it out
            printCompositeDataObject(*(cData.get()));

            try {
                auto ev = EvioEvent::getInstance(0, DataType::COMPOSITE, 0);
                auto & comps = ev->getCompositeData();
                comps.push_back(cData);
                ev->updateCompositeData();
                size_t rbSize = ev->getRawBytes().size();
                std::cout << "Raw byte size = " << rbSize << std::endl;
                uint32_t bytesOut = 200 > rbSize ? rbSize : 200;

                Util::printBytes(ev->getRawBytes().data(), bytesOut, "RawBytes of event with comp data");

                // Write it to this file
                std::cout << "Write ./composite.data" << std::endl;
                std::string fileName  = "./composite.dat";
                EventWriter writer(fileName);
                writer.writeEvent(ev);
                writer.close();

                Util::printBytes(fileName, 0, 188, "Composite Raw Data");


                // Read it from file
                std::cout << "Now read ./composite.data" << std::endl;
                EvioReader reader(fileName);
                auto evR = reader.parseNextEvent();

                if (evR != nullptr) {
                    auto h = evR->getHeader();
                    std::cout << "event: tag = " << h->getTag() <<
                              ", type = " << h->getDataTypeName() << ", len = " << h->getLength() << std::endl;

                    auto & cDataR = evR->getCompositeData();
                    std::cout << "event: comp data vec size = " << cDataR.size() << std::endl;
                    for (auto & cd : cDataR) {
                        printCompositeDataObject(cd);
                    }
                }


            }
            catch (EvioException & e) {
                std::cout << e.what() << std::endl;
            }



            return 0;
        }


        /**
         * Test vectors of CompositeData objects.
         */
        static int test4() {

            // Format to write 1 int and 1 float a total of N times
            std::string format = "N(I,F)";

            std::cout << "Format = " << format << std::endl;

            // Now create some data
            CompositeData::Data myData1;
            myData1.addN(1);
            myData1.addInt(1); // use array for convenience
            myData1.addFloat(1.0F);

            // Now create some data
            CompositeData::Data myData2;
            myData2.addN(1);
            myData2.addInt(2); // use array for convenience
            myData2.addFloat(2.0F);

            // Now create some data
            CompositeData::Data myData3;
            myData3.addN(1);
            myData3.addInt(3); // use array for convenience
            myData3.addFloat(3.0F);

            std::cout << "Create composite data objects" << format << std::endl;

            // Create CompositeData object
            std::vector<std::shared_ptr<CompositeData>> cData;
            auto cData0 = CompositeData::getInstance(format, myData1, 1, 1, 1);
            auto cData1 = CompositeData::getInstance(format, myData2, 2, 2, 2);
            auto cData2 = CompositeData::getInstance(format, myData3, 3, 3, 3);

            // Print it out
            std::cout << "Print composite data objects" << std::endl;
            printCompositeDataObject(*(cData0.get()));
            printCompositeDataObject(*(cData1.get()));
            printCompositeDataObject(*(cData2.get()));

            try {
                auto ev = EvioEvent::getInstance(0, DataType::COMPOSITE, 0);
              //  auto & compDataVec = ev->getCompositeData();

                // Write it to this file
                std::string fileName  = "./composite.dat";

                std::cout << "WRITE FILE:" << std::endl;
                EventWriter writer(fileName, ByteOrder::ENDIAN_LITTLE);
                writer.writeEvent(ev);
                writer.close();

                // Read it from file
                std::cout << "READ FILE & PRINT CONTENTS:" << std::endl;
                EvioReader reader(fileName);
                auto evR = reader.parseNextEvent();

                if (evR != nullptr) {
                    auto h = evR->getHeader();
                    std::cout << "event: tag = " << h->getTag() <<
                              ", type = " << h->getDataTypeName() << ", len = " << h->getLength() << std::endl;

                    auto cDataR = evR->getCompositeData();
                    for (auto & cd : cDataR) {
                        printCompositeDataObject(cd);
                    }
                }

            }
            catch (EvioException & e) {
                std::cout << e.what() << std::endl;
            }

            return 0;
        }



        /**
         * Print the data from a CompositeData object in a user-friendly form.
         * @param cData CompositeData object
         */
        static void printCompositeDataObject(std::shared_ptr<CompositeData> cData) {
            printCompositeDataObject(*(cData.get()));
        }


        /**
         * Print the data from a CompositeData object in a user-friendly form.
         * @param cData CompositeData object
         */
        static void printCompositeDataObject(CompositeData & cData) {

            std::cout << "\n************************\nFormat = " << cData.getFormat() << std::endl << std::endl;
            // Get vectors of data items & their types from composite data object
            auto items = cData.getItems();
            auto types = cData.getTypes();

            // Use these list to print out data of unknown format
            size_t len = items.size();

            for (size_t i=0; i < len; i++) {
                auto type = types[i];
                std::cout << "type = " << std::setw(9) << type.toString() << std::endl;

                if ((type == DataType::NVALUE || type == DataType::UNKNOWN32 ||
                     type == DataType::UINT32 || type == DataType::INT32)) {
                    uint32_t j = items[i].item.ui32;
                    std::cout << hex << showbase << j << std::endl;
                }
                else if (type == DataType::LONG64 || type == DataType::ULONG64) {
                    uint64_t l = items[i].item.ul64;
                    std::cout << hex << showbase << l << std::endl;
                }
                else if (type == DataType::SHORT16 || type == DataType::USHORT16) {
                    uint16_t s = items[i].item.us16;
                    std::cout << hex << showbase << s << std::endl;
                }
                else if (type == DataType::CHAR8 || type == DataType::UCHAR8) {
                    uint8_t b = items[i].item.ub8;
                    std::cout << hex << showbase << (char)b << std::endl;
                }
                else if (type == DataType::FLOAT32) {
                    float ff = items[i].item.flt;
                    std::cout << ff << std::endl;
                }
                else if (type == DataType::DOUBLE64) {
                    double dd = items[i].item.dbl;
                    std::cout << dd << std::endl;
                }
                else if (type == DataType::CHARSTAR8) {
                    auto strs = items[i].strVec;
                    for (std::string const & ss : strs) {
                        std::cout << ss << ", ";
                    }
                    std::cout << std::endl;
                }
                std::cout << dec;
            }

        }

    };


    // Test the EventBuilder and CompactEventBuilder classes
    static void EventBuilderTest() {
        //---------------------------
        // Test regular EventBuilder:
        //---------------------------

        uint16_t tag = 1;
        DataType dataType = DataType::BANK;
        uint8_t num = 1;

        EventBuilder eb(tag, dataType, num);
        //EventBuilder(std::shared_ptr<EvioEvent> & event);
        auto ev = eb.getEvent();

        EventBuilder eb2(tag+1, DataType::SHORT16, num+1);
        auto ev2 = eb2.getEvent();
        short sData[3] = {1, 2, 3};
        eb2.appendShortData(ev2, sData, 3);
        eb.addChild(ev, ev2);


        std::cout << "EventBuilder's ev:\n" << ev->toString() << std::endl;
        std::cout << "EventBuilder's ev2:\n" << ev2->toString() << std::endl;

        EventBuilder eb3(tag+2, DataType::UINT32, num+2);
        auto ev3 = eb3.getEvent();
        eb.setEvent(ev3);
        uint32_t iData[4] = {11, 22, 33, 44};
        eb.appendUIntData(ev3, iData, 4);

        std::cout << "EventBuilder's ev3:\n" << ev3->toString() << std::endl;

        //---------------------------
        // Test CompactEventBuilder:
        //---------------------------

        size_t bufSize = 1000;
        CompactEventBuilder ceb(bufSize, ByteOrder::ENDIAN_LOCAL, true);
        //explicit CompactEventBuilder(std::shared_ptr<ByteBuffer> buffer, bool generateNodes = false);
        ceb.openBank(4, DataType::SEGMENT, 4);
        ceb.openSegment(5, DataType::DOUBLE64);
        double dd[3] = {1.11, 2.22, 3.33};
        ceb.addDoubleData(dd, 3);
        ceb.closeAll();
        auto cebEvbuf = ceb.getBuffer();

        //Util::printBytes(cebEvbuf, 0 , 200, "From CompactEventBuilder");

        // Write into a buffer
        auto newBuf = std::make_shared<ByteBuffer>(1000);
        EventWriter writer(newBuf);
        writer.writeEvent(cebEvbuf);
        writer.close();
        auto writerBuf = writer.getByteBuffer();

        //Util::printBytes(newBuf, 0 , 200, "From EventWriter");

        // Read event back out of buffer
        EvioReader reader(writerBuf);
        auto cebEv = reader.getEvent(1);

        std::cout << "CompactEventBuilder's cebEv:\n" << cebEv->toString() << std::endl;
    }


}



int main(int argc, char **argv) {
    evio::TreeTest();
    //evio::ByteBufferTest1();
    //evio::myByteBufferTest2();
    //evio::CompositeTester::test1();
    //evio::CompositeTester::test2();
    //evio::CompositeTester::test3();
    //evio::CompositeTester::test4();
    //evio::EventBuilderTest();
    return 0;
}


