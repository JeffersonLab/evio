//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_RECORDSUPPLY_H
#define EVIO_6_0_RECORDSUPPLY_H


#include <string>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>


#include "ByteOrder.h"
#include "Compressor.h"
#include "RecordRingItem.h"
#include "EvioException.h"
#include "Disruptor/Util.h"
#include "Disruptor/Sequence.h"
#include "Disruptor/ISequence.h"
#include "Disruptor/RingBuffer.h"
#include "Disruptor/ISequenceBarrier.h"
#include "Disruptor/TimeoutException.h"
#include "Disruptor/SpinCountBackoffWaitStrategy.h"


namespace evio {


    /**
     * This thread-safe, lock-free class is used to provide a very fast supply
     * of RecordRingItems which are reused (using Disruptor software package).<p>
     *
     * It is a supply of RecordRingItems in which a single producer does a {@link #get()},
     * fills the record with data, and finally does a {@link #publish(std::shared_ptr<RecordRingItem> &)}
     * to let consumers know the data is ready.<p>
     *
     * This class is setup to handle 2 types of consumers.
     * The first type is a thread which compresses a record's data.
     * The number of such consumers is set in the constructor.
     * Each of these will call {@link #getToCompress(uint32_t)} to get a record
     * and eventually call {@link #releaseCompressor(std::shared_ptr<RecordRingItem> &)} to indicate it is
     * finished compressing and the record is available for writing to disk.<p>
     *
     * The second type of consumer is a single thread which writes all compressed
     * records to a file. This will call {@link #getToWrite()} to get a record
     * and eventually call {@link #releaseWriter(std::shared_ptr<RecordRingItem> &)} to indicate it is
     * finished writing and the record is available for being filled with new data.<p>
     *
     * Due to the multithreaded nature of writing files using this class, a mechanism
     * for reporting errors that occur in the writing and compressing threads is provided.
     * Also, and probably more importantly, one can call errorAlert() to notify any
     * compression or write threads that an error has occurred. That way these threads
     * can clean up and exit.<p>
     *
     * It transparently makes sure that all records are written in the proper order.
     *
     * <pre><code>
     *
     *   This is a graphical representation of how our ring buffer is set up.
     *
     *   (1) The producer who calls get() will get a ring item allowing a record to be
     *       filled. That same user does a publish() when done with the record.
     *
     *   (2) The consumer who calls getToCompress() will get that ring item and will
     *       compress its data. There may be any number of compression threads
     *       as long as <b># threads <= # of ring items!!!</b>.
     *       That same user does a releaseCompressor() when done with the record.
     *
     *   (3) The consumer who calls getToWrite() will get that ring item and will
     *       write its data to a file or another buffer. There may be only 1
     *       such thread. This same user does a releaseWriter() when done with the record.
     *
     *                         ||
     *                         ||  writeBarrier
     *           >             ||
     *         /             ________
     *    Write thread     /    |    \
     *              --->  / 1 _ | _ 2 \  <---- Compression Threads 1-M
     *  ================ | __ /   \ __ |               |
     *                   |  6 |    | 3 |               V
     *             ^     | __ | __ | __| ==========================
     *             |      \   5 |   4 /       compressBarrier
     *         Producer->  \ __ | __ /
     *
     *
     * </code></pre>
     *
     * @version 6.0
     * @since 6.0 11/5/19
     * @author timmer
     */
    class RecordSupply {

    private:

        /** Mutex for thread safety when setting error code or releasing resources. */
        std::mutex supplyMutex;

        /** Byte order of RecordOutputStream in each RecordRingItem. */
        ByteOrder order {ByteOrder::ENDIAN_LOCAL};

        /** Max number of events each record can hold.
         *  Value of O means use default (1M). */
        uint32_t maxEventCount = 0;
        /** Max number of uncompressed data bytes each record can hold.
         *  Value of < 8MB results in default of 8MB. */
        uint32_t maxBufferSize = 0;
        /** Type type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip). */
        Compressor::CompressionType compressionType {Compressor::UNCOMPRESSED};
        /** Number of threads doing compression simultaneously. */
        uint32_t compressionThreadCount = 1;
        /** Number of records held in this supply. */
        uint32_t ringSize = 0;


        /** Ring buffer. Variable ringSize needs to be defined first. */
        std::shared_ptr<Disruptor::RingBuffer<std::shared_ptr<RecordRingItem>>> ringBuffer = nullptr;


        // Stuff for reporting errors

        /** Do we have an error writing and/or compressing data? */
        std::atomic<bool> haveErrorCondition{false};
        /** Error string. No atomic<string> in C++, so protect with mutex. */
        std::string error {""};

        // Stuff for reporting conditions (disk is full)

        /** Writing of a RecordRingItem to disk has been stopped
         * due to the disk partition being full. */
        std::atomic<bool> diskFull{false};

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
        /** Index of next item desired. */
        int64_t nextWriteSeq = 0L;
        /** Largest index of sequentially available items. */
        int64_t availableWriteSeq = 0L;

        // For thread safety in getToWrite() & releaseWriter()

        /** The last sequence to have been released after writing. */
        int64_t lastSequenceReleased = -1L;
        /** The highest sequence to have asked for release after writing. */
        int64_t maxSequence = -1L;
        /** The number of sequences between maxSequence &
         * lastSequenceReleased which have called releaseWriter(), but not been released yet. */
        uint32_t between = 0;


    public:

        RecordSupply();
        // No need to copy these things
        RecordSupply(const RecordSupply & supply) = delete;
        RecordSupply(uint32_t ringSize, ByteOrder order,
                     uint32_t threadCount, uint32_t maxEventCount, uint32_t maxBufferSize,
                     Compressor::CompressionType & compressionType);

        ~RecordSupply() {
            compressSeqs.clear();
            nextCompressSeqs.clear();
            nextCompressSeqs.clear();
            availableCompressSeqs.clear();
            writeSeqs.clear();
            ringBuffer.reset();
        }

        void errorAlert();

        uint32_t getMaxRingBytes();
        uint32_t getRingSize();
        ByteOrder & getOrder();
        uint64_t getFillLevel();
        int64_t getLastSequence();

        std::shared_ptr<RecordRingItem> get();
        void publish(std::shared_ptr<RecordRingItem> & item);
        std::shared_ptr<RecordRingItem> getToCompress(uint32_t threadNumber);
        std::shared_ptr<RecordRingItem> getToWrite();

        void releaseCompressor(std::shared_ptr<RecordRingItem> & item);
        bool releaseWriterSequential(std::shared_ptr<RecordRingItem> & item);
        bool releaseWriter(std::shared_ptr<RecordRingItem> & item);
        void release(uint32_t threadNum, int64_t sequenceNum);

        bool haveError();
        void haveError(bool err);
        std::string getError();
        void setError(std::string & err);

        bool isDiskFull();
        void setDiskFull(bool full);

    };

}


#endif //EVIO_6_0_RECORDSUPPLY_H
