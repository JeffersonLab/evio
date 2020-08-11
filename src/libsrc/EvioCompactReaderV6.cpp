//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "EvioCompactReaderV6.h"


namespace evio {


    /**
     * Constructor for reading a file.
     * @param fileName the name of the file that contains events.
     * @throws EvioException if file arg is empty,
     *                       file not in the proper format.
     */
    EvioCompactReaderV6::EvioCompactReaderV6(std::string const & fileName) {
        if (fileName.empty()) {
            throw EvioException("Buffer arg is emptyl");
        }
        path = fileName;
        reader.open(fileName);
    }


    /**
     * Constructor for reading a buffer.
     * @param byteBuffer the buffer that contains events.
     * @throws EvioException if buffer arg is null,
     *                       buffer too small,
     *                       buffer not in the proper format,
     *                       or earlier than version 6.
     */
    EvioCompactReaderV6::EvioCompactReaderV6(std::shared_ptr<ByteBuffer> & byteBuffer) {
        if (byteBuffer == nullptr) {
            throw EvioException("Buffer arg is null");
        }
        reader.setBuffer(byteBuffer);
    }


    /** {@inheritDoc} */
    void EvioCompactReaderV6::setBuffer(std::shared_ptr<ByteBuffer> & buf) {
        reader.setBuffer(buf);
        dictionary = nullptr;
        closed = false;
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReaderV6::setCompressedBuffer(std::shared_ptr<ByteBuffer> & buf) {
        dictionary = nullptr;
        closed = false;
        return reader.setCompressedBuffer(buf);
    }


    /** {@inheritDoc} */
    bool EvioCompactReaderV6::isFile() {return reader.isFile();}


    /** {@inheritDoc} */
    bool EvioCompactReaderV6::isCompressed() {return reader.isCompressed();}


    /** {@inheritDoc} */
    bool EvioCompactReaderV6::isClosed() {return closed;}


    /** {@inheritDoc} */
    ByteOrder EvioCompactReaderV6::getByteOrder() {return reader.getByteOrder();}


    /** {@inheritDoc} */
    uint32_t EvioCompactReaderV6::getEvioVersion() {return reader.getVersion();}


    /** {@inheritDoc} */
    std::string EvioCompactReaderV6::getPath() {return path;}


    /** {@inheritDoc} */
    ByteOrder EvioCompactReaderV6::getFileByteOrder() {return reader.getByteOrder();}


    /** {@inheritDoc} */
    std::string EvioCompactReaderV6::getDictionaryXML() {return reader.getDictionary();}


    /** {@inheritDoc} */
    std::shared_ptr<EvioXMLDictionary>  EvioCompactReaderV6::getDictionary() {
        if (dictionary != nullptr) return dictionary;

        if (closed) {
            throw EvioException("object closed");
        }

        std::string dictXML = reader.getDictionary();
        if (!dictXML.empty()) {
            dictionary = std::make_shared<EvioXMLDictionary>(dictXML);
        }

        return dictionary;
    }


    /** {@inheritDoc} */
    bool EvioCompactReaderV6::hasDictionary() {return reader.hasDictionary();}


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReaderV6::getByteBuffer() {return reader.getBuffer();}


    /** {@inheritDoc} */
    size_t EvioCompactReaderV6::fileSize() {return reader.getFileSize();}


    /** {@inheritDoc} */
    std::shared_ptr<EvioNode> EvioCompactReaderV6::getEvent(size_t eventNumber) {
        try {
            return reader.getEventNode(eventNumber);
        }
        catch (std::out_of_range & e) { }
        return nullptr;
    }


    /** {@inheritDoc} */
    std::shared_ptr<EvioNode> EvioCompactReaderV6::getScannedEvent(size_t eventNumber) {
        try {
            return scanStructure(eventNumber);
        }
        catch (std::out_of_range & e) { }
        return nullptr;
    }


    /** {@inheritDoc} */
    std::shared_ptr<IBlockHeader> EvioCompactReaderV6::getFirstBlockHeader() {
        return reader.getFirstRecordHeader();
    }


    /**
     * This method scans the given event number in the buffer.
     * It returns an EvioNode object representing the event.
     * All the event's substructures, as EvioNode objects, are
     * contained in the node.allNodes list (including the event itself).
     *
     * @param eventNumber number of the event to be scanned starting at 1
     * @return the EvioNode object corresponding to the given event number,
     *         or null if eventNumber is out of bounds, reading a file or data is
     *         compressed.
     */
    std::shared_ptr<EvioNode> EvioCompactReaderV6::scanStructure(size_t eventNumber) {

        // Node corresponding to event
        auto node = reader.getEventNode(eventNumber - 1);
        if (node == nullptr) return nullptr;

        if (node->getScanned()) {
            node->clearLists();
        }

        // Do this before actual scan so clone() sets all "scanned" fields
        // of child nodes to "true" as well.
        node->scanned = true;
        EvioNode::scanStructure(node);
        return node;
    }


    /** {@inheritDoc} */
    void EvioCompactReaderV6::searchEvent(size_t eventNumber, uint16_t tag, uint8_t num,
                                          std::vector<std::shared_ptr<EvioNode>> & vec) {
        // check args
        if (eventNumber < 1 || eventNumber > reader.getEventCount()) {
            throw EvioException("bad arg value(s)");
        }

        if (closed) {
            throw EvioException("object closed");
        }

        vec.clear();

        // Scan the node
        auto node = scanStructure(eventNumber);
        if (node == nullptr) {
            return;
        }

        auto list = node->allNodes;
//        std::cout << "searchEvent: ev# = " << eventNumber << ", list size = " << list.size() <<
//                     " for tag/num = " << tag << "/" << num << std::endl;

        // Now look for matches in this event
        for (auto & enode: list) {
//            std::cout << "searchEvent: desired tag = " << tag << " found " << enode->tag << std::endl;
//            std::cout << "           : desired num = " << num << " found " << enode->num << std::endl;
            if (enode->tag == tag && enode->num == num) {
//                std::cout << "           : found node at pos = " << enode->pos + " len = " << enode->len << std::endl;
                vec.push_back(enode);
            }
        }
    }


    /** {@inheritDoc} */
    void EvioCompactReaderV6::searchEvent(size_t eventNumber, std::string const & dictName,
                                          std::shared_ptr<EvioXMLDictionary> & dictionary,
                                          std::vector<std::shared_ptr<EvioNode>> & vec) {

        if (dictName.empty()) {
            throw EvioException("empty dictionary entry name");
        }

        if (closed) {
            throw EvioException("object closed");
        }

        // If no dictionary is specified, use the one provided with the
        // file/buffer. If that does not exist, throw an exception.
        uint16_t tag;
        uint8_t  num;

        if (dictionary == nullptr && hasDictionary())  {
            dictionary = getDictionary();
        }

        if (dictionary != nullptr) {
            if (!dictionary->getTag(dictName, &tag)) {
                throw EvioException("no dictionary entry for " + dictName);
            }
            dictionary->getNum(dictName, &num);
        }
        else {
            throw EvioException("no dictionary available");
        }

        return searchEvent(eventNumber, tag, num, vec);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReaderV6::removeEvent(size_t eventNumber) {

        if (eventNumber < 1) {
            throw EvioException("event number must be > 0");
        }

        if (closed) {
            throw EvioException("object closed");
        }

        std::shared_ptr<EvioNode> eventNode;
        try {
            eventNode = reader.getEventNode(eventNumber - 1);
        }
        catch (std::out_of_range & e) {
            throw EvioException("event " + std::to_string(eventNumber) + " does not exist");
        }

        return removeStructure(eventNode);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReaderV6::removeStructure(std::shared_ptr<EvioNode> & removeNode) {
        return reader.removeStructure(removeNode);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReaderV6::addStructure(size_t eventNumber, ByteBuffer & addBuffer) {
        return reader.addStructure(eventNumber, addBuffer);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReaderV6::getData(std::shared_ptr<EvioNode> & node) {
        return getData(node, false);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReaderV6::getData(std::shared_ptr<EvioNode> & node,
                                                             bool copy) {
        auto buff = std::make_shared<ByteBuffer>(4*node->getDataLength());
        return node->getByteData(buff, copy);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReaderV6::getData(std::shared_ptr<EvioNode> & node,
                                                             std::shared_ptr<ByteBuffer> & buf)  {
        return getData(node, buf, false);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReaderV6::getData(std::shared_ptr<EvioNode> & node,
                                                             std::shared_ptr<ByteBuffer> & buf,
                                                             bool copy) {
        if (closed) {
            throw EvioException("object closed");
        }
        else if (node == nullptr) {
            throw EvioException("node arg is null");
        }
        return node->getByteData(buf, copy);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReaderV6::getEventBuffer(size_t eventNumber)  {
        return getEventBuffer(eventNumber, false);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReaderV6::getEventBuffer(size_t eventNumber,
                                                                    bool copy) {

        if (eventNumber < 1) {
            throw EvioException("event number must be > 0");
        }

        if (closed) {
            throw EvioException("object closed");
        }

        std::shared_ptr<EvioNode> node;
        try {
            node = reader.getEventNode(eventNumber - 1);
            auto buff = std::make_shared<ByteBuffer>(node->getTotalBytes());
            return node->getStructureBuffer(buff, copy);
        }
        catch (std::out_of_range & e) {
            throw EvioException("event " + std::to_string(eventNumber) + " does not exist");
        }
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReaderV6::getStructureBuffer(std::shared_ptr<EvioNode> & node)  {
        return getStructureBuffer(node, false);
    }


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReaderV6::getStructureBuffer(std::shared_ptr<EvioNode> & node,
                                                                        bool copy) {

        if (node == nullptr) {
            throw EvioException("node arg is null");
        }

        if (closed) {
            throw EvioException("object closed");
        }

        auto buff = std::make_shared<ByteBuffer>(node->getTotalBytes());
        return node->getStructureBuffer(buff, copy);
    }


    /** {@inheritDoc} */
    void EvioCompactReaderV6::close() {
        reader.getBuffer()->position(reader.getBufferOffset());
        closed = true;
    }


    /** {@inheritDoc} */
    uint32_t EvioCompactReaderV6::getEventCount() {return reader.getEventCount();}


    /** {@inheritDoc} */
    uint32_t EvioCompactReaderV6::getBlockCount() { return reader.getRecordCount();}


    /** {@inheritDoc} */
    void EvioCompactReaderV6::toFile(std::string const & fileName) {
        if (fileName.empty()) {
            throw EvioException("empty fileName arg");
        }

        if (closed) {
            throw EvioException("object closed");
        }

        // Remember where we were
        auto buf = reader.getBuffer();
        int pos = buf->position();

        std::ofstream ostrm(fileName, std::ios::binary);
        ostrm.write(reinterpret_cast<char*>(buf->array() + buf->arrayOffset() + buf->position()),
                    buf->remaining());
        ostrm.close();

        // Go back to where we were
        buf->position(pos);
    }


}