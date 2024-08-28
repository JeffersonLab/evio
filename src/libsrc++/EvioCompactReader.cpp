//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "EvioCompactReader.h"


namespace evio {


    /**
     * Constructor for reading an event file.
     *
     * @param path the full path to the file that contains events.
     *             For writing event files, use an <code>EventWriter</code> object.
     * @param sync if true use mutex to make threadsafe.
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if path arg is empty
     */
    EvioCompactReader::EvioCompactReader(std::string const & path, bool sync) : synced(sync) {

        if (path.empty()) {
            throw EvioException("path is empty");
        }

        /** Object for reading file. */
        std::ifstream inStreamRandom;
        inStreamRandom.open(path, std::ios::binary);

        // Create buffer of size 32 bytes
        size_t bytesToRead = 32;
        ByteBuffer headerBuffer(bytesToRead);
        auto headerBytes = headerBuffer.array();

        // Read first 32 bytes of file header
        inStreamRandom.read(reinterpret_cast<char *>(headerBytes), bytesToRead);

        // Parse file header to find the file's endianness & evio version #
        evioVersion = Util::findEvioVersion(headerBuffer, initialPosition);
        byteOrder = headerBuffer.order();

        // This object is no longer needed
        inStreamRandom.close();

        if (evioVersion < 5) {
            reader = std::make_shared<EvioCompactReaderV4>(path);
        }
        else if (evioVersion == 6) {
            reader = std::make_shared<EvioCompactReaderV6>(path);
        }
        else {
            throw  EvioException("unsupported evio version (" + std::to_string(evioVersion) + ")");
        }
    }


    /**
     * Constructor for reading a buffer with option of removing synchronization
     * for much greater speed. The proper buffer format, in evio version 6,
     * is NOT the file format which contains a file header, but only the regular records.
     * The proper format in earlier versions is the same for files as it is for buffers -
     * with regular blocks.
     *
     * @param bb the buffer that contains events.
     * @param sync if true use mutex to make threadsafe.
     * @see EventWriter
     * @throws BufferUnderflowException if not enough buffer data;
     * @throws EvioException if buffer arg is null;
     *                       failure to parse first block header;
     *                       unsupported evio version.
     */
    EvioCompactReader::EvioCompactReader(std::shared_ptr<ByteBuffer> bb, bool sync) :
                            byteBuffer(bb), synced(sync) {

        initialPosition = byteBuffer->position();

        // Parse file header to find the buffer's endianness & evio version #
        evioVersion = Util::findEvioVersion(*(byteBuffer.get()), initialPosition);
        byteOrder = byteBuffer->order();

        if (evioVersion < 4)  {
            throw EvioException("unsupported evio version (" + std::to_string(evioVersion) + "), only 4+");
        }

        if (evioVersion == 4) {
            reader = std::make_shared<EvioCompactReaderV4>(byteBuffer);
        }
        else if (evioVersion == 6) {
            reader = std::make_shared<EvioCompactReaderV6>(byteBuffer);
        }
        else {
            throw EvioException("unsupported evio version (" + std::to_string(evioVersion) + ")");
        }
    }


    /** {@inheritDoc} */
    bool EvioCompactReader::isFile() {return reader->isFile();}


    /** {@inheritDoc} */
    bool EvioCompactReader::isCompressed() {return reader->isCompressed();}


    /** {@inheritDoc} */
    void EvioCompactReader::setBuffer(std::shared_ptr<ByteBuffer> buf) {reader->setBuffer(buf);}


    /** {@inheritDoc} */
    bool EvioCompactReader::isClosed() {
        if (synced) {
            auto lock = std::unique_lock<std::recursive_mutex>(mtx);
            return reader->isClosed();
        }
        return reader->isClosed();
    }


    /** {@inheritDoc} */
    ByteOrder EvioCompactReader::getByteOrder() {return reader->getByteOrder();}


    /** {@inheritDoc} */
    uint32_t EvioCompactReader::getEvioVersion() {return evioVersion;}


    /** {@inheritDoc} */
    std::string EvioCompactReader::getPath() {return reader->getPath();}


    /** {@inheritDoc} */
    ByteOrder EvioCompactReader::getFileByteOrder() {return reader->getFileByteOrder();}


    /** {@inheritDoc} */
    std::string EvioCompactReader::getDictionaryXML() {
        if (synced) {
            auto lock = std::unique_lock<std::recursive_mutex>(mtx);
            return reader->getDictionaryXML();
        }
        return reader->getDictionaryXML();
    }


    /** {@inheritDoc} */
    std::shared_ptr<EvioXMLDictionary> EvioCompactReader::getDictionary() {
        if (synced) {
            auto lock = std::unique_lock<std::recursive_mutex>(mtx);
            return reader->getDictionary();
        }
        return reader->getDictionary();
    }


    /** {@inheritDoc} */
    bool EvioCompactReader::hasDictionary() {return reader->hasDictionary();}


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReader::getByteBuffer() {return reader->getByteBuffer();}


    /** {@inheritDoc} */
    size_t EvioCompactReader::fileSize() {return reader->fileSize();}


    /** {@inheritDoc} */
    std::shared_ptr<EvioNode> EvioCompactReader::getEvent(size_t eventNumber) {
        return reader->getEvent(eventNumber);
    }


    /** {@inheritDoc} */
    std::shared_ptr<EvioNode> EvioCompactReader::getScannedEvent(size_t eventNumber) {
        return reader->getScannedEvent(eventNumber);
    }


    /** {@inheritDoc} */
    std::shared_ptr<IBlockHeader> EvioCompactReader::getFirstBlockHeader() {
        return reader->getFirstBlockHeader();
    }


    /** {@inheritDoc} */
    void EvioCompactReader::searchEvent(size_t evNumber, uint16_t tag, uint8_t num,
                                        std::vector<std::shared_ptr<EvioNode>> & vec) {
        if (synced) {
            auto lock = std::unique_lock<std::recursive_mutex>(mtx);
            return reader->searchEvent(evNumber, tag, num, vec);
        }
        return reader->searchEvent(evNumber, tag, num, vec);
    }


    /** {@inheritDoc} */
    void EvioCompactReader::searchEvent(size_t eventNumber, std::string const & dictName,
                                        std::shared_ptr<EvioXMLDictionary> dictionary,
                                        std::vector<std::shared_ptr<EvioNode>> & vec) {
        if (synced) {
            auto lock = std::unique_lock<std::recursive_mutex>(mtx);
            return reader->searchEvent(eventNumber, dictName, dictionary, vec);
        }
        return reader->searchEvent(eventNumber, dictName, dictionary, vec);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReader::removeEvent(size_t eventNumber) {
        if (synced) {
            auto lock = std::unique_lock<std::recursive_mutex>(mtx);
            return reader->removeEvent(eventNumber);
        }
        return reader->removeEvent(eventNumber);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReader::removeStructure(std::shared_ptr<EvioNode> removeNode) {
        if (synced) {
            auto lock = std::unique_lock<std::recursive_mutex>(mtx);
            return reader->removeStructure(removeNode);
        }
        return reader->removeStructure(removeNode);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReader::addStructure(size_t eventNumber, ByteBuffer & addBuffer) {
        if (synced) {
            auto lock = std::unique_lock<std::recursive_mutex>(mtx);
            return reader->addStructure(eventNumber, addBuffer);
        }
        return reader->addStructure(eventNumber, addBuffer);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReader::getData(std::shared_ptr<EvioNode> node) {
        if (synced) {
            auto lock = std::unique_lock<std::recursive_mutex>(mtx);
            return reader->getData(node);
        }
        return reader->getData(node);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReader::getData(std::shared_ptr<EvioNode> node,
                                                           bool copy) {
        if (synced) {
            auto lock = std::unique_lock<std::recursive_mutex>(mtx);
            return reader->getData(node, copy);
        }
        return reader->getData(node, copy);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReader::getData(std::shared_ptr<EvioNode> node,
                                                           std::shared_ptr<ByteBuffer> buf) {
        if (synced) {
            auto lock = std::unique_lock<std::recursive_mutex>(mtx);
            return reader->getData(node, buf);
        }
        return reader->getData(node, buf);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReader::getData(std::shared_ptr<EvioNode> node,
                                                           std::shared_ptr<ByteBuffer> buf,
                                                           bool copy) {
        if (synced) {
            auto lock = std::unique_lock<std::recursive_mutex>(mtx);
            return reader->getData(node, buf, copy);
        }
        return reader->getData(node, buf, copy);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReader::getEventBuffer(size_t eventNumber) {
        if (synced) {
            auto lock = std::unique_lock<std::recursive_mutex>(mtx);
            return reader->getEventBuffer(eventNumber);
        }
        return reader->getEventBuffer(eventNumber);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReader::getEventBuffer(size_t eventNumber, bool copy) {
        if (synced) {
            auto lock = std::unique_lock<std::recursive_mutex>(mtx);
            return reader->getEventBuffer(eventNumber, copy);
        }
        return reader->getEventBuffer(eventNumber, copy);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReader::getStructureBuffer(std::shared_ptr<EvioNode> node) {
        if (synced) {
            auto lock = std::unique_lock<std::recursive_mutex>(mtx);
            return reader->getStructureBuffer(node);
        }
        return reader->getStructureBuffer(node);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReader::getStructureBuffer(std::shared_ptr<EvioNode> node, bool copy) {
        if (synced) {
            auto lock = std::unique_lock<std::recursive_mutex>(mtx);
            return reader->getStructureBuffer(node, copy);
        }
        return reader->getStructureBuffer(node, copy);
    }


    /** {@inheritDoc} */
    void EvioCompactReader::close() {
        if (synced) {
            auto lock = std::unique_lock<std::recursive_mutex>(mtx);
            reader->close();
        }
        reader->close();
    }


    /** {@inheritDoc} */
    uint32_t EvioCompactReader::getEventCount() {return reader->getEventCount();}


    /** {@inheritDoc} */
    uint32_t EvioCompactReader::getBlockCount() {return reader->getBlockCount();}


    /** {@inheritDoc} */
    void EvioCompactReader::toFile(std::string const & fileName) {
        if (synced) {
            auto lock = std::unique_lock<std::recursive_mutex>(mtx);
            reader->toFile(fileName);
        }
        reader->toFile(fileName);
    }


}