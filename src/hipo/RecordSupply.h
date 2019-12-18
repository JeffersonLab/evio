//
// Created by timmer on 11/5/19.
//

#ifndef EVIO_6_0_RECORDSUPPLY_H
#define EVIO_6_0_RECORDSUPPLY_H

#include <string>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>

//#include "disruptor.h"
#include "ByteOrder.h"
#include "Compressor.h"
#include "RecordRingItem.h"

#include "Disruptor/Util.h"
#include "Disruptor/Sequence.h"
#include "Disruptor/ISequence.h"
#include "Disruptor/RingBuffer.h"
#include "Disruptor/ISequenceBarrier.h"
#include "Disruptor/TimeoutException.h"

class RecordSupply {

private:

    std::mutex supplyMutex;

    /** Byte order of RecordOutputStream in each RecordRingItem. */
    ByteOrder order {ByteOrder::ENDIAN_LITTLE};

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
    string error {""};

    // Stuff for reporting conditions (disk is full)

    /** Writing of a RecordRingItem to disk has been stopped
     * due to the disk partition being full. */
    std::atomic<bool> diskFull{false};

    // Stuff for compression threads

    /** Ring barrier to prevent records from being used by write thread
     *  before compression threads release them. */
    std::shared_ptr<Disruptor::ISequenceBarrier> compressBarrier;
    /** Sequences for compressing data, one per compression thread. */
    std::vector<std::shared_ptr<Disruptor::Sequence>> compressSeqs;
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
    std::vector<std::shared_ptr<Disruptor::Sequence>> writeSeqs;
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
    RecordSupply(uint32_t ringSize, ByteOrder & order,
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

    void release(uint32_t threadNum, uint64_t sequenceNum);

    bool haveError();

    void haveError(bool err);

    std::string getError();

    void setError(string & err);

    bool isDiskFull();

    void setDiskFull(bool full);

};


#endif //EVIO_6_0_RECORDSUPPLY_H
