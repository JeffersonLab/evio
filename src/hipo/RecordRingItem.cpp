#include <memory>

//
// Created by timmer on 11/5/19.
//

#include "RecordRingItem.h"

//--------------------------------

// STATIC INITIALIZATION

uint64_t RecordRingItem::idValue = 0ULL;


// Set default values for RecordRingItems
int       RecordRingItem::factoryMaxEventCount {0};
int       RecordRingItem::factoryMaxBufferSize {0};
ByteOrder RecordRingItem::factoryByteOrder {ByteOrder::ENDIAN_LITTLE};
Compressor::CompressionType RecordRingItem::factoryCompressionType {Compressor::UNCOMPRESSED};


const std::function< std::shared_ptr<RecordRingItem> () >& RecordRingItem::eventFactory() {
    static std::function< std::shared_ptr<RecordRingItem> () > result([] {
        return std::move(std::make_shared<RecordRingItem>());
    });
    return result;
}


/** Method to set RecordRingItem parameters for objects created by eventFactory. */
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


///**
// * Constructor.
// * @param order         byte order of built record byte arrays.
// * @param maxEventCount max number of events record can hold.
// *                      Value <= O means use default (1M).
// * @param maxBufferSize max number of uncompressed data bytes record can hold.
// *                      Value of < 8MB results in default of 8MB.
// * @param compressionType type of data compression to do.
// */
//RecordRingItem::RecordRingItem(ByteOrder & byteOrder,
//                               uint32_t maxEventCount, uint32_t maxBufferSize,
//                               Compressor::CompressionType & compressionType) :
//        order(byteOrder) {
//
//    std::make_shared<RecordOutput>(order, maxEventCount, maxBufferSize, compressionType);
//
//    id = idValue++;
//}


///**
// * Copy constructor (sort of). Used in EventWriter(Unsync) for when disk is full and a copy
// * of the item to be written is made for later writing. Original item is released
// * so ring can function. Note, not everything is copied (sequenceObj) since in usage,
// * the original item has already been released. Also, {@link #alreadyReleased} is true. <p>
// * <b>Not to be used except internally by evio.</b>
// *
// * @param item ring item to copy.
// */
//RecordRingItem::RecordRingItem(const RecordRingItem & item) : order(item.order) {
//    // Avoid self copy ...
//    if (this != &item) {
//        id = item.id;
//        sequence = item.sequence;
//        lastItem.store(item.lastItem);
//        checkDisk.store(item.checkDisk);
//        forceToDiskBool.store(item.forceToDiskBool);
//        splitFileAfterWriteBool.store(item.splitFileAfterWriteBool);
//        alreadyReleased = true;
//
//        // This does some extra memory allocation we don't need, but good enough for now
//
//        // Copy data to write
//        const ByteBuffer &dataBuf = item.record->getBinaryBuffer();
//        ByteBuffer buf(dataBuf.capacity());
//        std::memcpy(buf.array(),
//                    dataBuf.array() + dataBuf.arrayOffset() + dataBuf.position(),
//                    dataBuf.remaining());
//// TODO: buf must not be on the STACK since it's stored in record !!!!!!!!!!!!!!!!!!!!!!!
////        RecordOutput & record2 = RecordOutput(buf,
////                              item.record.getMaxEventCount(),
////                              item.record.getCompressionType(),
////                              item.record.getHeaderType());
//        // Which ONE? This
//        record = item.record;
//        // or this ? Does buf get copied if given by user??
//        std::make_shared<RecordOutput>(buf,
//                                       item.record->getMaxEventCount(),
//                                       item.record->getCompressionType(),
//                                       item.record->getHeaderType());
//    }
//}


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
//        sequence = other.sequence;
//        lastItem.store(other.lastItem);
//        checkDisk.store(other.checkDisk);
//        forceToDiskBool.store(other.forceToDiskBool);
//        splitFileAfterWriteBool.store(other.splitFileAfterWriteBool);
//        alreadyReleased = other.alreadyReleased;
//
//        // Copy data to write
//        const ByteBuffer & dataBuf = other.record.getBinaryBuffer();
//        ByteBuffer buf(dataBuf.capacity());
//        std::memcpy(buf.array(),
//                    dataBuf.array() + dataBuf.arrayOffset() + dataBuf.position(),
//                    dataBuf.remaining());
//
//        record = RecordOutput(buf,
//                              other.record.getMaxEventCount(),
//                              other.record.getCompressionType(),
//                              other.record.getHeaderType());
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
int64_t RecordRingItem::getSequence() {return sequence;}

/**
 * Get the Sequence object allowing ring consumer to get/release this item.
 * @return Sequence object allowing ring consumer to get/release this item.
 */
shared_ptr<Disruptor::Sequence> & RecordRingItem::getSequenceObj() {return sequenceObj;}

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
void RecordRingItem::fromConsumer(int64_t seq, shared_ptr<Disruptor::Sequence> & seqObj) {
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
bool RecordRingItem::isAlreadyReleased() {return alreadyReleased;}

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
uint64_t RecordRingItem::getId() {return id;}

/**
 * Set this item's id number.
 * @param id id number.
 */
void RecordRingItem::setId(uint64_t idVal) {id = idVal;}
