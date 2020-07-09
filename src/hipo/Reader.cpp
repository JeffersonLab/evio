/**
 * Copyright (c) 2019, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 07/18/2019
 * @author timmer
 */

#include "Reader.h"


namespace evio {


/**
 * Reader class that reads files stored in the HIPO format.<p>
 *
 * <pre>
 * File has this structure:
 *
 *    +----------------------------------+
 *    |      General File Header         |
 *    +----------------------------------+
 *    +----------------------------------+
 *    |         Index (optional)         |
 *    +----------------------------------+
 *    +----------------------------------+
 *    |     User Header (optional)       |
 *    +----------------------------------+
 *    +----------------------------------+
 *    |                                  |
 *    |            Record 1              |
 *    |                                  |
 *    |                                  |
 *    |                                  |
 *    +----------------------------------+
 *                   ...
 *    +----------------------------------+
 *    |                                  |
 *    |            Record N              |
 *    |                                  |
 *    |                                  |
 *    |                                  |
 *    +----------------------------------+
 *    +----------------------------------+
 *    |       Trailer (optional)         |
 *    +----------------------------------+
 *    +----------------------------------+
 *    |    Trailer's Index (optional)    |
 *    +----------------------------------+
 *
 *
 *
 * Buffer or streamed data has this structure:
 *
 *    +----------------------------------+
 *    |                                  |
 *    |            Record 1              |
 *    |                                  |
 *    |                                  |
 *    |                                  |
 *    +----------------------------------+
 *                   ...
 *    +----------------------------------+
 *    |                                  |
 *    |            Record N              |
 *    |                                  |
 *    |                                  |
 *    |                                  |
 *    +----------------------------------+
 *    +----------------------------------+
 *    |       Trailer (optional)         |
 *    +----------------------------------+
 *
 * The important thing with a buffer or streaming is for the last header or
 * trailer to set the "last record" bit.
 *
 * </pre>
 *
 * Something to keep in mind is one can intersperse sequential calls
 * (getNextEvent, getPrevEvent, or getNextEventNode) with random access
 * calls (getEvent or getEventNode), and the sequence remains unchanged
 * after the random access.
 *
 * @version 6.0
 * @since 6.0 08/10/2017
 * @author gavalian
 * @author timmer
 * @see FileHeader
 * @see RecordInput
 */


// Define static member first (never actually used)
EvioNodeSource Reader::nodePoolStatic;


/**
 * Default constructor. Does nothing.
 * The {@link #open(String)} method has to be called to open the input stream.
 * Also {@link #forceScanFile()} needs to be called to find records.
 */
Reader::Reader() : nodePool(nodePoolStatic) {
    // Throw exception if logical or read/write error on io operation
    inStreamRandom.exceptions(ifstream::failbit | ifstream::badbit);
}

/**
 * Constructor with filename. Creates instance and opens
 * the input stream with given name. Uses existing indexes
 * in file before scanning.
 * @param filename input file name.
 * @throws IOException   if error reading file
 * @throws EvioException if file is not in the proper format or earlier than version 6
 */
Reader::Reader(string const & filename) : nodePool(nodePoolStatic) {
    // Throw exception if logical or read/write error on io operation
    inStreamRandom.exceptions(ifstream::failbit | ifstream::badbit);
    open(filename);
    scanFile(false);
}

/**
 * Constructor with filename. Creates instance and opens
 * the input stream with given name.
 * @param filename input file name.
 * @param forceScan if true, force a scan of file, else use existing indexes first.
 * @throws IOException   if error reading file
 * @throws EvioException if file is not in the proper format or earlier than version 6
 */
Reader::Reader(string const & filename, bool forceScan) : nodePool(nodePoolStatic) {
    // Throw exception if logical or read/write error on io operation
    inStreamRandom.exceptions(ifstream::failbit | ifstream::badbit);

    open(filename);
    if (forceScan){
        forceScanFile();
    } else {
        scanFile(forceScan);
    }
}

/**
 * Constructor for reading buffer with evio data.
 * Buffer must be ready to read with position and limit set properly.
 * @param buffer buffer with evio data.
 * @throws EvioException if buffer too small, not in the proper format, or earlier than version 6
 */
Reader::Reader(std::shared_ptr<ByteBuffer> & buffer): nodePool(nodePoolStatic) {

    this->buffer = buffer;
    bufferOffset = buffer->position();
    bufferLimit  = buffer->limit();
    fromFile = false;

    scanBuffer();
}


/**
 * Constructor for reading buffer with evio data.
 * Buffer must be ready to read with position and limit set properly.
 * @param buffer buffer with evio data.
 * @param pool pool of EvioNode objects for garbage-free operation.
 * @param checkRecordNumSeq if true, check to see if all record numbers are in order,
 *                          if not throw exception.
 * @throws EvioException if buffer too small, not in the proper format, or earlier than version 6;
 *                       if checkRecordNumSeq is true and records are out of sequence.
 */
Reader::Reader(std::shared_ptr<ByteBuffer> & buffer, EvioNodeSource & pool, bool checkRecordNumSeq) : nodePool(pool) {
    this->buffer = buffer;
    bufferOffset = buffer->position();
    bufferLimit  = buffer->limit();
    fromFile = false;
    checkRecordNumberSequence = checkRecordNumSeq;

    scanBuffer();
}

/**
 * Opens an input stream in binary mode. Scans for
 * records in the file and stores record information
 * in internal array. Each record can be read from the file.
 * @param filename input file name
 * @throws EvioException if error handling file
 */
void Reader::open(string const & filename) {
   try {
       if (inStreamRandom.is_open()) {
           //cout << "[READER] ---> closing current file : " << fileName << endl;
           inStreamRandom.close();
       }

       // This may be called after using a buffer as input, so zero some things out
       buffer = nullptr;
       bufferOffset = 0;
       bufferLimit  = 0;
       fromFile = true;

       fileName = filename;

       cout << "[READER] ----> opening file : " << filename << endl;
       // "ate" mode flag will go immediately to file's end (do this to get its size)
       inStreamRandom.open(filename, std::ios::binary | std::ios::ate);

       fileSize = inStreamRandom.tellg();
       // Go back to beginning of file
       inStreamRandom.seekg(0);
       fromFile = true;
       scanFile(false);
       cout << "[READER] ---> open successful, size : " << fileSize << endl;
   }
   catch (std::exception & e) {
       // e.what() does not give any useful information...
       throw EvioException(strerror(errno));
   }

}

/**
 * This closes the file.
 * @throws IOException if error accessing file
 */
void Reader::close() {
    if (closed) {
        return;
    }

    if (fromFile) {
        inStreamRandom.close();
    }

    closed = true;
}

/**
 * Has {@link #close()} been called (without reopening by calling
 * {@link #setBuffer(ByteBuffer)})?
 *
 * @return {@code true} if this object closed, else {@code false}.
 */
bool Reader::isClosed() const {return closed;}

/**
 * Is a file being read?
 * @return {@code true} if a file is being read, {@code false} if it's a buffer.
 */
bool Reader::isFile() const {return fromFile;}

/**
 * This method can be used to avoid creating additional Reader
 * objects by reusing this one with another buffer.
 * @param buf ByteBuffer to be read
 * @throws EvioException if buf arg is null, buffer too small,
 *                       not in the proper format, or earlier than version 6
 */
void Reader::setBuffer(std::shared_ptr<ByteBuffer> & buf) {
    buffer       = buf;
    bufferLimit  = buffer->limit();
    bufferOffset = buffer->position();
    eventIndex   = FileEventIndex();

    eventNodes.clear();
    recordPositions.clear();

    fromFile = false;
    compressed = false;
    firstEvent = nullptr;
    dictionaryXML = nullptr;
// TODO: set to -1 ???
    sequentialIndex = 0;
    firstRecordHeader->reset();
    currentRecordLoaded = 0;

    scanBuffer();

    closed = false;
}

/**
 * This method can be used to avoid creating additional Reader
 * objects by reusing this one with another buffer. The method
 * {@link #close()} is called before anything else.  The pool is <b>not</b>
 * reset in this method. Caller may do that prior to calling method.
 *
 * @param buf ByteBuffer to be read
 * @param pool pool of EvioNode objects to use when parsing buf.
 * @throws EvioException if buffer too small,
 *                       not in the proper format, or earlier than version 6
 */
void Reader::setBuffer(std::shared_ptr<ByteBuffer> & buf, EvioNodeSource & pool) {
    nodePool = pool;
    setBuffer(buf);
}

/**
 * This method can be used to avoid creating additional Reader
 * objects by reusing this one with another buffer. If the given buffer has
 * uncompressed data, this method becomes equivalent
 * to {@link #setBuffer(ByteBuffer, EvioNodeSource)} and its return value is just
 * the buf argument.<p>
 *
 * The given buffer may have compressed data, and if so, the data is uncompressed
 * in placed back into the same buffer. If, however, the given buffer does not have
 * enough space for the uncompressed data, a new buffer is internally allocated,
 * data is placed in the new buffer, and the new buffer is the return value.<p>
 *
 * @param buf  ByteBuffer to be read
 * @return buf arg if data is not compressed. If compressed and buf does not have the
 *         necessary space to contain all uncompressed data, a new buffer is allocated,
 *         filled, and returned.
 * @throws EvioException if buf arg is null, buffer too small,
 *                       not in the proper format, or earlier than version 6
 */
std::shared_ptr<ByteBuffer> & Reader::setCompressedBuffer(std::shared_ptr<ByteBuffer> & buf) {
    setBuffer(buf);
    return buffer;
}

/**
 * This method can be used to avoid creating additional Reader
 * objects by reusing this one with another buffer. If the given buffer has
 * uncompressed data, this method becomes equivalent
 * to {@link #setBuffer(ByteBuffer, EvioNodeSource)} and its return value is just
 * the buf argument.<p>
 *
 * The given buffer may have compressed data, and if so, the data is uncompressed
 * in placed back into the same buffer. If, however, the given buffer does not have
 * enough space for the uncompressed data, a new buffer is internally allocated,
 * data is placed in the new buffer, and the new buffer is the return value.<p>
 *
 * @param buf  ByteBuffer to be read
 * @param pool pool of EvioNode objects to use when parsing buf.
 * @return buf arg if data is not compressed. If compressed and buf does not have the
 *         necessary space to contain all uncompressed data, a new buffer is allocated,
 *         filled, and returned.
 * @throws EvioException if buf arg is null, buffer too small,
 *                       not in the proper format, or earlier than version 6
 */
std::shared_ptr<ByteBuffer> & Reader::setCompressedBuffer(std::shared_ptr<ByteBuffer> & buf,
                                                          EvioNodeSource & pool) {
    setBuffer(buf, pool);
    return buffer;
}

/**
 * Get the name of the file being read.
 * @return name of the file being read or null if none.
 */
string Reader::getFileName() const {return fileName;}

/**
 * Get the size of the file being read, in bytes.
 * @return size of the file being read, in bytes, or 0 if none.
 */
long Reader::getFileSize() const {return fileSize;}

/**
 * Get the buffer being read, if any.
 * @return buffer being read, if any.
 */
std::shared_ptr<ByteBuffer> & Reader::getBuffer() {return buffer;}

/**
 * Get the beginning position of the buffer being read.
 * @return the beginning position of the buffer being read.
 */
int Reader::getBufferOffset() const {return bufferOffset;}

/**
 * Get the file header from reading a file.
 * @return file header from reading a file.
 */
FileHeader & Reader::getFileHeader() {return fileHeader;}

/**
 * Get the first record header from reading a file/buffer.
 * @return first record header from reading a file/buffer.
 */
std::shared_ptr<RecordHeader> & Reader::getFirstRecordHeader() {return firstRecordHeader;}

/**
 * Get the byte order of the file/buffer being read.
 * @return byte order of the file/buffer being read.
 */
ByteOrder & Reader::getByteOrder() {return byteOrder;}

/**
 * Set the byte order of the file/buffer being read.
 * @param order byte order of the file/buffer being read.
 */
void Reader::setByteOrder(ByteOrder & order) {byteOrder = order;}

/**
 * Get the Evio format version number of the file/buffer being read.
 * @return Evio format version number of the file/buffer being read.
 */
int Reader::getVersion() const {return evioVersion;}

/**
 * Is the data in the file/buffer compressed?
 * @return true if data is compressed.
 */
bool Reader::isCompressed() const {return compressed;}

/**
 * Get the XML format dictionary if there is one.
 * @return XML format dictionary, else null.
 */
string Reader::getDictionary() {
    // Read in dictionary if necessary
    extractDictionaryAndFirstEvent();
    return dictionaryXML;
}

/**
 * Does this evio file/buffer have an associated XML dictionary?
 * @return <code>true</code> if this evio file/buffer has an associated XML dictionary,
 *         else <code>false</code>.
 */
bool Reader::hasDictionary() const {
    if (fromFile) {
        return fileHeader.hasDictionary();
    }
    return firstRecordHeader->hasDictionary();
}

/**
 * Get a byte array representing the first event.
 * @return byte array representing the first event. Null if none.
 */
shared_ptr<uint8_t> & Reader::getFirstEvent() {
    // Read in first event if necessary
    extractDictionaryAndFirstEvent();
    return firstEvent;
}

/**
 * Get size, in bytes, of byte array representing the first event.
 * @return size, in bytes, of byte array representing the first event. 0 if none.
 */
uint32_t Reader::getFirstEventSize() {
    // Read in first event if necessary
    extractDictionaryAndFirstEvent();
    return firstEventSize;
}

/**
 * Does this evio file/buffer have an associated first event?
 * @return <code>true</code> if this evio file/buffer has an associated first event,
 *         else <code>false</code>.
 */
bool Reader::hasFirstEvent() const {
    if (fromFile) {
        return fileHeader.hasFirstEvent();
    }
    return firstRecordHeader->hasFirstEvent();
}

/**
 * Get the number of events in file/buffer.
 * @return number of events in file/buffer.
 */
uint32_t Reader::getEventCount() const {return eventIndex.getMaxEvents();}

/**
 * Get the number of records read from the file/buffer.
 * @return number of records read from the file/buffer.
 */
uint32_t Reader::getRecordCount() const {return recordPositions.size();}

/**
 * Returns a reference to the list of record positions in the file.
 * @return
 */
vector<Reader::RecordPosition> & Reader::getRecordPositions() {return recordPositions;}

/**
 * Get a reference to the list of EvioNode objects contained in the buffer being read.
 * To be used internally to evio.
 * @return list of EvioNode objects contained in the buffer being read.
 */
vector<std::shared_ptr<EvioNode>> & Reader::getEventNodes() {return eventNodes;}

/**
 * Get whether or not record numbers are enforced to be sequential.
 * @return {@code true} if record numbers are enforced to be sequential.
 */
bool Reader::getCheckRecordNumberSequence() const {return checkRecordNumberSequence;}

/**
 * Get the number of events remaining in the file/buffer.
 * Useful only if doing a sequential read.
 *
 * @return number of events remaining in the file/buffer
 */
uint32_t Reader::getNumEventsRemaining() const {return (eventIndex.getMaxEvents() - sequentialIndex);}

// Methods for current record

/**
 * Get a byte array representing the next event from the file/buffer while sequentially reading.
 * If the previous call was to {@link #getPrevEvent}, this will get the event
 * past what that returned. Once the last event is returned, this will return null.
 * @param len pointer to int that gets filled with the returned event's len in bytes.
 * @return byte array representing the next event or null if there is none.
 * @throws EvioException if file/buffer not in hipo format
 */
shared_ptr<uint8_t> Reader::getNextEvent(uint32_t * len) {
    bool debug = false;

    // If the last method called was getPrev, not getNext,
    // we don't want to get the same event twice in a row, so
    // increment index. Take into account if this is the
    // first time getNextEvent or getPrevEvent called.
    if (sequentialIndex < 0) {
        sequentialIndex = 0;
        if (debug) cout << "getNextEvent first time index set to " << sequentialIndex << endl;
    }
        // else if last call was to getPrevEvent ...
    else if (!lastCalledSeqNext) {
        sequentialIndex++;
        if (debug) cout << "getNextEvent extra increment to " << sequentialIndex << endl;
    }

    auto array = getEvent(sequentialIndex++, len);
    lastCalledSeqNext = true;

    if (array == nullptr) {
        if (debug) cout << "getNextEvent hit limit at index " << (sequentialIndex - 1) <<
                        ", set to " << (sequentialIndex - 1) << endl << endl;
        sequentialIndex--;
    }
    else {
        if (debug) cout << "getNextEvent got event " << (sequentialIndex - 1) << endl << endl;
    }

    return array;
}

/**
 * Get a byte array representing the previous event from the sequential queue.
 * If the previous call was to {@link #getNextEvent}, this will get the event
 * previous to what that returned. If this is called before getNextEvent,
 * it will always return null. Once the first event is returned, this will
 * return null.
 * @return byte array representing the previous event or null if there is none.
 * @throws EvioException if the file/buffer is not in HIPO format
 */
shared_ptr<uint8_t> Reader::getPrevEvent(uint32_t * len) {
    bool debug = false;

    // If the last method called was getNext, not getPrev,
    // we don't want to get the same event twice in a row, so
    // decrement index. Take into account if this is the
    // first time getNextEvent or getPrevEvent called.
    if (sequentialIndex < 0) {
        if (debug) cout << "getPrevEvent first time index = " << sequentialIndex << endl;
    }
        // else if last call was to getNextEvent ...
    else if (lastCalledSeqNext) {
        sequentialIndex--;
        if (debug) cout << "getPrevEvent extra decrement to " << sequentialIndex << endl;
    }

    auto array = getEvent(--sequentialIndex, len);
    lastCalledSeqNext = false;

    if (array == nullptr) {
        if (debug) cout << "getPrevEvent hit limit at index " << sequentialIndex <<
                        ", set to " << (sequentialIndex + 1) << endl << endl;
        sequentialIndex++;
    }
    else {
        if (debug) cout << "getPrevEvent got event " << (sequentialIndex) << endl << endl;
    }

    return array;
}

/**
 * Get an EvioNode representing the next event from the buffer while sequentially reading.
 * Calling this and calling {@link #getNextEvent()} have the same effect in terms of
 * advancing the same internal counter.
 * If the previous call was to {@link #getPrevEvent}, this will get the event
 * past what that returned. Once the last event is returned, this will return null.
 *
 * @return EvioNode representing the next event or null if no more events,
 *         reading a file or data is compressed.
 */
    std::shared_ptr<EvioNode> Reader::getNextEventNode() {
    if (sequentialIndex >= eventIndex.getMaxEvents() || fromFile || compressed) {
        return nullptr;
    }

    if (sequentialIndex < 0) {
        sequentialIndex = 0;
    }
        // else if last call was to getPrevEvent ...
    else if (!lastCalledSeqNext) {
        sequentialIndex++;
    }

    lastCalledSeqNext = true;
    return eventNodes[sequentialIndex++];
}

/**
 * Reads user header of the file header/first record header of buffer.
 * The returned ByteBuffer also contains endianness of the file/buffer.
 * @return ByteBuffer containing the user header of the file/buffer.
 * @throws IOException if error reading file
 */
ByteBuffer Reader::readUserHeader() {

    if (fromFile) {
        int userLen = fileHeader.getUserHeaderLength();
        // cout << "  " << fileHeader.getUserHeaderLength() << "  " << fileHeader.getHeaderLength() <<
        //         "  " << fileHeader.getIndexLength() << endl;

        // This is turned into shared memory in ByteBuffer constructor below
        auto userBytes = new char[userLen];

        inStreamRandom.seekg(fileHeader.getHeaderLength() + fileHeader.getIndexLength());
        inStreamRandom.read(userBytes, userLen);
        // This is a local object and will go out of scope! Copying ByteBuffer is necessary,
        // but data is in shared_ptr and doesn't get copied since it's moved.
        return std::move(ByteBuffer(userBytes, userLen).order(fileHeader.getByteOrder()));
    }
    else {
        int userLen = firstRecordHeader->getUserHeaderLength();
        // cout << "  " << firstRecordHeader->getUserHeaderLength() << "  " << firstRecordHeader->getHeaderLength() <<
        //         "  " << firstRecordHeader->getIndexLength() << endl;
        auto userBytes = new uint8_t[userLen];

        buffer->position(firstRecordHeader->getHeaderLength() + firstRecordHeader->getIndexLength());
        buffer->getBytes(userBytes, userLen);
        return std::move(ByteBuffer(userBytes, userLen).order(firstRecordHeader->getByteOrder()));
    }
}

/**
 * Get a byte array representing the specified event from the file/buffer.
 * If index is out of bounds, null is returned.
 * @param index index of specified event within the entire file/buffer,
 *              contiguous starting at 0.
 * @param len pointer to int that gets filled with the returned event's len in bytes.
 * @return byte array representing the specified event or null if
 *         index is out of bounds.
 * @throws EvioException if file/buffer not in hipo format
 */
shared_ptr<uint8_t> Reader::getEvent(uint32_t index, uint32_t * len) {

    if (index >= eventIndex.getMaxEvents()) {
        cout << "[READER] getEvent: index = " << index << ", max events = " << eventIndex.getMaxEvents() << endl;
        return nullptr;
    }

    if (eventIndex.setEvent(index)) {
        // If here, the event is in another record
        cout << "[READER] getEvent: read record at index = " << eventIndex.getRecordNumber() << endl;
        readRecord(eventIndex.getRecordNumber());
    }

    if (inputRecordStream.getEntries() == 0) {
        cout << "[READER] getEvent: first time reading record at index = " << eventIndex.getRecordNumber() << endl;
        readRecord(eventIndex.getRecordNumber());
    }

    cout << "[READER] getEvent: try doing inputStream.getEvent(...)" << endl;
    return inputRecordStream.getEvent(eventIndex.getRecordEventNumber(), len);
}

/**
 * Get a byte array representing the specified event from the file/buffer
 * and place it in the given buf.
 * If no buf is given (arg is null), create a buffer internally and return it.
 * If index is out of bounds, null is returned.
 * @param buf buffer in which to place event data.
 * @param index index of specified event within the entire file/buffer,
 *              contiguous starting at 0.
 * @return buf or null if buf is null or index out of bounds.
 * @throws EvioException if file/buffer not in hipo format,
 *                       if buf has insufficient space to contain event
 *                       (buf.capacity() < event size), or
 *                       index too large.
 */
ByteBuffer & Reader::getEvent(ByteBuffer & buf, uint32_t index) {

    if (index >= eventIndex.getMaxEvents()) {
        throw EvioException("index too large");
    }

    if (eventIndex.setEvent(index)) {
        // If here, the event is in the next record
        readRecord(eventIndex.getRecordNumber());
    }
    if (inputRecordStream.getEntries() == 0) {
        //cout << "[READER] first time reading buffer" << endl;
        readRecord(eventIndex.getRecordNumber());
    }
    return inputRecordStream.getEvent(buf, eventIndex.getRecordEventNumber());
}

/**
 * Returns the length of the event with given index.
 * @param index index of the event
 * @return length of the data in bytes or zero if index
 *         does not correspond to a valid event.
 */
uint32_t Reader::getEventLength(uint32_t index) {

    if (index >= eventIndex.getMaxEvents()) {
        //cout << "[READER] getEventLength: index = " << index << ", max events = " << eventIndex.getMaxEvents() << endl;
        return 0;
    }

    if (eventIndex.setEvent(index)) {
        // If here, the event is in the next record
        //cout << "[READER] getEventLength: read record" << endl;
        readRecord(eventIndex.getRecordNumber());
    }
    if (inputRecordStream.getEntries() == 0) {
        // First time reading buffer
        //cout << "[READER] getEventLength: first time reading record" << endl;
        readRecord(eventIndex.getRecordNumber());
    }
    return inputRecordStream.getEventLength(eventIndex.getRecordEventNumber());
}

/**
 * Get an EvioNode representing the specified event from the buffer.
 * If index is out of bounds, nullptr is returned.
 * @param index index of specified event within the entire buffer,
 *              starting at 0.
 * @return EvioNode representing the specified event or null if
 *         index is out of bounds, reading a file or data is compressed.
 * @throws EvioException index too large or reading from file.
 */
std::shared_ptr<EvioNode>  Reader::getEventNode(uint32_t index) {
//cout << "getEventNode: index = " << index + " >? " << eventIndex.getMaxEvents() <<
//                   ", fromFile = " << fromFile << ", compressed = " << compressed << endl;
    if (index >= eventIndex.getMaxEvents() || fromFile) {
//cout << "getEventNode: index out of range, from file or compressed so node = NULL" << endl;
        throw EvioException("index too large or reading from file");
    }
//cout << "getEventNode: Getting node at index = " << index << endl;
    return eventNodes[index];
}

/**
 * Checks if the file has an event to read next.
 * @return true if the next event is available, false otherwise
 */
bool Reader::hasNext() const {return eventIndex.canAdvance();}

/**
 * Checks if the stream has previous event to be accessed through, getPrevEvent()
 * @return true if previous event is accessible, false otherwise
 */
bool Reader::hasPrev() const {return eventIndex.canRetreat();}

/**
 * Get the number of events in current record.
 * @return number of events in current record.
 */
uint32_t Reader::getRecordEventCount() const {return inputRecordStream.getEntries();}

/**
 * Get the index of the current record.
 * @return index of the current record.
 */
uint32_t Reader::getCurrentRecord() const {return currentRecordLoaded;}

/**
 * Get the current record stream.
 * @return current record stream.
 */
RecordInput & Reader::getCurrentRecordStream() {return inputRecordStream;}

/**
 * Reads record from the file/buffer at the given record index.
 * @param index record index  (starting at 0).
 * @return true if valid index and successful reading record, else false.
 * @throws EvioException if file/buffer not in hipo format
 */
bool Reader::readRecord(uint32_t index) {
    cout << "Reader.readRecord:  index = " << index << ", recPos.size() = " << recordPositions.size() << endl;

    if (index < recordPositions.size()) {
        RecordPosition pos = recordPositions[index];
        if (fromFile) {
            cout << "Reader.readRecord:  inputRecStream.readRecord(...)" << endl;
            inputRecordStream.readRecord(inStreamRandom, pos.getPosition());
        }
        else {
            inputRecordStream.readRecord(*(buffer.get()), pos.getPosition());
        }
        currentRecordLoaded = index;
        return true;
    }
    return false;
}


/** Extract dictionary and first event from file/buffer if possible, else do nothing. */
void Reader::extractDictionaryAndFirstEvent() {
    // If already read & parsed ...
    if (dictionaryXML.length() > 0 || firstEvent != nullptr) {
        return;
    }

    if (fromFile) {
        extractDictionaryFromFile();
        return;
    }
    extractDictionaryFromBuffer();
}

/** Extract dictionary and first event from buffer if possible, else do nothing. */
void Reader::extractDictionaryFromBuffer() {

    // If no dictionary or first event ...
    if (!firstRecordHeader->hasDictionary() && !firstRecordHeader->hasFirstEvent()) {
        return;
    }

    int userLen = firstRecordHeader->getUserHeaderLength();
    // 8 byte min for evio event, more for xml dictionary
    if (userLen < 8) {
        return;
    }

    RecordInput record;

    try {
        // Position right before record header's user header
        buffer->position(bufferOffset +
                         firstRecordHeader->getHeaderLength() +
                         firstRecordHeader->getIndexLength());
        // Read user header
        auto userBytes = new uint8_t[userLen];
        buffer->getBytes(userBytes, userLen);
        ByteBuffer userBuffer(userBytes, userLen);

        // Parse user header as record
        record = RecordInput(firstRecordHeader->getByteOrder());
        record.readRecord(userBuffer, 0);
    }
    catch (EvioException & e) {
        // Not in proper format
        return;
    }

    int evIndex = 0;
    uint32_t len;

    // Dictionary always comes first in record
    if (firstRecordHeader->hasDictionary()) {
        // Just plain ascii, not evio format
        auto dict = record.getEvent(evIndex++, & len);
        dictionaryXML = string(reinterpret_cast<const char *>(dict.get()), len);
    }

    // First event comes next
    if (firstRecordHeader->hasFirstEvent()) {
        firstEvent = record.getEvent(evIndex, &len);
        firstEventSize = len;
    }
}

/** Extract dictionary and first event from file if possible, else do nothing. */
void Reader::extractDictionaryFromFile() {
//cout << "extractDictionaryFromFile: IN, hasFirst = " << fileHeader.hasFirstEvent() << endl;

    // If no dictionary or first event ...
    if (!fileHeader.hasDictionary() && !fileHeader.hasFirstEvent()) {
        return;
    }

    uint32_t userLen = fileHeader.getUserHeaderLength();
    // 8 byte min for evio event, more for xml dictionary
    if (userLen < 8) {
        return;
    }

    RecordInput record;

    try {
//cout << "extractDictionaryFromFile: Read " << userLen << " bytes for record" << endl;
        // Position right before file header's user header
        inStreamRandom.seekg(fileHeader.getHeaderLength() + fileHeader.getIndexLength());
        // Read user header
        auto userBytes = new char[userLen];
        inStreamRandom.read(userBytes, userLen);
        // userBytes will be made into a shared pointer in next line
        ByteBuffer userBuffer(userBytes, userLen);
        // Parse user header as record
        record = RecordInput(fileHeader.getByteOrder());
        record.readRecord(userBuffer, 0);
    }
    catch (ifstream::failure & e) {
        // Can't read file
        return;
    }
    catch (EvioException & e) {
        // Not in proper format
        return;
    }

    int evIndex = 0;
    uint32_t len;

    // Dictionary always comes first in record
    if (fileHeader.hasDictionary()) {
        // Just plain ascii, not evio format,  dict of type shared_ptr<uint8_t>
        auto dict = record.getEvent(evIndex++, &len);
//cout << "extractDictionaryFromFile: dictionary len  " << len << " bytes" << endl;
        dictionaryXML = string(reinterpret_cast<const char *>(dict.get()), len);
    }

    // First event comes next
    if (fileHeader.hasFirstEvent()) {
        firstEvent = record.getEvent(evIndex, &len);
        firstEventSize = len;
    }
}

//-----------------------------------------------------------------

/**
 * Reads data from a record header in order to determine things
 * like the bitInfo word, various lengths, etc.
 * Does <b>not</b> change the position or limit of buffer.
 *
 * @param  buf     buffer containing evio header.
 * @param  offset  byte offset into buffer.
 * @param  info    array in which to store header info. Elements are:
 *  <ol start="0">
 *      <li>bit info word</li>
 *      <li>record length in bytes (inclusive)</li>
 *      <li>compression type</li>
 *      <li>header length in bytes</li>
 *      <li>index array length in bytes</li>
 *      <li>user header length in bytes</li>
 *      <li>uncompressed data length in bytes (w/o record header)</li>
 *  </ol>
 * @throws underflow_error if not enough data in buffer.
 * @throws EvioException null info arg or info.length &lt; 7.
 */
void Reader::findRecordInfo(ByteBuffer & buf, uint32_t offset, uint32_t* info, uint32_t infoLen) {

    if (info == nullptr || infoLen < 7) {
        throw EvioException("null info arg or info length < 7");
    }
//        if (buf.capacity() - offset < 1000) {
//            cout << "findRecInfo: buf cap = " << buf.capacity() << ", offset = " << offset <<
//                                       ", lim = " << buf.limit() << endl;
//        }
    // Have enough bytes to read 10 words of header?
    if ((buf.capacity() - offset) < 40) {
        cout << "findRecInfo: buf cap = " << buf.capacity() << ", offset = " << offset << ", lim = " <<  buf.limit() << endl;
        throw underflow_error("not enough data in buffer to read record header");
    }

    info[0] = buf.getInt(offset + RecordHeader::BIT_INFO_OFFSET);
    info[1] = buf.getInt(offset + RecordHeader::RECORD_LENGTH_OFFSET) * 4;
    info[2] =(buf.getInt(offset + RecordHeader::COMPRESSION_TYPE_OFFSET) >> 28) & 0xf;
    info[3] = buf.getInt(offset + RecordHeader::HEADER_LENGTH_OFFSET) * 4;
    info[4] = buf.getInt(offset + RecordHeader::INDEX_ARRAY_OFFSET);
    info[5] = buf.getInt(offset + RecordHeader::USER_LENGTH_OFFSET);
    info[6] = buf.getInt(offset + RecordHeader::UNCOMPRESSED_LENGTH_OFFSET);
}

/**
 * This method gets the total number of evio/hipo format bytes in
 * the given buffer, both compressed and uncompressed. Results are
 * stored in the given int array. First element is compressed length,
 * second is uncompressed length.
 *
 * @param buf   ByteBuffer containing evio/hipo data
 * @param info  integer array containing evio/hipo data. Elements are:
 *  <ol start="0">
 *      <li>compressed length in bytes</li>
 *      <li>uncompressed length in bytes</li>
 *  </ol>
 * @param infoLen number of elements in info array.
 * @return total compressed hipo/evio data in bytes.
 * @throws underflow_error if not enough data in buffer.
 * @throws EvioException null info arg or infoLen &lt; 7.
 */
int Reader::getTotalByteCounts(ByteBuffer & buf, uint32_t* info, uint32_t infoLen) {

    if (info == nullptr || infoLen < 7) {
        throw EvioException("bad arg or infoLen < 7");
    }

    int offset = buf.position();
    int totalCompressed = 0;
    int recordBytes, totalBytes = 0;

    do {
        // Look at the record
        findRecordInfo(buf, offset, info, infoLen);

        // Total uncompressed length of record
        recordBytes = info[3] + info[4] + info[5] + info[6];

        // Track total uncompressed & compressed sizes
        totalBytes += recordBytes;
        totalCompressed += info[1];

        // Hop over record
        offset += info[1];

    } while (!RecordHeader::isLastRecord((uint32_t)info[0])); // Go to the next record if any

    // No longer input, we now use array for output
    info[0] = totalCompressed;
    info[1] = totalBytes;

    return totalCompressed;
}

// TODO: THIS method is inefficient as it copies data once too many times!!! FIX IT!!!

/**
 * This method scans a buffer to find all records and store their position, length,
 * and event count. It also finds all events and creates & stores their associated
 * EvioNode objects.<p>
 *
 * The difficulty with doing this is that the buffer may contain compressed data.
 * It must then be uncompressed into a different buffer. There are 2 possibilities.
 * First, if the buffer being parsed is too small to hold its uncompressed form,
 * then a new, larger buffer is created, filled with the uncompressed data and then
 * given as the return value of this method. Second, if the buffer being parsed is
 * large enough to hold its uncompressed form, the data is uncompressed into a
 * temporary holding buffer. When all decompresssion and parsing is finished, the
 * contents of the temporary buffer are copied back into the original buffer which
 * then becomes the return value.
 *
 * @return buffer containing uncompressed data. This buffer may be different than the
 *         one originally scanned if the data was compressed and the uncompressed length
 *         is greater than the original buffer could hold.
 * @throws EvioException if buffer not in the proper format or earlier than version 6;
 *                       if checkRecordNumberSequence is true and records are out of sequence.
 * @throws BufferUnderflowException if not enough data in buffer.
 */
ByteBuffer Reader::scanBuffer() {

    ByteBuffer & buf = *(buffer.get());

    // Quick check to see if data in buffer is compressed (pos/limit unchanged)
    if (!RecordHeader::isCompressed(buf, bufferOffset)) {
        // Since data is not compressed ...
        scanUncompressedBuffer();
        return buf;
    }

    // The previous method call will set the endianness of the buffer properly.
    // Hop through ALL RECORDS to find their total lengths. This does NOT
    // change pos/limit of buffer.
    int totalCompressedBytes = getTotalByteCounts(buf, headerInfo, headerInfoLen);
    int totalUncompressedBytes = headerInfo[1];

    std::shared_ptr<ByteBuffer> bigEnoughBuf;
    bool useTempBuffer = false;

//cout << "scanBuffer: total Uncomp bytes = " << totalUncompressedBytes <<
//                " >? cap - off = " << (buffer.capacity() - bufferOffset) << endl;

    // If the buffer is too small to hold the expanded data, create one that isn't
    if (totalUncompressedBytes > (buf.capacity() - bufferOffset)) {
        // Time for a bigger buffer. Give buffer an extra 4KB, backed by array
        bigEnoughBuf = std::make_shared<ByteBuffer>(totalUncompressedBytes + bufferOffset + 4096);
        // Put stuff starting at bigEnoughBuf->position() = bufferOffset
        //bigEnoughBuf->order(buffer.order()).limit(bufferOffset + totalUncompressedBytes).position(bufferOffset);
        bigEnoughBuf->order(buf.order()).position(bufferOffset);

        // Copy in stuff up to offset
        std::memcpy((void *)(bigEnoughBuf->array()),
                    (const void *)(buf.array() + buf.arrayOffset()), bufferOffset);
    }
    else {
        // "buffer" is big enough to hold everything. However, we need another buffer
        // in which to temporarily decompress data which will then be copied back into
        // buffer. Don't bother to copy stuff from buffer.pos = 0 - bufferOffset, since
        // we'll be copying stuff back into buffer anyway.
        useTempBuffer = true;
        if (tempBuffer == nullptr) {
            tempBuffer = std::make_shared<ByteBuffer>(totalUncompressedBytes + bufferOffset + 4096);
        }
        else if (tempBuffer->capacity() < totalUncompressedBytes + bufferOffset) {
            tempBuffer->expand(totalUncompressedBytes + bufferOffset + 4096);
        }
        tempBuffer->order(buf.order()).limit(tempBuffer->capacity()).position(0);

        bigEnoughBuf = tempBuffer;
        // Put stuff starting at bigEnoughBuf->position() = 0.
    }

    bool haveFirstRecordHeader = false;

    RecordHeader recordHeader(HeaderType::EVIO_RECORD);

    // Start at the buffer's initial position
    int position  = bufferOffset;
    int recordPos = bufferOffset;
    int bytesLeft = totalUncompressedBytes;

//cout << "  scanBuffer: buffer pos = " << bufferOffset << ", bytesLeft = " << bytesLeft << endl;
    // Keep track of the # of records, events, and valid words in file/buffer
    int eventCount = 0, byteLen, recordBytes, eventsInRecord;
    eventNodes.clear();
    recordPositions.clear();
    eventIndex.clear();
    // TODO: this should NOT change in records in 1 buffer, only BETWEEN buffers!!!!!!!!!!!!
    recordNumberExpected = 1;

    // Go through data record-by-record
    do {
        // If this is not the first record anymore, then the limit of bigEnoughBuf,
        // set above, may not be big enough.

        // Uncompress record in buffer and place into bigEnoughBuf
        int origRecordBytes = RecordInput::uncompressRecord(
                buf, recordPos, *(bigEnoughBuf.get()), recordHeader);

        // The only certainty at this point about pos/limit is that
        // bigEnoughBuf->position = after header/index/user, just before data.
        // What exactly the decompression methods do is unknown.

        // uncompressRecord(), above, read the header. Save the first header.
        if (!haveFirstRecordHeader) {
            // First time through, save byte order and version
            byteOrder = recordHeader.getByteOrder();
            buf.order(byteOrder);
            evioVersion = recordHeader.getVersion();
            firstRecordHeader = std::make_shared<RecordHeader>(recordHeader);
            compressed = recordHeader.getCompressionType() != Compressor::UNCOMPRESSED;
            haveFirstRecordHeader = true;
        }

        //System.out.println("read header ->\n" + recordHeader);

        if (checkRecordNumberSequence) {
            if (recordHeader.getRecordNumber() != recordNumberExpected) {
                //cout << "  scanBuffer: record # out of sequence, got " << recordHeader.getRecordNumber() <<
                //                   " expecting " << recordNumberExpected << endl;
                throw EvioException("bad record # sequence");
            }
            recordNumberExpected++;
        }

        // Check to see if the whole record is there
        if (recordHeader.getLength() > bytesLeft) {
            cout << "    record size = " << recordHeader.getLength() << " >? bytesLeft = " << bytesLeft <<
                 ", pos = " << buf.position() << endl;
            throw EvioException("Bad hipo format: not enough data to read record");
        }

        // Header is now describing the uncompressed buffer, bigEnoughBuf
        recordBytes = recordHeader.getLength();
        eventsInRecord = recordHeader.getEntries();
        // Create a new RecordPosition object and store in vector
        recordPositions.emplace_back(position, recordBytes, eventsInRecord);
        // Track # of events in this record for event index handling
        eventIndex.addEventSize(eventsInRecord);

        // Next record position
        recordPos += origRecordBytes;

        // How many bytes left in the newly expanded buffer
        bytesLeft -= recordHeader.getUncompressedRecordLength();

        // After calling uncompressRecord(), bigEnoughBuf will be positioned
        // right before where the events start.
        position = bigEnoughBuf->position();

        // For each event in record, store its location
        for (int i=0; i < eventsInRecord; i++) {
            std::shared_ptr<EvioNode> node;
//System.out.println("      try extracting event " + i + ", pos = " + position +
//                                               ", place = " + (eventCount + i));
            node = EvioNode::extractEventNode(bigEnoughBuf, nodePool, 0,
                                              position, eventCount + i);
//cout << "      event " << i << ", pos = " << node.getPosition() <<
//                           ", dataPos = " << node.getDataPosition() << ", ev # = " << (eventCount + i + 1) << endl;
            eventNodes.push_back(node);

            // Hop over event
            byteLen   = node->getTotalBytes();
            position += byteLen;

            if (byteLen < 8) {
                throw EvioException("Bad evio format: bad bank length");
            }
//cout << "        hopped event " << i << ", bytesLeft = " << bytesLeft << ", pos = " << position << endl << endl;
        }

        bigEnoughBuf->position(position);
        eventCount += eventsInRecord;

        // If the evio buffer was written with the DAQ Java evio library,
        // there is only 1 record with event per buffer -- the first record.
        // It's followed by a trailer.

        // Read the next record if this is not the last one and there's enough data to
        // read a header.

    } while (!recordHeader.isLastRecord() && bytesLeft >= RecordHeader::HEADER_SIZE_BYTES);


    // At this point we have an uncompressed buffer in bigEnoughBuf.
    // If that is our temporary buf, we now copy it back into buffer
    // which we know will be big enough to handle it.
    if (useTempBuffer) {
        // Since we're using a temp buffer, it does NOT contain buffer's data
        // from position = 0 to bufferOffset.
        std::memcpy((void *)(buf.array() + bufferOffset + buf.arrayOffset()),
                    (const void *)(bigEnoughBuf->array()), totalUncompressedBytes);

        // Restore the original position and set new limit
        buf.limit(bufferOffset + totalUncompressedBytes).position(bufferOffset);

        // We've copied data from one buffer to another,
        // so adjust the nodes to compensate.
        for (auto & n : eventNodes) {
            n->shift(bufferOffset).setBuffer(buffer);
        }
    }
    else {
        // We had to allocate memory in this method since buffer was too small,
        // so return the new larger buffer.
        bigEnoughBuf->position(bufferOffset);
        return *(bigEnoughBuf.get());
    }

    return buf;
}

/**
  * Scan buffer to find all records and store their position, length, and event count.
  * Also finds all events and creates & stores their associated EvioNode objects.
  * @throws EvioException if buffer too small, not in the proper format, or earlier than version 6;
  *                       if checkRecordNumberSequence is true and records are out of sequence.
  */
void Reader::scanUncompressedBuffer() {

    auto headerBytes = new uint8_t[RecordHeader::HEADER_SIZE_BYTES];
    ByteBuffer headerBuffer(headerBytes, RecordHeader::HEADER_SIZE_BYTES);
    RecordHeader recordHeader;

    bool haveFirstRecordHeader = false;

    // Start at the buffer's initial position
    int position  = bufferOffset;
    int bytesLeft = bufferLimit - bufferOffset;

//cout << "  scanBuffer: buffer pos = " << bufferOffset << ", bytesLeft = " << bytesLeft << endl;
    // Keep track of the # of records, events, and valid words in file/buffer
    int eventCount = 0, byteLen, recordBytes, eventsInRecord, recPosition;
    eventNodes.clear();
    recordPositions.clear();
    eventIndex.clear();
    // TODO: this should NOT change in records in 1 buffer, only BETWEEN buffers!!!!!!!!!!!!
    recordNumberExpected = 1;

    while (bytesLeft >= RecordHeader::HEADER_SIZE_BYTES) {
        // Read record header
        buffer->position(position);
        // This moves the buffer's position
        buffer->getBytes(headerBytes, RecordHeader::HEADER_SIZE_BYTES);
        // Only sets the byte order of headerBuffer
        recordHeader.readHeader(headerBuffer);
//cout << "read header ->" << endl << recordHeader.toString() << endl;

        if (checkRecordNumberSequence) {
            if (recordHeader.getRecordNumber() != recordNumberExpected) {
//cout << "  scanBuffer: record # out of sequence, got " << recordHeader.getRecordNumber() <<
//        " expecting " << recordNumberExpected << endl;
                throw EvioException("bad record # sequence");
            }
            recordNumberExpected++;
        }

        // Save the first record header
        if (!haveFirstRecordHeader) {
            // First time through, save byte order and version
            byteOrder = recordHeader.getByteOrder();
            buffer->order(byteOrder);
            evioVersion = recordHeader.getVersion();
            firstRecordHeader = std::make_shared<RecordHeader>(recordHeader);
            compressed = recordHeader.getCompressionType() != Compressor::UNCOMPRESSED;
            haveFirstRecordHeader = true;
        }

        // Check to see if the whole record is there
        if (recordHeader.getLength() > bytesLeft) {
            cout << "    record size = " << recordHeader.getLength() << " >? bytesLeft = " << bytesLeft <<
                 ", pos = " << buffer->position() << endl;
            throw EvioException("Bad hipo format: not enough data to read record");
        }

        //cout << ">>>>>==============================================" << endl;
        //cout << recordHeader.toString() << endl;
        recordBytes = recordHeader.getLength();
        eventsInRecord = recordHeader.getEntries();
        recPosition = position;
//cout << " RECORD HEADER ENTRIES = " << eventsInRecord << endl;
        recordPositions.emplace_back(position, recordBytes, eventsInRecord);
        // Track # of events in this record for event index handling
        eventIndex.addEventSize(eventsInRecord);

        // Hop over record header, user header, and index to events
        byteLen = recordHeader.getHeaderLength() +
                  recordHeader.getUserHeaderLength() +
                  recordHeader.getIndexLength();
        position  += byteLen;
        bytesLeft -= byteLen;

        // Do this because extractEventNode uses the buffer position
        buffer->position(position);
//cout << "    hopped to data, pos = " << position << endl;

        // For each event in record, store its location
        for (int i=0; i < eventsInRecord; i++) {
            std::shared_ptr<EvioNode> node;
//cout << "      try extracting event " << i << " in record pos = " << recPosition <<
//        ", pos = " << position << ", place = " << (eventCount + i) << endl;
            node = EvioNode::extractEventNode(buffer, nodePool, recPosition,
                                              position, eventCount + i);
//cout << "      event " << i << " in record: pos = " << node.getPosition() <<
//        ", dataPos = " << node.getDataPosition() << ", ev # = " << (eventCount + i + 1) << endl;
            eventNodes.push_back(node);

            // Hop over event
            byteLen    = node->getTotalBytes();
            position  += byteLen;
            bytesLeft -= byteLen;

            if (byteLen < 8 || bytesLeft < 0) {
                throw EvioException("Bad evio format: bad bank length");
            }

//cout << "        hopped event " << i << ", bytesLeft = " << bytesLeft << ", pos = " << position << endl << endl;
        }

        eventCount += eventsInRecord;
    }

    buffer->position(bufferOffset);
}


/**
 * Scan file to find all records and store their position, length, and event count.
 * Safe to call this method successively.
 * @throws IOException   if error reading file
 * @throws EvioException if file is not in the proper format or earlier than version 6;
 *                       if checkRecordNumberSequence is true and records are out of sequence.
 */
void Reader::forceScanFile() {

    cout << "[READER] ---> force a file scan" << endl;

    auto headerBytes = new char[RecordHeader::HEADER_SIZE_BYTES];
    ByteBuffer headerBuffer(headerBytes, RecordHeader::HEADER_SIZE_BYTES);

    // Read and parse file header even if we have already done so in scanFile()
    fileHeader = FileHeader();
    // Go to file beginning
    inStreamRandom.seekg(0L);
    inStreamRandom.read(headerBytes, RecordHeader::HEADER_SIZE_BYTES);
    // headerBuffer position does not change in following call
    fileHeader.readHeader(headerBuffer);
    byteOrder = fileHeader.getByteOrder();
    evioVersion = fileHeader.getVersion();
    cout << "forceScanFile: file header -->" << endl << fileHeader.toString() << endl;

    int recordLen;
    eventIndex.clear();
    recordPositions.clear();
    recordNumberExpected = 1;
    RecordHeader recordHeader;
    bool haveFirstRecordHeader = false;

    cout << "forceScanFile: 1" << endl;
    // Scan file by reading each record header and
    // storing its position, length, and event count.

    // Don't go beyond 1 header length before EOF since we'll be reading in 1 header
    uint64_t maximumSize = fileSize - RecordHeader::HEADER_SIZE_BYTES;

    // First record position (past file's header + index + user header)
    uint64_t recordPosition = fileHeader.getHeaderLength() +
                              fileHeader.getUserHeaderLength() +
                              fileHeader.getIndexLength() +
                              fileHeader.getUserHeaderLengthPadding();

    cout << "forceScanFile: 2, file user header padding = " << fileHeader.getUserHeaderLengthPadding() << endl;
    int recordCount = 0;
    while (recordPosition < maximumSize) {
        cout << "forceScanFile: 3" << endl;
        inStreamRandom.seekg(recordPosition);
        inStreamRandom.read(headerBytes, RecordHeader::HEADER_SIZE_BYTES);
        cout << "forceScanFile: 4" << endl;
        recordHeader.readHeader(headerBuffer);
        cout << "forceScanFile: record header " << recordCount << " @ pos = " <<
             recordPosition << " -->" << endl << recordHeader.toString() << endl;
        recordCount++;


        // Checking record # sequence does NOT make sense when reading a file.
        // It only makes sense when reading from a stream and checking to see
        // if the record id, set by the sender, is sequential.
        // So feature turned off if reading from file.
        if (checkRecordNumberSequence) {
            if (recordHeader.getRecordNumber() != recordNumberExpected) {
                cout << "forceScanFile: record # out of sequence, got " << recordHeader.getRecordNumber() <<
                     " expecting " << recordNumberExpected << endl;

                throw EvioException("bad record # sequence");
            }
            recordNumberExpected++;
        }

        // Save the first record header
        if (!haveFirstRecordHeader) {
            firstRecordHeader = std::make_shared<RecordHeader>(recordHeader);
            compressed = firstRecordHeader->getCompressionType() != Compressor::UNCOMPRESSED;
            haveFirstRecordHeader = true;
        }

        recordLen = recordHeader.getLength();
        // Create a new RecordPosition object and store in vector
        recordPositions.emplace_back(recordPosition, recordLen, recordHeader.getEntries());
        // Track # of events in this record for event index handling
        eventIndex.addEventSize(recordHeader.getEntries());
        recordPosition += recordLen;
    }
    eventIndex.show();
    cout << "NUMBER OF RECORDS " << recordPositions.size() << endl;
}

/**
 * Scans the file to index all the record positions.
 * It takes advantage of any existing indexes in file.
 * @param force if true, force a file scan even if header
 *              or trailer have index info.
 * @throws IOException   if error reading file
 * @throws EvioException if file is not in the proper format or earlier than version 6
 */
void Reader::scanFile(bool force) {

    if (force) {
        forceScanFile();
        return;
    }

    eventIndex.clear();
    recordPositions.clear();
//        recordNumberExpected = 1;

    cout << "[READER] ---> scanning the file" << endl;
    auto headerBytes = new char[FileHeader::HEADER_SIZE_BYTES];
    ByteBuffer headerBuffer(headerBytes, FileHeader::HEADER_SIZE_BYTES);

    fileHeader = FileHeader();
    RecordHeader recordHeader;

    // Go to file beginning
    inStreamRandom.seekg(0L);

    // Read and parse file header
    inStreamRandom.read(headerBytes, FileHeader::HEADER_SIZE_BYTES);
    fileHeader.readHeader(headerBuffer);
    byteOrder = fileHeader.getByteOrder();
    evioVersion = fileHeader.getVersion();
    cout << "scanFile: file header: " << endl << fileHeader.toString() << endl;

    // Is there an existing record length index?
    // Index in trailer gets first priority.
    // Index in file header gets next priority.
    bool fileHasIndex = fileHeader.hasTrailerWithIndex() || (fileHeader.hasIndex());
    cout << "scanFile: file has index = " << fileHasIndex <<
         ", has trailer with index =  " << fileHeader.hasTrailerWithIndex() <<
         ", file header has index " << fileHeader.hasIndex() << endl;

    // If there is no index, scan file
    if (!fileHasIndex) {
        cout << "scanFile: CALL forceScanFile" << endl;
        forceScanFile();
        return;
    }

    // If we're using the trailer, make sure it's position is valid,
    // (ie 0 is NOT valid).
    bool useTrailer = fileHeader.hasTrailerWithIndex();
    if (useTrailer) {
        // If trailer position is NOT valid ...
        if (fileHeader.getTrailerPosition() < 1) {
            cout << "scanFile: bad trailer position, " << fileHeader.getTrailerPosition() << endl;
            if (fileHeader.hasIndex()) {
                // Use file header index if there is one
                useTrailer = false;
            }
            else {
                // Scan if no viable index exists
                forceScanFile();
                return;
            }
        }
    }

    // First record position (past file's header + index + user header)
    uint32_t recordPosition = fileHeader.getLength();
    cout << "scanFile: record position = " << recordPosition << endl;

    // Move to first record and save the header
    inStreamRandom.seekg(recordPosition);
    inStreamRandom.read(headerBytes, RecordHeader::HEADER_SIZE_BYTES);
    firstRecordHeader = std::make_shared<RecordHeader>(recordHeader);
    firstRecordHeader->readHeader(headerBuffer);
    compressed = firstRecordHeader->getCompressionType() != Compressor::UNCOMPRESSED;

    int indexLength;

    // If we have a trailer with indexes ...
    if (useTrailer) {
        // Position read right before trailing header
        inStreamRandom.seekg(fileHeader.getTrailerPosition());
        cout << "scanFile: position file to trailer = " << fileHeader.getTrailerPosition() << endl;
        // Read trailer
        inStreamRandom.read(headerBytes, RecordHeader::HEADER_SIZE_BYTES);
        recordHeader.readHeader(headerBuffer);
        indexLength = recordHeader.getIndexLength();
    }
    else {
        // Move back to immediately past file header
        // while taking care of non-standard size
        inStreamRandom.seekg(fileHeader.getHeaderLength());
        // Index immediately follows file header in this case
        indexLength = fileHeader.getIndexLength();
    }

    // Read indexes
    auto index = new char[indexLength];
    inStreamRandom.read(index, indexLength);
    // Turn bytes into record lengths & event counts
    auto intData = new uint32_t[indexLength/4];

    try {
        cout << "scanFile: transform int array from " << fileHeader.getByteOrder().getName() << endl;
        Util::toIntArray(index, indexLength, fileHeader.getByteOrder(), intData);

        // Turn record lengths into file positions and store in list
        recordPositions.clear();
        for (int i=0; i < indexLength/4; i += 2) {
            uint32_t len = intData[i];
            uint32_t count = intData[i+1];
            cout << "scanFile: record pos = " << recordPosition << ", len = " << len << ", count = " << count << endl;
            // Create a new RecordPosition object and store in vector
            recordPositions.emplace_back(recordPosition, len, count);
            // Track # of events in this record for event index handling
            cout << "scanFile: add record's event count (" << count << ") to eventIndex" << endl;
            eventIndex.addEventSize(count);
            recordPosition += len;
        }
    }
    catch (EvioException & e) {/* never happen */}

    delete[](index);
    delete[](intData);
}


/**
 * This method removes the data, represented by the given node, from the buffer.
 * It also marks all nodes taken from that buffer as obsolete.
 * They must not be used anymore.<p>
 *
 * @param  removeNode  evio structure to remove from buffer
 * @return ByteBuffer updated to reflect the node removal
 * @throws EvioException if object closed;
 *                       if node was not found in any event;
 *                       if internal programming error;
 *                       if buffer has compressed data;
 */
std::shared_ptr<ByteBuffer> & Reader::removeStructure(std::shared_ptr<EvioNode> & removeNode) {

    if (closed) {
        throw EvioException("object closed");
    }
    else if (removeNode.isObsolete()) {
        //cout << "removeStructure: node has already been removed" << endl;
        return buffer;
    }

    if (firstRecordHeader->getCompressionType() != Compressor::UNCOMPRESSED) {
        throw EvioException("cannot remove node from buffer of compressed data");
    }

    bool foundNode = false;

    // Locate the node to be removed ...
    for (auto & ev : eventNodes) {
        // See if it's an event ...
        if (removeNode == ev) {
            foundNode = true;
            break;
        }

        for (shared_ptr<EvioNode> const & nd : ev->getAllNodes()) {
            // The first node in allNodes is the event node
            if (&removeNode == nd.get()) {
                foundNode = true;
                break;
            }
        }

        if (foundNode) {
            break;
        }
    }

    if (!foundNode) {
        throw EvioException("removeNode not found in any event");
    }

    // The data these nodes represent will be removed from the buffer,
    // so the node will be obsolete along with all its descendants.
    removeNode.setObsolete(true);

    //---------------------------------------------------
    // Remove structure. Keep using current buffer.
    // We'll move all data that came after removed node
    // to where removed node used to be.
    //---------------------------------------------------

    // Amount of data being removed
    uint32_t removeDataLen = removeNode.getTotalBytes();

    // Just after removed node (start pos of data being moved)
    uint32_t startPos = removeNode.getPosition() + removeDataLen;

    // Duplicate buffer shares data, but we need to copy it so use copy constructor.
    ByteBuffer moveBuffer(*(buffer.get()));
    // Prepare to move data currently sitting past the removed node
    moveBuffer.limit(bufferLimit).position(startPos);

    // Set place to put the data being moved - where removed node starts
    buffer->position(removeNode.getPosition());
    // Copy it over
    buffer->put(moveBuffer);

    // Reset some buffer values
    buffer->position(bufferOffset);
    bufferLimit -= removeDataLen;
    buffer->limit(bufferLimit);

    // Reduce lengths of parent node
    removeNode.getParentNode()->updateLengths(-removeDataLen);

    // Reduce containing record's length
    uint32_t pos = removeNode.getRecordPosition();
    // Header length in words
    uint32_t oldLen = 4*buffer->getInt(pos);
    buffer->putInt(pos, (oldLen - removeDataLen)/4);
    // Uncompressed data length in bytes
    oldLen = buffer->getInt(pos + RecordHeader::UNCOMPRESSED_LENGTH_OFFSET);
    buffer->putInt(pos + RecordHeader::UNCOMPRESSED_LENGTH_OFFSET, oldLen - removeDataLen);

    // Invalidate all nodes obtained from the last buffer scan
    for (auto & ev : eventNodes) {
        ev->setObsolete(true);
    }

    // Now the evio data in buffer is in a valid state so rescan buffer to update everything
    scanBuffer();

    return buffer;
}


/**
 * This method adds an evio container (bank, segment, or tag segment) as the last
 * structure contained in an event. It is the responsibility of the caller to make
 * sure that the buffer argument contains valid evio data (only data representing
 * the structure to be added - not in file format with record header and the like)
 * which is compatible with the type of data stored in the given event.<p>
 *
 * To produce such evio data use {@link EvioBank#write(ByteBuffer)},
 * {@link EvioSegment#write(ByteBuffer)} or
 * {@link EvioTagSegment#write(ByteBuffer)} depending on whether
 * a bank, seg, or tagseg is being added.<p>
 *
 * The given buffer argument must be ready to read with its position and limit
 * defining the limits of the data to copy.
 *
 * @param eventNumber number of event to which addBuffer is to be added
 * @param addBuffer buffer containing evio data to add (<b>not</b> evio file format,
 *                  i.e. no record headers)
 * @return a new ByteBuffer object which is created and filled with all the data
 *         including what was just added.
 * @throws EvioException if eventNumber out of bounds;
 *                       if addBuffer arg is empty or has non-evio format;
 *                       if addBuffer is opposite endian to current event buffer;
 *                       if added data is not the proper length (i.e. multiple of 4 bytes);
 *                       if the event number does not correspond to an existing event;
 *                       if there is an internal programming error;
 *                       if object closed
 */
std::shared_ptr<ByteBuffer> & Reader::addStructure(uint32_t eventNumber, ByteBuffer & addBuffer) {

    if (addBuffer.remaining() < 8) {
        throw EvioException("empty or non-evio format buffer arg");
    }

    if (addBuffer.order() != byteOrder) {
        throw EvioException("trying to add wrong endian buffer");
    }

    if (eventNumber < 1 || eventNumber > eventNodes.size()) {
        throw EvioException("event number out of bounds");
    }

    if (closed) {
        throw EvioException("object closed");
    }

    auto & eventNode = eventNodes[eventNumber - 1];

    // Position in byteBuffer just past end of event
    uint32_t endPos = eventNode->getDataPosition() + 4*eventNode->getDataLength();

    // How many bytes are we adding?
    size_t appendDataLen = addBuffer.remaining();

    // Make sure it's a multiple of 4
    if (appendDataLen % 4 != 0) {
        throw EvioException("data added is not in evio format");
    }

    //--------------------------------------------
    // Add new structure to end of specified event
    //--------------------------------------------

    // Create a new buffer
    std::shared_ptr<ByteBuffer> newBuffer = std::make_shared<ByteBuffer>(bufferLimit - bufferOffset + appendDataLen);
    newBuffer->order(byteOrder);

    // Copy beginning part of existing buffer into new buffer
    buffer->limit(endPos).position(bufferOffset);
    newBuffer->put(buffer);

    // Copy new structure into new buffer
    newBuffer->put(addBuffer);

    // Copy ending part of existing buffer into new buffer
    buffer->limit(bufferLimit).position(endPos);
    newBuffer->put(buffer);

    // Get new buffer ready for reading
    newBuffer->flip();
    bufferOffset = 0;
    bufferLimit  = newBuffer->limit();
    buffer = newBuffer;

    // Increase lengths of parent nodes
    auto & addToNode = eventNodes[eventNumber];
    auto parent = addToNode->getParentNode();
    parent->updateLengths(appendDataLen);

    // Increase containing record's length
    uint32_t pos = addToNode->getRecordPosition();
    // Header length in words
    uint32_t oldLen = 4*buffer->getInt(pos);
    buffer->putInt(pos, (oldLen + appendDataLen)/4);
    // Uncompressed data length in bytes
    oldLen = buffer->getInt(pos + RecordHeader::UNCOMPRESSED_LENGTH_OFFSET);
    buffer->putInt(pos + RecordHeader::UNCOMPRESSED_LENGTH_OFFSET, oldLen + appendDataLen);

    // Invalidate all nodes obtained from the last buffer scan
    for (auto ev : eventNodes) {
        ev->setObsolete(true);
    }

    // Now the evio data in buffer is in a valid state so rescan buffer to update everything
    scanBuffer();

    return buffer;
}


void Reader::show() const {
    cout << " ***** FILE: (info), RECORDS = " << recordPositions.size() << " *****" << endl;
    for (RecordPosition entry : this->recordPositions) {
        cout << entry.toString();
    }
}

int Reader::main(int argc, char **argv) {

    try {
        string filename = "/tmp/filename";
        Reader reader(filename, true);

        int icounter = 0;
        //reader.show();
        while (reader.hasNext()) {
            cout << " reading event # " << icounter << endl;
            try {
                uint32_t len;
                shared_ptr<uint8_t> event = reader.getNextEvent(&len);
            } catch (EvioException &ex) {
                //Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
            }

            icounter++;
        }

        //reader.open("test.evio");
//        reader.readRecord(0);
//        uint32_t nevents = reader.getRecordEventCount();
//        cout << "-----> events = " << nevents << endl;
//        for (int i = 0; i < 10; i++) {
//            shared_ptr<uint8_t> event = reader.getEvent(i);
//            uint32_t eventLen = reader.getEventLength(i);
//            cout << "---> events length = " << eventLen << endl;
//            ByteBuffer buf(reinterpret_cast<char *>(event.get()), eventLen);
//            buf.order(ByteOrder::ENDIAN_LITTLE);
//            string data = Reader::getStringArray(buf, 10, 30);
//            cout << data << endl;
//        }
    }
    catch (exception & e) {
        cout << "error = " << string(e.what()) << endl;
    }

    return 0;
}

/**
 *
 * @param buffer
 * @param wrap
 * @return
 */
string Reader::getStringArray(ByteBuffer & buffer, int wrap, int max) {
    stringstream ss;
    int limit = buffer.limit();
    int counter = 1;
    for (int i = 0; i < limit; i+=4){
        int32_t value = buffer.getInt(i);
        ss << setw(10) << Reader::getHexStringInt(value);
        if( (counter)%wrap==0) ss << endl;
        counter++;
        if(counter>max) break;
    }
    return ss.str();
}


string Reader::getHexStringInt(int32_t value) {
    stringstream ss;
    ss << hex << setw(8) << value;
    return ss.str();
}

}


