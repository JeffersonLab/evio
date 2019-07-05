//
// Created by Carl Timmer on 2019-05-06.
//

#include "Writer.h"


/**
 * Default constructor.
 * <b>No</b> file is opened. Any file will have little endian byte order.
 */
Writer::Writer() {
    outputRecord = RecordOutput();
    fileHeader   = FileHeader(true);
    recordLengths.reserve(1500);
    headerArray.reserve(RecordHeader::HEADER_SIZE_BYTES);
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
        Writer(HeaderType::EVIO_FILE, order, maxEventCount, maxBufferSize) {
}

/**
 * Constructor with byte order and header type. <b>No</b> file is opened.
 *
 * @param hType the type of the file. If set to {@link HeaderType#HIPO_FILE},
 *              the header will be written with the first 4 bytes set to HIPO.
 * @param order byte order of written file.
 * @param maxEventCount max number of events a record can hold.
 *                      Value of O means use default (1M).
 * @param maxBufferSize max number of uncompressed data bytes a record can hold.
 *                      Value of < 8MB results in default of 8MB.
 */
Writer::Writer(const HeaderType & hType, const ByteOrder & order,
               uint32_t maxEventCount, uint32_t maxBufferSize) :
        Writer(hType, order, maxEventCount, maxBufferSize, "", nullptr, 0) {
}

/**
 * Constructor with byte order. The given file is opened so any subsequent call to open will fail.
 * This method places the dictionary and first event into the file header's user header.
 *
 * @param hType         the type of the file. If set to {@link HeaderType#HIPO_FILE},
 *                      the header will be written with the first 4 bytes set to HIPO.
 * @param order         byte order of written file.
 * @param maxEventCount max number of events a record can hold.
 *                      Value of O means use default (1M).
 * @param maxBufferSize max number of uncompressed data bytes a record can hold.
 *                      Value of < 8MB results in default of 8MB.
 * @param dictionary    string holding an evio format dictionary to be placed in userHeader.
 * @param firstEvent    byte array containing an evio event to be included in userHeader.
 *                      It must be in the same byte order as the order argument.
 * @param firstEventLen number of bytes in firstEvent.
 */
Writer::Writer(const HeaderType & hType, const ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize,
               const string & dictionary, uint8_t* firstEvent, uint32_t firstEventLen) {

    this->dictionary = dictionary;
    this->firstEvent = firstEvent;
    firstEventLength = firstEventLen;

    byteOrder = order;
    compressionType = Compressor::LZ4;

    userHeader = nullptr;
    userHeaderLength = 0;
    writerBytesWritten = 0;

    closed             = false;
    opened             = false;
    addingTrailer      = false;
    addTrailerIndex    = false;
    firstRecordWritten = false;

    recordLengths.reserve(1500);
    headerArray.reserve(RecordHeader::HEADER_SIZE_BYTES);
    outputRecord = RecordOutput(order, maxEventCount, maxBufferSize, compressionType);

    if ((dictionary.length() > 0) ||
        (firstEvent != nullptr && firstEventLen > 0))  {
        dictionaryFirstEventBuffer = createDictionaryRecord();
    }

    if (hType == HeaderType::HIPO_FILE) {
        fileHeader = FileHeader(false);
    } else {
        fileHeader = FileHeader(true);
    }
}

/**
 * Constructor with filename.
 * The output file will be created with no user header.
 * File byte order is little endian.
 * @param filename output file name
 * @throws IOException  if file cannot be found or IO error writing to file
 */
Writer::Writer(string & filename) : Writer() {
    headerArray.reserve(RecordHeader::HEADER_SIZE_BYTES);
    try {
        open(filename);
    }
    catch (HipoException & e) {/* never happen */}
}

/**
 * Constructor with filename & byte order.
 * The output file will be created with no user header. LZ4 compression.
 *
 * @param filename      output file name
 * @param order         byte order of written file;
 * @param maxEventCount max number of events a record can hold.
 *                      Value of O means use default (1M).
 * @param maxBufferSize max number of uncompressed data bytes a record can hold.
 *                      Value of < 8MB results in default of 8MB.
 * @throws HipoException  if file cannot be found or IO error writing to file
 */
Writer::Writer(string & filename, const ByteOrder & order,
               uint32_t maxEventCount, uint32_t maxBufferSize) :
        Writer(order, maxEventCount, maxBufferSize) {

    headerArray.reserve(RecordHeader::HEADER_SIZE_BYTES);
    outFile.open(filename, std::ios::binary);
    if (outFile.bad() || outFile.fail()) {
        std::cout << "End of file reached successfully\n";
        throw HipoException("error opening file " + filename);
    }
}

//////////////////////////////////////////////////////////////////////

/**
 * Constructor for writing to a ByteBuffer. Byte order is taken from the buffer.
 * No compression.
 * @param buf buffer in to which to write events and/or records.
 */
Writer::Writer(ByteBuffer & buf) :
    Writer(buf, buf.order(), 0, 0, "", nullptr, 0) {
}

/**
 * Constructor with byte order.
 * This method places the dictionary and first event into the file header's user header.
 * No compression.
 *
 * @param buf           buffer in to which to write events and/or records.
 * @param order         byte order of data to be written. Little endian if null.
 * @param maxEventCount max number of events a record can hold.
 *                      Value of O means use default (1M).
 * @param maxBufferSize max number of uncompressed data bytes a record can hold.
 *                      Value of < 8MB results in default of 8MB.
 * @param dictionary    string holding an evio format dictionary to be placed in userHeader.
 * @param firstEvent    byte array containing an evio event to be included in userHeader.
 *                      It must be in the same byte order as the order argument.
 * @param firstEventLen number of bytes in firstEvent.
 */
Writer::Writer(ByteBuffer & buf, const ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize,
               const string & dictionary, uint8_t* firstEvent, uint32_t firstEventLen) {

    buffer = buf;
    buf.order(byteOrder);

    this->dictionary = dictionary;
    this->firstEvent = firstEvent;
    this->firstEventLength = firstEventLen;

    headerArray.reserve(RecordHeader::HEADER_SIZE_BYTES);
    outputRecord = RecordOutput(order, maxEventCount, maxBufferSize, Compressor::UNCOMPRESSED);

    if ( (dictionary.length() > 0) ||
         (firstEvent != nullptr && firstEventLen > 0))  {

        dictionaryFirstEventBuffer = createDictionaryRecord();
        // make this the user header by default since open() may not get called for buffers
// TODO: SHOULD NOT AVOID the shared pointer!!!!! Look at userHeader uses!!!
        userHeader = dictionaryFirstEventBuffer.array();
        userHeaderLength = dictionaryFirstEventBuffer.remaining();
        userHeaderBuffer = dictionaryFirstEventBuffer;
    }

    toFile = false;
}

//////////////////////////////////////////////////////////////////////
// PRIVATE

/**
 * Create a buffer representation of a record
 * containing the dictionary and/or the first event.
 * No compression.
 * @return buffer representation of record containing dictionary and/or first event,
 *         of zero size if first event and dictionary don't exist.
 */
ByteBuffer Writer::createDictionaryRecord() {
    return createRecord(dictionary, firstEvent, firstEventLength, byteOrder, &fileHeader, nullptr);
}


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
 * Write internal record with incremented record # to file or buffer.
 * Not thread safe with {@link #writeRecord}.
 * @throws HipoException if cannot write to file.
 */
void Writer::writeOutput() {

    if (!toFile) {
        writeOutputToBuffer();
        return;
    }

    // Wait for previous (if any) write to finish
    if (future.valid()) {
        future.get();
        // Now that we're done writing record, make it available to be filled again
        unusedRecord = beingWrittenRecord;

        if (outFile.fail()) {
            throw HipoException("problem writing to file");
        }
    }

    RecordHeader header = outputRecord.getHeader();
    header.setRecordNumber(recordNumber++);
    header.setCompressionType(compressionType);
    outputRecord.build();
    uint32_t bytesToWrite = header.getLength();

    // Record length of this record
    recordLengths.push_back(bytesToWrite);
    // Followed by events in record
    recordLengths.push_back(header.getEntries());

    writerBytesWritten += bytesToWrite;

    // Launch async write in separate thread. That way the current thread can be filling
    // and compressing one record while another is simultaneously being written.
    // Unfortunately, unlike java which can do concurrent, thread-safe writes to a single file,
    // C++ cannot do so. This limits us to one writing thread.
    future = std::future<void>(
                   std::async(std::launch::async,  // run in a separate thread
                              staticWriteFunction, // function to run
                              this,                // arguments to function ...
                              reinterpret_cast<const char *>(outputRecord.getBinaryBuffer().array()),
                              bytesToWrite));

    // Keep track of which record is being written
    beingWrittenRecord = outputRecord;

    // Next record to work with
    outputRecord = unusedRecord;
    outputRecord.reset();
}


/** Write internal record with incremented record # to buffer. */
void Writer::writeOutputToBuffer() {
    RecordHeader header = outputRecord.getHeader();
    header.setRecordNumber(recordNumber++);
    header.setCompressionType(compressionType);

    // For writing to buffer, user header cannot be written
    // in file header since there is none. So write it into
    // first record header instead.
    if (!firstRecordWritten) {
        // This will take care of any unpadded user header data
        outputRecord.build(userHeaderBuffer);
        firstRecordWritten = true;
    }
    else {
        outputRecord.build();
    }

    int bytesToWrite = header.getLength();
    // Record length of this record
    recordLengths.push_back(bytesToWrite);
    // Followed by events in record
    recordLengths.push_back(header.getEntries());
    writerBytesWritten += bytesToWrite;

    std::memcpy((void *)(buffer.array() + buffer.position()),
                (const void *)(outputRecord.getBinaryBuffer().array()), bytesToWrite);

//    System.arraycopy(outputRecord.getBinaryBuffer().array(), 0,
//                     buffer.array(), buffer.arrayOffset() + buffer.position(),
//                     bytesToWrite);

    outputRecord.reset();
}

//////////////////////////////////////////////////////////////////////

/**
 * Get the buffer being written to.
 * @return buffer being written to.
 */
ByteBuffer & Writer::getBuffer() {return buffer;}

/**
 * Get the file's byte order.
 * @return file's byte order.
 */
const ByteOrder &  Writer::getByteOrder() const {return byteOrder;}

/**
 * Get the file header.
 * @return file header.
 */
FileHeader & Writer::getFileHeader() {return fileHeader;}

/**
 * Get the internal record's header.
 * @return internal record's header.
 */
RecordHeader & Writer::getRecordHeader() {return outputRecord.getHeader();}

/**
 * Get the internal record used to add events to file.
 * @return internal record used to add events to file.
 */
RecordOutput & Writer::getRecord() {return outputRecord;}

/**
 * Convenience method that gets compression type for the file being written.
 * @return compression type for the file being written.
 */
uint32_t Writer::getCompressionType() {return outputRecord.getHeader().getCompressionType();}

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
 * @throws HipoException if open already called without being followed by calling close.
 * @throws IOException if file cannot be found or IO error writing to file
 */
void Writer::open(string & filename) {
    outFile.open(filename, ios::binary);
}

/**
 * Open a file and write file header with given user header.
 * User header is automatically padded when written.
 * @param filename   name of file to write to.
 * @param userHdr    byte array representing the optional user's header.
 *                   If this is null AND dictionary and/or first event are given,
 *                   the dictionary and/or first event will be placed in its
 *                   own record and written as the user header.
 * @param userLen   length of userHdr in bytes.
 * @throws HipoException filename arg is null, if constructor specified writing to a buffer,
 *                       or if open() was already called without being followed by reset().
 * @throws IOException   if file cannot be found or IO error writing to file
 */
void Writer::open(string & filename, uint8_t* userHdr, uint32_t userLen) {

    if (opened) {
        throw HipoException("currently open, call reset() first");
    }
    else if (!toFile) {
        throw HipoException("can only write to a buffer, call open(buffer, userHdr, userLen)");
    }

    ByteBuffer headerBuffer;

    // User header given as arg has precedent
    if (userHdr != nullptr) {
        headerBuffer = createHeader(userHdr, userLen);
    }
    else {
        // If dictionary & firstEvent not defined and user header not given ...
        if (dictionaryFirstEventBuffer.remaining() < 1) {
            headerBuffer = createHeader(nullptr, 0);
        }
        // else place dictionary and/or firstEvent into
        // record which becomes user header
        else {
            headerBuffer = createHeader(dictionaryFirstEventBuffer);
        }
    }

    // Write this to file
    // TODO: what flags??? instead of "rw"
    outFile = ofstream(filename, ios::binary);
    outFile.write(reinterpret_cast<const char*>(headerBuffer.array()), headerBuffer.remaining());

    writerBytesWritten = (size_t) (fileHeader.getLength());
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
 * @param userLen   length of userHdr in bytes.
 * @throws HipoException buf arg is null, if constructor specified writing to a file,
 *                       or if open() was already called without being followed by reset().
 */
void Writer::open(ByteBuffer & buf, uint8_t* userHdr, uint32_t userLen) {

    if (opened) {
        throw HipoException("currently open, call reset() first");
    }
    else if (toFile) {
        throw HipoException("can only write to a file, call open(filename, userHdr)");
    }

    // Put dictionary / first event in user header of first record
    if ((userHdr == nullptr || userLen < 1) && (dictionaryFirstEventBuffer.remaining() < 1)) {
        userHdr = dictionaryFirstEventBuffer.array();
        userLen = dictionaryFirstEventBuffer.remaining();
    }

    if (userHdr == nullptr) {
        if (dictionaryFirstEventBuffer.remaining() > 0) {
            userHeader = dictionaryFirstEventBuffer.array();
            userHeaderBuffer = dictionaryFirstEventBuffer;
            userHeaderLength = dictionaryFirstEventBuffer.remaining();
        }
        else {
            userHeader = nullptr;
            userHeaderBuffer.clear();
            userHeaderLength = 0;
        }
    }
    else if (userLen > 0) {
        userHeader = userHdr;
        userHeaderBuffer = ByteBuffer(reinterpret_cast<char *>(userHdr), userLen);
        userHeaderLength = userLen;
    }
    else if (dictionaryFirstEventBuffer.remaining() > 0) {
        userHeader = dictionaryFirstEventBuffer.array();
        userHeaderBuffer = dictionaryFirstEventBuffer;
        userHeaderLength = dictionaryFirstEventBuffer.remaining();
    }
    else {
        userHeader = nullptr;
        userHeaderBuffer.clear();
        userHeaderLength = 0;
    }

    buffer = buf;
    buffer.order(byteOrder);

    opened = true;
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
 * @return buffer representation of record
 *         containing dictionary and/or first event.
 *         Null if both are null.
 */
ByteBuffer Writer::createRecord(const string & dict, uint8_t* firstEv, uint32_t firstEvLen,
                                  const ByteOrder & order, FileHeader* fileHdr,
                                  RecordHeader* recordHdr) {

    if (dict.length() < 1 && firstEv == nullptr) {
        return ByteBuffer(0);
    }

    // Create record.
    // Bit of chicken&egg problem, so start with default internal buf size.
    RecordOutput record(order, 2, 0, Compressor::UNCOMPRESSED);

    // How much data we got?
    int bytes=0;

    if (dict.length() > 0) {
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
    if (dict.length() > 0) {
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
 * Convenience method that sets compression type for the file.
 * The compression type is also set for internal record.
 * When writing to the file, record data will be compressed
 * according to the given type.
 * @param compression compression type
 * @return this object
 */
Writer & Writer::setCompressionType(Compressor::CompressionType compression) {
    outputRecord.getHeader().setCompressionType(compression);
    compressionType = outputRecord.getHeader().getCompressionType();
    return *this;
}


/**
 * Create and return a buffer containing a general file header
 * followed by the user header given in the argument.
 * If user header is not padded to 4-byte boundary, it's done here.
 *
 * @param userHdr byte array containing a user-defined header, may be null.
 * @param userLen array length in bytes.
 * @return buffer (same as buf arg).
 * @throws HipoException if writing to buffer, not file.
 */
ByteBuffer Writer::createHeader(uint8_t* userHdr, uint32_t userLen) {

    if (!toFile) {
        throw HipoException("call only if writing to file");
    }

    cout << "createHeader: IN, fe bit = " << fileHeader.hasFirstEvent() << endl;

    // Amount of user data in bytes
    int userHeaderBytes = 0;
    if (userHdr != nullptr) {
        userHeaderBytes = userLen;
    }
// TODO: make sure next line is necessary (taken from WriterMT)
    fileHeader.reset();
    fileHeader.setUserHeaderLength(userHeaderBytes);

    cout << "createHeader: after set user header len, fe bit = " << fileHeader.hasFirstEvent() << endl;
    uint32_t totalLen = fileHeader.getLength();
    ByteBuffer buf(totalLen);
    buf.order(byteOrder);

    try {
        cout << "createHeader: will write file header into buffer: hasFE = " << fileHeader.hasFirstEvent() << endl;
        fileHeader.writeHeader(buf, 0);
    }
    catch (HipoException & e) {/* never happen */}

    if (userHeaderBytes > 0) {
        std::memcpy((void *)(buf.array() + FileHeader::HEADER_SIZE_BYTES),
                    (const void *)(userHdr), userHeaderBytes);
//            System.arraycopy(userHdr, 0, buf.array(), RecordHeader::HEADER_SIZE_BYTES, userHeaderBytes);
    }

    // Get ready to read, buffer position is still 0
    buf.limit(totalLen);
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
 * @throws HipoException if writing to buffer, or buf too small
 *                       (needs to be userLen + FileHeader::HEADER_SIZE_BYTES bytes).
 */
void Writer::createHeader(ByteBuffer & buf, uint8_t* userHdr, uint32_t userLen) {

    if (!toFile) {
        throw HipoException("call only if writing to file");
    }

    if (userLen + FileHeader::HEADER_SIZE_BYTES < buf.capacity()) {
        throw HipoException("buffer too small, need " +
                            to_string(userLen + FileHeader::HEADER_SIZE_BYTES) + " bytes");
    }

    cout << "createHeader: IN, fe bit = " << fileHeader.hasFirstEvent() << endl;

    // Amount of user data in bytes
    int userHeaderBytes = 0;
    if (userHdr != nullptr) {
        userHeaderBytes = userLen;
    }
    fileHeader.reset();
    fileHeader.setUserHeaderLength(userHeaderBytes);

    cout << "createHeader: after set user header len, fe bit = " << fileHeader.hasFirstEvent() << endl;
    uint32_t totalLen = fileHeader.getLength();
    buf.clear();
    buf.order(byteOrder);

    try {
        cout << "createHeader: will write file header into buffer: hasFE = " << fileHeader.hasFirstEvent() << endl;
        fileHeader.writeHeader(buf, 0);
    }
    catch (HipoException & e) {/* never happen */}

    if (userHeaderBytes > 0) {
        std::memcpy((void *)(buf.array() + FileHeader::HEADER_SIZE_BYTES),
                    (const void *)(userHdr), userHeaderBytes);
//            System.arraycopy(userHdr, 0, buf.array(), RecordHeader::HEADER_SIZE_BYTES, userHeaderBytes);
    }

    // Get ready to read, buffer position is still 0
    buf.limit(totalLen);
}


/**
 * Fill given buffer (buf) with a general file header followed by the given user header (userHdr).
 * The buffer is cleared and set to desired byte order prior to writing.
 * If user header is not padded to 4-byte boundary, it's done here.
 *
 * @param userHdr buffer containing a user-defined header which must be READY-TO-READ!
 * @return buffer containing a file header followed by the user-defined header.
 * @throws HipoException if writing to buffer, not file.
 */
ByteBuffer Writer::createHeader(ByteBuffer & userHdr) {

    if (!toFile) {
        throw HipoException("call only if writing to file");
    }
    cout << "createHeader: IN, fe bit = " << fileHeader.hasFirstEvent() << endl;

    int userHeaderBytes = userHdr.remaining();
    fileHeader.reset();
    fileHeader.setUserHeaderLength(userHeaderBytes);

    cout << "createHeader: after set user header len, fe bit = " << fileHeader.hasFirstEvent() << endl;
    uint32_t totalLen = fileHeader.getLength();
    ByteBuffer buf(totalLen);
    buf.order(byteOrder);

    try {
        cout << "createHeader: will write file header into buffer: hasFE = " << fileHeader.hasFirstEvent() << endl;
        fileHeader.writeHeader(buf, 0);
    }
    catch (HipoException & e) {/* never happen */}

    if (userHeaderBytes > 0) {
        std::memcpy((void *)(buf.array() + FileHeader::HEADER_SIZE_BYTES),
                    (const void *)(userHdr.array() + userHdr.arrayOffset()+ userHdr.position()), userHeaderBytes);
    }

    // Get ready to read, buffer position is still 0
    buf.limit(totalLen);
    return std::move(buf);
}


/**
 * Fill given buffer (buf) with a general file header followed by the given user header (userHdr).
 * The buffer is cleared and set to desired byte order prior to writing.
 * If user header is not padded to 4-byte boundary, it's done here.
 *
 * @param userHdr buffer containing a user-defined header which must be READY-TO-READ!
 * @return buffer containing a file header followed by the user-defined header.
 * @throws HipoException if writing to buffer, or buf too small
 *                       (needs to be userHdr.remaining() + FileHeader::HEADER_SIZE_BYTES bytes).
 */
void Writer::createHeader(ByteBuffer & buf, ByteBuffer & userHdr) {

    if (!toFile) {
        throw HipoException("call only if writing to file");
    }
    cout << "createHeader: IN, fe bit = " << fileHeader.hasFirstEvent() << endl;

    int userHeaderBytes = userHdr.remaining();

    if (userHeaderBytes + FileHeader::HEADER_SIZE_BYTES < buf.capacity()) {
        throw HipoException("buffer too small, need " +
                            to_string(userHeaderBytes + FileHeader::HEADER_SIZE_BYTES) + " bytes");
    }

    fileHeader.reset();
    fileHeader.setUserHeaderLength(userHeaderBytes);

    cout << "createHeader: after set user header len, fe bit = " << fileHeader.hasFirstEvent() << endl;
    uint32_t totalLen = fileHeader.getLength();
    buf.clear();
    buf.order(byteOrder);

    try {
        cout << "createHeader: will write file header into buffer: hasFE = " << fileHeader.hasFirstEvent() << endl;
        fileHeader.writeHeader(buf, 0);
    }
    catch (HipoException & e) {/* never happen */}

    if (userHeaderBytes > 0) {
        std::memcpy((void *)(buf.array() + FileHeader::HEADER_SIZE_BYTES),
                    (const void *)(userHdr.array() + userHdr.arrayOffset()+ userHdr.position()), userHeaderBytes);
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
 *
 * @param writeIndex if true, write an index of all record lengths in trailer.
 * @throws IOException if error writing to file.
 */
void Writer::writeTrailer(bool writeIndex) {

    // If we're NOT adding a record index, just write trailer
    if (!writeIndex) {
        try {
            RecordHeader::writeTrailer(&headerArray[0], headerArray.max_size(), 0,
                                       recordNumber, byteOrder, nullptr, 0);

            // TODO: not really necessary to keep track here?
            writerBytesWritten += RecordHeader::HEADER_SIZE_BYTES;

            if (toFile) {
                outFile.write(reinterpret_cast<const char*>(&headerArray[0]), headerArray.size());
            }
            else {
                buffer.put(&headerArray[0], 0, headerArray.size());
            }
        }
        catch (HipoException & ex) {
            // never happen
        }
        return;
    }

    // Create the index of record lengths & entries in proper byte order
    size_t recordLengthsBytes = 4*recordLengths.size();
    auto recordIndex = new uint8_t[recordLengthsBytes];

    try {
        for (int i = 0; i < recordLengths.size(); i++) {
            toBytes(recordLengths[i], byteOrder, recordIndex, 4*i, recordLengthsBytes);
//cout << "Writing record length = " << recordLengths[i] << showbase << hex <<
//                ", = " << recordLengths[i] << endl;
        }
    }
    catch (HipoException & e) {/* never happen */}

    // Write trailer with index

    // How many bytes are we writing here?
    int dataBytes = RecordHeader::HEADER_SIZE_BYTES + recordLengthsBytes;

    // Make sure our array can hold everything
    if (headerArray.max_size() < dataBytes) {
//cout << "Allocating byte array of " << dataBytes << " bytes in size" << endl;
        headerArray.reserve(dataBytes);
    }

    try {
        // Place data into headerArray - both header and index
        RecordHeader::writeTrailer(&headerArray[0], headerArray.max_size(), 0,
                                   recordNumber, byteOrder, (const uint32_t*)recordIndex,
                                   recordLengthsBytes);

//            RecordHeader::writeTrailer(headerArray, RecordHeader::HEADER_SIZE_BYTES, 0,
//                                       recordNumber, byteOrder, recordIndex, indexLen);
        // TODO: not really necessary to keep track here?
        writerBytesWritten += dataBytes;
        if (toFile) {
            outFile.write(reinterpret_cast<const char*>(&headerArray[0]), dataBytes);
        }
        else {
            buffer.put(&headerArray[0], 0, dataBytes);
        }
    }
    catch (HipoException & ex) {
        // never happen
    }

    delete[] recordIndex;
}


/**
 * Turn int into byte array.
 *
 * @param data        int to convert.
 * @param byteOrder   byte order of returned bytes.
 * @param dest        array in which to store returned bytes.
 * @param off         offset into dest array where returned bytes are placed.
 * @param destMaxSize max size in bytes of dest array.
 * @throws HipoException if dest is null or too small.
 */
void Writer::toBytes(uint32_t data, const ByteOrder & byteOrder,
             uint8_t* dest, uint32_t off, uint32_t destMaxSize) {

    if (dest == nullptr || destMaxSize < 4+off) {
        throw HipoException("bad arg(s)");
    }

    if (byteOrder == ByteOrder::ENDIAN_BIG) {
        dest[off  ] = (uint8_t)(data >> 24);
        dest[off+1] = (uint8_t)(data >> 16);
        dest[off+2] = (uint8_t)(data >>  8);
        dest[off+3] = (uint8_t)(data      );
    }
    else {
        dest[off  ] = (uint8_t)(data      );
        dest[off+1] = (uint8_t)(data >>  8);
        dest[off+2] = (uint8_t)(data >> 16);
        dest[off+3] = (uint8_t)(data >> 24);
    }
}


/**
 * Appends the record to the file.
 * Using this method in conjunction with addEvent() is not thread-safe.
 * @param record record object
 * @throws HipoException if error writing to file.
 */
void Writer::writeRecord(RecordOutput & record) {

    // Wait for previous (if any) write to finish
    if (toFile && future.valid()) {
        future.get();
        unusedRecord = beingWrittenRecord;

        if (outFile.fail()) {
            throw HipoException("problem writing to file");
        }
    }

    RecordHeader header = record.getHeader();

    // Make sure given record is consistent with this writer
    header.setCompressionType(compressionType);
    header.setRecordNumber(recordNumber++);
    //System.out.println( " set compression type = " + compressionType);
    record.getHeader().setCompressionType(compressionType);
    record.setByteOrder(byteOrder);

    record.build();
    int bytesToWrite = header.getLength();
    // Record length of this record
    recordLengths.push_back(bytesToWrite);
    // Followed by events in record
    recordLengths.push_back(header.getEntries());
    writerBytesWritten += bytesToWrite;

    if (toFile) {
        // Launch async write in separate thread.
        future = std::future<void>(
                      std::async(std::launch::async,  // run in a separate thread
                           staticWriteFunction, // function to run
                           this,                // arguments to function ...
                           reinterpret_cast<const char *>(record.getBinaryBuffer().array()),
                           bytesToWrite));

        // Next record to work with
        outputRecord = unusedRecord;
        outputRecord.reset();
    }
    else {
        buffer.put(record.getBinaryBuffer().array(), 0, bytesToWrite);
    }
}

// Use internal outputRecordStream to write individual events

///**
// * Add a byte array to the internal record. If the length of
// * the buffer exceeds the maximum size of the record, the record
// * will be written to the file (compressed if the flag is set).
// * Internal record will be reset to receive new buffers.
// * Using this method in conjunction with writeRecord() is not thread-safe.
// * <b>The byte order of event's byte array must
// * match the byte order given in constructor!</b>
// *
// * @param buffer array to add to the file.
// * @throws IOException if cannot write to file.
// */
//void Writer::addEvent(uint8_t* buffer) {
//        addEvent(buffer, 0, buffer.length);
//}

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
 * @throws IOException if cannot write to file.
 */
void Writer::addEvent(uint8_t* buffer, uint32_t offset, uint32_t length) {
    bool status = outputRecord.addEvent(buffer, offset, length);
    if (!status){
        writeOutput();
        outputRecord.addEvent(buffer, offset, length);
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
 * @throws IOException if cannot write to file.
 */
void Writer::addEvent(ByteBuffer & buffer) {
    bool status = outputRecord.addEvent(buffer);
    if (!status){
        writeOutput();
        outputRecord.addEvent(buffer);
    }
}

///**
// * Add an EvioBank to the internal record. If the length of
// * the bank exceeds the maximum size of the record, the record
// * will be written to the file (compressed if the flag is set).
// * Internal record will be reset to receive new buffers.
// * Using this method in conjunction with writeRecord() is not thread-safe.
// *
// * @param bank event to add to the file.
// * @throws IOException if cannot write to file.
// */
//void Writer::addEvent(EvioBank & bank) {
//    bool status = outputRecord.addEvent(bank);
//    if (!status){
//        writeOutput();
//        outputRecord.addEvent(bank);
//    }
//}
//
///**
// * Add an EvioNode to the internal record. If the length of
// * the data exceeds the maximum size of the record, the record
// * will be written to the file (compressed if the flag is set).
// * Internal record will be reset to receive new buffers.
// * Using this method in conjunction with writeRecord() is not thread-safe.
// * <b>The byte order of node's data must
// * match the byte order given in constructor!</b>
// *
// * @param node node to add to the file.
// * @throws HipoException if node does not correspond to a bank.
// * @throws IOException if cannot write to file.
// */
//void Writer::addEvent(EvioNode & node) {
//    bool status = outputRecord.addEvent(node);
//    if (!status){
//        writeOutput();
//        outputRecord.addEvent(node);
//    }
//}


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
        if (closed) return;

        if (outputRecord.getEventCount() > 0) {
            // This will wait for previous asynchronous write to finish
            writeOutput();
        }
        else if (toFile) {
            // Wait for previous (if any) asynchronous write to finish
            if (future.valid()) {
                future.get();
            }
        }

        if (addingTrailer) {
            // Keep track of where we are right now which is just before trailer
            uint64_t trailerPosition = writerBytesWritten;

            // Write the trailer
            writeTrailer(addTrailerIndex);

            if (toFile) {
                // Find & update file header's trailer position word
                outFile.seekp(FileHeader::TRAILER_POSITION_OFFSET);
                if (byteOrder == ByteOrder::ENDIAN_LITTLE) {
                    outFile << SWAP_64(trailerPosition);
                    //outFile.writeLong(Long.reverseBytes(trailerPosition));
                }
                else {
                    outFile << trailerPosition;
                    //outFile.writeLong(trailerPosition);
                }

                // Find & update file header's bit-info word
                if (addTrailerIndex) {
                    outFile.seekp(RecordHeader::BIT_INFO_OFFSET);
                    int bitInfo = fileHeader.hasTrailerWithIndex(true);

                    if (byteOrder == ByteOrder::ENDIAN_LITTLE) {
                        outFile << SWAP_32(bitInfo);
                        //outFile.writeInt(Integer.reverseBytes(bitInfo));
                    }
                    else {
                        outFile << bitInfo;
                        //outFile.writeInt(bitInfo);
                    }
                }
            }
        }

        if (toFile) {
            // Need to update the record count in file header
            outFile.seekp(FileHeader::RECORD_COUNT_OFFSET);
            if (byteOrder == ByteOrder::ENDIAN_LITTLE) {
                outFile << SWAP_32(recordNumber - 1);
                //outFile.writeInt(Integer.reverseBytes(recordNumber - 1));
            }
            else {
                outFile << (recordNumber - 1);
                //outFile.writeInt(recordNumber - 1);
            }

            outFile.close();
        }

        closed = true;
        //cout << "[writer] ---> bytes written " << writerBytesWritten << endl;
}
