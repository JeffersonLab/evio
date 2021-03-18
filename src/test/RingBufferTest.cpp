//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


/**
 * Class used for testing C++ based ring buffer code.
 * @date 07/05/2019
 * @author timmer
 */


#include <string>
#include <cstdint>
#include <chrono>
#include <thread>
#include <memory>
#include <regex>
#include <iostream>


#include "Disruptor/RingBuffer.h"


using namespace std;
using namespace Disruptor;


namespace evio {

///////////////////////////////////////////////////////////////////////////////////////////

    class Integer {

        private:
            int val;

        public:
            Integer() {val = 123;}
            explicit Integer(int v) {val = v;}
            int get() {return val;}
            void set(int v) {val = v;}

            /** Function to create Integers by RingBuffer. */
            static const std::function< std::shared_ptr<Integer> () >& eventFactory() {
                static std::function< std::shared_ptr<Integer> () > result([] {
                    return std::move(std::make_shared<Integer>());
                });
                return result;
            }
    };


/////////////////////////////////////////////////////////////////////////////////////////


    /**
      * Class used to compressed items, "write" them, and put them back.
      * Last barrier on ring.
      * It is an interruptible thread from the boost library, and only 1 exists.
      */
    class Writer {

    private:

        /** RingBuffer. */
        std::shared_ptr<RingBuffer<std::shared_ptr<Integer>>> ringBuffer;
        /** WriteBarrier to use when getting items to "write". */
        std::shared_ptr<Disruptor::ISequenceBarrier> writeBarrier;
        /** Gating sequence (last before publisher). */
        std::shared_ptr<Disruptor::ISequence> gateSequence;
        /** Thread which does the file writing. */
        boost::thread thd;

    public:

        /**
         * Constructor.
          * @param ringBuf
          * @param writeBar
          * @param gateSeq
          */
        Writer(std::shared_ptr<RingBuffer< std::shared_ptr<Integer>>> & ringBuf,
               std::shared_ptr<Disruptor::ISequenceBarrier> & barrier,
               std::shared_ptr<Disruptor::ISequence> sequence) :
            ringBuffer(ringBuf), writeBarrier(barrier), gateSequence(sequence)  {}


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

            cout << "Running Writer thd " << endl;

            try {
                auto seq1 = std::make_shared<Disruptor::Sequence>(Disruptor::Sequence::InitialCursorValue);
                int64_t nextWriteSeq = seq1->value() + 1;
                int64_t availableWriteSeq = -1L;

                while (true) {
                    if (availableWriteSeq < nextWriteSeq) {
                        availableWriteSeq = writeBarrier->waitFor(nextWriteSeq);
                    }

                    std::shared_ptr<Integer> & item = ((*ringBuffer.get())[nextWriteSeq]);
                    cout << "Writing item " << item->get() << endl;
                    gateSequence->setValue(nextWriteSeq++);
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            }
            catch (std::exception & e) {
                cout << "Writer INTERRUPTED, return" << endl;
            }
        }
    };


//////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Class used to take items from ring buffer, "compress" them, and place them back.
     */
    class Compressor {

    private:

        /** RingBuffer. */
        std::shared_ptr<RingBuffer<std::shared_ptr<Integer>>> ringBuffer;
        /** CompressBarrier to use when getting items to "compress". */
        std::shared_ptr<Disruptor::ISequenceBarrier> compBarrier;
        /** Sequence. */
        std::shared_ptr<Disruptor::ISequence> compSequence;
        /** Thread which does the file writing. */
        boost::thread thd;

        /** Keep track of this thread with id number. */
        uint32_t threadNumber;
        /** Total number of Compressor threads. */
        uint32_t threadCount;

    public:

          /**
           * Constructor.
           * @param threadNum
           * @param threadCount
           * @param ringBuf
           * @param barrier
           * @param sequence
           */
        Compressor(uint32_t threadNum, uint32_t threadCnt,
                   std::shared_ptr<RingBuffer< std::shared_ptr<Integer>>> & ringBuf,
                   std::shared_ptr<Disruptor::ISequenceBarrier> & barrier,
                   std::shared_ptr<Disruptor::ISequence> sequence) :
        threadNumber(threadNum), threadCount(threadCnt), ringBuffer(ringBuf),
        compBarrier(barrier), compSequence(sequence)  {}


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

            cout << "Running Compressor thd " << threadNumber << endl;

            // The first time through, we need to release all records coming before
            // our first in case there are < threadNumber records before close() is called.
            // This way close() is not waiting for thread #12 to get and subsequently
            // release items 0 - 11 when there were only 5 records total.
            // (threadNumber starts at 0).
            compSequence->setValue((int)threadNumber - 1);
            cout << "Compressor thd " << threadNumber << " intially releasing " << ((int)threadNumber - 1) << endl;

            try {
                auto seq1 = std::make_shared<Disruptor::Sequence>(Disruptor::Sequence::InitialCursorValue);
                int64_t nextWriteSeq = seq1->value() + 1 + threadNumber;
                int64_t availableWriteSeq = -1L;

                while (true) {
                    if (availableWriteSeq < nextWriteSeq) {
                        availableWriteSeq = compBarrier->waitFor(nextWriteSeq);
                    }

                    std::shared_ptr<Integer> & item = ((*ringBuffer.get())[nextWriteSeq]);
cout << "Comp " << threadNumber << ":  " << item->get() << ", next " << (nextWriteSeq+threadCount) << endl;
                    compSequence->setValue(nextWriteSeq);
                    nextWriteSeq += threadCount;

                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            }
            catch (std::exception & e) {
                cout << "Comp thread " << threadNumber << " INTERRUPTED, return" << endl;
            }
        }
    };

////////////////////////////////////////////////////////////////////////////////////////////




        static void disruptorTest() {


            /** Ring buffer. Variable ringSize needs to be defined first. */
            std::shared_ptr<Disruptor::RingBuffer<std::shared_ptr<Integer>>> ringBuffer;

            // Stuff for compression threads

            /** Ring barrier to prevent records from being used by write thread
             *  before compression threads release them. */
            std::shared_ptr<Disruptor::ISequenceBarrier> compressBarrier;
            /** Sequences for compressing data, one per compression thread. */
            std::vector<std::shared_ptr<Disruptor::ISequence>> compressSeqs;
            /** Array of next sequences (index of next item desired),
             *  one per compression thread. */
            std::vector<int64_t> nextCompressSeqs;
            /** Array of available sequences (largest index of sequentially available items),
             *  one per compression thread. */
            std::vector<int64_t> availableCompressSeqs;

            // Stuff for writing thread

            /** Ring barrier to prevent records from being re-used by producer
             *  before write thread releases them. */
            std::shared_ptr<Disruptor::ISequenceBarrier> writeBarrier;
            /** Sequence for writing data. */
            std::vector<std::shared_ptr<Disruptor::ISequence>> writeSeqs;


            /** Threads used to compress data. */
            std::vector<Compressor> compressorThreads;
            /** Thread used to write data to file/buffer.
             *  Easier to use vector here so we don't have to construct it immediately. */
            std::vector<Writer> writerThreads;


            /** Number of threads doing compression simultaneously. */
            uint32_t compressionThreadCount = 2;

            /** Number of records held in this supply. */
            const uint32_t ringSize = 32;


            // Create ring buffer with "ringSize" # of elements
            ringBuffer = Disruptor::RingBuffer<std::shared_ptr<Integer>>::createSingleProducer(
                         Integer::eventFactory(), ringSize);

            // Thread which fills records is considered the "producer" and doesn't need a barrier

            // Barrier & sequences so record-COMPRESSING threads can get records.
            // This is the first group of consumers which all share the same barrier.
            compressBarrier = ringBuffer->newBarrier();
            compressSeqs.reserve(compressionThreadCount);
            nextCompressSeqs.reserve(compressionThreadCount);
            availableCompressSeqs.reserve(compressionThreadCount);
            compressorThreads.reserve(compressionThreadCount);

            for (int i=0; i < compressionThreadCount; i++) {
                // Create seq with usual initial value
                std::shared_ptr<Disruptor::Sequence> seq = std::make_shared<Disruptor::Sequence>(Disruptor::Sequence::InitialCursorValue);

                // Each thread will get different records from each other.
                // First thread gets 0, 2nd thread gets 1, etc.
                int64_t firstSeqToGet = Disruptor::Sequence::InitialCursorValue + 1 + i;
                nextCompressSeqs.push_back(firstSeqToGet);
                // Release, in advance, records to be skipped next. Keeps things from hanging up.
                if (i != 0) {
//cout << "SKIPPING over val of " << (firstSeqToGet - 1) << " for thd# i, " << i << endl;
                    seq->setValue(firstSeqToGet - 1);
                }
                compressSeqs.push_back(seq);
                // Initialize with -1's
                availableCompressSeqs.push_back(-1);

                // Create compression thread
                compressorThreads.emplace_back(i, compressionThreadCount, ringBuffer, compressBarrier, seq);
            }
cout << "EventWriter constr: created " << compressionThreadCount << " number of comp thds" << endl;

            // Barrier & sequence so a single record-WRITING thread can get records.
            // This barrier comes after all compressing threads and depends on them
            // first releasing their records.
            writeBarrier = ringBuffer->newBarrier(compressSeqs);
            auto writeSeq = std::make_shared<Disruptor::Sequence>(Disruptor::Sequence::InitialCursorValue);
            writeSeqs.push_back(writeSeq);
            // After this writing thread releases a record, make it available for re-filling.
            // In other words, this is the last consumer.
            ringBuffer->addGatingSequences(writeSeqs);


            // Start compression threads
            for (int i=0; i < compressionThreadCount; i++) {
                compressorThreads[i].startThread();
            }

            // Create and start writing thread
            writerThreads.emplace_back(ringBuffer, writeBarrier, writeSeq);
            writerThreads[0].startThread();

            uint32_t counter = 0;

            while (true) {
                // Producer gets next available record
                int64_t getSequence = ringBuffer->next();

                // Get object in that position (sequence) of ring buffer
                std::shared_ptr<Integer> & item = (*ringBuffer.get())[getSequence];
                item->set(counter++);

                cout << "Producer got ring item & set to " << item->get() << endl;
                ringBuffer->publish(getSequence);
            }


        }



}



int main(int argc, char **argv) {
    evio::disruptorTest();
    return 0;
}


