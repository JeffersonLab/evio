/**
 * Copyright (c) 2020, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 06/8/2020
 * @author timmer
 */

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
                currentState->filePosition = fileChannel.position();
                currentState->byteBufferLimit = byteBuffer.limit();
                currentState->byteBufferPosition = byteBuffer.position();
        }
        else {
            if (byteBuffer != nullptr) {
                currentState->byteBufferLimit = byteBuffer.limit();
                currentState->byteBufferPosition = byteBuffer.position();
            }
        }

        if (evioVersion > 3) {
            currentState->blockHeader4 = (BlockHeaderV4)blockHeader4.clone();
        }
        else {
            currentState->blockHeader2 = (BlockHeaderV2)blockHeader2.clone();
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
                fileChannel.position(state->filePosition);
                byteBuffer.limit(state->byteBufferLimit);
                byteBuffer.position(state->byteBufferPosition);
        }
        else {
            if (byteBuffer != nullptr) {
                byteBuffer.limit(state->byteBufferLimit);
                byteBuffer.position(state->byteBufferPosition);
            }
        }

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
     * @param sequential     if <code>true</code> read the file sequentially,
     *                       else use memory mapped buffers. If file &gt; 2.1 GB,
     *                       reads are always sequential for the older evio format.
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null;
     *                       if first block number != 1 when checkBlkNumSeq arg is true
     */
    EvioReaderV4::EvioReaderV4(string const & path, bool checkBlkNumSeq, bool sequential) {

            File file = new File(path);

            if (file == null) {
                throw EvioException("File arg is null");
            }
            this.file = file;

            checkBlockNumSeq = checkBlkNumSeq;
            sequentialRead = sequential;
            initialPosition = 0;

            FileInputStream fileInputStream = new FileInputStream(file);
            path = file.getAbsolutePath();
            fileChannel = fileInputStream.getChannel();
            fileBytes = fileChannel.size();

            if (fileBytes < 40) {
                throw new EvioException("File too small to have valid evio data");
            }

            // Look at the first block header to get various info like endianness and version.
            // Store it for later reference in blockHeader2,4 and in other variables.
            ByteBuffer headerBuf = ByteBuffer.allocate(32);
            int bytesRead = 0;
            while (bytesRead < 32) {
                bytesRead += fileChannel.read(headerBuf);
            }
            parseFirstHeader(headerBuf);
            fileChannel.position(0);
            parser = new EventParser();

            // What we do from here depends on the evio format version.
            // If we've got the old version, don't memory map big (> 2.1 GB) files,
            // use sequential reading instead.
            if (evioVersion < 4) {
                // Remember, no dictionaries exist for these early versions

                // Got a big file? If so we cannot use a memory mapped file.
                if (fileBytes > Integer.MAX_VALUE) {
                    sequentialRead = true;
                }

                if (sequentialRead) {
                    // Taken from FileInputStream javadoc:
                    //
                    // "Reading bytes from this [fileInput] stream will increment the channel's
                    //  position.  Changing the channel's position, either explicitly or by
                    //  reading, will change this stream's file position".
                    //
                    // So reading from either the dataStream or fileChannel object will
                    // change both positions.

                    dataStream = new DataInputStream(fileInputStream);
//std::cout << "Big file or reading sequentially for evio versions 2,3");
                    prepareForSequentialRead();
                }
                else {
                    byteBuffer = fileChannel.map(FileChannel.MapMode.READ_ONLY, 0L, fileBytes);
                    byteBuffer.order(byteOrder);
                    prepareForBufferRead(byteBuffer);
//std::cout << "Memory Map versions 2,3");
                }
            }
            // For the new version ...
            else {
                if (sequentialRead) {
                    dataStream = new DataInputStream(fileInputStream);
                    prepareForSequentialRead();
                    if (blockHeader4.hasDictionary()) {
                        // Dictionary is always the first event
                        EvioEvent dict = parseNextEvent();
                        if (dict != null) {
                            String[] strs = dict.getStringData();
                            dictionaryXML = strs[0];
                        }
                    }
                }
                else {
                    // Memory map the file - even the big ones
                    mappedMemoryHandler = new MappedMemoryHandler(fileChannel, byteOrder);
                    if (blockHeader4.hasDictionary()) {
                        ByteBuffer buf = mappedMemoryHandler.getFirstMap();
                        // Jump to the first event
                        prepareForBufferRead(buf);
                        // Dictionary is always the first event
                        readDictionary(buf);
                    }
                }
            }

    }


    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     * @param checkBlkNumSeq if <code>true</code> check the block number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1
     * @see EventWriter
     * @throws EvioException if buffer arg is null;
     *                       if first block number != 1 when checkBlkNumSeq arg is true
     */
    EvioReaderV4::EvioReaderV4(ByteBuffer & byteBuffer, bool checkBlkNumSeq) {

            if (byteBuffer == null) {
                throw EvioException("Buffer arg is null");
            }

            checkBlockNumSeq = checkBlkNumSeq;
            this.byteBuffer = byteBuffer.slice(); // remove necessity to track initial position

            // Look at the first block header to get various info like endianness and version.
            // Store it for later reference in blockHeader2,4 and in variables.
            // Position is moved past header.
            parseFirstHeader(byteBuffer);
            // Move position back to beginning
            byteBuffer.position(0);

            // For the latest evio format, generate a table
            // of all event positions in buffer for random access.
            if (evioVersion > 3) {
// std::cout << "EvioReader const: evioVersion = " <, evioVersion << ", create mem handler" << std::endl;
                mappedMemoryHandler = new MappedMemoryHandler(byteBuffer);
                if (blockHeader4.hasDictionary()) {
                    ByteBuffer buf = mappedMemoryHandler.getFirstMap();
                    // Jump to the first event
                    prepareForBufferRead(buf);
                    // Dictionary is the first event
                    readDictionary(buf);
                }
            }
            else {
                // Setting the byte order is only necessary if someone hands
                // this method a buffer in which the byte order is improperly set.
                byteBuffer.order(byteOrder);
                prepareForBufferRead(byteBuffer);
            }

            parser = new EventParser();
    }

    /**
     * This method can be used to avoid creating additional EvioReader
     * objects by reusing this one with another buffer. The method
     * {@link #close()} is called before anything else.
     *
     * @param buf ByteBuffer to be read
     * @throws IOException   if read failure
     * @throws EvioException if buf is null;
     *                       if first block number != 1 when checkBlkNumSeq arg is true
     */
    void EvioReaderV4::setBuffer(ByteBuffer & buf) {

            if (buf == null) {
                throw EvioException("arg is null");
            }

            close();

            lastBlock           =  false;
            eventNumber         =  0;
            blockCount          = -1;
            eventCount          = -1;
            blockNumberExpected =  1;
            dictionaryXML       =  null;
            initialPosition     =  buf.position();
            byteBuffer          =  buf.slice();
            sequentialRead      = false;

            parseFirstHeader(byteBuffer);
            byteBuffer.position(0);

            if (evioVersion > 3) {
                mappedMemoryHandler = new MappedMemoryHandler(byteBuffer);
                if (blockHeader4.hasDictionary()) {
                    ByteBuffer bb = mappedMemoryHandler.getFirstMap();
                    // Jump to the first event
                    prepareForBufferRead(bb);
                    // Dictionary is the first event
                    readDictionary(bb);
                }
            }
            else {
                byteBuffer.order(byteOrder);
                prepareForBufferRead(byteBuffer);
            }

            closed = false;
    }

    /**
     * Has {@link #close()} been called (without reopening by calling
     * {@link #setBuffer(ByteBuffer)})?
     *
     * @return {@code true} if this object closed, else {@code false}.
     */
    bool EvioReaderV4::isClosed() const {return closed;}

    /**
     * Is this reader checking the block number sequence and
     * throwing an exception is it's not sequential and starting with 1?
     * @return <code>true</code> if checking block number sequence, else <code>false</code>
     */
    bool EvioReaderV4::checkBlockNumberSequence() const {return checkBlockNumSeq;}

    /**
     * Get the byte order of the file/buffer being read.
     * @return byte order of the file/buffer being read.
     */
    ByteOrder EvioReaderV4::getByteOrder() const {return byteOrder;}

    /**
     * Get the evio version number.
     * @return evio version number.
     */
    uint32_t EvioReaderV4::getEvioVersion() const {return evioVersion;}

    /**
      * Get the path to the file.
      * @return path to the file
      */
    string EvioReaderV4::getPath() const {return path;}

    /**
     * Get the file/buffer parser.
     * @return file/buffer parser.
     */
    std::shared_ptr<EventParser> EvioReaderV4::getParser() const {return parser;}

    /**
     * Set the file/buffer parser.
     * @param evParser file/buffer parser.
     */
    void EvioReaderV4::setParser(std::shared_ptr<EventParser> & evParser) {parser = evParser;}

    /**
    * Get the XML format dictionary if there is one.
    * @return XML format dictionary if there is one, else empty string.
    */
    string EvioReaderV4::getDictionaryXML() const {return dictionaryXML;}

    /**
     * Does this evio file have an associated XML dictionary?
     * @return <code>true</code> if this evio file has an associated XML dictionary,
     *         else <code>false</code>
     */
    bool EvioReaderV4::hasDictionaryXML() const {return !dictionaryXML.empty();}

    /**
     * Get the number of events remaining in the file.
     * @return number of events remaining in the file
     * @throws IOException if failed file access
     * @throws EvioException if failed reading from coda v3 file
     */
    size_t EvioReaderV4::getNumEventsRemaining() const {return getEventCount() - eventNumber;}

    /**
     * Get the byte buffer being read directly or corresponding to the event file.
     * Not a very useful method. For files, it works only for evio format versions 2,3 and
     * returns the internal buffer containing an evio block if using sequential access
     * (for example files &gt; 2.1 GB). It returns the memory mapped buffer otherwise.
     * For reading buffers it returns the buffer being read.
     * @return the byte buffer being read (in certain cases).
     */
    ByteBuffer & EvioReaderV4::getByteBuffer() {return byteBuffer;}

    /**
     * Get the size of the file being read, in bytes.
     * For small files, obtain the file size using the memory mapped buffer's capacity.
     * @return the file size in bytes
     */
    size_t EvioReaderV4::fileSize() const {return fileBytes;}


    /**
     * This returns the FIRST block (physical record) header.
     * @return the first block header.
     */
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
    void EvioReaderV4::parseFirstHeader(ByteBuffer & headerBuf) {

            // Check buffer length
            headerBuf.position(0);
            if (headerBuf.remaining() < 32) {
                throw EvioException("buffer too small");
            }

            // Get the file's version # and byte order
            byteOrder = headerBuf.order();

            int magicNumber = headerBuf.getInt(MAGIC_OFFSET);

            if (magicNumber != IBlockHeader::MAGIC_NUMBER) {
                swap = true;

                if (byteOrder == ByteOrder::ENDIAN_BIG) {
                    byteOrder = ByteOrder::ENDIAN_LITTLE;
                }
                else {
                    byteOrder = ByteOrder::ENDIAN_BIG;
                }
                headerBuf.order(byteOrder);

                // Reread magic number to make sure things are OK
                magicNumber = headerBuf.getInt(MAGIC_OFFSET);
                if (magicNumber != IBlockHeader::MAGIC_NUMBER) {
                    std::cout << "ERROR reread magic # (" << std::to_string(magicNumber) <<
                                  ") & still not right" << std::endl;
                    throw EvioException("bad magic #");
                }
            }

            // Check the version number. This requires peeking ahead 5 ints or 20 bytes.
            evioVersion = headerBuf.getInt(VERSION_OFFSET) & VERSION_MASK;
            if (evioVersion < 1)  {
                throw EvioException("bad version");
            }
//            std::cout << "Evio version# = " << evioVersion << std::endl;

            if (evioVersion >= 4) {
                blockHeader4.setBufferStartingPosition(0);

//                int pos = 0;
//                std::cout << "BlockHeader v4:" << std::endl << hex << showbase;
//                std::cout << "   block length  = " << headerBuf.getInt(pos) << std::endl; pos+=4;
//                std::cout << "   block number  = " << headerBuf.getInt(pos) << std::endl; pos+=4;
//                std::cout << "   header length = " << headerBuf.getInt(pos) << std::endl; pos+=4;
//                std::cout << "   event count   = " << headerBuf.getInt(pos) << std::endl; pos+=8;
//                std::cout << "   version       = " << headerBuf.getInt(pos) << std::endl; pos+=8;
//                std::cout << "   magic number  = " << headerBuf.getInt(pos) << std::endl; pos+=4;
//                std::cout << std::endl << dec;

                // Read the header data
                blockHeader4.setSize(        headerBuf.getInt());
                blockHeader4.setNumber(      headerBuf.getInt());
                blockHeader4.setHeaderLength(headerBuf.getInt());
                blockHeader4.setEventCount(  headerBuf.getInt());
                blockHeader4.setReserved1(   headerBuf.getInt());

                // Use 6th word to set bit info & version
                blockHeader4.parseToBitInfo(headerBuf.getInt());
                blockHeader4.setVersion(evioVersion);
                lastBlock = blockHeader4.getBitInfo(1);
                blockHeader4.setReserved2(headerBuf.getInt());
                blockHeader4.setMagicNumber(headerBuf.getInt());
                blockHeader4.setByteOrder(byteOrder);
                blockHeader = blockHeader4;
                firstBlockHeader = new BlockHeaderV4(blockHeader4);

                // Deal with non-standard header lengths here
                int headerLenDiff = blockHeader4.getHeaderLength() - BlockHeaderV4::HEADER_SIZE;
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
                blockHeader2.setBufferStartingPosition(0);

                // read the header data.
                blockHeader2.setSize(        headerBuf.getInt());
                blockHeader2.setNumber(      headerBuf.getInt());
                blockHeader2.setHeaderLength(headerBuf.getInt());
                blockHeader2.setStart(       headerBuf.getInt());
                blockHeader2.setEnd(         headerBuf.getInt());
                // skip version
                headerBuf.getInt();
                blockHeader2.setVersion(evioVersion);
                blockHeader2.setReserved1(   headerBuf.getInt());
                blockHeader2.setMagicNumber( headerBuf.getInt());
                blockHeader2.setByteOrder(byteOrder);
                blockHeader = blockHeader2;

                firstBlockHeader = new BlockHeaderV2(blockHeader2);
            }

            // Store this for later regurgitation of blockCount
            firstBlockSize = 4*blockHeader.getSize();

            // check block number if so configured
            if (checkBlockNumSeq) {
                if (blockHeader.getNumber() != blockNumberExpected) {
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
     * @throws IOException if file access problems
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
                size_t bytesLeftInFile = fileBytes - fileChannel.position();
                bytesToRead = DEFAULT_READ_BYTES < bytesLeftInFile ?
                              DEFAULT_READ_BYTES : bytesLeftInFile;
            }

            if (byteBuffer.capacity() < bytesToRead) {
                ByteBuffer bb(bytesToRead);
                byteBuffer = std::move(bb);
                byteBuffer.order(byteOrder);
            }
            byteBuffer.clear().limit(bytesToRead);

            // Read the first chunk of data from file
            fileChannel.read(byteBuffer);
            // Get it ready for reading from internal buffer
            byteBuffer.flip();

            // Position buffer properly (past block header)
            prepareForBufferRead(byteBuffer);
    }


    /**
     * Sets the proper buffer position for first-time read AFTER the first header.
     * @param buffer buffer to prepare
     */
    void EvioReaderV4::prepareForBufferRead(ByteBuffer & buffer) const {
        // Position after header
        size_t pos = 32;
        buffer.position(pos);

        // Deal with non-standard first header length.
        // No non-standard header lengths in evio version 2 & 3 files.
        if (evioVersion < 4) return;

        int headerLenDiff = blockHeader4.getHeaderLength() - BlockHeaderV4::HEADER_SIZE;
        // Hop over any extra header words
        if (headerLenDiff > 0) {
            for (int i=0; i < headerLenDiff; i++) {
                //std::cout << "Skip extra header int");
                pos += 4;
                buffer.position(pos);
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
     * @throws IOException if file access problems, evio format problems
     */
    EvioReader::ReadStatus EvioReaderV4::processNextBlock() {

        // We already read the last block header
        if (lastBlock) {
            return EvioReader::ReadStatus::END_OF_FILE;
        }

        try {

            if (sequentialRead) {
                if (evioVersion < 4) {
                    int bytesInBuf = bufferBytesRemaining();
                    if (bytesInBuf == 0) {

                        // How much of the file is left to read?
                        long bytesLeftInFile = fileBytes - fileChannel.position();
                        if (bytesLeftInFile < 32L) {
                            return EvioReader::ReadStatus::END_OF_FILE;
                        }

                        // The block size is 32kB which is on the small side.
                        // We want to read in 16MB (DEFAULT_READ_BYTES) or so
                        // at once for efficiency.
                        int bytesToRead = DEFAULT_READ_BYTES < bytesLeftInFile ?
                                          DEFAULT_READ_BYTES : (int) bytesLeftInFile;

                        // Reset buffer
                        byteBuffer.position(0).limit(bytesToRead);

                        // Read the entire chunk of data
                        int bytesActuallyRead = fileChannel.read(byteBuffer);
                        while (bytesActuallyRead < bytesToRead) {
                            bytesActuallyRead += fileChannel.read(byteBuffer);
                        }

                        byteBuffer.flip();
                        // Now keeping track of pos in this new blockBuffer
                        blockHeader->setBufferStartingPosition(0);
                    }
                    else if (bytesInBuf % 32768 == 0) {
                        // Next block header starts at this position in buffer
                        blockHeader->setBufferStartingPosition(byteBuffer.position());
                    }
                    else {
                        throw IOException("file contains non-integral # of 32768 byte blocks");
                    }
                }
                else {
                    // Enough data left to read len?
                    if (fileBytes - fileChannel.position() < 4L) {
                        return EvioReader::ReadStatus::END_OF_FILE;
                    }

                    // Read len of block in 32 bit words
                    uint32_t blkSize = dataStream.readInt();
                    if (swap) blkSize = SWAP_32(blkSize);
                    // Change to bytes
                    uint32_t blkBytes = 4 * blkSize;

                    // Enough data left to read rest of block?
                    if (fileBytes - fileChannel.position() < blkBytes-4) {
                        return EvioReader::ReadStatus::END_OF_FILE;
                    }

                    // Create a buffer to hold the entire first block of data
                    if (byteBuffer.capacity() >= blkBytes) {
                        byteBuffer.clear();
                        byteBuffer.limit(blkBytes);
                    }
                    else {
                        // Make this bigger than necessary so we're not constantly reallocating
                        ByteBuffer bb(blkBytes + 10000);
                        byteBuffer = std::move(bb);
                        byteBuffer.limit(blkBytes);
                        byteBuffer.order(byteOrder);
                    }

                    // Read the entire block of data.
                    // First put in length we just read.
                    byteBuffer.putInt(blkSize);

                    // Now the rest of the block (already put int, 4 bytes, in)
                    int bytesActuallyRead = fileChannel.read(byteBuffer) + 4;
                    while (bytesActuallyRead < blkBytes) {
                        bytesActuallyRead += fileChannel.read(byteBuffer);
                    }

                    byteBuffer.flip();
                    // Now keeping track of pos in this new blockBuffer
                    blockHeader->setBufferStartingPosition(0);
                }
            }
            else {
                if (byteBuffer.remaining() < 32) {
                    byteBuffer.clear();
                    return EvioReader::ReadStatus::END_OF_FILE;
                }
                // Record starting position
                blockHeader->setBufferStartingPosition(byteBuffer.position());
            }

            if (evioVersion >= 4) {
                // Read the header data.
                blockHeader4.setSize(byteBuffer.getInt());
                blockHeader4.setNumber(byteBuffer.getInt());
                blockHeader4.setHeaderLength(byteBuffer.getInt());
                blockHeader4.setEventCount(byteBuffer.getInt());
                blockHeader4.setReserved1(byteBuffer.getInt());
                // Use 6th word to set bit info
                blockHeader4.parseToBitInfo(byteBuffer.getInt());
                blockHeader4.setVersion(evioVersion);
                lastBlock = blockHeader4.getBitInfo(1);
                blockHeader4.setReserved2(byteBuffer.getInt());
                blockHeader4.setMagicNumber(byteBuffer.getInt());
                blockHeader = blockHeader4;

                // Deal with non-standard header lengths here
                int headerLenDiff = blockHeader4.getHeaderLength() - BlockHeaderV4::HEADER_SIZE;
                // If too small quit with error since headers have a minimum size
                if (headerLenDiff < 0) {
                    return EvioReader::ReadStatus::EVIO_EXCEPTION;
                }
                    // If bigger, read extra ints
                else if (headerLenDiff > 0) {
                    for (int i=0; i < headerLenDiff; i++) {
                        byteBuffer.getInt();
                    }
                }
            }
            else if (evioVersion < 4) {
                // read the header data
                blockHeader2.setSize(byteBuffer.getInt());
                blockHeader2.setNumber(byteBuffer.getInt());
                blockHeader2.setHeaderLength(byteBuffer.getInt());
                blockHeader2.setStart(byteBuffer.getInt());
                blockHeader2.setEnd(byteBuffer.getInt());
                // skip version
                byteBuffer.getInt();
                blockHeader2.setVersion(evioVersion);
                blockHeader2.setReserved1(byteBuffer.getInt());
                blockHeader2.setMagicNumber(byteBuffer.getInt());
                blockHeader = blockHeader2;
            }
            else {
                // bad version # - should never happen
                return EvioReader::ReadStatus::EVIO_EXCEPTION;
            }

            // check block number if so configured
            if (checkBlockNumSeq) {
                if (blockHeader.getNumber() != blockNumberExpected) {

                    std::cout << "block # out of sequence, got " + blockHeader.getNumber() +
                                 " expecting " + blockNumberExpected);

                    return EvioReader::ReadStatus::EVIO_EXCEPTION;
                }
                blockNumberExpected++;
            }
        }
        catch (EvioException & e) {
            return EvioReader::ReadStatus::EVIO_EXCEPTION;
        }
        catch (BufferUnderflowException & a) {
            std::cout << "ERROR endOfBuffer " << a << std::endl;
            byteBuffer.clear();
            return EvioReader::ReadStatus::UNKNOWN_ERROR;
        }

        return EvioReader::ReadStatus::SUCCESS;
    }


    /**
     * This method is only called once at the very beginning if buffer is known to have
     * a dictionary. It then reads that dictionary. Only called in format versions 4 & up.
     * Position buffer after dictionary. Called from synchronized method or constructor.
     *
     * @since 4.0
     * @param buffer buffer to read to get dictionary
     * @throws EvioException if failed read due to bad buffer format;
     *                       if version 3 or earlier
     */
    void EvioReaderV4::readDictionary(ByteBuffer & buffer) {

            if (evioVersion < 4) {
                throw EvioException("Unsupported version (" + std::to_string(evioVersion) + ")");
            }

            // How many bytes remain in this buffer?
            size_t bytesRemaining = buffer.remaining();
            if (bytesRemaining < 12) {
                throw EvioException("Not enough data in buffer");
            }

            // Once here, we are assured the entire next event is in this buffer.
            uint32_t length;
            length = buffer.getUInt();

            if (length < 1) {
                throw EvioException("Bad value for dictionary length");
            }
            bytesRemaining -= 4;

            // Since we're only interested in length, read but ignore rest of the header.
            buffer.getInt();
            bytesRemaining -= 4;

            // get the raw data
            uint32_t eventDataSizeBytes = 4*(length - 1);
            if (bytesRemaining < eventDataSizeBytes) {
                throw EvioException("Not enough data in buffer");
            }

            uint8_t bytes[eventDataSizeBytes];

            // Read in dictionary data
            buffer.getBytes(bytes, eventDataSizeBytes);

            std::vector<string> strs;

            // This is the very first event and must be a dictionary
            BaseStructure::unpackRawBytesToStrings(bytes, eventDataSizeBytes, strs);
            if (strs.empty()) {
                throw EvioException("Data in bad format");
            }
            dictionaryXML = strs[0];
    }

    /**
     * Get the event in the file/buffer at a given index (starting at 1).
     * As useful as this sounds, most applications will probably call
     * {@link #parseNextEvent()} or {@link #parseEvent(int)} instead,
     * since it combines combines getting an event with parsing it.<p>
     *
     * @param  index the event number in a 1,2,..N counting sense, from beginning of file/buffer.
     * @return the event in the file/buffer at the given index or null if none
     * @throws IOException   if failed file access
     * @throws EvioException if failed read due to bad file/buffer format;
     *                       if out of memory;
     *                       if index &lt; 1;
     *                       if object closed
     */
    std::shared_ptr<EvioEvent> EvioReaderV4::getEvent(size_t index) {
        if (index < 1) {
            throw EvioException("index arg starts at 1");
        }

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
     * @throws IOException   if failed file access
     * @throws EvioException if failed read due to bad file/buffer format;
     *                       if out of memory;
     *                       if object closed
     */
    /*synchronized*/ std::shared_ptr<EvioEvent> EvioReaderV4::getEventV4(size_t index) {
        if (index > mappedMemoryHandler.getEventCount()) {
            return nullptr;
        }

        if (closed) {
            throw EvioException("object closed");
        }

        index--;

        std::shared_ptr<BankHeader> header;
        auto event = EvioEvent::getInstance(header);

        uint32_t length, eventDataSizeBytes = 0;

        ByteBuffer buf = mappedMemoryHandler.getByteBuffer(index);
        length = buf.getUInt();

        if (length < 1) {
            throw EvioException("Bad file/buffer format");
        }
        header->setLength(length);

        // Read and parse second header word
        uint32_t word = buf.getUInt();
        header->setTag(word >>> 16);
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
        header->setPadding(dt >>> 6);
        header->setNumber(word & 0xff);

        try {
            // Read the raw data
            eventDataSizeBytes = 4*(length - 1);
            uint8_t bytes[eventDataSizeBytes];
            buf.get(bytes, eventDataSizeBytes);

            event->setRawBytes(bytes);
            event->setByteOrder(byteOrder);
            event->setEventNumber(++eventNumber);
        }
        catch (OutOfMemoryError e) {
            throw new EvioException("Out Of Memory: (event size = " + eventDataSizeBytes + ")", e);
        }
        catch (Exception e) {
            throw EvioException("Error", e);
        }

        return event;
    }


    /**
     * This is a workhorse method. It retrieves the desired event from the file/buffer,
     * and then parses it SAX-like. It will drill down and uncover all structures
     * (banks, segments, and tagsegments) and notify any interested listeners.
     *
     * @param  index number of event desired, starting at 1, from beginning of file/buffer
     * @return the parsed event at the given index or null if none
     * @throws IOException if failed file access
     * @throws EvioException if failed read due to bad file/buffer format;
     *                       if out of memory;
     *                       if index &lt; 1;
     *                       if object closed
     */
    /*synchronized*/ std::shared_ptr<EvioEvent> EvioReaderV4::parseEvent(size_t index) {
        auto event = getEvent(index);
        if (event != nullptr) parseEvent(event);
        return event;
    }


    /**
     * Get the next event in the file/buffer. As useful as this sounds, most
     * applications will probably call {@link #parseNextEvent()} instead, since
     * it combines getting the next event with parsing the next event.<p>
     *
     * Although this method can get events in versions 4+, it now delegates that
     * to another method. No changes were made to this method from versions 1-3 in order
     * to read the version 4 format as it is subset of versions 1-3 with variable block
     * length.
     *
     * @return the next event in the file.
     *         On error it throws an EvioException.
     *         On end of file, it returns <code>null</code>.
     * @throws IOException   if failed file access
     * @throws EvioException if failed read due to bad buffer format;
     *                       if object closed
     */
    /*synchronized*/ std::shared_ptr<EvioEvent> EvioReaderV4::nextEvent() {

        if (!sequentialRead && evioVersion > 3) {
            return getEvent(eventNumber+1);
        }

        if (closed) {
            throw EvioException("object closed");
        }

        std::shared_ptr<BankHeader> header;
        auto event = EvioEvent::getInstance(header);
        size_t currentPosition = byteBuffer.position();

        // How many bytes remain in this block until we reach the next block header?
        int blockBytesRemaining = blockBytesRemaining();

        if (blockBytesRemaining < 0) {
            throw EvioException("Number of block bytes remaining is negative.");
        }

        // Are we at the block boundary? If so, read/skip-over next header.
        // Read in more blocks of data if necessary.
        //
        // version 1-3:
        // We now read in bigger chunks that are integral multiples of a single block
        // (32768 bytes). Must see if we have to deal with an event crossing physical
        // record boundaries. Previously, java evio only read 1 block at a time.
        if (blockBytesRemaining == 0) {
            EvioReader::ReadStatus status = processNextBlock();
            if (status == EvioReader::ReadStatus::SUCCESS) {
                return nextEvent();
            }
            else if (status == EvioReader::ReadStatus::END_OF_FILE) {
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
        uint32_t length = byteBuffer.getUInt();
        if (length < 1) {
            throw EvioException("non-positive length (0x" + Integer.toHexString(length) + ")");
        }
        header->setLength(length);
        blockBytesRemaining -= 4; // just read in 4 bytes

        // Versions 1-3: if we were unlucky, after reading the length
        //               there are no bytes remaining in this block.
        // Don't really need the "if (version < 4)" here except for clarity.
        if (evioVersion < 4) {
            if (bufferBytesRemaining() == 0) {
                EvioReader::ReadStatus status = processNextBlock();
                if (status == EvioReader::ReadStatus::END_OF_FILE) {
                    return nullptr;
                }
                else if (status != EvioReader::ReadStatus::SUCCESS) {
                    throw EvioException("Failed reading block header in nextEvent.");
                }
                blockBytesRemaining = blockBytesRemaining();
            }
        }

        // Now we should be good to go, except data may cross block boundary.
        // In any case, should be able to read the rest of the header.

        // Read and parse second header word
        int word = byteBuffer.getInt();
        header->setTag(word >>> 16);
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
        header->setPadding(dt >>> 6);
        header->setNumber(word & 0xff);

        blockBytesRemaining -= 4; // just read in 4 bytes

        // get the raw data
        int eventDataSizeBytes = 4*(length - 1);

        try {
            byte bytes[] = new byte[eventDataSizeBytes];

            int bytesToGo = eventDataSizeBytes;
            int offset = 0;

            // Don't really need the "if (version < 4)" here except for clarity.
            if (evioVersion < 4) {

                // Be in while loop if have to cross block boundary[ies].
                while (bytesToGo > 0) {

                    // Don't read more than what is left in current block
                    int bytesToReadNow = bytesToGo > blockBytesRemaining ?
                                         blockBytesRemaining : bytesToGo;

                    // Read in bytes remaining in internal buffer
                    byteBuffer.getBytes(bytes + offset, bytesToReadNow);
                    offset               += bytesToReadNow;
                    bytesToGo            -= bytesToReadNow;
                    blockBytesRemaining  -= bytesToReadNow;

                    if (blockBytesRemaining == 0) {
                        EvioReader::ReadStatus status = processNextBlock();
                        if (status == EvioReader::ReadStatus::END_OF_FILE) {
                            return nullptr;
                        }
                        else if (status != EvioReader::ReadStatus::SUCCESS) {
                            throw EvioException("Failed reading block header after crossing boundary in nextEvent.");
                        }

                        blockBytesRemaining  = blockBytesRemaining();
                    }
                }
            }

            // Last (perhaps only) read
            byteBuffer.getBytes(bytes + offset, bytesToGo);
            event->setRawBytes(bytes);
            event->setByteOrder(byteOrder); // add this to track endianness, timmer
            // Don't worry about dictionaries here as version must be 1-3
            event->setEventNumber(++eventNumber);
            return event;
        }
        catch (OutOfMemoryError ome) {
            std::cout << "Out Of Memory << std::endl" <<
                         "eventDataSizeBytes = " << eventDataSizeBytes << std::endl <<
                         "bytes Remaining = " << blockBytesRemaining << std::endl <<
                         "event Count: " << eventCount << std::endl;
            return nullptr;
        }
        catch (Exception e) {
            e.printStackTrace();
            return nullptr;
        }
    }


    /**
     * This is a workhorse method. It retrieves the next event from the file/buffer,
     * and then parses it SAX-like. It will drill down and uncover all structures
     * (banks, segments, and tagsegments) and notify any interested listeners.
     *
     * @return the event that was parsed.
     *         On error it throws an EvioException.
     *         On end of file, it returns <code>null</code>.
     * @throws IOException if failed file access
     * @throws EvioException if read failure or bad format
     *                       if object closed
     */
    /*synchronized*/ std::shared_ptr<EvioEvent> EvioReaderV4::parseNextEvent() {
        auto event = nextEvent();
        if (event != nullptr) {
            parseEvent(event);
        }
        return event;
    }

    /**
     * This will parse an event, SAX-like. It will drill down and uncover all structures
     * (banks, segments, and tagsegments) and notify any interested listeners.<p>
     *
     * As useful as this sounds, most applications will probably call {@link #parseNextEvent()}
     * instead, since it combines combines getting the next event with parsing the next event.<p>
     *
     * This method is only called by synchronized methods and therefore is not synchronized.
     *
     * @param evioEvent the event to parse.
     * @throws EvioException if bad format
     */
    void EvioReaderV4::parseEvent(std::shared_ptr<EvioEvent> evioEvent) {
        // This method is synchronized too
        parser->parseEvent(evioEvent);
    }

    /**
     * Get an evio bank or event in byte array form.
     * @param evNumber number of event of interest
     * @return array containing bank's/event's bytes.
     * @throws IOException if failed file access
     * @throws EvioException if eventNumber &lt; 1;
     *                       if the event number does not correspond to an existing event;
     *                       if object closed
     */
    std::vector<uint8_t> EvioReaderV4::getEventArray(size_t evNumber) {
        auto ev = gotoEventNumber(evNumber, false);
        if (ev == nullptr) {
            throw EvioException("event number must be > 0");
        }
        return ev.toArray();
    }

    /**
     * Get an evio bank or event in ByteBuffer form.
     * @param evNumber number of event of interest
     * @return buffer containing bank's/event's bytes.
     * @throws IOException if failed file access
     * @throws EvioException if eventNumber &lt; 1;
     *                       if the event number does not correspond to an existing event;
     *                       if object closed
     */
    ByteBuffer & EvioReaderV4::getEventBuffer(size_t evNumber) {
        return ByteBuffer.wrap(getEventArray(evNumber)).order(byteOrder);
    }

    /**
     * Get the number of bytes remaining in the internal byte buffer.
     * Called only by {@link #nextEvent()}.
     * @return the number of bytes remaining in the current block (physical record).
     */
    size_t EvioReaderV4::bufferBytesRemaining() const {return byteBuffer.remaining();}

    /**
     * Get the number of bytes remaining in the current block (physical record).
     * This is used for pathology checks like crossing the block boundary.
     * Called only by {@link #nextEvent()}.
     *
     * @return the number of bytes remaining in the current block (physical record).
     */
// TODO: RETURNS -1  !!!
    size_t EvioReaderV4::blockBytesRemaining() {
        try {
            return blockHeader->bytesRemaining(byteBuffer.position());
        }
        catch (EvioException & e) {
            std::cout << e.what() << std::endl;
            return -1;
        }
    }

    /**
     * The equivalent of rewinding the file. What it actually does
     * is set the position of the file/buffer back to where it was
     * after calling the constructor - after the first header.
     * This method, along with the two <code>position()</code> and the
     * <code>close()</code> method, allows applications to treat files
     * in a normal random access manner.
     *
     * @throws IOException   if failed file access or buffer/file read
     * @throws EvioException if object closed
     */
    /*synchronized*/ void EvioReaderV4::rewind() {
        if (closed) {
            throw EvioException("object closed");
        }

        if (sequentialRead) {
            fileChannel.position(initialPosition);
            prepareForSequentialRead();
        }
        else if (evioVersion < 4) {
            byteBuffer.position(initialPosition);
            prepareForBufferRead(byteBuffer);
        }

        lastBlock = false;
        eventNumber = 0;
        blockNumberExpected = 1;

        if (evioVersion < 4) {
            blockHeader = blockHeader2 = new BlockHeaderV2((BlockHeaderV2) firstBlockHeader);
        }
        else {
            blockHeader = blockHeader4 = new BlockHeaderV4((BlockHeaderV4) firstBlockHeader);
        }

        blockHeader->setBufferStartingPosition(initialPosition);

        if (sequentialRead && hasDictionaryXML()) {
            // Dictionary is always the first event so skip over it.
            // For sequential reads, do this after each rewind.
            nextEvent();
        }
    }

    /**
     * This is equivalent to obtaining the current position in the file.
     * What it actually does is return the position of the buffer. This
     * method, along with the <code>rewind()</code>, <code>position(int)</code>
     * and the <code>close()</code> method, allows applications to treat files
     * in a normal random access manner. Only meaningful to evio versions 1-3
     * and for sequential reading.<p>
     *
     * @return the position of the buffer; -1 if version 4+
     * @throws IOException   if error accessing file
     * @throws EvioException if object closed
     */
// TODO:: return -1 !!!!!!!!!!
    /*synchronized*/ size_t EvioReaderV4::position() const {
        if (!sequentialRead && evioVersion > 3) return -1L;

        if (closed) {
            throw EvioException("object closed");
        }

        if (sequentialRead) {
            return fileChannel.position();
        }
        return byteBuffer.position();
    }

//	/**
//	 * This method sets the current position in the file or buffer. This
//     * method, along with the <code>rewind()</code>, <code>position()</code>
//     * and the <code>close()</code> method, allows applications to treat files
//     * in a normal random access manner. Only meaningful to evio versions 1-3
//     * and for sequential reading.<p>
//     *
//     * <b>HOWEVER</b>, using this method is not necessary for random access of
//     * events and is no longer recommended because it interferes with the sequential
//     * reading of events. Therefore it is now deprecated.
//     *
//	 * @deprecated
//	 * @param position the new position of the buffer.
//     * @throws IOException   if error accessing file
//     * @throws EvioException if object closed
//     */
//	@Override
//    public synchronized void position(long position) throws IOException, EvioException  {
//        if (!sequentialRead && evioVersion > 3) return;
//
//        if (closed) {
//            throw new EvioException("object closed");
//        }
//
//        if (sequentialRead) {
//            fileChannel.position(position);
//        }
//        else {
//            byteBuffer.position((int)position);
//        }
//	}

    /**
     * This is closes the file, but for buffers it only sets the position to 0.
     * This method, along with the <code>rewind()</code> and the two
     * <code>position()</code> methods, allows applications to treat files
     * in a normal random access manner.
     *
     * @throws IOException if error accessing file
     */
    /*synchronized*/ void EvioReaderV4::close() {
        if (closed) {
            return;
        }

        if (!sequentialRead && evioVersion > 3) {
            if (byteBuffer != null) byteBuffer.position(initialPosition);
            mappedMemoryHandler = null;

            if (fileChannel != null) {
                fileChannel.close();
                fileChannel = null;
            }

            closed = true;
            return;
        }

        if (sequentialRead) {
            fileChannel.close();
            dataStream.close();
        }
        else {
            byteBuffer.position(initialPosition);
        }

        closed = true;
    }

    /**
     * This returns the current (active) block (physical record) header.
     * Since most users have no interest in physical records, this method
     * should not be used. Mostly it is used by the test programs in the
     * <code>EvioReaderTest</code> class.
     *
     * @return the current block header.
     */
    std::shared_ptr<IBlockHeader> EvioReaderV4::getCurrentBlockHeader() {return blockHeader;}

    /**
     * Go to a specific event in the file. The events are numbered 1..N.
     * This number is transient--it is not part of the event as stored in the evio file.
     * In versions 4 and up this is just a wrapper on {@link #getEvent(int)}.
     *
     * @param evNumber the event number in a 1..N counting sense, from the start of the file.
     * @return the specified event in file or null if there's an error or nothing at that event #.
     * @throws IOException if failed file access
     * @throws EvioException if object closed
     */
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
     * @throws IOException if failed file access
     * @throws EvioException if object closed
     */
    /*synchronized*/ std::shared_ptr<EvioEvent> EvioReaderV4::gotoEventNumber(size_t evNumber, bool parse) {
            if (evNumber < 1) {
                return nullptr;
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
            EvioEvent event;

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


    /**
     * This is the number of events in the file. Any dictionary event is <b>not</b>
     * included in the count. In versions 3 and earlier, it is not computed unless
     * asked for, and if asked for it is computed and cached.
     *
     * @return the number of events in the file.
     * @throws IOException   if failed file access
     * @throws EvioException if read failure;
     *                       if object closed
     */
    /*synchronized*/ size_t EvioReaderV4::getEventCount() const {

            if (closed) {
                throw EvioException("object closed");
            }

            if (!sequentialRead && evioVersion > 3) {
                return mappedMemoryHandler.getEventCount();
            }

            if (eventCount < 0) {
                // The difficulty is that this method can be called at
                // any time. So we need to save our state and then restore
                // it when we're done.
                ReaderState state = getState();

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
                    for (int i=1; i < state.eventNumber; i++) {
                        nextEvent();
                    }
                }

                // Restore our original settings
                restoreState(state);
            }

            return eventCount;
    }

    /**
     * This is the number of blocks in the file including the empty
     * block usually at the end of version 4 files/buffers.
     * For version 3 files, a block size read from the first block is used
     * to calculate the result.
     * It is not computed unless in random access mode or is
     * asked for, and if asked for it is computed and cached.
     *
     * @throws EvioException if object closed
     * @return the number of blocks in the file (estimate for version 3 files)
     */
    /*synchronized*/ size_t EvioReaderV4::getBlockCount() const {

            if (closed) {
                throw EvioException("object closed");
            }

            if (!sequentialRead && evioVersion > 3) {
                return mappedMemoryHandler.getBlockCount();
            }

            if (blockCount < 0) {
                // Although block size is theoretically adjustable, I believe
                // that everyone used 8192 words for the block size in version 3.
                blockCount = (int) (fileBytes/firstBlockSize);
            }

            return blockCount;
    }


}