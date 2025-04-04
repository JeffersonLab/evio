//
// Copyright (c) 2020, Jefferson Science Associates
//
// Thomas Jefferson National Accelerator Facility
// EPSCI Group
//
// 12000, Jefferson Ave, Newport News, VA 23606
// Phone : (757)-269-7100
//


#include "RecordSupply.h"


namespace evio {


    /**
     * Constructor. Ring size of 4 records, compression thread count of 1,
     * no compression, local endian data.
     */
    RecordSupply::RecordSupply() : ringSize(4) {
        ringBuffer = Disruptor::RingBuffer<std::shared_ptr<RecordRingItem>>::createSingleProducer(RecordRingItem::eventFactory(), ringSize);
    }


    /**
     * Constructor.
     * @param ringSize        number of RecordRingItem objects in ring buffer.
     * @param order           byte order of RecordOutputStream in each RecordRingItem object.
     * @param threadCount     number of threads simultaneously doing compression.
     *                        Must be <= ringSize.
     * @param maxEventCount   max number of events each record can hold.
     *                        Value <= O means use default (1M).
     * @param maxBufferSize   max number of uncompressed data bytes each record can hold.
     *                        Value of < 8MB results in default of 8MB.
     * @param compressionType type of data compression to do :
     *                         (Compressor::UNCOMPRESSED = 0 = none,
     *                          Compressor::LZ4 = 1 = lz4 fast,
     *                          Compressor::LZ4_BEST = 2 = lz4 best,
     *                          Compressor::GZIP = 3 = gzip).
     * @throws EvioException if args < 1, ringSize not power of 2,
     *                                  threadCount > ringSize.
     */
    RecordSupply::RecordSupply(uint32_t ringSize, ByteOrder order,
                               uint32_t threadCount, uint32_t maxEventCount, uint32_t maxBufferSize,
                               Compressor::CompressionType & compressionType) :

            order(order), maxBufferSize(maxBufferSize), compressionType(compressionType), ringSize(ringSize)
    {

        if (ringSize < 1 || !Disruptor::Util::isPowerOf2(ringSize)) {
            throw EvioException("ringSize must be a power of 2");
        }

        if (ringSize < threadCount) {
            throw EvioException("threadCount must be <= ringSize");
        }

        // # compression threads defaults to 1 if given bad value
        if (threadCount > 0) {
            compressionThreadCount = threadCount;
        }

        // Set RecordRingItem static values to be used when eventFactory is creating RecordRingItem objects
        RecordRingItem::setEventFactorySettings(order, maxEventCount, maxBufferSize, compressionType);

        // Spin first then block
        auto blockingStrategy = std::make_shared< Disruptor::BlockingWaitStrategy >();
        auto waitStrategy = std::make_shared< Disruptor::SpinCountBackoffWaitStrategy >(10000, blockingStrategy);
        // Create ring buffer with "ringSize" # of elements
        ringBuffer = Disruptor::RingBuffer<std::shared_ptr<RecordRingItem>>::createSingleProducer(
                RecordRingItem::eventFactory(), ringSize, waitStrategy);

        // Thread which fills records is considered the "producer" and doesn't need a barrier

        // Barrier & sequences so record-COMPRESSING threads can get records.
        // This is the first group of consumers which all share the same barrier.
        compressBarrier = ringBuffer->newBarrier();
        compressSeqs.reserve(compressionThreadCount);
        nextCompressSeqs.reserve(compressionThreadCount);
        availableCompressSeqs.reserve(compressionThreadCount);

        for (uint32_t i=0; i < compressionThreadCount; i++) {
            // Create seq with usual initial value
            auto seq = std::make_shared<Disruptor::Sequence>(Disruptor::Sequence::InitialCursorValue);

            // Each thread will get different records from each other.
            // First thread gets 0, 2nd thread gets 1, etc.
            int64_t firstSeqToGet = Disruptor::Sequence::InitialCursorValue + 1 + i;
            nextCompressSeqs.push_back(firstSeqToGet);
            // Release, in advance, records to be skipped next. Keeps things from hanging up.
            if (i != 0) {
                seq->setValue(firstSeqToGet - 1);
            }
            compressSeqs.push_back(seq);
            // Initialize with -1's
            availableCompressSeqs.push_back(-1);
        }

        // Barrier & sequence so a single record-WRITING thread can get records.
        // This barrier comes after all compressing threads and depends on them
        // first releasing their records.
        writeBarrier = ringBuffer->newBarrier(compressSeqs);
        auto seq = std::make_shared<Disruptor::Sequence>(Disruptor::Sequence::InitialCursorValue);
        nextWriteSeq = Disruptor::Sequence::InitialCursorValue + 1;
        writeSeqs.push_back(seq);
        availableWriteSeq = -1L;
        // After this writing thread releases a record, make it available for re-filling.
        // In other words, this is the last consumer.
        ringBuffer->addGatingSequences(writeSeqs);
    }


    /**
     * Method to have sequence barriers throw a Disruptor's AlertException.
     * In this case, we can use it to warn write and compress threads which
     * are waiting on barrier.waitFor() in {@link #getToCompress(uint32_t)} and
     * {@link #getToWrite()}. Do this in case of a write, compress, or some other error.
     * This allows any threads waiting on these 2 methods to wake up, clean up,
     * and exit.
     */
    void RecordSupply::errorAlert() {
        writeBarrier->alert();
        compressBarrier->alert();
    }


    /**
     * Get the max number of bytes the records in this supply can hold all together.
     * @return max number of bytes the records in this supply can hold all together.
     */
    uint32_t RecordSupply::getMaxRingBytes() const {return (int) (ringSize*1.1*maxBufferSize);}


    /**
     * Get the number of records in this supply.
     * @return number of records in this supply.
     */
    uint32_t RecordSupply::getRingSize() const {return ringSize;}


    /**
     * Get the byte order of all records in this supply.
     * @return byte order of all records in this supply.
     */
    ByteOrder & RecordSupply::getOrder() {return order;}


    /**
     * Get the percentage of data-filled but unwritten records in ring.
     * Value of 0 means everything's been written. Value of 100 means
     * that all records in the ring are filled with data (perhaps in
     * various stages of being compressed) and have not been written yet.
     *
     * @return percentage of used records in ring.
     */
    uint64_t RecordSupply::getFillLevel() {
        return 100*(ringBuffer->cursor() - ringBuffer->getMinimumGatingSequence())/ringBuffer->bufferSize();
    }


    /**
     * Get the sequence of last ring buffer item published (seq starts at 0).
     * @return sequence of last ring buffer item published (seq starts at 0).
     */
    int64_t RecordSupply::getLastSequence() {
        return ringBuffer->cursor();
    }


    /**
     * Get the next available record item from the ring buffer.
     * Use it to write data into the record.
     * @return next available record item in ring buffer in order to write data into it.
     */
    std::shared_ptr<RecordRingItem> RecordSupply::get() {
        // Producer gets next available record
        int64_t getSequence = ringBuffer->next();

        // Get object in that position (sequence) of ring buffer
        std::shared_ptr<RecordRingItem> bufItem = (*ringBuffer.get())[getSequence];

        // This reset does not change compression type, fileId, or header type
        bufItem->reset();

        // Store sequence for later releasing of record
        bufItem->fromProducer(getSequence);

        return bufItem;
    }


    /**
     * Tell consumers that the record item is ready for consumption.
     * To be used in conjunction with {@link #get()}.
     * @param item record item available for consumers' use.
     */
    void RecordSupply::publish(std::shared_ptr<RecordRingItem> item) {
        if (item == nullptr) return;
        ringBuffer->publish(item->getSequence());
    }


    /**
     * Get the next available record item from the ring buffer
     * in order to compress the data already in it.
     * @param threadNumber number of thread (0,1, ...) used to compress.
     *                     This number cannot exceed (compressionThreadCount - 1).
     * @return next available record item in ring buffer
     *         in order to compress data already in it.
     * @throws Disruptor::AlertException  if {@link #errorAlert()} called.
     */
    std::shared_ptr<RecordRingItem> RecordSupply::getToCompress(uint32_t threadNumber) {

        try  {
            // Only wait for read of volatile memory if necessary ...
            if (availableCompressSeqs[threadNumber] < nextCompressSeqs[threadNumber]) {
                // Return # of largest consecutively available item
                availableCompressSeqs[threadNumber] = compressBarrier->waitFor(nextCompressSeqs[threadNumber]);
            }

            // Get the item since we know it's available
            std::shared_ptr<RecordRingItem> item = (*ringBuffer.get())[nextCompressSeqs[threadNumber]];
            // Store variables that will help free this item when release is called
            item->fromConsumer(nextCompressSeqs[threadNumber], compressSeqs[threadNumber]);
            // Set the next item we'll be trying to get.
            // Note that different compression threads get different items.
            nextCompressSeqs[threadNumber] += compressionThreadCount;
            return item;
        }
        catch (Disruptor::TimeoutException & ex) {
            // Never happen since we don't use timeout wait strategy
            std::cout << ex.message() << std::endl;
        }

        return nullptr;
    }


    /**
     * Get the next available record item from the ring buffer
     * in order to write data into it.
     * @return next available record item in ring buffer
     *         in order to write data into it.
     * @throws Disruptor::AlertException  if {@link #errorAlert()} called.
     */
    std::shared_ptr<RecordRingItem> RecordSupply::getToWrite() {

        try  {
            if (availableWriteSeq < nextWriteSeq) {
                availableWriteSeq = writeBarrier->waitFor(nextWriteSeq);
            }

            std::shared_ptr<RecordRingItem> item = ((*ringBuffer.get())[nextWriteSeq]);
            item->fromConsumer(nextWriteSeq++, writeSeqs[0]);
            return item;
        }
        catch (Disruptor::TimeoutException & ex) {
            // Never happen since we don't use timeout wait strategy
            std::cout << ex.message() << std::endl;
        }

        return nullptr;
    }


    /**
     * A compressing thread releases its claim on the given ring buffer item
     * so it becomes available for use by writing thread behind the write barrier.
     * Because a compressing thread gets only every Nth record where
     * N = compressionThreadCount, once it releases this record it also
     * needs to release all events coming after, up until the one it will
     * take next. In other words, also release the records it will skip over
     * next. This allows close() to be called at any time without things
     * hanging up.<p>
     *
     * To be used in conjunction with {@link #getToCompress(uint32_t)}.
     * @param item item in ring buffer to release for reuse.
     */
    void RecordSupply::releaseCompressor(std::shared_ptr<RecordRingItem> item) {
        if (item == nullptr) return;
        item->getSequenceObj()->setValue(item->getSequence() + compressionThreadCount - 1);
    }


    /**
     * A writer thread releases its claim on the given ring buffer item
     * so it becomes available for reuse by the producer.
     * To be used in conjunction with {@link #getToWrite()}.<p>
     *
     * Care must be taken to ensure thread-safety.
     * This method may only be called if the writing is done IN THE SAME THREAD
     * as the calling of this method so that items are released in sequence
     * as ensured by the caller.
     * Otherwise use {@link #releaseWriter(std::shared_ptr<RecordRingItem>)}.
     *
     * @param item item in ring buffer to release for reuse.
     * @return false if item not released or item is null, else true.
     */
    bool RecordSupply::releaseWriterSequential(std::shared_ptr<RecordRingItem> item) {
        if (item == nullptr || item->isAlreadyReleased()) return false;
        item->getSequenceObj()->setValue(item->getSequence());
        return true;
    }


    /**
     * A writer thread releases its claim on the given ring buffer item
     * so it becomes available for reuse by the producer.
     * To be used in conjunction with {@link #getToWrite()}.<p>
     *
     * Care must be taken to ensure thread-safety.
     * The following can happen if no precautions are taken.
     * In the case of EventWriterUnsync, writing to a file involves 2, simultaneous,
     * asynchronous writes to a file - both in separate threads to the thread which
     * calls the "write" method. If the writing of the later item finishes first, it
     * releases it's item and sequence which, unfortunately, also releases the
     * previous item's sequence (which is still being written). When the first write is complete,
     * it also releases its item. However item.getSequenceObj() will return null
     * (causing NullPointerException) because it was already released thereby allowing
     * it to be reused and reset called on it.<p>
     *
     * In order to prevent such a scenario, releaseWriter ensures that items are only
     * released in sequence.<p>
     *
     * If the same item is released more than once, bad things will happen.
     * Thus the caller must take steps to prevent it. To avoid problems,
     * call {@link RecordRingItem#setAlreadyReleased(bool)} and set to true if
     * item is released but will still be used in some manner.
     *
     * @param item item in ring buffer to release for reuse.
     * @return false if item not released since item is null or is already released, else true.
     */
    bool RecordSupply::releaseWriter(std::shared_ptr<RecordRingItem> item) {

        if (item == nullptr || item->isAlreadyReleased()) {
            //cout << "RecordSupply: item already released!" << endl;
            return false;
        }

        supplyMutex.lock();
        {
            int64_t seq = item->getSequence();

            // If we got a new max ...
            if (seq > maxSequence) {
                // If the old max was > the last released ...
                if (maxSequence > lastSequenceReleased) {
                    // we now have another sequence between last released & new max
                    between++;
                }

                // Set the new max
                maxSequence = seq;
            }
            // If we're < max and > last, then we're in between
            else if (seq > lastSequenceReleased) {
                between++;
            }

            // If we now have everything between last & max, release it all.
            // This way higher sequences are never released before lower.
            if ((maxSequence - lastSequenceReleased - 1L) == between) {
                item->getSequenceObj()->setValue(maxSequence);
                lastSequenceReleased = maxSequence;
                between = 0;
            }
        }
        supplyMutex.unlock();

        return true;
    }


    /**
     * Release claim on ring items up to sequenceNum for the given compressor thread.
     * For internal use only - to free up records that the compressor thread will skip
     * over anyway.
     * @param threadNum    compressor thread number.
     * @param sequenceNum  sequence to release.
     */
    void RecordSupply::release(uint32_t threadNum, int64_t sequenceNum) {
        if (sequenceNum < 0) return;
        compressSeqs[threadNum]->setValue(sequenceNum);
    }


    /**
     * Has an error occurred in writing or compressing data?
     * @return {@code true} if an error occurred in writing or compressing data, else {@code false}.
     */
    bool RecordSupply::haveError() {return haveErrorCondition.load();}


    /**
     * Set whether an error occurred in writing or compressing data.
     * @param err if {@code true} an error occurred in writing or compressing data, else {@code false}.
     */
    void RecordSupply::haveError(bool err) {haveErrorCondition.store(err);}


    /**
     * If there is an error, this contains the error message.
     * @return error message if there is an error.
     */
    std::string RecordSupply::getError() {
        supplyMutex.lock();
        std::string cpy = error;
        supplyMutex.unlock();
        return cpy;
    }


    /**
     * Set the error message.
     * @param err error message.
     */
    void RecordSupply::setError(std::string & err) {
        supplyMutex.lock();
        error = err;
        supplyMutex.unlock();
    }


    /**
     * Has the writing of a RecordRingItem to disk has been stopped
     * due to the disk partition being full?
     * @return  true if he writing of a RecordRingItem to disk has been stopped
     *          due to the disk partition being full.
     */
    bool RecordSupply::isDiskFull() {return diskFull.load();}


    /**
     * Set whether the writing of a RecordRingItem to disk has been stopped
     * due to the disk partition being full.
     * @param full true if he writing of a RecordRingItem to disk has been stopped
     *             due to the disk partition being full.
     */
    void RecordSupply::setDiskFull(bool full) {diskFull.store(full);}

}


