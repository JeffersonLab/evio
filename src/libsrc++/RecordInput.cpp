/**
 * Copyright (c) 2019, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 04/09/2019
 * @author timmer
 */


#include "RecordInput.h"


namespace evio {


    /** Default constructor. */
    RecordInput::RecordInput() : headerBuffer(RecordHeader::HEADER_SIZE_BYTES) {
        header = std::make_shared<RecordHeader>();
        allocate(DEFAULT_BUF_SIZE);
        headerBuffer.order(byteOrder);
    }


    /**
     * Constructor.
     * @param order byte order of internal byte arrays.
     */
    RecordInput::RecordInput(const ByteOrder & order) : RecordInput() {
        byteOrder = order;
        setByteOrder(order);
    }


    /**
     * Allocates data & record buffers for constructing a record from the input.
     * @param size number of bytes to allocate for each internal ByteBuffer.
     */
    void RecordInput::allocate(size_t size) {
        dataBuffer = std::make_shared<ByteBuffer>(size);
        dataBuffer->order(byteOrder);

        recordBuffer = ByteBuffer(size);
        recordBuffer.order(byteOrder);
    }


    /** Copy constructor. */
    RecordInput::RecordInput(const RecordInput & srcRec) {

        if (this != &srcRec) {
            // Copy the buffers, do not use the same shared pointer
            dataBuffer   = srcRec.dataBuffer;
            recordBuffer = srcRec.recordBuffer;
            headerBuffer = srcRec.headerBuffer;

            header->copy(srcRec.header);
            nEntries                 = srcRec.nEntries;
            userHeaderOffset         = srcRec.userHeaderOffset;
            eventsOffset             = srcRec.eventsOffset;
            uncompressedEventsLength = srcRec.uncompressedEventsLength;
            byteOrder                = srcRec.byteOrder;
        }
    }


    /**
     * Move constructor.
     * @param srcRec RecordInput to move.
     */
    RecordInput::RecordInput(RecordInput && srcRec) noexcept {
        // Use code below in move assignment operator
        *this = std::move(srcRec);
    }


    /**
     * Move assignment operator.
     * @param other right side object.
     * @return left side object.
     */
    RecordInput & RecordInput::operator=(RecordInput&& other) noexcept {

        // Avoid self assignment ...
        if (this != &other) {
            // Moves the shared pointer in the buffer, rest is simply copied
            dataBuffer               = std::move(other.dataBuffer);
            recordBuffer             = std::move(other.recordBuffer);
            headerBuffer             = std::move(other.headerBuffer);

            header                   = std::move(other.header);
            nEntries                 = other.nEntries;
            userHeaderOffset         = other.userHeaderOffset;
            eventsOffset             = other.eventsOffset;
            uncompressedEventsLength = other.uncompressedEventsLength;
            byteOrder                = other.byteOrder;
        }
        return *this;
    }


    /**
     * Assignment operator.
     * @param other right side object.
     * @return left side object.
     */
    RecordInput & RecordInput::operator=(const RecordInput& other) {

        // Avoid self assignment ...
        if (this != &other) {
            dataBuffer               = other.dataBuffer;
            recordBuffer             = other.recordBuffer;
            headerBuffer             = other.headerBuffer;
            header->copy(other.header);
            nEntries                 = other.nEntries;
            userHeaderOffset         = other.userHeaderOffset;
            eventsOffset             = other.eventsOffset;
            uncompressedEventsLength = other.uncompressedEventsLength;
            byteOrder                = other.byteOrder;
        }
        return *this;
    }


    /**
     * Get the header of this record.
     * @return header of this record.
     */
    std::shared_ptr<RecordHeader> RecordInput::getHeader() {return header;}


    /**
     * Get the byte order of the internal buffers.
     * @return byte order of the internal buffers.
     */
    const ByteOrder & RecordInput::getByteOrder() {return byteOrder;}


    /**
     * Set the byte order of the internal buffers.
     * @param order desired byte order of internal buffers.
     */
    void RecordInput::setByteOrder(const ByteOrder & order) {
        byteOrder = order;
        dataBuffer->order(order);
        recordBuffer.order(order);
        headerBuffer.order(order);
    }


    /**
     * Get the buffer with all uncompressed data in it.
     * It's position and limit are set to read only event data.
     * That means no header, index, or user-header.
     * @return  the buffer with uncompressed event data in it.
     */
    std::shared_ptr<ByteBuffer> RecordInput::getUncompressedDataBuffer() {
        dataBuffer->limit(eventsOffset + uncompressedEventsLength).position(eventsOffset);
        return dataBuffer;
    }


    /**
     * Does this record contain an event index?
     * @return true if record contains an event index, else false.
     */
    bool RecordInput::hasIndex() const {return (header->getIndexLength() > 0);}


    /**
     * Does this record contain a user header?
     * @return true if record contains a user header, else false.
     */
    bool RecordInput::hasUserHeader() const {return (header->getUserHeaderLength() > 0);}


    /**
     * Get the event at the given index and return it in an allocated array.
     *
     * @param index  index of event starting at 0. If index too large, it's set to last index.
     * @param len    pointer to int which gets filled with the data length in bytes.
     * @return byte array containing event.
     */
    std::shared_ptr<uint8_t> RecordInput::getEvent(uint32_t index, uint32_t * len) {
        uint32_t firstPosition = 0;

        if (index > 0) {
            if (index >= header->getEntries()) {
                index = header->getEntries() - 1;
            }
            // Remember, the index array of events lengths (at beginning of dataBuffer)
            // was overwritten in readRecord() to contain offsets to next event.
            firstPosition = dataBuffer->getInt( (index-1)*4 );
        }

        uint32_t lastPosition = dataBuffer->getUInt(index*4);
        uint32_t length = lastPosition - firstPosition;
        uint32_t offset = eventsOffset + firstPosition;

        // TODO: Allocating memory here!!!
        auto event = std::shared_ptr<uint8_t>(new uint8_t[length], std::default_delete<uint8_t[]>());

        std::memcpy((void *)event.get(), (const void *)(dataBuffer->array() + offset), length);
        if (len != nullptr) {
            *len = length;
        }

//std::cout << "getEvent: reading from " << offset << ",  length = " << length << std::endl;
        return event;
    }


    /**
     * Get the event at the given index and return it in the provided array.
     *
     * @param event  pointer at which to write event.
     * @param index  index of event starting at 0. If index too large, it's set to largest valid index.
     * @param evLen  available memory for writing event in bytes.
     * @return length of data written in bytes.
     * @throws std::overflow_error if provided mem is too small to hold event.
     */
    uint32_t RecordInput::getEvent(uint8_t *event, uint32_t index, uint32_t evLen) {
        uint32_t firstPosition = 0;

        if (index > 0) {
            if (index >= header->getEntries()) {
                index = header->getEntries() - 1;
            }
            // Remember, the index array of events lengths (at beginning of dataBuffer)
            // was overwritten in readRecord() to contain offsets to next event.
            firstPosition = dataBuffer->getInt( (index-1)*4 );
        }

        uint32_t lastPosition = dataBuffer->getUInt(index*4);
        uint32_t length = lastPosition - firstPosition;
        uint32_t offset = eventsOffset + firstPosition;

        if (length > evLen) {
            // not enough mem to store event
            throw std::overflow_error("event mem (" + std::to_string(evLen) +
                                      " bytes) is too small to hold data (" +
                                      std::to_string(length) + ")");
        }

        std::memcpy((void *)event, (const void *)(dataBuffer->array() + offset), length);
        return length;
    }


    /**
     * Returns the length of the event with given index.
     * @param index index of the event
     * @return length of the data in bytes or zero if index
     *         does not coresspond to a valid event.
     */
    uint32_t RecordInput::getEventLength(uint32_t index) const {
        if (index >= getEntries()) return 0;

        uint32_t firstPosition = 0;
        if (index > 0) {
            if (index >= header->getEntries()) {
                index = header->getEntries() - 1;
            }
            firstPosition = dataBuffer->getInt( (index-1)*4 );
        }

        uint32_t lastPosition = dataBuffer->getUInt(index*4);
        return lastPosition - firstPosition;
    }


    /**
     * Get the event at the given index and write it into the given byte buffer.
     * The given byte buffer has to be large enough to receive all the event's data,
     * but the buffer->limit() is ignored & reset.
     * Buffer pos & lim are ready to read on return.
     * Buffer's byte order is set to that of the internal buffers.
     * Data written at buffer's position.
     *
     * @param buffer    buffer to be filled with event.
     * @param index     index of event starting at 0.
     * @return buffer   buffer arg.
     * @throws EvioException if index too large, or buffer has insufficient space to
     *                       contain event (buffer->capacity() < event size).
     */
    std::shared_ptr<ByteBuffer> & RecordInput::getEvent(std::shared_ptr<ByteBuffer> & buffer, uint32_t index) {
        getEvent(*(buffer.get()), buffer->position(), index);
        return buffer;
    }


    /**
     * Get the event at the given index and write it into the given byte buffer.
     * The given byte buffer has to be large enough to receive all the event's data,
     * but the buffer->limit() is ignored & reset.
     * Buffer pos & lim are ready to read on return.
     * Buffer's byte order is set to that of the internal buffers.
     *
     * @param buffer    buffer to be filled with event.
     * @param bufOffset offset into buffer to place event.
     * @param index     index of event starting at 0.
     * @return buffer   buffer arg.
     * @throws EvioException if index too large, or buffer has insufficient space to
     *                       contain event (buffer->capacity() < event size).
     */
    std::shared_ptr<ByteBuffer> & RecordInput::getEvent(std::shared_ptr<ByteBuffer> & buffer,
                                                      size_t bufOffset, uint32_t index) {
        getEvent(*buffer, bufOffset, index);
        return buffer;
    }


    /**
     * Get the event at the given index and write it into the given byte buffer.
     * The given byte buffer has to be large enough to receive all the event's data,
     * but the buffer.limit() is ignored & reset.
     * Buffer pos & lim are ready to read on return.
     * Buffer's byte order is set to that of the internal buffers.
     * Data written at buffer's position.
     *
     * @param buffer    buffer to be filled with event.
     * @param index     index of event starting at 0.
     * @return buffer   buffer arg.
     * @throws EvioException if index too large, or buffer has insufficient space to
     *                       contain event (buffer.capacity() < event size).
     */
    ByteBuffer & RecordInput::getEvent(ByteBuffer & buffer, uint32_t index) {
        return getEvent(buffer, buffer.position(), index);
    }


    /**
     * Get the event at the given index and write it into the given byte buffer.
     * The given byte buffer has to be large enough to receive all the event's data,
     * but the buffer.limit() is ignored & reset.
     * Buffer pos & lim are ready to read on return.
     * Buffer's byte order is set to that of the internal buffers.
     *
     * @param buffer    buffer to be filled with event.
     * @param bufOffset offset into buffer to place event.
     * @param index     index of event starting at 0.
     * @return buffer   buffer arg.
     * @throws EvioException if index too large, or buffer has insufficient space to
     *                       contain event (buffer.capacity() < event size).
     */
    ByteBuffer & RecordInput::getEvent(ByteBuffer & buffer, size_t bufOffset, uint32_t index) {
        uint32_t firstPosition = 0;
        if (index > 0) {
            if (index >= header->getEntries()) {
                throw EvioException("index too large");
            }
            firstPosition = dataBuffer->getUInt((index - 1) * 4);
        }
        uint32_t lastPosition = dataBuffer->getUInt(index * 4);
        uint32_t length = lastPosition - firstPosition;
        uint32_t offset = eventsOffset + firstPosition;

        if (bufOffset + length > buffer.capacity()) {
            throw EvioException("buffer with offset " + std::to_string(bufOffset) +
                                " is smaller than the event.");
        }

        buffer.order(byteOrder);

        std::memcpy((void *)(buffer.array() + buffer.arrayOffset() + bufOffset),
                    (const void *)(dataBuffer->array() + offset), length);

        // Make buffer ready to read.
        // Always set limit first, else you can cause exception.
        buffer.limit(bufOffset + length).position(bufOffset);

        return buffer;
    }


    /**
     * Get the user header contained in this record and return it in an allocated array.
     * @return the user header contained in this record, or null if none.
     */
    std::shared_ptr<uint8_t> RecordInput::getUserHeader() {

        uint32_t length = header->getUserHeaderLength();
        auto userHeader = std::shared_ptr<uint8_t>(new uint8_t[length], std::default_delete<uint8_t[]>());
        std::memcpy((void *)(userHeader.get()),
                    (const void *)(dataBuffer->array() + userHeaderOffset), length);

        return userHeader;
    }


    /**
     * Get any existing user header and write it into the given byte buffer.
     * The given byte buffer must be large enough to contain user header.
     * Buffer's byte order is set to that of the internal buffers.
     * Buffer's position is set to bufOffset and limit is set to bufOffset +
     * userHeader size.
     *
     * @param buffer    buffer to be filled with user header.
     * @param bufOffset offset into buffer to place user header.
     * @return buffer passed in (position = limit = bufOffset if no user header exists).
     * @throws EvioException if buffer has insufficient space to contain user header
     *                       (buffer.capacity() - bufOffset < user header size).
     */
    std::shared_ptr<ByteBuffer> & RecordInput::getUserHeader(std::shared_ptr<ByteBuffer> & buffer, size_t bufOffset) {
        getUserHeader(*buffer, bufOffset);
        return buffer;
    }


    /**
     * Get any existing user header and write it into the given byte buffer.
     * The given byte buffer must be large enough to contain user header.
     * Buffer's byte order is set to that of the internal buffers.
     * Buffer's position is set to bufOffset and limit is set to bufOffset +
     * userHeader size.
     *
     * @param buffer    buffer to be filled with user header.
     * @param bufOffset offset into buffer to place user header.
     * @return buffer passed in (position = limit = bufOffset if no user header exists).
     * @throws EvioException if buffer has insufficient space to contain user header
     *                       (buffer.capacity() - bufOffset < user header size).
     */
    ByteBuffer & RecordInput::getUserHeader(ByteBuffer & buffer, size_t bufOffset) {

        uint32_t length = header->getUserHeaderLength();

        if (bufOffset + length > buffer.capacity()) {
            throw EvioException("buffer with offset " + std::to_string(bufOffset) +
                                " is smaller than the user header.");
        }

        buffer.order(byteOrder);

        std::memcpy((void *)(buffer.array() + buffer.arrayOffset() + bufOffset),
                    (const void *)(dataBuffer->array() + userHeaderOffset), length);

        // Make buffer ready to read.
        // Always set limit first, else you can cause exception.
        buffer.limit(bufOffset + length).position(bufOffset);

        return buffer;
    }


    /**
     * Get any existing user header and write it into the given byte buffer.
     * The byte buffer must be large enough to contain it.
     * Warning, buffer.limit() is ignored & reset.
     * Parse the user header into the given recordInput object which will be set to
     * the byte order of this object.
     * @param buffer    buffer to be filled with user header
     * @param bufOffset offset into buffer to place user header.
     * @return record parsed from user header or nullptr if no user header exists.
     * @throws EvioException if buffer has insufficient space to contain user header
     *                       (buffer.capacity() - bufOffset < user header size), or
     *                       if buffer not in hipo format.
     */
    std::shared_ptr<RecordInput> RecordInput::getUserHeaderAsRecord(ByteBuffer & buffer, size_t bufOffset) {

        // Read user header into given buffer, ready to read & with proper byte order.
        getUserHeader(buffer, bufOffset);
        // If there is no user header ...
        if (buffer.remaining() < 1) {
            return nullptr;
        }

        // Parse user header into record
        auto newRecord = std::make_shared<RecordInput>(byteOrder);
        newRecord->readRecord(buffer, bufOffset);
        return newRecord;
    }


    /**
     * Reads record from the file at given position. Call this method or
     * {@link #readRecord(ByteBuffer &, size_t)} before calling any other.
     * Any compressed data is decompressed. Memory is allocated as needed.
     * First the header is read, then the length of
     * the record is read from header, then
     * following bytes are read and decompressed.
     * Handles the case in which index array length = 0.
     *
     * @param file opened file descriptor
     * @param position position in the file
     * @throws EvioException if file contains too little data,
     *                       if the input data was corrupted (including if the input data is an incomplete stream),
     *                       is not in proper format, or version earlier than 6, or
     *                       error in uncompressing gzipped data.
     */
    void RecordInput::readRecord(std::ifstream & file, size_t position) {

        // Read header
        if (!file.is_open()) {
            throw EvioException("file not open");
        }
        file.seekg(position);
        file.read(reinterpret_cast<char *>(headerBuffer.array()), RecordHeader::HEADER_SIZE_BYTES);

        // This will switch headerBuffer to proper byte order
        header->readHeader(headerBuffer);

//std::cout << "readRecord: header = \n" << header->toString() << std::endl;

        // Make sure all internal buffers have the same byte order
        setByteOrder(headerBuffer.order());

        uint32_t recordLengthBytes = header->getLength();
        uint32_t headerLength      = header->getHeaderLength();
        nEntries                   = header->getEntries();     // # of events
        uint32_t indexLen          = header->getIndexLength(); // bytes
        uint32_t cLength           = header->getCompressedDataLength();
        uint32_t userHdrLen        = 4*header->getUserHeaderLengthWords(); // bytes + padding

        // Handle the case of len = 0 for index array in header:
        // The necessary memory to hold such an index = 4*nEntries bytes.
        // In this case it must be reconstructed here by scanning the record.
        bool findEvLens = false;
        if (indexLen == 0) {
            findEvLens = true;
            indexLen = 4*nEntries;
        }
        else if (indexLen != 4*nEntries) {
            // Header info is goofed up
            throw EvioException("Record header index array len " + std::to_string(indexLen) +
                                " does not match 4*(event cnt) " + std::to_string(4*nEntries));
        }

        // How many bytes will the expanded record take?
        // Just data:
        uncompressedEventsLength = 4*header->getDataLengthWords();
        // Everything except the header & don't forget padding:
        uint32_t neededSpace =  indexLen + userHdrLen +uncompressedEventsLength;

        // Handle rare situation in which compressed data takes up more room.
        // This also determines size of recordBuffer in which compressed
        // data is first read.
        neededSpace = neededSpace < cLength ? cLength : neededSpace;

        // Make room to handle all data to be read & uncompressed
        if (dataBuffer->capacity() < neededSpace) {
            allocate(neededSpace);
        }
        dataBuffer->clear();
        if (findEvLens) {
            // Leave room in front for reconstructed index array
            dataBuffer->position(indexLen);
        }

        // Go here to read rest of record
        file.seekg(position + headerLength);

        // Decompress data
        switch (header->getCompressionType()) {
            case 1:
            case 2:
                // LZ4
                // Read compressed data
                file.read(reinterpret_cast<char *>(recordBuffer.array()), cLength);
                Compressor::getInstance().uncompressLZ4(recordBuffer, cLength, *(dataBuffer.get()));
                break;

            case 3:
                // GZIP
#ifdef USE_GZIP
                {
            file.read(reinterpret_cast<char *>(recordBuffer.array()), cLength);
            // size of destination buffer on entry, uncompressed bytes on exit
            uint32_t uncompLen = recordBuffer.capacity();
            uint8_t* ungzipped = Compressor::getInstance().uncompressGZIP(recordBuffer.array(), 0,
                                                                          cLength, &uncompLen,
                                                                          uncompressedEventsLength);
            dataBuffer->put(ungzipped, uncompLen);
            delete[] ungzipped;
        }
#endif
                break;

            case 0:
            default:
                // Read uncompressed data - rest of record
                uint32_t len = recordLengthBytes - headerLength;
                if (findEvLens) {
                    // If we still have to find event lengths to reconstruct missing index array,
                    // leave room for it in dataBuffer.
                    file.read(reinterpret_cast<char *>(dataBuffer->array() + indexLen), len);
                }
                else {
                    file.read(reinterpret_cast<char *>(dataBuffer->array()), len);
                }
        }

        // Offset in dataBuffer past index array, to user header
        userHeaderOffset = indexLen;
        // Offset in dataBuffer just past index + user header, to events
        eventsOffset = indexLen + userHdrLen;

        // Overwrite event lengths with event offsets.
        // First offset is to beginning of 2nd (starting at 1) event, etc.
        int event_pos = 0, read_pos = eventsOffset;

        for (uint32_t i = 0; i < nEntries; i++) {
            int size;
            if (findEvLens) {
                // In the case there is no index array, evio MUST be the format!
                // Reconstruct index - the first bank word = len - 1 words.
                size = 4*(dataBuffer->getInt(read_pos) + 1);
                read_pos += size;
            }
            else {
                // ev size in index array
                size = dataBuffer->getInt(i * 4);
            }
            event_pos += size;
            dataBuffer->putInt(i*4, event_pos);
        }
    }


    /**
     * Reads a record from the buffer at the given offset. Call this method or
     * {@link #readRecord(std::ifstream &, size_t)} before calling any other.
     * Any compressed data is decompressed. Memory is allocated as needed.
     * Handles the case in which index array length = 0.
     *
     * @param buffer buffer containing record data.
     * @param offset offset into buffer to beginning of record data.
     * @throws EvioException if buffer contains too little data,
     *                       is not in proper format, or version earlier than 6 or
     *                       error in uncompressing gzipped data.
     */
    void RecordInput::readRecord(ByteBuffer & buffer, size_t offset) {

        // This will switch buffer to proper byte order
        header->readHeader(buffer, offset);

//std::cout << "readRecord: header = \n" << header->toString() << std::endl;

        // Make sure all internal buffers have the same byte order
        setByteOrder(buffer.order());

        uint32_t recordLengthBytes = header->getLength();
        uint32_t headerLength      = header->getHeaderLength();
        nEntries                   = header->getEntries();     // # of events
        uint32_t indexLen          = header->getIndexLength(); // bytes
        uint32_t cLength           = header->getCompressedDataLength();
        uint32_t userHdrLen        = 4*header->getUserHeaderLengthWords(); // bytes + padding

        size_t compDataOffset = offset + headerLength;

        // Handle the case of len = 0 for index array in header:
        // The necessary memory to hold such an index = 4*nEntries bytes.
        // In this case it must be reconstructed here by scanning the record.
        bool findEvLens = false;
        if (indexLen == 0) {
            findEvLens = true;
            indexLen = 4*nEntries;
        }
        else if (indexLen != 4*nEntries) {
            // Header info is goofed up
            throw EvioException("Record header index array len " + std::to_string(indexLen) +
                                    " does not match 4*(event cnt) " + std::to_string(4*nEntries));
        }

        // How many bytes will the expanded record take?
        // Just data:
        uncompressedEventsLength = 4*header->getDataLengthWords();
        // Everything except the header & don't forget padding:
        uint32_t neededSpace =  indexLen + userHdrLen +uncompressedEventsLength;

        // Make room to handle all data to be read & uncompressed
        if (dataBuffer->capacity() < neededSpace) {
            allocate(neededSpace);
        }
        dataBuffer->clear();
        if (findEvLens) {
            // Leave room in front for reconstructed index array
            dataBuffer->position(indexLen);
        }

        // Decompress data
        switch (header->getCompressionType()) {
            case Compressor::CompressionType::LZ4 :
            case Compressor::CompressionType::LZ4_BEST :
                // Read LZ4 compressed data (WARNING: this does set limit on dataBuffer!)
                Compressor::getInstance().uncompressLZ4(buffer, compDataOffset, cLength, *(dataBuffer.get()));
                break;

            case Compressor::CompressionType::GZIP :
#ifdef USE_GZIP
                {
                // Read GZIP compressed data
                uint32_t uncompLen = neededSpace;
                buffer.limit(compDataOffset + cLength).position(compDataOffset);
                uint8_t* ungzipped = Compressor::getInstance().uncompressGZIP(buffer, &uncompLen);
                dataBuffer->put(ungzipped, uncompLen);
                delete[] ungzipped;
            }
#endif
                break;

            case Compressor::CompressionType::UNCOMPRESSED :
            default:
                // TODO: See if we can avoid this unnecessary copy!
                // Read uncompressed data - rest of record
//std::cout << "readRecord: start recordLengthByte = " << recordLengthBytes << ", headerLength = " << headerLength << std::endl;
                uint32_t len = recordLengthBytes - headerLength;
//std::cout << "readRecord: start reading at buffer pos = " << (buffer.arrayOffset() + compDataOffset) <<
//", buffer limit = " << buffer.limit() << ", dataBuf pos = " << dataBuffer->position() << ", lim = " << dataBuffer->limit() <<
//", LEN ===== " << len << std::endl;
                if (findEvLens) {
                    // If we still have to find event lengths to reconstruct missing index array,
                    // leave room for it in dataBuffer.
                    std::memcpy((void *)(dataBuffer->array() + indexLen),
                                (const void *)(buffer.array() + buffer.arrayOffset() + compDataOffset), len);
                }
                else {
                    std::memcpy((void *)dataBuffer->array(),
                                (const void *)(buffer.array() + buffer.arrayOffset() + compDataOffset), len);
                }
        }

        // Offset in dataBuffer past index array, to user header
        userHeaderOffset = indexLen;
        // Offset in dataBuffer just past index + user header, to events
        eventsOffset = indexLen + userHdrLen;

// TODO: How do we handle trailers???

        // Overwrite event lengths with event offsets.
        // First offset is to beginning of 2nd (starting at 1) event, etc.
        int event_pos = 0, read_pos = eventsOffset;

        for (uint32_t i = 0; i < nEntries; i++) {
            int size;
            if (findEvLens) {
                // In the case there is no index array, evio MUST be the format!
                // Reconstruct index - the first bank word = len - 1 words.
                size = 4*(dataBuffer->getInt(read_pos) + 1);
                read_pos += size;
            }
            else {
                // ev size in index array
                size = dataBuffer->getInt(i * 4);
            }
            event_pos += size;
            dataBuffer->putInt(i*4, event_pos);
        }
    }


    /**
     * Uncompress the data of a record from the source buffer at the given offset
     * into the destination buffer.
     * Be aware that the position & limit of srcBuf may be changed.
     * The limit of dstBuf may be changed. The position of dstBuf will
     * be set to just after the user-header and just before the data.
     *
     * @param srcBuf buffer containing record data.
     * @param srcOff offset into srcBuf to beginning of record data.
     * @param dstBuf buffer into which the record is uncompressed.
     * @param hdr    RecordHeader to be used to read the record header in srcBuf.
     * @return the original record size in srcBuf (bytes).
     * @throws EvioException if srcBuf contains too little data,
     *                       is not in proper format, or version earlier than 6.
     */
    uint32_t RecordInput::uncompressRecord(std::shared_ptr<ByteBuffer> & srcBuf, size_t srcOff,
                                           std::shared_ptr<ByteBuffer> & dstBuf,
                                           RecordHeader & hdr) {
        return uncompressRecord(*srcBuf, srcOff, *dstBuf, hdr);
    }


    /**
     * Uncompress the data of a record from the source buffer at the given offset
     * into the destination buffer.
     * Be aware that the position & limit of srcBuf may be changed.
     * The limit of dstBuf may be changed. The position of dstBuf will
     * be set to just after the user-header and just before the data.
     *
     * @param srcBuf buffer containing record data.
     * @param srcOff offset into srcBuf to beginning of record data.
     * @param dstBuf buffer into which the record is uncompressed.
     * @param hdr    RecordHeader to be used to read the record header in srcBuf.
     * @return the original record size in srcBuf (bytes).
     * @throws EvioException if srcBuf contains too little data,
     *                       is not in proper format, or version earlier than 6.
     */
    uint32_t RecordInput::uncompressRecord(ByteBuffer & srcBuf, size_t srcOff, ByteBuffer & dstBuf,
                                           RecordHeader & hdr) {

        size_t dstOff = dstBuf.position();

        // Read in header. This will switch srcBuf to proper byte order.
        hdr.readHeader(srcBuf, srcOff);
//std::cout << std::endl << "uncompressRecord: hdr --> " << std::endl << hdr.toString() << std::endl;

        uint32_t headerBytes              = hdr.getHeaderLength();
        uint32_t compressionType          = hdr.getCompressionType();
        uint32_t origRecordBytes          = hdr.getLength();
        uint32_t compressedDataLength     = hdr.getCompressedDataLength();
        uint32_t uncompressedRecordLength = hdr.getUncompressedRecordLength();

        size_t   compressedDataOffset = srcOff + headerBytes;
        uint32_t indexLen = hdr.getIndexLength();
        uint32_t userLen  = 4*hdr.getUserHeaderLengthWords();  // padded

        // How many bytes will the expanded record take?
        // Everything except the record header & don't forget padding:
        uint32_t neededSpace = indexLen + userLen + 4 * hdr.getDataLengthWords();

        // Make sure destination buffer has the same byte order
        //dstBuf.order(srcBuf.order());

        if (compressionType != Compressor::UNCOMPRESSED) {
            // Copy (uncompressed) general record header to destination buffer
            std::memcpy((void *)(dstBuf.array() + dstOff + dstBuf.arrayOffset()),
                        (const void *)(srcBuf.array() + srcOff + srcBuf.arrayOffset()), headerBytes);

            dstBuf.position(dstOff + headerBytes);
        }
        else {
            // Since everything is uncompressed, copy it all over as is
            std::memcpy((void *)(dstBuf.array() + dstOff + dstBuf.arrayOffset()),
                        (const void *)(srcBuf.array() + srcOff + srcBuf.arrayOffset()), headerBytes + neededSpace);

            dstBuf.position(dstOff + headerBytes);
        }

        // Decompress data
        switch (compressionType) {
            case 1:
            case 2:
                // Read LZ4 compressed data
                Compressor::getInstance().uncompressLZ4(srcBuf, compressedDataOffset,
                                                        compressedDataLength, dstBuf);
                dstBuf.limit(dstBuf.capacity());
                break;

            case 3:
                // Read GZIP compressed data
#ifdef USE_GZIP
                {
                 uint32_t uncompLen = neededSpace;
                 srcBuf.limit(compressedDataOffset + compressedDataLength).position(compressedDataOffset);
                 uint8_t* ungzipped = Compressor::getInstance().uncompressGZIP(srcBuf, &uncompLen);
                 dstBuf.put(ungzipped, uncompLen);
                 delete[] ungzipped;
      }
#endif
                break;

            case 0:
            default:
                // Everything copied over above
                break;
        }

        srcBuf.limit(srcBuf.capacity());

        // Position dstBuf just before the data so it can be scanned for EvioNodes.
        // This takes user header padding into account.
        dstBuf.position(dstOff + headerBytes + indexLen + userLen);

        // Reset the compression type and length in header to 0
        dstBuf.putInt(dstOff + RecordHeader::COMPRESSION_TYPE_OFFSET, 0);
        hdr.setCompressionType(Compressor::UNCOMPRESSED).setCompressedDataLength(0);
//        // The previous call updated the bitinfo word in hdr. Write this into buf:
//        dstBuf.putInt(dstOff + RecordHeader::BIT_INFO_OFFSET, hdr.getBitInfoWord());

        // Reset the header length
        dstBuf.putInt(dstOff + RecordHeader::RECORD_LENGTH_OFFSET, uncompressedRecordLength/4);
        hdr.setLength(uncompressedRecordLength);

        //            // If there is an index, change lengths to event offsets
        //            if (hdr.getIndexLength() > 0) {
        //                // Number of entries in index
        //                int nEntries = hdr.getEntries();
        //
        //                // Overwrite event lengths with event offsets
        //                int index, eventPos = 0;
        //                int off = dstOff + headerBytes;
        //
        //                for (int i = 0; i < nEntries; i++) {
        //                    index = off + 4*i;
        //                    int size = dstBuf.getInt(index);
        //                    eventPos += size;
        //                    dstBuf.putInt(index, eventPos);
        //                }
        //            }

        return origRecordBytes;
    }


    /**
     * Returns number of the events packed in the record.
     * @return number of the events packed in the record
     */
    uint32_t RecordInput::getEntries() const {return nEntries;}


    /**
     * Prints on the screen the index array of the record.
     */
    void RecordInput::showIndex() const {
        for(uint32_t i = 0; i < nEntries; i++){
            std::cout << std::setw(3) << dataBuffer->getInt(i*4) << "  ";
        }
        std::cout << std::endl;
    }

}

