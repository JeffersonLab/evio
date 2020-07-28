//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "RecordRingItem.h"


namespace evio {


    //--------------------------------
    // STATIC INITIALIZATION
    //--------------------------------

    uint64_t RecordRingItem::idValue = 0ULL;


    // Set default values for RecordRingItems
    int       RecordRingItem::factoryMaxEventCount {0};
    int       RecordRingItem::factoryMaxBufferSize {0};
    ByteOrder RecordRingItem::factoryByteOrder {ByteOrder::ENDIAN_LITTLE};
    Compressor::CompressionType RecordRingItem::factoryCompressionType {Compressor::UNCOMPRESSED};


    /** Function to create RecordRingItems by RingBuffer. */
    const std::function< std::shared_ptr<RecordRingItem> () >& RecordRingItem::eventFactory() {
        static std::function< std::shared_ptr<RecordRingItem> () > result([] {
            return std::move(std::make_shared<RecordRingItem>());
        });
        return result;
    }


    // --------------------------------


    /**
     * Method to set RecordRingItem parameters for objects created by eventFactory.
     * @param order           byte order.
     * @param maxEventCount   max number of events each record can hold.
     *                        Value <= O means use default (1M).
     * @param maxBufferSize   max number of uncompressed data bytes each record can hold.
     *                        Value of < 8MB results in default of 8MB.
     * @param compressionType type of data compression to do.
     */
    void RecordRingItem::setEventFactorySettings(ByteOrder & order,
                                                 uint32_t maxEventCount, uint32_t maxBufferSize,
                                                 Compressor::CompressionType & compressionType) {
        RecordRingItem::factoryByteOrder = order;
        RecordRingItem::factoryMaxEventCount = maxEventCount;
        RecordRingItem::factoryMaxBufferSize = maxBufferSize;
        RecordRingItem::factoryCompressionType = compressionType;
    }


    /** Default constructor.
     *  Used in RecordSupply by eventFactory to create RecordRingItems for supply. */
    RecordRingItem::RecordRingItem() : order(ByteOrder::ENDIAN_LITTLE) {

        record = std::make_shared<RecordOutput>(RecordRingItem::factoryByteOrder,
                                                RecordRingItem::factoryMaxEventCount,
                                                RecordRingItem::factoryMaxBufferSize,
                                                RecordRingItem::factoryCompressionType);
        id = idValue++;
    }


    /**
     * Copy constructor (sort of). Used in EventWriter for when disk is full and a copy
     * of the item to be written is made for later writing. Original item is released
     * so ring can function. Note, not everything is copied (sequenceObj) since in usage,
     * the original item has already been released. Also, {@link #alreadyReleased} is true. <p>
     * <b>NOT to be used except internally by evio.</b>
     *
     * @param item ring item to copy.
     */
    RecordRingItem::RecordRingItem(const RecordRingItem & item) : order(item.order) {

        // Avoid self copy ...
        if (this != &item) {
            id = item.id;
            order = item.order;
            sequence = item.sequence;
            lastItem.store(item.lastItem);
            checkDisk.store(item.checkDisk);
            forceToDiskBool.store(item.forceToDiskBool);
            splitFileAfterWriteBool.store(item.splitFileAfterWriteBool);
            alreadyReleased = true;

            // Copying this object disconnects it from the ring, so the
            // sequence used to obtain it is irrelevant. Forget about sequenceObj.

            // Need to copy construct record and them make shared pointer out of it
            record = std::make_shared<RecordOutput>(*(item.record.get()));
        }
    }


    ///**
    // * Assignment operator.
    // * @param other right side object.
    // * @return left side object.
    // */
    //RecordRingItem & RecordRingItem::operator=(const RecordRingItem& other) {
    //
    //    // Avoid self assignment ...
    //    if (this != &other) {
    //        id = other.id;
    //        order = other.order;
    //        sequence = other.sequence;
    //        lastItem.store(other.lastItem);
    //        checkDisk.store(other.checkDisk);
    //        forceToDiskBool.store(other.forceToDiskBool);
    //        splitFileAfterWriteBool.store(other.splitFileAfterWriteBool);
    //        alreadyReleased = other.alreadyReleased;
    //
    //        // Need to copy construct record & make shared pointer out of it
    //        record = std::make_shared<RecordOutput>(*(item.record.get()));
    //    }
    //    return *this;
    //}


    /** Method to reset this item each time it is retrieved from the supply. */
    void RecordRingItem::reset() {
        record->reset();
        // id is not automatically reset, use setter
        sequence = 0L;
        sequenceObj = nullptr;
        lastItem.store(false);
        checkDisk.store(false);
        forceToDiskBool.store(false);
        splitFileAfterWriteBool.store(false);
        alreadyReleased = false;
    }


    /**
     * Get the contained record. Record is reset.
     * @return contained record.
     */
    std::shared_ptr<RecordOutput> & RecordRingItem::getRecord() {return record;}


    /**
     * Get the byte order used to build record.
     * @return byte order used to build record.
     */
    ByteOrder & RecordRingItem::getOrder() {return order;}


    /**
     * Get the sequence at which this object was taken from ring by one of the "get" calls.
     * @return sequence at which this object was taken from ring by one of the "get" calls.
     */
    int64_t RecordRingItem::getSequence() const {return sequence;}


    /**
     * Get the Sequence object allowing ring consumer to get/release this item.
     * @return Sequence object allowing ring consumer to get/release this item.
     */
    std::shared_ptr<Disruptor::ISequence> & RecordRingItem::getSequenceObj() {return sequenceObj;}


    /**
     * Set the sequence of an item obtained through {@link RecordSupply#get()}.
     * @param seq sequence used to get item.
     */
    void RecordRingItem::fromProducer(int64_t seq) {sequence = seq;}


    /**
     * Set the sequence of an item obtained through {@link RecordSupply#getToCompress(int)}.
     * @param seq sequence used to get item.
     * @param seqObj sequence object used to get/release item.
     */
    void RecordRingItem::fromConsumer(int64_t seq, std::shared_ptr<Disruptor::ISequence> & seqObj) {
        sequence = seq;
        sequenceObj = seqObj;
    }


    /**
     * Get whether a file writer splits the file after writing this record.
     * @return true if file writer splits the file after writing this record.
     */
    bool RecordRingItem::splitFileAfterWrite() {return splitFileAfterWriteBool.load();}


    /**
     * Set whether a file writer splits the file after writing this record.
     * @param split if true, file writer splits the file after writing this record,
     *              else false.
     */
    void RecordRingItem::splitFileAfterWrite(bool split) {splitFileAfterWriteBool = split;}


    /**
     * Get whether a file writer forces this record to be physically written to disk.
     * @return true if file writer forces this record to be physically written to disk.
     */
    bool RecordRingItem::forceToDisk() {return forceToDiskBool.load();}


    /**
     * Set whether a file writer forces this record to be physically written to disk.
     * @param force if true, file writer forces this record to be physically written
     *              to disk, else false.
     */
    void RecordRingItem::forceToDisk(bool force) {forceToDiskBool = force;}


    /**
     * Get whether there is not enough free space on the disk partition for the
     * next, complete file to be written, resulting in not creating or writing to file.
     * @return true if there is not enough free space on the disk partition for the
     *         next, complete file to be written, resulting in not creating or writing to file.
     */
    bool RecordRingItem::isCheckDisk() {return checkDisk.load();}


    /**
     * Set whether there is not enough free space on the disk partition for the
     * next, complete file to be written, resulting in not creating or writing to file.
     * @param check  if true, there is not enough free space on the disk partition for the
     *               next, complete file to be written, resulting in not creating or writing to file.
     */
    void RecordRingItem::setCheckDisk(bool check) {checkDisk = check;}


    /**
     * Get whether this is the last item in the supply to be used.
     * @return true this is the last item in the supply to be used.
     */
    bool RecordRingItem::isLastItem() {return lastItem.load();}


    /**
     * Set whether this is the last item in the supply to be used.
     * Used in WriterMT when closing.
     * @param last if true, this is the last item in the supply to be used.
     */
    void RecordRingItem::setLastItem(bool last) {lastItem = last;}


    /**
     * Has this item already been released by the RecordSupply?
     * @return true if item already been released by the RecordSupply.
     */
    bool RecordRingItem::isAlreadyReleased() const {return alreadyReleased;}


    /**
     * Set whether this item has already been released by the RecordSupply.
     * @param released true if this item has already been released by the RecordSupply,
     *                 else false.
     */
    void RecordRingItem::setAlreadyReleased(bool released) {alreadyReleased = released;}


    /**
     *  Get item's id. Id is 0 if unused.
     * @return id number.
     */
    uint64_t RecordRingItem::getId() const {return id;}


    /**
     * Set this item's id number.
     * @param id id number.
     */
    void RecordRingItem::setId(uint64_t idVal) {id = idVal;}

}
