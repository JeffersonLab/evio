//
// Created by timmer on 4/9/19.
//

#include "RecordOutput.h"



/** Default, no-arg constructor. Little endian. LZ4 compression. */
RecordOutput::RecordOutput() {

    header = RecordHeader();
    header.setCompressionType(Compressor::LZ4);

    recordIndex = ByteBuffer(MAX_EVENT_COUNT * 4);
    recordIndex.order(byteOrder);

    allocate();
}


/**
 * Constructor with arguments.
 * @param order byte order of built record byte arrays.
 * @param maxEventCount max number of events this record can hold.
 *                      Value <= O means use default (1M).
 * @param maxBufferSize max number of uncompressed data bytes this record can hold.
 *                      Value of < 8MB results in default of 8MB.
 * @param compressionType type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip).
 * @param hType           type of record header to use.
 */
RecordOutput::RecordOutput(const ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize,
                           Compressor::CompressionType compressionType, HeaderType hType) {
    //dataCompressor = Compressor::getInstance();
    try {
        if (hType.isEvioFileHeader()) {
            hType = HeaderType::EVIO_RECORD;
        }
        else if (hType.isHipoFileHeader()) {
            hType = HeaderType::HIPO_RECORD;
        }
        header = RecordHeader(hType);
    }
    catch (HipoException & e) {/* never happen */}

    header.setCompressionType(compressionType);
    byteOrder = order;

    if (maxEventCount > 0) {
        MAX_EVENT_COUNT = maxEventCount;
    }

    if (maxBufferSize > MAX_BUFFER_SIZE) {
        MAX_BUFFER_SIZE = maxBufferSize;
        // If it's at least 10% bigger we'll be OK.
        // Done so that if we "compress" random data,
        // we have a place to put its larger size.
        RECORD_BUFFER_SIZE = (int) (1.1 * MAX_BUFFER_SIZE);
    }

    recordIndex = ByteBuffer(MAX_EVENT_COUNT * 4);
    recordIndex.order(byteOrder);

    allocate();
}

/**
 * Constructor with arguments.
 * @param buffer buffer in which to put constructed (& compressed) binary record.
 *               Must have position and limit set to accept new data.
 * @param maxEventCount max number of events this record can hold.
 *                      Value <= O means use default (1M).
 * @param compressionType type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip).
 * @param hType           type of record header to use.
 */
RecordOutput::RecordOutput(ByteBuffer & buffer, uint32_t maxEventCount,
                           Compressor::CompressionType compressionType, HeaderType & hType) {

    //dataCompressor = Compressor.getInstance();
    try {
        if (hType.isEvioFileHeader()) {
            hType = HeaderType::EVIO_RECORD;
        }
        else if (hType.isHipoFileHeader()) {
            hType = HeaderType::HIPO_RECORD;
        }
        header = RecordHeader(hType);
    }
    catch (HipoException & e) {/* never happen */}

    header.setCompressionType(compressionType);
    userProvidedBuffer = true;
    recordBinary = buffer;
    byteOrder = buffer.order();

    // Start writing at startingPosition, but allow writing
    // to extend to the full buffer capacity and not be limited
    // to only the limit. This is done as we may be adding records
    // to already existing records, but we most likely will not be
    // trying to fit a record in between 2 existing records.
    startingPosition = buffer.position();
    userBufferSize = buffer.capacity() - startingPosition;
    buffer.limit(buffer.capacity());

    if (maxEventCount > 0) {
        MAX_EVENT_COUNT = maxEventCount;
    }

    // Set things so user buffer is 10% bigger
    MAX_BUFFER_SIZE = (int) (0.91*userBufferSize);
    // Not used with user provided buffer,
    // but change it anyway for use in copy(RecordOutput)
    RECORD_BUFFER_SIZE = userBufferSize;

    recordIndex = ByteBuffer(MAX_EVENT_COUNT * 4);
    recordIndex.order(byteOrder);

    allocate();
}


/**
 * Copy constructor.
 * @param rec RecordOutput to copy.
 * @throws HipoException if trying to copy bigger record and
 *                       internal buffer was provided by user.
 */
RecordOutput::RecordOutput(const RecordOutput & rec) {

    // Copy primitive types & immutable objects
    eventCount = rec.eventCount;
    indexSize  = rec.indexSize;
    eventSize  = rec.eventSize;
    byteOrder  = rec.byteOrder;
    startingPosition = rec.startingPosition;

    // Copy construct header
    header = RecordHeader(rec.header);

    // For now, we're not going to use a RecordSupply class (from Java).
    // We're just going to use queues instead, thus we don't have to
    // worry about trying to leave MAX_EVENT_COUNT as is so RecordSupply
    // has consistent behavior.

    // Choose the larger of rec's or this object's buffer sizes
    if (rec.MAX_BUFFER_SIZE > MAX_BUFFER_SIZE ||
        rec.RECORD_BUFFER_SIZE > RECORD_BUFFER_SIZE) {

        MAX_BUFFER_SIZE = rec.MAX_BUFFER_SIZE;
        RECORD_BUFFER_SIZE = rec.RECORD_BUFFER_SIZE;

        // Reallocate memory
        if (!userProvidedBuffer) {
            recordBinary = ByteBuffer(RECORD_BUFFER_SIZE);
            recordBinary.order(byteOrder);
        }
        else {
            // If this record has a user-provided recordBinary buffer,
            // then the user is expecting data to be built into that same buffer.
            // If the data to be copied is larger than can be contained by the
            // user-provided buffer, then we throw an exception.
            throw HipoException("trying to copy bigger record which may not fit into buffer provided by user");
        }

        recordEvents = ByteBuffer(MAX_BUFFER_SIZE);
        recordEvents.order(byteOrder);

        recordData = ByteBuffer(MAX_BUFFER_SIZE);
        recordData.order(byteOrder);
    }

    if (rec.eventCount > MAX_EVENT_COUNT) {
        MAX_EVENT_COUNT = rec.eventCount;
        recordIndex = ByteBuffer(MAX_EVENT_COUNT*4);
        recordIndex.order(byteOrder);
    }

    // Copy data (recordData is just a temporary holding buffer and does NOT need to be copied)
    std::memcpy((void *)recordIndex.array(),  (const void *)rec.recordIndex.array(),  indexSize);
    std::memcpy((void *)recordEvents.array(), (const void *)rec.recordEvents.array(), eventSize);
    std::memcpy((void *)recordBinary.array(), (const void *)rec.recordBinary.array(),
                        rec.recordBinary.limit());

    // Copy buffer limits & positions:
    recordBinary.limit(rec.recordBinary.limit()).position(rec.recordBinary.position());
    recordEvents.limit(rec.recordEvents.limit()).position(rec.recordEvents.position());
    recordIndex.limit(rec.recordIndex.limit()).position(rec.recordIndex.position());
}


/**
 * Move constructor.
 * @param srcBuf ByteBuffer to move.
 */
RecordOutput::RecordOutput(RecordOutput && rec) noexcept {
    // Use code below in move assignment operator
    *this = std::move(rec);
}


/**
 * Move assignment operator.
 * @param other right side object.
 * @return left side object.
 */
RecordOutput & RecordOutput::operator=(RecordOutput&& other) noexcept {

    // Avoid self assignment ...
    if (this != &other) {
        // Copy primitive types & immutable objects
        eventCount = other.eventCount;
        indexSize  = other.indexSize;
        eventSize  = other.eventSize;
        byteOrder  = other.byteOrder;

        MAX_EVENT_COUNT    = other.eventCount;
        MAX_BUFFER_SIZE    = other.MAX_BUFFER_SIZE;
        RECORD_BUFFER_SIZE = other.RECORD_BUFFER_SIZE;

        userBufferSize     = other.userBufferSize;
        startingPosition   = other.startingPosition;
        userProvidedBuffer = other.userProvidedBuffer;

        // Copy construct header (nothing needs moving)
        header = RecordHeader(other.header);

        // Move all the buffers.
        // Copies everything except shared pointer to buffer which gets moved
        recordBinary = std::move(other.recordBinary);
        recordEvents = std::move(other.recordEvents);
        recordIndex  = std::move(other.recordIndex);
        recordData   = std::move(other.recordData);
    }

    return *this;
}


/**
 * Assignment operator.
 * @param other right side object.
 * @return left side object.
 * @throws HipoException if trying to copy bigger record and
 *                       internal buffer was provided by user.
 */
RecordOutput & RecordOutput::operator=(const RecordOutput& other) {

    // Avoid self assignment ...
    if (this != &other) {

        // Copy primitive types & immutable objects
        eventCount = other.eventCount;
        indexSize  = other.indexSize;
        eventSize  = other.eventSize;
        byteOrder  = other.byteOrder;
        startingPosition = other.startingPosition;

        // Copy construct header
        header = RecordHeader(other.header);

        // For now, we're not going to use a RecordSupply class (from Java).
        // We're just going to use queues instead, thus we don't have to
        // worry about trying to leave MAX_EVENT_COUNT as is so RecordSupply
        // has consistent behavior.

        // Choose the larger of rec's or this object's buffer sizes
        if (other.MAX_BUFFER_SIZE > MAX_BUFFER_SIZE ||
            other.RECORD_BUFFER_SIZE > RECORD_BUFFER_SIZE) {

            MAX_BUFFER_SIZE = other.MAX_BUFFER_SIZE;
            RECORD_BUFFER_SIZE = other.RECORD_BUFFER_SIZE;

            // Reallocate memory
            if (!userProvidedBuffer) {
                recordBinary = ByteBuffer(RECORD_BUFFER_SIZE);
                recordBinary.order(byteOrder);
            }
            else {
                // If this record has a user-provided recordBinary buffer,
                // then the user is expecting data to be built into that same buffer.
                // If the data to be copied is larger than can be contained by the
                // user-provided buffer, then we throw an exception.
                throw HipoException("trying to copy bigger record which may not fit into buffer provided by user");
            }

            recordEvents = ByteBuffer(MAX_BUFFER_SIZE);
            recordEvents.order(byteOrder);

            recordData = ByteBuffer(MAX_BUFFER_SIZE);
            recordData.order(byteOrder);
        }

        if (other.eventCount > MAX_EVENT_COUNT) {
            MAX_EVENT_COUNT = other.eventCount;
            recordIndex = ByteBuffer(MAX_EVENT_COUNT*4);
            recordIndex.order(byteOrder);
        }

        // Copy data (recordData is just a temporary holding buffer and does NOT need to be copied)
        std::memcpy((void *)recordIndex.array(),  (const void *)other.recordIndex.array(),  indexSize);
        std::memcpy((void *)recordEvents.array(), (const void *)other.recordEvents.array(), eventSize);
        std::memcpy((void *)recordBinary.array(), (const void *)other.recordBinary.array(),
                    other.recordBinary.limit());

        // Copy buffer limits & positions:
        recordBinary.limit(other.recordBinary.limit()).position(other.recordBinary.position());
        recordEvents.limit(other.recordEvents.limit()).position(other.recordEvents.position());
        recordIndex.limit(other.recordIndex.limit()).position(other.recordIndex.position());
    }

    return *this;
}


/**
 * Reset internal buffers and set the buffer in which to build this record.
 * The given buffer should be made ready to receive new data by setting its
 * position and limit properly. Its byte order is set to the same as this writer's.
 * The argument ByteBuffer can be retrieved by calling {@link #getBinaryBuffer()}.
 * @param buf buffer in which to build record.
 */
void RecordOutput::setBuffer(ByteBuffer & buf) {
    if (buf.order() != byteOrder) {
        cout << "setBuffer(): warning, changing buffer's byte order!" << endl;
    }

    recordBinary = buf;
    recordBinary.order(byteOrder);
    userProvidedBuffer = true;

    startingPosition = buf.position();
    userBufferSize = buf.capacity() - startingPosition;
    buf.limit(buf.capacity());

    // Only allocate memory if current buffers are too small
    if (userBufferSize > RECORD_BUFFER_SIZE) {
        MAX_BUFFER_SIZE = (int) (0.91*userBufferSize);
        RECORD_BUFFER_SIZE = userBufferSize;
        allocate();
    }
    else {
        MAX_BUFFER_SIZE = (int) (0.91*userBufferSize);
        RECORD_BUFFER_SIZE = userBufferSize;
    }

    reset();
}


/**
 * Get the number of initially available bytes to be written into in the user-given buffer,
 * that goes from position to limit. The user-given buffer is referenced by recordBinary.
 * @return umber of initially available bytes to be written into in the user-given buffer.
 */
int RecordOutput::getUserBufferSize() const {return userBufferSize;}

/**
 * Get the current uncompressed size of the record in bytes.
 * This does NOT count any user header.
 * @return current uncompressed size of the record in bytes.
 */
int RecordOutput::getUncompressedSize() const {
    return eventSize + indexSize + RecordHeader::HEADER_SIZE_BYTES;
}

/**
 * Get the capacity of the internal buffer in bytes.
 * This is the upper limit of the memory needed to store this
 * uncompressed record.
 * @return capacity of the internal buffer in bytes.
 */
int RecordOutput::getInternalBufferCapacity() const {return MAX_BUFFER_SIZE;}

/**
 * Get the general header of this record.
 * @return general header of this record.
 */
RecordHeader & RecordOutput::getHeader() {return header;}

/**
 * Get the number of events written so far into the buffer
 * @return number of events written so far into the buffer
 */
int RecordOutput::getEventCount() const {return eventCount;}

/**
 * Get the internal ByteBuffer used to construct binary representation of this record.
 * @return internal ByteBuffer used to construct binary representation of this record.
 */
ByteBuffer & RecordOutput::getBinaryBuffer() {return recordBinary;}

/**
 * Was the internal buffer provided by the user?
 * @return true if internal buffer provided by user.
 */
bool RecordOutput::hasUserProvidedBuffer() const {return userProvidedBuffer;}

/**
 * Get the byte order of the record to be built.
 * @return byte order of the record to be built.
 */
const ByteOrder & RecordOutput::getByteOrder() const {return byteOrder;}

/**
 * Set the byte order of the record to be built.
 * Only use this internally to the package.
 * @param order byte order of the record to be built.
 */
void RecordOutput::setByteOrder(const ByteOrder & order) {byteOrder = order;}


/**
 * Is there room in this record's memory for an additional event
 * of the given length in bytes (length NOT including accompanying index).
 * @param length length of data to add in bytes
 * @return {@code true} if room in record, else {@code false}.
 */
bool RecordOutput::roomForEvent(uint32_t length) const {
    // Account for this record's header including index
    return ((indexSize + 4 + eventSize + RecordHeader::HEADER_SIZE_BYTES + length) <= MAX_BUFFER_SIZE);
}

/**
 * Does adding one more event exceed the event count limit?
 * @return {@code true} if one more event exceeds count limit, else {@code false}.
 */
bool RecordOutput::oneTooMany() const {return (eventCount + 1 > MAX_EVENT_COUNT);}

//---------------------
// private
//---------------------

/** Allocates all buffers for constructing the record stream. */
void RecordOutput::allocate() {

//    if (recordIndex == null) {
//        // This only needs to be done once
//        recordIndex = ByteBuffer(MAX_EVENT_COUNT * 4);
//        recordIndex.order(byteOrder);
//    }
//
    recordEvents = ByteBuffer(MAX_BUFFER_SIZE);
    recordEvents.order(byteOrder);

    // Making this a direct buffer slows it down by 6%
    recordData = ByteBuffer(MAX_BUFFER_SIZE);
    recordData.order(byteOrder);

    if (!userProvidedBuffer) {
        // Trying to compress random data will expand it, so create a cushion.
        // Using a direct buffer takes 2/3 the time of an array-backed buffer
        //recordBinary = ByteBuffer.allocateDirect(RECORD_BUFFER_SIZE);
        recordBinary = ByteBuffer(RECORD_BUFFER_SIZE);
        recordBinary.order(byteOrder);
    }
}

/**
 * Is another event of the given length allowed into this record's memory?
 * It may not be allowed if its exceeds the memory or count limit.
 * @param length length of event to add in bytes
 * @return {@code true} if allowed into record, else {@code false}.
 */
bool RecordOutput::allowedIntoRecord(uint32_t length) {
    return (eventCount < 1 && !roomForEvent(length));
}


//------------------------


///**
// * Adds an event's ByteBuffer into the record.
// * If a single event is too large for the internal buffers,
// * more memory is allocated.
// * On the other hand, if the buffer was provided by the user,
// * then obviously the buffer cannot be expanded and false is returned.<p>
// * <b>The byte order of event must match the byte order given in constructor!</b>
// *
// *
// * @param event  event's byte array.
// * @return true if event was added; false if the event was not added because the
// *         count limit would be exceeded or the buffer is full and cannot be
// *         expanded since it's user-provided.
// */
//bool RecordOutput::addEvent(uint8_t event[]) {
//    return addEvent(event, 0, event.length, 0);
//}

/**
 * Adds an event's ByteBuffer into the record.
 * If a single event is too large for the internal buffers,
 * more memory is allocated.
 * On the other hand, if the buffer was provided by the user,
 * then obviously the buffer cannot be expanded and false is returned.<p>
 * <b>The byte order of event must match the byte order given in constructor!</b>
 *
 *
 * @param event     event's byte array.
 * @param offset    offset into event byte array from which to begin reading.
 * @param eventLen  number of bytes from byte array to add.
 * @return true if event was added; false if the event was not added because the
 *         count limit would be exceeded or the buffer is full and cannot be
 *         expanded since it's user-provided.
 */
bool RecordOutput::addEvent(const uint8_t* event, size_t offset, uint32_t eventLen) {
    return addEvent(event, offset, eventLen, 0);
}

/**
 * Adds an event's ByteBuffer into the record.
 * Can specify the length of additional data to follow the event
 * (such as an evio trailer record) to see if by adding this event
 * everything will fit in the available memory.<p>
 * If a single event is too large for the internal buffers,
 * more memory is allocated.
 * On the other hand, if the buffer was provided by the user,
 * then obviously the buffer cannot be expanded and false is returned.<p>
 * <b>The byte order of event must match the byte order given in constructor!</b>
 *
 *
 * @param event        event's byte array.
 * @param offset       offset into event byte array from which to begin reading.
 * @param eventLen     number of bytes from byte array to add.
 * @param extraDataLen additional data bytes to follow event (e.g. trailer length).
 * @return true if event was added; false if the event was not added because the
 *         count limit would be exceeded or the buffer is full and cannot be
 *         expanded since it's user-provided.
 */
bool RecordOutput::addEvent(const uint8_t* event, size_t offset, uint32_t eventLen, uint32_t extraDataLen) {

    // If we receive a single event larger than our memory, we must accommodate this
    // by increasing our internal buffer size(s). Cannot simply refuse to write an
    // event during event building for example.
    if (eventCount < 1 && !roomForEvent(eventLen + extraDataLen)) {
        if (userProvidedBuffer) {
            return false;
        }

        // Allocate roughly what we need + 1MB
        MAX_BUFFER_SIZE = eventLen + ONE_MEG;
        RECORD_BUFFER_SIZE = MAX_BUFFER_SIZE + ONE_MEG;
        allocate();
        // This does NOT reset record type, compression type, or byte order
        reset();
    }

    if (oneTooMany() || !roomForEvent(eventLen)) {
        return false;
    }

    // Where do we start writing in buffer?
    size_t pos = recordEvents.position();
    // Add event data

    std::memcpy((void *)(recordEvents.array() + pos), (const void *)(event + offset), eventLen);
    //System.arraycopy(event, position, recordEvents.array(), pos, eventLen);

    // Make sure we write in the correct position next time
    recordEvents.position(pos + eventLen);
    // Same as above, but above method is a lot faster:
    //recordEvents.put(event, position, length);
    eventSize += eventLen;

    // Add 1 more index
    recordIndex.putInt(indexSize, eventLen);
    indexSize += 4;

    eventCount++;

    return true;
}

/**
 * Adds an event's ByteBuffer into the record.
 * If a single event is too large for the internal buffers,
 * more memory is allocated.
 * On the other hand, if the buffer was provided by the user,
 * then obviously the buffer cannot be expanded and false is returned.<p>
 * <b>The byte order of event must match the byte order given in constructor!</b>
 *
 * @param event  event's ByteBuffer object.
 * @return true if event was added; false if the buffer is full,
 *         the event count limit is exceeded, or this single event cannot fit into
 *         the user-provided buffer.
 */
bool RecordOutput::addEvent(const ByteBuffer & event) {
    return addEvent(event, 0);
}

/**
 * Adds an event's ByteBuffer into the record.
 * Can specify the length of additional data to follow the event
 * (such as an evio trailer record) to see if by adding this event
 * everything will fit in the available memory.<p>
 * If a single event is too large for the internal buffers,
 * more memory is allocated.
 * On the other hand, if the buffer was provided by the user,
 * then obviously the buffer cannot be expanded and false is returned.<p>
 * <b>The byte order of event must match the byte order given in constructor!</b>
 *
 * @param event        event's ByteBuffer object.
 * @param extraDataLen additional data bytes to follow event (e.g. trailer length).
 * @return true if event was added; false if the event was not added because the
 *         count limit would be exceeded or the buffer is full and cannot be
 *         expanded since it's user-provided.
 */
bool RecordOutput::addEvent(const ByteBuffer & event, uint32_t extraDataLen) {

    int eventLen = event.remaining();

    if (eventCount < 1 && !roomForEvent(eventLen + extraDataLen)) {
        if (userProvidedBuffer) {
            return false;
        }

        MAX_BUFFER_SIZE = eventLen + ONE_MEG;
        RECORD_BUFFER_SIZE = MAX_BUFFER_SIZE + ONE_MEG;
        allocate();
        reset();
    }

    if (oneTooMany() || !roomForEvent(eventLen)) {
        return false;
    }

    if (event.hasArray()) {
        // recordEvents backing array's offset = 0
        size_t pos = recordEvents.position();

        std::memcpy((void *)(recordEvents.array() + pos),
                     (const void *)(event.array() + event.arrayOffset() + event.position()), eventLen);
//        System.arraycopy(event.array(),
//                         event.arrayOffset() + event.position(),
//                         recordEvents.array(), pos, eventLen);

        recordEvents.position(pos + eventLen);
    }
    else {
        recordEvents.put(event);
    }

    eventSize += eventLen;
    recordIndex.putInt(indexSize, eventLen);
    indexSize += 4;
    eventCount++;

    return true;
}


//
///**
// * Adds an event's ByteBuffer into the record.
// * If a single event is too large for the internal buffers,
// * more memory is allocated.
// * On the other hand, if the buffer was provided by the user,
// * then obviously the buffer cannot be expanded and false is returned.<p>
// * <b>The byte order of event must match the byte order given in constructor!</b>
// * This method is not thread-safe with respect to the node as it's backing
// * ByteBuffer's limit and position may be concurrently changed.
// *
// * @param node         event's EvioNode object
// * @return true if event was added; false if the event was not added because the
// *         count limit would be exceeded or the buffer is full and cannot be
// *         expanded since it's user-provided.
// * @throws HipoException if node does not correspond to a bank.
// */
//bool RecordOutput::addEvent(EvioNode & node) {
//    return addEvent(node, 0);
//}
//
///**
// * Adds an event's ByteBuffer into the record.
// * Can specify the length of additional data to follow the event
// * (such as an evio trailer record) to see if by adding this event
// * everything will fit in the available memory.<p>
// * If a single event is too large for the internal buffers,
// * more memory is allocated.
// * On the other hand, if the buffer was provided by the user,
// * then obviously the buffer cannot be expanded and false is returned.<p>
// * <b>The byte order of event must match the byte order given in constructor!</b>
// * This method is not thread-safe with respect to the node as it's backing
// * ByteBuffer's limit and position may be concurrently changed.
// *
// * @param node         event's EvioNode object
// * @param extraDataLen additional data bytes to follow event (e.g. trailer length).
// * @return true if event was added; false if the event was not added because the
// *         count limit would be exceeded or the buffer is full and cannot be
// *         expanded since it's user-provided.
// * @throws HipoException if node does not correspond to a bank.
// */
//bool RecordOutput::addEvent(EvioNode & node, uint32_t extraDataLen) {
//
//    int eventLen = node.getTotalBytes();
//
//    if (!node.getTypeObj().isBank()) {
//        throw HipoException("node does not represent a bank");
//    }
//
//    if (eventCount < 1 && !roomForEvent(eventLen + extraDataLen)) {
//        if (userProvidedBuffer) {
//            return false;
//        }
//
//        MAX_BUFFER_SIZE = eventLen + ONE_MEG;
//        RECORD_BUFFER_SIZE = MAX_BUFFER_SIZE + ONE_MEG;
//        allocate();
//        reset();
//    }
//
//    if (oneTooMany() || !roomForEvent(eventLen)) {
//        return false;
//    }
//
//    ByteBuffer buf = node.getStructureBuffer(false);
//    if (buf.hasArray()) {
//        size_t pos = recordEvents.position();
//
//        std::memcpy((void *)(recordEvents.array() + pos),
//                       (const void *)(buf.array() + buf.arrayOffset() + buf.position()),
//                    eventLen);
////        System.arraycopy(buf.array(),
////                         buf.arrayOffset() + buf.position(),
////                         recordEvents.array(), pos, eventLen);
//
//        recordEvents.position(pos + eventLen);
//    }
//    else {
//        recordEvents.put(buf);
//    }
//
//    eventSize += eventLen;
//    recordIndex.putInt(indexSize, eventLen);
//    indexSize += 4;
//    eventCount++;
//
//    return true;
//}
//
///**
// * Adds an event's ByteBuffer into the record.
// * If a single event is too large for the internal buffers,
// * more memory is allocated.
// * On the other hand, if the buffer was provided by the user,
// * then obviously the buffer cannot be expanded and false is returned.<p>
// * <b>The byte order of event must match the byte order given in constructor!</b>
// *
// * @param event event's EvioBank object.
// * @return true if event was added; false if the event was not added because the
// *         count limit would be exceeded or the buffer is full and cannot be
// *         expanded since it's user-provided.
// */
//bool RecordOutput::addEvent(EvioBank & event) {
//    return addEvent(event, 0);
//}
//
///**
// * Adds an event's ByteBuffer into the record.
// * Can specify the length of additional data to follow the event
// * (such as an evio trailer record) to see if by adding this event
// * everything will fit in the available memory.<p>
// * If a single event is too large for the internal buffers,
// * more memory is allocated.
// * On the other hand, if the buffer was provided by the user,
// * then obviously the buffer cannot be expanded and false is returned.<p>
// * <b>The byte order of event must match the byte order given in constructor!</b>
// *
// * @param event        event's EvioBank object.
// * @param extraDataLen additional data bytes to follow event (e.g. trailer length).
// * @return true if event was added; false if the event was not added because the
// *         count limit would be exceeded or the buffer is full and cannot be
// *         expanded since it's user-provided.
// */
//bool RecordOutput::addEvent(EvioBank & event, int extraDataLen) {
//
//    int eventLen = event.getTotalBytes();
//
//    if (eventCount < 1 && !roomForEvent(eventLen + extraDataLen)) {
//        if (userProvidedBuffer) {
//            return false;
//        }
//
//        MAX_BUFFER_SIZE = eventLen + ONE_MEG;
//        RECORD_BUFFER_SIZE = MAX_BUFFER_SIZE + ONE_MEG;
//        allocate();
//        reset();
//    }
//
//    if (oneTooMany() || !roomForEvent(eventLen)) {
//        return false;
//    }
//
//    event.write(recordEvents);
//
//    eventSize += eventLen;
//    recordIndex.putInt(indexSize, eventLen);
//    indexSize += 4;
//    eventCount++;
//
//    return true;
//}



/**
 * Reset internal buffers. The buffer is ready to receive new data.
 * Also resets the header including removing any compression.
 */
void RecordOutput::reset() {
    indexSize  = 0;
    eventSize  = 0;
    eventCount = 0;

    recordData.clear();
    recordIndex.clear();
    recordEvents.clear();
    recordBinary.clear();

    // TODO: This may do way too much! Think about this more.
    header.reset();
}

/**
 * Set the starting position of the user-given buffer being written into.
 * Calling this may be necessary from EventWriter(Unsync) when a common record
 * (dictionary + first event) is written after the constructor for this
 * object has been called, but before any events have been written.
 * This method should <b>not</b> be called in general as it will MESS UP
 * THE WRITING OF DATA!
 * @param pos position in buffer to start writing.
 */
void RecordOutput::setStartingBufferPosition(size_t pos) {
    recordBinary.position(pos);
    startingPosition = pos;
}

/**
 * Builds the record. Compresses data, header is constructed,
 * then header & data written into internal buffer.
 * This method may be called multiple times in succession without
 * any problem.
 */
void RecordOutput::build() {

    // If no events have been added yet, just write a header
    if (eventCount < 1) {
        header.setEntries(0);
        header.setDataLength(0);
        header.setIndexLength(0);
        header.setCompressedDataLength(0);
        header.setLength(RecordHeader::HEADER_SIZE_BYTES);
        recordBinary.position(startingPosition);
        try {
            header.writeHeader(recordBinary);
        }
        catch (HipoException & e) {/* never happen */}
//            cout << "build: buf lim = " << recordBinary.limit() <<
//                    ", cap = " << recordBinary.capacity() << endl;
        recordBinary.limit(RecordHeader::HEADER_SIZE_BYTES);
        return;
    }

    // Position in recordBinary buffer of just past the record header
    size_t recBinPastHdr = startingPosition + RecordHeader::HEADER_SIZE_BYTES;

    // Position in recordBinary buffer's backing array of just past the record header.
    // Usually the same as the corresponding buffer position. But need to
    // account for the user providing a buffer which is mapped on to a bigger backing array.
    // This may happen if the user provides a slice() of another buffer.
    // Right now, slice() is not implemented in C++ so buf.arrayOffset() = 0 and
    // recBinPastHdr = recBinPastHdrAbsolute.
    size_t recBinPastHdrAbsolute = recBinPastHdr + recordBinary.arrayOffset();

    uint32_t compressionType = header.getCompressionType();

    // Write index & event arrays

    // If compressing data ...
    if (compressionType > 0) {
        // Write into a single, temporary buffer
        recordData.position(0);
        recordData.put( recordIndex.array(), 0, indexSize);
        recordData.put(recordEvents.array(), 0, eventSize);
    }
    // If NOT compressing data ...
    else {
//cout << "build: recordBinary len = " << userBufferSize <<
//        ", start pos = " << startingPosition << ", data to write = " <<
//        (RecordHeader::HEADER_SIZE_BYTES + indexSize + eventSize) << endl;

        // Write directly into final buffer, past where header will go
        recordBinary.position(recBinPastHdr);
        recordBinary.put(recordIndex.array(), 0, indexSize);

//cout << "build: recordBinary pos = " << recordBinary.position() <<
//        ", eventSize = " << eventSize << ", recordEvents.array().len = " <<
//        recordEvents.capacity() << endl;

        recordBinary.put(recordEvents.array(), 0, eventSize);

//cout << "build: writing index of size " << indexSize << endl <<
//        "build: events of size " << eventSize << endl;
    }

    // Evio data is padded, but not necessarily all hipo data.
    // Uncompressed data length is NOT padded, but the record length is.
    uint32_t uncompressedDataSize = indexSize + eventSize;
    uint32_t compressedSize = 0;
    uint8_t* gzippedData;

    // Compress that temporary buffer into destination buffer
    // (skipping over where record header will be written).
    try {
        switch (compressionType) {
            case 1:
                // LZ4 fastest compression
                compressedSize = Compressor::getInstance().compressLZ4(
                        recordData.array(), 0, uncompressedDataSize,
                        recordBinary.array(), recBinPastHdrAbsolute,
                        (recordBinary.capacity() - recBinPastHdrAbsolute));

                // Length of compressed data in bytes
                header.setCompressedDataLength(compressedSize);
                // Length of entire record in bytes (don't forget padding!)
                header.setLength(4*header.getCompressedDataLengthWords() +
                                 RecordHeader::HEADER_SIZE_BYTES);
                break;

            case 2:
                // LZ4 highest compression
                compressedSize = Compressor::getInstance().compressLZ4Best(
                        recordData.array(), 0, uncompressedDataSize,
                        recordBinary.array(), recBinPastHdrAbsolute,
                        (recordBinary.capacity() - recBinPastHdrAbsolute));
//
//cout << "Compressing data array from offset = 0, size = " << uncompressedDataSize <<
//         " to output.array offset = " << recBinPastHdrAbsolute << ", compressed size = " <<  compressedSize <<
//         ", available size = " << (recordBinary.capacity() - recBinPastHdrAbsolute) << endl;

//cout << "BEFORE setting header len: comp size = " << header.getCompressedDataLength() <<
//        ", comp words = " << header.getCompressedDataLengthWords() << ", padding = " <<
//        header.getCompressedDataLengthPadding();

                header.setCompressedDataLength(compressedSize);
                header.setLength(4*header.getCompressedDataLengthWords() +
                                 RecordHeader::HEADER_SIZE_BYTES);

//cout << "AFTER setting, read back from header: comp size = " << header.getCompressedDataLength() <<
//        ", comp words = " << header.getCompressedDataLengthWords() << ", padding = " <<
//        header.getCompressedDataLengthPadding() << ", rec len = " << header.getLength() << endl;

                break;

            case 3:
                // GZIP compression
                gzippedData = Compressor::getInstance().compressGZIP(recordData.array(), 0,
                                                                 uncompressedDataSize, &compressedSize);
                recordBinary.position(recBinPastHdr);
                recordBinary.put(gzippedData, 0, compressedSize);
                delete[] gzippedData;
                header.setCompressedDataLength(compressedSize);
                header.setLength(4*header.getCompressedDataLengthWords() +
                                 RecordHeader::HEADER_SIZE_BYTES);
                break;

            case 0:
            default:
                // No compression. The uncompressed data size may not be padded to a 4byte boundary,
                // so make sure that's accounted for here.
                header.setCompressedDataLength(0);
                int words = uncompressedDataSize/4;
                if (uncompressedDataSize % 4 != 0) words++;
                header.setLength(words*4 + RecordHeader::HEADER_SIZE_BYTES);
        }
    }
    catch (HipoException & e) {/* should not happen */}

    // Set the rest of the header values
    header.setEntries(eventCount);
    header.setDataLength(eventSize);
    header.setIndexLength(indexSize);

//    cout << " COMPRESSED = " << compressedSize << "  events size = " << eventSize << "  type = " <<
//            compressionType << "  uncompressed = " << uncompressedDataSize <<
//            " record bytes = " << header.getLength() << endl << endl;

    // Go back and write header into destination buffer
    try {
        // Does NOT change recordBinary pos or lim
        header.writeHeader(recordBinary, startingPosition);
    }
    catch (HipoException & e) {/* never happen */}

    // Make ready to read
    recordBinary.limit(startingPosition + header.getLength()).position(0);
}


/**
 * Builds the record. Compresses data, header is constructed,
 * then header & data written into internal buffer.
 * If user header is not padded to 4-byte boundary, it's done here.
 * This method may be called multiple times in succession without
 * any problem.
 *
 * @param userHeader user's ByteBuffer which must be READY-TO-READ!
 */
void RecordOutput::build(ByteBuffer & userHeader) {

    // How much user-header data do we actually have (limit - position) ?
    size_t userHeaderSize = userHeader.remaining();

    if (userHeaderSize == 0) {
        build();
        return;
    }

//    cout << "  INDEX = 0 " << indexSize << "  " << (indexSize + userHeaderSize) <<
//            "  DIFF " << userHeaderSize << endl;

    // Position in recordBinary buffer of just past the record header
    size_t recBinPastHdr = startingPosition + RecordHeader::HEADER_SIZE_BYTES;

    // Position in recordBinary buffer's backing array of just past the record header.
    // Usually the same as the corresponding buffer position. But need to
    // account for the user providing a buffer which is mapped on to a bigger backing array.
    // This may happen if the user provides a slice() of another buffer.
    // Right now, slice() is not implemented in C++ so buf.arrayOffset() = 0 and
    // recBinPastHdr = recBinPastHdrAbsolute.
    size_t recBinPastHdrAbsolute = recBinPastHdr + recordBinary.arrayOffset();

    uint32_t compressionType = header.getCompressionType();
    uint32_t uncompressedDataSize = indexSize;

    // If compressing data ...
    if (compressionType > 0) {
        // Write into a single, temporary buffer the following:

        // 1) uncompressed index array
        recordData.position(0);
        // Note, put() will increment position
        recordData.put(recordIndex.array(), 0, indexSize);

        // 2) uncompressed user header
        recordData.put(userHeader.array(), 0, userHeaderSize);

        // Account for unpadded user header.
        // This will find the user header length in words & account for padding.
        header.setUserHeaderLength(userHeaderSize);
        // Hop over padded user header length
        uncompressedDataSize += 4*header.getUserHeaderLengthWords();
        recordData.position(uncompressedDataSize);

        // 3) uncompressed data array (hipo/evio data is already padded)
        recordData.put(recordEvents.array(), 0, eventSize);
        // May not be padded
        uncompressedDataSize += eventSize;
    }
    // If NOT compressing data ...
    else {
        // Write directly into final buffer, past where header will go
        recordBinary.position(recBinPastHdr);

        // 1) uncompressed index array
        recordBinary.put(recordIndex.array(), 0, indexSize);

        // 2) uncompressed user header array
        recordBinary.put(userHeader.array(), 0, userHeaderSize);

        header.setUserHeaderLength(userHeaderSize);
        uncompressedDataSize += 4*header.getUserHeaderLengthWords();
        recordBinary.position(recBinPastHdr + uncompressedDataSize);

        // 3) uncompressed data array (hipo/evio data is already padded)
        recordBinary.put(recordEvents.array(), 0, eventSize);
        // May not be padded ...
        uncompressedDataSize += eventSize;
    }

    // Compress that temporary buffer into destination buffer
    // (skipping over where record header will be written).
    uint32_t compressedSize = 0;
    uint8_t* gzippedData;

    try {
        switch (compressionType) {
            case 1:
                // LZ4 fastest compression
                compressedSize = Compressor::getInstance().compressLZ4(
                        recordData.array(), 0, uncompressedDataSize,
                        recordBinary.array(), recBinPastHdrAbsolute,
                        (recordBinary.capacity() - recBinPastHdrAbsolute));

                // Length of compressed data in bytes
                header.setCompressedDataLength(compressedSize);
                // Length of entire record in bytes (don't forget padding!)
                header.setLength(4*header.getCompressedDataLengthWords() +
                                         RecordHeader::HEADER_SIZE_BYTES);
                break;

            case 2:
                // LZ4 highest compression
                compressedSize = Compressor::getInstance().compressLZ4Best(
                        recordData.array(), 0, uncompressedDataSize,
                        recordBinary.array(), recBinPastHdrAbsolute,
                        (recordBinary.capacity() - recBinPastHdrAbsolute));

                header.setCompressedDataLength(compressedSize);
                header.setLength(4*header.getCompressedDataLengthWords() +
                                 RecordHeader::HEADER_SIZE_BYTES);
                break;

            case 3:
                // GZIP compression
                gzippedData = Compressor::getInstance().compressGZIP(recordData.array(), 0,
                                                                     uncompressedDataSize, &compressedSize);
                recordBinary.position(recBinPastHdr);
                recordBinary.put(gzippedData, 0, compressedSize);
                delete[] gzippedData;
                header.setCompressedDataLength(compressedSize);
                header.setLength(4*header.getCompressedDataLengthWords() +
                                 RecordHeader::HEADER_SIZE_BYTES);
                break;

            case 0:
            default:
                // No compression. The uncompressed data size may not be padded to a 4byte boundary,
                // so make sure that's accounted for here.
                header.setCompressedDataLength(0);
                int words = uncompressedDataSize/4;
                if (uncompressedDataSize % 4 != 0) words++;
                header.setLength(words*4 + RecordHeader::HEADER_SIZE_BYTES);
        }
    }
    catch (HipoException & e) {/* should not happen */}

    //cout << " COMPRESSED SIZE = " << compressedSize << endl;

    // Set header values (user header length already set above)
    header.setEntries(eventCount);
    header.setDataLength(eventSize);
    header.setIndexLength(indexSize);

    // Go back and write header into destination buffer
    try {
        header.writeHeader(recordBinary, startingPosition);
    }
    catch (HipoException & e) {/* never happen */}

    // Make ready to read
    recordBinary.limit(startingPosition + header.getLength()).position(0);
}
