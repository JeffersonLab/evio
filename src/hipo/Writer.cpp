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
            Writer(ByteOrder::ENDIAN_LITTLE, 0, 0) {
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
                   string(""), nullptr, 0, Compressor::UNCOMPRESSED, false) {
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
    Writer::Writer(string & filename, const ByteOrder & order,
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
                   const string & dictionary, uint8_t* firstEvent, uint32_t firstEventLen,
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

        //    // Set as having no data
        //    userHeaderBuffer->limit(0);

        recordLengths = std::make_shared<std::vector<uint32_t>>();
        outputRecord = RecordOutput(order, maxEventCount, maxBufferSize, compType, hType);
        headerArray.reserve(RecordHeader::HEADER_SIZE_BYTES);

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
            dictionaryFirstEventBuffer->limit(0);
        }
    }


    //////////////////////////////////////////////////////////////////////


    /**
     * Constructor for writing to a ByteBuffer. Byte order is taken from the buffer.
     * No compression.
     * @param buf buffer in to which to write events and/or records.
     */
    Writer::Writer(std::shared_ptr<ByteBuffer> & buf) :
            Writer(buf, 0, 0, "", nullptr, 0) {
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
    Writer::Writer(std::shared_ptr<ByteBuffer> & buf, uint32_t maxEventCount, uint32_t maxBufferSize,
                   const string & dictionary, uint8_t* firstEvent, uint32_t firstEventLen) {

        byteOrder = buf->order();
        buffer = buf;

        this->dictionary = dictionary;
        this->firstEvent = firstEvent;
        firstEventLength = firstEventLen;

        recordLengths = std::make_shared<std::vector<uint32_t>>();
        headerArray.reserve(RecordHeader::HEADER_SIZE_BYTES);
        outputRecord = RecordOutput(byteOrder, maxEventCount, maxBufferSize, Compressor::UNCOMPRESSED);

        haveDictionary = !dictionary.empty();
        haveFirstEvent = (firstEvent != nullptr && firstEventLen > 0);

        if (haveDictionary || haveFirstEvent)  {
            dictionaryFirstEventBuffer = createDictionaryRecord();
            // make this the user header by default since open() may not get called for buffers
            // TODO: SHOULD NOT AVOID the shared pointer!!!!! Look at userHeader uses!!!
            userHeader = dictionaryFirstEventBuffer->array();
            userHeaderLength = dictionaryFirstEventBuffer->remaining();
            userHeaderBuffer = dictionaryFirstEventBuffer;
        }
        else {
            //        // Set these as having no data
            //        userHeaderBuffer->limit(0);
            dictionaryFirstEventBuffer->limit(0);
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

            outputRecord = other.outputRecord;
            unusedRecord = other.unusedRecord;
            beingWrittenRecord = other.beingWrittenRecord;

            headerArray   = other.headerArray;
            // Copy over the vector
            auto recLenVector = *recordLengths.get();
            recLenVector = *(other.recordLengths.get());

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
    //RecordHeader & Writer::getRecordHeader() {return outputRecord.getHeader();}
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
     * @return this object
     */
    void Writer::setCompressionType(Compressor::CompressionType compression) {
        if (toFile) {
            compressionType = compression;
            outputRecord.getHeader()->setCompressionType(compression);
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
    void Writer::open(string & filename) {
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
     * @throws EvioException filename arg is null, if constructor specified writing to a buffer,
     *                       if open() was already called without being followed by reset(),
     *                       if file cannot be found, if IO error writing to file,
     *                       or if filename is empty.
     */
    void Writer::open(string & filename, uint8_t* userHdr, uint32_t userLen) {

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
            cout << "writer::open: given a valid user header to write" << endl;
            fileHeaderBuffer = createHeader(userHdr, userLen);
        }
        else {
            // If dictionary & firstEvent not defined and user header not given ...
            if (dictionaryFirstEventBuffer->remaining() < 1) {
                cout << "writer::open: given a null user header to write, userLen = " << userLen <<  endl;
                fileHeaderBuffer = createHeader(nullptr, 0);
            }
                // else place dictionary and/or firstEvent into
                // record which becomes user header
            else {
                cout << "writer::open: given a valid dict/first ev header to write" << endl;
                fileHeaderBuffer = createHeader(*(dictionaryFirstEventBuffer.get()));
            }
        }

        // Write this to file
        fileName = filename;
        // TODO: what flags??? instead of "rw"
        outFile.open(filename, ios::binary);
        outFile.write(reinterpret_cast<const char*>(fileHeaderBuffer->array()), fileHeaderBuffer->remaining());
        if (outFile.fail()) {
            throw EvioException("error opening file " + filename);
        }

        writerBytesWritten = (size_t) fileHeader.getLength();
        //cout << "open: wrote " << writerBytesWritten << " bytes to file for file header only " << endl;
        opened = true;
    }


    /**
     * Specify a buffer and write first record header with given user header.
     * User header is automatically padded when written.
     * @param buf        buffer to writer to.
     * @param userHdr    byte array representing the optional user's header.
     *                   <b>Warning: this will not be used until first record is written!
     *                   So don't go changing it in the meantime!</b>
     *                   If this is null AND dictionary and/or first event are given,
     *                   the dictionary and/or first event will be placed in its
     *                   own record and written as the user header of the first record's
     *                   header.
     * @param len        length of valid data (bytes) in userHdr (starting at off).
     * @throws EvioException if constructor specified writing to a file,
     *                       or if open() was already called without being followed by reset().
     */
    void Writer::open(std::shared_ptr<ByteBuffer> & buf, uint8_t* userHdr, uint32_t len) {

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
                userHeaderBuffer->clear();
                userHeaderLength = 0;
            }
        }
        else if (len > 0) {
            userHeader = userHdr;
            userHeaderBuffer = std::make_shared<ByteBuffer>(userHdr, len);
            userHeaderBuffer->order(byteOrder);
            userHeaderLength = len;
        }
        else if (dictionaryFirstEventBuffer->remaining() > 0) {
            userHeader = dictionaryFirstEventBuffer->array();
            userHeaderBuffer = dictionaryFirstEventBuffer;
            userHeaderLength = dictionaryFirstEventBuffer->remaining();
        }
        else {
            userHeader = nullptr;
            userHeaderBuffer->clear();
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
     * @param recordHdr record header to update with dictionary/first-event info (may be null).
     * @return buffer representation of record containing dictionary and/or first event.
     *         Null pointer if both are empty/null.
     */
    std::shared_ptr<ByteBuffer> Writer::createRecord(const string & dict, uint8_t* firstEv, uint32_t firstEvLen,
                                                     const ByteOrder & order, FileHeader* fileHdr,
                                                     RecordHeader* recordHdr) {

        if (dict.empty() && firstEv == nullptr) {
            return nullptr;
        }

        // Create record.
        // Bit of chicken&egg problem, so start with default internal buf size.
        RecordOutput record(order, 2, 0, Compressor::UNCOMPRESSED);

        // How much data we got?
        int bytes=0;

        if (!dict.empty()) {
            bytes += dict.length();
        }

        if (firstEv != nullptr) {
            cout << "createRecord: add first event bytes " << firstEvLen << endl;
            bytes += firstEvLen;
        }

        // If we have huge dictionary/first event ...
        if (bytes > record.getInternalBufferCapacity()) {
            record = RecordOutput(order, 2, bytes, Compressor::UNCOMPRESSED);
        }

        // Add dictionary to record
        if (!dict.empty()) {
            record.addEvent(reinterpret_cast<const uint8_t*>(&dict[0]), 0, dict.length());
            // Also need to set bits in headers
            if (fileHdr   != nullptr)   fileHdr->hasDictionary(true);
            if (recordHdr != nullptr) recordHdr->hasDictionary(true);
        }

        // Add first event to record
        if (firstEv != nullptr) {
            cout << "createRecord: add first event to record" << endl;
            record.addEvent(firstEv, 0, firstEvLen);
            if (fileHdr   != nullptr)   fileHdr->hasFirstEvent(true);
            if (recordHdr != nullptr) recordHdr->hasFirstEvent(true);
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

        //cout << "createHeader: IN, fe bit = " << fileHeader.hasFirstEvent() << endl;

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

        //cout << "createHeader: after set user header len, fe bit = " << fileHeader.hasFirstEvent() << endl;
        uint32_t totalLen = fileHeader.getLength();
        std::shared_ptr<ByteBuffer> buf = std::make_shared<ByteBuffer>(totalLen);
        buf->order(byteOrder);

        try {
            //cout << "createHeader: will write file header into buffer: hasFE = " << fileHeader.hasFirstEvent() << endl;
            fileHeader.writeHeader(buf, 0);
        }
        catch (EvioException & e) {/* never happen */}

        if (userHeaderBytes > 0) {
            std::memcpy((void *)(buf->array() + FileHeader::HEADER_SIZE_BYTES),
                        (const void *)(userHdr), userHeaderBytes);
        }

        // Get ready to read, buffer position is still 0
        buf->limit(totalLen);
        return std::move(buf);
    }


    /**
     * Fill given buffer with a general file header followed by the given user header.
     * The buffer is cleared and set to desired byte order prior to writing.
     * If user header is not padded to 4-byte boundary, it's done here.
     *
     * @param userHdr byte array containing a user-defined header, may be null.
     * @param userLen array length in bytes.
     * @return buffer (same as buf arg).
     * @throws EvioException if writing to buffer, or buf too small
     *                       (needs to be userLen + FileHeader::HEADER_SIZE_BYTES bytes).
     */
    void Writer::createHeader(ByteBuffer & buf, uint8_t* userHdr, uint32_t userLen) {

        if (!toFile) {
            throw EvioException("call only if writing to file");
        }

        if (userLen + FileHeader::HEADER_SIZE_BYTES < buf.capacity()) {
            throw EvioException("buffer too small, need " +
                                to_string(userLen + FileHeader::HEADER_SIZE_BYTES) + " bytes");
        }

        //cout << "createHeader: IN, fe bit = " << fileHeader.hasFirstEvent() << endl;

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

        //cout << "createHeader: after set user header len, fe bit = " << fileHeader.hasFirstEvent() << endl;
        uint32_t totalLen = fileHeader.getLength();
        buf.clear();
        buf.order(byteOrder);

        try {
            //cout << "createHeader: will write file header into buffer: hasFE = " << fileHeader.hasFirstEvent() << endl;
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
        //cout << "createHeader: IN, fe bit = " << fileHeader.hasFirstEvent() << endl;

        int userHeaderBytes = userHdr.remaining();
        fileHeader.reset();
        if (haveUserHeader) {
            fileHeader.setBitInfo(false, false, addTrailerIndex);
        }
        else {
            fileHeader.setBitInfo(haveFirstEvent, haveDictionary, addTrailerIndex);
        }
        fileHeader.setUserHeaderLength(userHeaderBytes);

        //cout << "createHeader: after set user header len, fe bit = " << fileHeader.hasFirstEvent() << endl;
        uint32_t totalLen = fileHeader.getLength();
        std::shared_ptr<ByteBuffer> buf = std::make_shared<ByteBuffer>(totalLen);
        buf->order(byteOrder);

        try {
            //cout << "createHeader: will write file header into buffer: hasFE = " << fileHeader.hasFirstEvent() << endl;
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
        return std::move(buf);
    }


    /**
     * Fill given buffer (buf) with a general file header followed by the given user header (userHdr).
     * The buffer is cleared and set to desired byte order prior to writing.
     * If user header is not padded to 4-byte boundary, it's done here.
     *
     * @param userHdr buffer containing a user-defined header which must be READY-TO-READ!
     * @return buffer containing a file header followed by the user-defined header.
     * @throws EvioException if writing to buffer, or buf too small
     *                       (needs to be userHdr.remaining() + FileHeader::HEADER_SIZE_BYTES bytes).
     */
    void Writer::createHeader(ByteBuffer & buf, ByteBuffer & userHdr) {
        if (!toFile) {
            throw EvioException("call only if writing to file");
        }
        //cout << "createHeader: IN, fe bit = " << fileHeader.hasFirstEvent() << endl;

        int userHeaderBytes = userHdr.remaining();

        if (userHeaderBytes + FileHeader::HEADER_SIZE_BYTES < buf.capacity()) {
            throw EvioException("buffer too small, need " +
                                to_string(userHeaderBytes + FileHeader::HEADER_SIZE_BYTES) + " bytes");
        }

        fileHeader.reset();
        if (haveUserHeader) {
            fileHeader.setBitInfo(false, false, addTrailerIndex);
        }
        else {
            fileHeader.setBitInfo(haveFirstEvent, haveDictionary, addTrailerIndex);
        }
        fileHeader.setUserHeaderLength(userHeaderBytes);

        //cout << "createHeader: after set user header len, fe bit = " << fileHeader.hasFirstEvent() << endl;
        uint32_t totalLen = fileHeader.getLength();
        buf.clear();
        buf.order(byteOrder);

        try {
            //cout << "createHeader: will write file header into buffer: hasFE = " << fileHeader.hasFirstEvent() << endl;
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
     * @throws EvioException if error writing to file.
     */
    void Writer::writeTrailer(bool writeIndex, uint32_t recordNum) {

        // If we're NOT adding a record index, just write trailer
        if (!writeIndex) {
            RecordHeader::writeTrailer(headerArray, 0, recordNum, byteOrder, nullptr);

            // TODO: not really necessary to keep track here?
            writerBytesWritten += RecordHeader::HEADER_SIZE_BYTES;

            if (toFile) {
                outFile.write(reinterpret_cast<const char*>(&headerArray[0]), RecordHeader::HEADER_SIZE_BYTES);
                if (outFile.fail()) {
                    throw EvioException("error writing file " + fileName);
                }
            }
            else {
                buffer->put(&headerArray[0], RecordHeader::HEADER_SIZE_BYTES);
            }
            return;
        }

        // Write trailer with index

        // How many bytes are we writing here?
        int dataBytes = RecordHeader::HEADER_SIZE_BYTES + 4*recordLengths->size();

        // Make sure our array can hold everything
        if (headerArray.capacity() < dataBytes) {
            //cout << "Allocating byte array of " << dataBytes << " bytes in size" << endl;
            headerArray.reserve(dataBytes);
        }

        // Place data into headerArray - both header and index
        RecordHeader::writeTrailer(headerArray, 0, recordNum, byteOrder, recordLengths);

        // TODO: not really necessary to keep track here?
        writerBytesWritten += dataBytes;
        if (toFile) {
            outFile.write(reinterpret_cast<const char*>(&headerArray[0]), dataBytes);
            if (outFile.fail()) {
                throw EvioException("error opening file " + fileName);
            }
        }
        else {
            buffer->put(&headerArray[0], dataBytes);
        }
    }


    /**
     * Appends the record to the file/buffer.
     * Using this method in conjunction with addEvent() is not thread-safe.
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
        if (outputRecord.getEventCount() > 0) {
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
        auto & header = rec.getHeader();
        header->setCompressionType(compressionType);
        header->setRecordNumber(recordNumber++);
        rec.build();

        int bytesToWrite = header->getLength();
        // Record length of this record
        recordLengths->push_back(bytesToWrite);
        // Followed by events in record
        recordLengths->push_back(header->getEntries());
        writerBytesWritten += bytesToWrite;

        if (toFile) {
            // Launch async write in separate thread.
            future = std::future<void>(
                    std::async(std::launch::async,  // run in a separate thread
                               staticWriteFunction, // function to run
                               this,                // arguments to function ...
                               reinterpret_cast<const char *>(rec.getBinaryBuffer()->array()),
                               bytesToWrite));

            // Next record to work with
            outputRecord = unusedRecord;
            outputRecord.reset();
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
     * @param buffer array to add to the file.
     * @param offset offset into array from which to start writing data.
     * @param length number of bytes to write from array.
     * @throws EvioException if cannot write to file.
     */
    void Writer::addEvent(uint8_t* buf, uint32_t offset, uint32_t length) {
        bool status = outputRecord.addEvent(buf, offset, length);
        if (!status){
            writeOutput();
            outputRecord.addEvent(buf, offset, length);
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
     * @param buffer array to add to the file.
     * @throws EvioException if cannot write to file or buf arg's byte order is wrong.
     */
    void Writer::addEvent(std::shared_ptr<ByteBuffer> & buf) {
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
     * @param buffer array to add to the file.
     * @throws EvioException if cannot write to file or buf arg's byte order is wrong.
     */
    void Writer::addEvent(ByteBuffer & buf) {
        if (buffer->order() != byteOrder) {
            throw EvioException("buffer arg byte order is wrong");
        }

        bool status = outputRecord.addEvent(buf);
        if (!status) {
            writeOutput();
            outputRecord.addEvent(buf);
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
    void Writer::addEvent(std::shared_ptr<EvioBank> & bank) {
        bool status = outputRecord.addEvent(bank);
        if (!status) {
            writeOutput();
            outputRecord.addEvent(bank);
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
    void Writer::addEvent(std::shared_ptr<EvioNode> & node) {
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
        bool status = outputRecord.addEvent(node);
        if (!status){
            writeOutput();
            outputRecord.addEvent(node);
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

        //cout << "writeOutput: IN" << endl;

        // Wait for previous (if any) write to finish
        if (future.valid()) {
            //cout << "writeOutput: Before future.get()" << endl;
            future.get();
            //cout << "writeOutput: DOne with future" << endl;
            // Now that we're done writing record, make it available to be filled again
            unusedRecord = beingWrittenRecord;

            if (outFile.fail()) {
                throw EvioException("problem writing to file");
            }
        }
        //    else {
        //        cout << "writeOutput: No future to wait for" << endl;
        //    }

        // TODO: This header is reference !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        auto & header = outputRecord.getHeader();

        header->setRecordNumber(recordNumber++);
        header->setCompressionType(compressionType);
        outputRecord.build();

        uint32_t bytesToWrite = header->getLength();
        uint32_t eventCount   = header->getEntries();
        //cout << "writeOutput: header len = " << to_string(bytesToWrite) << endl;

        // Trailer's index has length followed by count
        recordLengths->push_back(bytesToWrite);
        recordLengths->push_back(eventCount);
        writerBytesWritten += bytesToWrite;

        // Launch async write in separate thread. That way the current thread can be filling
        // and compressing one record while another is simultaneously being written.
        // Unfortunately, unlike java which can do concurrent, thread-safe writes to a single file,
        // C++ cannot do so. This limits us to one writing thread.
        //cout << "writeOutput: Launch future to write bytes = " << to_string(bytesToWrite) << endl;
        future = std::future<void>(
                std::async(std::launch::async,  // run in a separate thread
                           staticWriteFunction, // function to run
                           this,                // arguments to function ...
                           reinterpret_cast<const char *>(outputRecord.getBinaryBuffer()->array()),
                           bytesToWrite));

        // Keep track of which record is being written
        beingWrittenRecord = outputRecord;

        // Next record to work with
        outputRecord = unusedRecord;
        outputRecord.reset();
    }


    /** Write internal record with incremented record # to buffer. */
    void Writer::writeOutputToBuffer() {
        auto & header = outputRecord.getHeader();
        header->setRecordNumber(recordNumber++);
        header->setCompressionType(compressionType);

        // For writing to buffer, user header cannot be written
        // in file header since there is none. So write it into
        // first record header instead.
        if (!firstRecordWritten) {
            // This will take care of any unpadded user header data
            outputRecord.build(*(userHeaderBuffer.get()));
            firstRecordWritten = true;
        }
        else {
            outputRecord.build();
        }

        int bytesToWrite = header->getLength();
        // Record length of this record
        recordLengths->push_back(bytesToWrite);
        // Followed by events in record
        recordLengths->push_back(header->getEntries());
        writerBytesWritten += bytesToWrite;

        std::memcpy((void *)(buffer->array() + buffer->arrayOffset() + buffer->position()),
                    (const void *)(outputRecord.getBinaryBuffer()->array()), bytesToWrite);

        outputRecord.reset();
    }

    //---------------------------------------------------------------------

    /** Get this object ready for re-use.
     * Follow calling this with call to {@link #open(String)}. */
    void Writer::reset() {
        outputRecord.reset();
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
        //cout << "close: IN" << endl;
        if (closed) return;

        if (outputRecord.getEventCount() > 0) {
            // This will wait for previous asynchronous write to finish
            //cout << "close: call writeOutput(), output record's ev count = " << outputRecord.getEventCount() << endl;
            writeOutput();
        }
        else if (toFile) {
            // Wait for previous (if any) asynchronous write to finish
            if (future.valid()) {
                future.get();
            }
        }

        uint32_t recordCount = recordNumber - 1;

        if (addingTrailer) {
            recordCount++;

            // Keep track of where we are right now which is just before trailer
            uint64_t trailerPosition = writerBytesWritten;

            // Write the trailer
            writeTrailer(addTrailerIndex, recordCount);

            if (toFile) {
                // Find & update file header's trailer position word
                if (byteOrder != ByteOrder::ENDIAN_LOCAL) {
                    trailerPosition = SWAP_64(trailerPosition);
                }
                outFile.seekp(FileHeader::TRAILER_POSITION_OFFSET);
                outFile.write(reinterpret_cast<const char *>(&trailerPosition), sizeof(uint64_t));

                // Find & update file header's bit-info word
                if (addTrailerIndex) {
                    uint32_t bitInfo = fileHeader.hasTrailerWithIndex(true);
                    if (byteOrder != ByteOrder::ENDIAN_LOCAL) {
                        bitInfo = SWAP_32(bitInfo);
                    }
                    outFile.seekp(RecordHeader::BIT_INFO_OFFSET);
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

        closed = true;
        opened = false;
        //cout << "[writer] ---> bytes written " << writerBytesWritten << endl;
    }

}
