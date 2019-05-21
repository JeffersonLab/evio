//
// Created by Carl Timmer on 2019-05-13.
//

#include "WriterMT.h"



/**
 * Default constructor. Compression is single-threaded TODO:  LZ4.
 * Little endian.
 * <b>No</b> file is opened. Any file will have little endian byte order.
 */
WriterMT::WriterMT() {

    compressionThreadCount = 1;
    outputRecord = RecordOutput();
    fileHeader   = FileHeader(true);
    recordLengths.reserve(1500);
    headerArray.reserve(RecordHeader::HEADER_SIZE_BYTES);

    // create record qs - one for each compression thread
    queues = vector<ConcurrentQueue<RecordOutput>>();
    for (int i=0; i < compressionThreadCount; i++) {
        queues.emplace_back(ConcurrentQueue<RecordOutput>());
    }

    /** Thread used to write data to file/buffer. */
    recordWriterThread = WritingThread(this, queues);

    writeQueue = ConcurrentQueue<RecordOutput>();

    /** Threads used to compress data. */
    uint32_t recNum = 1;
    recordCompressorThreads = vector<CompressingThread>();
    for (int i=0; i < compressionThreadCount; i++) {
        recordCompressorThreads.emplace_back(CompressingThread(i, recNum, compressionType,
                                             queues[i], writeQueue));
    }
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
                   Compressor::CompressionType compType, uint32_t compressionThreads, uint32_t ringSize) :
        WriterMT(HeaderType::EVIO_FILE, order, maxEventCount, maxBufferSize,
                 compType, compressionThreads, ringSize, "", nullptr, 0) {
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
                   Compressor::CompressionType compType, uint32_t compressionThreads, uint32_t ringSize,
                   const string & dictionary, uint8_t* firstEvent, uint32_t firstEventLen) {

    this->dictionary = dictionary;
    this->firstEvent = firstEvent;
    this->firstEventLength = firstEventLen;

    headerArray.reserve(RecordHeader::HEADER_SIZE_BYTES);
    outputRecord = RecordOutput(order, maxEventCount, maxBufferSize, 1);

    if ( (dictionary.length() > 0) ||
         (firstEvent != nullptr && firstEventLen > 0))  {
        dictionaryFirstEventBuffer = createDictionaryRecord();
    }

    if (hType == HeaderType::HIPO_FILE) {
        fileHeader = FileHeader(false);
    } else {
        fileHeader = FileHeader(true);
    }

    if (compressionType < 4) {
        this->compressionType = compressionType;
    }
    else {
        throw HipoException("compressionType must be 0,1,2,or 3");
    }

    compressionThreadCount = compressionThreads;

    // Number of ring items must be >= compressionThreads
    int finalRingSize = ringSize;
    if (finalRingSize < compressionThreads) {
        finalRingSize = compressionThreads;
    }
    // AND is must be multiple of 2
    finalRingSize = Utilities.powerOfTwo(finalRingSize, true);

    if (finalRingSize != ringSize) {
        cout << "WriterMT: change to ring size = " << finalRingSize << endl;
    }

    supply = new RecordSupply(ringSize, byteOrder, compressionThreads,
                              maxEventCount, maxBufferSize, compressionType);

    // Get a single blank record to start writing into
    ringItem = supply.get();
    record   = ringItem.getRecord();
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
                   Compressor::CompressionType compType, uint32_t compressionThreads, uint32_t ringSize) :
         WriterMT(HeaderType::EVIO_FILE, order, maxEventCount, maxBufferSize,
                  compType, compressionThreads, ringSize, "", nullptr, 0) {

    open(filename);
}




//////////////////////////////////////////////////////////////////////

/**
 * Get the buffer being written to.
 * @return buffer being written to.
 */
ByteBuffer & WriterMT::getBuffer() {return buffer;}

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

    if (!toFile) {
        addTrailerIndex = false;
    }
}







/**
 * Open a new file and write file header with no user header.
 * @param filename output file name
 */
public final void open(String filename) {open(filename, new byte[]{});}

/**
 * Open a file and write file header with given user's header.
 * User header is automatically padded when written.
 * @param filename disk file name.
 * @param userHeader byte array representing the optional user's header.
 */
public final void open(String filename, byte[] userHeader){

if (userHeader == null) {
userHeader = new byte[0];
}

try {
outStream = new RandomAccessFile(filename, "rw");
fileChannel = outStream.getChannel();
// Create complete file header here (general file header + index array + user header)
ByteBuffer headerBuffer = createHeader(userHeader);
// Write this to file
outStream.write(headerBuffer.array());

} catch (FileNotFoundException ex) {
Logger.getLogger(WriterMT.class.getName()).log(Level.SEVERE, null, ex);
} catch (IOException ex) {
Logger.getLogger(WriterMT.class.getName()).log(Level.SEVERE, null, ex);
}

writerBytesWritten = (long) (fileHeader.getLength());

// Create and start compression threads
recordCompressorThreads = new RecordCompressor[compressionThreadCount];
for (int i=0; i < compressionThreadCount; i++) {
recordCompressorThreads[i] = new RecordCompressor(i);
recordCompressorThreads[i].start();
}

// Create and start writing thread
recordWriterThread = new RecordWriter();
recordWriterThread.start();

//        // Get a single blank record to start writing into
//        ringItem = supply.get();
//        record   = ringItem.getRecord();
}

/**
 * Convenience method that sets compression type for the file.
 * When writing to the file, record data will be compressed
 * according to the given type.
 * @param compression compression type
 * @return this object
 */
public final WriterMT setCompressionType(int compression){
    if (compression > -1 && compression < 4) {
        compressionType = compression;
    }
    return this;
}

/**
 * Create and return a byte array containing a general file header
 * followed by the user header given in the argument.
 * If user header is not padded to 4-byte boundary, it's done here.
 * @param userHeader byte array containing a user-defined header
 * @return byte array containing a file header followed by the user-defined header
 */
public ByteBuffer createHeader(byte[] userHeader){
    // Amount of user data in bytes
    int userHeaderBytes = userHeader.length;

    fileHeader.reset();
    fileHeader.setUserHeaderLength(userHeaderBytes);
    // Amount of user data in bytes + padding
    int userHeaderPaddedBytes = 4*fileHeader.getUserHeaderLengthWords();
    int bytes = RecordHeader.HEADER_SIZE_BYTES + userHeaderPaddedBytes;
    fileHeader.setLength(bytes);

    byte[] array = new byte[bytes];
    ByteBuffer buffer = ByteBuffer.wrap(array);
    buffer.order(byteOrder);

    try {
        fileHeader.writeHeader(buffer, 0);
    }
    catch (HipoException e) {/* never happen */}
    System.arraycopy(userHeader, 0, array,
                     RecordHeader.HEADER_SIZE_BYTES, userHeaderBytes);

    return buffer;
}


/**
 * Write a general header as the last "header" or trailer in the file
 * optionally followed by an index of all record lengths.
 * @param writeIndex if true, write an index of all record lengths in trailer.
 * @throws IOException if problems writing to file.
 */
private void writeTrailer(boolean writeIndex) throws IOException {

        // Keep track of where we are right now which is just before trailer
        long trailerPosition = writerBytesWritten;

        // If we're NOT adding a record index, just write trailer
        if (!writeIndex) {
            try {
                RecordHeader.writeTrailer(headerArray, recordNumber, byteOrder, null);
            }
            catch (HipoException e) {/* never happen */}
            writerBytesWritten += RecordHeader.HEADER_SIZE_BYTES;
            outStream.write(headerArray, 0, RecordHeader.HEADER_SIZE_BYTES);
        }
        else {
            // Create the index of record lengths in proper byte order
            byte[] recordIndex = new byte[4*recordLengths.size()];
            try {
                for (int i = 0; i < recordLengths.size(); i++) {
                    ByteDataTransformer.toBytes(recordLengths.get(i), byteOrder,
                                                recordIndex, 4*i);
//System.out.println("Writing record length = " + recordOffsets.get(i) +
//", = 0x" + Integer.toHexString(recordOffsets.get(i)));
                }
            }
            catch (EvioException e) {/* never happen */}

            // Write trailer with index

            // How many bytes are we writing here?
            int dataBytes = RecordHeader.HEADER_SIZE_BYTES + recordIndex.length;

            // Make sure our array can hold everything
            if (headerArray.length < dataBytes) {
//System.out.println("Allocating byte array of " + dataBytes + " bytes in size");
                headerArray = new byte[dataBytes];
            }

            // Place data into headerArray - both header and index
            try {
                RecordHeader.writeTrailer(headerArray, recordNumber,
                                          byteOrder, recordIndex);
            }
            catch (HipoException e) {/* never happen */}
            writerBytesWritten += dataBytes;
            outStream.write(headerArray, 0, dataBytes);
        }

        // Find & update file header's trailer position word
        outStream.seek(FileHeader.TRAILER_POSITION_OFFSET);
        if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
            outStream.writeLong(Long.reverseBytes(trailerPosition));
        }
        else {
            outStream.writeLong(trailerPosition);
        }

        // Find & update file header's bit-info word
        if (addTrailerIndex) {
            outStream.seek(RecordHeader.BIT_INFO_OFFSET);
            int bitInfo = fileHeader.setBitInfo(false, false, true);
            if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
                outStream.writeInt(Integer.reverseBytes(bitInfo));
            }
            else {
                outStream.writeInt(bitInfo);
            }
        }

        return;
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
public void writeRecord(RecordOutputStream rec)
throws IllegalArgumentException, HipoException {

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
