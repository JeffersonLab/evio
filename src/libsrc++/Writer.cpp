//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "Writer.h"


namespace evio {


    /**
     * Default constructor.
     * <b>No</b> file is opened. Any file will have little endian byte order.
     */
    Writer::Writer() :
            Writer(ByteOrder::ENDIAN_LOCAL, 0, 0) {
    }


    /**
     * Constructor with byte order. <b>No</b> file is opened.
     * File header type is evio file ({@link HeaderType#EVIO_FILE}).
     *
     * @param order byte order of written file.
     * @param maxEventCount max number of events a record can hold.
     *                      Value of O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes a record can hold.
     *                      Value of < 8MB results in default of 8MB.
     */
    Writer::Writer(const ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize) :
            Writer(HeaderType::EVIO_FILE, order, maxEventCount, maxBufferSize,
                   std::string(""), nullptr, 0, Compressor::UNCOMPRESSED, false) {
    }


    /**
     * Constructor with filename & byte order.
     * The output file will be created with no user header. No compression.
     *
     * @param filename      output file name
     * @param order         byte order of written file;
     * @param maxEventCount max number of events a record can hold.
     *                      Value of O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes a record can hold.
     *                      Value of < 8MB results in default of 8MB.
     * @throws EvioException  if file cannot be found or IO error writing to file
     */
    Writer::Writer(const std::string & filename, const ByteOrder & order,
                   uint32_t maxEventCount, uint32_t maxBufferSize) :
            Writer(order, maxEventCount, maxBufferSize) {

        open(filename);
    }


    /**
     * Constructor with byte order. The given file is opened so any subsequent call to open will fail.
     * This method places the dictionary and first event into the file header's user header.
     *
     * @param hType           the type of the file. If set to {@link HeaderType#HIPO_FILE},
     *                        the header will be written with the first 4 bytes set to HIPO.
     * @param order           byte order of written file.
     * @param maxEventCount   max number of events a record can hold.
     *                        Value of O means use default (1M).
     * @param maxBufferSize   max number of uncompressed data bytes a record can hold.
     *                        Value of < 8MB results in default of 8MB.
     * @param dictionary      string holding an evio format dictionary to be placed in userHeader.
     * @param firstEvent      byte array containing an evio event to be included in userHeader.
     *                        It must be in the same byte order as the order argument.
     * @param firstEventLen   number of bytes in firstEvent.
     * @param compType        type of data compression to do (one, lz4 fast, lz4 best, gzip)
     * @param addTrailerIndex if true, we add a record index to the trailer.
     */
    Writer::Writer(const HeaderType & hType, const ByteOrder & order,
                   uint32_t maxEventCount, uint32_t maxBufferSize,
                   const std::string & dictionary, uint8_t* firstEvent, uint32_t firstEventLen,
                   const Compressor::CompressionType & compType, bool addTrailerIndex) {

        byteOrder = order;
        this->dictionary = dictionary;
        this->firstEvent = firstEvent;
        if (firstEvent == nullptr) {
            firstEventLen = 0;
        }
        firstEventLength = firstEventLen;
        compressionType = compType;
        this->addTrailerIndex = addTrailerIndex;


        recordLengths = std::make_shared<std::vector<uint32_t>>();
        unusedRecord = std::make_shared<RecordOutput>();
        outputRecord = std::make_shared<RecordOutput>(order, maxEventCount, maxBufferSize, compType, hType);
        headerArray.resize(RecordHeader::HEADER_SIZE_BYTES);

        if (hType == HeaderType::HIPO_FILE) {
            fileHeader = FileHeader(false);
        } else {
            fileHeader = FileHeader(true);
        }

        haveDictionary = !dictionary.empty();
        haveFirstEvent = (firstEvent != nullptr && firstEventLen > 0);

        if (haveDictionary || haveFirstEvent)  {
            dictionaryFirstEventBuffer = createDictionaryRecord();
        }
        else {
            // Tell open() there is no dictionary/first event data
            dictionaryFirstEventBuffer = std::make_shared<ByteBuffer>(1);
            dictionaryFirstEventBuffer->limit(0);
        }
    }


    //////////////////////////////////////////////////////////////////////


    /**
     * Constructor for writing to a ByteBuffer. Byte order is taken from the buffer.
     * No compression.
     * @param buf buffer in to which to write events and/or records.
     */
    Writer::Writer(std::shared_ptr<ByteBuffer> buf) :
            Writer(buf, 0, 0, "", nullptr, 0) {
    }


    /**
     * Constructor with user header. No compression.
     *
     * @param buf        buffer in to which to write events and/or records.
     * @param userHdr    byte array representing the optional user's header.
     *                   <b>Warning: this will not be used until first record is written!
     *                   So don't go changing it in the meantime!</b>
     * @param len        length of valid data (bytes) in userHdr (starting at off).
     */
    Writer::Writer(std::shared_ptr<ByteBuffer> buf, uint8_t * userHdr, uint32_t len) :
            Writer(buf, 0, 0, "", nullptr, 0) {
        open(buf, userHdr, len);
    }


    /**
     * Constructor with byte order.
     * This method places the dictionary and first event into the file header's user header.
     * No compression.
     *
     * @param buf           buffer in to which to write events and/or records.
     * @param maxEventCount max number of events a record can hold.
     *                      Value of O means use default (1M).
     * @param maxBufferSize max number of uncompressed data bytes a record can hold.
     *                      Value of < 8MB results in default of 8MB.
     * @param dictionary    string holding an evio format dictionary to be placed in userHeader.
     * @param firstEvent    byte array containing an evio event to be included in userHeader.
     *                      It must be in the same byte order as the order argument.
     * @param firstEventLen number of bytes in firstEvent.
     */
    Writer::Writer(std::shared_ptr<ByteBuffer> buf, uint32_t maxEventCount, uint32_t maxBufferSize,
                   const std::string & dictionary, uint8_t* firstEvent, uint32_t firstEventLen) {

        byteOrder = buf->order();
        buffer = buf;

        this->dictionary = dictionary;
        this->firstEvent = firstEvent;
        firstEventLength = firstEventLen;

        recordLengths = std::make_shared<std::vector<uint32_t>>();
        headerArray.resize(RecordHeader::HEADER_SIZE_BYTES);
        unusedRecord = std::make_shared<RecordOutput>();
        outputRecord = std::make_shared<RecordOutput>(byteOrder, maxEventCount, maxBufferSize, Compressor::UNCOMPRESSED);

        haveDictionary = !dictionary.empty();
        haveFirstEvent = (firstEvent != nullptr && firstEventLen > 0);

        if (haveDictionary || haveFirstEvent)  {
            dictionaryFirstEventBuffer = createDictionaryRecord();
//std::cout << "Writer const: created dict/fe event buffer of size " << dictionaryFirstEventBuffer->remaining() << std::endl;
//Util::printBytes(dictionaryFirstEventBuffer, 0, dictionaryFirstEventBuffer->remaining(), "FIRST RECORD Bytes");
            // make this the user header by default since open() may not get called for buffers
            // TODO: SHOULD NOT AVOID the shared pointer!!!!! Look at userHeader uses!!!
            userHeader = dictionaryFirstEventBuffer->array();
            userHeaderLength = dictionaryFirstEventBuffer->remaining();
            userHeaderBuffer = dictionaryFirstEventBuffer;
        }

        toFile = false;
    }


    ///**
    // * Move assignment operator.
    // * @param other right side object.
    // * @return left side object.
    // */
    //Writer & Writer::operator=(Writer&& other) noexcept {
    //
    //    // Avoid self assignment ...
    //    if (this != &other) {
    //
    //        toFile = other.toFile;
    //
    //        fileHeader = other.fileHeader;
    //        buffer = other.buffer;
    //        userHeaderBuffer = std::move(other.userHeaderBuffer);
    //        // transfer ownership of the array
    //        userHeader = other.userHeader;
    //        other.userHeader = nullptr;
    //        userHeaderLength = other.userHeaderLength;
    //        firstRecordWritten = other.firstRecordWritten;
    //
    //
    //        dictionary = other.dictionary;
    //        // transfer ownership of the array
    //        firstEvent = other.firstEvent;
    //        other.firstEvent = nullptr;
    //        firstEventLength = other.firstEventLength;
    //        dictionaryFirstEventBuffer = std::move(other.dictionaryFirstEventBuffer);
    //
    //        byteOrder = other.byteOrder;
    //
    //        outputRecord = std::move(other.outputRecord);
    //        unusedRecord = std::move(other.unusedRecord);
    //        beingWrittenRecord = std::move(other.beingWrittenRecord);
    //
    //        headerArray = std::move(other.headerArray);
    //        recordLengths = std::move(other.recordLengths);
    //
    //        compressionType = other.compressionType;
    //        writerBytesWritten = other.writerBytesWritten;
    //        recordNumber = other.recordNumber;
    //
    //        addingTrailer = other.addingTrailer;
    //        addTrailerIndex = other.addTrailerIndex;
    //        closed = other.closed;
    //        opened = other.opened;
    //
    //        if (opened) {
    //            if (toFile) {
    //                // Need to set outFile properly, best done this way
    //                open(fileName, userHeader, userHeaderLength);
    //            } else {
    //                open(buffer, userHeader, userHeaderLength);
    //            }
    //        }
    //
    //        return *this;
    //    }
    //}


    /**
     * Copy assignment operator.
     * @param other right side object.
     * @return left side object.
     */
    Writer & Writer::operator=(const Writer& other) {

        // Avoid self assignment ...
        if (this != &other) {

            toFile = other.toFile;

            fileHeader = other.fileHeader;
            buffer = other.buffer;
            userHeaderBuffer = other.userHeaderBuffer;

            // transfer ownership of the array
            userHeader = other.userHeader;

            userHeaderLength = other.userHeaderLength;
            firstRecordWritten = other.firstRecordWritten;


            dictionary = other.dictionary;
            // transfer ownership of the array
            firstEvent = other.firstEvent;

            firstEventLength = other.firstEventLength;
            dictionaryFirstEventBuffer = other.dictionaryFirstEventBuffer;

            byteOrder = other.byteOrder;

            outputRecord = std::make_shared<RecordOutput>(*(other.outputRecord.get()));
            unusedRecord = std::make_shared<RecordOutput>(*(other.unusedRecord.get()));
            beingWrittenRecord = std::make_shared<RecordOutput>(*(other.beingWrittenRecord.get()));

            headerArray   = other.headerArray;
            // Copy over the vector
            std::vector<uint32_t> & otherRecLen = *(other.recordLengths);
            recordLengths->assign(otherRecLen.begin(), otherRecLen.end());

            compressionType = other.compressionType;
            writerBytesWritten = other.writerBytesWritten;
            recordNumber = other.recordNumber;

            addingTrailer = other.addingTrailer;
            addTrailerIndex = other.addTrailerIndex;
            closed = other.closed;
            opened = other.opened;

            if (opened) {
                if (toFile) {
                    open(fileName, userHeader, userHeaderLength);
                } else {
                    open(buffer, userHeader, userHeaderLength);
                }
            }
        }

        return *this;
    }


    //////////////////////////////////////////////////////////////////////


    /**
     * Static wrapper function used to asynchronously run threads to write to file.
     * Necesssary because std::async cannot run member functions (i.e. outFile.write)
     * directly.
     *
     * @param pWriter pointer to this object.
     * @param data    pointer to data.
     * @param len     number of bytes to write.
     */
    void Writer::staticWriteFunction(Writer *pWriter, const char* data, size_t len) {
        pWriter->outFile.write(data, len);
    }


    /**
     * Get the buffer being written to.
     * This should only be called after calling close() so data is complete.
     * @return buffer being written to.
     */
    std::shared_ptr<ByteBuffer> Writer::getBuffer() {return buffer;}


    /**
     * Get the file's byte order.
     * @return file's byte order.
     */
    const ByteOrder & Writer::getByteOrder() const {return byteOrder;}


    /**
     * Get the file header.
     * @return file header.
     */
    FileHeader & Writer::getFileHeader() {return fileHeader;}


    ///**
    // * Get the internal record's header.
    // * @return internal record's header.
    // */
    //RecordHeader & Writer::getRecordHeader() {return outputRecord->getHeader();}
    //
    ///**
    // * Get the internal record used to add events to file.
    // * @return internal record used to add events to file.
    // */
    //RecordOutput & Writer::getRecord() {return outputRecord;}


    /**
     * Convenience method that gets compression type for the file being written.
     * @return compression type for the file being written.
     */
    Compressor::CompressionType Writer::getCompressionType() {return compressionType;}


    /**
     * Convenience method that sets compression type for the file.
     * The compression type is also set for internal record.
     * When writing to the file, record data will be compressed
     * according to the given type.
     * @param compression compression type
     */
    void Writer::setCompressionType(Compressor::CompressionType compression) {
        if (toFile) {
            compressionType = compression;
            outputRecord->getHeader()->setCompressionType(compression);
        }
    }


    /**
     * Does this writer add a trailer to the end of the file/buffer?
     * @return true if this writer adds a trailer to the end of the file/buffer, else false.
     */
    bool Writer::addTrailer() const {return addingTrailer;}


    /**
     * Set whether this writer adds a trailer to the end of the file/buffer.
     * @param add if true, at the end of file/buffer, add an ending header (trailer)
     *            with no index of records and no following data.
     *            Update the file header to contain a file offset to the trailer.
     */
    void Writer::addTrailer(bool add) {addingTrailer = add;}


    /**
     * Does this writer add a trailer with a record index to the end of the file?
     * Or, if writing to a buffer, is a trailer added with no index?
     * @return if writing to a file: true if this writer adds a trailer with a record index
     *         to the end of the file, else false. If writing to a buffer, true if this
     *         writer adds a traile to the end of the buffer, else false.
     */
    bool Writer::addTrailerWithIndex() {return addTrailerIndex;}


    /**
     * Set whether this writer adds a trailer with a record index to the end of the file.
     * Or, if writing to a buffer, set whether a trailer is added with no index.
     * @param addTrailingIndex if true, at the end of file, add an ending header (trailer)
     *                         with an index of all records but with no following data.
     *                         Update the file header to contain a file offset to the trailer.
     *                         If true, and writing to a buffer, add a trailer with no index
     *                         to the end of the buffer.
     */
    void Writer::addTrailerWithIndex(bool addTrailingIndex) {
        addTrailerIndex = addTrailingIndex;
        if (addTrailingIndex) {
            addingTrailer = true;
        }

        if (!toFile) {
            addTrailerIndex = false;
        }
    }


    /**
     * Open a new file and write file header with no user header.
     * @param filename output file name
     * @throws EvioException if open already called without being followed by calling close.
     * @throws IOException if file cannot be found or IO error writing to file
     */
    void Writer::open(const std::string & filename) {
        open(filename, nullptr, 0);
    }


    /**
     * Open a file and write file header with given user header.
     * User header is automatically padded when written.
     * @param filename   name of file to write to.
     * @param userHdr    byte array representing the optional user's header.
     *                   If this is null AND dictionary and/or first event are given,
     *                   the dictionary and/or first event will be placed in its
     *                   own record and written as the user header.
     * @param userLen    length of valid data (bytes) in userHdr (starting at off).
     * @param overwrite if true, overwrite existing file.
     * @throws EvioException filename arg is null, if constructor specified writing to a buffer,
     *                       if open() was already called without being followed by reset(),
     *                       if IO error writing to file,
     *                       if filename is empty,
     *                       or if overwrite is false and file exists.

     */
    void Writer::open(const std::string & filename, uint8_t* userHdr, uint32_t userLen, bool overwrite) {

        if (opened) {
            throw EvioException("currently open, call reset() first");
        }
        else if (!toFile) {
            throw EvioException("can only write to a buffer, call open(buffer, userHdr, userLen)");
        }

        if (filename.empty()) {
            throw EvioException("bad filename");
        }

        std::shared_ptr<ByteBuffer> fileHeaderBuffer;
        haveUserHeader = false;

        // User header given as arg has precedent
        if (userHdr != nullptr) {
            haveUserHeader = true;
//std::cout << "writer::open: given a valid user header to write" << std::endl;
            fileHeaderBuffer = createHeader(userHdr, userLen);
        }
        else {
            // If dictionary & firstEvent not defined and user header not given ...
            if (dictionaryFirstEventBuffer->remaining() < 1) {
//std::cout << "writer::open: given a null user header to write, userLen = " << userLen <<  std::endl;
                fileHeaderBuffer = createHeader(nullptr, 0);
            }
            // else place dictionary and/or firstEvent into
            // record which becomes user header
            else {
//std::cout << "writer::open: given a valid dict/first ev header to write" << std::endl;
                fileHeaderBuffer = createHeader(*(dictionaryFirstEventBuffer.get()));
            }
        }
//std::cout << "writer::open: fileHeaderBuffer -> \n" << fileHeaderBuffer->toString() << std::endl;

        // Write this to file
        fileName = filename;
        if (!overwrite) {
            // Check if the file exists using std::ifstream
            std::ifstream fileCheck(filename);
            if (fileCheck.good()) {
                throw EvioException("file already exists: " + filename);
            }
        }

        outFile.open(filename, std::ios::binary);
        if (outFile.fail()) {
            throw EvioException("error opening file " + filename);
        }

        outFile.write(reinterpret_cast<const char*>(fileHeaderBuffer->array() +
                                                       fileHeaderBuffer->arrayOffset() +
                                                       fileHeaderBuffer->position()),
                         fileHeaderBuffer->remaining());
        if (outFile.fail()) {
            throw EvioException("error writing to file " + filename);
        }

        writerBytesWritten = (size_t) fileHeader.getLength();
//std::cout << "open: wrote " << writerBytesWritten << " bytes to file for file header only " << std::endl;
        opened = true;
    }


    /**
     * Specify a buffer and write first record header with given user header.
     * User header is automatically padded when written.
     * @param buf        buffer to writer to.
     * @param userHdr    byte array representing the optional user's header.
     *                   <b>Warning: this data will be copied!</b>
     *                   If this is null AND dictionary and/or first event are given,
     *                   the dictionary and/or first event will be placed in its
     *                   own record and written as the user header of the first record's
     *                   header.
     * @param len        length of valid data (bytes) in userHdr.
     * @throws EvioException if constructor specified writing to a file,
     *                       or if open() was already called without being followed by reset().
     */
    void Writer::open(std::shared_ptr<ByteBuffer> buf, uint8_t* userHdr, uint32_t len) {

        if (opened) {
            throw EvioException("currently open, call reset() first");
        }
        else if (toFile) {
            throw EvioException("can only write to a file, call open(filename, userHdr)");
        }

        if (userHdr == nullptr) {
            if (dictionaryFirstEventBuffer->remaining() > 0) {
                userHeader = dictionaryFirstEventBuffer->array();
                userHeaderBuffer = dictionaryFirstEventBuffer;
                userHeaderLength = dictionaryFirstEventBuffer->remaining();
            }
            else {
                userHeader = nullptr;
                userHeaderBuffer = nullptr;
                userHeaderLength = 0;
            }
        }
        else if (len > 0) {
            // We're going to be placing the userHdr into a ByteBuffer. But ...
            // we should not turn userHdr into a shared pointer since we don't know if it
            // was created with "new" and it may catch the caller unaware. So ...
            // we'll allocate memory here and copy it in.
            userHeaderBuffer = std::make_shared<ByteBuffer>(len);
            userHeader = userHeaderBuffer->array();
            userHeaderBuffer->order(byteOrder);
            userHeaderLength = len;
            std::memcpy(userHeader, userHdr, len);
        }
        else if (dictionaryFirstEventBuffer->remaining() > 0) {
            userHeader = dictionaryFirstEventBuffer->array();
            userHeaderBuffer = dictionaryFirstEventBuffer;
            userHeaderLength = dictionaryFirstEventBuffer->remaining();
        }
        else {
            userHeader = nullptr;
            userHeaderBuffer = nullptr;
            userHeaderLength = 0;
        }

        buffer = buf;
        buffer->order(byteOrder);

        opened = true;
    }


    /**
     * Create a buffer representation of a record
     * containing the dictionary and/or the first event.
     * No compression.
     * @return buffer representation of record containing dictionary and/or first event,
     *         Null pointer if first event and dictionary don't exist.
     */
    std::shared_ptr<ByteBuffer> Writer::createDictionaryRecord() {
        return createRecord(dictionary, firstEvent, firstEventLength,
                            byteOrder, &fileHeader, nullptr);
    }


    /**
     * STATIC.
     * Create a buffer representation of a record
     * containing dictionary and/or first event.
     * No compression.
     *
     * @param dict      dictionary xml string
     * @param firstEv   bytes representing evio event
     * @param firstEvLen number of bytes in firstEv
     * @param order     byte order of returned byte array
     * @param fileHdr   file header to update with dictionary/first-event info (may be null).
     * @param recordHdr record header to update with dictionary info (may be null).
     * @return buffer representation of record containing dictionary and/or first event.
     *         Null pointer if both are empty/null.
     */
    std::shared_ptr<ByteBuffer> Writer::createRecord(const std::string & dict, uint8_t* firstEv, uint32_t firstEvLen,
                                                     const ByteOrder & order, FileHeader* fileHdr,
                                                     RecordHeader* recordHdr) {

        if (dict.empty() && firstEv == nullptr) {
            return nullptr;
        }

        // Create record.
        // Bit of chicken&egg problem, so start with default internal buf size.
        RecordOutput record(order, 2, 0, Compressor::UNCOMPRESSED);

        // How much data we got?
        size_t bytes=0;

        if (!dict.empty()) {
            bytes += dict.length();
        }

        if (firstEv != nullptr) {
//std::cout << "createRecord: add first event bytes " << firstEvLen << std::endl;
            bytes += firstEvLen;
        }

        // If we have huge dictionary/first event ...
        if (bytes > record.getInternalBufferCapacity()) {
            record = RecordOutput(order, 2, bytes, Compressor::UNCOMPRESSED);
        }

        // Add dictionary to record
        if (!dict.empty()) {
//std::cout << "createRecord: add dictionary to record, bytes " << dict.length() << std::endl;
            record.addEvent(reinterpret_cast<const uint8_t*>(&dict[0]), dict.length(), 0);
            // Also need to set bits in headers
            if (fileHdr   != nullptr)   fileHdr->hasDictionary(true);
            if (recordHdr != nullptr) recordHdr->hasDictionary(true);
        }

        // Add first event to file header but not record
        if (firstEv != nullptr) {
//std::cout << "createRecord: add first event to record" << std::endl;
            record.addEvent(firstEv, firstEvLen, 0);
            if (fileHdr   != nullptr)   fileHdr->hasFirstEvent(true);
            //if (recordHdr != nullptr) recordHdr->hasFirstEvent(true);
        }

        // Make events into record. Pos = 0, limit = # valid bytes.
        record.build();

        // Ready-to-read buffer contains record data
        return record.getBinaryBuffer();
    }


    /**
     * Create and return a buffer containing a general file header
     * followed by the user header given in the argument.
     * If user header is not padded to 4-byte boundary, it's done here.
     *
     * @param userHdr byte array containing a user-defined header, may be null.
     * @param userLen array length in bytes.
     * @return buffer (same as buf arg).
     * @throws EvioException if writing to buffer, not file.
     */
    std::shared_ptr<ByteBuffer> Writer::createHeader(uint8_t* userHdr, uint32_t userLen) {
        if (!toFile) {
            throw EvioException("call only if writing to file");
        }

        // Amount of user data in bytes
        int userHeaderBytes = 0;
        if (userHdr != nullptr) {
            userHeaderBytes = userLen;
        }
        // TODO: make sure next line is necessary (taken from WriterMT)
        fileHeader.reset();
        if (haveUserHeader) {
            fileHeader.setBitInfo(false, false, addTrailerIndex);
        }
        else {
            fileHeader.setBitInfo(haveFirstEvent, haveDictionary, addTrailerIndex);
        }
        fileHeader.setUserHeaderLength(userHeaderBytes);

        uint32_t totalLen = fileHeader.getLength();
        std::shared_ptr<ByteBuffer> buf = std::make_shared<ByteBuffer>(totalLen);
        buf->order(byteOrder);

        try {
            fileHeader.writeHeader(buf, 0);
        }
        catch (EvioException & e) {/* never happen */}

        if (userHeaderBytes > 0) {
            std::memcpy((void *)(buf->array() + FileHeader::HEADER_SIZE_BYTES),
                        (const void *)(userHdr), userHeaderBytes);
        }

        // Get ready to read, buffer position is still 0
        buf->limit(totalLen);
        return buf;
    }


    /**
     * Fill given buffer with a general file header followed by the given user header.
     * The buffer is cleared and set to desired byte order prior to writing.
     * If user header is not padded to 4-byte boundary, it's done here.
     *
     * @param buf buffer to contain the file header followed by the user-defined header.
     * @param userHdr byte array containing a user-defined header, may be null.
     * @param userLen array length in bytes.
     * @throws EvioException if writing to buffer, or buf too small
     *                       (needs to be userLen + FileHeader::HEADER_SIZE_BYTES bytes).
     */
    void Writer::createHeader(ByteBuffer & buf, uint8_t* userHdr, uint32_t userLen) {

        if (!toFile) {
            throw EvioException("call only if writing to file");
        }

        if (userLen + FileHeader::HEADER_SIZE_BYTES < buf.capacity()) {
            throw EvioException("buffer too small, need " +
                                        std::to_string(userLen + FileHeader::HEADER_SIZE_BYTES) + " bytes");
        }

        // Amount of user data in bytes
        int userHeaderBytes = 0;
        if (userHdr != nullptr) {
            userHeaderBytes = userLen;
        }
        fileHeader.reset();
        if (haveUserHeader) {
            fileHeader.setBitInfo(false, false, addTrailerIndex);
        }
        else {
            fileHeader.setBitInfo(haveFirstEvent, haveDictionary, addTrailerIndex);
        }
        fileHeader.setUserHeaderLength(userHeaderBytes);

        uint32_t totalLen = fileHeader.getLength();
        buf.clear();
        buf.order(byteOrder);

        try {
            fileHeader.writeHeader(buf, 0);
        }
        catch (EvioException & e) {/* never happen */}

        if (userHeaderBytes > 0) {
            std::memcpy((void *)(buf.array() + buf.arrayOffset() + FileHeader::HEADER_SIZE_BYTES),
                        (const void *)(userHdr), userHeaderBytes);
        }

        // Get ready to read, buffer position is still 0
        buf.limit(totalLen);
    }


    /**
     * Create and return a buffer containing a general file header
     * followed by the user header given in the argument.
     * If user header is not padded to 4-byte boundary, it's done here.
     * @param userHdr buffer containing a user-defined header which must be READY-TO-READ!
     * @return buffer containing a file header followed by the user-defined header.
     * @throws EvioException if writing to buffer, not file.
     */
    std::shared_ptr<ByteBuffer> Writer::createHeader(ByteBuffer & userHdr) {
        if (!toFile) {
            throw EvioException("call only if writing to file");
        }

        int userHeaderBytes = userHdr.remaining();
        fileHeader.reset();
        if (haveUserHeader) {
            fileHeader.setBitInfo(false, false, addTrailerIndex);
        }
        else {
            fileHeader.setBitInfo(haveFirstEvent, haveDictionary, addTrailerIndex);
        }
        fileHeader.setUserHeaderLength(userHeaderBytes);

        uint32_t totalLen = fileHeader.getLength();
        std::shared_ptr<ByteBuffer> buf = std::make_shared<ByteBuffer>(totalLen);
        buf->order(byteOrder);

        try {
            fileHeader.writeHeader(buf, 0);
        }
        catch (EvioException & e) {/* never happen */}

        if (userHeaderBytes > 0) {
            std::memcpy((void *)(buf->array() + buf->arrayOffset() + FileHeader::HEADER_SIZE_BYTES),
                        (const void *)(userHdr.array() + userHdr.arrayOffset()+ userHdr.position()),
                        userHeaderBytes);
        }

        // Get ready to read, buffer position is still 0
        buf->limit(totalLen);
        return buf;
    }


    /**
     * Fill given buffer (buf) with a general file header followed by the given user header (userHdr).
     * The buffer is cleared and set to desired byte order prior to writing.
     * If user header is not padded to 4-byte boundary, it's done here.
     *
     * @param buf buffer to contain the file header followed by the user-defined header.
     * @param userHdr buffer containing a user-defined header which must be READY-TO-READ!
     * @throws EvioException if writing to buffer, or buf too small
     *                       (needs to be userHdr.remaining() + FileHeader::HEADER_SIZE_BYTES bytes).
     */
    void Writer::createHeader(ByteBuffer & buf, ByteBuffer & userHdr) {
        if (!toFile) {
            throw EvioException("call only if writing to file");
        }

        int userHeaderBytes = userHdr.remaining();

        if (userHeaderBytes + FileHeader::HEADER_SIZE_BYTES < buf.capacity()) {
            throw EvioException("buffer too small, need " +
                                        std::to_string(userHeaderBytes + FileHeader::HEADER_SIZE_BYTES) + " bytes");
        }

        fileHeader.reset();
        if (haveUserHeader) {
            fileHeader.setBitInfo(false, false, addTrailerIndex);
        }
        else {
            fileHeader.setBitInfo(haveFirstEvent, haveDictionary, addTrailerIndex);
        }
        fileHeader.setUserHeaderLength(userHeaderBytes);

//std::cout << "createHeader: after set user header len, fe bit = " << fileHeader.hasFirstEvent() << std::endl;
        uint32_t totalLen = fileHeader.getLength();
        buf.clear();
        buf.order(byteOrder);

        try {
//std::cout << "createHeader: will write file header into buffer: hasFE = " << fileHeader.hasFirstEvent() << std::endl;
            fileHeader.writeHeader(buf, 0);
        }
        catch (EvioException & e) {/* never happen */}

        if (userHeaderBytes > 0) {
            std::memcpy((void *)(buf.array() + FileHeader::HEADER_SIZE_BYTES),
                        (const void *)(userHdr.array() + userHdr.arrayOffset()+ userHdr.position()),
                        userHeaderBytes);
        }

        // Get ready to read, buffer position is still 0
        buf.limit(totalLen);
    }


    /**
     * Write a general header as the last "header" or trailer
     * optionally followed by an index of all record lengths.
     * It's best <b>NOT</b> to call this directly. The way to write a trailer to
     * file or buffer is to call {@link #addTrailer(bool)} or
     * {@link #addTrailerWithIndex(bool)}. Then when {@link #close()} is
     * called, the trailer will be written.
     * @param writeIndex if true, write an index of all record lengths in trailer.
     * @param recordNum record number for trailing record.
     * @param trailerPos position in buffer to write trailer.
     * @throws EvioException if error writing to file.
     */
    void Writer::writeTrailer(bool writeIndex, uint32_t recordNum, uint64_t trailerPos) {

        // If we're NOT adding a record index, just write trailer
        if (!writeIndex) {
//std::cout << "    writeTrailer: write record header inito vector without index" << std::endl;
            RecordHeader::writeTrailer(headerArray, 0, recordNum, byteOrder, nullptr);

            // TODO: not really necessary to keep track here?
            writerBytesWritten += RecordHeader::HEADER_SIZE_BYTES;

            if (toFile) {
                outFile.write(reinterpret_cast<const char*>(&headerArray[0]), RecordHeader::HEADER_SIZE_BYTES);
                //outFile.write(reinterpret_cast<const char*>(headerArray.data()), RecordHeader::HEADER_SIZE_BYTES);
                if (outFile.fail()) {
                    throw EvioException("error writing file " + fileName);
                }
            }
            else {
//std::cout << "    writeTrailer: put trailer at buffer pos = " << buffer->position() <<
//             "?, or at trailerPos = " << trailerPos << std::endl;
                buffer->position(trailerPos);
                buffer->put(&headerArray[0], RecordHeader::HEADER_SIZE_BYTES);
                buffer->position(0);
            }
            return;
        }

        // Write trailer with index

        // How many bytes are we writing here?
        size_t dataBytes = RecordHeader::HEADER_SIZE_BYTES + 4*recordLengths->size();

        // Make sure our array can hold everything
        if (headerArray.capacity() < dataBytes) {
            headerArray.resize(dataBytes);
        }

        // Place data into headerArray - both header and index
        RecordHeader::writeTrailer(headerArray, 0, recordNum, byteOrder, recordLengths);

        // TODO: not really necessary to keep track here?
        writerBytesWritten += dataBytes;
        if (toFile) {
//std::cout << "    writeTrailer: write into file, " << dataBytes << " bytes" << std::endl;
            outFile.write(reinterpret_cast<const char*>(&headerArray[0]), dataBytes);
            //outFile.write(reinterpret_cast<const char*>(headerArray.data()), dataBytes);
            if (outFile.fail()) {
                throw EvioException("error opening file " + fileName);
            }
        }
        else {
            buffer->put(&headerArray[0], dataBytes);
        }
    }


    /**
     * Appends the record to the file/buffer. When writing to a file,
     * using this method in conjunction with addEvent() is <b>NOT</b> thread-safe.
     * For that reason, this write is done synchronously, to avoid interference with the normal,
     * asynchronous file writes. Thus, writing using this method will not
     * be quite as efficient.
     *
     * @param rec record object
     * @throws EvioException if error writing to file or
     *                       record's byte order is opposite to output endian.
     */
    void Writer::writeRecord(RecordOutput & rec) {

        if (rec.getByteOrder() != byteOrder) {
            throw EvioException("record byte order is wrong");
        }

        // If we have already written stuff into our current internal record,
        // write that first.
        if (outputRecord->getEventCount() > 0) {
//std::cout << "writeRecord: have already written stuff into internal record, write that first ..." << std::endl;
            writeOutput();
        }

        // Wait for previous (if any) write to finish
        if (toFile && future.valid()) {
            future.get();
            unusedRecord = beingWrittenRecord;

            if (outFile.fail()) {
                throw EvioException("problem writing to file");
            }
        }

        // Make sure given record is consistent with this writer
        auto header = rec.getHeader();
        header->setCompressionType(compressionType);
        header->setRecordNumber(recordNumber++);
        rec.build();

        int bytesToWrite = header->getLength();
        // Record length of this record
        recordLengths->push_back(bytesToWrite);
        // Followed by events in record
        recordLengths->push_back(header->getEntries());
        writerBytesWritten += bytesToWrite;
//std::cout << "writeRecord: bytes to write = " << bytesToWrite << std::endl;
//std::cout << "writeRecord: new record header = \n" << header->toString() << std::endl;

        if (toFile) {
            outFile.write(reinterpret_cast<const char *>(rec.getBinaryBuffer()->array()), bytesToWrite);
        }
        else {
            buffer->put(rec.getBinaryBuffer()->array(), bytesToWrite);
        }
    }

    // Use internal outputRecordStream to write individual events

    /**
     * Add a byte array to the internal record. If the length of
     * the buffer exceeds the maximum size of the record, the record
     * will be written to the file (compressed if the flag is set).
     * Internal record will be reset to receive new buffers.
     * Using this method in conjunction with writeRecord() is not thread-safe.
     * <b>The byte order of event's byte array must
     * match the byte order given in constructor!</b>
     *
     * @param buf buffer to add to the file.
     * @param length number of bytes to write from buffer.
     * @throws EvioException if cannot write to file.
     */
    void Writer::addEvent(uint8_t* buf, uint32_t length) {
        bool status = outputRecord->addEvent(buf, length, 0);
        if (!status){
            writeOutput();
            outputRecord->addEvent(buf, length, 0);
        }
    }


    /**
     * Add a ByteBuffer to the internal record. If the length of
     * the buffer exceeds the maximum size of the record, the record
     * will be written to the file (compressed if the flag is set).
     * Internal record will be reset to receive new buffers.
     * Using this method in conjunction with writeRecord() is not thread-safe.
     * <b>The byte order of event's data must
     * match the byte order given in constructor!</b>
     *
     * @param buf buffer to add to the file.
     * @throws EvioException if cannot write to file or buf arg's byte order is wrong.
     */
    void Writer::addEvent(std::shared_ptr<ByteBuffer> buf) {
        addEvent(*(buf.get()));
    }


    /**
     * Add a ByteBuffer to the internal record. If the length of
     * the buffer exceeds the maximum size of the record, the record
     * will be written to the file (compressed if the flag is set).
     * Internal record will be reset to receive new buffers.
     * Using this method in conjunction with writeRecord() is not thread-safe.
     * <b>The byte order of event's data must
     * match the byte order given in constructor!</b>
     *
     * @param buf buffer to add to the file.
     * @throws EvioException if cannot write to file or buf arg's byte order is wrong.
     */
    void Writer::addEvent(ByteBuffer & buf) {
        if (buf.order() != byteOrder) {
            throw EvioException("buf arg byte order is wrong");
        }

        bool status = outputRecord->addEvent(buf);
        if (!status) {
            writeOutput();
            outputRecord->addEvent(buf);
        }
    }


    /**
     * Add an EvioBank to the internal record. If the length of
     * the bank exceeds the maximum size of the record, the record
     * will be written to the file (compressed if the flag is set).
     * Internal record will be reset to receive new buffers.
     * Using this method in conjunction with writeRecord() is not thread-safe.
     *
     * @param bank event to add to the file.
     * @throws IOException if cannot write to file.
     */
    void Writer::addEvent(std::shared_ptr<EvioBank> bank) {
        bool status = outputRecord->addEvent(bank);
        if (!status) {
            writeOutput();
            outputRecord->addEvent(bank);
        }
    }


    /**
     * Add an EvioNode to the internal record. If the length of
     * the data exceeds the maximum size of the record, the record
     * will be written to the file (compressed if the flag is set).
     * Internal record will be reset to receive new buffers.
     * Using this method in conjunction with writeRecord() is not thread-safe.
     * <b>The byte order of node's data must
     * match the byte order given in constructor!</b>
     *
     * @param node node to add to the file.
     * @throws EvioException if node does not correspond to a bank.
     * @throws IOException if cannot write to file.
     */
    void Writer::addEvent(std::shared_ptr<EvioNode> node) {
        addEvent(*(node.get()));
    }


    /**
     * Add an EvioNode to the internal record. If the length of
     * the data exceeds the maximum size of the record, the record
     * will be written to the file (compressed if the flag is set).
     * Internal record will be reset to receive new buffers.
     * Using this method in conjunction with writeRecord() is not thread-safe.
     * <b>The byte order of node's data must
     * match the byte order given in constructor!</b>
     *
     * @param node node to add to the file.
     * @throws EvioException if node does not correspond to a bank.
     * @throws IOException if cannot write to file.
     */
    void Writer::addEvent(EvioNode & node) {
        bool status = outputRecord->addEvent(node);
        if (!status){
            writeOutput();
            outputRecord->addEvent(node);
        }
    }


    /**
     * Write internal record with incremented record # to file or buffer.
     * Not thread safe with {@link #writeRecord}.
     * @throws EvioException if cannot write to file.
     */
    void Writer::writeOutput() {
        if (!toFile) {
            writeOutputToBuffer();
            return;
        }

        // Wait for previous (if any) write to finish
        if (future.valid()) {
//std::cout << "  ***  writeOutput: Before future.get()" << std::endl;
            future.get();
//std::cout << "  ***  writeOutput: DONE with future" << std::endl;
            // Now that we're done writing record, make it available to be filled again
            unusedRecord = beingWrittenRecord;

            if (outFile.fail()) {
                throw EvioException("problem writing to file");
            }
        }
//        else {
//            std::cout << "  ***  writeOutput: No future to wait for" << std::endl;
//        }

        auto header = outputRecord->getHeader();

        header->setRecordNumber(recordNumber++);
        header->setCompressionType(compressionType);
        outputRecord->build();

        uint32_t bytesToWrite = header->getLength();
        uint32_t eventCount   = header->getEntries();
//std::cout << "  ***  writeOutput: header len = " << std::to_string(bytesToWrite) << std::endl;

        // Trailer's index has length followed by count
        recordLengths->push_back(bytesToWrite);
        recordLengths->push_back(eventCount);
        writerBytesWritten += bytesToWrite;

        // Launch async write in separate thread. That way the current thread can be filling
        // and compressing one record while another is simultaneously being written.
        // Unfortunately, unlike java which can do concurrent, thread-safe writes to a single file,
        // C++ cannot do so. This limits us to one writing thread.
//std::cout << "  ***  writeOutput: Launch future to write bytes = " << std::to_string(bytesToWrite) << std::endl;
        future = std::future<void>(
                std::async(std::launch::async,  // run in a separate thread
                           staticWriteFunction, // function to run
                           this,                // arguments to function ...
                           reinterpret_cast<const char *>(outputRecord->getBinaryBuffer()->array()),
                           bytesToWrite));

        // Keep track of which record is being written.
        beingWrittenRecord = outputRecord;

        // Next record to work with
        outputRecord = unusedRecord;
        outputRecord->reset();
    }


    /** Write internal record with incremented record # to buffer. */
    void Writer::writeOutputToBuffer() {
        auto header = outputRecord->getHeader();
        header->setRecordNumber(recordNumber++);
        header->setCompressionType(compressionType);

        // For writing to buffer, user header cannot be written
        // in file header since there is none. So write it into
        // first record header instead.
        if (!firstRecordWritten) {
            // This will take care of any unpadded user header dataƒ
//std::cout << "writeOutputToBUffer: build outputRecord with userHeaderBuffer" << std::endl;
            outputRecord->build(*(userHeaderBuffer.get()));
            firstRecordWritten = true;
        }
        else {
//std::cout << "writeOutputToBUffer: build outputRecord with NO userHeaderBuffer" << std::endl;
            outputRecord->build();
        }

        int bytesToWrite = header->getLength();
        // Record length of this record
        recordLengths->push_back(bytesToWrite);
        // Followed by events in record
        recordLengths->push_back(header->getEntries());
        writerBytesWritten += bytesToWrite;

//std::cout << "writeOutputToBUffer: write to buffer pos = " << (buffer->arrayOffset() + buffer->position()) <<
//             ", " << bytesToWrite << " bytes" << std::endl;
        std::memcpy((void *)(buffer->array() + buffer->arrayOffset() + buffer->position()),
                    (const void *)(outputRecord->getBinaryBuffer()->array()), bytesToWrite);

        buffer->position(buffer->position() + bytesToWrite);

        outputRecord->reset();
    }

    //---------------------------------------------------------------------

    /** Get this object ready for re-use.
     * Follow calling this with call to {@link #open(const std::string &)}. */
    void Writer::reset() {
        outputRecord->reset();
        fileHeader.reset();
        writerBytesWritten = 0L;
        recordNumber = 1;
        addingTrailer = false;
        firstRecordWritten = false;

        closed = false;
        opened = false;
    }


    /**
     * Close opened file. If the output record contains events,
     * they will be flushed to file/buffer. Trailer and its optional index
     * written if requested.
     * @throws IOException if error writing to file
     */
    void Writer::close() {
        if (closed) return;

        if (outputRecord->getEventCount() > 0) {
            // This will wait for previous asynchronous write to finish, but it also launches an async call
            writeOutput();
        }

        if (toFile) {
            // Wait for previous (if any) asynchronous write to finish
            if (future.valid()) {
//std::cout << "close: Found one, who do a future.get() ...." << std::endl;
                future.get();
//std::cout << "close: BACK from the call to future.get() ...." << std::endl;
            }
        }
//std::cout << "close: addingTrailer = " << addingTrailer << ", addTrailerINdex = " << addTrailerIndex << std::endl;

        uint32_t recordCount = recordNumber - 1;

        if (addingTrailer) {
            recordCount++;

            // Keep track of where we are right now which is just before trailer
            uint64_t trailerPosition = writerBytesWritten;

            // Write the trailer
//std::cout << "close: write trailer" << std::endl;
            writeTrailer(addTrailerIndex, recordCount, trailerPosition);

            if (toFile) {
                // Find & update file header's trailer position word
                if (byteOrder != ByteOrder::ENDIAN_LOCAL) {
                    trailerPosition = SWAP_64(trailerPosition);
                }

//std::cout << "close: call writeOutput(), seek file position = " << FileHeader::TRAILER_POSITION_OFFSET << std::endl;
                outFile.seekp(FileHeader::TRAILER_POSITION_OFFSET);
//std::cout << "close: call writeOutput(), write file (trailer pos), " <<  sizeof(uint64_t) << " bytes" << std::endl;
                outFile.write(reinterpret_cast<const char *>(&trailerPosition), sizeof(uint64_t));

                // Find & update file header's bit-info word
                if (addTrailerIndex) {
                    uint32_t bitInfo = fileHeader.hasTrailerWithIndex(true);
                    if (byteOrder != ByteOrder::ENDIAN_LOCAL) {
                        bitInfo = SWAP_32(bitInfo);
                    }
                    outFile.seekp(RecordHeader::BIT_INFO_OFFSET);
//std::cout << "close: call writeOutput(), write file (bitinfo), bytes = " <<  sizeof(uint32_t) << " bytes" << std::endl;
                    outFile.write(reinterpret_cast<const char *>(&bitInfo), sizeof(uint32_t));
                }
            }
        }

        if (toFile) {
            // Need to update the record count in file header
            if (byteOrder != ByteOrder::ENDIAN_LOCAL) {
                recordCount = SWAP_32(recordCount);
            }

            outFile.seekp(FileHeader::RECORD_COUNT_OFFSET);
            outFile.write(reinterpret_cast<const char *>(&recordCount), sizeof(uint32_t));

            outFile.close();
            recordLengths->clear();
        }
        else {
            // Get it ready for reading
            buffer->limit(writerBytesWritten).position(0);
        }

        closed = true;
        opened = false;
//std::cout << "close: bytes written " << writerBytesWritten << std::endl;
    }

}
