//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_RECORDRINGITEM_H
#define EVIO_6_0_RECORDRINGITEM_H


#include <memory>
#include <atomic>
#include <functional>


#include "Disruptor/Sequence.h"
#include "RecordOutput.h"
#include "ByteOrder.h"
#include "Compressor.h"


namespace evio {

    /**
     * This class provides the items which are supplied by the RecordSupply class.
     *
     * @date 11/05/2019
     * @author timmer
     */
    class RecordRingItem {

    private:

        // These static members are NOT THREAD SAFE! Fortunately, the chances that an
        // application uses more than one RecordSupply is remote. The chances that
        // their construction occurs at the same time is even more remote.

        /** Byte order to be used by factory creating these RecordRingItems. */
        static ByteOrder factoryByteOrder;
        /** MaxEventCount to be used by factory creating these RecordRingItems. */
        static int factoryMaxEventCount;
        /** MaxBufferSize to be used by factory creating these RecordRingItems. */
        static int factoryMaxBufferSize;
        /** CompressionType to be used by factory creating these RecordRingItems. */
        static Compressor::CompressionType factoryCompressionType;

        /** Assign each record a unique id for debugging purposes. */
        static uint64_t idValue;


        /** Record object, needs shared outside access. */
        std::shared_ptr<RecordOutput> record;

        /** Byte order of record being built. */
        ByteOrder order {ByteOrder::ENDIAN_LOCAL};

        /** Sequence at which this object was taken from ring by one of the "get" calls. */
        int64_t sequence = 0UL;

        /** Sequence object allowing ring consumer to get/release this item. */
        std::shared_ptr<Disruptor::ISequence> sequenceObj = nullptr;

        /** Do we split a file after writing this record? */
        std::atomic<bool> splitFileAfterWriteBool{false};

        /** Do we force the record to be physically written to disk? */
        std::atomic<bool> forceToDiskBool{false};

        /** If a new file needs to be created ({@link #splitFileAfterWrite} is true),
         * but there is not enough free space on the disk partition for the
         * next, complete file, return without creating or writing to file.
         * If {@link #forceToDisk} is true, write anyway. */
        std::atomic<bool> checkDisk{false};

        /** Processing thread may need to know if this is the last item
         *  to be processed so thread can shutdown. */
        std::atomic<bool> lastItem{false};

        /** Keep track of whether this item has already been released. */
        bool alreadyReleased = false;

        /** We may want to track a particular record for debugging. */
        uint64_t id = 0;


    public:


        static const std::function< std::shared_ptr<RecordRingItem> () >& eventFactory();

        static void setEventFactorySettings(ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize,
                                            Compressor::CompressionType & compressionType);


        RecordRingItem();
        RecordRingItem(const RecordRingItem & item);
        ~RecordRingItem() = default;

        RecordRingItem & operator=(const RecordRingItem & other) = delete;

        void reset();

        std::shared_ptr<RecordOutput> & getRecord();
        ByteOrder & getOrder();
        int64_t getSequence() const;
        std::shared_ptr<Disruptor::ISequence> & getSequenceObj();

        void fromProducer(int64_t seq);
        void fromConsumer(int64_t seq, std::shared_ptr<Disruptor::ISequence> & seqObj);

        bool splitFileAfterWrite();
        void splitFileAfterWrite(bool split);

        bool forceToDisk();
        void forceToDisk(bool force);

        bool isCheckDisk();
        void setCheckDisk(bool check);

        bool isLastItem();
        void setLastItem(bool last);

        bool isAlreadyReleased() const;
        void setAlreadyReleased(bool released);

        uint64_t getId() const;
        void setId(uint64_t idVal);

    };

}


#endif //EVIO_6_0_RECORDRINGITEM_H
