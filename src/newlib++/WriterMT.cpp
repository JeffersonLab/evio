//
// Created by Carl Timmer on 2019-05-13.
//

#include "WriterMT.h"



/**
 * Default constructor. Compression is single-threaded, LZ4. Little endian.
 * <b>No</b> file is opened. Any file will have little endian byte order.
 * 1M max event count and 8M max buffer size.
 */
WriterMT::WriterMT() :
    WriterMT(HeaderType::EVIO_FILE, ByteOrder::ENDIAN_LITTLE, 0, 0,
             Compressor::LZ4, 1, "", nullptr, 0) {
}


/**
 * Constructor with byte order. <b>No</b> file is opened.
 * File header type is evio file ({@link HeaderType#EVIO_FILE}).
 * Any dictionary will be placed in the user header which will create a conflict if
 * user tries to call {@link #open(String, byte[])} with another user header array.
 *
 * @param order byte order of written file.
 * @param maxEventCount max number of events a record can hold.
 *                      Value of O means use default (1M).
 * @param maxBufferSize max number of uncompressed data bytes a record can hold.
 *                      Value of < 8MB results in default of 8MB.
 */
WriterMT::WriterMT(const ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize,
                   Compressor::CompressionType compType, uint32_t compressionThreads) :
    WriterMT(HeaderType::EVIO_FILE, order, maxEventCount, maxBufferSize,
             compType, compressionThreads, "", nullptr, 0) {
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
WriterMT::WriterMT(const HeaderType & hType, const ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize,
                   Compressor::CompressionType compType, uint32_t compressionThreads,
                   const string & dictionary, uint8_t* firstEvent, uint32_t firstEventLen) {

    this->dictionary = dictionary;
    this->firstEvent = firstEvent;
    firstEventLength = firstEventLen;

    byteOrder = order;
    compressionType = compType;
    compressionThreadCount = compressionThreads;

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
    outputRecord = RecordOutput(order, maxEventCount, maxBufferSize, compType);

    if ((dictionary.length() > 0) ||
        (firstEvent != nullptr && firstEventLen > 0))  {
        dictionaryFirstEventBuffer = createDictionaryRecord();
    }

    if (hType == HeaderType::HIPO_FILE) {
        fileHeader = FileHeader(false);
    } else {
        fileHeader = FileHeader(true);
    }

    // Create uncompressed record Queues - one for each compression thread input
    queues = vector<ConcurrentFixedQueue<RecordOutput>>();
    for (int i=0; i < compressionThreadCount; i++) {
        queues.emplace_back(ConcurrentFixedQueue<RecordOutput>());
    }

    // Create 1 queue for compressed records
    writeQueue = ConcurrentFixedQueue<RecordOutput>();

    // 1 thread used to write data to file/buffer
    recordWriterThread = WritingThread(this, queues);

    // Multiple threads used to compress data
    uint32_t recNum = 1;
    recordCompressorThreads = vector<CompressingThread>();
    for (int i=0; i < compressionThreadCount; i++) {
        recordCompressorThreads.emplace_back(CompressingThread(i, recNum, compressionType,
                                                               queues[i], writeQueue));
    }
}


/**
 * Constructor with filename.
 * The output file will be created with no user header.
 * File byte order is little endian.
 * @param filename output file name
 */
WriterMT::WriterMT(string & filename) : WriterMT() {
    open(filename);
}


/**
 * Constructor with filename & byte order.
 * The output file will be created with no user header.
 * @param filename      output file name
 * @param order         byte order of written file or null for default (little endian)
 * @param maxEventCount max number of events a record can hold.
 *                      Value of O means use default (1M).
 * @param maxBufferSize max number of uncompressed data bytes a record can hold.
 *                      Value of < 8MB results in default of 8MB.
 * @param compressionType type of data compression to do (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip)
 * @param compressionThreads number of threads doing compression simultaneously
 * @param ringSize      number of records in supply ring, must be multiple of 2
 *                      and >= compressionThreads.
 */
WriterMT::WriterMT(string & filename, const ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize,
                   Compressor::CompressionType compType, uint32_t compressionThreads) :
         WriterMT(HeaderType::EVIO_FILE, order, maxEventCount, maxBufferSize,
                  compType, compressionThreads, "", nullptr, 0) {

    open(filename);
}


//////////////////////////////////////////////////////////////////////


///**
// * Get the buffer being written to.
// * @return buffer being written to.
// */
//ByteBuffer & WriterMT::getBuffer() {return buffer;}

/**
 * Get the file's byte order.
 * @return file's byte order.
 */
const ByteOrder &  WriterMT::getByteOrder() const {return byteOrder;}

/**
 * Get the file header.
 * @return file header.
 */
FileHeader & WriterMT::getFileHeader() {return fileHeader;}

/**
 * Get the internal record's header.
 * @return internal record's header.
 */
RecordHeader & WriterMT::getRecordHeader() {return outputRecord.getHeader();}

/**
 * Get the internal record used to add events to file.
 * @return internal record used to add events to file.
 */
RecordOutput & WriterMT::getRecord() {return outputRecord;}

/**
 * Convenience method that gets compression type for the file being written.
 * @return compression type for the file being written.
 */
uint32_t WriterMT::getCompressionType() {return outputRecord.getHeader().getCompressionType();}

/**
 * Does this writer add a trailer to the end of the file/buffer?
 * @return true if this writer adds a trailer to the end of the file/buffer, else false.
 */
bool WriterMT::addTrailer() const {return addingTrailer;}

/**
 * Set whether this writer adds a trailer to the end of the file/buffer.
 * @param add if true, at the end of file/buffer, add an ending header (trailer)
 *            with no index of records and no following data.
 *            Update the file header to contain a file offset to the trailer.
 */
void WriterMT::addTrailer(bool add) {addingTrailer = add;}

/**
 * Does this writer add a trailer with a record index to the end of the file?
 * Or, if writing to a buffer, is a trailer added with no index?
 * @return if writing to a file: true if this writer adds a trailer with a record index
 *         to the end of the file, else false. If writing to a buffer, true if this
 *         writer adds a traile to the end of the buffer, else false.
 */
bool WriterMT::addTrailerWithIndex() {return addTrailerIndex;}

/**
 * Set whether this writer adds a trailer with a record index to the end of the file.
 * Or, if writing to a buffer, set whether a trailer is added with no index.
 * @param addTrailingIndex if true, at the end of file, add an ending header (trailer)
 *                         with an index of all records but with no following data.
 *                         Update the file header to contain a file offset to the trailer.
 *                         If true, and writing to a buffer, add a trailer with no index
 *                         to the end of the buffer.
 */
void WriterMT::addTrailerWithIndex(bool addTrailingIndex) {
    addTrailerIndex = addTrailingIndex;
    if (addTrailingIndex) {
        addingTrailer = true;
    }
}


/**
 * Open a new file and write file header with no user header.
 * @param filename output file name
 * @throws HipoException if open already called without being followed by calling close.
 * @throws IOException if file cannot be found or IO error writing to file
 */
void WriterMT::open(string & filename) {
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
void WriterMT::open(string & filename, uint8_t* userHdr, uint32_t userLen) {

    if (opened) {
        throw HipoException("currently open, call reset() first");
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

    // Start compression threads
    for (int i=0; i < compressionThreadCount; i++) {
        recordCompressorThreads[i].startThread();
    }

    // Start writing thread
    recordWriterThread.startThread();

    opened = true;
}


/**
 * Convenience method that sets compression type for the file.
 * The compression type is also set for internal record.
 * When writing to the file, record data will be compressed
 * according to the given type.
 * @param compression compression type
 * @return this object
 */
WriterMT & WriterMT::setCompressionType(Compressor::CompressionType compression) {
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
 */
ByteBuffer WriterMT::createHeader(uint8_t* userHdr, uint32_t userLen) {

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
 * Turn int into byte array.
 *
 * @param data        int to convert.
 * @param byteOrder   byte order of returned bytes.
 * @param dest        array in which to store returned bytes.
 * @param off         offset into dest array where returned bytes are placed.
 * @param destMaxSize max size in bytes of dest array.
 * @throws HipoException if dest is null or too small.
 */
void WriterMT::toBytes(uint32_t data, const ByteOrder & byteOrder,
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
void WriterMT::writeTrailer(bool writeIndex) {

    // Keep track of where we are right now which is just before trailer
    uint64_t trailerPosition = writerBytesWritten;

    // If we're NOT adding a record index, just write trailer
    if (!writeIndex) {
        try {
            RecordHeader::writeTrailer(&headerArray[0], headerArray.max_size(), 0,
                                       recordNumber, byteOrder, nullptr, 0);

            // TODO: not really necessary to keep track here?
            writerBytesWritten += RecordHeader::HEADER_SIZE_BYTES;
            outFile.write(reinterpret_cast<const char*>(&headerArray[0]), headerArray.size());
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
        outFile.write(reinterpret_cast<const char*>(&headerArray[0]), dataBytes);
    }
    catch (HipoException & ex) {
        // never happen
    }

    // Find & update file header's trailer position word
    outFile.seekp(FileHeader::TRAILER_POSITION_OFFSET);
    if (byteOrder == ByteOrder::ENDIAN_LITTLE) {
        uint64_t pos = SWAP_64(trailerPosition);
        outFile.write(reinterpret_cast<const char *>(&pos), sizeof(uint64_t));
    }
    else {
        outFile.write(reinterpret_cast<const char *>(&trailerPosition), sizeof(uint64_t));
    }

    // Find & update file header's bit-info word
    if (addTrailerIndex) {
        outFile.seekp(RecordHeader::BIT_INFO_OFFSET);
        int bitInfo = fileHeader.setBitInfo(false, false, true);
        if (byteOrder == ByteOrder::ENDIAN_LITTLE) {
            uint32_t bitSwap = SWAP_32(bitInfo);
            outFile.write(reinterpret_cast<const char *>(&bitSwap), sizeof(uint32_t));
        }
        else {
            outFile.write(reinterpret_cast<const char *>(&bitInfo), sizeof(uint32_t));
        }
    }

    delete[] recordIndex;
}


/**
 * Appends the record to the file.
 * Using this method in conjunction with addEvent() is not thread-safe.
 * @param record record object
 * @throws HipoException if error writing to file.
 */
void WriterMT::writeRecord(RecordOutput & record) {

    RecordHeader header = record.getHeader();

    // Make sure given record is consistent with this writer
    header.setCompressionType(compressionType);
    header.setRecordNumber(recordNumber++);
    //System.out.println( " set compression type = " + compressionType);
    record.getHeader().setCompressionType(compressionType);
    record.setByteOrder(byteOrder);

    record.build();

// TODO: we only write the trailer at the VERY VERY end!!!
    int bytesToWrite = header.getLength();
    // Record length of this record
    recordLengths.push_back(bytesToWrite);
    // Followed by events in record
    recordLengths.push_back(header.getEntries());
    writerBytesWritten += bytesToWrite;

// TODO: put the bytesToWrite into the record object so the write thread knows how much data to write!!!!!!!!!!!!!
    queues[nextQueueIndex++].push(record);

    // Launch async write in separate thread.
    future = std::future<void>(
            std::async(std::launch::async,  // run in a separate thread
                       staticWriteFunction, // function to run
                       this,                // arguments to function ...
                       reinterpret_cast<const char *>(record.getBinaryBuffer().array()),
                       bytesToWrite));
}


/**
 * Write this record into one taken from the supply.
 * Using this method in conjunction with {@link #addEvent(byte[], int, int)}
 * is not thread-safe.
 * @param rec record object
 * @throws IllegalArgumentException if arg's byte order is opposite to record supply.
 * @throws HipoException if we cannot replace internal buffer of the current record
 *                       if it needs to be expanded since the buffer was provided
 *                       by the user.
 */
void writeRecord(RecordOutput rec) {

        // Need to ensure that the given record has a byte order the same
        // as all the records in the supply.
        if (rec.getByteOrder() != byteOrder) {
            throw new IllegalArgumentException("byte order of record is wrong");
        }

        // If we have already written stuff into our current internal record ...
        if (record.getEventCount() > 0) {
            // Put it back in supply for compressing
            supply.publish(ringItem);

            // Now get another, empty record.
            // This may block if threads are busy compressing
            // and/or writing all records in supply.
            ringItem = supply.get();
            record = ringItem.getRecord();
        }

        // Copy rec into an empty record taken from the supply
        record.copy(rec);

        // Make sure given record is consistent with this writer
        RecordHeader header = record.getHeader();
        header.setCompressionType(compressionType);
        header.setRecordNumber(recordNumber++);
        record.setByteOrder(byteOrder);

        // Put it back in supply for compressing
        supply.publish(ringItem);

        // Get another
        ringItem = supply.get();
        record = ringItem.getRecord();
}

/**
 * Add a byte array to the current internal record. If the length of
 * the buffer exceeds the maximum size of the record, the record
 * will be written to the file (compressed if the flag is set).
 * And another record will be obtained from the supply to receive the buffer.
 * Using this method in conjunction with {@link #writeRecord(RecordOutputStream)}
 * is not thread-safe.
 *
 * @param buffer array to add to the file.
 * @param offset offset into array from which to start writing data.
 * @param length number of bytes to write from array.
 */
public void addEvent(byte[] buffer, int offset, int length) {
    // Try putting data into current record being filled
    boolean status = record.addEvent(buffer, offset, length);

    // If record is full ...
    if (!status) {
        // Put it back in supply for compressing
        supply.publish(ringItem);

        // Now get another, empty record.
        // This may block if threads are busy compressing
        // and/or writing all records in supply.
        ringItem = supply.get();
        record   = ringItem.getRecord();

        // Adding the first event to a record is guaranteed to work
        record.addEvent(buffer, offset, length);
    }
}

/**
 * Add a byte array to the current internal record. If the length of
 * the buffer exceeds the maximum size of the record, the record
 * will be written to the file (compressed if the flag is set).
 * And another record will be obtained from the supply to receive the buffer.
 * Using this method in conjunction with {@link #writeRecord(RecordOutputStream)}
 * is not thread-safe.
 *
 * @param buffer array to add to the file.
 */
public void addEvent(byte[] buffer){
    addEvent(buffer, 0, buffer.length);
}

//---------------------------------------------------------------------

/** Get this object ready for re-use.
 * Follow calling this with call to {@link #open(String)}. */
public void reset() {
    record.reset();
    fileHeader.reset();
    writerBytesWritten = 0L;
    recordNumber = 1;
    addingTrailer = false;
}

/**
 * Close opened file. If the output record contains events,
 * they will be flushed to file. Trailer and its optional index
 * written if requested.<p>
 * <b>The addEvent or addRecord methods must no longer be called.</b>
 */
public void close(){

    // If we're in the middle of building a record, send it off since we're done
    if (record.getEventCount() > 0) {
        // Put it back in supply for compressing
        supply.publish(ringItem);
    }

    // Since the writer thread is the last to process each record,
    // wait until it's done with the last item, then exit the thread.
    recordWriterThread.waitForLastItem();

    // Stop all compressing threads which by now are stuck on get
    for (RecordCompressor rc : recordCompressorThreads) {
        rc.interrupt();
    }

    try {
        if (addingTrailer) {
            // Write the trailer
            writeTrailer(addTrailerIndex);
        }

        // Need to update the record count in file header
        outStream.seek(FileHeader.RECORD_COUNT_OFFSET);
        if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
            outStream.writeInt(Integer.reverseBytes(recordNumber - 1));
        }
        else {
            outStream.writeInt(recordNumber - 1);
        }
        outStream.close();
        //System.out.println("[writer] ---> bytes written " + writerBytesWritten);
    } catch (IOException ex) {
        Logger.getLogger(WriterMT.class.getName()).log(Level.SEVERE, null, ex);
    }
}
}
