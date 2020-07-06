/**
 * Copyright (c) 2020, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * EPSCI Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 07/01/2020
 * @author timmer
 */

#include "EvioCompactReaderV4.h"


namespace evio {


    /**
     * Constructor for reading an event file.
     *
     * @param path the full path to the file that contains events.
     *             For writing event files, use an <code>EventWriter</code> object.
     * @see EventWriter
     * @throws EvioException if read failure, if path arg is empty
     */
    EvioCompactReaderV4::EvioCompactReaderV4(std::string const & path) {
        if (path.empty()) {
            throw EvioException("path is empty");
        }

        this->path = path;

        /** Object for reading file. */
        file.open(path, std::ios::binary);

        // "ate" mode flag will go immediately to file's end (do this to get its size)
        file.open(path, std::ios::binary | std::ios::ate);
        // Record file length
        fileBytes = file.tellg();

        if (fileBytes < 40) {
            throw EvioException("File too small to have valid evio data");
        }

        // Is the file byte size > max int value?
        // If so we cannot use a memory mapped file.
        if (fileBytes > INT_MAX) {
            throw EvioException("file too large (must be < 2.1475GB)");
        }

        // this object is no longer needed
        file.close();

        // Map file into ByteBuffer
        mapFile(path, fileBytes);

        initialPosition = 0;

        // Parse first block header and find the file's endianness & evio version #.
        // If there's a dictionary, read that too.
        if (readFirstHeader() != IEvioReader::ReadWriteStatus::SUCCESS) {
            throw EvioException("Failed reading first block header/dictionary");
        }

        // Generate a table of all event positions in buffer for random access.
        generateEventPositionTable();

        readingFile = true;
    }


    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     *
     * @see EventWriter
     * @throws EvioException if buffer arg is null;
     *                       failure to read first block header
     */
    EvioCompactReaderV4::EvioCompactReaderV4(std::shared_ptr<ByteBuffer> & byteBuffer) {

        if (byteBuffer == nullptr) {
            throw EvioException("Buffer arg is null");
        }

        initialPosition = byteBuffer->position();
        this->byteBuffer = byteBuffer;

        // Read first block header and find the file's endianness & evio version #.
        // If there's a dictionary, read that too.
        if (readFirstHeader() != IEvioReader::ReadWriteStatus::SUCCESS) {
            throw EvioException("Failed reading first block header/dictionary");
        }

        // Generate a table of all event positions in buffer for random access.
        generateEventPositionTable();
    }


    /**
     * Constructor for reading a buffer.
     *
     * @param buf the buffer that contains events.
     *
     * @see EventWriter
     * @throws EvioException if buffer arg is null;
     *                       failure to read first block header
     */
    EvioCompactReaderV4::EvioCompactReaderV4(std::shared_ptr<ByteBuffer> & buf, EvioNodeSource & pool) {

        if (buf == nullptr) {
            throw EvioException("Buffer arg is null");
        }

        initialPosition = buf->position();
        this->byteBuffer = buf;
        nodePool = pool;

        // Read first block header and find the file's endianness & evio version #.
        // If there's a dictionary, read that too.
        if (readFirstHeader() != IEvioReader::ReadWriteStatus::SUCCESS) {
            throw EvioException("Failed reading first block header/dictionary");
        }

        // Generate a table of all event positions in buffer for random access.
        generateEventPositionTable();
    }


    /**
     * This method can be used to avoid creating additional EvioCompactReader
     * objects by reusing this one with another buffer. The method
     * {@link #close()} is called before anything else.
     *
     * @param buf ByteBuffer to be read
     * @throws EvioException if arg is null;
     *                       if failure to read first block header
     */
    void EvioCompactReaderV4::setBuffer(std::shared_ptr<ByteBuffer> & buf) {
        if (buf == nullptr) {
            throw EvioException("arg is null");
        }

        blockNodes.clear();
        eventNodes.clear();

        blockCount      = -1;
        eventCount      = -1;
        dictionaryXML   = nullptr;
        initialPosition = buf->position();
        this->byteBuffer = buf;

        if (readFirstHeader() != IEvioReader::ReadWriteStatus::SUCCESS) {
            throw EvioException("Failed reading first block header/dictionary");
        }

        generateEventPositionTable();
        closed = false;
    }


    /**
     * This method can be used to avoid creating additional EvioCompactReader
     * objects by reusing this one with another buffer. The method
     * {@link #close()} is called before anything else.
     *
     * @param buf ByteBuffer to be read
     * @throws EvioException if arg is null;
     *                       if failure to read first block header
     */
    void EvioCompactReaderV4::setBuffer(std::shared_ptr<ByteBuffer> & buf, EvioNodeSource & pool) {
        if (buf == nullptr) {
            throw EvioException("arg is null");
        }

        blockNodes.clear();
        eventNodes.clear();
        nodePool = pool;

        blockCount      = -1;
        eventCount      = -1;
        dictionaryXML   = nullptr;
        initialPosition = buf->position();
        this->byteBuffer = buf;

        if (readFirstHeader() != IEvioReader::ReadWriteStatus::SUCCESS) {
            throw EvioException("Failed reading first block header/dictionary");
        }

        generateEventPositionTable();
        closed = false;
    }

    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioCompactReaderV4::setCompressedBuffer(std::shared_ptr<ByteBuffer> & buf,
                                                                         EvioNodeSource & pool) {
        setBuffer(buf, pool);
        return buf;
    }

    /** {@inheritDoc} */
    bool EvioCompactReaderV4::isFile() {return readingFile;}

    /** {@inheritDoc} */
    bool EvioCompactReaderV4::isCompressed() {return false;}

    /**
     * Has {@link #close()} been called (without reopening by calling
     * {@link #setBuffer(ByteBuffer)})?
     *
     * @return {@code true} if this object closed, else {@code false}.
     */
    /*synchronized*/ bool EvioCompactReaderV4::isClosed() {return closed;}

    /**
     * Get the byte order of the file/buffer being read.
     * @return byte order of the file/buffer being read.
     */
    ByteOrder EvioCompactReaderV4::getByteOrder() {return byteOrder;}

    /**
     * Get the evio version number.
     * @return evio version number.
     */
    uint32_t EvioCompactReaderV4::getEvioVersion() {return evioVersion;}

    /**
      * Get the path to the file.
      * @return path to the file
      */
    std::string EvioCompactReaderV4::getPath() {return path;}

    /**
     * Get the byte order of the file being read (or the ByteBuffer for that matter).
     * @return the byte order.
     */
    ByteOrder EvioCompactReaderV4::getFileByteOrder() {return byteOrder;}

    /**
     * Get the XML format dictionary is there is one.
     * @throws EvioException if object closed and dictionary still unread
     * @return XML format dictionary, else null.
     */
    /*synchronized*/ string EvioCompactReaderV4::getDictionaryXML() {
            if (!dictionaryXML.empty()) return dictionaryXML;

            if (closed) {
                throw EvioException("object closed");
            }

            // Is there a dictionary? If so, read it now if we haven't already.
            if (hasDict) {
                try {
                    readDictionary();
                }
                catch (EvioException & e) {}
            }

            return dictionaryXML;
    }

    /**
     * Get the evio dictionary if is there is one.
     * @throws EvioException if object closed and dictionary still unread
     * @return evio dictionary if exists, else null.
     */
    /*synchronized*/ std::shared_ptr<EvioXMLDictionary> EvioCompactReaderV4::getDictionary() {
        if (dictionary != nullptr) return dictionary;

        if (closed) {
            throw EvioException("object closed");
        }

        // Is there a dictionary? If so, read it now if we haven't already.
        if (hasDict) {
            try {
                if (dictionaryXML.empty()) {
                    readDictionary();
                }
                auto dict = std::make_shared<EvioXMLDictionary>(dictionaryXML);
                return dict;
            }
            catch (EvioException & e) {}
        }

        return dictionary;
    }

    /**
     * Does this evio file have an associated XML dictionary?
     *
     * @return <code>true</code> if this evio file has an associated XML dictionary,
     *         else <code>false</code>
     */
    bool EvioCompactReaderV4::hasDictionary() {return hasDict;}

    /**
 	  * Method to memory map a file and allow it to be accessed through a ByteBuffer.
 	  * Allows random access to file.
 	  * @param filename name of file to be mapped.
 	  * @param fileSz size of file in bytes.
     * @throws EvioException if file does not exist, cannot be opened, or cannot be mapped.
 	  */
    void EvioCompactReaderV4::mapFile(std::string const & filename, size_t fileSz) {
        // Create a read-write memory mapped file
        int   fd;
        void *pmem;

        if ((fd = ::open(filename.c_str(), O_RDWR)) < 0) {
            throw EvioException("file does NOT exist");
        }
        else {
            // set shared mem size
            if (::ftruncate(fd, (off_t) fileSz) < 0) {
                ::close(fd);
                throw EvioException("fail to open file");
            }
        }

        // map file to process space
        if ((pmem = ::mmap((caddr_t) 0, fileSz, PROT_READ | PROT_WRITE,
                           MAP_SHARED, fd, (off_t)0)) == MAP_FAILED) {
            ::close(fd);
            throw EvioException("fail to map file");
        }

        // close fd for mapped mem since no longer needed
        ::close(fd);

        // Change the mapped memory into a ByteBuffer for ease of handling ...
        byteBuffer = std::make_shared<ByteBuffer>(static_cast<char *>(pmem), fileSz);
        mappedByteBuffer = byteBuffer;
    }


    /**
     * Get the memory mapped buffer corresponding to the event file.
     * @return the memory mapped buffer corresponding to the event file.
     */
    std::shared_ptr<ByteBuffer> EvioCompactReaderV4::getMappedByteBuffer() {return mappedByteBuffer;}

    /**
     * Get the byte buffer being read directly or corresponding to the event file.
     * @return the byte buffer being read directly or corresponding to the event file.
     */
    std::shared_ptr<ByteBuffer> EvioCompactReaderV4::getByteBuffer() {return byteBuffer;}

    /**
     * Get the size of the file being read, in bytes.
     * For small files, obtain the file size using the memory mapped buffer's capacity.
     * @return the file size in bytes
     */
    size_t EvioCompactReaderV4::fileSize() {return fileBytes;}

    /**
     * Get the EvioNode object associated with a particular event number.
     * @param eventNumber number of event (place in file/buffer) starting at 1.
     * @return  EvioNode object associated with a particular event number,
     *          or null if there is none.
     */
    std::shared_ptr<EvioNode> EvioCompactReaderV4::getEvent(size_t eventNumber) {
        try {
            return eventNodes.at(eventNumber - 1);
        }
        catch (std::out_of_range & e) { }
        return nullptr;
    }


    /**
     * Get the EvioNode object associated with a particular event number
     * which has been scanned so all substructures are contained in the
     * node.allNodes list.
     * @param eventNumber number of event (place in file/buffer) starting at 1.
     * @return  EvioNode object associated with a particular event number,
     *          or null if there is none.
     */
    std::shared_ptr<EvioNode> EvioCompactReaderV4::getScannedEvent(size_t eventNumber) {
        try {
            return scanStructure(eventNumber);
        }
        catch (std::out_of_range & e) { }
        return nullptr;
    }


    /**
     * Get the EvioNode object associated with a particular event number
     * which has been scanned so all substructures are contained in the
     * node.allNodes list.
     * @param eventNumber number of event (place in file/buffer) starting at 1.
     * @param nodeSource  source of EvioNode objects to use while parsing evio data.
     * @return  EvioNode object associated with a particular event number,
     *          or null if there is none.
     */
    std::shared_ptr<EvioNode> EvioCompactReaderV4::getScannedEvent(size_t eventNumber, EvioNodeSource & nodeSource) {
        try {
            return scanStructure(eventNumber, nodeSource);
        }
        catch (std::out_of_range & e) {}
        return nullptr;
    }


    /**
     * Generate a table (ArrayList) of positions of events in file/buffer.
     * This method does <b>not</b> affect the byteBuffer position, eventNumber,
     * or lastBlock values. Uses only absolute gets so byteBuffer position
     * does not change. Called only in constructors and
     * {@link #setBuffer(ByteBuffer)}.
     *
     * @throws EvioException if bytes not in evio format
     */
    void EvioCompactReaderV4::generateEventPositionTable() {

        uint32_t  byteInfo, byteLen, blockHdrSize, blockSize, blockEventCount, magicNum;
        bool      firstBlock=true, hasDictionary=false, isLastBlock;

//        long t2, t1 = System.currentTimeMillis();

        // Start at the beginning of byteBuffer without changing
        // its current position. Do this with absolute gets.
        size_t position  = initialPosition;
        size_t bytesLeft = byteBuffer->limit() - position;

        // Keep track of the # of blocks, events, and valid words in file/buffer
        blockCount = 0;
        eventCount = 0;
        validDataWords = 0;

        // uint32_t blockCounter = 0;

        while (bytesLeft > 0) {
            // Need enough data to at least read 1 block header (32 bytes)
            if (bytesLeft < 32) {
                throw EvioException("Bad evio format: extra " + std::to_string(bytesLeft) +
                                    " bytes at file end");
            }

            // Swapping is taken care of
            blockSize       = byteBuffer->getUInt(position);
            byteInfo        = byteBuffer->getUInt(position + 4*BlockHeaderV4::EV_VERSION);
            blockHdrSize    = byteBuffer->getUInt(position + 4*BlockHeaderV4::EV_HEADERSIZE);
            blockEventCount = byteBuffer->getUInt(position + 4*BlockHeaderV4::EV_COUNT);
            magicNum        = byteBuffer->getUInt(position + 4*BlockHeaderV4::EV_MAGIC);
            isLastBlock     = BlockHeaderV4::isLastBlock(byteInfo);

//                std::cout << "    read block header " << blockCounter++ << ": " <<
//                             "ev count = " << blockEventCount <<
//                             ", blockSize = " << blockSize <<
//                             ", blockHdrSize = " << blockHdrSize << showbase << hex <<
//                             ", byteInfo/ver  = " << byteInfo <<
//                             ", magicNum = " << magicNum << dec << std::endl;

            // If magic # is not right, file is not in proper format
            if (magicNum != BlockHeaderV4::MAGIC_NUMBER) {
                throw EvioException("Bad evio format: block header magic # incorrect");
            }

            if (blockSize < 8 || blockHdrSize < 8) {
                throw EvioException("Bad evio format (block: len = " +
                                    std::to_string(blockSize) + ", blk header len = " +
                                    std::to_string(blockHdrSize) + ")" );
            }

            // Check to see if the whole block is there
            if (4*blockSize > bytesLeft) {
//std::cout << "    4*blockSize = " << (4*blockSize) << " >? bytesLeft = " << bytesLeft <<
//             ", pos = " << position << std::endl;
                throw EvioException("Bad evio format: not enough data to read block");
            }

            // File is now positioned before block header.
            // Look at block header to get info.
            auto blockNode = std::make_shared<RecordNode>();
            blockNode->setPos(position);
            blockNode->setLen(blockSize);
            blockNode->setCount(blockEventCount);
            blockNode->setPlace(blockCount);

            blockNodes.insert({blockCount, blockNode});
            blockCount++;

            validDataWords += blockSize;
            if (firstBlock) hasDictionary = BlockHeaderV4::hasDictionary(byteInfo);

            // Hop over block header to events
            position  += 4*blockHdrSize;
            bytesLeft -= 4*blockHdrSize;

//std::cout << "    hopped blk hdr, pos = " << position << ", is last blk = " << isLastBlock << std::endl;

            // Check for a dictionary - the first event in the first block.
            // It's not included in the header block count, but we must take
            // it into account by skipping over it.
            if (firstBlock && hasDictionary) {
                firstBlock = false;

                // Get its length - bank's len does not include itself
                byteLen = 4*(byteBuffer->getUInt(position) + 1);

                // Skip over dictionary
                position  += byteLen;
                bytesLeft -= byteLen;
//std::cout << "    hopped dict, pos = " << position << std::endl;
            }

            // For each event in block, store its location
            for (int i=0; i < blockEventCount; i++) {
                // Sanity check - must have at least 1 header's amount left
                if (bytesLeft < 8) {
                    throw EvioException("Bad evio format: not enough data to read event (bad bank len?)");
                }

                auto node = EvioNode::extractEventNode(byteBuffer, nodePool, *(blockNode.get()),
                                                       position, eventCount + i);
//std::cout << "      event " << i << " in block: pos = " << node->getPosition() <<
//             ", dataPos = " << node->getDataPosition() << ", ev # = " << (eventCount + i + 1) << std::endl;
                eventNodes.push_back(node);
                //blockNode.allEventNodes.add(node);

                // Hop over header + data
                byteLen = 8 + 4*(node->getDataLength());
                position  += byteLen;
                bytesLeft -= byteLen;

                if (byteLen < 8 || bytesLeft < 0) {
                    throw EvioException("Bad evio format: bad bank length");
                }

//std::cout << "    hopped event " << (i+1) << ", bytesLeft = " << bytesLeft << ", pos = " << position << std::endl;
            }

            eventCount += blockEventCount;

            if (isLastBlock) break;
        }

//        t2 = System.currentTimeMillis();
//std::cout << "Time to scan file = " << (t2-t1) << " milliseconds" << std::endl;
    }


    /**
     * Reads the first block (physical record) header in order to determine
     * characteristics of the file or buffer in question. These things
     * do <b>not</b> need to be examined in subsequent block headers.
     * File or buffer must be evio version 4 or greater.
     * Called only in constructors and setBuffer.
     *
     * @return status of read attempt
     */
    IEvioReader::ReadWriteStatus EvioCompactReaderV4::readFirstHeader() {
        // Get first block header
        size_t pos = initialPosition;

        // Have enough remaining bytes to read (first part of) header?
        if (byteBuffer->limit() - pos < 32) {
            std::cout << "     readFirstHeader: EOF, remaining = " << (byteBuffer->limit() - pos) << std::endl;
            byteBuffer->clear();
            return IEvioReader::ReadWriteStatus::END_OF_FILE;
        }

        try {
            // Set the byte order to match the file's ordering.

            // Check the magic number for endianness (buffer defaults to little endian)
            byteOrder = byteBuffer->order();

            uint32_t magicNumber = byteBuffer->getUInt(pos + EvioReaderV4::MAGIC_OFFSET);

            if (magicNumber != IBlockHeader::MAGIC_NUMBER) {

                if (byteOrder == ByteOrder::ENDIAN_BIG) {
                    byteOrder = ByteOrder::ENDIAN_LITTLE;
                }
                else {
                    byteOrder = ByteOrder::ENDIAN_BIG;
                }
                byteBuffer->order(byteOrder);

                // Reread magic number to make sure things are OK
                magicNumber = byteBuffer->getUInt(pos + EvioReaderV4::MAGIC_OFFSET);
                if (magicNumber != IBlockHeader::MAGIC_NUMBER) {
                    std::cout << "     readFirstHeader: reread magic # (" << magicNumber <<
                                 ") & still not right" << std::endl;
                    Util::printBytes(*(byteBuffer.get()), 0, 32,
                               "Tried to parse this as block header");
                    return IEvioReader::ReadWriteStatus::EVIO_EXCEPTION;
                }
            }

            // Check the version number
            uint32_t bitInfo = byteBuffer->getUInt(pos + EvioReaderV4::VERSION_OFFSET);
            evioVersion = bitInfo & VERSION_MASK;
            if (evioVersion < 4)  {
                std::cout << "     readFirstHeader: unsupported evio version (" << evioVersion << ")" << std::endl;
                return IEvioReader::ReadWriteStatus::EVIO_EXCEPTION;
            }

            // Does this file/buffer have a dictionary?
            hasDict = BlockHeaderV4::hasDictionary(bitInfo);

            // # of words in first block header
            firstBlockHeaderWords = byteBuffer->getUInt(pos + BLOCK_HEADER_SIZE_OFFSET);

            // Store first header data
            blockHeader->setSize(byteBuffer->getInt(pos + BLOCK_SIZE_OFFSET));
            blockHeader->setNumber(byteBuffer->getInt(pos + BLOCK_NUMBER));
            blockHeader->setHeaderLength(firstBlockHeaderWords);
            blockHeader->setEventCount(byteBuffer->getInt(pos + BLOCK_EVENT_COUNT));
            blockHeader->setReserved1(byteBuffer->getInt(pos + BLOCK_RESERVED_1));   // used from ROC

            if (blockHeader->getSize() < 8) {
                std::cout << "     readFirstHeader: block size too small, " << blockHeader->getSize() << std::endl;
                byteBuffer->clear();
                Util::printBytes(*(byteBuffer.get()), 0, 32, "Tried to parse this as block header");
                return IEvioReader::ReadWriteStatus::EVIO_EXCEPTION;
            }

            if (blockHeader->getHeaderLength() < 8) {
                std::cout << "     readFirstHeader: block header too small, " << blockHeader->getHeaderLength() << std::endl;
                byteBuffer->clear();
                Util::printBytes(*(byteBuffer.get()), 0, 8,
                           "Tried to parse this as block header");
                return IEvioReader::ReadWriteStatus::EVIO_EXCEPTION;
            }

            // Use 6th word to set bit info & version
            blockHeader->parseToBitInfo(bitInfo);
            blockHeader->setVersion(evioVersion);
            blockHeader->setReserved2(0);  // not used
            blockHeader->setMagicNumber(magicNumber);
            blockHeader->setByteOrder(byteOrder);
        }
        catch (EvioException & a) {
            byteBuffer->clear();
            Util::printBytes(*(byteBuffer.get()), 0, 8, "Tried to parse this as block header");
            return IEvioReader::ReadWriteStatus::EVIO_EXCEPTION;
        }

        return IEvioReader::ReadWriteStatus::SUCCESS;
    }


    /** {@inheritDoc} */
    std::shared_ptr<IBlockHeader> EvioCompactReaderV4::getFirstBlockHeader() {return blockHeader;}


    /**
     * This method is only called if the user wants to look at the dictionary.
     * This method is called by synchronized code because of its use of a bulk,
     * relative get method.
     *
     * @throws EvioException if failed read due to bad buffer format
     */
    void EvioCompactReaderV4::readDictionary() {

        // Where are we?
        size_t originalPos = byteBuffer->position();
        size_t pos = initialPosition + 4*firstBlockHeaderWords;

        // How big is the event?
        uint32_t length = byteBuffer->getUInt(pos);
        if (length < 1) {
            throw EvioException("Bad value for dictionary length");
        }
        pos += 4;

        // Since we're only interested in length, read but ignore rest of the header.
        byteBuffer->getInt(pos);
        pos += 4;

        // Get the raw data
        size_t eventDataSizeBytes = 4*(length - 1);
        uint8_t bytes[eventDataSizeBytes];

        // Read in dictionary data (using a relative get)
        try {
            byteBuffer->position(pos);
            byteBuffer->getBytes(bytes, eventDataSizeBytes);
        }
        catch (EvioException & e) {
            throw EvioException("Problems reading buffer");
        }

        // Unpack array into dictionary
        std::vector<string> strs;
        BaseStructure::unpackRawBytesToStrings(bytes, eventDataSizeBytes, strs);
        if (strs.empty()) {
            throw EvioException("Data in bad format");
        }
        dictionaryXML = strs[0];

        byteBuffer->position(originalPos);
    }


    /**
     * This method scans the given event number in the buffer.
     * It returns an EvioNode object representing the event.
     * All the event's substructures, as EvioNode objects, are
     * contained in the node.allNodes list (including the event itself).
     *
     * @param eventNumber number of the event to be scanned starting at 1
     * @return the EvioNode object corresponding to the given event number
     * @throws std::out_of_range if arg is out of range.
     */
    std::shared_ptr<EvioNode> EvioCompactReaderV4::scanStructure(size_t eventNumber) {

        // Node corresponding to event
        auto node = eventNodes.at(eventNumber - 1);

        if (node->scanned) {
            node->clearLists();
        }

        // Do this before actual scan so clone() sets all "scanned" fields
        // of child nodes to "true" as well.
        node->scanned = true;

        EvioNode::scanStructure(node);

        return node;
    }


    /**
     * This method scans the given event number in the buffer.
     * It returns an EvioNode object representing the event.
     * All the event's substructures, as EvioNode objects, are
     * contained in the node.allNodes list (including the event itself).
     *
     * @param eventNumber number of the event to be scanned starting at 1
     * @param nodeSource  source of EvioNode objects to use while parsing evio data.
     * @return the EvioNode object corresponding to the given event number
      * @throws std::out_of_range if arg is out of range.
     */
    std::shared_ptr<EvioNode> EvioCompactReaderV4::scanStructure(size_t eventNumber, EvioNodeSource & nodeSource) {

        // Node corresponding to event
        auto node = eventNodes.at(eventNumber - 1);

        if (node->scanned) {
            node->clearLists();
        }

        // Do this before actual scan so clone() sets all "scanned" fields
        // of child nodes to "true" as well.
        node->scanned = true;

        EvioNode::scanStructure(node, nodeSource);

        return node;
    }


    /**
     * This method searches the specified event in a file/buffer and
     * returns a list of objects each of which contain information
     * about a single evio structure which matches the given tag and num.
     *
     * @param eventNumber place of event in buffer (starting with 1)
     * @param tag tag to match
     * @param num num to match
     * @return list of EvioNode objects corresponding to matching evio structures
     *         (empty if none found)
     * @throws EvioException if bad arg value(s);
     *                       if object closed
     */
    /*synchronized*/ void EvioCompactReaderV4::searchEvent(size_t eventNumber, uint16_t tag, uint8_t num,
                                                           std::vector<std::shared_ptr<EvioNode>> & vec) {
        // check args
        if (eventNumber > eventCount) {
            throw EvioException("eventNumber arg too large");
        }

        if (closed) {
            throw EvioException("object closed");
        }

        vec.clear();
        vec.reserve(100);

        // Scan the node
        auto list = scanStructure(eventNumber)->allNodes;
//std::cout << "searchEvent: ev# = " << eventNumber << ", list size = " << list.size() <<
//" for tag/num = " << tag << "/" << num << std::endl;

        // Now look for matches in this event
        for (auto const & enode: list) {
//            std::cout << "searchEvent: desired tag = " << tag << " found " << enode->getTag() << std::endl;
//            std::cout << "           : desired num = " << num << " found " << enode->getNum() << std::endl;
            if (enode->getTag() == tag && enode->getNum() == num) {
//                std::cout << "           : found node at pos = " << enode->getPosition() <<
//                             " len = " << enode->getLength() << std::endl;
                vec.push_back(enode);
            }
        }
    }


    /**
     * This method searches the specified event in a file/buffer and
     * returns a list of objects each of which contain information
     * about a single evio structure which matches the given dictionary
     * entry name.
     *
     * @param  eventNumber place of event in buffer (starting with 1)
     * @param  dictName name of dictionary entry to search for
     * @param  dict dictionary to use; if null, use dictionary with file/buffer
     *
     * @return list of EvioNode objects corresponding to matching evio structures
     *         (empty if none found)
     * @throws EvioException if dictName is empty;
     *                       if dictName is an invalid dictionary entry;
     *                       if dictionary is null and none provided in file/buffer being read;
     *                       if object closed
     */
    /*synchronized*/ void EvioCompactReaderV4::searchEvent(size_t eventNumber, std::string const & dictName,
                                                           std::shared_ptr<EvioXMLDictionary> & dict,
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

        if (dict == nullptr && hasDict)  {
            dict = getDictionary();
        }

        if (dict != nullptr) {
            bool found = dict->getTag(dictName, &tag);
            if (!found) {
                throw EvioException("no dictionary entry available");
            }
            dict->getNum(dictName, &num);
        }
        else {
            throw EvioException("no dictionary available");
        }

        return searchEvent(eventNumber, tag, num, vec);
    }


    /**
     * This method removes the data of the given event from the buffer.
     * It also marks any existing EvioNodes representing the event and its
     * descendants as obsolete. They must not be used anymore.<p>
     *
     * @param eventNumber number of event to remove from buffer
     * @return new ByteBuffer created and updated to reflect the event removal
     * @throws EvioException if eventNumber &lt; 1;
     *                       if event number does not correspond to existing event;
     *                       if object closed;
     *                       if node was not found in any event;
     *                       if internal programming error
     */
    /*synchronized*/ std::shared_ptr<ByteBuffer> EvioCompactReaderV4::removeEvent(size_t eventNumber) {

        if (closed) {
            throw EvioException("object closed");
        }

        std::shared_ptr<EvioNode> eventNode;
        try {
            eventNode = eventNodes.at(eventNumber - 1);
        }
        catch (std::out_of_range & e) {
            throw EvioException("event " + std::to_string(eventNumber) + " does not exist");
        }

        return removeStructure(eventNode);
    }


    /**
     * This method removes the data, represented by the given node, from the buffer.
     * It also marks the node and its descendants as obsolete. They must not be used
     * anymore.<p>
     *
     * @param removeNode  evio structure to remove from buffer
     * @return ByteBuffer updated to reflect the node removal
     * @throws EvioException if object closed;
     *                       if node was not found in any event;
     *                       if internal programming error
     */
    /*synchronized*/ std::shared_ptr<ByteBuffer> EvioCompactReaderV4::removeStructure(
            std::shared_ptr<EvioNode> & removeNode) {

        // If we're removing nothing, then DO nothing
        if (removeNode == nullptr) {
            return byteBuffer;
        }

        if (closed) {
            throw EvioException("object closed");
        }
        else if (removeNode->isObsolete()) {
            //std::cout << "removeStructure: node has already been removed" << std::endl;
            return byteBuffer;
        }

        std::shared_ptr<EvioNode> eventNode = nullptr;
        bool isEvent = false, foundNode = false;
        // If removed node is an event, place of the removed node in eventNodes list
        uint32_t eventRemovePlace = 0;
        // Place of the removed node in allNodes list.
        // 0 means event itself (top level).
        uint32_t removeNodePlace = 0;

        // Locate the node to be removed ...
        for (auto const & ev : eventNodes) {
            removeNodePlace = 0;

            // See if it's an event ...
            if (removeNode == ev) {
                eventNode = ev;
                isEvent = true;
                foundNode = true;
                break;
            }

            for (auto const & n : ev->allNodes) {
                // The first node in allNodes is the event node,
                // so do not increment removeNodePlace now.

                if (removeNode == n) {
                    eventNode = ev;
                    foundNode = true;
                    goto out;
                }

                // Keep track of where inside the event it is
                removeNodePlace++;
            }
            eventRemovePlace++;
        }
        out:

        if (!foundNode) {
            throw EvioException("removeNode not found in any event");
        }

        // The data these nodes represent will be removed from the buffer,
        // so the node will be obsolete along with all its descendants.
        removeNode->setObsolete(true);

        // If we started out by reading a file, now we switch to using a buffer
        if (readingFile) {
            readingFile = false;
            mappedByteBuffer = nullptr;

            // Create a new buffer by duplicating existing one
            auto newBuffer = std::make_shared<ByteBuffer>(byteBuffer->capacity());
            newBuffer->order(byteOrder).position(byteBuffer->position()).limit(byteBuffer->limit());

            // Copy data into new buffer
            newBuffer->put(byteBuffer);
            newBuffer->position(initialPosition);

            // Use new buffer from now on
            byteBuffer = newBuffer;

            // All nodes need to use this new buffer
            for (auto const & ev : eventNodes) {
                for (auto const & n : ev->allNodes) {
                    n->setBuffer(byteBuffer);
                }
            }
        }

        //---------------------------------------------------
        // Remove structure. Keep using current buffer.
        // We'll move all data that came after removed node
        // to where removed node used to be.
        //---------------------------------------------------

        // Amount of data being removed
        uint32_t removeDataLen = removeNode->getTotalBytes();
        uint32_t removeWordLen = removeDataLen / 4;

        // Just after removed node (start pos of data being moved)
        uint32_t startPos = removeNode->getPosition() + removeDataLen;
        // Length of data to move in bytes
        uint32_t moveLen = initialPosition + 4*validDataWords - startPos;


        // Can't use duplicate(), must copy the backing buffer
        auto moveBuffer = std::make_shared<ByteBuffer>(moveLen);
        moveBuffer->order(byteBuffer->order());

        size_t bufferLim = byteBuffer->limit();
        byteBuffer->limit(startPos + moveLen).position(startPos);
        moveBuffer->put(byteBuffer);
        byteBuffer->limit(bufferLim);

        // Prepare to move data currently sitting past the removed node
        moveBuffer->clear();

        // Set place to put the data being moved - where removed node starts
        byteBuffer->position(removeNode->getPosition());
        // Copy it over
        byteBuffer->put(moveBuffer);

        // Reset some buffer values
        validDataWords -= removeWordLen;
        byteBuffer->position(initialPosition);
        byteBuffer->limit(4*validDataWords + initialPosition);

        //-------------------------------------
        // By removing a structure, we need to shift the POSITIONS of all
        // structures that follow by the size of the deleted chunk.
        //-------------------------------------
        vector<shared_ptr<EvioNode>> nodeList;
        uint32_t place = eventNode->place;

        for (int i=0; i < eventCount; i++) {
            int level = 0;
            nodeList = eventNodes[i]->allNodes;

            for (shared_ptr<EvioNode> const & n : nodeList) {
                // For events that come after, move all contained nodes
                if (i > place) {
                    n->pos -= removeDataLen;
                    n->dataPos -= removeDataLen;
                }
                    // For the event in which the removed node existed ...
                else if (i == place && !isEvent) {
                    // There may be structures that came after the removed node,
                    // but within the same event. They need to be moved too.
                    if (level > removeNodePlace) {
                        n->pos -= removeDataLen;
                        n->dataPos -= removeDataLen;
                    }
                }
                level++;
            }
        }

        place = eventNode->recordNode.getPlace();
        for (int i=0; i < blockCount; i++) {
            if (i > place) {
                blockNodes[i]->pos -= removeDataLen;
            }
        }

        //--------------------------------------------
        // We need to update the lengths of all the
        // removed node's parent structures as well as
        // the length of the block containing it.
        //--------------------------------------------

//std::cout << "block object len = " << eventNode->recordNode.getLen() <<
//                   ", set to " << (eventNode->recordNode.getLen() - removeWordLen) << std::endl;
        // If removing entire event ...
        if (isEvent) {
            // Decrease total event count
            eventCount--;
            // Decrease block count
            eventNode->recordNode.count--;
            // Skip over 3 ints to update the block header's event count
            byteBuffer->putInt(eventNode->recordNode.pos + 12, eventNode->recordNode.count);
        }
        eventNode->recordNode.len -= removeWordLen;
        byteBuffer->putInt(eventNode->recordNode.pos, eventNode->recordNode.len);

        // Position of parent in new byteBuffer of event
        int parentPos;
        std::shared_ptr<EvioNode> removeParent, parent;
        removeParent = parent = removeNode->parentNode;

        while (parent != nullptr) {
            // Update event size
            parent->len     -= removeWordLen;
            parent->dataLen -= removeWordLen;
            parentPos = parent->pos;
            // Since we're changing parent's data, get rid of stored data in int[] format
            parent->clearIntArray();

            // Parent contains data of this type
            DataType parentType = parent->getDataTypeObj();
            if (parentType == DataType::BANK || parentType == DataType::ALSOBANK) {
//std::cout << "parent bank pos = " << parentPos << ", len was = " << (parent->len + removeWordLen) <<
//             ", now set to " << parent->len << std::endl;
                byteBuffer->putInt(parentPos, parent->len);
            }
            else if (parentType == DataType::SEGMENT ||
                     parentType == DataType::ALSOSEGMENT ||
                     parentType == DataType::TAGSEGMENT) {
//std::cout << "parent seg/tagseg pos = " << parentPos << ", len was = " << (parent->len + removeWordLen) <<
//             ", now set to " << parent->len << std::endl;
                if (byteOrder == ByteOrder::ENDIAN_BIG) {
                    byteBuffer->putShort(parentPos + 2, (short) (parent->len));
                }
                else {
                    byteBuffer->putShort(parentPos, (short) (parent->len));
                }
            }
            else {
                throw EvioException("internal programming error");
            }

            parent = parent->parentNode;
        }

        // Remove node and node's children from lists
        if (removeParent != nullptr) {
            removeParent->removeChild(removeNode);
        }

        if (isEvent) {
            eventNodes.erase(eventNodes.begin() + eventRemovePlace);
        }

        return byteBuffer;
    }

// TODO: This method will change a memory mapped buffer into one that is NOT!!!
// Map is opened as READ_ONLY

    /**
     * This method adds an evio container (bank, segment, or tag segment) as the last
     * structure contained in an event. It is the responsibility of the caller to make
     * sure that the buffer argument contains valid evio data (only data representing
     * the structure to be added - not in file format with block header and the like)
     * which is compatible with the type of data stored in the given event.<p>
     *
     * To produce such evio data use {@link EvioBank#write(ByteBuffer)},
     * {@link EvioSegment#write(ByteBuffer)} or
     * {@link EvioTagSegment#write(ByteBuffer)} depending on whether
     * a bank, seg, or tagseg is being added.<p>
     *
     * The given buffer argument must be ready to read with its position and limit
     * defining the limits of the data to copy.
     * This method is synchronized due to the bulk, relative puts.
     *
     * @param eventNumber number of event to which addBuffer is to be added
     * @param addBuffer buffer containing evio data to add (<b>not</b> evio file format,
     *                  i.e. no block headers)
     * @return a new ByteBuffer object which is created and filled with all the data
     *         including what was just added.
     * @throws EvioException if addBuffer arg is empty or has non-evio format;
     *                       if addBuffer is opposite endian to current event buffer;
     *                       if added data is not the proper length (i.e. multiple of 4 bytes);
     *                       if the event number does not correspond to an existing event;
     *                       if there is an internal programming error;
     *                       if object closed
     */
    /*synchronized*/ std::shared_ptr<ByteBuffer> EvioCompactReaderV4::addStructure(size_t eventNumber,
                                                                                   ByteBuffer & addBuffer) {

        if (addBuffer.remaining() < 8) {
            throw EvioException("empty or non-evio format buffer arg");
        }

        if (addBuffer.order() != byteOrder) {
            throw EvioException("trying to add wrong endian buffer");
        }

        if (closed) {
            throw EvioException("object closed");
        }

        std::shared_ptr<EvioNode> eventNode;
        try {
            eventNode = eventNodes.at(eventNumber - 1);
        }
        catch (std::out_of_range & e) {
            throw EvioException("event " + std::to_string(eventNumber) + " does not exist");
        }

        // Position in byteBuffer just past end of event
        uint32_t endPos = eventNode->dataPos + 4*eventNode->dataLen;

        // Original position of buffer being added
        size_t origAddBufPos = addBuffer.position();

        // How many bytes are we adding?
        size_t appendDataLen = addBuffer.remaining();

        // Make sure it's a multiple of 4
        if (appendDataLen % 4 != 0) {
            throw EvioException("data added is not in evio format");
        }

        // Since we're changing node's data, get rid of stored data in int[] format
        eventNode->clearIntArray();

        // Data length in 32-bit words
        size_t appendDataWordLen = appendDataLen / 4;

        // Event contains structures of this type
        DataType eventDataType = eventNode->getDataTypeObj();

        //--------------------------------------------
        // Add new structure to end of specified event
        //--------------------------------------------

        // Create a new buffer
        auto newBuffer = std::make_shared<ByteBuffer>(4*validDataWords + appendDataLen);
        newBuffer->order(byteOrder);

        // Copy beginning part of existing buffer into new buffer
        byteBuffer->limit(endPos).position(initialPosition);
        newBuffer->put(byteBuffer);

        // Copy new structure into new buffer
        int newBankBufPos = newBuffer->position();
        newBuffer->put(addBuffer);

        // Copy ending part of existing buffer into new buffer
        byteBuffer->limit(4*validDataWords + initialPosition).position(endPos);
        newBuffer->put(byteBuffer);

        // Get new buffer ready for reading
        newBuffer->flip();

        // Restore original positions of buffers
        byteBuffer->position(initialPosition);
        addBuffer.position(origAddBufPos);

        //-------------------------------------
        // By inserting a structure, we've definitely changed the positions of all
        // structures that follow. Everything downstream gets shifted by appendDataLen
        // bytes.
        // And, if initialPosition was not 0 (in the new buffer it always is),
        // then ALL nodes need their position members shifted by initialPosition
        // bytes upstream.
        //-------------------------------------
        uint32_t place = eventNode->place;

        for (int i=0; i < eventCount; i++) {
            for (auto const & n : eventNodes[i]->allNodes) {
                // Make sure nodes are using the new buffer
                n->setBuffer(newBuffer);

                //System.out.println("Event node " + (i+1) + ", pos = " + n.pos + ", dataPos = " + n.dataPos);
                if (i > place) {
                    n->pos     += appendDataLen - initialPosition;
                    n->dataPos += appendDataLen - initialPosition;
                    //System.out.println("      pos -> " + n.pos + ", dataPos -> " + n.dataPos);
                }
                else {
                    n->pos     -= initialPosition;
                    n->dataPos -= initialPosition;
                }
            }
        }

        place = eventNode->recordNode.place;
        for (int i=0; i < blockCount; i++) {
            if (i > place) {
                blockNodes[i]->pos += appendDataLen - initialPosition;
            }
            else {
                blockNodes[i]->pos -= initialPosition;
            }
        }

        // At this point all EvioNode objects (including those in
        // user's possession) should be updated.

        // This reader object is NOW using the new buffer
        byteBuffer      = newBuffer;
        initialPosition = newBuffer->position();
        validDataWords += appendDataWordLen;

        // If we started out by reading a file, now we are using the new buffer.
        if (readingFile) {
            readingFile = false;
            mappedByteBuffer = nullptr;
        }

        //--------------------------------------------
        // Adjust event and block header sizes in both
        // block/event node objects and in new buffer.
        //--------------------------------------------

        // Position in new byteBuffer of event
        int eventLenPos = eventNode->pos;

        // Increase block size
        //System.out.println("block object len = " +  eventNode.blockNode.len +
        //                   ", set to " + (eventNode.blockNode.len + appendDataWordLen));
        eventNode->recordNode.len += appendDataWordLen;
        newBuffer->putInt(eventNode->recordNode.pos, eventNode->recordNode.len);

        // Increase event size
        eventNode->len     += appendDataWordLen;
        eventNode->dataLen += appendDataWordLen;

        if (eventDataType == DataType::BANK || eventDataType == DataType::ALSOBANK) {

//            std::cout << "event pos = " << eventLenPos << ", len = " << (eventNode->len - appendDataWordLen) <<
//                         ", set to " << (eventNode->len) << std::endl;

            newBuffer->putInt(eventLenPos, eventNode->len);
        }
        else if (eventDataType == DataType::SEGMENT ||
                 eventDataType == DataType::ALSOSEGMENT ||
                 eventDataType == DataType::TAGSEGMENT) {

//            std::cout << "event SEG/TAGSEG pos = " << eventLenPos << ", len = " <<
//                         (eventNode->len - appendDataWordLen) << ", set to " << (eventNode->len) << std::endl;
            if (byteOrder == ByteOrder::ENDIAN_BIG) {
                newBuffer->putShort(eventLenPos + 2, (uint16_t) (eventNode->len));
            }
            else {
                newBuffer->putShort(eventLenPos, (uint16_t) (eventNode->len));
            }
        }
        else {
            throw EvioException("internal programming error");
        }

        // Since the event's values (positions and lengths) have now been set properly,
        // we can now rescan the event to update all the sub-structure info, thereby
        // including the newly add structure. Problem is, that invalidates all existing
        // node objects for this event and users may try to continue using those - BAD.
        //
        // Instead, create a single new node by coping the event object and resetting
        // all its internal values by parsing (or extracting from) the buffer.
        if (eventNode->scanned) {

            // copy & empty childNodes
            auto pNode = new EvioNode(eventNode);
            auto newNode = std::shared_ptr<EvioNode>(pNode);
            newNode->childNodes.clear();
            newNode->data.clear();
            newNode->izEvent = false;
            newNode->eventNode = newNode->parentNode = eventNode;

            // Extract data from buffer (not children data)
            EvioNode::extractNode(newNode, newBankBufPos);

            // Now that we have this new node, we must place it in the correct order
            // in both the child & allNodes lists. This is easy since we are inserting
            // the bank as the last bank of this event.
            eventNode->addChild(newNode);

            // This node may contain other nodes. Find those by scanning this one.
            // This will add all nodes in this tree to all lists.
            EvioNode::scanStructure(newNode);
        }

        return newBuffer;
    }


    /**
     * Get the data associated with an evio structure in ByteBuffer form.
     * The returned buffer is a view into this reader's buffer (no copy done).
     * Changes in one will affect the other.
     *
     * @param node evio structure whose data is to be retrieved
     * @throws EvioException if object closed
     * @return ByteBuffer object containing data. Position and limit are
     *         set for reading.
     */
    std::shared_ptr<ByteBuffer> EvioCompactReaderV4::getData(std::shared_ptr<EvioNode> & node) {
            return getData(node, false);
    }


    /**
     * Get the data associated with an evio structure in ByteBuffer form.
     * Depending on the copy argument, the returned buffer will either be
     * a copy of or a view into the data of this reader's buffer.<p>
     * This method is synchronized due to the bulk, relative gets & puts.
     *
     * @param node evio structure whose data is to be retrieved
     * @param copy if <code>true</code>, then return a copy as opposed to a
     *             view into this reader object's buffer.
     * @throws EvioException if object closed
     * @return ByteBuffer object containing data. Position and limit are
     *         set for reading.
     */
    /*synchronized*/ std::shared_ptr<ByteBuffer> EvioCompactReaderV4::getData(std::shared_ptr<EvioNode> & node, bool copy) {
            return node->getByteData(copy);
    }


    /**
     * Get an evio bank or event in ByteBuffer form.
     * The returned buffer is a view into the data of this reader's buffer.<p>
     * This method is synchronized due to the bulk, relative gets & puts.
     *
     * @param eventNumber number of event of interest
     * @return ByteBuffer object containing bank's/event's bytes. Position and limit are
     *         set for reading.
     * @throws EvioException if eventNumber &lt; 1;
     *                       if the event number does not correspond to an existing event;
     *                       if object closed
     */
    std::shared_ptr<ByteBuffer> EvioCompactReaderV4::getEventBuffer(size_t eventNumber) {
            return getEventBuffer(eventNumber, false);
    }


    /**
     * Get an evio bank or event in ByteBuffer form.
     * Depending on the copy argument, the returned buffer will either be
     * a copy of or a view into the data of this reader's buffer.<p>
     * This method is synchronized due to the bulk, relative gets & puts.
     *
     * @param eventNumber number of event of interest
     * @param copy if <code>true</code>, then return a copy as opposed to a
     *        view into this reader object's buffer.
     * @return ByteBuffer object containing bank's/event's bytes. Position and limit are
     *         set for reading.
     * @throws EvioException if the event number does not correspond to an existing event;
     *                       if object closed
     */
    /*synchronized*/ std::shared_ptr<ByteBuffer> EvioCompactReaderV4::getEventBuffer(size_t eventNumber, bool copy) {

            if (closed) {
                throw EvioException("object closed");
            }

            EvioNode node;
            try {
                node = eventNodes[eventNumber - 1];
            }
            catch (IndexOutOfBoundsException e) {
                throw EvioException("event " + std::to_string(eventNumber) + " does not exist");
            }

            return node.getStructureBuffer(copy);
    }


    /**
     * Get an evio structure (bank, seg, or tagseg) in ByteBuffer form.
     * The returned buffer is a view into the data of this reader's buffer.<p>
     * This method is synchronized due to the bulk, relative gets & puts.
     *
     * @param node node object representing evio structure of interest
     * @return ByteBuffer object containing bank's/event's bytes. Position and limit are
     *         set for reading.
     * @throws EvioException if node is null;
     *                       if object closed
     */
    std::shared_ptr<ByteBuffer> EvioCompactReaderV4::getStructureBuffer(std::shared_ptr<EvioNode> & node) {
            return getStructureBuffer(node, false);
    }


    /**
     * Get an evio structure (bank, seg, or tagseg) in ByteBuffer form.
     * Depending on the copy argument, the returned buffer will either be
     * a copy of or a view into the data of this reader's buffer.<p>
     * This method is synchronized due to the bulk, relative gets & puts.
     *
     * @param node node object representing evio structure of interest
     * @param copy if <code>true</code>, then return a copy as opposed to a
     *        view into this reader object's buffer.
     * @return ByteBuffer object containing structure's bytes. Position and limit are
     *         set for reading.
     * @throws EvioException if object closed
     */
    /*synchronized*/ std::shared_ptr<ByteBuffer> EvioCompactReaderV4::getStructureBuffer(std::shared_ptr<EvioNode> & node, bool copy) {

            if (closed) {
                throw EvioException("object closed");
            }

            return node->getStructureBuffer(copy);
    }



    /**
     * This only sets the position to its initial value.
     */
    /*synchronized*/ void EvioCompactReaderV4::close() {
        byteBuffer->position(initialPosition);
        closed = true;
    }


    /**
     * This is the number of events in the file/buffer. Any dictionary event is <b>not</b>
     * included in the count. In versions 3 and earlier, it is not computed unless
     * asked for, and if asked for it is computed and cached.
     *
     * @return the number of events in the file.
     */
    uint32_t EvioCompactReaderV4::getEventCount() {return eventCount;}

    /**
     * This is the number of blocks in the file including the empty
     * block usually at the end of version 4 files/buffers.
     * For version 3 files, a block size read from the first block is used
     * to calculate the result.
     * It is not computed unless in random access mode or is
     * asked for, and if asked for it is computed and cached.
     *
     * @return the number of blocks in the file (estimate for version 3 files)
     */
    uint32_t EvioCompactReaderV4::getBlockCount() {return blockCount;}

    /**
     * Save the internal byte buffer to the given file
     * (overwrites existing file).
     *
     * @param fileName  name of file to write
     * @throws IOException if error writing to file
     * @throws EvioException if fileName arg is null;
     *                       if object closed
     */
    void EvioCompactReaderV4::toFile(std::string const & fileName) {
            if (fileName.empty()) {
                throw EvioException("empty fileName arg");
            }
            File f = new File(fileName);
            toFile(f);
    }

    /**
     * Save the internal byte buffer to the given file
     * (overwrites existing file).
     *
     * @param file  object of file to write
     * @throws EvioException if file arg is null;
     *                       if object closed
     * @throws IOException if error writing to file
     */
    /*synchronized*/ void EvioCompactReaderV4::toFile(File & file) {
            if (file == null) {
                throw EvioException("null file arg");
            }

            if (closed) {
                throw EvioException("object closed");
            }

            // Remember where we were
            size_t pos = byteBuffer->position();

            // Write the file
            FileOutputStream fos = new FileOutputStream(file);
            FileChannel channel = fos.getChannel();
            channel.write(byteBuffer);
            channel.close();

            // Go back to where we were
            byteBuffer->position(pos);
    }



}