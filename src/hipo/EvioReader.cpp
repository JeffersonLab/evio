//
// Created by Carl Timmer on 6/8/20.
//

#include "EvioReader.h"

namespace evio {


//------------------------------------------
//   FILE
//------------------------------------------


/**
 * Constructor for reading an event file.
 * Do <b>not</b> set sequential to false for remote files.
 *
 * @param path the full path to the file that contains events.
 *             For writing event files, use an <code>EventWriter</code> object.
 * @param checkRecNumSeq if <code>true</code> check the block number sequence
 *                       and throw an exception if it is not sequential starting
 *                       with 1
 * @param sequential     if <code>true</code> read the file sequentially,
 *                       else use memory mapped buffers. If file &gt; 2.1 GB,
 *                       reads are always sequential for the older evio format.
 * @param synced         if true, methods are synchronized for thread safety, else false.
 *
 * @see EventWriter
 * @throws IOException   if read failure
 * @throws EvioException if path is empty; bad evio version #;
 *                       if file is too small to have valid evio format data;
 *                       if first block number != 1 when checkBlkNumSeq arg is true
 */
EvioReader::EvioReader(string const & path, bool checkRecNumSeq, bool sequential, bool synced) {

        if (path.empty()) {
            throw EvioException("path is empty");
        }

        /** Object for reading file. */
        ifstream inStreamRandom;
        inStreamRandom.open(path, std::ios::binary);

        initialPosition = 0;

        // Create buffer of size 32 bytes
        size_t bytesToRead = 32;
        ByteBuffer headerBuffer(bytesToRead);
        byteBuffer = headerBuffer;
        auto headerBytes = headerBuffer.array();

        // Read first 32 bytes of file header
        inStreamRandom.read(reinterpret_cast<char *>(headerBytes), bytesToRead);

        // Parse file header to find the file's endianness & evio version #
        if (findEvioVersion() != EvioReader::ReadStatus::SUCCESS) {
            throw EvioException("Failed reading first block header");
        }

        // This object is no longer needed
        inStreamRandom.close();

        if (evioVersion > 0 && evioVersion < 5) {
            if (synced) {
                reader = std::make_shared<IEvioReader>(new EvioReaderV4(path, checkRecNumSeq, sequential));
            }
            else {
                reader = std::make_shared<IEvioReader>(new EvioReaderUnsyncV4(path, checkRecNumSeq, sequential));
            }
        }
        else if (evioVersion == 6) {
            if (synced) {
                reader = std::make_shared<IEvioReader>(new EvioReaderV6(path, checkRecNumSeq));
            }
            else {
                reader = std::make_shared<IEvioReader>(new EvioReaderUnsyncV6(path, checkRecNumSeq));
            }
        }
        else {
            throw EvioException("unsupported evio version (" + std::to_string(evioVersion) + ")");
        }
}


//------------------------------------------
//   BUFFER
//------------------------------------------


/**
 * Constructor for reading a buffer with option of removing synchronization
 * for much greater speed.
 * @param byteBuffer     the buffer that contains events.
 * @param checkRecNumSeq if <code>true</code> check the record number sequence
 *                       and throw an exception if it is not sequential starting
 *                       with 1
 * @param synced         if true, methods are synchronized for thread safety, else false.
 * @see EventWriter
 * @throws EvioException if buffer arg is null; bad evio version #;
 *                       failure to read first block header
 */
EvioReader::EvioReader(ByteBuffer byteBuffer, bool checkRecNumSeq, bool synced) {

        if (byteBuffer == null) {
            throw EvioException("Buffer arg is null");
        }

        this.byteBuffer = byteBuffer.slice(); // remove necessity to track initial position
        initialPosition = byteBuffer.position();

        // Read first block header and find the file's endianness & evio version #.
        if (findEvioVersion() != EvioReader::ReadStatus::SUCCESS) {
            throw EvioException("Failed reading first record header");
        }

        if (evioVersion > 0 && evioVersion < 5) {
            if (synced) {
                reader = new EvioReaderV4(byteBuffer, checkRecNumSeq);
            }
            else {
                reader = new EvioReaderUnsyncV4(byteBuffer, checkRecNumSeq);
            }
        }
        else if (evioVersion == 6) {
            if (synced) {
                reader = new EvioReaderV6(byteBuffer, checkRecNumSeq);
            }
            else {
                reader = new EvioReaderUnsyncV6(byteBuffer, checkRecNumSeq);
            }
        }
        else {
            throw EvioException("unsupported evio version (" + std::to_string(evioVersion) + ")");
        }
}


/** {@inheritDoc} */
void EvioReader::setBuffer(ByteBuffer & buf) {reader->setBuffer(buf);}

/** {@inheritDoc} */
/*synchronized*/ bool EvioReader::isClosed() {return reader->isClosed();}

/** {@inheritDoc} */
bool EvioReader::checkBlockNumberSequence() {return reader->checkBlockNumberSequence();}

/** {@inheritDoc} */
ByteOrder & EvioReader::getByteOrder() {return reader->getByteOrder();}

/** {@inheritDoc} */
uint32_t EvioReader::getEvioVersion() {return evioVersion;}

/** {@inheritDoc} */
string EvioReader::getPath() {return reader->getPath();}

/** {@inheritDoc} */
std::shared_ptr<EventParser> & EvioReader::getParser() {return reader->getParser();}

/** {@inheritDoc} */
void EvioReader::setParser(std::shared_ptr<EventParser> & parser) {reader->setParser(parser);}

/** {@inheritDoc} */
string EvioReader::getDictionaryXML() {return reader->getDictionaryXML();}

/** {@inheritDoc} */
bool EvioReader::hasDictionaryXML() {return reader->hasDictionaryXML();}

/** {@inheritDoc} */
size_t EvioReader::getNumEventsRemaining() {return reader->getNumEventsRemaining();}

/** {@inheritDoc} */
ByteBuffer & EvioReader::getByteBuffer() {return reader->getByteBuffer();}

/** {@inheritDoc} */
size_t EvioReader::fileSize() {return reader->fileSize();}

/** {@inheritDoc} */
std::shared_ptr<IBlockHeader> EvioReader::getFirstBlockHeader() {return reader->getFirstBlockHeader();}

/**
 * Reads a couple things in the first block (physical record) header
 * in order to determine the evio version of buffer.
 * @return status of read attempt
 */
EvioReader::ReadStatus EvioReader::findEvioVersion() {
    // Look at first record header

    // Have enough remaining bytes to read 8 words of header?
    if (byteBuffer.limit() - initialPosition < 32) {
        return EvioReader::ReadStatus::END_OF_FILE;
    }

    // Set the byte order to match the file's ordering.

    // Check the magic number for endianness (buffer defaults to big endian)
    byteOrder = byteBuffer.order();

    // Offset to magic # is in the SAME LOCATION FOR ALL EVIO VERSIONS
    int magicNumber = byteBuffer.getInt(initialPosition + RecordHeader::MAGIC_OFFSET);
    if (magicNumber != IBlockHeader::MAGIC_NUMBER) {
        if (byteOrder == ByteOrder::ENDIAN_BIG) {
            byteOrder = ByteOrder::ENDIAN_LITTLE;
        }
        else {
            byteOrder = ByteOrder::ENDIAN_BIG;
        }
        byteBuffer.order(byteOrder);

        // Reread magic number to make sure things are OK
        magicNumber = byteBuffer.getInt(initialPosition + RecordHeader::MAGIC_OFFSET);
        if (magicNumber != IBlockHeader::MAGIC_NUMBER) {
            return EvioReader::ReadStatus::EVIO_EXCEPTION;
        }
    }

    // Find the version number, again, SAME LOCATION FOR ALL EVIO VERSIONS
    uint32_t bitInfo = byteBuffer.getUInt(initialPosition + RecordHeader::BIT_INFO_OFFSET);
    evioVersion = bitInfo & RecordHeader::VERSION_MASK;

    return EvioReader::ReadStatus::SUCCESS;
}


/** {@inheritDoc} */
std::shared_ptr<EvioEvent> EvioReader::getEvent(size_t index) {return reader->getEvent(index);}


/** {@inheritDoc} */
std::shared_ptr<EvioEvent> EvioReader::parseEvent(int index) {return reader->>parseEvent(index);}


/** {@inheritDoc} */
std::shared_ptr<EvioEvent> EvioReader::nextEvent() {return reader->nextEvent();}


/** {@inheritDoc} */
std::shared_ptr<EvioEvent> EvioReader::parseNextEvent() {return reader->parseNextEvent();}


/** {@inheritDoc} */
void EvioReader::parseEvent(std::shared_ptr<EvioEvent> evioEvent) {reader->parseEvent(evioEvent);}


/**
 * Transform an event in the form of a byte array into an EvioEvent object.
 * However, only top level header is parsed. Most users will want to call
 * {@link #parseEvent(byte[], int, ByteOrder)} instead since it returns a
 * fully parsed event.
 * Byte array must not be in file format (have record headers),
 * but must consist of only the bytes comprising the evio event.
 *
 * @param array   array to parse into EvioEvent object.
 * @param offset  offset into array.
 * @param order   byte order to use.
 * @return an EvioEvent object parsed from the given array.
 * @throws EvioException if null arg, too little data, length too large, or data not in evio format.
 */
/*static*/ /*std::shared_ptr<EvioEvent> EvioReader::getEvent(byte[] array, int offset, ByteOrder order) { */
std::shared_ptr<EvioEvent> getEvent(uint8_t * dest, size_t len, ByteOrder const & order) {

    if (array == null || array.length - offset < 8) {
        throw EvioException("arg null or too little data");
    }

    int byteLen = array.length - offset;
    EvioEvent event = new EvioEvent();
    BaseStructureHeader header = event.getHeader();

    // Read the first header word - the length in 32bit words
    int wordLen   = ByteDataTransformer.toInt(array, order, offset);
    int dataBytes = 4*(wordLen - 1);
    if (wordLen < 1) {
        throw EvioException("non-positive length (0x" + Integer.toHexString(wordLen) + ")");
    }
    else if (dataBytes > byteLen) {
        // Protect against too large length
        throw EvioException("bank length too large (needed " + dataBytes +
                            " but have " + byteLen + " bytes)");
    }
    header.setLength(wordLen);

    // Read and parse second header word
    int word = ByteDataTransformer.toInt(array, order, offset+4);
    header.setTag(word >>> 16);
    int dt = (word >> 8) & 0xff;
    header.setDataType(dt & 0x3f);
    header.setPadding(dt >>> 6);
    header.setNumber(word & 0xff);

    // Set the raw data
    byte data[] = new byte[dataBytes];
    System.arraycopy(array, offset+8, data, 0, dataBytes);
    event.setRawBytes(data);
    event.setByteOrder(order);

    return event;
}

/**
 * Completely parse the given byte array into an EvioEvent.
 * Byte array must not be in file format (have record headers),
 * but must consist of only the bytes comprising the evio event.
 *
 * @param array   array to parse into EvioEvent object.
 * @param offset  offset into array.
 * @param order   byte order to use.
 * @return the EvioEvent object parsed from the given bytes.
 * @throws EvioException if null arg, too little data, length too large, or data not in evio format.
 */
/*static*/ std::shared_ptr<EvioEvent> EvioReader::parseEvent(byte[] array, int offset, ByteOrder order) {
        EvioEvent event = EvioReader.getEvent(array, offset, order);
        EventParser.eventParse(event);
        return event;
}


/** {@inheritDoc} */
std::vector<uint8_t> EvioReader::getEventArray(size_t eventNumber) {return reader->getEventArray(eventNumber);}

/** {@inheritDoc} */
ByteBuffer & EvioReader::getEventBuffer(int eventNumber) {return reader->getEventBuffer(eventNumber);}

/** {@inheritDoc} */
void EvioReader::rewind() {reader->rewind();}

/** {@inheritDoc} */
long EvioReader::position() {return reader->position();}

/** {@inheritDoc} */
void EvioReader::close() {reader->close();}

/** {@inheritDoc} */
std::shared_ptr<IBlockHeader> EvioReader::getCurrentBlockHeader() {return reader->getCurrentBlockHeader();}

/** {@inheritDoc} */
std::shared_ptr<EvioEvent> EvioReader::gotoEventNumber(size_t evNumber) {
    return reader->gotoEventNumber(evNumber);
}

/** {@inheritDoc} */
size_t EvioReader::getEventCount() {return reader->getEventCount();}

/** {@inheritDoc} */
size_t EvioReader::getBlockCount() {return reader->getBlockCount();}

}