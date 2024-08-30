//
// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "EventWriterV4.h"


namespace evio {


    //---------------------------------------------
    // FILE Constructors
    //---------------------------------------------


    /**
     * Creates an <code>EventWriterV4</code> for writing to a file in the
     * specified byte order.
     * If the file already exists, its contents will be overwritten unless
     * it is being appended to. If it doesn't exist, it will be created.
     *
     * @param filename  name of the file to write to.<br>
     * @param append    if <code>true</code> and the file already exists,
     *                  all events to be written will be appended to the
     *                  end of the file.
     * @param byteOrder the byte order in which to write the file.
     *
     * @throws EvioException file cannot be created, or
     *                       if appending and file is too small to be evio format;
     */
    EventWriterV4::EventWriterV4(std::string & filename, const ByteOrder & byteOrder, bool append) :
            EventWriterV4(filename, "", "", 1, 0,
            DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT,
            byteOrder, "", true, append) {}


    /**
     * Creates an <code>EventWriterV4</code> for writing to a file in the
     * specified byte order.
     * If the file already exists, its contents will be overwritten unless
     * it is being appended to. If it doesn't exist, it will be created.
     *
     * @param filename      the file object to write to.<br>
     * @param xmlDictionary dictionary in xml format or empty if none.
     * @param byteOrder     the byte order in which to write the file.
     * @param append        if <code>true</code> and the file already exists,
     *                      all events to be written will be appended to the
     *                      end of the file.
     *
     * @throws EvioException file cannot be created, or
     *                       if appending and file is too small to be evio format;
     */
    EventWriterV4::EventWriterV4(std::string & filename,
                                const std::string & xmlDictionary,
                                const ByteOrder & byteOrder,
                                bool append) :
            EventWriterV4(filename, "", "", 1, 0,
                          DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT,
                          byteOrder, xmlDictionary, true, append) {}


    /**
     * Create an <code>EventWriter</code> for writing events to a file.
     * If the file already exists, its contents will be overwritten
     * unless the "overWriteOK" argument is <code>false</code> in
     * which case an exception will be thrown. Unless ..., the option to
     * append these events to an existing file is <code>true</code>,
     * in which case everything is fine. If the file doesn't exist,
     * it will be created. Byte order defaults to big endian if arg is null.
     * File can be split while writing.<p>
     *
     * The base file name may contain up to 2, C-style integer format specifiers using
     * "d" and "x" (such as <b>%03d</b>, or <b>%x</b>).
     * If more than 2 are found, an exception will be thrown.
     * If no "0" precedes any integer between the "%" and the "d" or "x" of the format specifier,
     * it will be added automatically in order to avoid spaces in the file name.
     * The first specifier will be substituted with the given runNumber value.
     * If the file is being split, the second will be substituted with the split number
     * which starts at 0.
     * If 2 specifiers exist and the file is not being split, no substitutions are made.
     * If no specifier for the splitNumber exists, it is tacked onto the end of the file
     * name after a dot (.).
     * If streamCount &gt; 1, the split number is calculated starting with streamId and incremented
     * by streamCount each time. In this manner, all split files will have unique, sequential
     * names even though there are multiple parallel ERs.
     * <p>
     *
     * The base file name may contain characters of the form <b>$(ENV_VAR)</b>
     * which will be substituted with the value of the associated environmental
     * variable or a blank string if none is found.<p>
     *
     * The base file name may also contain occurrences of the string "%s"
     * which will be substituted with the value of the runType arg or nothing if
     * the runType is null.<p>
     *
     * If multiple streams of data, each writing a file, end up with the same file name,
     * they can be differentiated by a stream id, starting split # and split increment.
     *
     * @param baseName       base file name used to generate complete file name (may not be empty)
     * @param directory      directory in which file is to be placed
     * @param runType        name of run type configuration to be used in naming files
     * @param runNumber      number of the CODA run, used in naming files
     * @param split          if &lt; 1, do not split file, write to only one file of unlimited size.
     *                       Else this is max size in bytes to make a file
     *                       before closing it and starting writing another.
     * @param maxBlockSize   the max blocksize in bytes to use which must be &gt;= {@link #MIN_BLOCK_SIZE}
     *                       and &lt;= {@link #MAX_BLOCK_SIZE}.
     *                       The size of the block will not be larger than this size
     *                       unless a single event itself is larger.
     * @param maxEventCount  the max number of events (including dictionary) in a single block
     *                       which must be &gt;= {@link #MIN_BLOCK_COUNT} and &lt;= {@link #MAX_BLOCK_COUNT}.
     * @param byteOrder      the byte order in which to write the file. This is ignored
     *                       if appending to existing file.
     * @param xmlDictionary  dictionary in xml format or empty if none.
     * @param overWriteOK    if <code>false</code> and the file already exists,
     *                       an exception is thrown rather than overwriting it.
     * @param append         if <code>true</code> append written data to given file.
     * @param firstEvent     the first event written into each file (after any dictionary)
     *                       including all split files; may be null. Useful for adding
     *                       common, static info into each split file.
     * @param streamId       streamId number (100 &gt; id &gt; -1) for file name
     * @param splitNumber    number at which to start the split numbers
     * @param splitIncrement amount to increment split number each time another file is created.
     * @param streamCount    total number of streams in DAQ.
     * @param bufferSize     number of bytes to make the internal buffer which will
     *                       be storing events before writing them to a file. Must be at least
     *                       a max block size + 1kB (add a little extra). If not, it is set to that.
     *
     * @throws EvioException if defined dictionary or first event while appending;
     *                       if splitting file while appending;
     *                       if appending and file is too small to be evio format;
     *                       if file name arg is empty;
     *                       if file could not be opened, positioned, or written to;
     *                       if file exists but user requested no over-writing or appending.
    */
    EventWriterV4::EventWriterV4(std::string & baseName, const std::string & directory, const std::string & runType,
                             uint32_t runNumber, uint64_t split,
                             uint32_t maxBlockSize, uint32_t maxEventCount,
                             const ByteOrder & byteOrder, const std::string & xmlDictionary,
                             bool overWriteOK, bool append,
                             std::shared_ptr<EvioBank> firstEvent, uint32_t streamId,
                             uint32_t splitNumber, uint32_t splitIncrement, uint32_t streamCount,
                             uint32_t bufferSize, std::bitset<24> *bitInfo) {

        if (baseName.empty()) {
            throw EvioException("baseName arg is null");
        }

        if (maxBlockSize < MIN_BLOCK_SIZE) {
            maxBlockSize = MIN_BLOCK_SIZE;
        }

        if (maxBlockSize > MAX_BLOCK_SIZE) {
            maxBlockSize = MAX_BLOCK_SIZE;
        }

        if (maxEventCount < MIN_BLOCK_COUNT) {
            maxEventCount = MIN_BLOCK_COUNT;
        }

        if (maxEventCount > MAX_BLOCK_COUNT) {
            maxEventCount = MAX_BLOCK_COUNT;
        }

        if (splitIncrement < 1) {
            splitIncrement = 1;
        }

        if (append) {
            if (split > 0) {
                throw EvioException("Cannot specify split when appending");
            }
            else if ((!xmlDictionary.empty()) || (firstEvent != nullptr && firstEvent->getHeader()->getLength() > 0)) {
                throw EvioException("Cannot specify dictionary or first event when appending");
            }
        }

        if (!xmlDictionary.empty()) {
            // 56 is the minimum number of characters for a valid xml dictionary
            if (xmlDictionary.length() < 56) {
                throw EvioException("Dictionary improper format");
            }

            // Turn dictionary data into bytes
            std::vector<std::string> vec = {xmlDictionary};
            BaseStructure::stringsToRawBytes(vec, dictionaryByteArray);

            // Dictionary length in bytes including bank header of 8 bytes
            dictionaryBytes = dictionaryByteArray.size() + 8;

            // Common block has dictionary
            commonBlockByteSize = dictionaryBytes;
            commonBlockCount = 1;
        }

        // Store arguments
        this->split          = split;
        this->append         = append;
        this->runNumber      = runNumber;
        this->byteOrder      = byteOrder;   // byte order may be overwritten if appending
        this->overWriteOK    = overWriteOK;
        this->maxBlockSize   = maxBlockSize;
        this->maxEventCount  = maxEventCount;
        this->xmlDictionary  = xmlDictionary;
        this->streamId       = streamId;
        this->splitNumber    = splitNumber;
        this->splitIncrement = splitIncrement;
        this->streamCount    = streamCount;

        toFile = true;
        blockNumber = 1;

        if (bitInfo != nullptr) {
            this->bitInfo = *bitInfo;
        }

        // The following may not be backwards compatible.
        // Make substitutions in the baseName to create the base file name.
        if (!directory.empty()) baseName = directory + "/" + baseName;
        specifierCount = Util::generateBaseFileName(baseName, runType, baseFileName);
        // Also create the first file's name with more substitutions
        std::string fileName = Util::generateFileName(baseFileName, specifierCount,
                                                      runNumber, split, splitNumber,
                                                      streamId, streamCount);
        // All subsequent split numbers are calculated by adding the splitIncrement
        this->splitNumber += splitIncrement;


#ifdef __cpp_lib_filesystem
        currentFilePath = fs::path(fileName);
        currentFileName = currentFilePath.generic_string();
        bool fileExists = fs::exists(currentFilePath);
        bool isRegularFile = fs::is_regular_file(currentFilePath);
#else
        currentFileName = fileName;
        struct stat stt{};
        bool fileExists = stat( fileName.c_str(), &stt ) == 0;
        bool isRegularFile = S_ISREG(stt.st_mode);
#endif

        // If we can't overwrite or append and file exists, throw exception
        if (!overWriteOK && !append && (fileExists && isRegularFile)) {
            throw EvioException("File exists but user requested no over-writing of or appending to "
                                + fileName);
        }

#ifdef __cpp_lib_filesystem
        fs::space_info spaceInfo = fs::space(currentFilePath.parent_path());
        uint64_t freeBytes = spaceInfo.available;
        //std::cout << "EventWriter constr: " << freeBytes << " bytes available in dir = " <<
        //         currentFilePath.parent_path().generic_string() << std::endl;
#else
        uint64_t freeBytes;
        struct statvfs sttvfs{};
        if (statvfs(fileName.c_str(), &sttvfs) == 0) {
            freeBytes = sttvfs.f_bavail * sttvfs.f_frsize;
        }
        else {
            // Don't know how much space we have so pretend like we have an extra 40GB.
            freeBytes = split + 40000000000;
            // std::cout << "error getting disk partition's available space for " << fileName << std::endl;
        }
#endif
        // If there isn't enough to accommodate 1 split of the file + full supply + 10MB extra,
        // then don't even start writing ...
        if (freeBytes < split + 10000000) {
            //cout << "EventWriter constr: Disk is FULL" << endl;
            diskIsFull = true;
            diskIsFullVolatile = true;
        }

        // Create internal storage buffers.
        // The reason there are 2 internal buffers is that we'll be able to
        // do 1 asynchronous write while still filling up the second
        // simultaneously.

        // Allow the user to set the size of the internal buffers up to a point.
        // This size is set to at least 1 max block.
        if (bufferSize < maxBlockSize + 1024) {
            bufferSize = maxBlockSize + 1024;
        }

        // Don't use any size < 16MB.
        internalBufSize = bufferSize < 16000000 ? 16000000 : bufferSize;
        this->bufferSize = internalBufSize;

        internalBuffers.reserve(2);
        internalBuffers.push_back(std::make_shared<ByteBuffer>(internalBufSize));
        internalBuffers.push_back(std::make_shared<ByteBuffer>(internalBufSize));
        internalBuffers[0]->order(byteOrder);
        internalBuffers[1]->order(byteOrder);
        buffer = internalBuffers[0];

        // Aim for this size block (in bytes)
        targetBlockSize = maxBlockSize;

        // Object to close files in a separate thread when splitting, to speed things up
        if (split > 0) {
            fileCloser = std::make_shared<FileCloserV4>(4);
        }

        asyncFileChannel = std::make_shared<std::fstream>();

        if (append) {
            // For reading existing file and preparing for stream writes
            asyncFileChannel->open(currentFileName, std::ios::binary | std::ios::in | std::ios::out);
            if (asyncFileChannel->fail()) {
                throw EvioException("error opening file " + currentFileName);
            }

            // Right now file is open for writing
            fileOpen = true;

            // If we have an empty file, that's OK.
            // Otherwise we have to examine it for compatibility
            // and position ourselves for the first write.
#ifdef __cpp_lib_filesystem
            if (fs::file_size(currentFilePath) >= 32) {
#else
            if (stt.st_size >= 32) {
#endif
                // Look at first block header to find endianness & version.
                // Endianness given in constructor arg, when appending, is ignored.
                // this->byteOrder set in next call.
                examineFirstBlockHeader();

                // Oops, gotta redo this since file has different byte order
                // than specified in constructor arg.
                if (this->byteOrder != byteOrder) {
                    // From now on, this->byteOrder must be used, not local byteOrder!
                    internalBuffers[0]->order(this->byteOrder);
                    internalBuffers[1]->order(this->byteOrder);
                }

                // Prepare for appending by moving file position to end of last block
                toAppendPosition();

                // File position is now after the last event written.

                // Reset the buffer which has been used to read the header
                // and to prepare the file for event writing.
                buffer->clear();
            }
            else {
                throw EvioException("File too small to be evio format, cannot append");
            }
        }

        // Convert first event into bytes
        if (firstEvent != nullptr) {
            firstEventBytes = firstEvent->getTotalBytes();
            std::shared_ptr<ByteBuffer> firstEventBuf = std::make_shared<ByteBuffer>(firstEventBytes);
            firstEventBuf->order(byteOrder);
            firstEvent->write(firstEventBuf);
            // copy data into vector
            firstEventByteArray.resize(firstEventBytes);
            std::copy(firstEventBuf->array(), firstEventBuf->array() + firstEventBytes, firstEventByteArray.begin());
            commonBlockByteSize += firstEventBytes;
            commonBlockCount++;
            haveFirstEvent = true;
        }

        // Write out the beginning block header
        // (size & count words are updated when writing event)

        if (xmlDictionary.empty()) {
            writeNewHeader(0,blockNumber++,bitInfo,false,false,haveFirstEvent);
        }
        else {
            writeNewHeader(0,blockNumber++,bitInfo,true,false,haveFirstEvent);
        }

        // Write out dictionary & firstEvent if any (currentBlockSize updated)
        writeCommonBlock();
    }


    //---------------------------------------------
    // BUFFER Constructors
    //---------------------------------------------


    /**
     * Create an <code>EventWriterV4</code> for writing events to a ByteBuffer.
     * Uses the default number and size of blocks in buffer.
     *
     * @param buf            the buffer to write to.
     * @param xmlDictionary  dictionary in xml format or null if none.
     * @param append         if <code>true</code>, all events to be written will be
     *                       appended to the end of the buffer.
     * @throws EvioException if buf arg is null, or
     *                       if appending and buffer is too small to be evio format;
     */
    EventWriterV4::EventWriterV4(std::shared_ptr<ByteBuffer> buf, const std::string & xmlDictionary, bool append) :

            EventWriterV4(buf, DEFAULT_BLOCK_SIZE, DEFAULT_BLOCK_COUNT, xmlDictionary, nullptr, 0, append) {}



    /**
     * Create an <code>EventWriterV4</code> for writing events to a ByteBuffer.
     *
     * @param buf            the buffer to write to.
     * @param maxBlockSize   the max blocksize to use which must be &gt;= {@link #MIN_BLOCK_SIZE}
     *                       and &lt;= {@link #MAX_BLOCK_SIZE} ints.
     *                       The size of the block will not be larger than this size
     *                       unless a single event itself is larger.
     * @param maxEventCount the max number of events (including dictionary) in a single block
     *                      which must be &gt;= {@link #MIN_BLOCK_COUNT} and &lt;= {@link #MAX_BLOCK_COUNT}.
     * @param xmlDictionary  dictionary in xml format or null if none.
     * @param bitInfo        set of bits to include in first block header.
     * @param reserved1      set the value of the first "reserved" int in first block header.
     *                       NOTE: only CODA (i.e. EMU) software should use this.
     * @param blockNumber    number at which to start block number counting.
     * @param append         if <code>true</code>, all events to be written will be
     *                       appended to the end of the buffer.
     * @param firstEvent     the first event written into the buffer (after any dictionary).
     *                       May be null. Not useful when writing to a buffer as this
     *                       event may be written using normal means.
     *
     * @throws EvioException if maxBlockSize or maxEventCount exceed limits;
     *                       if appending and buffer is too small to be evio format;
     *                       if buf arg is null;
     *                       if defined dictionary while appending;
     */
    EventWriterV4::EventWriterV4(std::shared_ptr<ByteBuffer> buf, int maxBlockSize, int maxEventCount,
                           const std::string & xmlDictionary, std::bitset<24> *bitInfo, int reserved1,
                           int blockNumber, bool append, std::shared_ptr<EvioBank> firstEvent) :
                                byteOrder(ByteOrder::ENDIAN_BIG)
    {
            initializeBuffer(buf, maxBlockSize, maxEventCount,
                             xmlDictionary, bitInfo, reserved1,
                             blockNumber, append, firstEvent);
    }

    /**
     * Encapsulate constructor initialization for buffers.
     * The buffer's position is set to 0 before writing.
     *
     * @param buf            the buffer to write to.
     * @param maxBlockSize   the max blocksize to use which must be &gt;= {@link #MIN_BLOCK_SIZE}
     *                       and &lt;= {@link #MAX_BLOCK_SIZE} ints.
     *                       The size of the block will not be larger than this size
     *                       unless a single event itself is larger.
     * @param maxEventCount  the max number of events (including dictionary) in a single block
     *                       which must be &gt;= {@link #MIN_BLOCK_COUNT} and &lt;= {@link #MAX_BLOCK_COUNT}.
     * @param xmlDictionary  dictionary in xml format or null if none.
     * @param bitInfo        set of bits to include in first block header.
     * @param reserved1      set the value of the first "reserved" int in first block header.
     *                       NOTE: only CODA (i.e. EMU) software should use this.
     * @param blockNumber    number at which to start block number counting.
     * @param append         if <code>true</code>, all events to be written will be
     *                       appended to the end of the buffer.
     * @param firstEvent     the first event written into the buffer (after any dictionary).
     *                       May be null. Not useful when writing to a buffer as this
     *                       event may be written using normal means.
     *
     * @throws EvioException if maxBlockSize or maxEventCount exceed limits;
     *                       if appending and buffer is too small to be evio format;
     *                       if buf arg is null;
     *                       if defined dictionary while appending;
     */
    void EventWriterV4::initializeBuffer(std::shared_ptr<ByteBuffer> buf, int maxBlockSize, int maxEventCount,
                              const std::string & xmlDictionary, std::bitset<24> *bitInfo, int reserved1,
                              int blockNumber, bool append, std::shared_ptr<EvioBank> firstEvent)
    {

            if (maxBlockSize < MIN_BLOCK_SIZE) {
                throw EvioException("Max block size arg (" + std::to_string(maxBlockSize) + ") must be >= " +
                                        std::to_string(MIN_BLOCK_SIZE));
            }

            if (maxBlockSize > MAX_BLOCK_SIZE) {
                throw EvioException("Max block size arg (" + std::to_string(maxBlockSize) + ") must be <= " +
                                    std::to_string(MAX_BLOCK_SIZE));
            }

            if (maxEventCount < MIN_BLOCK_COUNT) {
                throw EvioException("Max block count arg (" + std::to_string(maxEventCount) + ") must be >= " +
                                    std::to_string(MIN_BLOCK_COUNT));
            }

            if (maxEventCount > MAX_BLOCK_COUNT) {
                throw EvioException("Max block count arg (" + std::to_string(maxEventCount) + ") must be <= " +
                                    std::to_string(MAX_BLOCK_COUNT));
            }

            if (buf == nullptr) {
                throw EvioException("Buffer arg cannot be nullptr");
            }

            if (append && ((!xmlDictionary.empty()) || (firstEvent != nullptr))) {
                throw EvioException("Cannot specify dictionary or first event when appending");
            }

            if (!xmlDictionary.empty()) {
                // 56 is the minimum number of characters for a valid xml dictionary
                if (xmlDictionary.length() < 56) {
                    throw EvioException("Dictionary improper format");
                }

                // Turn dictionary data into bytes
                std::vector<std::string> vec = {xmlDictionary};
                BaseStructure::stringsToRawBytes(vec, dictionaryByteArray);

                // Dictionary length in bytes including bank header of 8 bytes
                dictionaryBytes = dictionaryByteArray.size() + 8;
                // Common block has dictionary
                commonBlockByteSize = dictionaryBytes;
                commonBlockCount = 1;
            }

            this->append        = append;
            this->buffer        = buf;
            this->byteOrder     = buf->order();
            this->reserved1     = reserved1;
            this->blockNumber   = blockNumber;
            this->maxBlockSize  = maxBlockSize;
            this->maxEventCount = maxEventCount;
            this->xmlDictionary = xmlDictionary;

            // Init variables
            split  = 0L;
            toFile = false;
            closed = false;
            eventsWrittenTotal = 0;
            eventsWrittenToBuffer = 0;
            bytesWrittenToBuffer = 0;

            // Get buffer ready for writing. If we're appending, setting
            // the position to 0 lets us read up to the end of the evio
            // data and find the proper place to append to.
            buffer->position(0);
            bufferSize = buf->capacity();

            // Aim for this size block (in bytes)
            targetBlockSize = 4*maxBlockSize;

            if (bitInfo != nullptr) {
                this->bitInfo = *bitInfo;
            }

            if (append) {
                // Check endianness & version
                examineFirstBlockHeader();

                // Prepare for appending by moving buffer position
                toAppendPosition();

                // Buffer position is just before empty last block header
            }

            // Convert first event into bytes
            if (firstEvent != nullptr) {
                firstEventBytes = firstEvent->getTotalBytes();
                std::shared_ptr<ByteBuffer> firstEventBuf = std::make_shared<ByteBuffer>(firstEventBytes);
                firstEventBuf->order(byteOrder);
                firstEvent->write(firstEventBuf);
                uint8_t* data = firstEventBuf->array();
                firstEventByteArray.assign(data, data + firstEventBytes);
                commonBlockByteSize += firstEventBytes;
                commonBlockCount++;
                haveFirstEvent = true;
            }

            // Write first block header into buffer
            // (size & count words are updated when writing events).
            // currentHeaderPosition is set in writeNewHeader() below.
            if (xmlDictionary.empty()) {
                writeNewHeader(0,this->blockNumber++,bitInfo,false,false,haveFirstEvent);
            }
            else {
                writeNewHeader(0,this->blockNumber++,bitInfo,true,false,haveFirstEvent);
            }

            // Write out any dictionary & firstEvent (currentBlockSize updated)
            writeCommonBlock();
    }


    /**
     * Initialization new buffer (not from constructor).
     * The buffer's position is set to 0 before writing.
     *
     * @param buf            the buffer to write to.
     * @param bitInfo        set of bits to include in first block header.
     * @param blockNumber    number at which to start block number counting.
     *
     * @throws EvioException not enough memory in buf for writing.
     */
    void EventWriterV4::reInitializeBuffer(std::shared_ptr<ByteBuffer> buf,
                                           std::bitset<24> *bitInfo, int blockNumber) {
        this->buffer      = buf;
        this->byteOrder   = buf->order();
        this->blockNumber = blockNumber;

        // Init variables
        split  = 0L;
        toFile = false;
        closed = false;
        eventsWrittenTotal = 0;
        eventsWrittenToBuffer = 0;
        bytesWrittenToBuffer = 0;

        // Get buffer ready for writing
        buffer->position(0);
        bufferSize = buf->capacity();

        if (bitInfo != nullptr) {
            this->bitInfo = *bitInfo;
        }

        // Write first block header into buffer
        // (size & count words are updated when writing events).
        // currentHeaderPosition is set in writeNewHeader() below.
        if (xmlDictionary.empty()) {
            writeNewHeader(0,this->blockNumber++,bitInfo,false,false,haveFirstEvent);
        }
        else {
            writeNewHeader(0,this->blockNumber++,bitInfo,true,false,haveFirstEvent);
        }

        // Write out any dictionary & firstEvent (currentBlockSize updated)
        writeCommonBlock();
    }


    /**
     * Static wrapper function used to asynchronously run threads to write to file.
     * Necesssary because std::async cannot run member functions (i.e. asyncFileChannel->write)
     * directly.
     *
     * @param pWriter pointer to this object.
     * @param data    pointer to data.
     * @param len     number of bytes to write.
     */
    void EventWriterV4::staticWriteFunction(EventWriterV4 *pWriter, const char* data, size_t len) {
        pWriter->asyncFileChannel->write(data, len);
    }


    /**
     * Static wrapper function used to asynchronously run threads to do nothing.
     * Used when testing this software but not actually writing to file.
     *
     * @param pWriter pointer to this object.
     */
    void EventWriterV4::staticDoNothingFunction(EventWriterV4 *pWriter) {}



    /**
     * If writing file, is the partition it resides on full?
     * Not full, in this context, means there's enough space to write
     * a full split file + a full record + an extra 10MB as a safety factor.
     *
     * @return true if the partition the file resides on is full, else false.
     */
    bool EventWriterV4::isDiskFull() const {
        if (!toFile) return false;
        return diskIsFull;
    }


    /**
     * If writing to a buffer, get the number of bytes written to it
     * including the ending header.
     * @return number of bytes written to buffer
     */
    uint64_t EventWriterV4::getBytesWrittenToBuffer() const {return bytesWrittenToBuffer;}


    /**
     * Set the buffer being written into (initially set in constructor).
     * This method allows the user to avoid having to create a new EventWriterV4
     * each time a bank needs to be written to a different buffer.
     * This does nothing if writing to a file. Not for use if appending.<p>
     * Do <b>not</b> use this method unless you know what you are doing.
     *
     * @param buf the buffer to write to.
     * @param bitInfo        set of bits to include in first block header.
     * @param blockNumber    number at which to start block number counting.
     * @throws EvioException if this object was not closed prior to resetting the buffer,
     *                       buffer arg is null, or in appending mode.
     */
    void EventWriterV4::setBuffer(std::shared_ptr<ByteBuffer> buf,
                                  std::bitset<24> * bitInfo, int blockNumber) {
            if (toFile) return;
            if (buf == nullptr) {
                throw EvioException("Buffer arg null");
            }
            if (append) {
                throw EvioException("Method not for use if appending");
            }
            if (!closed) {
                throw EvioException("Close EventWriter before changing buffers");
            }

            if (bitInfo != nullptr) {
                this->bitInfo = *bitInfo;
            }

            reInitializeBuffer(buf, bitInfo, blockNumber);
    }


    /**
     * Set the buffer being written into (initially set in constructor).
     * This method allows the user to avoid having to create a new EventWriterV4
     * each time a bank needs to be written to a different buffer.
     * This does nothing if writing to a file. Not for use if appending.<p>
     * Do <b>not</b> use this method unless you know what you are doing.
     *
     * @param buf the buffer to write to.
     * @throws EvioException if this object was not closed prior to resetting the buffer,
     *                       buffer arg is null, or in appending mode.
     */
    void EventWriterV4::setBuffer(std::shared_ptr<ByteBuffer> buf) {
            if (toFile) return;
            if (buf == nullptr) {
                throw EvioException("Buffer arg null");
            }
            if (append) {
                throw EvioException("Method not for use if appending");
            }
            if (!closed) {
                throw EvioException("Close EventWriter before changing buffers");
            }

            reInitializeBuffer(buf, &bitInfo, 1);
    }


    /**
     * Get the buffer being written into.
     * If writing to a buffer, this was initially supplied by user in constructor.
     * If writing to a file, this is the internal buffer.
     * Although this method may seems useful, it requires a detailed knowledge of
     * this class's internals. The {@link #getByteBuffer()} method is much more
     * useful to the user.
     *
     * @return buffer being written into
     */
    std::shared_ptr<ByteBuffer> EventWriterV4::getBuffer() {return buffer;}


    /**
     * If writing to a file, return null.
     * If writing to a buffer, get a duplicate of the user-given buffer
     * being written into. The buffer's position will be 0 and its
     * limit will be the size of the valid data. Basically, it's
     * ready to be read from. The returned buffer shares data with the
     * original buffer but has separate limit, position, and mark.
     * Useful if trying to send buffer over the network.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or writeEvent.
     *
     * @return buffer being written into, made ready for reading;
     *         null if writing to file
     */
    std::shared_ptr<ByteBuffer> EventWriterV4::getByteBuffer() {
        // It does NOT make sense to give the caller the internal buffer
        // used in writing to files. That buffer may contain nothing and
        // most probably won't contain the full file contents.
        if (isToFile()) return nullptr;

        std::lock_guard<std::recursive_mutex> lock(mtx);

        // TODO: We synchronize here so we do not write/close in the middle
        //  of our messing with the buffer.
        std::shared_ptr<ByteBuffer> buf = buffer->duplicate();
        buf->order(buffer->order());

        // Get buffer ready for reading
        buf->flip();

        return buf;
    }


    /**
     * Is this object writing to file?
     * @return {@code true} if writing to file, else {@code false}.
     */
    bool EventWriterV4::isToFile() const {return toFile;}


    /**
     * Has {@link #close()} been called (without reopening by calling
     * {@link #setBuffer(std::shared_ptr<ByteBuffer>)}) ?
     *
     * @return {@code true} if this object closed, else {@code false}.
     */
    bool EventWriterV4::isClosed() {
        std::lock_guard<std::recursive_mutex> lock(mtx);
        return closed;
    }


    /**
     * Get the name of the current file being written to.
     * Returns empty string if no file.
     * @return the name of the current file being written to.
     */
    std::string EventWriterV4::getCurrentFilename() const {
         return currentFileName;
    }


    /**
     * Get the full or absolute name or path of the current file being written to.
     * Returns empty string if no file.
     * @return the absolute name or path of the current file being written to.
     */
    std::string EventWriterV4::getCurrentFilePath() const {
#ifdef USE_FILESYSTEMLIB
        fs::path absolutePath = fs::absolute(currentFilePath);
        return absolutePath.generic_string();
#endif
        char *actualPath = realpath(currentFileName.c_str(), nullptr);
        if (actualPath != nullptr) {
            auto strPath = std::string(actualPath);
            free(actualPath);
            return strPath;
        }
        return currentFileName;
    }


    /**
     * Get the current split count which is the number of files
     * created by this object. Warning, this value may be changing.
     * @return the current split count which is the number of files created by this object.
     */
    uint32_t EventWriterV4::getSplitNumber() const {return splitNumber;}


    /**
     * Get the number of split files produced by this writer.
     * @return number of split files produced by this writer.
     */
    uint32_t EventWriterV4::getSplitCount() const {return splitCount;}


    /**
     * Get the current block number.
     * Warning, this value may be changing.
     * @return the current block number.
     */
    uint32_t EventWriterV4::getBlockNumber() const {return blockNumber;}


    /**
     * Get the number of events written to a file/buffer.
     * Remember that a particular event may not yet be
     * flushed to the file/buffer.
     * If the file being written to is split, the returned
     * value refers to all split files taken together.
     *
     * @return number of events written to a file/buffer.
     */
    uint32_t EventWriterV4::getEventsWritten() const {return eventsWrittenTotal;}


    /**
     * Get the byte order of the buffer/file being written into.
     * @return byte order of the buffer/file being written into.
     */
    ByteOrder EventWriterV4::getByteOrder() const {return byteOrder;}


    /**
     * Set the number with which to start block numbers.
     * This method does nothing if events have already been written.
     * @param startingBlockNumber  the number with which to start block numbers.
     */
    void EventWriterV4::setStartingBlockNumber(int startingBlockNumber) {
        // If events have been written already, forget about it
        if (eventsWrittenTotal > 0) return;
        blockNumber = startingBlockNumber;
    }


    /**
     * Set an event which will be written to the file/buffer as
     * well as to all split files. It's called the "first event" as it will be the
     * first event written to each split file (after the dictionary) if this method
     * is called early enough or the first event was defined in the constructor.<p>
     *
     * Since this method is always called after the constructor, the common block will
     * have already been written with a dictionary and firstEvent if either was
     * defined in the constructor. The event given here will be written
     * immediately somewhere in the body of the file, with the forth-coming split
     * files having that event in the first block along with any dictionary.<p>
     *
     * This means that if the firstEvent is given in the constructor, then the
     * caller may end up with 2 copies of it in a single file (if this method
     * is called once). It's also possible to get 2 copies in a file if this
     * method is called immediately prior to the file splitting.<p>
     *
     * By its nature this method is not useful for writing to a buffer since
     * it is never split and the event can be written to it as any other.<p>
     *
     * Data is transferred byte for byte, so data better be in an endian
     * compatible with the output of this writer.<p>
     *
     * Do not call this while simultaneously calling
     * close, flush, writeEvent, or getByteBuffer.
     *
     * @param node node representing event to be placed first in each file written
     *             including all splits. If null, no more first events are written
     *             to any files.
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if event is bad format;
     *                       if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    void EventWriterV4::setFirstEvent(std::shared_ptr<EvioNode> node) {

        std::lock_guard<std::recursive_mutex> lock(mtx);

        // If getting rid of the first event ...
        if (node == nullptr) {
            if (!xmlDictionary.empty()) {
                commonBlockCount = 1;
                commonBlockByteSize = dictionaryBytes;
            }
            else {
                commonBlockCount = 0;
                commonBlockByteSize = 0;
            }
            firstEventBytes = 0;
            firstEventByteArray.clear();
            haveFirstEvent = false;
            return;
        }

        // Find the first event's bytes and the memory size needed
        // to contain the common events (dictionary + first event).
        if (!xmlDictionary.empty()) {
            commonBlockCount = 1;
            commonBlockByteSize = dictionaryBytes;
        }
        else {
            commonBlockCount = 0;
            commonBlockByteSize = 0;
        }

        firstEventBytes = node->getTotalBytes();
        std::shared_ptr<ByteBuffer> firstEventBuf = std::make_shared<ByteBuffer>(firstEventBytes);
        firstEventBuf->order(byteOrder);
        node->getStructureBuffer(firstEventBuf, true);
        uint8_t* data = firstEventBuf->array();
        firstEventByteArray.assign(data, data + firstEventBytes);

        commonBlockByteSize += firstEventBytes;
        commonBlockCount++;
        haveFirstEvent = true;

        // Write it to the file/buffer now. In this case it may not be the
        // first event written and some splits may not even have it
        // (depending on how many events have been written so far).
        writeEvent(nullptr, firstEventBuf, false);
    }


    /**
     * Set an event which will be written to the file/buffer as
     * well as to all split files. It's called the "first event" as it will be the
     * first event written to each split file (after the dictionary) if this method
     * is called early enough or the first event was defined in the constructor.<p>
     *
     * Since this method is always called after the constructor, the common block will
     * have already been written with a dictionary and firstEvent if either was
     * defined in the constructor. The event given here will be written
     * immediately somewhere in the body of the file, with the forth-coming split
     * files having that event in the first block along with any dictionary.<p>
     *
     * This means that if the firstEvent is given in the constructor, then the
     * caller may end up with 2 copies of it in a single file (if this method
     * is called once). It's also possible to get 2 copies in a file if this
     * method is called immediately prior to the file splitting.<p>
     *
     * By its nature this method is not useful for writing to a buffer since
     * it is never split and the event can be written to it as any other.<p>
     *
     * Data is transferred byte for byte, so data better be in an endian
     * compatible with the output of this writer.<p>
     *
     * The method {@link #writeEvent} calls writeCommonBlock() which, in turn,
     * only gets called when synchronized. So synchronizing this method will
     * make sure firstEvent only gets set while nothing is being written.
     *
     * @param buffer buffer containing event to be placed first in each file written
     *               including all splits. If null, no more first events are written
     *               to any files.
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if event is bad format;
     *                       if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    /*synchronized*/
    void EventWriterV4::setFirstEvent(std::shared_ptr<ByteBuffer> buffer) {

        std::lock_guard<std::recursive_mutex> lock(mtx);

        // If getting rid of the first event ...
        if (buffer == nullptr) {
            if (!xmlDictionary.empty()) {
                commonBlockCount = 1;
                commonBlockByteSize = dictionaryBytes;
            }
            else {
                commonBlockCount = 0;
                commonBlockByteSize = 0;
            }
            firstEventBytes = 0;
            firstEventByteArray.clear();
            haveFirstEvent = false;
            return;
        }

        // Find the first event's bytes and the memory size needed
        // to contain the common events (dictionary + first event).
        if (!xmlDictionary.empty()) {
            commonBlockCount = 1;
            commonBlockByteSize = dictionaryBytes;
        }
        else {
            commonBlockCount = 0;
            commonBlockByteSize = 0;
        }

        firstEventBytes = buffer->remaining();
        std::shared_ptr<ByteBuffer> firstEventBuf = std::make_shared<ByteBuffer>(firstEventBytes);
        firstEventBuf->order(byteOrder);
        firstEventBuf->put(buffer).limit(firstEventBytes).position(0);
        uint8_t* data = firstEventBuf->array();
        firstEventByteArray.assign(data, data + firstEventBytes);

        commonBlockByteSize += firstEventBytes;
        commonBlockCount++;
        haveFirstEvent = true;

        // Write it to the file/buffer now. In this case it may not be the
        // first event written and some splits may not even have it
        // (depending on how many events have been written so far).
        writeEvent(nullptr, firstEventBuf, false);
    }


    /**
     * Set an event which will be written to the file/buffer as
     * well as to all split files. It's called the "first event" as it will be the
     * first event written to each split file (after the dictionary) if this method
     * is called early enough or the first event was defined in the constructor.<p>
     *
     * Since this method is always called after the constructor, the common block will
     * have already been written with a dictionary and firstEvent if either was
     * defined in the constructor. The event given here will be written
     * immediately somewhere in the body of the file, with the forth-coming split
     * files having that event in the first block along with any dictionary.<p>
     *
     * This means that if the firstEvent is given in the constructor, then the
     * caller may end up with 2 copies of it in a single file (if this method
     * is called once). It's also possible to get 2 copies in a file if this
     * method is called immediately prior to the file splitting.<p>
     *
     * By its nature this method is not useful for writing to a buffer since
     * it is never split and the event can be written to it as any other.<p>
     *
     * Do not call this while simultaneously calling
     * close, flush, writeEvent, or getByteBuffer.
     *
     * @param bank event to be placed first in each file written including all splits.
     *             If null, no more first events are written to any files.
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if event is bad format;
     *                       if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if no room when writing to user-given buffer;
     */
    void EventWriterV4::setFirstEvent(std::shared_ptr<EvioBank> bank) {

        std::lock_guard<std::recursive_mutex> lock(mtx);

        // If getting rid of the first event ...
        if (bank == nullptr) {
            if (!xmlDictionary.empty()) {
                commonBlockCount = 1;
                commonBlockByteSize = dictionaryBytes;
            }
            else {
                commonBlockCount = 0;
                commonBlockByteSize = 0;
            }
            firstEventBytes = 0;
            firstEventByteArray.clear();
            haveFirstEvent = false;
            return;
        }

        // Find the first event's bytes and the memory size needed
        // to contain the common events (dictionary + first event).
        if (!xmlDictionary.empty()) {
            commonBlockCount = 1;
            commonBlockByteSize = dictionaryBytes;
        }
        else {
            commonBlockCount = 0;
            commonBlockByteSize = 0;
        }

        firstEventBytes = bank->getTotalBytes();
        std::shared_ptr<ByteBuffer> firstEventBuf = std::make_shared<ByteBuffer>(firstEventBytes);
        firstEventBuf->order(byteOrder);
        bank->write(firstEventBuf);
        firstEventBuf->flip();
        uint8_t* data = firstEventBuf->array();
        firstEventByteArray.assign(data, data + firstEventBytes);
        commonBlockByteSize += firstEventBytes;
        commonBlockCount++;
        haveFirstEvent = true;

        // Write it to the file/buffer now. In this case it may not be the
        // first event written and some splits may not even have it
        // (depending on how many events have been written so far).
        writeEvent(nullptr, firstEventBuf, false);
    }


    /** This method flushes any remaining internally buffered data to file.
     *  Calling {@link #close()} automatically does this so it isn't necessary
     *  to call before closing. This method should only be used when writing
     *  events at such a low rate that it takes an inordinate amount of time
     *  for internally buffered data to be written to the file.<p>
     *
     *  Calling this can kill performance. May not call this when simultaneously
     *  calling writeEvent, close, setFirstEvent, or getByteBuffer. */
    void EventWriterV4::flush() {

        std::lock_guard<std::recursive_mutex> lock(mtx);

        // If lastEmptyBlockHeaderExists is true, then resetBuffer
        // has been called and no events have been written into buffer yet.
        // In other words, no need to flush an empty, last block header.
        // That is only done in close().
        if (closed || !toFile || lastEmptyBlockHeaderExists) {
            return;
        }

        // Write any remaining data
        try {
            // This will kill performance!
            if (flushToFile(true, false)) {
                // If we actually wrote some data, start a new block.
                resetBuffer(false);
            }
        }
        catch (EvioException & e) {}
    }



    /** This method flushes any remaining data to file and disables this object.
     *  May not call this when simultaneously calling
     *  writeEvent, flush, setFirstEvent, or getByteBuffer. */
    void EventWriterV4::close() {

        std::lock_guard<std::recursive_mutex> lock(mtx);

        if (closed) {
            return;
        }

        // Write any remaining data
        try {
            if (toFile) {
                // We need to end the file with an empty block header.
                // If resetBuffer (or flush) was just called,
                // a block header with nothing following will already exist;
                // however, it will not be a "last" block header.
                // Add that now.
                writeNewHeader(0, blockNumber, nullptr, false, true);
                flushToFile(false, false);
            }
            else {
                // Data is written, but need to write empty last header
                writeNewHeader(0, blockNumber, nullptr, false, true);
            }
        }
        catch (EvioException & e) {}

        // Close everything
        try {
            if (toFile) {
                // Finish writing to current file
                if (future1 != nullptr && future1->valid()) {
                    // Wait for last write to end before we continue
                    future1->get();
                }

                if (asyncFileChannel->is_open()) asyncFileChannel->close();
                // Close split file handler thread pool
                if (fileCloser != nullptr) fileCloser->close();
            }
        }
        catch (std::exception & e) {}

        closed = true;
    }


    /**
     * Reads part of the first block (physical record) header in order to determine
     * the evio version # and endianness of the file or buffer in question. These things
     * do <b>not</b> need to be examined in subsequent block headers.
     *
     * @throws EvioException if error reading file;
     *                       if not in append mode;
     *                       if file has bad format;
     */
    void EventWriterV4::examineFirstBlockHeader() {

        std::lock_guard<std::recursive_mutex> lock(mtx);

        // Only for append mode
        if (!append) {
            // Internal logic error, should never have gotten here
            throw EvioException("need to be in append mode");
        }

        uint32_t currentPosition;

        if (toFile) {
            buffer->clear();
            buffer->limit(32);

            //System.out.println("Internal buffer capacity = " + buffer.capacity() + ", pos = " + buffer.position());
            asyncFileChannel->seekg(0);
            asyncFileChannel->read(reinterpret_cast<char *>(buffer->array()), 32);

            // check for errors (eof never reached)
            if (asyncFileChannel->bad()) {
                throwEvioLine("I/O reading error");
            }
            else if (asyncFileChannel->fail()) {
                throwEvioLine("Logical error on read");
            }

            buffer->position(32);
            currentPosition = 0;
        }
        else {
            // Have enough remaining bytes to read?
            if (buffer->remaining() < 32) {
                throwEvioLine("not enough data in buffer");
            }
            currentPosition = buffer->position();
        }

        try {
            // Set the byte order to match the buffer/file's ordering.

            // Check the magic number for endianness. This requires
            // peeking ahead 7 ints or 28 bytes. Set the endianness
            // once we figure out what it is (buffer defaults to big endian).
            byteOrder = buffer->order();
            int magicNumber = buffer->getInt(currentPosition + MAGIC_OFFSET);
            // std::cout << "magic # = " << std::hex << std::showbase << magicNumber << std::endl;
            // std::cout << std::dec;

            if (magicNumber != IBlockHeader::MAGIC_NUMBER) {
                if (byteOrder == ByteOrder::ENDIAN_BIG) {
                    byteOrder = ByteOrder::ENDIAN_LITTLE;
                }
                else {
                    byteOrder = ByteOrder::ENDIAN_BIG;
                }
                buffer->order(byteOrder);

                // Reread magic number to make sure things are OK
                magicNumber = buffer->getInt(currentPosition + MAGIC_OFFSET);
                // std::cout << "Re-read magic # = " << std::hex << std::showbase << magicNumber << std::endl;
                // std::cout << std::dec;
                if (magicNumber != IBlockHeader::MAGIC_NUMBER) {
                    std::cout << "ERROR: reread magic # = " << std::hex << std::showbase << magicNumber << ") & still not right" << std::endl;
                    std::cout << std::dec;
                    throwEvioLine("magic number bad value");
                }
            }

            // Check the version number
            uint32_t bitInfoWord = buffer->getInt(currentPosition + BIT_INFO_OFFSET);
            uint32_t evioVersion = bitInfoWord & VERSION_MASK;
            if (evioVersion != 4)  {
                std::cerr << "ERROR: evio version# = " << evioVersion << std::endl;
                throwEvioLine("wrong evio version data, " + std::to_string(evioVersion));
            }

            // Is there a dictionary?
            hasAppendDictionary = BlockHeaderV4::hasDictionary(bitInfoWord);

//            uint32_t blockLen   = buffer->getInt(currentPosition + BLOCK_LENGTH_OFFSET);
//            uint32_t headerLen  = buffer->getInt(currentPosition + HEADER_LENGTH_OFFSET);
//            uint32_t eventCount = buffer->getInt(currentPosition + EVENT_COUNT_OFFSET);
//            uint32_t blockNum   = buffer->getInt(currentPosition + BLOCK_NUMBER_OFFSET);
//
//            bool lastBlock = BlockHeaderV4::isLastBlock(bitInfoWord);
//
//            std::cout << "blockLength     = " << blockLen << std::endl;
//            std::cout << "blockNumber     = " << blockNum << std::endl;
//            std::cout << "headerLength    = " << headerLen << std::endl;
//            std::cout << "blockEventCount = " << eventCount << std::endl;
//            std::cout << "lastBlock       = " << lastBlock << std::endl;
//            std::cout << "bit info        = " << std::hex << std::showbase << bitInfoWord << std::endl << std::endl;
//            std::cout << std::dec;

        }
        catch (std::underflow_error & a) {
            std::cerr << "ERROR: " << a.what() << std::endl;
            throw EvioException(a);
        }
    }


    /**
     * This method positions a file or buffer for the first {@link #writeEvent(std::shared_ptr<EvioBank>)}
     * in append mode. It places the writing position after the last event (not block header).
     *
     * @throws IOException     if file reading/writing problems
     * @throws EvioException   if bad file/buffer format; if not in append mode
     */
    void EventWriterV4::toAppendPosition() {

            // Only for append mode
            if (!append) {
                // Internal logic error, should never have gotten here
                throw EvioException("need to be in append mode");
            }

            bool lastBlock, readEOF = false;
            uint32_t blockLength, blockEventCount;
            int nBytes;
            uint32_t bitInfo, headerLength, headerPosition;
            uint64_t bytesLeftInFile=0L;
            std::shared_ptr<std::future<void>> future;
            fileWritingPosition = 0L;

            if (toFile) {
#ifdef USE_FILESYSTEMLIB
                bytesLeftInFile = fs::file_size(currentFileName);
#else
                struct stat stt{};
                if (stat(currentFileName.c_str(), &stt) == 0) {
                    bytesLeftInFile = stt.st_size;
                }
                else {
                    throw EvioException("error getting file size of " + currentFileName);
                }
#endif
            }

            // The file's block #s may be fine or they may be messed up.
            // Assume they start with one and increment from there. That
            // way any additional blocks now added to the file will have a
            // reasonable # instead of incrementing from the last existing
            // block.
            blockNumber = 1;

            while (true) {
                nBytes = 0;

                // Read in 8 ints (32 bytes) of block header
                if (toFile) {
                    buffer->clear();
                    buffer->limit(32);

                    while (nBytes < 32) {
                        // There is no internal asyncFileChannel position
                        asyncFileChannel->seekg(fileWritingPosition);
                        asyncFileChannel->read(reinterpret_cast<char *>(buffer->array()) + nBytes, 32-nBytes);
                        int partial = asyncFileChannel->gcount();
                        if (asyncFileChannel->fail()) {
                            throw EvioException("error reading record header from " + currentFileName);
                        }

                        // If EOF ...
                        if (asyncFileChannel->eof()) {
                            if (nBytes != 0) {
                                throw EvioException("bad buffer format");
                            }
                            // Missing last empty record header
                            readEOF = true;
                            break;
                        }
                        nBytes += partial;
                        bytesLeftInFile -= partial;
                    }

                    // If we did not read correct # of bytes or didn't run into EOF right away
                    if (nBytes != 0 && nBytes != 32) {
                        throw EvioException("internal file reading error");
                    }

                    headerPosition = 0;
                    fileWritingPosition += 32;
                }
                else {
                    // std::cout << "toAppendPosition: pos = " << buffer->position() <<
                    //              ", limit = " << buffer->limit() << std::endl;
                    // Have enough remaining bytes to read?
                    if (buffer->remaining() < 32) {
                        throw EvioException("bad buffer format");
                    }
                    headerPosition = buffer->position();
                }

                bitInfo         = buffer->getUInt(headerPosition + BIT_INFO_OFFSET);
                blockLength     = buffer->getUInt(headerPosition + BLOCK_LENGTH_OFFSET);
                headerLength    = buffer->getUInt(headerPosition + HEADER_LENGTH_OFFSET);
                blockEventCount = buffer->getUInt(headerPosition + EVENT_COUNT_OFFSET);
                lastBlock       = BlockHeaderV4::isLastBlock(bitInfo);

//                std::cout << "bitInfo         = " << std::hex << std::showbase << bitInfo << std::endl;
//                std::cout << std::dec;
//                std::cout << "blockLength     = " << blockLength << std::endl;
//                std::cout << "blockNumber     = " << blockNumber << std::endl;
//                std::cout << "headerLength    = " << headerLength << std::endl;
//                std::cout << "blockEventCount = " << blockEventCount << std::endl;
//                std::cout << "lastBlock       = " << lastBlock << std::endl;
//                std::cout << std::endl;

                // Track total number of events in file/buffer (minus dictionary)
                eventsWrittenTotal += blockEventCount;

                blockNumber++;

                // Stop at the last block. The file may not have a last block if
                // improperly terminated. Running into an End-Of-File will flag
                // this condition.
                if (lastBlock || readEOF) {
                    break;
                }

                // Hop to next block header
                if (toFile) {
                    uint64_t bytesToNextBlockHeader = 4*blockLength - 32;
                    if (bytesLeftInFile < bytesToNextBlockHeader) {
                        throw EvioException("bad file format");
                    }

                    //asyncFileChannel->seekg(bytesToNextBlockHeader, std::ios::cur);
                    fileWritingPosition += bytesToNextBlockHeader;
                    bytesLeftInFile -=  bytesToNextBlockHeader;
                }
                else {
                    // Is there enough buffer space to hop over block?
                    if (buffer->remaining() < 4*blockLength) {
                        throw  EvioException("bad buffer format");
                    }

                    buffer->position(buffer->position() + 4*blockLength);
                }
            }

            if (hasAppendDictionary) {
                eventsWrittenToFile = eventsWrittenToBuffer = eventsWrittenTotal + 1;
            }
            else {
                eventsWrittenToFile = eventsWrittenToBuffer = eventsWrittenTotal;
            }

            //-------------------------------------------------------------------------------
            // If we're here, we've just read the last block header (at least 8 words of it).
            // File position is just past header, but buffer position is just before it.
            // Either that or we ran into end of file (last block header missing).
            //
            // If EOF, last block header missing, we're good.
            //
            // Else check to see if the last block contains data. If it does,
            // change a single bit so it's not labeled as the last block,
            // then jump past all data.
            //
            // Else if there is no data, position file before it as preparation for writing
            // the next block.
            //-------------------------------------------------------------------------------

            // If no last, empty block header in file ...
            if (readEOF) {
                // It turns out we need to do nothing. The constructor that
                // calls this method will write out the next block header.
                blockNumber--;
            }
            // If last block has event(s) in it ...
            else if (blockLength > headerLength) {
                // Clear last block bit in 6th header word
                bitInfo = BlockHeaderV4::clearLastBlockBit(bitInfo);

                // Rewrite header word with new bit info & hop over block

                // File now positioned right after the last header to be read
                if (toFile) {
                    // Back up to before 6th block header word
                    fileWritingPosition -= 32 - BIT_INFO_OFFSET;
                    //System.out.println("toAppendPosition: writing over last block's 6th word, back up %d words" +(8 - 6));

                    // Write over 6th block header word
                    buffer->clear();
                    buffer->putInt(bitInfo);
                    buffer->flip();

                    asyncFileChannel->seekg(fileWritingPosition);
                    asyncFileChannel->write(reinterpret_cast<char *>(buffer->array()), 32);
                    buffer->clear();

                    // Hop over the entire block
                    // std::cout << "toAppendPosition: wrote over last block's 6th word, hop over %d words" <<
                    //                   (blockLength - (6 + 1)) << std::endl;
                    fileWritingPosition += 4 * blockLength - (BIT_INFO_OFFSET + 1);
                }
                // Buffer still positioned right before the last header to be read
                else {
                    //System.out.println("toAppendPosition: writing bitInfo (" +
                    //                   Integer.toHexString(bitInfo) +  ") over last block's 6th word for buffer at pos " +
                    //                   (buffer.position() + BIT_INFO_OFFSET));

                    // Write over 6th block header word
                    buffer->putInt(buffer->position() + BIT_INFO_OFFSET, bitInfo);

                    // Hop over the entire block
                    buffer->position(buffer->position() + 4*blockLength);
                }
            }
            // else if last block has NO data in it ...
            else {
                // We already read in the block header, now back up so we can overwrite it.
                // If using buffer, we never incremented the position, so we're OK.
                blockNumber--;
                if (toFile) {
                    fileWritingPosition -= 32;
                    //System.out.println("toAppendPos: position (bkup) = " + fileWritingPosition);
                }
            }

            // Write empty last block header. Thus if our program crashes, the file
            // will be OK. This last block header will be over-written with each
            // subsequent write/flush.
            if (toFile) {
                //System.out.println("toAppendPos: file pos = " + fileWritingPosition);
                bytesWrittenToFile = fileWritingPosition;
            }
            else {
                bytesWrittenToBuffer = buffer->position() + headerBytes;
            }

            // We should now be in a state identical to that if we had
            // just now written everything currently in the file/buffer.
            //System.out.println("toAppendPos: at END, blockNum = " + blockNumber);
    }


    /**
     * Write a block header into the given buffer.
     * After this method is called the buffer position is after the
     * new block header.
     *
     * @param eventCount    number of events in block
     * @param blockNumber   number of block
     * @param bitInfo       set of bits to include in first block header
     * @param hasDictionary does this block have a dictionary?
     * @param isLast        is this the last block?
     * @param hasFirstEv    does this block have a first event?
     *
     * @throws EvioException if no room in buffer to write this block header
     */
    void EventWriterV4::writeNewHeader(uint32_t eventCount,
                            uint32_t blockNumber, std::bitset<24> * bitInfo,
                            bool hasDictionary, bool isLast, bool hasFirstEv)
    {

            // If no room left for a header to be written ...
            if (buffer->remaining() < 32) {
                throw EvioException("Buffer size exceeded, need 32 but have " +
                                    std::to_string(buffer->remaining()) + " bytes");
            }

            // Record where beginning of header is so we can
            // go back and update block size and event count.
            currentHeaderPosition = buffer->position();
            //System.out.println("writeNewHeader: set currentHeaderPos to " + currentHeaderPosition);

            // Calculate the 6th header word (ok if bitInfo == nullptr)
            int sixthWord = BlockHeaderV4::generateSixthWord(bitInfo, 4, hasDictionary,
                                                             isLast, 0, hasFirstEv);

            //        System.out.println("EventWriter (header): words = " + words +
            //                ", block# = " + blockNumber + ", ev Cnt = " + eventCount +
            //                ", 6th wd = " + sixthWord);
            //        System.out.println("Evio header: block# = " + blockNumber + ", last = " + isLast);

            // Write header words, some of which will be
            // overwritten later when the length/event count are determined.
            buffer->putInt(headerWords);
            buffer->putInt(blockNumber);
            buffer->putInt(headerWords);
            buffer->putInt(eventCount);
            buffer->putInt(reserved1);
            buffer->putInt(sixthWord);
            buffer->putInt(reserved2);
            buffer->putInt(IBlockHeader::MAGIC_NUMBER);
            if (isLast) {
                //System.out.println("writeNewHeader: last empty header added");
                // Last item in internal buffer is last empty block header
                lastEmptyBlockHeaderExists = true;
            }
            //System.out.println("writeNewHeader: buffer pos = " + buffer->position());

            currentBlockSize = headerWords;
            currentBlockEventCount = 0;
            bytesWrittenToBuffer += headerBytes;
            //System.out.println("writeNewHeader: set bytesWrittenToBuffer +32 to " + bytesWrittenToBuffer);
    }


    /**
     * Write common events (if any) into the first block of the file/buffer.
     * The common events includes the dictionary and the firstEvent.
     * Note that the first event may be set some time after events have
     * started to be written. In that case the firstEvent is written like
     * any other. When the file subsequently splits and this method is called,
     * it will at that time be a part of the first, common block.
     *
     * @throws EvioException if not enough room in buffer
     */
    void EventWriterV4::writeCommonBlock() {

            // No common events to write
            if (xmlDictionary.empty() && !haveFirstEvent) return;

            // Check to see if there is room in buffer for everything
            if (commonBlockByteSize > buffer->remaining()) {
                // If writing to fixed sized buffer, throw exception
                if (!toFile) {
                    throw EvioException("Not enough buffer mem for dictionary & first event");
                }

                // Use a bigger buffer
                expandBuffer(commonBlockByteSize + 2*headerBytes);
                resetBuffer(true);
            }

            //if (debug) System.out.println("writeDictionary: write common block with bank bytes = " +
            //                               commonBlockByteSize + ", remaining = " + buffer.remaining());

            if (!xmlDictionary.empty()) {
                // Write bank header

                // dictionary data len in bytes
                size_t dictBytes = dictionaryByteArray.size();

                buffer->putInt(dictBytes / 4 + 1);

                if (buffer->order() == ByteOrder::ENDIAN_BIG) {
                    buffer->putShort((short) 0);
                    buffer->put((uint8_t) (DataType::CHARSTAR8.getValue()));
                    buffer->put((uint8_t) 0);
                }
                else {
                    buffer->put((uint8_t) 0);
                    buffer->put((uint8_t) (DataType::CHARSTAR8.getValue()));
                    buffer->putShort((short) 0);
                }

                // Write dictionary characters
                buffer->put(dictionaryByteArray, 0, dictBytes);

                // Book keeping
                // evensWrittenTotal is NOT increased here since
                // it does NOT include any dictionaries
                wroteDictionary = true;
                eventsWrittenToBuffer++;
                currentBlockEventCount++;
            }

            if (haveFirstEvent) {
                // Write first event
                buffer->put(firstEventByteArray, 0, firstEventBytes);

                // Book keeping
                eventsWrittenTotal++;
                eventsWrittenToBuffer++;
                currentBlockEventCount++;

                // One event in this block (dictionary gets ignored)
                buffer->putInt(currentHeaderPosition + EVENT_COUNT_OFFSET, 1);
            }

            // Update header's # of 32 bit words written to current block/buffer
            currentBlockSize     += commonBlockByteSize /4;
            bytesWrittenToBuffer += commonBlockByteSize;

            buffer->putInt(currentHeaderPosition, currentBlockSize);
            // As soon as we write an event, we need another last empty block
            lastEmptyBlockHeaderExists = false;
            //if (debug) System.out.println("writeDictionary: done writing dictionary, remaining = " +
            //                              buffer->remaining());
    }


    /**
     * This method initializes the internal buffer
     * as if the constructor was just called and resets some variables.
     * It starts a new block.
     * @param beforeDictionary is this to reset buffer as it was before the
     *                         writing of the dictionary?
     */
    void EventWriterV4::resetBuffer(bool beforeDictionary) {
        // Go back to the beginning of the buffer & set limit to cap
        buffer->clear();

        // Reset buffer values
        bytesWrittenToBuffer  = 0;
        eventsWrittenToBuffer = 0;

        // Initialize block header as empty, last block
        // and start writing after it.

        try {
            if (beforeDictionary) {
                //if (debug) System.out.println("      resetBuffer: as in constructor");
                blockNumber = 1;
                writeNewHeader(0, blockNumber++, nullptr, (!xmlDictionary.empty()), false, haveFirstEvent);
            }
            else {
                //if (debug) System.out.println("      resetBuffer: NOTTTT as in constructor");
                writeNewHeader(0, blockNumber++, nullptr, false, false, haveFirstEvent);
            }
        }
        catch (EvioException e) {/* never happen */}
    }


    /**
     * This method expands the size of the internal buffers used when
     * writing to files. Some variables are updated.
     *
     * @param newSize size in bytes to make the new buffers
     */
    void EventWriterV4::expandBuffer(int newSize) {
        // No need to increase it
        if (newSize <= bufferSize) {
            //System.out.println("    expandBuffer: buffer is big enough");
            return;
        }

        // Use the new buffer from here on
        internalBuffers.clear();
        internalBuffers.push_back(std::make_shared<ByteBuffer>(newSize));
        internalBuffers.push_back(std::make_shared<ByteBuffer>(newSize));
        internalBuffers[0]->order(byteOrder);
        internalBuffers[1]->order(byteOrder);
        buffer = internalBuffers[0];
        bufferSize = newSize;

        //System.out.println("    expandBuffer: increased buf size to " + newSize + " bytes");
    }


    /**
     * This routine writes an event into the internal buffer
     * and does much of the bookkeeping associated with it.
     *
     * @param currentEventBytes  number of bytes to write from buffer
     * @throws EvioException if error writing event into internal buffer.
     */
    void EventWriterV4::writeEventToBuffer(std::shared_ptr<EvioBank> bank,
                                           std::shared_ptr<ByteBuffer> bankBuffer,
                                           int currentEventBytes) {

        //if (debug) std::cout << "  writeEventToBuffer: before write, bytesToBuf = " <<
        //                bytesWrittenToBuffer << std::endl;

        // Write event to internal buffer
        uint32_t bankLim = 0,bankPos = 0,bankCap = 0;
        if (bankBuffer != nullptr) {
            bankLim = bankBuffer->limit();
            bankPos = bankBuffer->position();
            bankCap = bankBuffer->capacity();
        }

        uint32_t rbankLim = buffer->limit();
        uint32_t rbankPos = buffer->position();
        uint32_t rbankCap = buffer->capacity();

        try {
            if (bankBuffer != nullptr) {
                buffer->put(bankBuffer);
            }
            else if (bank != nullptr) {
                bank->write(buffer);
            }
            else {
                return;
            }
        }
        catch (std::runtime_error & e) {
            std::cerr << "Error trying to write event buf (lim = " << bankLim << ", cap = " << bankCap <<
                               ", pos = " << bankPos << ") to internal buf (lim = " << rbankLim <<
                               ", cap = " << rbankCap << ", pos = " << rbankPos << ")" << std::endl;
            throw EvioException(e);
        }

        // Update the current block header's size and event count
        currentBlockSize += currentEventBytes/4;
        bytesWrittenToBuffer += currentEventBytes;

        eventsWrittenTotal++;
        eventsWrittenToBuffer++;
        currentBlockEventCount++;

        buffer->putInt(currentHeaderPosition, currentBlockSize);
        buffer->putInt(currentHeaderPosition + EVENT_COUNT_OFFSET,
                       currentBlockEventCount);

        // If we wrote a dictionary and it's the first block,
        // don't count dictionary in block header's event count
        if (wroteDictionary && (blockNumber == 2) && (currentBlockEventCount > 1)) {
            //if (debug)  System.out.println("  writeEventToBuffer: subtract ev cnt since in dictionary's blk, cnt = " +
            //                                      (currentBlockEventCount - 1));
            buffer->putInt(currentHeaderPosition + EVENT_COUNT_OFFSET,
                          currentBlockEventCount - 1);
        }

        //if (debug) System.out.println("  writeEventToBuffer: after write,  bytesToBuf = " +
        //                bytesWrittenToBuffer + ", blksiz = " + currentBlockSize + ", blkEvCount (w/ dict) = " +
        //                currentBlockEventCount + ", blk # = " + blockNumber + ", wrote Dict = " + wroteDictionary);

        // If we're writing over the last empty block header, clear last block bit
        // This will happen if expanding buffer.
        int headerInfoWord = buffer->getInt(currentHeaderPosition + BIT_INFO_OFFSET);
        if (BlockHeaderV4::isLastBlock(headerInfoWord)) {
            //System.out.println("  writeEventToBuffer: clear last event bit in block header");
            buffer->putInt(currentHeaderPosition + BIT_INFO_OFFSET,
                          BlockHeaderV4::clearLastBlockBit(headerInfoWord));
        }

        // As soon as we write an event, we need another last empty block
        lastEmptyBlockHeaderExists = false;

        //        if (debug) {
        //            System.out.println("evWrite: after last header written, Events written to:");
        //            System.out.println("         cnt total (no dict) = " + eventsWrittenTotal);
        //            System.out.println("         file cnt total (dict) = " + eventsWrittenToFile);
        //            System.out.println("         internal buffer cnt (dict) = " + eventsWrittenToBuffer);
        //            System.out.println("         block cnt (dict) = " + currentBlockEventCount);
        //            System.out.println("         bytes-to-buf  = " + bytesWrittenToBuffer);
        //            System.out.println("         bytes-to-file = " + bytesWrittenToFile);
        //            System.out.println("         block # = " + blockNumber);
        //        }
    }


    /**
     * Is there room to write this many bytes to an output buffer as a single event?
     * Will always return true when writing to a file.
     * @param bytes number of bytes to write
     * @return {@code true} if there still room in the output buffer, else {@code false}.
     */
    bool EventWriterV4::hasRoom(int bytes) {
        //System.out.println("Buffer size = " + bufferSize + ", bytesWritten = " + bytesWrittenToBuffer +
        //        ", <? " + (bytes + headerBytes));
        return isToFile() || (bufferSize - bytesWrittenToBuffer) >= bytes + headerBytes;
    }

    /**
     * Write an event (bank) to the buffer in evio version 4 format.
     * If the internal buffer is full, it will be flushed to the file if writing to a file.
     * Otherwise an exception will be thrown. Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.
     *
     * @param node   object representing the event to write in buffer form
     * @param force  if writing to disk, force it to write event to the disk.
     *
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         or bank and bankBuffer args are both null.
     *         If there is an InterruptedException.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     */
    bool EventWriterV4::writeEvent(std::shared_ptr<EvioNode> node, bool force) {
            // Duplicate buffer so we can set pos & limit without messing others up
            return writeEvent(node, force, true);
    }

    /**
     * Write an event (bank) to the buffer in evio version 4 format.
     * If the internal buffer is full, it will be flushed to the file if writing to a file.
     * Otherwise an exception will be thrown. Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.
     *
     * @param node       object representing the event to write in buffer form
     * @param force      if writing to disk, force it to write event to the disk.
     * @param duplicate  if true, duplicate node's buffer so its position and limit
     *                   can be changed without issue.
     *
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         or bank and bankBuffer args are both null.
     *         If there is an InterruptedException.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if null node arg;
     */
    bool EventWriterV4::writeEvent(std::shared_ptr<EvioNode> node, bool force, bool duplicate) {
            if (node == nullptr) {
                throw EvioException("null node arg");
            }

            //        int origLim=0,origPos=0;
            std::shared_ptr<ByteBuffer> eventBuffer, bb = node->getBuffer();

            // Duplicate buffer so we can set pos & limit without messing others up
            if (duplicate) {
                // TODO: garbage producing call
                eventBuffer = bb->duplicate();
                eventBuffer->order(bb->order());
            }
            else {
                eventBuffer = bb;
                //            origLim = bb->limit();
                //            origPos = bb->position();
            }

            int pos = node->getPosition();
            eventBuffer->limit(pos + node->getTotalBytes()).position(pos);
            return writeEvent(nullptr, eventBuffer, force);

            // Shouldn't the pos & lim be reset for non-duplicate?
            // It don't think it matters since node knows where to
            // go inside the buffer.
            //        if (!duplicate) {
            //            bb->limit(origLim).position(origPos);
            //        }
    }

    /**
     * Write an event (bank) into a record and eventually to a file in evio
     * version 4 format.
     * If the internal buffer is full, it will be flushed to the file.
     * Otherwise an exception will be thrown.<p>
     *
     * <b>
     * If splitting files, this method returns false if disk partition is too full
     * to write the complete, next split file. If force arg is true, write anyway.
     * DO NOT mix calling this method with calling {@link #writeEvent(std::shared_ptr<EvioNode>, bool, bool)}
     * (or the methods which call it). Results are unpredictable as it messes up the
     * logic used to quit writing to full disk.
     * </b>
     *
     * The buffer must contain only the event's data (event header and event data)
     * and must <b>not</b> be in complete evio file format.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     * <b>kill</b> performance when writing to a file.
     * A true 3rd arg can be used when the backing buffer
     * of the node is accessed by multiple threads simultaneously. This allows
     * that buffer's limit and position to be changed without interfering
     * with the other threads.<p>
     *
     * @param node       object representing the event to write in buffer form
     * @param force      if writing to disk, force it to write event to the disk.
     * @param duplicate  if true, duplicate node's buffer so its position and limit
     *                   can be changed without issue.
     * @return true if event was added to record. If splitting files, false if disk
     *         partition too full to write the complete, next split file.
     *         False if interrupted. If force arg is true, write anyway.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if null node arg;
     */
    bool EventWriterV4::writeEventToFile(std::shared_ptr<EvioNode> node, bool force, bool duplicate) {
        if (node == nullptr) {
            throw EvioException("null node arg");
        }

        if (node->getBuffer() == nullptr) {
            throw EvioException("EvioNode backing buf = null! race condition?");
        }
        std::shared_ptr<ByteBuffer> eventBuffer;
        std::shared_ptr<ByteBuffer> bb = node->getBuffer();

        // Duplicate buffer so we can set pos & limit without messing others up
        if (duplicate) {
            // TODO: garbage producing call
            eventBuffer = bb->duplicate();
            eventBuffer->order(bb->order());
        }
        else {
            eventBuffer = bb;
        }

        size_t pos = node->getPosition();
        eventBuffer->limit(pos + node->getTotalBytes()).position(pos);
        return writeEventToFile(nullptr, eventBuffer, force);
    }

    /**
     * Write an event (bank) to the buffer in evio version 4 format.
     * The given event buffer must contain only the event's data (event header
     * and event data) and must <b>not</b> be in complete evio file format.
     * If the internal buffer is full, it will be flushed to the file if writing to a file.
     * Otherwise an exception will be thrown. Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.
     *
     * @param eventBuffer the event (bank) to write in buffer form
     *
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         or bank and bankBuffer args are both null.
     *         If there is an InterruptedException.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     */
    bool EventWriterV4::EventWriterV4::writeEvent(std::shared_ptr<ByteBuffer> eventBuffer) {
        return writeEvent(nullptr, eventBuffer, false);
    }

    /**
     * Write an event (bank) to a buffer containing evio version 4 format blocks.
     * Each block has an integral number of events. There are limits to the
     * number of events in each block and the total size of each block.
     * If writing to a file, each full buffer is written - one at a time -
     * and may contain multiple blocks. Dictionary is never written with
     * this method. Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.
     *
     * @param bank the bank to write.
     *
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         or bank and bankBuffer args are both null.
     *         If there is an InterruptedException.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     */
    bool EventWriterV4::writeEvent(std::shared_ptr<EvioBank> bank) {
        return writeEvent(bank, nullptr, false);
    }


    /**
     * Write an event (bank) to the buffer in evio version 4 format.
     * The given event buffer must contain only the event's data (event header
     * and event data) and must <b>not</b> be in complete evio file format.
     * If the internal buffer is full, it will be flushed to the file if
     * writing to a file. Otherwise an exception will be thrown.
     *  Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     * Be warned that injudicious use of the 2nd arg, the force flag, will
     * <b>kill</b> performance.
     *
     * @param bankBuffer the bank (as a ByteBuffer object) to write.
     * @param force      if writing to disk, force it to write event to the disk.
     *
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         or bank and bankBuffer args are both null.
     *         If there is an InterruptedException.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if close() already called;
     *                       if bad eventBuffer format;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     */
    bool EventWriterV4::writeEvent(std::shared_ptr<ByteBuffer> bankBuffer, bool force) {
            return writeEvent(nullptr, bankBuffer, force);
    }


    /**
     * Write an event (bank) to a buffer containing evio version 4 format blocks.
     * Each block has an integral number of events. There are limits to the
     * number of events in each block and the total size of each block.
     * If writing to a file, each full buffer is written - one at a time -
     * and may contain multiple blocks. Dictionary is never written with
     * this method. Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     * Be warned that injudicious use of the 2nd arg, the force flag, will
     * <b>kill</b> performance.
     *
     * @param bank   the bank to write.
     * @param force  if writing to disk, force it to write event to the disk.
     *
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         or bank and bankBuffer args are both null.
     *         If there is an InterruptedException.
     *
     * @throws IOException   if error writing file
     * @throws EvioException if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     */
    bool EventWriterV4::writeEvent(std::shared_ptr<EvioBank> bank, bool force) {
            return writeEvent(bank, nullptr, force);
    }


    /**
     * Write an event (bank) to a buffer in evio version 4 format.
     * The event to be written may be in one of two forms.
     * The first is as an EvioBank object and the second is as a ByteBuffer
     * containing only the event's data (event header and event data) and must
     * <b>not</b> be in complete evio file format.
     * The first non-null of the bank arguments will be written.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     *<b>kill</b> performance.<p>
     *
     * This method is not used to write the dictionary or the first event
     * (common block). That is only done with the method {@link #writeCommonBlock}.
     *
     * @param bank the bank (as an EvioBank object) to write.
     * @param bankBuffer the bank (as a ByteBuffer object) to write.
     * @param force      if writing to disk, force it to write event to the disk.
     *
     * @return if writing to buffer: true if event was added to record, false if buffer full,
     *         or bank and bankBuffer args are both null.
     *
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if bad bankBuffer format;
     *                       if close() already called;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if error writing file;
     */
    bool EventWriterV4::writeEvent(std::shared_ptr<EvioBank> bank,
                                   std::shared_ptr<ByteBuffer> bankBuffer, bool force) {

        std::lock_guard<std::recursive_mutex> lock(mtx);

            if (closed) {
                throw EvioException("close() has already been called");
            }

            bool doFlush = false;
            bool roomInBuffer = true;
            bool splittingFile = false;
            bool needBiggerBuffer = false;
            bool writeNewBlockHeader = true;

            // See how much space the event will take up
            int newBufSize = 0;
            int currentEventBytes;

            // Which bank do we write?
            if (bankBuffer != nullptr) {
                if (bankBuffer->order() != byteOrder) {
                    throw EvioException("event buf is " + bankBuffer->order().getName() +
                                        ", and writer is " + byteOrder.getName());
                }

                // Event size in bytes (from buffer ready to read)
                currentEventBytes = bankBuffer->remaining();

                // Size must be multiple of 4 bytes (whole 32-bit ints)
                // The following is the equivalent of the mod operation:
                //  (currentEventBytes % 4 != 0) but is much faster (x mod 2^n == x & (2^n - 1))
                if ((currentEventBytes & 3) != 0) {
                    throw EvioException("bad bankBuffer format");
                }

                // Check for inconsistent lengths
                if (currentEventBytes != 4*(bankBuffer->getInt(bankBuffer->position()) + 1)) {
                    throw EvioException("inconsistent event lengths: total bytes from event = " +
                                         std::to_string(4*(bankBuffer->getInt(bankBuffer->position()) + 1)) +
                                         ", from buffer = " + std::to_string(currentEventBytes));
                }
            }
            else if (bank != nullptr) {
                currentEventBytes = bank->getTotalBytes();
            }
            else {
                return false;
            }

            //        if (split > 0) {
            //            System.out.println("evWrite: splitting, bytesToFile = " + bytesWrittenToFile +
            //                               ", event bytes = " + currentEventBytes +
            //                               ", bytesToBuf = " + bytesWrittenToBuffer +
            //                               ", split = " + split);
            //
            //            System.out.println("evWrite: blockNum = " + blockNumber +
            //                              ", (blkNum == 2) = " + (blockNumber == 2) +
            //                              ", eventsToBuf (" + eventsWrittenToBuffer +
            //                              ")  <=? common blk cnt (" + commonBlockCount + ")");
            //        }

            // If we have enough room in the current block and have not exceeded
            // the number of allowed events, write it in the current block.
            // Worry about memory later.
            if ( ((currentEventBytes + 4*currentBlockSize) <= targetBlockSize) &&
                  (currentBlockEventCount < maxEventCount)) {
                writeNewBlockHeader = false;
                //System.out.println("evWrite: do NOT need a new blk header: blk size target = " + targetBlockSize +
                //                    " >= " + (currentEventBytes + 4*currentBlockSize) +
                //                    " bytes,  " + "blk count = " + currentBlockEventCount + ", max = " + maxEventCount);
            }
            //        else {
            //System.out.println("evWrite: DO need a new blk header: blk size target = " + targetBlockSize +
            //                    " < " + (currentEventBytes + 4*currentBlockSize + headerBytes) + " bytes,  " +
            //                    "blk count = " + currentBlockEventCount + ", max = " + maxEventCount);
            //            if (currentBlockEventCount >= maxEventCount) {
            //                System.out.println("evWrite: too many events in block, already have " +
            //                                           currentBlockEventCount);
            //            }
            //        }


            // Are we splitting files in general?
            while (split > 0) {
                // If it's the first block and all that has been written so far are
                // the dictionary and first event, don't split after writing it.
                if (blockNumber == 2 && eventsWrittenToBuffer <= commonBlockCount) {
                    //if (debug) System.out.println("evWrite: don't split file cause only common block written so far");
                    break;
                }

                // Is this event (with the current buffer, current file,
                // and ending block header) large enough to split the file?
                long totalSize = currentEventBytes + bytesWrittenToFile +
                                 bytesWrittenToBuffer + headerBytes;

                // If we have to add another block header (before this event), account for it.
                if (writeNewBlockHeader) {
                    totalSize += headerBytes;
                    //                headerCount++;
                    //if (debug) System.out.println("evWrite: account for another block header when splitting");
                }

                //if (debug) System.out.println("evWrite: splitting = " + (totalSize > split) +
                //                    ": total size = " + totalSize + " >? split = " + split);
                //
                //if (debug) System.out.println("evWrite: total size components: bytesToFile = " +
                //                bytesWrittenToFile + ", bytesToBuf = " + bytesWrittenToBuffer +
                //                ", event bytes = " + currentEventBytes);

                // If we're going to split the file ...
                if (totalSize > split) {
                    // Yep, we're gonna do it
                    splittingFile = true;

                    // Flush the current buffer if any events contained and prepare
                    // for a new file (split) to hold the current event.
                    if (eventsWrittenToBuffer > 0) {
                        doFlush = true;
                        //if (debug) System.out.println("evWrite: eventsToBuf > 0 so doFlush = True");
                    }
                }

                break;
            }

            //if (debug) System.out.println("evWrite: bufSize = " + bufferSize +
            //                              " <? bytesToWrite = " + (bytesWrittenToBuffer + currentEventBytes) +
            //                              " + 64 = " + (bytesWrittenToBuffer + currentEventBytes + 64) +
            //                              ", events in buf = " + eventsWrittenToBuffer);

            // Is this event (by itself) too big for the current internal buffer?
            // Internal buffer needs room for first block header, event, and ending empty block.
            if (bufferSize < currentEventBytes + 2*headerBytes) {
                if (!toFile) {
                    std::cerr << "evWrite: error, bufSize = " << bufferSize <<
                                       " <? current event bytes = " << currentEventBytes <<
                                       " + 2 headers (64), total = " << (currentEventBytes + 64) <<
                                       ", room = " << (bufferSize - bytesWrittenToBuffer - headerBytes) << std::endl;
                    throw EvioException("Buffer too small to write event");
                }
                roomInBuffer = false;
                needBiggerBuffer = true;
                //System.out.println("evWrite: NEED bigger internal buffer for 1 big ev, current size = " +
                //                           bufferSize + ", ev + blk hdrs size = " + (currentEventBytes + 2*headerBytes) );
            }
            // Is this event plus ending block header, in combination with events previously written
            // to the current internal buffer, too big for it?
            else if ((!writeNewBlockHeader && ((bufferSize - bytesWrittenToBuffer) < currentEventBytes + headerBytes)) ||
                     ( writeNewBlockHeader && ((bufferSize - bytesWrittenToBuffer) < currentEventBytes + 2*headerBytes)))  {

                if (!toFile) {
                    return false;
                }

                //            if (debug) {
                //                System.out.print("evWrite: NEED to flush buffer and re-use, ");
                //                if (writeNewBlockHeader) {
                //                    System.out.println("buf room = " + (bufferSize - bytesWrittenToBuffer) +
                //                                       ", needed = "  + (currentEventBytes + 2*headerBytes));
                //                }
                //                else {
                //                    System.out.println("buf room = " + (bufferSize - bytesWrittenToBuffer) +
                //                                       ", needed = "  + (currentEventBytes + headerBytes));
                //                }
                //            }
                roomInBuffer = false;
            }
            //        else {
            //            // If we're here, there is room to add event into existing buffer.
            //            // As we're the very first event, we need to set blockNumber.
            //            System.out.println("evWrite: event will fit, events written to buf so far = " + eventsWrittenToBuffer +
            //            ", bytes to buf so far = " + bytesWrittenToBuffer);
            //        }


            // If there is no room in the buffer for this event ...
            if (!roomInBuffer) {
                // If we need more room for a single event ...
                if (needBiggerBuffer) {
                    // We're here because there is not enough room in the internal buffer
                    // to write this single large event.
                    newBufSize = currentEventBytes + 2*headerBytes;
                    //System.out.println("         must expand, bytes needed for 1 big ev + 2 hdrs = " + newBufSize);
                }
                // Flush what we have to file (if anything)
                doFlush = true;
                //System.out.println("evWrite: no room in Buf so doFlush = 1");
            }

            // Do we flush?
            if (doFlush) {
                //System.out.println("evWrite: call flushToFile 1");
                flushToFile(false, false);
             }

            // Do we split the file?
            if (splittingFile) {
                splitFile();
            }

            // Do we expand buffer?
            if (needBiggerBuffer) {
                // If here, we just flushed.
                expandBuffer(newBufSize);
            }

            // If we either flushed events or split the file, reset the
            // internal buffer to prepare it for writing another event.
            // Start a new block.
            if (doFlush || splittingFile) {
                //System.out.println("evWrite: call resetBuffer(false) 1");
                resetBuffer(false);
                // We have a newly initialized buffer ready to write
                // to, so we don't need a new block header.
                writeNewBlockHeader = false;
            }

            //********************************************************************
            // Now we have enough room for the event in the buffer, block & file
            //********************************************************************

            //********************************************************************
            // Before we go on, if the file was actually split, we must add any
            // existing common block (dictionary & first event & block) in the
            // new file before we write the event.
            //********************************************************************
            if (splittingFile && (!xmlDictionary.empty() || haveFirstEvent)) {
                // Memory needed to write: dictionary + first event + 3 block headers
                // (beginning, after dict & first event, and ending) + event
                int neededBytes = commonBlockByteSize + 3*headerBytes + currentEventBytes;
                //if (debug) System.out.println("evWrite: write DICTIONARY after splitting, needed bytes = " + neededBytes);

                // Write block header after dictionary + first event
                writeNewBlockHeader = true;

                // Give us more buffer memory if we need it
                expandBuffer(neededBytes);

                // Reset internal buffer as it was just
                // before writing dictionary in constructor
                //System.out.println("evWrite: call resetBuffer(true) 2");
                resetBuffer(true);

                // Write common block to the internal buffer
                writeCommonBlock();

                // Now continue with writing the event ...
            }

            // Write new block header if required
            if (writeNewBlockHeader) {
                //System.out.println("evWrite: wrote new blk hdr, block # = " + blockNumber);
                writeNewHeader(1, blockNumber++, nullptr, false, false);
                //if (debug) System.out.println("evWrite: wrote new blk hdr, bytesToBuf = " + bytesWrittenToBuffer);
            }

            // Write out the event
            writeEventToBuffer(bank, bankBuffer, currentEventBytes);

            //        if (debug) {
            //            System.out.println("evWrite: after last header written, Events written to:");
            //            System.out.println("         cnt total (no dict) = " + eventsWrittenTotal);
            //            System.out.println("         file cnt total = " + eventsWrittenToFile);
            //            System.out.println("         internal buffer cnt = " + eventsWrittenToBuffer);
            //            System.out.println("         common block cnt = " + commonBlockCount);
            //            System.out.println("         current block cnt (dict) = " + currentBlockEventCount);
            //            System.out.println("         bytes-to-buf  = " + bytesWrittenToBuffer);
            //            System.out.println("         bytes-to-file = " + bytesWrittenToFile);
            //            System.out.println("         block # = " + blockNumber);
            //        }

            // If caller wants to flush the event to disk (say, prestart event) ...
            if (force && toFile) {
                // This will kill performance!
                flushToFile(true, false);

                // Start a new block
                resetBuffer(false);
            }

            return true;
    }


    /**
     * Write an event (bank) into a block and eventually to a file in evio
     * version 4 format. This method will <b>not</b> work with this object setup
     * to write into a buffer.
     *
     * <b>
     * If splitting files, this method returns false if disk partition is too full
     * to write the complete, next split file. If force arg is true, write anyway.
     * DO NOT mix calling this method with calling {@link #writeEvent(std::shared_ptr<EvioBank>, std::shared_ptr<ByteBuffer>, bool)}
     * (or the methods which call it). Results are unpredictable as it messes up the
     * logic used to quit writing to full disk.
     * </b>
     * <p>
     * The event to be written may be in one of two forms.
     * The first is as an EvioBank object and the second is as a ByteBuffer
     * containing only the event's data (event header and event data) and must
     * <b>not</b> be in complete evio file format.
     * The first non-null of the bank arguments will be written.
     * Do not call this while simultaneously calling
     * close, flush, setFirstEvent, or getByteBuffer.<p>
     *
     * Be warned that injudicious use of a true 2nd arg, the force flag, will
     *<b>kill</b> performance.<p>
     *
     * This method is not used to write the dictionary or the first event
     * (common block). That is only done with the method {@link #writeCommonBlock}.
     *
     * @param bank the bank (as an EvioBank object) to write.
     * @param bankBuffer the bank (as a ByteBuffer object) to write.
     * @param force      if writing to disk, force it to write event to the disk.
     *
     * @return true if event was added to block. If splitting files, false if disk
     *         partition too full to write the complete, next split file.
     *         False if interrupted. If force arg is true, write anyway.
     *
     * @throws EvioException if event is opposite byte order of internal buffer;
     *                       if bad bankBuffer format;
     *                       if close() already called;
     *                       if not writing to file;
     *                       if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if error writing file;
     */
    bool EventWriterV4::writeEventToFile(std::shared_ptr<EvioBank> bank,
                                         std::shared_ptr<ByteBuffer> bankBuffer, bool force) {

        std::lock_guard<std::recursive_mutex> lock(mtx);

            if (closed) {
                throw EvioException("close() has already been called");
            }

            if (!toFile) {
                throw EvioException("cannot write to buffer with this method");
            }

            bool doFlush = false;
            bool roomInBuffer = true;
            bool splittingFile = false;
            bool needBiggerBuffer = false;
            bool writeNewBlockHeader = true;

            // See how much space the event will take up
            int newBufSize = 0;
            int currentEventBytes;

            // Which bank do we write?
            if (bankBuffer != nullptr) {
                if (bankBuffer->order() != byteOrder) {
                    throw EvioException("event buf is " + bankBuffer->order().getName() +
                                        ", and writer is " + byteOrder.getName());
                }

                // Event size in bytes (from buffer ready to read)
                currentEventBytes = bankBuffer->remaining();

                // Size must be multiple of 4 bytes (whole 32-bit ints)
                // The following is the equivalent of the mod operation:
                //  (currentEventBytes % 4 != 0) but is much faster (x mod 2^n == x & (2^n - 1))
                if ((currentEventBytes & 3) != 0) {
                    throw EvioException("bad bankBuffer format");
                }

                // Check for inconsistent lengths
                int lengthFromBankData = 4*(bankBuffer->getInt(bankBuffer->position()) + 1);
                if (currentEventBytes != lengthFromBankData) {

                    Util::printBytes(bankBuffer, bankBuffer->position(), 100,
                                         "Bad event, 100 bytes starting at buf pos = " +
                                         std::to_string(bankBuffer->position()));

                    throw EvioException("inconsistent event lengths: total bytes from event = " +
                                            std::to_string(lengthFromBankData) + ", from buffer = " +
                                            std::to_string(currentEventBytes));
                }
            }
            else if (bank != nullptr) {
                currentEventBytes = bank->getTotalBytes();
            }
            else {
                return false;
            }

            // If we have enough room in the current block and have not exceeded
            // the number of allowed events, write it in the current block.
            // Worry about memory later.
            if ( ((currentEventBytes + 4*currentBlockSize) <= targetBlockSize) &&
            (currentBlockEventCount < maxEventCount)) {
                writeNewBlockHeader = false;
            }

            // Are we splitting files in general?
            while (split > 0) {
                // If it's the first block and all that has been written so far are
                // the dictionary and first event, don't split after writing it.
                if (blockNumber == 2 && eventsWrittenToBuffer <= commonBlockCount) {
                    break;
                }

                // Is this event (with the current buffer, current file,
                // and ending block header) large enough to split the file?
                long totalSize = currentEventBytes + bytesWrittenToFile +
                                 bytesWrittenToBuffer + headerBytes;

                // If we have to add another block header (before this event), account for it.
                if (writeNewBlockHeader) {
                    totalSize += headerBytes;
                }

                // If we're going to split the file ...
                if (totalSize > split) {
                    // Yep, we're gonna do it
                    splittingFile = true;

                    // Flush the current buffer if any events contained and prepare
                    // for a new file (split) to hold the current event.
                    if (eventsWrittenToBuffer > 0) {
                        doFlush = true;
                    }
                }

                break;
            }

            // Is this event (by itself) too big for the current internal buffer?
            // Internal buffer needs room for first block header, event, and ending empty block.
            if (bufferSize < currentEventBytes + 2*headerBytes) {
                roomInBuffer = false;
                needBiggerBuffer = true;
            }
            // Is this event plus ending block header (plus new block header as worst case),
            // in combination with events previously written to the current internal buffer,
            // too big for it?
            else if ((bufferSize - bytesWrittenToBuffer) < currentEventBytes + 2*headerBytes) {
                roomInBuffer = false;
            }

            // If there is no room in the buffer for this event ...
            if (!roomInBuffer) {
                // If we need more room for a single event ...
                if (needBiggerBuffer) {
                    // We're here because there is not enough room in the internal buffer
                    // to write this single large event.
                    newBufSize = currentEventBytes + 2*headerBytes;
                }
                // Flush what we have to file (if anything)
                doFlush = true;
            }

            // Do we flush?
            if (doFlush) {
                    // If disk full (not enough room to write another whole split),
                    // this will fail if it's the first write after a split and a
                    // new file needs to be created.
                    if (!flushToFile(false, true)) {
                        return false;
                    }
            }

            // Do we split the file?
            if (splittingFile) {
                // We are always able to finishing writing a file
                splitFile();
            }

            // Do we expand buffer?
            if (needBiggerBuffer) {
                // If here, we just flushed.
                expandBuffer(newBufSize);
            }

            // If we either flushed events or split the file, reset the
            // internal buffer to prepare it for writing another event.
            // Start a new block.
            if (doFlush || splittingFile) {
                resetBuffer(false);
                // We have a newly initialized buffer ready to write
                // to, so we don't need a new block header.
                writeNewBlockHeader = false;
            }

            //********************************************************************
            // Now we have enough room for the event in the buffer, block & file
            //********************************************************************

            //********************************************************************
            // Before we go on, if the file was actually split, we must add any
            // existing common block (dictionary & first event & block) in the
            // new file before we write the event.
            //********************************************************************
            if (splittingFile && (!xmlDictionary.empty() || haveFirstEvent)) {
                // Memory needed to write: dictionary + first event + 3 block headers
                // (beginning, after dict & first event, and ending) + event
                int neededBytes = commonBlockByteSize + 3*headerBytes + currentEventBytes;

                // Write block header after dictionary + first event
                writeNewBlockHeader = true;

                // Give us more buffer memory if we need it
                expandBuffer(neededBytes);

                // Reset internal buffer as it was just
                // before writing dictionary in constructor
                resetBuffer(true);

                // Write common block to the internal buffer
                writeCommonBlock();

                // Now continue with writing the event ...
            }

            // If we can't allow any more events in due to limited disk space
            if (diskIsFull && !force) {
                // Check disk again
                if (fullDisk()) {
                    return false;
                }
            }

            // Write new block header if required
            if (writeNewBlockHeader) {
                writeNewHeader(1, blockNumber++, nullptr, false, false);
            }

            // Write out the event
            writeEventToBuffer(bank, bankBuffer, currentEventBytes);

            // If caller wants to flush the event to disk (say, prestart event) ...
            if (force) {
                // This will kill performance!
                // If forcing, we ignore any warning of a full disk
                // (not enough room to write another whole split).
                flushToFile(true, false);

                // Start a new block
                resetBuffer(false);
            }

            return true;
    }


    /**
     * Check if disk is able to store 1 full split, 1 max block, and 10MB buffer zone.
     * @return  true if disk is full and not able to accommodate needs, else false.
     */
    bool EventWriterV4::fullDisk() {

        uint64_t freeBytes;

#ifdef __cpp_lib_filesystem
        fs::space_info spaceInfo = fs::space(currentFilePath.parent_path());
        freeBytes = spaceInfo.available;
        //std::cout << "EventWriter constr: " << freeBytes << " bytes available in dir = " <<
        //         currentFilePath.parent_path().generic_string() << std::endl;
#else
        struct statvfs sttvfs{};
        if (statvfs(currentFileName.c_str(), &sttvfs) == 0) {
            freeBytes = sttvfs.f_bavail * sttvfs.f_frsize;
        }
        else {
            // Don't know how much space we have so pretend like we have an extra 40GB.
            freeBytes = split + 40000000000;
            // std::cout << "error getting disk partition's available space for " << currentFileName << std::endl;
        }
#endif

        // If there isn't enough free space to write the complete, projected
        // size file, and we're not trying to force the write, don't flush to file.
        //
        // Note that at this point we are trying to write an entire block just
        // after the file was split. We need to create a new file to do it.
        // So ... we need to leave enough room for both a full split and the
        // current block and a little extra (10MB).
        if (freeBytes < split + bytesWrittenToBuffer + 10000000) {
            //cout << "EventWriter constr: Disk is FULL" << endl;
            diskIsFull = true;
            diskIsFullVolatile = true;
        }

        return diskIsFull;
    }


    /**
     * Flush everything in buffer to file.
     * Does nothing if object already closed.
     *
     * @param force     if true, force it to write event to the disk.
     * @param checkDisk if true and if a new file needs to be created but there is
     *                  not enough free space on the disk partition for the
     *                  complete intended file, return false without creating or
     *                  writing to file. If force arg is true, write anyway.
     *
     * @return true if everything normal; false if a new file needs to be created
     *         (first write after a split) but there is not enough free space on
     *         the disk partition for the next, complete file and checkDisk arg is true.
     *         Will return false if not writing to file.
     *
     * @throws EvioException if this object already closed;
     *                       if file exists but user requested no over-writing;
     *                       if error opening/writing/forcing write to file.
     */
    bool EventWriterV4::flushToFile(bool force, bool checkDisk) {
            if (closed) {
                throw EvioException("close() has already been called");
            }

            // If not writing to file, just return
            if (!toFile) {
                return false;
            }

            // If nothing to write...
            if (buffer->position() < 1) {
                return false;
            }

            // This actually creates the file. Do it only once.
            if (bytesWrittenToFile < 1) {

                // We want to check to see if there is enough room to write the
                // next split, before it's written. Thus, before writing the first block
                // of a new file, we check to see if there's space for the whole thing.
                // (Actually, the whole split + current block + some extra for safety).
                if (checkDisk && (!force) && fullDisk()) {
                    // If we're told to check the disk, and we're not forcing things,
                    // AND disk is full, don't write the block.
                    return false;
                }

// TODO: deal with currentFilePath vs currrentFileName

                // New shared pointer for each file ...
                asyncFileChannel = std::make_shared<std::fstream>();
                asyncFileChannel->open(currentFileName, std::ios::binary | std::ios::trunc | std::ios::out);
                if (asyncFileChannel->fail()) {
                    throw EvioException("error opening file " + currentFileName);
                }

                fileOpen = true;
                fileWritingPosition = 0L;
                splitCount++;
            }

            // The byteBuffer position is just after the last event or
            // the last, empty block header. Get buffer ready to write.
            buffer->flip();

            // Which buffer do we fill next?
            std::shared_ptr<ByteBuffer> unusedBuffer;

            // Write everything in internal buffer out to file
            int bytesWritten = buffer->remaining();

            // We need future job to be completed in order to proceed

            // If 1st time thru, proceed without waiting
            if (future1 == nullptr) {
                // Fill 2nd buffer next
                unusedBuffer = internalBuffers[1];
            }
            // After first time, wait until the future is finished before proceeding
            else {
                // If previous write in progress ...
                if (future1->valid()) {
                    future1->get();

                    // Reuse the buffer future1 just finished using
                    unusedBuffer = usedBuffer;
                }
            }

            future1 = std::make_shared<std::future<void>>(std::future<void>(
                      std::async(std::launch::async,  // run in a separate thread
                           staticWriteFunction, // function to run
                           this,                // arguments to function ...
                           reinterpret_cast<const char *>(buffer->array()),
                           bytesWritten)));

            // Keep track of which buffer future1 used so it can be reused when done
            usedBuffer = buffer;

            // Next buffer to work with
            buffer = unusedBuffer;
            buffer->clear();

            // Force it to write to physical disk (KILLS PERFORMANCE!!!),
            // TODO: This may not work since data may NOT have been written yet!
            if (force) asyncFileChannel->sync();

            // Keep track of what is written to this, one, file
            fileWritingPosition += bytesWritten;
            bytesWrittenToFile  += bytesWritten;
            eventsWrittenToFile += eventsWrittenToBuffer; // events written to internal buffer

            // TODO: from C++ code, see if correct!
            eventsWrittenTotal  += eventsWrittenToBuffer;

        //        if (false) {
        //            std::cout << "    flushToFile: after last header written, Events written to:" << std::endl;
        //            std::cout << "                 cnt total (no dict) = " << eventsWrittenTotal << std::endl;
        //            std::cout << "                 file cnt total (dict) = " << eventsWrittenToFile << std::endl;
        //            std::cout << "                 internal buffer cnt (dict) = " << eventsWrittenToBuffer << std::endl;
        //            std::cout << "                 current  block  cnt (dict) = " << currentBlockEventCount << std::endl;
        //            std::cout << "                 bytes-written  = " << bytesWritten << std::endl;
        //            std::cout << "                 bytes-to-file = " << bytesWrittenToFile << std::endl;
        //            std::cout << "                 block # = " << blockNumber << std::endl;
        //        }

            // New internal buffer has nothing in it
            bytesWrittenToBuffer   = 0;
            eventsWrittenToBuffer  = 0;

            return true;
    }


    /**
     * Split the file.
     * Never called when output destination is buffer.
     * Otherwise it resets the buffer, forces the physical disk to update,
     * closes the old file, and opens the new.
     *
     * @throws EvioException if file could not be opened for writing;
     *                       if file exists but user requested no over-writing;
     *                       if error writing file
     */
    void EventWriterV4::splitFile() {
            // Close existing file (in separate thread for speed)
            // which will also flush remaining data.
            if (asyncFileChannel != nullptr) {
                // We need to end the file with an empty block header.
                // If resetBuffer (or flush) was just called,
                // a block header with nothing following will already exist;
                // however, it will not be a "last" block header.
                // Add that now.
                writeNewHeader(0, blockNumber, nullptr, false, true);
                flushToFile(false, false);
                //System.out.println("    split file: flushToFile for file being closed");

                fileCloser->closeAsyncFile(asyncFileChannel, future1);
            }

            // Right now no file is open for writing
            asyncFileChannel = nullptr;

            // Create the next file's name
            std::string fileName = Util::generateFileName(baseFileName, specifierCount,
                                                          runNumber, split, splitNumber,
                                                          streamId, streamCount);
            splitNumber += splitIncrement;

#ifdef __cpp_lib_filesystem
            currentFilePath = fs::path(fileName);
            bool fileExists = fs::exists(currentFilePath);
            bool isRegularFile = fs::is_regular_file(currentFilePath);
#else
            struct stat stt{};
            bool fileExists = stat( fileName.c_str(), &stt ) == 0;
            bool isRegularFile = S_ISREG(stt.st_mode);
#endif

            // If we can't overwrite and file exists, throw exception
            if (!overWriteOK && (fileExists && isRegularFile)) {
                throw EvioException("File exists but user requested no over-writing, "
                                    + fileName);
            }
            currentFileName = fileName;

            // Reset file values for reuse
            blockNumber         = 1;
            bytesWrittenToFile  = 0;
            eventsWrittenToFile = 0;
            wroteDictionary     = false;

            std::cout << "    splitFile: generated file name = " << fileName << std::endl;
    }

}
