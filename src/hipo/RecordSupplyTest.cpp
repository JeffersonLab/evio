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

#include "EvioBank.h"
#include "EvioSegment.h"
#include "StructureTransformer.h"
#include "EvioSwap.h"

#include "RecordSupply.h"
#include "ByteOrder.h"
#include "Disruptor/RingBuffer.h"
#include <boost/thread.hpp>

#include "IBlockHeader.h"
#include "BlockHeaderV2.h"

using namespace std;


namespace evio {


/////////////////////////////////////////////////////////////////////////////////////////


    /**
      * Class used to compressed items, "write" them, and put them back.
      * Last barrier on ring.
      * It is an interruptible thread from the boost library, and only 1 exists.
      */
    class Writer2 {

    private:

        /** Supply of RecordRingItems. */
        std::shared_ptr<RecordSupply> supply;
        /** Thread which does the file writing. */
        boost::thread thd;

    public:

        /**
         * Constructor.
         * @param recSupply
         */
        Writer2(std::shared_ptr<RecordSupply> recSupply) : supply(recSupply)  {}


        /** Create and start a thread to execute the run() method of this class. */
        void startThread() {
            thd = boost::thread([this]() {this->run();});
        }

        /** Stop the thread. */
        void stopThread() {
            // Send signal to interrupt it
            thd.interrupt();
            // Wait for it to stop
            thd.join();
        }

        /** Run this method in thread. */
        void run() {
            try {
                while (true) {
                    // Get the next record for this thread to write
                    auto item = supply->getToWrite();
                    cout << "   W : v" << item->getId() << endl;
                    supply->releaseWriterSequential(item);
                }
            }
            catch (boost::thread_interrupted & e) {
                cout << "     Writer: INTERRUPTED, return" << endl;
            }
        }

    };


//////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Class used to take items from ring buffer, "compress" them, and place them back.
     */
    class Compressor2 {

    private:

        /** Supply of RecordRingItems. */
        std::shared_ptr<RecordSupply> supply;
        /** Thread which does the file writing. */
        boost::thread thd;
        /** Keep track of this thread with id number. */
        uint32_t threadNumber;

    public:

          /**
           * Constructor.
           * @param threadNum
           * @param threadCount
           * @param ringBuf
           * @param barrier
           * @param sequence
           */
        Compressor2(uint32_t threadNum, std::shared_ptr<RecordSupply> & recSupply) :
                    threadNumber(threadNum), supply(recSupply)  {}


        /** Create and start a thread to execute the run() method of this class. */
        void startThread() {
            thd = boost::thread([this]() {this->run();});
        }

        /** Stop the thread. */
        void stopThread() {
            // Send signal to interrupt it
            thd.interrupt();
            // Wait for it to stop
            thd.join();
        }

        /** Method to run in the thread. */
        void run() {

            try {

                // The first time through, we need to release all records coming before
                // our first in case there are < threadNumber records before close() is called.
                // This way close() is not waiting for thread #12 to get and subsequently
                // release items 0 - 11 when there were only 5 records total.
                // (threadNumber starts at 0).
                if (threadNumber > 0) {
                    supply->release(threadNumber, threadNumber - 1);
                }

                while (true) {
                    // Get the next record for this thread to compress
                    auto item = supply->getToCompress(threadNumber);
                    cout << "   C" << threadNumber << ": v" << item->getId() << endl;

                    // Release back to supply
                    supply->releaseCompressor(item);

                    std::this_thread::sleep_for(2s);
                }
            }
            catch (std::exception & e) {
                cout << "Com" << threadNumber << ": INTERRUPTED, return" << endl;
            }
        }
    };

////////////////////////////////////////////////////////////////////////////////////////////



    static void recordSupplyTest() {

        /** Threads used to compress data. */
        std::vector<Compressor2> compressorThreads;
        /** Thread used to write data to file/buffer.
         *  Easier to use vector here so we don't have to construct it immediately. */
        std::vector<Writer2> writerThreads;

        /** Number of threads doing compression simultaneously. */
        const uint32_t compressionThreadCount = 2;

        /** Number of records held in this supply. */
        const uint32_t ringSize = 32;

        /** The byte order in which to write a file or buffer. */
        ByteOrder byteOrder = ByteOrder::ENDIAN_LITTLE;

        Compressor::CompressionType compressionType = Compressor::UNCOMPRESSED;

        /** Fast supply of record items for filling, compressing and writing. */
        std::shared_ptr<RecordSupply> supply =
                std::make_shared<RecordSupply>(ringSize, byteOrder,
                                               compressionThreadCount,
                                                0, 0, compressionType);

        // Create compression threads
        compressorThreads.reserve(compressionThreadCount);
        for (int i=0; i < compressionThreadCount; i++) {
            compressorThreads.emplace_back(i, supply);
        }

        // Start compression threads
        for (int i=0; i < compressionThreadCount; i++) {
            compressorThreads[i].startThread();
        }

        // Create and start writing thread
        writerThreads.emplace_back(supply);
        writerThreads[0].startThread();

        uint32_t counter = 0;

        while (true) {
            // Producer gets next available record
            std::shared_ptr<RecordRingItem> item = supply->get();
            item->setId(counter++);
            cout << "P -> " << item->getId() << endl;
            supply->publish(item);
        }


    }

    static void mySwapTest() {
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
        midBank->add(childBank);

        std::cout << "EvioBank = " << topBank->toString() << std::endl;

        EvioSwap::swapData(topBank);

        std::cout << "Swapped top bank = " << topBank->toString() << std::endl;
        auto & swappedData = childBank->getFloatData();
        std::cout << "Swapped float data = " << std::endl;
        for (auto i : swappedData) {
            std::cout << "data -> " << i << std::endl;
        }

        EvioSwap::swapData(topBank);

        std::cout << "Swapped top bank AGAIN = " << topBank->toString() << std::endl;
        auto & swappedData2 = childBank->getFloatData();
        std::cout << "Swapped float data = " << std::endl;
        for (auto i : swappedData2) {
            std::cout << "data -> " << i << std::endl;
        }

        uint16_t tag2 = 2;
        DataType type2 = DataType::BANK;
        const auto evSeg = EvioSegment::getInstance(tag2, type2);
        std::cout << "EvioSeg = " << evSeg->toString() << std::endl;

        StructureTransformer::copy(evSeg, topBank);
        std::cout << "EvioSeg after copy = " << evSeg->toString() << std::endl;

        std::shared_ptr<EvioSegment> const & newSegment = StructureTransformer::transform(topBank);
        std::cout << "EvioSeg after transform = " << newSegment->toString() << std::endl;

    }


        static void myTreeTest() {

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
        for (int i=0; i < kidCount; i++) {
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

            bool isLeaf() const;
            std::shared_ptr<BaseStructure> getFirstLeaf();
            std::shared_ptr<BaseStructure> getLastLeaf();
            std::shared_ptr<BaseStructure> getNextLeaf();
            std::shared_ptr<BaseStructure> getPreviousLeaf();
            ssize_t getLeafCount();

        }

    static std::string myStrings[3] = {"a", "b", "c"};

    static void printStrings(std::string * strs, int count) {
        for (int i=0; i < count; i++) {
            cout << "string #" << i << " = " << strs[i] << std::endl;
        }
    }
     static void myTest2() {
        // Can we handle an array of strings??
        printStrings(myStrings, 3);


    }



}



int main(int argc, char **argv) {
    evio::myTreeTest();
    return 0;
}


