//
// Copyright (c) 2020, Jefferson Science Associates
//
// Thomas Jefferson National Accelerator Facility
// EPSCI Group
//
// 12000, Jefferson Ave, Newport News, VA 23606
// Phone : (757)-269-7100
//


#include "EvioReaderV4.h"

namespace evio {

    /**
     * This method saves the current state of this EvioReader object.
     * @return the current state of this EvioReader object.
     */
    EvioReaderV4::ReaderState * EvioReaderV4::getState() {
        auto * currentState = new ReaderState();
        currentState->lastBlock   = lastBlock;
        currentState->eventNumber = eventNumber;
        currentState->blockNumberExpected = blockNumberExpected;

        if (sequentialRead) {
            currentState->filePosition = file.tellg();
        }
        currentState->byteBufferLimit = byteBuffer->limit();
        currentState->byteBufferPosition = byteBuffer->position();

        if (evioVersion > 3) {
            currentState->blockHeader4 = blockHeader4;
        }
        else {
            currentState->blockHeader2 = blockHeader2;
        }

        return currentState;
    }


    /**
     * This method restores a previously saved state of this EvioReader object.
     * @param state a previously stored state of this EvioReader object.
     */
    void EvioReaderV4::restoreState(ReaderState * state) {
        lastBlock   = state->lastBlock;
        eventNumber = state->eventNumber;
        blockNumberExpected = state->blockNumberExpected;

        if (sequentialRead) {
            file.seekg(state->filePosition);
        }
        byteBuffer->limit(state->byteBufferLimit);
        byteBuffer->position(state->byteBufferPosition);

        if (evioVersion > 3) {
            blockHeader = blockHeader4 = state->blockHeader4;
        }
        else {
            blockHeader = blockHeader2 = state->blockHeader2;
        }
    }


    //------------------------


    /**
     * Constructor for reading an event file.
     * Do <b>not</b> set sequential to false for remote files.
     *
     * @param path the full path to the file that contains events.
     *             For writing event files, use an <code>EventWriter</code> object.
     * @param checkBlkNumSeq if <code>true</code> check the block number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1
     * @see EventWriter
     * @throws EvioException if file arg is null; if read failure;
     *                       if first block number != 1 when checkBlkNumSeq arg is true
     */
    EvioReaderV4::EvioReaderV4(string const & path, bool checkBlkNumSeq, bool synced) {

        if (path.empty()) {
            throw EvioException("path is empty");
        }

        // "ate" mode flag will go immediately to file's end (do this to get its size)
        file.open(path, std::ios::binary | std::ios::ate);
        // Record file length
        fileBytes = file.tellg();
        // Go back to beginning of file
        file.seekg(0);

        if (fileBytes < 40) {
            throw EvioException("File too small to have valid evio data");
        }

        checkBlockNumSeq = checkBlkNumSeq;
        synchronized = synced;
        sequentialRead = true;
        initialPosition = 0;

        // Look at the first block header to get various info like endianness and version.
        // Store it for later reference in blockHeader2,4 and in other variables.

        // Create buffer of size 32 bytes
        size_t bytesToRead = 32;
        auto headerBuffer = std::make_shared<ByteBuffer>(bytesToRead);
        auto headerBytes = headerBuffer->array();

        // Read 32 bytes of file's first block header
        file.read(reinterpret_cast<char *>(headerBytes), bytesToRead);
        if (file.fail()) {
            throw EvioException("I/O error reading file");
        }

        parseFirstHeader(headerBuffer);
        file.seekg(0);

        parser = std::make_shared<EventParser>();

        // What we do from here depends on the evio format version.
        // Do not use memory mapping as the Java version did.
        // Since evio data files tend to be large (> 2GB), memory mapping
        // can be slower than conventional reads.
        if (evioVersion < 4) {
            // Remember, no dictionaries exist for these early versions
//dataStream = new DataInputStream(fileInputStream);
             prepareForSequentialRead();
        }
        // For version 4 ...
        else {
//dataStream = new DataInputStream(fileInputStream);
            prepareForSequentialRead();
            if (blockHeader4->hasDictionary()) {
                // Dictionary is always the first event
                auto dict = parseNextEvent();
                if (dict != nullptr) {
                    auto strs = dict->getStringData();
                    dictionaryXML = strs[0];
                }
            }
        }
    }


    /**
     * Constructor for reading a buffer.
     *
     * @param bb the buffer that contains events.
     * @param checkBlkNumSeq if <code>true</code> check the block number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1
     * @see EventWriter
     * @throws EvioException if buffer arg is null;
     *                       if first block number != 1 when checkBlkNumSeq arg is true
     */
    EvioReaderV4::EvioReaderV4(std::shared_ptr<ByteBuffer> & bb, bool checkBlkNumSeq, bool synced) {

        checkBlockNumSeq = checkBlkNumSeq;
        synchronized = synced;
        byteBuffer = bb->slice(); // remove necessity to track initial position

        // Look at the first block header to get various info like endianness and version.
        // Store it for later reference in blockHeader2,4 and in variables.
        // Position is moved past header.
        parseFirstHeader(byteBuffer);
        // Move position back to beginning
        byteBuffer->position(0);

        // For the latest evio format, generate a table
        // of all event positions in buffer for random access.
        if (evioVersion > 3) {
// std::cout << "EvioReader const: evioVersion = " <, evioVersion << ", create mem handler" << std::endl;
            generateEventPositions(byteBuffer);
            if (blockHeader4->hasDictionary()) {
                // Jump to the first event
                prepareForBufferRead(byteBuffer);
                // Dictionary is the first event
                readDictionary(byteBuffer);
            }
        }
        else {
            // Setting the byte order is only necessary if someone hands
            // this method a buffer in which the byte order is improperly set.
            byteBuffer->order(byteOrder);
            prepareForBufferRead(byteBuffer);
        }

        parser =  std::make_shared<EventParser>();
    }


    /**
     * Generate a table (vector) of positions of events in file/buffer.
     * This method does <b>not</b> affect the byteBuffer position, eventNumber,
     * or lastBlock values. Only called if there are at least 32 bytes available.
     * Valid only in versions 4 and later.
     *
     * @param bb buffer to analyze
     * @return the number of bytes representing all the full blocks
     *         contained in the given byte buffer.
     * @throws EvioException if bad file format
     */
    size_t EvioReaderV4::generateEventPositions(std::shared_ptr<ByteBuffer> & bb) {

            uint32_t      blockSize, blockHdrSize, blockEventCount, magicNum;
            uint32_t      byteInfo, byteLen, bytesLeft, position;
            bool          firstBlock=true, hasDictionary=false;
//            bool          curLastBlock;

            eventPositions.reserve(20000);

            // Start at the beginning of byteBuffer
            position  = 0;
            bytesLeft = bb->limit();

            while (bytesLeft > 0) {
                // Check to see if enough data to read block header.
                // If not return the amount of memory we've used/read.
                if (bytesLeft < 32) {
//std::cout << "return, not enough to read header, bytes left = " << bytesLeft << std::endl;
                    return position;
                }

                // File is now positioned before block header.
                // Look at block header to get info.  Swapping is taken care of
                byteInfo        = bb->getUInt(position + 4*BlockHeaderV4::EV_VERSION);
                blockSize       = bb->getUInt(position + 4*BlockHeaderV4::EV_BLOCKSIZE);
                blockHdrSize    = bb->getUInt(position + 4*BlockHeaderV4::EV_HEADERSIZE);
                blockEventCount = bb->getUInt(position + 4*BlockHeaderV4::EV_COUNT);
                magicNum        = bb->getUInt(position + 4*BlockHeaderV4::EV_MAGIC);

//            std::cout << "    genEvTablePos: pos " << position <<
//                               ", blk ev count = " << blockEventCount <<
//                               ", blockSize = " << blockSize <<
//                               ", blockHdrSize = " << blockHdrSize << showbase <<
//                               ", byteInfo = " << hex << byteInfo <<
//                               ", magic # = " << magicNum << dec << std::endl;

                // If magic # is not right, file is not in proper format
                if (magicNum != BlockHeaderV4::MAGIC_NUMBER) {
                    throw EvioException("Bad evio format: block header magic # incorrect");
                }

                // Check lengths in block header
                if (blockSize < 8 || blockHdrSize < 8) {
                    throw EvioException("Bad evio format: (block: total len = " + std::to_string(blockSize) +
                                        ", header len = " + std::to_string(blockHdrSize) + ")" );
                }

                // Check to see if the whole block is within the mapped memory.
                // If not return the amount of memory we've used/read.
                if (4*blockSize > bytesLeft) {
//std::cout << "    4*blockSize = " << std::to_string(4*blockSize) + " >? bytesLeft = " <<
//             std::to_string(bytesLeft) + ", pos = " + std::to_string(position) << std::endl;
//std::cout << "return, not enough to read all block data" << std::endl;
                    return position;
                }

                blockCount++;
//            curLastBlock = BlockHeaderV4.isLastBlock(byteInfo);
                if (firstBlock) {
                    hasDictionary = BlockHeaderV4::hasDictionary(byteInfo);
                }

//std::cout << "    genEvTablePos: blk count = " << blockCount <<
//                   ", total ev count = " << (eventCount + blockEventCount) <<
//                   "\n                   firstBlock = " << firstBlock <<
//                /*   ", isLastBlock = " + curLastBlock + */
//                   ", hasDict = " << hasDictionary <<
//                   ", pos = " << position << std::endl;

                // Hop over block header to data
                position  += 4*blockHdrSize;
                bytesLeft -= 4*blockHdrSize;

//std::cout << "    hopped blk hdr, bytesLeft = " << bytesLeft << ", pos = " << position << std::endl;

                // Check for a dictionary - the first event in the first block.
                // It's not included in the header block count, but we must take
                // it into account by skipping over it.
                if (firstBlock && hasDictionary) {
                    // Get its length - bank's len does not include itself
                    byteLen = 4*(bb->getUInt(position) + 1);

                    if (byteLen < 4) {
                        throw EvioException("Bad evio format: bad bank length");
                    }

                    // Skip over dictionary
                    position  += byteLen;
                    bytesLeft -= byteLen;
//std::cout << "    hopped dict, dic bytes = " << byteLen << ", bytesLeft = " <<
//             bytesLeft << ", pos = " << position << std::endl;
                }

                firstBlock = false;

                // For each event in block, store its location
                for (int i=0; i < blockEventCount; i++) {
                    // Sanity check - must have at least 1 header's amount left
                    if (bytesLeft < 8) {
                        throw EvioException("Bad evio format: not enough data to read event (bad bank len?)");
                    }

                    // Get length of current event (including full header)
                    byteLen = 4*(bb->getInt(position) + 1);
                    if (byteLen < 4 || bytesLeft < byteLen) {
                        throw EvioException("Bad evio format: bad bank length");
                    }
                    bytesLeft -= byteLen;

                    // Store current position
                    eventPositions.push_back(position);
                    eventCount++;

                    position += byteLen;
//std::cout << "    hopped event " << (i+1) << ", bytesLeft = " << bytesLeft << ", pos = " << position << std::endl;
                }
            }

            return position;
    }


    /** {@inheritDoc} */
   void EvioReaderV4::setBuffer(std::shared_ptr<ByteBuffer> & buf) {
        close();

        if (buf == nullptr) {
            throw EvioException("null buf arg");
        }

        lastBlock           =  false;
        eventNumber         =  0;
        blockCount          =  0;
        eventCount          = -1;
        blockNumberExpected =  1;
        dictionaryXML       =  "";
        initialPosition     =  buf->position();
        sequentialRead      = false;

        byteBuffer = buf->slice();
        parseFirstHeader(byteBuffer);
        byteBuffer->position(0);

        if (evioVersion > 3) {
            generateEventPositions(byteBuffer);
            if (blockHeader4->hasDictionary()) {
                // Jump to the first event
                prepareForBufferRead(byteBuffer);
                // Dictionary is the first event
                readDictionary(byteBuffer);
            }
        }
        else {
            byteBuffer->order(byteOrder);
            prepareForBufferRead(byteBuffer);
        }

        closed = false;
    }


    /** {@inheritDoc} */
    bool EvioReaderV4::isClosed() {return closed;}


    /** {@inheritDoc} */
    bool EvioReaderV4::checkBlockNumberSequence() {return checkBlockNumSeq;}


    /** {@inheritDoc} */
    ByteOrder & EvioReaderV4::getByteOrder() {return byteOrder;}


    /** {@inheritDoc} */
    uint32_t EvioReaderV4::getEvioVersion() {return evioVersion;}


    /** {@inheritDoc} */
    string EvioReaderV4::getPath() {return path;}


    /** {@inheritDoc} */
    std::shared_ptr<EventParser> & EvioReaderV4::getParser() {return parser;}


    /** {@inheritDoc} */
    void EvioReaderV4::setParser(std::shared_ptr<EventParser> & evParser) {parser = evParser;}


    /** {@inheritDoc} */
    string EvioReaderV4::getDictionaryXML() {return dictionaryXML;}


    /** {@inheritDoc} */
    bool EvioReaderV4::hasDictionaryXML() {return !dictionaryXML.empty();}


    /** {@inheritDoc} */
    size_t EvioReaderV4::getNumEventsRemaining() {return getEventCount() - eventNumber;}


    /**
     * {@inheritDoc}.
     * For files, it works only for evio format versions 2,3 and
     * returns the internal buffer containing an evio block if using sequential access
     * (for example files &gt; 2.1 GB). It returns the memory mapped buffer otherwise.
     */
    std::shared_ptr<ByteBuffer> EvioReaderV4::getByteBuffer() {return byteBuffer;}


    /** {@inheritDoc} */
    size_t EvioReaderV4::fileSize() {return fileBytes;}


    /** {@inheritDoc} */
    std::shared_ptr<IBlockHeader> EvioReaderV4::getFirstBlockHeader() {return firstBlockHeader;}


    /**
     * Reads 8 words of the first block (physical record) header in order to determine
     * the evio version # and endianness of the file or buffer in question. These things
     * do <b>not</b> need to be examined in subsequent block headers. Called only by
     * synchronized methods or constructors.
     *
     * @throws EvioException if buffer too small, contains invalid data,
     *                       or bad block # sequence
     */
    void EvioReaderV4::parseFirstHeader(std::shared_ptr<ByteBuffer> & headerBuf) {

        // Check buffer length
        headerBuf->position(0);
        if (headerBuf->remaining() < 32) {
            throw EvioException("buffer too small");
        }

        // Get the file's version # and byte order
        byteOrder = headerBuf->order();

        int magicNumber = headerBuf->getInt(MAGIC_OFFSET);

        if (magicNumber != IBlockHeader::MAGIC_NUMBER) {
            swap = true;

            byteOrder = byteOrder.getOppositeEndian();
            headerBuf->order(byteOrder);

            // Reread magic number to make sure things are OK
            magicNumber = headerBuf->getInt(MAGIC_OFFSET);
            if (magicNumber != IBlockHeader::MAGIC_NUMBER) {
                std::cout << "ERROR reread magic # (" << std::to_string(magicNumber) <<
                          ") & still not right" << std::endl;
                throw EvioException("bad magic #");
            }
        }

        // Check the version number. This requires peeking ahead 5 ints or 20 bytes.
        evioVersion = headerBuf->getInt(VERSION_OFFSET) & VERSION_MASK;
        if (evioVersion < 1)  {
            throw EvioException("bad version");
        }
//            std::cout << "Evio version# = " << evioVersion << std::endl;

        if (evioVersion >= 4) {
            blockHeader4->setBufferStartingPosition(0);

//                int pos = 0;
//                std::cout << "BlockHeader v4:" << std::endl << hex << showbase;
//                std::cout << "   block length  = " << headerBuf->getInt(pos) << std::endl; pos+=4;
//                std::cout << "   block number  = " << headerBuf->getInt(pos) << std::endl; pos+=4;
//                std::cout << "   header length = " << headerBuf->getInt(pos) << std::endl; pos+=4;
//                std::cout << "   event count   = " << headerBuf->getInt(pos) << std::endl; pos+=8;
//                std::cout << "   version       = " << headerBuf->getInt(pos) << std::endl; pos+=8;
//                std::cout << "   magic number  = " << headerBuf->getInt(pos) << std::endl; pos+=4;
//                std::cout << std::endl << dec;

            // Read the header data
            blockHeader4->setSize(        headerBuf->getInt());
            blockHeader4->setNumber(      headerBuf->getInt());
            blockHeader4->setHeaderLength(headerBuf->getInt());
            blockHeader4->setEventCount(  headerBuf->getInt());
            blockHeader4->setReserved1(   headerBuf->getInt());

            // Use 6th word to set bit info & version
            blockHeader4->parseToBitInfo(headerBuf->getInt());
            blockHeader4->setVersion(evioVersion);
            lastBlock = blockHeader4->getBitInfo(1);
            blockHeader4->setReserved2(headerBuf->getInt());
            blockHeader4->setMagicNumber(headerBuf->getInt());
            blockHeader4->setByteOrder(byteOrder);
            blockHeader = blockHeader4;

            // Copy it
            firstBlockHeader = firstBlockHeader4 = std::make_shared<BlockHeaderV4>(blockHeader4);

            // Deal with non-standard header lengths here
            int64_t headerLenDiff = blockHeader4->getHeaderLength() - BlockHeaderV4::HEADER_SIZE;
            // If too small quit with error since headers have a minimum size
            if (headerLenDiff < 0) {
                throw EvioException("header size too small");
            }

//std::cout << "BlockHeader v4:" << std::endl;
//std::cout << "   block length  = " << blockHeader4.getSize() << " ints" << std::endl;
//std::cout << "   block number  = " << blockHeader4.getNumber() << std::endl;
//std::cout << "   header length = " << blockHeader4.getHeaderLength() + " ints" << std::endl;
//std::cout << "   event count   = " << blockHeader4.getEventCount() << std::endl;
//std::cout << "   version       = " << blockHeader4.getVersion() << std::endl;
//std::cout << "   has Dict      = " << blockHeader4.getBitInfo(0) << std::endl;
//std::cout << "   is End        = " << lastBlock << std::endl << hex;
//std::cout << "   magic number  = " << blockHeader4.getMagicNumber() << std::endl << dec;

        }
        else {
            // Cache the starting position
            blockHeader2->setBufferStartingPosition(0);

            // read the header data.
            blockHeader2->setSize(        headerBuf->getInt());
            blockHeader2->setNumber(      headerBuf->getInt());
            blockHeader2->setHeaderLength(headerBuf->getInt());
            blockHeader2->setStart(       headerBuf->getInt());
            blockHeader2->setEnd(         headerBuf->getInt());
            // skip version
            headerBuf->getInt();
            blockHeader2->setVersion(evioVersion);
            blockHeader2->setReserved1(   headerBuf->getInt());
            blockHeader2->setMagicNumber( headerBuf->getInt());
            blockHeader2->setByteOrder(byteOrder);
            blockHeader = blockHeader2;

            firstBlockHeader = firstBlockHeader2 = std::make_shared<BlockHeaderV2>(blockHeader2);
        }

        // Store this for later regurgitation of blockCount
        firstBlockSize = 4*blockHeader->getSize();

        // check block number if so configured
        if (checkBlockNumSeq) {
            if (blockHeader->getNumber() != blockNumberExpected) {
                std::cout << "block # out of sequence, got " << blockHeader->getNumber() <<
                          " expecting " << blockNumberExpected << std::endl;

                throw EvioException("bad block # sequence");
            }
            blockNumberExpected++;
        }
    }


    /**
     * Reads the first block header into a buffer and gets that
     * buffer ready for first-time read.
     *
     * @throws EvioException if file access problems
     */
    void EvioReaderV4::prepareForSequentialRead() {
        // Create a buffer to hold a chunk of data.
        size_t bytesToRead;

        // Evio format version 4 or greater has a large enough default block size
        // so that reading a single block at a time is not inefficient.
        if (evioVersion > 3) {
            bytesToRead = 4*firstBlockHeader->getSize();
        }
        // Reading data by 32768 byte blocks in older versions is inefficient,
        // so read in 500 block (16MB) chunks.
        else {
            size_t bytesLeftInFile = fileBytes - file.tellg();
            bytesToRead = DEFAULT_READ_BYTES < bytesLeftInFile ?
                          DEFAULT_READ_BYTES : bytesLeftInFile;
        }

        if (byteBuffer->capacity() < bytesToRead) {
            byteBuffer = std::make_shared<ByteBuffer>(bytesToRead);
            byteBuffer->order(byteOrder);
        }
        byteBuffer->clear().limit(bytesToRead);

        // Read the first chunk of data from file
        file.read((char *)(byteBuffer->array() + byteBuffer->arrayOffset()), bytesToRead);
        if (file.fail()) {
            throw EvioException("file read failure");
        }

        // Buffer is ready to read since our file-read was absolute and position did NOT change

        // Position buffer properly (past block header)
        prepareForBufferRead(byteBuffer);
    }


    /**
     * Sets the proper buffer position for first-time read AFTER the first header.
     * @param buffer buffer to prepare
     */
    void EvioReaderV4::prepareForBufferRead(std::shared_ptr<ByteBuffer> & buffer) const {
        // Position after header
        size_t pos = 32;
        buffer->position(pos);

        // Deal with non-standard first header length.
        // No non-standard header lengths in evio version 2 & 3 files.
        if (evioVersion < 4) return;

        int64_t headerLenDiff = blockHeader4->getHeaderLength() - BlockHeaderV4::HEADER_SIZE;
        // Hop over any extra header words
        if (headerLenDiff > 0) {
            for (int i=0; i < headerLenDiff; i++) {
                //std::cout << "Skip extra header int");
                pos += 4;
                buffer->position(pos);
            }
        }

        // When reading files sequentially, the file is read into this buffer.
        // If there's a dictionary present, skip over it now.
    }


    /**
     * Reads the block (physical record) header.
     * Assumes mapped buffer or file is positioned at start of the next block header.
     * If a sequential file:
     *   version 4,   Read the entire next block into internal buffer.
     *   version 1-3, If unused data still exists in internal buffer, don't
     *                read anymore in right now as there is at least 1 block there
     *                (integral # of blocks read in).
     *                If no data in internal buffer read DEFAULT_READ_BYTES or the
     *                rest of the file, whichever is smaller, into the internal buffer.
     *
     * By the time this is called,
     * the version # and byte order have already been determined. Not necessary to do that
     * for each block header that's read. Called from synchronized method.<p>
     *
     * A Bank header is 8, 32-bit ints. The first int is the size of the block in ints
     * (not counting the length itself, i.e., the number of ints to follow).
     *
     * Most users should have no need for this method, since most applications do not
     * care about the block (physical record) header.
     *
     * @return status of read attempt
     * @throws EvioException if file access problems, evio format problems
     */
    IEvioReader::ReadWriteStatus EvioReaderV4::processNextBlock() {

        // We already read the last block header
        if (lastBlock) {
            return IEvioReader::ReadWriteStatus::END_OF_FILE;
        }

        try {
            if (sequentialRead) {
                if (evioVersion < 4) {
                    size_t bytesInBuf = bufferBytesRemaining();
                    if (bytesInBuf == 0) {

                        // How much of the file is left to read?
                        size_t bytesLeftInFile = fileBytes - file.tellg();
                        if (bytesLeftInFile < 32L) {
                            return IEvioReader::ReadWriteStatus::END_OF_FILE;
                        }

                        // The block size is 32kB which is on the small side.
                        // We want to read in 16MB (DEFAULT_READ_BYTES) or so
                        // at once for efficiency.
                        size_t bytesToRead = DEFAULT_READ_BYTES < bytesLeftInFile ?
                                             DEFAULT_READ_BYTES : bytesLeftInFile;

                        // Reset buffer
                        byteBuffer->position(0).limit(bytesToRead);

                        // Read the entire chunk of data
                        file.read((char *)(byteBuffer->array() + byteBuffer->arrayOffset()), bytesToRead);
                        if (file.fail()) {
                            throw EvioException("file read failure");
                        }
                        byteBuffer->limit(bytesToRead);

                        // Now keeping track of pos in this new blockBuffer
                        blockHeader->setBufferStartingPosition(0);
                    }
                    else if (bytesInBuf % 32768 == 0) {
                        // Next block header starts at this position in buffer
                        blockHeader->setBufferStartingPosition(byteBuffer->position());
                    }
                    else {
                        throw EvioException("file contains non-integral # of 32768 byte blocks");
                    }
                }
                else {
                    // Enough data left to read len?
                    if (fileBytes - file.tellg() < 4L) {
                        return IEvioReader::ReadWriteStatus::END_OF_FILE;
                    }

                    // Read len of block in 32 bit words
                    uint32_t blkSize;
                    file >> blkSize;
                    if (file.fail()) {
                        throw EvioException("file read failure");
                    }
                    if (swap) blkSize = SWAP_32(blkSize);
                    // Change to bytes
                    uint32_t blkBytes = 4 * blkSize;

                    // Enough data left to read rest of block?
                    if (fileBytes - file.tellg() < blkBytes-4) {
                        return IEvioReader::ReadWriteStatus::END_OF_FILE;
                    }

                    // Create a buffer to hold the entire first block of data
                    if (byteBuffer->capacity() >= blkBytes) {
                        byteBuffer->clear();
                        byteBuffer->limit(blkBytes);
                    }
                    else {
                        // Make this bigger than necessary so we're not constantly reallocating
                        byteBuffer = std::make_shared<ByteBuffer>(blkBytes + 10000);
                        byteBuffer->limit(blkBytes);
                        byteBuffer->order(byteOrder);
                    }

                    // Read the entire block of data.
                    // First put in length we just read, but leave position - 0
                    byteBuffer->putInt(0, blkSize);

                    // Now the rest of the block (already put int, 4 bytes, in)
                    file.read((char *)(byteBuffer->array() + byteBuffer->arrayOffset() + 4), blkBytes-4);
                    if (file.fail()) {
                        throw EvioException("file read failure");
                    }

                    // Now keeping track of pos in this new blockBuffer
                    blockHeader->setBufferStartingPosition(0);
                }
            }
            else {
                if (byteBuffer->remaining() < 32) {
                    byteBuffer->clear();
                    return IEvioReader::ReadWriteStatus::END_OF_FILE;
                }
                // Record starting position
                blockHeader->setBufferStartingPosition(byteBuffer->position());
            }

            if (evioVersion >= 4) {
                // Read the header data.
                blockHeader4->setSize(byteBuffer->getInt());
                blockHeader4->setNumber(byteBuffer->getInt());
                blockHeader4->setHeaderLength(byteBuffer->getInt());
                blockHeader4->setEventCount(byteBuffer->getInt());
                blockHeader4->setReserved1(byteBuffer->getInt());
                // Use 6th word to set bit info
                blockHeader4->parseToBitInfo(byteBuffer->getInt());
                blockHeader4->setVersion(evioVersion);
                lastBlock = blockHeader4->getBitInfo(1);
                blockHeader4->setReserved2(byteBuffer->getInt());
                blockHeader4->setMagicNumber(byteBuffer->getInt());
                blockHeader = blockHeader4;

                // Deal with non-standard header lengths here
                int64_t headerLenDiff = blockHeader4->getHeaderLength() - BlockHeaderV4::HEADER_SIZE;
                // If too small quit with error since headers have a minimum size
                if (headerLenDiff < 0) {
                    return IEvioReader::ReadWriteStatus::EVIO_EXCEPTION;
                }
                    // If bigger, read extra ints
                else if (headerLenDiff > 0) {
                    for (int i=0; i < headerLenDiff; i++) {
                        byteBuffer->getInt();
                    }
                }
            }
            else if (evioVersion < 4) {
                // read the header data
                blockHeader2->setSize(byteBuffer->getInt());
                blockHeader2->setNumber(byteBuffer->getInt());
                blockHeader2->setHeaderLength(byteBuffer->getInt());
                blockHeader2->setStart(byteBuffer->getInt());
                blockHeader2->setEnd(byteBuffer->getInt());
                // skip version
                byteBuffer->getInt();
                blockHeader2->setVersion(evioVersion);
                blockHeader2->setReserved1(byteBuffer->getInt());
                blockHeader2->setMagicNumber(byteBuffer->getInt());
                blockHeader = blockHeader2;
            }
            else {
                // bad version # - should never happen
                return IEvioReader::ReadWriteStatus::EVIO_EXCEPTION;
            }

            // check block number if so configured
            if (checkBlockNumSeq) {
                if (blockHeader->getNumber() != blockNumberExpected) {

                    std::cout << "block # out of sequence, got " << std::to_string(blockHeader->getNumber()) +
                                 " expecting " << std::to_string(blockNumberExpected) << std::endl;

                    return IEvioReader::ReadWriteStatus::EVIO_EXCEPTION;
                }
                blockNumberExpected++;
            }
        }
        catch (EvioException & e) {
            return IEvioReader::ReadWriteStatus::EVIO_EXCEPTION;
        }
//        catch (BufferUnderflowException & a) {
//            std::cout << "ERROR endOfBuffer " << a << std::endl;
//            byteBuffer->clear();
//            return IEvioReader::ReadWriteStatus::UNKNOWN_ERROR;
//        }

        return IEvioReader::ReadWriteStatus::SUCCESS;
    }


    /**
     * This method is only called once at the very beginning if buffer is known to have
     * a dictionary. It then reads that dictionary. Only called in format versions 4 & up.
     * Position buffer after dictionary. Called from synchronized method or constructor.
     *
     * @since 4.0
     * @param buffer buffer to read to get dictionary
     * @throws underflow_error if not enough data in buffer.
     * @throws EvioException if failed read due to bad buffer format;
     *                       if version 3 or earlier
     */
    void EvioReaderV4::readDictionary(std::shared_ptr<ByteBuffer> & buffer) {

        if (evioVersion < 4) {
            throw EvioException("Unsupported version (" + std::to_string(evioVersion) + ")");
        }

        // How many bytes remain in this buffer?
        size_t bytesRemaining = buffer->remaining();
        if (bytesRemaining < 12) {
            throw std::underflow_error("Not enough data in buffer");
        }

        // Once here, we are assured the entire next event is in this buffer.
        uint32_t length = buffer->getUInt();
        bytesRemaining -= 4;

        // Since we're only interested in length, read but ignore rest of the header.
        buffer->getInt();
        bytesRemaining -= 4;

        // get the raw data
        uint32_t eventDataSizeBytes = 4*(length - 1);
        if (bytesRemaining < eventDataSizeBytes) {
            throw EvioException("Not enough data in buffer");
        }

        // Read in dictionary data
        uint8_t bytes[eventDataSizeBytes];
        buffer->getBytes(bytes, eventDataSizeBytes);
        std::vector<string> strs;

        // This is the very first event and must be a dictionary
        BaseStructure::unpackRawBytesToStrings(bytes, eventDataSizeBytes, strs);
        if (strs.empty()) {
            throw EvioException("Data in bad format");
        }
        dictionaryXML = strs[0];
    }


    /** {@inheritDoc} */
    std::shared_ptr<EvioEvent> EvioReaderV4::getEvent(size_t index) {
        if (sequentialRead || evioVersion < 4) {
            // Do not fully parse events up to index_TH event
            return gotoEventNumber(index, false);
        }

        //  Version 4 and up && non sequential
        return getEventV4(index);
    }


    /**
     * Get the event in the file/buffer at a given index (starting at 1).
     * It is only valid for evio versions 4+.
     * As useful as this sounds, most applications will probably call
     * {@link #parseNextEvent()} or {@link #parseEvent(int)} instead,
     * since it combines combines getting an event with parsing it.
     * Only called if not sequential reading.<p>
     *
     * @param  index the event number in a 1,2,..N counting sense, from beginning of file/buffer.
     * @return the event in the file/buffer at the given index or null if none
     * @throws EvioException if failed read due to bad file/buffer format;
     *                       if object closed
     */
    std::shared_ptr<EvioEvent> EvioReaderV4::getEventV4(size_t index) {
        // Lock this method
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }

        if (index > getEventCount()) {
            return nullptr;
        }

        if (closed) {
            throw EvioException("object closed");
        }

        index--;

        std::shared_ptr<BankHeader> header;
        auto event = EvioEvent::getInstance(header);

        uint32_t eventDataSizeBytes = 0;
        uint32_t length = byteBuffer->getUInt();
        header->setLength(length);

        // Read and parse second header word
        uint32_t word = byteBuffer->getUInt();
        header->setTag((word >> 16) & 0xfff);
        int dt = (word >> 8) & 0xff;
        // If only 7th bit set, it can be tag=0, num=0, type=0, padding=1.
        // This regularly happens with composite data.
        // However, it that MAY also be the legacy tagsegment type
        // with no padding information. Ignore this as having tag & num
        // in legacy code is probably rare.
        //if (dt == 0x40) {
        //    type = DataType.TAGSEGMENT.getValue();
        //    padding = 0;
        //}
        header->setDataType(dt & 0x3f);
        header->setPadding(dt >> 6);
        header->setNumber(word & 0xff);

        // Read the raw data
        eventDataSizeBytes = 4*(length - 1);
        event->setRawBytes((uint8_t *)(byteBuffer->array() + byteBuffer->arrayOffset() + byteBuffer->position()),
                           (size_t)eventDataSizeBytes);
        byteBuffer->position(byteBuffer->position() + eventDataSizeBytes);

        event->setByteOrder(byteOrder);
        event->setEventNumber(++eventNumber);

        return event;
    }


    /** {@inheritDoc} */
    std::shared_ptr<EvioEvent> EvioReaderV4::parseEvent(size_t index) {
        // Lock this method
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }

        auto event = getEvent(index);
        if (event != nullptr) parseEvent(event);
        return event;
    }


    /** {@inheritDoc} */
    std::shared_ptr<EvioEvent> EvioReaderV4::nextEvent() {

        // Lock this method
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }

        if (!sequentialRead && evioVersion > 3) {
            return getEvent(eventNumber+1);
        }

        if (closed) {
            throw EvioException("object closed");
        }

        std::shared_ptr<BankHeader> header;
        auto event = EvioEvent::getInstance(header);
        size_t currentPosition = byteBuffer->position();

        // How many bytes remain in this block until we reach the next block header?
        uint32_t blkBytesRemaining = blockBytesRemaining();

        // Are we at the block boundary? If so, read/skip-over next header.
        // Read in more blocks of data if necessary.
        //
        // version 1-3:
        // We now read in bigger chunks that are integral multiples of a single block
        // (32768 bytes). Must see if we have to deal with an event crossing physical
        // record boundaries. Previously, java evio only read 1 block at a time.
        if (blkBytesRemaining == 0) {
            IEvioReader::ReadWriteStatus status = processNextBlock();
            if (status == IEvioReader::ReadWriteStatus::SUCCESS) {
                return nextEvent();
            }
            else if (status == IEvioReader::ReadWriteStatus::END_OF_FILE) {
                return nullptr;
            }
            else {
                throw EvioException("Failed reading block header in nextEvent.");
            }
        }
        // Or have we already read in the last event?
        // If jevio versions 1-3, the last block may not be full.
        // Thus bytesRemaining may be > 0, but we may have read
        // in all the existing data. (This should never happen in version 4).
        else if (blockHeader->getBufferEndingPosition() == currentPosition) {
            return nullptr;
        }

        // Version   4: Once here, we are assured the entire next event is in this block.
        //
        // Version 1-3: No matter what, we can get the length of the next event.
        //              This is because we read in multiples of blocks each with
        //              an integral number of 32 bit words.
        uint32_t length = byteBuffer->getUInt();
        header->setLength(length);
        blkBytesRemaining -= 4; // just read in 4 bytes

        // Versions 1-3: if we were unlucky, after reading the length
        //               there are no bytes remaining in this block.
        // Don't really need the "if (version < 4)" here except for clarity.
        if (evioVersion < 4) {
            if (bufferBytesRemaining() == 0) {
                IEvioReader::ReadWriteStatus status = processNextBlock();
                if (status == IEvioReader::ReadWriteStatus::END_OF_FILE) {
                    return nullptr;
                }
                else if (status != IEvioReader::ReadWriteStatus::SUCCESS) {
                    throw EvioException("Failed reading block header in nextEvent.");
                }
                blkBytesRemaining = blockBytesRemaining();
            }
        }

        // Now we should be good to go, except data may cross block boundary.
        // In any case, should be able to read the rest of the header.

        // Read and parse second header word
        uint32_t word = byteBuffer->getUInt();
        header->setTag((word >> 16) & 0xfff);
        int dt = (word >> 8) & 0xff;
        header->setDataType(dt & 0x3f);
        header->setPadding(dt >> 6);
        header->setNumber(word & 0xff);

        blkBytesRemaining -= 4; // just read in 4 bytes

        // get the raw data
        uint32_t eventDataSizeBytes = 4*(length - 1);

        try {
            auto *bytes = new uint8_t[eventDataSizeBytes];

            uint32_t bytesToGo = eventDataSizeBytes;
            uint32_t offset = 0;

            // Don't really need the "if (version < 4)" here except for clarity.
            if (evioVersion < 4) {

                // Be in while loop if have to cross block boundary[ies].
                while (bytesToGo > 0) {

                    // Don't read more than what is left in current block
                    uint32_t bytesToReadNow = bytesToGo > blkBytesRemaining ?
                                              blkBytesRemaining : bytesToGo;

                    // Read in bytes remaining in internal buffer
                    byteBuffer->getBytes(bytes + offset, bytesToReadNow);
                    offset               += bytesToReadNow;
                    bytesToGo            -= bytesToReadNow;
                    blkBytesRemaining    -= bytesToReadNow;

                    if (blkBytesRemaining == 0) {
                        IEvioReader::ReadWriteStatus status = processNextBlock();
                        if (status == IEvioReader::ReadWriteStatus::END_OF_FILE) {
                            return nullptr;
                        }
                        else if (status != IEvioReader::ReadWriteStatus::SUCCESS) {
                            throw EvioException("Failed reading block header after crossing boundary in nextEvent.");
                        }

                        blkBytesRemaining = blockBytesRemaining();
                    }
                }
            }

            // Last (perhaps only) read
            byteBuffer->getBytes(bytes + offset, bytesToGo);
            event->setRawBytes(bytes, eventDataSizeBytes);
            event->setByteOrder(byteOrder); // add this to track endianness, timmer
            // Don't worry about dictionaries here as version must be 1-3
            event->setEventNumber(++eventNumber);
            return event;
        }
        catch (EvioException & e) {
            std::cout << e.what() << std::endl;
            return nullptr;
        }
    }


    /** {@inheritDoc} */
    std::shared_ptr<EvioEvent> EvioReaderV4::parseNextEvent() {
        // Lock this method
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }

        auto event = nextEvent();
        if (event != nullptr) {
            parseEvent(event);
        }
        return event;
    }


    /** {@inheritDoc} */
    void EvioReaderV4::parseEvent(std::shared_ptr<EvioEvent> evioEvent) {
        // This method is called by locked methods
        parser->parseEvent(evioEvent);
    }


    /** {@inheritDoc} */
    uint32_t EvioReaderV4::getEventArray(size_t evNumber, std::vector<uint8_t> & vec) {
        auto ev = gotoEventNumber(evNumber, false);
        if (ev == nullptr) {
            throw EvioException("event number must be > 0");
        }

        uint32_t numBytes = ev->getTotalBytes();
        vec.clear();
        vec.reserve(numBytes);
        ev->writeQuick(vec.data());
        return numBytes;
    }


    /** {@inheritDoc} */
    uint32_t EvioReaderV4::getEventBuffer(size_t evNumber, ByteBuffer & buf) {
        auto ev = gotoEventNumber(evNumber, false);
        if (ev == nullptr) {
            throw EvioException("event number must be > 0");
        }

        uint32_t numBytes = ev->getTotalBytes();
        buf.expand(numBytes);
        buf.limit(numBytes).position(0);
        ev->writeQuick(buf.array());

        return numBytes;
    }


    /**
     * Get the number of bytes remaining in the internal byte buffer.
     * Called only by {@link #nextEvent()}.
     * @return the number of bytes remaining in the current block (physical record).
     */
    size_t EvioReaderV4::bufferBytesRemaining() const {return byteBuffer->remaining();}


    /**
     * Get the number of bytes remaining in the current block (physical record).
     * This is used for pathology checks like crossing the block boundary.
     * Called only by {@link #nextEvent()}.
     *
     * @return the number of bytes remaining in the current block (physical record).
     * @throws EvioException if position out of bounds
     */
    uint32_t EvioReaderV4::blockBytesRemaining() const {
        return blockHeader->bytesRemaining(byteBuffer->position());
    }


    /** {@inheritDoc} */
    void EvioReaderV4::rewind() {
        // Lock this method
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }

        if (closed) {
            throw EvioException("object closed");
        }

        if (sequentialRead) {
            file.seekg(initialPosition);
            prepareForSequentialRead();
        }
        else if (evioVersion < 4) {
            byteBuffer->position(initialPosition);
            prepareForBufferRead(byteBuffer);
        }

        lastBlock = false;
        eventNumber = 0;
        blockNumberExpected = 1;

        if (evioVersion < 4) {
            blockHeader = blockHeader4 = std::make_shared<BlockHeaderV4>(firstBlockHeader4);
        }
        else {
            blockHeader = blockHeader2 = std::make_shared<BlockHeaderV2>(firstBlockHeader2);
        }

        blockHeader->setBufferStartingPosition(initialPosition);

        if (sequentialRead && hasDictionaryXML()) {
            // Dictionary is always the first event so skip over it.
            // For sequential reads, do this after each rewind.
            nextEvent();
        }
    }

    /** {@inheritDoc} */
    ssize_t EvioReaderV4::position() {
        // Lock this method
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }

        if (!sequentialRead && evioVersion > 3) return -1L;

        if (closed) {
            throw EvioException("object closed");
        }

        if (sequentialRead) {
            return file.tellg();
        }
        return byteBuffer->position();
    }

    /** {@inheritDoc} */
    void EvioReaderV4::close() {
        // Lock this method
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }

        if (closed) {
            return;
        }

        if (sequentialRead) {
            file.close();
        }
        else {
            try {
                byteBuffer->position(initialPosition);
            }
            catch (EvioException & e) {}
        }

        closed = true;
    }


    /** {@inheritDoc} */
    std::shared_ptr<IBlockHeader> EvioReaderV4::getCurrentBlockHeader() {return blockHeader;}


    /** {@inheritDoc} */
    std::shared_ptr<EvioEvent> EvioReaderV4::gotoEventNumber(size_t evNumber) {
        return gotoEventNumber(evNumber, true);
    }


    /**
     * Go to a specific event in the file. The events are numbered 1..N.
     * This number is transient--it is not part of the event as stored in the evio file.
     * Before version 4, this does the work for {@link #getEvent(int)}.
     *
     * @param  evNumber the event number in a 1,2,..N counting sense, from beginning of file/buffer.
     * @param  parse if {@code true}, parse the desired event
     * @return the specified event in file or null if there's an error or nothing at that event #.
     * @throws EvioException if object closed; if failed file access
     */
    std::shared_ptr<EvioEvent> EvioReaderV4::gotoEventNumber(size_t evNumber, bool parse) {
        // Lock this method
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }

        if (closed) {
            throw EvioException("object closed");
        }

        if (!sequentialRead && evioVersion > 3) {
            try {
                if (parse) {
                    return parseEvent(evNumber);
                }
                else {
                    return getEvent(evNumber);
                }
            }
            catch (EvioException & e) {
                return nullptr;
            }
        }

        rewind();
        std::shared_ptr<EvioEvent> event;

        try {
            // get the first evNumber - 1 events without parsing
            for (int i = 1; i < evNumber; i++) {
                event = nextEvent();
                if (event == nullptr) {
                    throw EvioException("Asked to go to event: " + std::to_string(evNumber) +
                                        ", which is beyond the end of file");
                }
            }
            // get one more event, the evNumber'th event
            if (parse) {
                return parseNextEvent();
            }
            else {
                return nextEvent();
            }
        }
        catch (EvioException & e) {
            std::cout << e.what() << std::endl;
        }
        return nullptr;
    }


    /** {@inheritDoc} */
    size_t EvioReaderV4::getEventCount() {

        // Lock this method
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }

        if (closed) {
                throw EvioException("object closed");
            }

            if (!sequentialRead && evioVersion > 3) {
                // Already calculated by calling generateEventPositions in constructor ...
                return eventCount;
            }

            if (eventCount < 0) {
                // The difficulty is that this method can be called at
                // any time. So we need to save our state and then restore
                // it when we're done.
                ReaderState *state = getState();

                rewind();
                eventCount = 0;

                while (nextEvent() != nullptr) {
                    eventCount++;
                }

                // If sequential access to v2 file, then nextEvent() places
                // new data into byteBuffer. Restoring the original state
                // is useless without also restoring/re-reading the data.
                if (sequentialRead) {
                    rewind();

                    // Skip dictionary
                    if (hasDictionaryXML()) {
                        nextEvent();
                    }

                    // Go back to original event # & therefore buffer data
                    for (int i=1; i < state->eventNumber; i++) {
                        nextEvent();
                    }
                }

                // Restore our original settings
                restoreState(state);
            }

            return eventCount;
    }


    /** {@inheritDoc} */
    size_t EvioReaderV4::getBlockCount() {

        // Lock this method
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }

        if (closed) {
                throw EvioException("object closed");
            }

            if (!sequentialRead && evioVersion > 3) {
                return blockCount;
            }

            if (blockCount < 0) {
                // Although block size is theoretically adjustable, I believe
                // that everyone used 8192 words for the block size in version 3.
                blockCount = (uint32_t) (fileBytes/firstBlockSize);
            }

            return blockCount;
    }


}