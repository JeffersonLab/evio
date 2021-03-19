//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "RecordOutput.h"


namespace evio {


    /** Default, no-arg constructor. Little endian. No compression. */
    RecordOutput::RecordOutput() :
            RecordOutput(ByteOrder::ENDIAN_LITTLE, 0, 0,
                         Compressor::UNCOMPRESSED, HeaderType::EVIO_RECORD) {
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
                               Compressor::CompressionType compressionType, HeaderType hType)  {

        try {
            if (hType.isEvioFileHeader()) hType = HeaderType::EVIO_RECORD;
            else if (hType.isHipoFileHeader())  hType = HeaderType::HIPO_RECORD;
            else if (hType == HeaderType::EVIO_TRAILER) hType = HeaderType::EVIO_RECORD;
            else if (hType == HeaderType::HIPO_TRAILER) hType = HeaderType::HIPO_RECORD;
            header = std::make_shared<RecordHeader>(hType);
            header->setCompressionType(compressionType);
        }
        catch (EvioException & e) {/* never happen */}

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

        recordIndex = std::make_shared<ByteBuffer>(MAX_EVENT_COUNT * 4);
        recordIndex->order(byteOrder);

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
    RecordOutput::RecordOutput(std::shared_ptr<ByteBuffer> & buffer, uint32_t maxEventCount,
                               Compressor::CompressionType compressionType, HeaderType hType) {

        try {
            if (hType.isEvioFileHeader()) hType = HeaderType::EVIO_RECORD;
            else if (hType.isHipoFileHeader())  hType = HeaderType::HIPO_RECORD;
            else if (hType == HeaderType::EVIO_TRAILER) hType = HeaderType::EVIO_RECORD;
            else if (hType == HeaderType::HIPO_TRAILER) hType = HeaderType::HIPO_RECORD;
            header = std::make_shared<RecordHeader>(hType);
            header->setCompressionType(compressionType);
        }
        catch (EvioException & e) {/* never happen */}

        userProvidedBuffer = true;

        recordBinary = buffer;
        byteOrder = buffer->order();

        // Start writing at startingPosition, but allow writing
        // to extend to the full buffer capacity and not be limited
        // to only the limit. This is done as we may be adding records
        // to already existing records, but we most likely will not be
        // trying to fit a record in between 2 existing records.
        startingPosition = buffer->position();
        userBufferSize = buffer->capacity() - startingPosition;
        buffer->limit(buffer->capacity());

        if (maxEventCount > 0) {
            MAX_EVENT_COUNT = maxEventCount;
        }

        // Set things so user buffer is 10% bigger
        MAX_BUFFER_SIZE = (int) (0.91*userBufferSize);
        // Not used with user provided buffer,
        // but change it anyway for use in copy(RecordOutput)
        RECORD_BUFFER_SIZE = userBufferSize;

        recordIndex = std::make_shared<ByteBuffer>(MAX_EVENT_COUNT * 4);
        recordIndex->order(byteOrder);

        allocate();
    }


    /**
     * Copy constructor.
     * @param rec RecordOutput to copy.
     * @throws EvioException if trying to copy bigger record and
     *                       internal buffer was provided by user.
     */
    RecordOutput::RecordOutput(const RecordOutput & rec) {
        copy(rec);

        // Copy buffer limits & positions:
        recordBinary->limit(rec.recordBinary->limit()).position(rec.recordBinary->position());
        recordEvents->limit(rec.recordEvents->limit()).position(rec.recordEvents->position());
        recordIndex->limit(rec.recordIndex->limit()).position(rec.recordIndex->position());
    }


    /**
     * Move constructor.
     * @param rec RecordOutput object to move.
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

        // Avoid self assignment
        if (this != &other) {
            // Copy primitive types & immutable objects
            eventCount = other.eventCount;
            indexSize  = other.indexSize;
            eventSize  = other.eventSize;
            byteOrder  = other.byteOrder;

            MAX_EVENT_COUNT    = other.MAX_EVENT_COUNT;
            MAX_BUFFER_SIZE    = other.MAX_BUFFER_SIZE;
            RECORD_BUFFER_SIZE = other.RECORD_BUFFER_SIZE;

            userBufferSize     = other.userBufferSize;
            startingPosition   = other.startingPosition;
            userProvidedBuffer = other.userProvidedBuffer;

            // Copy construct header (nothing needs moving)
            header = std::make_shared<RecordHeader>(*(other.header.get()));

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
     * @throws EvioException if trying to copy bigger record and
     *                       internal buffer was provided by user.
     */
    RecordOutput & RecordOutput::operator=(const RecordOutput& other) {
        copy(other);

        // Copy buffer limits & positions:
        recordBinary->limit(other.recordBinary->limit()).position(other.recordBinary->position());
        recordEvents->limit(other.recordEvents->limit()).position(other.recordEvents->position());
        recordIndex->limit(other.recordIndex->limit()).position(other.recordIndex->position());

        return *this;
    }


    /**
     * Reset internal buffers and set the buffer in which to build this record.
     * The given buffer should be made ready to receive new data by setting its
     * position and limit properly. Its byte order is set to the same as this writer's.
     * The argument ByteBuffer can be retrieved by calling {@link #getBinaryBuffer()}.
     * @param buf buffer in which to build this record.
     */
    void RecordOutput::setBuffer(std::shared_ptr<ByteBuffer> & buf) {
        if (buf->order() != byteOrder) {
            std::cout << "setBuffer(): warning, changing buffer's byte order!" << std::endl;
        }

        recordBinary = buf;
        recordBinary->order(byteOrder);
        userProvidedBuffer = true;

        startingPosition = buf->position();
        userBufferSize = buf->capacity() - startingPosition;
        buf->limit(buf->capacity());

        MAX_BUFFER_SIZE = (int) (0.91*userBufferSize);
        RECORD_BUFFER_SIZE = userBufferSize;

        // Only re-allocate memory if current buffers are too small
        if (userBufferSize > RECORD_BUFFER_SIZE) {
            allocate();
        }

        reset();
    }


    /**
     * Copy the contents of the arg into this object and get data buffer ready for reading.
     * If the arg has more data than will fit, increase buffer sizes.
     * If the arg has more events than our allowed max, increase the max.
     * @param rec object to copy, must be ready to read
     * @throws EvioException if we cannot replace internal buffer if it needs to be
     *                       expanded since it was provided by the user.
     */
    void RecordOutput::transferDataForReading(const RecordOutput & rec) {
        copy(rec);

        // Get buffers ready to read
        recordBinary->limit(rec.recordBinary->limit()).position(0);
        recordEvents->limit(eventSize).position(0);
        recordIndex->limit(indexSize).position(0);
    }


    /**
     * Copy data from arg into this object, but don't set positions/limits of data buffer.
     * Copy all data up to the buffer limit (not capacity).
     * @param rec RecordOutput to copy.
     * @throws EvioException if trying to copy bigger record and
     *                       internal buffer was provided by user.
     */
    void RecordOutput::copy(const RecordOutput & rec) {

        // Avoid self copy
        if (this == &rec) {
            return;
        }

        // Copy primitive types & immutable objects
        eventCount = rec.eventCount;
        indexSize  = rec.indexSize;
        eventSize  = rec.eventSize;
        byteOrder  = rec.byteOrder;
        startingPosition = rec.startingPosition;

        // Copy construct header
        header = std::make_shared<RecordHeader>(*(rec.header.get()));

        // It would be nice to leave MAX_EVENT_COUNT as is so RecordSupply
        // has consistent behavior. But I don't think that's possible if
        // the record output stream being copied is larger. Since the record
        // being copied may not have had build() called, go by the max sizes
        // and not how much data are in the buffers.

        // Choose the larger of rec's or this object's buffer sizes
        if (rec.MAX_BUFFER_SIZE > MAX_BUFFER_SIZE ||
            rec.RECORD_BUFFER_SIZE > RECORD_BUFFER_SIZE) {

            MAX_BUFFER_SIZE = rec.MAX_BUFFER_SIZE;
            RECORD_BUFFER_SIZE = rec.RECORD_BUFFER_SIZE;

            // Reallocate memory
            if (!userProvidedBuffer) {
                recordBinary = std::make_shared<ByteBuffer>(RECORD_BUFFER_SIZE);
                recordBinary->order(byteOrder);
            }
            else {
                // If this record has a user-provided recordBinary buffer,
                // then the user is expecting data to be built into that same buffer.
                // If the data to be copied is larger than can be contained by the
                // user-provided buffer, then we throw an exception.
                throw EvioException("trying to copy bigger record which may not fit into buffer provided by user");
            }

            recordEvents = std::make_shared<ByteBuffer>(MAX_BUFFER_SIZE);
            recordEvents->order(byteOrder);

            recordData = std::make_shared<ByteBuffer>(MAX_BUFFER_SIZE);
            recordData->order(byteOrder);
        }

        if (rec.MAX_EVENT_COUNT > MAX_EVENT_COUNT) {
            MAX_EVENT_COUNT = rec.MAX_EVENT_COUNT;
            recordIndex = std::make_shared<ByteBuffer>(MAX_EVENT_COUNT*4);
            recordIndex->order(byteOrder);
        }

        // Copy data (recordData is just a temporary holding buffer and does NOT need to be copied)
        std::memcpy((void *)recordIndex->array(),  (const void *)rec.recordIndex->array(),  indexSize);
        std::memcpy((void *)recordEvents->array(), (const void *)rec.recordEvents->array(), eventSize);
        std::memcpy((void *)recordBinary->array(), (const void *)rec.recordBinary->array(), rec.recordBinary->limit());

        // Don't set limits or positions of buffers in this method
    }


    /**
     * Get the maximum number of events allowed in this record.
     * @return maximum number of events allowed in this record.
     */
    uint32_t RecordOutput::getMaxEventCount() const {return MAX_EVENT_COUNT;}


    /**
     * Get the number of initially available bytes to be written into in the user-given buffer,
     * that goes from position to limit. The user-given buffer is referenced by recordBinary.
     * @return umber of initially available bytes to be written into in the user-given buffer.
     */
    uint32_t RecordOutput::getUserBufferSize() const {return userBufferSize;}


    /**
     * Get the current uncompressed size of the record in bytes.
     * This does NOT count any user header.
     * @return current uncompressed size of the record in bytes.
     */
    uint32_t RecordOutput::getUncompressedSize() const {
        return eventSize + indexSize + RecordHeader::HEADER_SIZE_BYTES;
    }


    /**
     * Get the capacity of the internal buffer in bytes.
     * This is the upper limit of the memory needed to store this
     * uncompressed record.
     * @return capacity of the internal buffer in bytes.
     */
    uint32_t RecordOutput::getInternalBufferCapacity() const {return MAX_BUFFER_SIZE;}


    /**
     * Get the general header of this record.
     * @return general header of this record.
     */
    std::shared_ptr<RecordHeader> & RecordOutput::getHeader() {return header;}


    /**
     * Get the number of events written so far into the buffer
     * @return number of events written so far into the buffer
     */
    uint32_t RecordOutput::getEventCount() const {return eventCount;}


    /**
     * Get the internal ByteBuffer used to construct binary representation of this record.
     * @return internal ByteBuffer used to construct binary representation of this record.
     */
    const std::shared_ptr<ByteBuffer> RecordOutput::getBinaryBuffer() const {return recordBinary;}


    /**
     * Get the compression type of the contained record.
     * Implemented to allow "const" in {@link RecordRingItem} equal operator
     * and copy constructor.
     * @return compression type of the contained record.
     */
    const Compressor::CompressionType RecordOutput::getCompressionType() const {
        return header->getCompressionType();
    }


    /**
     * Get the header type of the contained record.
     * Implemented to allow "const" in {@link RecordRingItem} equal operator
     * and copy constructor.
     * @return compression type of the contained record.
     */
    const HeaderType RecordOutput::getHeaderType() const {
        return header->getHeaderType();
    }


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


    /** Allocates all buffers for constructing the record stream. */
    void RecordOutput::allocate() {

        recordEvents = std::make_shared<ByteBuffer>(MAX_BUFFER_SIZE);
        recordEvents->order(byteOrder);

        recordData = std::make_shared<ByteBuffer>(MAX_BUFFER_SIZE);
        recordData->order(byteOrder);

        if (!userProvidedBuffer) {
            // Trying to compress random data will expand it, so create a cushion.
            recordBinary = std::make_shared<ByteBuffer>(RECORD_BUFFER_SIZE);
            recordBinary->order(byteOrder);
        }
    }

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


    /**
     * Is another event of the given length allowed into this record's memory?
     * It may not be allowed if its exceeds the memory or count limit.
     * @param length length of event to add in bytes
     * @return {@code true} if allowed into record, else {@code false}.
     */
    bool RecordOutput::allowedIntoRecord(uint32_t length) {
        return (eventCount < 1 && !roomForEvent(length));
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
     * @param eventLen     number of bytes from byte array to add.
     * @param extraDataLen additional data bytes to follow event (e.g. trailer length).
     * @return true if event was added; false if the event was not added because the
     *         count limit would be exceeded or the buffer is full and cannot be
     *         expanded since it's user-provided.
     */
    bool RecordOutput::addEvent(const uint8_t* event, uint32_t eventLen, uint32_t extraDataLen) {

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

        // Add event data.
        // Uses memcpy underneath since, unlike Java, there is no "direct" buffer
        recordEvents->put(event, eventLen);
        eventSize += eventLen;

        // Add 1 more index
        recordIndex->putInt(indexSize, eventLen);
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
     *
     * @param event event's data in vector.
     * @return true if event was added; false if the event was not added because the
     *         count limit would be exceeded or the buffer is full and cannot be
     *         expanded since it's user-provided.
     */
    bool RecordOutput::addEvent(const std::vector<uint8_t> & event) {
        return addEvent(event, 0, event.size(), 0);
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
     * @param event        event's data in vector.
     * @param offset       offset into vector's elements from which to begin reading.
     * @param eventLen     number of bytes from vector to add.
     * @param extraDataLen additional data bytes to follow event (e.g. trailer length).
     * @return true if event was added; false if the event was not added because the
     *         count limit would be exceeded or the buffer is full and cannot be
     *         expanded since it's user-provided.
     */
    bool RecordOutput::addEvent(const std::vector<uint8_t> & event, size_t offset, uint32_t eventLen, uint32_t extraDataLen) {
        return addEvent((reinterpret_cast<const uint8_t*>(event.data())) + offset, eventLen, extraDataLen);
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

        // recordEvents backing array's offset = 0
        size_t pos = recordEvents->position();

//std::cout << "\nRecordOutput::addEvent(buf): write (in recordEvents) to pos = " << pos << std::endl;
        std::memcpy((void *)(recordEvents->array() + pos),
                    reinterpret_cast<void *>(event.array() + event.arrayOffset() + event.position()),
                    eventLen);

        recordEvents->position(pos + eventLen);

        eventSize += eventLen;
        recordIndex->putInt(indexSize, eventLen);
        indexSize += 4;
        eventCount++;

        return true;
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
    bool RecordOutput::addEvent(const std::shared_ptr<ByteBuffer> & event, uint32_t extraDataLen) {
        return addEvent(*(event.get()), extraDataLen);
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
     * This method is not thread-safe with respect to the node as it's backing
     * ByteBuffer's limit and position may be concurrently changed.
     *
     * @param node         event's EvioNode object
     * @param extraDataLen additional data bytes to follow event (e.g. trailer length).
     * @return true if event was added; false if the event was not added because the
     *         count limit would be exceeded or the buffer is full and cannot be
     *         expanded since it's user-provided.
     * @throws EvioException if node does not correspond to a bank.
     */
    bool RecordOutput::addEvent(EvioNode & node, uint32_t extraDataLen) {

        uint32_t eventLen = node.getTotalBytes();

        if (!node.getTypeObj().isBank()) {
            throw EvioException("node does not represent a bank (" + node.getTypeObj().toString() + ")");
        }

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


        ByteBuffer buf(eventLen);
        node.getStructureBuffer(buf, false);
        size_t pos = recordEvents->position();

//std::cout << "\nRecordOutput::addEvent(node): write (in recordEvents)to pos = " << pos << std::endl;
        std::memcpy((void *)(recordEvents->array() + pos),
                    (const void *)(buf.array() + buf.arrayOffset() + buf.position()),
                    eventLen);

        recordEvents->position(pos + eventLen);

        eventSize += eventLen;
        recordIndex->putInt(indexSize, eventLen);
        indexSize += 4;
        eventCount++;

        return true;
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
     * This method is not thread-safe with respect to the node as it's backing
     * ByteBuffer's limit and position may be concurrently changed.
     *
     * @param node         event's EvioNode object
     * @param extraDataLen additional data bytes to follow event (e.g. trailer length).
     * @return true if event was added; false if the event was not added because the
     *         count limit would be exceeded or the buffer is full and cannot be
     *         expanded since it's user-provided.
     * @throws EvioException if node does not correspond to a bank.
     */
    bool RecordOutput::addEvent(std::shared_ptr<EvioNode> & node, uint32_t extraDataLen) {
        return addEvent(*(node.get()), extraDataLen);
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
     * @param event        event's EvioBank object.
     * @param extraDataLen additional data bytes to follow event (e.g. trailer length).
     * @return true if event was added; false if the event was not added because the
     *         count limit would be exceeded or the buffer is full and cannot be
     *         expanded since it's user-provided.
     */
    bool RecordOutput::addEvent(EvioBank & event, uint32_t extraDataLen) {

        uint32_t eventLen = event.getTotalBytes();

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

        size_t rePos = recordEvents->position();
        event.write(recordEvents->array() + rePos, recordEvents->order());
        recordEvents->position(rePos + eventLen);

        eventSize += eventLen;
        recordIndex->putInt(indexSize, eventLen);
        indexSize += 4;
        eventCount++;

        return true;
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
     * @param event        event's EvioBank object.
     * @param extraDataLen additional data bytes to follow event (e.g. trailer length).
     * @return true if event was added; false if the event was not added because the
     *         count limit would be exceeded or the buffer is full and cannot be
     *         expanded since it's user-provided.
     */
    bool RecordOutput::addEvent(std::shared_ptr<EvioBank> & event, uint32_t extraDataLen) {
        return addEvent(*(event.get()), extraDataLen);
    }


    /**
     * Reset internal buffers. The buffer is ready to receive new data.
     * Also resets the header including removing any compression.
     * If data buffer externally provided, the starting position is set to 0.
     */
    void RecordOutput::reset() {
        indexSize  = 0;
        eventSize  = 0;
        eventCount = 0;
        startingPosition = 0;

        recordData->clear();
        recordIndex->clear();
        recordEvents->clear();
        recordBinary->clear();

        // TODO: This may do way too much! Think about this more.
        header->reset();
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
        recordBinary->position(pos);
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
            header->setEntries(0);
            header->setDataLength(0);
            header->setIndexLength(0);
            header->setCompressedDataLength(0);
            header->setLength(RecordHeader::HEADER_SIZE_BYTES);
            recordBinary->limit(startingPosition + RecordHeader::HEADER_SIZE_BYTES);
            recordBinary->position(startingPosition);
            try {
                header->writeHeader(recordBinary, 0);
            }
            catch (EvioException & e) {/* never happen */}
//std::cout << "build: buf lim = " << recordBinary->limit() <<
//             ", cap = " << recordBinary->capacity() << std::endl;
            return;
        }

        uint32_t compressionType = header->getCompressionType();

        // Position in recordBinary buffer of just past the record header
        size_t recBinPastHdr = startingPosition + RecordHeader::HEADER_SIZE_BYTES;
//std::cout << "build: pos past header = " << recBinPastHdr << std::endl;

        // Position in recordBinary buffer's backing array of just past the record header.
        // Usually the same as the corresponding buffer position. But need to
        // account for the user providing a buffer which is mapped on to a bigger backing array.
        // This may happen if the user provides a slice() of another buffer.
        size_t recBinPastHdrAbsolute = recBinPastHdr + recordBinary->arrayOffset();

        // Write index & event arrays

        // If compressing data ...
        if (compressionType != Compressor::UNCOMPRESSED) {
            // Write into a single, temporary buffer
            recordBinary->clear();
            recordData->clear();
            recordData->put( recordIndex->array(), indexSize);
            recordData->put(recordEvents->array(), eventSize);
        }
        // If NOT compressing data ...
        else {
//std::cout << "build: recordBinary len = " << userBufferSize <<
//             ", start pos = " << startingPosition << ", data to write = " <<
//        (RecordHeader::HEADER_SIZE_BYTES + indexSize + eventSize) << std::endl;

            recordBinary->clear();

            // Write directly into final buffer, past where header will go
            recordBinary->position(recBinPastHdr);
            // Write index
            recordBinary->put(recordIndex->array(), indexSize);

//std::cout << "build: recordBinary pos = " << recordBinary->position() <<
//             ", eventSize = " << eventSize << ", recordEvents buffer capacity = " <<
//             recordEvents->capacity() << std::endl;

            recordBinary->put(recordEvents->array(), eventSize);
        }

        // Evio data is padded, but not necessarily all hipo data.
        // Uncompressed data length is NOT padded, but the record length is.
        uint32_t uncompressedDataSize = indexSize + eventSize;
        uint32_t compressedSize = 0;
        uint8_t* gzippedData;
//std::cout << "build: writing index of size " << indexSize << ", events of size " <<
//             eventSize << ", total = " << uncompressedDataSize << std::endl;

        // Compress that temporary buffer into destination buffer
        // (skipping over where record header will be written).
        try {
            switch (compressionType) {
                case 1:
                    // LZ4 fastest compression
                    compressedSize = Compressor::getInstance().compressLZ4(
                            recordData->array(), 0, uncompressedDataSize,
                            recordBinary->array(), recBinPastHdrAbsolute,
                            (recordBinary->capacity() - recBinPastHdrAbsolute));

                    // Length of compressed data in bytes
                    header->setCompressedDataLength(compressedSize);
                    // Length of entire record in bytes (don't forget padding!)
                    header->setLength(4*header->getCompressedDataLengthWords() +
                                      RecordHeader::HEADER_SIZE_BYTES);
                    break;

                case 2:
                    // LZ4 highest compression
                    compressedSize = Compressor::getInstance().compressLZ4Best(
                            recordData->array(), 0, uncompressedDataSize,
                            recordBinary->array(), recBinPastHdrAbsolute,
                            (recordBinary->capacity() - recBinPastHdrAbsolute));

//std::cout << "Compressing data array from offset = 0, size = " << uncompressedDataSize <<
//             " to output.array offset = " << recBinPastHdrAbsolute << ", compressed size = " <<  compressedSize <<
//             ", available size = " << (recordBinary->capacity() - recBinPastHdrAbsolute) << std::endl;
//
//std::cout << "BEFORE setting header len: comp size = " << header->getCompressedDataLength() <<
//             ", comp words = " << header->getCompressedDataLengthWords() << ", padding = " <<
//             header->getCompressedDataLengthPadding() << std::endl;

                    header->setCompressedDataLength(compressedSize);
                    header->setLength(4*header->getCompressedDataLengthWords() +
                                      RecordHeader::HEADER_SIZE_BYTES);

//std::cout << "AFTER setting, read back from header: comp size = " << header->getCompressedDataLength() <<
//             ", comp words = " << header->getCompressedDataLengthWords() << ", padding = " <<
//             header->getCompressedDataLengthPadding() << ", rec len = " << header->getLength() << std::endl;

                    break;

                case 3:
                    // GZIP compression
#ifdef USE_GZIP
                    gzippedData = Compressor::getInstance().compressGZIP(recordData->array(), 0,
                                                                 uncompressedDataSize, &compressedSize);
                recordBinary->position(recBinPastHdr);
                recordBinary->put(gzippedData, compressedSize);
                delete[] gzippedData;
                header->setCompressedDataLength(compressedSize);
                header->setLength(4*header->getCompressedDataLengthWords() +
                                 RecordHeader::HEADER_SIZE_BYTES);
#endif
                    break;

                case 0:
                default:
                    // No compression. The uncompressed data size may not be padded to a 4byte boundary,
                    // so make sure that's accounted for here.
                    header->setCompressedDataLength(0);
                    int words = uncompressedDataSize/4;
                    if (uncompressedDataSize % 4 != 0) words++;
                    header->setLength(words*4 + RecordHeader::HEADER_SIZE_BYTES);
//std::cout << "  build(): set header length = " << header->getLength() << ", uncompressed data size = " << uncompressedDataSize << std::endl;
            }
        }
        catch (EvioException & e) {/* should not happen */}

        // Set the rest of the header values
        header->setEntries(eventCount);
        header->setDataLength(eventSize);
        header->setIndexLength(indexSize);

//std::cout << " COMPRESSED = " << compressedSize << "  events size (data  len) = " << eventSize << "  type = " <<
//             compressionType << "  uncompressed = " << uncompressedDataSize <<
//             " record bytes = " << header->getLength() << std::endl << std::endl;

        // Go back and write header into destination buffer
        try {
            // Does NOT change recordBinary pos or lim
            header->writeHeader(recordBinary, startingPosition);
        }
        catch (EvioException & e) {/* never happen */}

        // Make ready to read
        recordBinary->limit(startingPosition + header->getLength()).position(0);
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
    void RecordOutput::build(std::shared_ptr<ByteBuffer> userHeader) {
        build(*(userHeader.get()));
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
    void RecordOutput::build(const ByteBuffer & userHeader) {

        // How much user-header data do we actually have (limit - position) ?
        size_t userHeaderSize = userHeader.remaining();

        if (userHeaderSize == 0) {
            build();
            return;
        }

//std::cout << "  buld: indexSize = " << indexSize << ", index + userHeader =  " << (indexSize + userHeaderSize) <<
//             ",  userheader = " << userHeaderSize << std::endl;

        uint32_t compressionType = header->getCompressionType();
        uint32_t uncompressedDataSize = indexSize;

        // Position in recordBinary buffer of just past the record header
        size_t recBinPastHdr = startingPosition + RecordHeader::HEADER_SIZE_BYTES;

        // Position in recordBinary buffer's backing array of just past the record header.
        // Usually the same as the corresponding buffer position. But need to
        // account for the user providing a buffer which is mapped on to a bigger backing array.
        // This may happen if the user provides a slice() of another buffer.
        // Right now, slice() is not implemented in C++ so buf.arrayOffset() = 0 and
        // recBinPastHdr = recBinPastHdrAbsolute.
        size_t recBinPastHdrAbsolute = recBinPastHdr + recordBinary->arrayOffset();

        // If compressing data ...
        if (compressionType != Compressor::UNCOMPRESSED) {
            recordData->clear();
            recordBinary->clear();

            // Write into a single, temporary buffer the following:

            // 1) uncompressed index array
            // Note, put() will increment position
            recordData->put(recordIndex->array(), indexSize);

            // 2) uncompressed user header
            recordData->put(userHeader.array() + userHeader.position(), userHeaderSize);

            // Account for unpadded user header.
            // This will find the user header length in words & account for padding.
            header->setUserHeaderLength(userHeaderSize);
            // Hop over padded user header length
            uncompressedDataSize += 4*header->getUserHeaderLengthWords();
            recordData->position(uncompressedDataSize);

            // 3) uncompressed data array
            recordData->put(recordEvents->array(), eventSize);
            // Evio data is padded, but not necessarily all hipo data.
            // Uncompressed data length is NOT padded, but the record length is.
            uncompressedDataSize += eventSize;
        }
            // If NOT compressing data ...
        else {
            // Write directly into final buffer, past where header will go
            recordBinary->clear();
            recordBinary->position(recBinPastHdr);

            // 1) uncompressed index array
            recordBinary->put(recordIndex->array(), indexSize);

            // 2) uncompressed user header array
            recordBinary->put(userHeader.array() + userHeader.position(), userHeaderSize);

            header->setUserHeaderLength(userHeaderSize);
            uncompressedDataSize += 4*header->getUserHeaderLengthWords();
            recordBinary->position(recBinPastHdr + uncompressedDataSize);

            // 3) uncompressed data array (hipo/evio data is already padded)
            recordBinary->put(recordEvents->array(), eventSize);
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
                            recordData->array(), 0, uncompressedDataSize,
                            recordBinary->array(), recBinPastHdrAbsolute,
                            (recordBinary->capacity() - recBinPastHdrAbsolute));

                    // Length of compressed data in bytes
                    header->setCompressedDataLength(compressedSize);
                    // Length of entire record in bytes (don't forget padding!)
                    header->setLength(4*header->getCompressedDataLengthWords() +
                                      RecordHeader::HEADER_SIZE_BYTES);
                    break;

                case 2:
                    // LZ4 highest compression
                    compressedSize = Compressor::getInstance().compressLZ4Best(
                            recordData->array(), 0, uncompressedDataSize,
                            recordBinary->array(), recBinPastHdrAbsolute,
                            (recordBinary->capacity() - recBinPastHdrAbsolute));

                    header->setCompressedDataLength(compressedSize);
                    header->setLength(4*header->getCompressedDataLengthWords() +
                                      RecordHeader::HEADER_SIZE_BYTES);
                    break;

                case 3:
                    // GZIP compression
#ifdef USE_GZIP
                    gzippedData = Compressor::getInstance().compressGZIP(recordData->array(), 0,
                                                                     uncompressedDataSize, &compressedSize);
                recordBinary->position(recBinPastHdr);
                recordBinary->put(gzippedData, compressedSize);
                delete[] gzippedData;
                header->setCompressedDataLength(compressedSize);
                header->setLength(4*header->getCompressedDataLengthWords() +
                                 RecordHeader::HEADER_SIZE_BYTES);
#endif
                    break;

                case 0:
                default:
                    // No compression. The uncompressed data size may not be padded to a 4byte boundary,
                    // so make sure that's accounted for here.
                    header->setCompressedDataLength(0);
                    int words = uncompressedDataSize/4;
                    if (uncompressedDataSize % 4 != 0) words++;
                    header->setLength(words*4 + RecordHeader::HEADER_SIZE_BYTES);
            }
        }
        catch (EvioException & e) {/* should not happen */}

        //std::cout << " COMPRESSED SIZE = " << compressedSize << std::endl;

        // Set header values (user header length already set above)
        header->setEntries(eventCount);
        header->setDataLength(eventSize);
        header->setIndexLength(indexSize);

        // Go back and write header into destination buffer
        try {
            header->writeHeader(recordBinary, startingPosition);
        }
        catch (EvioException & e) {/* never happen */}

        // Make ready to read
        recordBinary->limit(startingPosition + header->getLength()).position(0);
    }

}
