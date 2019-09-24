//
// Created by timmer on 4/9/19.
//

#include "RecordInput.h"

/** Default constructor. */
RecordInput::RecordInput() {
    header = RecordHeader();
    allocate(DEFAULT_BUF_SIZE);

    // Allocate buffer to read header into
    headerBuffer = ByteBuffer(RecordHeader::HEADER_SIZE_BYTES);
    headerBuffer.order(byteOrder);
}


/**
 * Constructor with argument.
 * @param order byte order of internal byte arrays.
 */
RecordInput::RecordInput(const ByteOrder & order) {
    byteOrder = order;
    header = RecordHeader();
    allocate(DEFAULT_BUF_SIZE);

    // Allocate buffer to read header into
    headerBuffer = ByteBuffer(RecordHeader::HEADER_SIZE_BYTES);
    headerBuffer.order(byteOrder);
}


/** Copy constructor. */
RecordInput::RecordInput(const RecordInput & srcRec) {

    if (this != &srcRec) {
        // Copy the buffers, do not use the same shared pointer
        dataBuffer = srcRec.dataBuffer;
        recordBuffer = srcRec.recordBuffer;
        headerBuffer = srcRec.headerBuffer;

        header = srcRec.header;
        nEntries = srcRec.nEntries;
        userHeaderOffset = srcRec.userHeaderOffset;
        eventsOffset = srcRec.eventsOffset;
        uncompressedEventsLength = srcRec.uncompressedEventsLength;
        byteOrder = srcRec.byteOrder;
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

        header                   = other.header;
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
        header                   = other.header;
        nEntries                 = other.nEntries;
        userHeaderOffset         = other.userHeaderOffset;
        eventsOffset             = other.eventsOffset;
        uncompressedEventsLength = other.uncompressedEventsLength;
        byteOrder                = other.byteOrder;
    }
    return *this;
}


/**
 * Allocates data & record buffers for constructing a record from the input.
 * @param size number of bytes to allocate for each internal ByteBuffer.
 */
void RecordInput::allocate(size_t size) {

    dataBuffer = ByteBuffer(size);
    dataBuffer.order(byteOrder);

    recordBuffer = ByteBuffer(size);
    recordBuffer.order(byteOrder);
}


/**
 * Get the header of this record.
 * @return header of this record.
 */
RecordHeader & RecordInput::getHeader() {return header;}


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
    dataBuffer.order(order);
    recordBuffer.order(order);
    headerBuffer.order(order);
}


/**
 * Get the buffer with all uncompressed data in it.
 * It's position and limit are set to read only event data.
 * That means no header, index, or user-header.
 * @return  the buffer with uncompressed event data in it.
 */
ByteBuffer & RecordInput::getUncompressedDataBuffer() {
    dataBuffer.limit(eventsOffset + uncompressedEventsLength).position(eventsOffset);
    return dataBuffer;
}


/**
 * Does this record contain an event index?
 * @return true if record contains an event index, else false.
 */
bool RecordInput::hasIndex() {return (header.getIndexLength() > 0);}


/**
 * Does this record contain a user header?
 * @return true if record contains a user header, else false.
 */
bool RecordInput::hasUserHeader() {return (header.getUserHeaderLength() > 0);}


/**
 * Get the event at the given index and return it in an allocated array.
 * Caller must do delete[] on returned pointer!
 *
 * @param index  index of event starting at 0. If index too large, it's
 *               set to last index.
 * @return byte array containing event.
 */
uint8_t* RecordInput::getEvent(uint32_t index) {

    uint32_t firstPosition = 0;

    if (index > 0) {
        if (index >= header.getEntries()) {
            index = header.getEntries() - 1;
        }
        // Remember, the index array of events lengths (at beginning of dataBuffer)
        // was overwritten in readRecord() to contain offsets to events.
        firstPosition = dataBuffer.getInt( (index-1)*4 );
    }

    uint32_t lastPosition = dataBuffer.getUInt(index*4);
    uint32_t length = lastPosition - firstPosition;

// TODO: Allocating memory here!!!
    auto event = new uint8_t[length];
    uint32_t offset = eventsOffset + firstPosition;

    std::memcpy((void *)event, (const void *)(dataBuffer.array() + offset), length);
    //System.arraycopy(dataBuffer.array(), offset, event, 0, length);

//cout << "getEvent: reading from " << offset << "  length = " << length << endl;
    return event;
}


/**
 * Returns the length of the event with given index.
 * @param index index of the event
 * @return length of the data in bytes or zero if index
 *         does not coresspond to a valid event.
 */
uint32_t RecordInput::getEventLength(uint32_t index) {
    if (index >= getEntries()) return 0;

    uint32_t firstPosition = dataBuffer.getUInt((index - 1) * 4);
    uint32_t lastPosition  = dataBuffer.getUInt(index * 4);
    uint32_t length = lastPosition - firstPosition;
    return length;
}


/**
 * Get the event at the given index and write it into the given byte buffer.
 * The given byte buffer has to be large enough to receive all the event's data,
 * but the buffer.limit() is ignored & reset.
 * Buffer's byte order is set to that of the internal buffers.
 *
 * @param buffer buffer to be filled with event starting at position = 0.
 * @param index  index of event starting at 0.
 * @return buffer.
 * @throws HipoException if index too large, or buffer has insufficient space to
 *                       contain event (buffer.capacity() < event size).
 */
ByteBuffer & RecordInput::getEvent(ByteBuffer & buffer, uint32_t index) {
    return getEvent(buffer, 0, index);
}


/**
 * Get the event at the given index and write it into the given byte buffer.
 * The given byte buffer has to be large enough to receive all the event's data,
 * but the buffer.limit() is ignored & reset.
 * Buffer's byte order is set to that of the internal buffers.
 *
 * @param buffer    buffer to be filled with event.
 * @param bufOffset offset into buffer to place event.
 * @param index     index of event starting at 0.
 * @return buffer.
 * @throws HipoException if index too large, or buffer has insufficient space to
 *                       contain event (buffer.capacity() < event size).
 */
ByteBuffer & RecordInput::getEvent(ByteBuffer & buffer, size_t bufOffset, uint32_t index) {

    uint32_t firstPosition = 0;
    if (index > 0) {
        if (index >= header.getEntries()) {
            throw HipoException("index too large");
        }
        firstPosition = dataBuffer.getUInt((index - 1) * 4);
    }
    uint32_t lastPosition = dataBuffer.getUInt(index * 4);
    uint32_t length = lastPosition - firstPosition;
    uint32_t offset = eventsOffset + firstPosition;

    if (bufOffset + length > buffer.capacity()) {
        throw HipoException("buffer with offset " + to_string(bufOffset) +
                            " is smaller than the event.");
    }

    buffer.order(byteOrder);

    std::memcpy((void *)(buffer.array() + buffer.arrayOffset() + bufOffset),
                (const void *)(dataBuffer.array() + offset), length);
//    System.arraycopy(dataBuffer.array(), offset, buffer.array(),
//                     buffer.arrayOffset() + bufOffset, length);

    // Make buffer ready to read.
    // Always set limit first, else you can cause exception.
    buffer.limit(bufOffset + length).position(bufOffset);

    return buffer;
}


/**
 * Get the user header contained in this record and return it in an allocated array.
 * Caller must do delete[] on returned pointer!
 * @return the user header contained in this record, or null if none.
 */
uint8_t* RecordInput::getUserHeader() {

    uint32_t length = header.getUserHeaderLength();
    if (length < 1) {
        return nullptr;
    }

    auto userHeader = new uint8_t[length];

    std::memcpy((void *)(userHeader),
                (const void *)(dataBuffer.array() + userHeaderOffset), length);
//    System.arraycopy(dataBuffer.array(), userHeaderOffset, userHeader, 0, length);

    return userHeader;
}


/**
 * Get any existing user header and write it into the given byte buffer.
 * The given byte buffer must be large enough to contain user header.
 * Note that the buffer.limit() is ignored & reset.
 *
 * @param buffer    buffer to be filled with user header.
 * @param bufOffset offset into buffer to place user header.
 * @return buffer passed in (position = limit = bufOffset if no user header exists).
 * @throws HipoException if buffer has insufficient space to contain user header
 *                       (buffer.capacity() - bufOffset < user header size).
 */
ByteBuffer & RecordInput::getUserHeader(ByteBuffer & buffer, size_t bufOffset) {

    uint32_t length = header.getUserHeaderLength();

    if (length < 1) {
        buffer.limit(bufOffset).position(bufOffset);
        return buffer;
    }

    if (bufOffset + length > buffer.capacity()) {
        throw HipoException("buffer with offset " + to_string(bufOffset) +
                            " is smaller than the user header.");
    }

    buffer.order(byteOrder);

    std::memcpy((void *)(buffer.array() + bufOffset),
                (const void *)(dataBuffer.array() + userHeaderOffset), length);
//    System.arraycopy(dataBuffer.array(), userHeaderOffset, buffer.array(),
//                             buffer.arrayOffset() + bufOffset, length);

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
 * @return false if no user header exists and no parsing into record done, else true.
 * @throws HipoException if buffer has insufficient space to contain user header
 *                       (buffer.capacity() - bufOffset < user header size), or
 *                       if buffer not in hipo format.
 */
bool RecordInput::getUserHeaderAsRecord(ByteBuffer & buffer, size_t bufOffset,
                                        RecordInput & record) {

    // Read user header into given buffer, ready to read & with proper byte order.
    getUserHeader(buffer, bufOffset);
    // If there is no user header ...
    if (buffer.remaining() < 1) {
        return false;
    }

    // Parse user header into record
    record.setByteOrder(byteOrder);
    record.readRecord(buffer, bufOffset);
    return true;
}


/**
 * Reads record from the file at given position. Call this method or
 * {@link #readRecord(ByteBuffer, int)} before calling any other.
 * Any compressed data is decompressed. Memory is allocated as needed.
 * First the header is read, then the length of
 * the record is read from header, then
 * following bytes are read and decompressed.
 *
 * @param file opened file descriptor
 * @param position position in the file
 * @throws HipoException if file contains too little data,
 *                       if the input data was corrupted (including if the input data is
 *                       an incomplete stream),
 *                       is not in proper format, or version earlier than 6, or
 *                       error in uncompressing gzipped data.
 */
void RecordInput::readRecord(ifstream & file, size_t position) {

//    if (file == null || position < 0L) {
//        throw HipoException("bad argument(s)");
//    }

        //FileChannel channel = file.getChannel();

    // Read header
    if (!file.is_open()) {
        throw HipoException("file not open");
    }
    file.seekg(position);
    //channel.position(position);

    file.read(reinterpret_cast<char *>(headerBuffer.array()), RecordHeader::HEADER_SIZE_BYTES);
    //file.read(headerBuffer.array());

    // This will switch headerBuffer to proper byte order
    header.readHeader(headerBuffer);
    // Make sure all internal buffers have the same byte order
    setByteOrder(headerBuffer.order());

    int recordLengthWords = header.getLength();
    int headerLength      = header.getHeaderLength();
    int cLength           = header.getCompressedDataLength();

    // How many bytes will the expanded record take?
    // Just data:
    uncompressedEventsLength = 4*header.getDataLengthWords();
    // Everything except the header & don't forget padding:
    int neededSpace =   header.getIndexLength() +
                        4*header.getUserHeaderLengthWords() +
                        uncompressedEventsLength;

    // Handle rare situation in which compressed data takes up more room
    neededSpace = neededSpace < cLength ? cLength : neededSpace;

    // Make room to handle all data to be read & uncompressed
    if (dataBuffer.capacity() < neededSpace) {
        allocate(neededSpace);
    }
    dataBuffer.clear();

    // Go here to read rest of record
    file.seekg(position + headerLength);
    //channel.position(position + headerLength);

    // Decompress data
    switch (header.getCompressionType()) {
        case 1:
        case 2:
            // LZ4
            // Read compressed data
            file.read(reinterpret_cast<char *>(recordBuffer.array()), cLength);
            //file.read(recordBuffer.array(), 0, cLength);
            //file.read(recordBuffer.array(), 0, (recordLengthWords - headerLength));
            Compressor::getInstance().uncompressLZ4(recordBuffer, cLength, dataBuffer);
            break;

        case 3:
            // GZIP
            //file.read(recordBuffer.array(), 0, cLength);
            //uint8_t* unzipped = Compressor::getInstance().uncompressGZIP(recordBuffer.array(), 0, cLength);
            //dataBuffer.put(unzipped);
#ifdef USE_GZIP
        {
            file.read(reinterpret_cast<char *>(recordBuffer.array()), cLength);
            uint32_t uncompLen;
            uint8_t* ungzipped = Compressor::getInstance().uncompressGZIP(recordBuffer.array(), 0,
                                                                          cLength, &uncompLen);
            dataBuffer.put(ungzipped, 0, uncompLen);
            delete[] ungzipped;
        }
#endif
            break;

        case 0:
        default:
            // None
            // Read uncompressed data - rest of record
            file.read(reinterpret_cast<char *>(recordBuffer.array()), cLength);
            //file.read(dataBuffer.array(), 0, (recordLengthWords - headerLength));
    }

    // Number of entries in index
    nEntries = header.getEntries();
    // Offset from just past header to user header (past index)
    userHeaderOffset = nEntries*4;
    // Offset from just past header to data (past index + user header)
    eventsOffset = userHeaderOffset + header.getUserHeaderLengthWords()*4;

    // Overwrite event lengths with event offsets
    int event_pos = 0;
    for(int i = 0; i < nEntries; i++){
        int   size = dataBuffer.getInt(i*4);
        event_pos += size;
        dataBuffer.putInt(i*4, event_pos);
    }
}

/**
 * Reads a record from the buffer at the given offset. Call this method or
 * {@link #readRecord(ifstream &, size_t)} before calling any other.
 * Any compressed data is decompressed. Memory is allocated as needed.
 *
 * @param buffer buffer containing record data.
 * @param offset offset into buffer to beginning of record data.
 * @throws HipoException if buffer contains too little data,
 *                       is not in proper format, or version earlier than 6 or
 *                       error in uncompressing gzipped data.
 */
void RecordInput::readRecord(ByteBuffer & buffer, size_t offset) {

    // This will switch buffer to proper byte order
    header.readHeader(buffer, offset);

    // Make sure all internal buffers have the same byte order
    setByteOrder(buffer.order());

    uint32_t recordLengthBytes = header.getLength();
    uint32_t headerLength      = header.getHeaderLength();
    uint32_t cLength           = header.getCompressedDataLength();

    size_t compDataOffset = offset + headerLength;

    // How many bytes will the expanded record take?
    // Just data:
    uncompressedEventsLength = 4*header.getDataLengthWords();
    // Everything except the header & don't forget padding:
    uint32_t neededSpace =  header.getIndexLength() +
                            4*header.getUserHeaderLengthWords() +
                            uncompressedEventsLength;

    // Make room to handle all data to be read & uncompressed
    dataBuffer.clear();
    if (dataBuffer.capacity() < neededSpace) {
        allocate(neededSpace);
    }

    // Decompress data
    switch (header.getCompressionType()) {
        case 1:
        case 2:
            // Read LZ4 compressed data (WARNING: this does set limit on dataBuffer!)
            Compressor::getInstance().uncompressLZ4(buffer, compDataOffset, cLength, dataBuffer);
            break;

        case 3:
#ifdef USE_GZIP
            {
                // Read GZIP compressed data
                uint32_t uncompLen;
                buffer.limit(compDataOffset + cLength).position(compDataOffset);
                uint8_t* ungzipped = Compressor::getInstance().uncompressGZIP(buffer, &uncompLen);
                dataBuffer.put(ungzipped, 0, uncompLen);
                delete[] ungzipped;
            }
#endif
            break;

        case 0:
        default:
// TODO: See if we can avoid this unnecessary copy!
            // Read uncompressed data - rest of record
            int len = recordLengthBytes - headerLength;
            std::memcpy((void *)dataBuffer.array(),
                        (const void *)(buffer.array() + buffer.arrayOffset() + compDataOffset), len);
//                System.arraycopy(buffer.array(), buffer.arrayOffset() + compDataOffset,
//                                     dataBuffer.array(), 0, len);
    }

    // Number of entries in index
    nEntries = header.getEntries();
    // Offset from just past header to user header (past index)
    userHeaderOffset = nEntries*4;
    // Offset from just past header to data (past index + user header)
    eventsOffset = userHeaderOffset + header.getUserHeaderLengthWords()*4;

// TODO: How do we handle trailers???
    // Overwrite event lengths with event offsets
    int event_pos = 0;
    for(int i = 0; i < nEntries; i++){
        int   size = dataBuffer.getInt(i*4);
        event_pos += size;
        dataBuffer.putInt(i*4, event_pos);
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
 * @param header RecordHeader to be used to read the record header in srcBuf.
 * @return the original record size in srcBuf (bytes).
 * @throws HipoException if srcBuf contains too little data,
 *                       is not in proper format, or version earlier than 6.
 */
uint32_t RecordInput::uncompressRecord(ByteBuffer & srcBuf, size_t srcOff, ByteBuffer & dstBuf,
                                       RecordHeader & header) {

    size_t dstOff = dstBuf.position();

    // Read in header. This will switch srcBuf to proper byte order.
    header.readHeader(srcBuf, srcOff);
//cout << endl << "uncompressRecord: header --> " << endl << header.toString() << endl;

    uint32_t headerBytes              = header.getHeaderLength();
    uint32_t compressionType          = header.getCompressionType();
    uint32_t origRecordBytes          = header.getLength();
    uint32_t compressedDataLength     = header.getCompressedDataLength();
    uint32_t uncompressedRecordLength = header.getUncompressedRecordLength();

    size_t   compressedDataOffset = srcOff + headerBytes;
    uint32_t indexLen = header.getIndexLength();
    uint32_t userLen  = 4*header.getUserHeaderLengthWords();  // padded

    // Make sure destination buffer has the same byte order
    //dstBuf.order(srcBuf.order());

    if (compressionType != 0) {
        // Copy (uncompressed) general record header to destination buffer
        std::memcpy((void *)(dstBuf.array() + dstOff + dstBuf.arrayOffset()),
                    (const void *)(srcBuf.array() + srcOff + srcBuf.arrayOffset()), headerBytes);

//        System.arraycopy(srcBuf.array(), srcOff + srcBuf.arrayOffset(),
//                         dstBuf.array(), dstOff + dstBuf.arrayOffset(),
//                         headerBytes);

        dstBuf.position(dstOff + headerBytes);
    }
    else {
        // Since everything is uncompressed, copy it all over as is
        int copyBytes = indexLen + userLen + 4*header.getDataLengthWords();  // padded

        std::memcpy((void *)(dstBuf.array() + dstOff + dstBuf.arrayOffset()),
                    (const void *)(srcBuf.array() + srcOff + srcBuf.arrayOffset()), headerBytes + copyBytes);
//        System.arraycopy(srcBuf.array(), srcOff + srcBuf.arrayOffset(),
//                         dstBuf.array(), dstOff + dstBuf.arrayOffset(),
//                         headerBytes  + copyBytes);

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
                uint32_t uncompLen;
                srcBuf.limit(compressedDataOffset + compressedDataLength).position(compressedDataOffset);
                uint8_t* ungzipped = Compressor::getInstance().uncompressGZIP(srcBuf, &uncompLen);
                dstBuf.put(ungzipped, 0, uncompLen);
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
    header.setCompressionType(Compressor::UNCOMPRESSED).setCompressedDataLength(0);

    // Reset the header length
    dstBuf.putInt(dstOff + RecordHeader::RECORD_LENGTH_OFFSET, uncompressedRecordLength);
    header.setLength(uncompressedRecordLength);

//            // If there is an index, change lengths to event offsets
//            if (header.getIndexLength() > 0) {
//                // Number of entries in index
//                int nEntries = header.getEntries();
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
uint32_t RecordInput::getEntries() {return nEntries;}



/**
 * Prints on the screen the index array of the record.
 */
void RecordInput::showIndex() {
    for(int i = 0; i < nEntries; i++){
        cout << setw(3) << dataBuffer.getInt(i*4) << "  ";
    }
    cout << endl;
}

