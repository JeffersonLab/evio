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
        auto headerBytes = headerBuffer.array();

        // Read first 32 bytes of file header
        inStreamRandom.read(reinterpret_cast<char *>(headerBytes), bytesToRead);

        // Parse file header to find the file's endianness & evio version #
        if (findEvioVersion(headerBuffer) != EvioReader::ReadWriteStatus::SUCCESS) {
            throw EvioException("Failed reading first block header");
        }

        // This object is no longer needed
        inStreamRandom.close();

        if (evioVersion > 0 && evioVersion < 5) {
            reader = std::make_shared<EvioReaderV4>(path, checkRecNumSeq, synced);
        }
        else if (evioVersion == 6) {
            reader = std::make_shared<EvioReaderV6>(path, checkRecNumSeq, synced);
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
EvioReader::EvioReader(std::shared_ptr<ByteBuffer> & bb, bool checkRecNumSeq, bool synced) {

        byteBuffer = bb->slice(); // remove necessity to track initial position
        initialPosition = byteBuffer->position();

        // Read first block header and find the file's endianness & evio version #.
        if (findEvioVersion(*(byteBuffer.get())) != EvioReader::ReadWriteStatus::SUCCESS) {
            throw EvioException("Failed reading first record header");
        }

        if (evioVersion > 0 && evioVersion < 5) {
            reader = std::make_shared<EvioReaderV4>(byteBuffer, checkRecNumSeq, synced);
        }
        else if (evioVersion == 6) {
            reader = std::make_shared<EvioReaderV6>(byteBuffer, checkRecNumSeq, synced);
        }
        else {
            throw EvioException("unsupported evio version (" + std::to_string(evioVersion) + ")");
        }
    }


    /** {@inheritDoc} */
    void EvioReader::setBuffer(std::shared_ptr<ByteBuffer> & buf) {reader->setBuffer(buf);}

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
    std::shared_ptr<ByteBuffer> EvioReader::getByteBuffer() {return reader->getByteBuffer();}

    /** {@inheritDoc} */
    size_t EvioReader::fileSize() {return reader->fileSize();}

    /** {@inheritDoc} */
    std::shared_ptr<IBlockHeader> EvioReader::getFirstBlockHeader() {return reader->getFirstBlockHeader();}

    /**
     * Reads a couple things in the first block (physical record) header
     * in order to determine the evio version of buffer.
     * @param bb ByteBuffer to read from.
     * @return status of read attempt.
     */
    EvioReader::ReadWriteStatus EvioReader::findEvioVersion(ByteBuffer & bb) {
        // Look at first record header

        // Have enough remaining bytes to read 8 words of header?
        if (bb.limit() - initialPosition < 32) {
            return EvioReader::ReadWriteStatus::END_OF_FILE;
        }

        // Set the byte order to match the file's ordering.

        // Check the magic number for endianness (buffer defaults to big endian)
        byteOrder = bb.order();

        // Offset to magic # is in the SAME LOCATION FOR ALL EVIO VERSIONS
        int magicNumber = bb.getInt(initialPosition + RecordHeader::MAGIC_OFFSET);
        if (magicNumber != IBlockHeader::MAGIC_NUMBER) {
            if (byteOrder == ByteOrder::ENDIAN_BIG) {
                byteOrder = ByteOrder::ENDIAN_LITTLE;
            }
            else {
                byteOrder = ByteOrder::ENDIAN_BIG;
            }
            bb.order(byteOrder);

            // Reread magic number to make sure things are OK
            magicNumber = bb.getInt(initialPosition + RecordHeader::MAGIC_OFFSET);
            if (magicNumber != IBlockHeader::MAGIC_NUMBER) {
                return EvioReader::ReadWriteStatus::EVIO_EXCEPTION;
            }
        }

        // Find the version number, again, SAME LOCATION FOR ALL EVIO VERSIONS
        uint32_t bitInfo = bb.getUInt(initialPosition + RecordHeader::BIT_INFO_OFFSET);
        evioVersion = bitInfo & RecordHeader::VERSION_MASK;

        return EvioReader::ReadWriteStatus::SUCCESS;
    }


    /** {@inheritDoc} */
    std::shared_ptr<EvioEvent> EvioReader::getEvent(size_t index) {return reader->getEvent(index);}


    /** {@inheritDoc} */
    std::shared_ptr<EvioEvent> EvioReader::parseEvent(size_t index) {return reader->parseEvent(index);}


    /** {@inheritDoc} */
    std::shared_ptr<EvioEvent> EvioReader::nextEvent() {return reader->nextEvent();}


    /** {@inheritDoc} */
    std::shared_ptr<EvioEvent> EvioReader::parseNextEvent() {return reader->parseNextEvent();}


    /** {@inheritDoc} */
    void EvioReader::parseEvent(std::shared_ptr<EvioEvent> evioEvent) {reader->parseEvent(evioEvent);}

// Static

    /**
     * Transform an event in the form of a byte array into an EvioEvent object.
     * However, only top level header is parsed. Most users will want to call
     * {@link #parseEvent(byte[], int, ByteOrder)} instead since it returns a
     * fully parsed event.
     * Byte array must not be in file format (have record headers),
     * but must consist of only the bytes comprising the evio event.
     *
     * @param src   array to parse into EvioEvent object.
     * @param maxLen  max length of valid data in src.
     * @param order   byte order to use.
     * @return an EvioEvent object parsed from the given array.
     * @throws EvioException if null arg, too little data, length too large, or data not in evio format.
     */
    std::shared_ptr<EvioEvent> EvioReader::getEvent(uint8_t * src, size_t maxLen, ByteOrder const & order) {

        if (src == nullptr || maxLen < 8) {
            throw EvioException("arg null or too little data");
        }

        std::shared_ptr<BankHeader> header;
        auto event = EvioEvent::getInstance(header);

        // Read the first header word - the length in 32bit words
        uint32_t wordLen = Util::toInt(src, order);
        uint32_t dataBytes = 4*(wordLen - 1);
        if (dataBytes > maxLen) {
            // Protect against too large length
            throw EvioException("bank length too large (needed " + std::to_string(dataBytes) +
                                " but have " + std::to_string(maxLen) + " bytes)");
        }
        header->setLength(wordLen);

        // Read and parse second header word
        int word = Util::toInt(src + 4, order);
        header->setTag((word >> 16) & 0xfff);
        int dt = (word >> 8) & 0xff;
        header->setDataType(dt & 0x3f);
        header->setPadding(dt >> 6);
        header->setNumber(word & 0xff);

        // Set the raw data
        event->setRawBytes(src+8, dataBytes);
        event->setByteOrder(order);

        return event;
    }

// Static

    /**
     * Completely parse the given byte array into an EvioEvent.
     * Byte array must not be in file format (have record headers),
     * but must consist of only the bytes comprising the evio event.
     *
     * @param src   array to parse into EvioEvent object.
     * @param maxLen  max length of valid data in src.
     * @param order   byte order to use.
     * @return the EvioEvent object parsed from the given bytes.
     * @throws EvioException if null arg, too little data, length too large, or data not in evio format.
     */
    std::shared_ptr<EvioEvent> EvioReader::parseEvent(uint8_t * src, size_t len, ByteOrder const & order) {
        auto event = EvioReader::getEvent(src, len, order);
        EventParser::eventParse(event);
        return event;
    }


    /** {@inheritDoc} */
    uint32_t EvioReader::getEventArray(size_t evNumber, std::vector<uint8_t> & vec) {
        return reader->getEventArray(evNumber, vec);
    }

    /** {@inheritDoc} */
    uint32_t EvioReader::getEventBuffer(size_t evNumber, ByteBuffer & buf)  {
        return reader->getEventBuffer(evNumber, buf);
    }

    /** {@inheritDoc} */
    void EvioReader::rewind() {reader->rewind();}

    /** {@inheritDoc} */
    size_t EvioReader::position() {return reader->position();}

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