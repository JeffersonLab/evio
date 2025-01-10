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


#include <cstdint>
#include <chrono>
#include <thread>
#include <memory>
#include <regex>
#include <fstream>

#include "eviocc.h"


using namespace std;


namespace evio {

    //-------------------------------------------------------------
    // Class used to test RecordSupply and RecordSupplyItem classes.
    // These are never used by the user directly.
    //-------------------------------------------------------------



    /////////////////////////////////////////////////////////////////////////////////////////


    /**
      * Class used to get compressed items, "write" them, and put them back.
      * Last barrier on ring.
      * It is an interruptable thread from the boost library, and only 1 exists.
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
                    supply(recSupply), threadNumber(threadNum)  {}


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

                    std::this_thread::sleep_for(std::chrono::seconds(2));
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
        for (uint32_t i=0; i < compressionThreadCount; i++) {
            compressorThreads.emplace_back(i, supply);
        }

        // Start compression threads
        for (uint32_t i=0; i < compressionThreadCount; i++) {
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

}



int main(int argc, char **argv) {
    evio::recordSupplyTest();
    return 0;
}


