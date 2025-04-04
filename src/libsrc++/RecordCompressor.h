//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_RECORDCOMPRESSOR_H
#define EVIO_6_0_RECORDCOMPRESSOR_H


#include <iostream>
#include <iomanip>
#include <string>
#include <thread>
#include <memory>


#include "RecordOutput.h"
#include "RecordHeader.h"
#include "Compressor.h"
#include "RecordSupply.h"


#include "Disruptor/Util.h"
#include <boost/thread.hpp>


namespace evio {


    /**
     * Class used to create a thread which takes data-filled records from a RingBuffer-backed
     * RecordSupply, compresses them, and places them back into the supply.
     * It is an interruptible thread from the boost library.
     * <b>This of use internally only.</b>
     *
     * @date 01/22/2020
     * @author timmer
     */
    class RecordCompressor {

    private:

        /** Keep track of this thread with id number. */
        uint32_t threadNumber;
        /** Type of compression to perform. */
        Compressor::CompressionType compressionType;
        /** Supply of RecordRingItems. */
        std::shared_ptr<RecordSupply> supply;
        /** Thread which does the compression. */
        boost::thread thd;

    public:

        /**
         * Constructor.
         * @param thdNum        unique thread number starting at 0.
         * @param type          type of compression to do.
         * @param recordSupply  supply of records to compress.
         */
        RecordCompressor(uint32_t thdNum, Compressor::CompressionType & type,
                         std::shared_ptr<RecordSupply> recordSupply) :
                threadNumber(thdNum),
                compressionType(type),
                supply(recordSupply) {
        }


        /**
         * Move constructor.
         * Used to transfer ownership of resources from one object to another without making a deep copy.
         * It is typically used when an object is being moved rather than copied.
         * @param obj
         */
        RecordCompressor(RecordCompressor && obj) noexcept :
                threadNumber(obj.threadNumber),
                compressionType(obj.compressionType),
                supply(std::move(obj.supply)),
                thd(std::move(obj.thd)) {
        }

        /** Define equal operator. */
        RecordCompressor & operator=(RecordCompressor && obj) noexcept {
            if (this != &obj) {
                threadNumber = obj.threadNumber;
                compressionType = obj.compressionType;
                supply = std::move(obj.supply);
                thd  = std::move(obj.thd);
            }
            return *this;
        }

        /** Destructor. */
        ~RecordCompressor() {
            try {
                stopThread();
            }
            catch (const std::exception& e) {
                std::cerr << "Exception during thread cleanup: " << e.what() << std::endl;
            }
        }

        /** Create and start a thread to execute the run() method of this class. */
        void startThread() {
            thd = boost::thread([this]() {this->run();});
        }

        /** Stop the thread. */
        void stopThread() {
             if (thd.joinable()) {
                 // Send signal to interrupt it
                 thd.interrupt();

                // Wait for it to stop
                if (thd.try_join_for(boost::chrono::milliseconds(1))) {
                    //std::cout << "RecordCompressor JOINED from interrupt" << std::endl;
                    return;
                }

                // If that didn't work, send Alert signal to ring
                supply->errorAlert();

                if (thd.joinable()) {
                    thd.join();
                    //std::cout << "RecordCompressor JOINED from alert" << std::endl;
                }
            }
        }

        /** Method to run in the thread. */
        void run() {

            try {

                // The first time through, we need to release all records coming before
                // our first in case there are < threadNumber records before close() is called.
                // This way close() is not waiting for thread #12 to get and subsequently
                // release items 0 - 11 when there were only 5 records total.
                // (threadNumber starts at 0).

                // Be careful when dealing with negative numbers and unsigned ints ...
                int64_t seqNumber = (int64_t)threadNumber - 1;
                supply->release(threadNumber, seqNumber);

                while (true) {

                    // Get the next record for this thread to compress
                    auto item = supply->getToCompress(threadNumber);

                    {
                        // Only allow interruption when blocked on trying to get item
                        boost::this_thread::interruption_point();
                        boost::this_thread::disable_interruption d1;

                        // Pull record out of wrapping object
                        std::shared_ptr<RecordOutput> record = item->getRecord();
                        // Set compression type
                        auto header = record->getHeader();
                        header->setCompressionType(compressionType);
//cout << "RecordCompressor thd " << threadNumber << ": got record, set rec # to " << header->getRecordNumber() << endl;
                        // Do compression
                        record->build();
                        // Release back to supply
                        supply->releaseCompressor(item);
                    }
                }
            }
            catch (Disruptor::AlertException & e) {
                // Woken up in getToCompress through user call to supply.errorAlert()
                //std::cout << "RecordCompressor: quit thread through alert" << std::endl;
            }
            catch (boost::thread_interrupted & e) {
                // Never happen since we don't use timeout wait strategy
                //cout << "RecordCompressor thd " << threadNumber << ": INTERRUPTED, return" << endl;
            }
        }
    };


}


#endif //EVIO_6_0_RECORDCOMPRESSOR_H
