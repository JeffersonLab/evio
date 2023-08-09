//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "Reader.h"


namespace evio {


    /**
     * Default constructor. Does nothing.
     * The {@link #open(std::string const &, bool)} method has to be called to open the input stream.
     * Also {@link #forceScanFile()} needs to be called to find records.
     */
    Reader::Reader() {}


    /**
     * Constructor with filename. Creates instance and opens
     * the input stream with given name. Uses existing indexes
     * in file before scanning.
     * @param filename input file name.
     * @throws IOException   if error reading file
     * @throws EvioException if file is not in the proper format or earlier than version 6
     */
    Reader::Reader(std::string const & filename) {
        open(filename, true);
    }


    /**
     * Constructor with filename. Creates instance and opens
     * the input stream with given name.
     * @param filename input file name.
     * @param forceScan if true, force a scan of file, else use existing indexes first.
     * @throws IOException   if error reading file
     * @throws EvioException if file is not in the proper format or earlier than version 6
     */
    Reader::Reader(std::string const & filename, bool forceScan) {
        open(filename, false);
        scanFile(forceScan);
    }


    /**
     * Constructor for reading buffer with evio data.
     * Buffer must be ready to read with position and limit set properly.
     * If the given buffer contains compressed data, it is uncompressed into another buffer.
     * The buffer containing the newly uncompressed data then becomes the internal buffer of
     * this object. It can be obtained by calling {@link #getBuffer}.
     *
     * @param buffer buffer with evio data.
     * @param checkRecordNumSeq if true, check to see if all record numbers are in order,
     *                          if not throw exception.
     * @throws EvioException if buffer too small, not in the proper format, or earlier than version 6;
     *                       if checkRecordNumSeq is true and records are out of sequence.
     */
    Reader::Reader(std::shared_ptr<ByteBuffer> & buffer, bool checkRecordNumSeq) {
        this->buffer = buffer;
        bufferOffset = buffer->position();
        bufferLimit  = buffer->limit();
        byteOrder = buffer->order();
        fromFile = false;
        checkRecordNumberSequence = checkRecordNumSeq;

        auto bb = scanBuffer();
        if (compressed) {
            // scanBuffer() will uncompress all data in buffer
            // and store it in the returned buffer (bb).
            // Make that our internal buffer so we don't have to do any more uncompression.
            this->buffer = bb;
            compressed = false;
        }
    }


    /**
     * Opens an input stream in binary mode. Scans for
     * records in the file and stores record information
     * in internal array. Each record can be read from the file.
     * @param filename input file name
     * @param scan if true, call scanFile(false).
     * @throws EvioException if error handling file
     */
    void Reader::open(std::string const & filename, bool scan) {
        // Throw exception if logical or read/write error on io operation
        inStreamRandom.exceptions(std::ifstream::failbit | std::ifstream::badbit);

        try {
            if (inStreamRandom.is_open()) {
//std::cout << "[READER] ---> closing current file : " << fileName << std::endl;
                inStreamRandom.close();
            }

            // This may be called after using a buffer as input, so zero some things out
            buffer = nullptr;
            bufferOffset = 0;
            bufferLimit  = 0;
            fromFile = true;

            fileName = filename;

//std::cout << "[READER] ---> opening file : " << filename << std::endl;
            // "ate" mode flag will go immediately to file's end (do this to get its size)
            inStreamRandom.open(filename, std::ios::binary | std::ios::ate);

            fileSize = inStreamRandom.tellg();
            // Go back to beginning of file
            inStreamRandom.seekg(0);
            fromFile = true;
            if (scan) {
                scanFile(false);
            }
        }
        catch (std::exception & e) {
            throw EvioException(e.what());
        }
    }


    /** This closes the file.  */
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
     * {@link #setBuffer(std::shared_ptr<ByteBuffer> &)})?
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
     * If the given buffer contains compressed data, it is uncompressed into another buffer.
     * The buffer containing the newly uncompressed data then becomes the internal buffer of
     * this object. It can be obtained by calling {@link #getBuffer}.
     *
     * @param buf ByteBuffer to be read
     * @throws underflow_error if not enough data in buffer.
     * @throws EvioException if buf arg is null,
     *                       not in the proper format, or earlier than version 6
     */
    void Reader::setBuffer(std::shared_ptr<ByteBuffer> & buf) {

        if (buf == nullptr) {
            throw EvioException("null buf arg");
        }

        // Possible no-arg constructor set this to true, change it now
        fromFile = false;

        close();

        buffer       = buf;
        bufferLimit  = buffer->limit();
        bufferOffset = buffer->position();
        byteOrder    = buffer->order();

        eventIndex.clear();
        eventNodes.clear();
        recordPositions.clear();

        compressed = false;
        firstEvent = nullptr;
        dictionaryXML.clear();
        // TODO: set to -1 ???
        sequentialIndex = 0;
        if (firstRecordHeader != nullptr) {
            firstRecordHeader->reset();
        }
        currentRecordLoaded = 0;

        auto bb = scanBuffer();
        if (compressed) {
            // scanBuffer() will uncompress all data in buffer
            // and store it in the returned buffer (bb).
            // Make that our internal buffer so we don't have to do any more uncompression.
            this->buffer = bb;
            compressed = false;
        }

        closed = false;
    }


    /**
     * Get the name of the file being read.
     * @return name of the file being read or null if none.
     */
    std::string Reader::getFileName() const {return fileName;}


    /**
     * Get the size of the file being read, in bytes.
     * @return size of the file being read, in bytes, or 0 if none.
     */
    size_t Reader::getFileSize() const {return fileSize;}


    /**
     * Get the buffer being read, if any.
     * This may not be the buffer given in the constructor or in {@link #setBuffer} if
     * the original data was compressed. All data in the returned buffer is uncompressed.
     * @return buffer being read, if any.
     */
    std::shared_ptr<ByteBuffer> Reader::getBuffer() {return buffer;}


    /**
     * Get the beginning position of the buffer being read.
     * @return the beginning position of the buffer being read.
     */
    size_t Reader::getBufferOffset() const {return bufferOffset;}


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
    uint32_t Reader::getVersion() const {return evioVersion;}


    /**
     * Is the data in the file/buffer compressed?
     * @return true if data is compressed.
     */
    bool Reader::isCompressed() const {return compressed;}


    /**
     * Does this file/buffer contain non-evio format events?
     * @return true if all events are in evio format, else false.
     */
    bool Reader::isEvioFormat() const {return evioFormat;}


    /**
     * Get the XML format dictionary if there is one.
     * @return XML format dictionary, else null.
     */
    std::string Reader::getDictionary() {
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
     * @param size pointer filled with the size, in bytes, of the first event (0 if none).
     *             If null, this is ignored.
     * @return byte array representing the first event. Null if none.
     */
    std::shared_ptr<uint8_t> & Reader::getFirstEvent(uint32_t *size) {
        // Read in first event if necessary
        extractDictionaryAndFirstEvent();
        if (size != nullptr) {
            *size = firstEventSize;
        }
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
    std::vector<Reader::RecordPosition> & Reader::getRecordPositions() {return recordPositions;}


    /**
     * Get a reference to the list of EvioNode objects contained in the buffer being read.
     * To be used internally to evio.
     * @return list of EvioNode objects contained in the buffer being read.
     */
    std::vector<std::shared_ptr<EvioNode>> & Reader::getEventNodes() {return eventNodes;}


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
    std::shared_ptr<uint8_t> Reader::getNextEvent(uint32_t * len) {

        // If the last method called was getPrev, not getNext,
        // we don't want to get the same event twice in a row, so
        // increment index. Take into account if this is the
        // first time getNextEvent or getPrevEvent called.
        if (sequentialIndex < 0) {
            sequentialIndex = 0;
//std::cout << "getNextEvent first time index set to " << sequentialIndex << std::endl;
        }
        // else if last call was to getPrevEvent ...
        else if (!lastCalledSeqNext) {
            sequentialIndex++;
//std::cout << "getNextEvent extra increment to " << sequentialIndex << std::endl;
        }

        auto array = getEvent(sequentialIndex++, len);
        lastCalledSeqNext = true;

        if (array == nullptr) {
//std::cout << "getNextEvent hit limit at index " << (sequentialIndex - 1) <<
//             ", set to " << (sequentialIndex - 1) << std::endl << std::endl;
            sequentialIndex--;
        }
//        else {
//            if (debug) std::cout << "getNextEvent got event " << (sequentialIndex - 1) << std::endl << std::endl;
//        }

        return array;
    }


    /**
     * Get a byte array representing the previous event from the sequential queue.
     * If the previous call was to {@link #getNextEvent}, this will get the event
     * previous to what that returned. If this is called before getNextEvent,
     * it will always return null. Once the first event is returned, this will
     * return null.
     * @param len pointer to int that gets filled with the returned event's len in bytes.
     * @return byte array representing the previous event or null if there is none.
     * @throws EvioException if the file/buffer is not in HIPO format
     */
    std::shared_ptr<uint8_t> Reader::getPrevEvent(uint32_t * len) {

        // If the last method called was getNext, not getPrev,
        // we don't want to get the same event twice in a row, so
        // decrement index. Take into account if this is the
        // first time getNextEvent or getPrevEvent called.
        if (sequentialIndex < 0) {
//std::cout << "getPrevEvent first time index = " << sequentialIndex << std::endl;
        }
        // else if last call was to getNextEvent ...
        else if (lastCalledSeqNext) {
            sequentialIndex--;
//std::cout << "getPrevEvent extra decrement to " << sequentialIndex << std::endl;
        }

        auto array = getEvent(--sequentialIndex, len);
        lastCalledSeqNext = false;

        if (array == nullptr) {
//std::cout << "getPrevEvent hit limit at index " << sequentialIndex <<
//             ", set to " << (sequentialIndex + 1) << std::endl << std::endl;
            sequentialIndex++;
        }
//        else {
//            std::cout << "getPrevEvent got event " << (sequentialIndex) << std::endl << std::endl;
//        }

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
    std::shared_ptr<ByteBuffer> Reader::readUserHeader() {

        if (fromFile) {
            int userLen = fileHeader.getUserHeaderLength();
// std::cout << "  " << fileHeader.getUserHeaderLength() << "  " << fileHeader.getHeaderLength() <<
//         "  " << fileHeader.getIndexLength() << std::endl;

            // This is turned into shared memory in ByteBuffer constructor below
            auto userBytes = new char[userLen];

            inStreamRandom.seekg(fileHeader.getHeaderLength() + fileHeader.getIndexLength());
            inStreamRandom.read(userBytes, userLen);
            // This is a local object and will go out of scope! Copying ByteBuffer is necessary,
            // but data is in shared_ptr and doesn't get copied since it's moved.
            auto buf = std::make_shared<ByteBuffer>(userBytes, userLen);
            buf->order(fileHeader.getByteOrder());
            return buf;
        }
        else {
            int userLen = firstRecordHeader->getUserHeaderLength();
// std::cout << "  " << firstRecordHeader->getUserHeaderLength() << "  " << firstRecordHeader->getHeaderLength() <<
//              "  " << firstRecordHeader->getIndexLength() << std::endl;
            auto userBytes = new uint8_t[userLen];

            buffer->position(firstRecordHeader->getHeaderLength() + firstRecordHeader->getIndexLength());
            buffer->getBytes(userBytes, userLen);

            auto buf = std::make_shared<ByteBuffer>(userBytes, userLen);
            buf->order(firstRecordHeader->getByteOrder());
            return buf;
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
    std::shared_ptr<uint8_t> Reader::getEvent(uint32_t index, uint32_t * len) {

        if (index >= eventIndex.getMaxEvents()) {
//std::cout << "[READER] getEvent: index = " << index << ", max events = " << eventIndex.getMaxEvents() << std::endl;
            return nullptr;
        }

        if (eventIndex.setEvent(index)) {
            // If here, the event is in another record
//std::cout << "[READER] getEvent: read record at index = " << eventIndex.getRecordNumber() << std::endl;
            readRecord(eventIndex.getRecordNumber());
        }

        if (inputRecordStream.getEntries() == 0) {
//std::cout << "[READER] getEvent: first time reading record at index = " << eventIndex.getRecordNumber() << std::endl;
            readRecord(eventIndex.getRecordNumber());
        }

//std::cout << "[READER] getEvent: try doing inputStream.getEvent(" << eventIndex.getRecordEventNumber() << ")" << std::endl;
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
     * @return buf.
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
            //std::cout << "[READER] first time reading buffer" << std::endl;
            readRecord(eventIndex.getRecordNumber());
        }
        return inputRecordStream.getEvent(buf, eventIndex.getRecordEventNumber());
    }


    /**
     * Get a byte array representing the specified event from the file/buffer
     * and place it in the given buf.
     * If no buf is given (arg is null), create a buffer internally and return it.
     * If index is out of bounds, null is returned.
     * @param buf buffer in which to place event data.
     * @param index index of specified event within the entire file/buffer,
     *              contiguous starting at 0.
     * @return buf or null if buf is null.
     * @throws EvioException if file/buffer not in hipo format,
     *                       if buf has insufficient space to contain event
     *                       (buf.capacity() < event size), or
     *                       index too large.
     */
    std::shared_ptr<ByteBuffer> Reader::getEvent(std::shared_ptr<ByteBuffer> & buf, uint32_t index) {
        if (buf == nullptr) return nullptr;
        getEvent(*(buf.get()), index);
        return buf;
    }


    /**
     * Returns the length of the event with given index.
     * @param index index of the event
     * @return length of the data in bytes or zero if index
     *         does not correspond to a valid event.
     */
    uint32_t Reader::getEventLength(uint32_t index) {

        if (index >= eventIndex.getMaxEvents()) {
//std::cout << "[READER] getEventLength: index = " << index << ", max events = " << eventIndex.getMaxEvents() << std::endl;
            return 0;
        }

        if (eventIndex.setEvent(index)) {
            // If here, the event is in the next record
//std::cout << "[READER] getEventLength: read record" << std::endl;
            readRecord(eventIndex.getRecordNumber());
        }
        if (inputRecordStream.getEntries() == 0) {
            // First time reading buffer
//std::cout << "[READER] getEventLength: first time reading record" << std::endl;
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
//std::cout << "     getEventNode: index = " << index << " >? " << eventIndex.getMaxEvents() <<
//             ", fromFile = " << fromFile << ", compressed = " << compressed << std::endl;
        if (index >= eventIndex.getMaxEvents() || fromFile) {
            throw EvioException("index too large or reading from file");
        }
//std::cout << "     getEventNode: Getting node at index = " << index << std::endl;
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
//std::cout << "Reader.readRecord:  index = " << index << ", recPos.size() = " << recordPositions.size() <<
// ", rec pos = " << recordPositions[index].getPosition() << std::endl;

        if (index < recordPositions.size()) {
            size_t pos = recordPositions[index].getPosition();
            if (fromFile) {
                inputRecordStream.readRecord(inStreamRandom, pos);
            }
            else {
                inputRecordStream.readRecord(*(buffer.get()), pos);
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

//std::cout << "extractDictionaryFromBuffer: IN, hasFirst = " << fileHeader.hasFirstEvent() <<
//             ", hasDict = " << fileHeader.hasDictionary() << std::endl;

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
            dictionaryXML = std::string(reinterpret_cast<const char *>(dict.get()), len);
        }

        // First event comes next
        if (firstRecordHeader->hasFirstEvent()) {
            firstEvent = record.getEvent(evIndex, &len);
            firstEventSize = len;
        }
    }


    /** Extract dictionary and first event from file if possible, else do nothing. */
    void Reader::extractDictionaryFromFile() {
//std::cout << "extractDictionaryFromFile: IN, hasFirst = " << fileHeader.hasFirstEvent() << std::endl;

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
//std::cout << "extractDictionaryFromFile: Read " << userLen << " bytes for record" << std::endl;
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
        catch (std::ifstream::failure & e) {
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
//std::cout << "extractDictionaryFromFile: dictionary len  " << len << " bytes" << std::endl;
            dictionaryXML = std::string(reinterpret_cast<const char *>(dict.get()), len);
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
     *      <li>user header length in bytes (no padding included) </li>
     *      <li>uncompressed data length in bytes (no padding, w/o record header)</li>
     *  </ol>
     * @param  infoLen  len in 32-bit words of array at info.
     * @throws underflow_error if not enough data in buffer.
     * @throws EvioException null info arg or info.length &lt; 7.
     */
    void Reader::findRecordInfo(std::shared_ptr<ByteBuffer> & buf, uint32_t offset,
                                uint32_t* info, uint32_t infoLen) {
        findRecordInfo(*(buf.get()), offset, info, infoLen);
    }


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
     *      <li>user header length in bytes (no padding included) </li>
     *      <li>uncompressed data length in bytes (no padding, w/o record header)</li>
     *  </ol>
     * @param  infoLen  len in 32-bit words of array at info.
     * @throws underflow_error if not enough data in buffer.
     * @throws EvioException null info arg or info.length &lt; 7.
     */
    void Reader::findRecordInfo(ByteBuffer & buf, uint32_t offset, uint32_t* info, uint32_t infoLen) {

        if (info == nullptr || infoLen < 7) {
            throw EvioException("null info arg or info length < 7");
        }

        //        if (buf.capacity() - offset < 1000) {
        //            std::cout << "findRecInfo: buf cap = " << buf.capacity() << ", offset = " << offset <<
        //                                       ", lim = " << buf.limit() << std::endl;
        //        }

        // Have enough bytes to read 10 words of header?
        if ((buf.capacity() - offset) < 40) {
std::cout << "findRecInfo: buf cap = " << buf.capacity() << ", offset = " << offset << ", lim = " <<  buf.limit() << std::endl;
            throw std::underflow_error("not enough data in buffer to read record header");
        }

        info[0] = buf.getInt(offset + RecordHeader::BIT_INFO_OFFSET);
        info[1] = buf.getInt(offset + RecordHeader::RECORD_LENGTH_OFFSET) * 4;
        info[2] =(buf.getInt(offset + RecordHeader::COMPRESSION_TYPE_OFFSET) >> 28) & 0xf;
        info[3] = buf.getInt(offset + RecordHeader::HEADER_LENGTH_OFFSET) * 4;
        info[4] = buf.getInt(offset + RecordHeader::INDEX_ARRAY_OFFSET);
        info[5] = buf.getInt(offset + RecordHeader::USER_LENGTH_OFFSET);
        info[6] = buf.getInt(offset + RecordHeader::UNCOMPRESSED_LENGTH_OFFSET);
        info[7] = buf.getInt(offset + RecordHeader::EVENT_COUNT_OFFSET);

//        std::cout << "findRecInfo: record len bytes = " <<  info[1] << ", header len bytes = " << info[3] <<
//                  ", index len = " << info[4] <<   ", user len = " << info[5] << std::endl;

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
     *      <li>compressed length in bytes (padded) </li>
     *      <li>uncompressed length in bytes (padded) </li>
     *  </ol>
     * @param infoLen number of elements in info array.
     * @return total uncompressed hipo/evio data in bytes (padded).
     * @throws underflow_error if not enough data in buffer.
     * @throws EvioException null info arg or infoLen &lt; 7.
     */
    uint32_t Reader::getTotalByteCounts(ByteBuffer & buf, uint32_t* info, uint32_t infoLen) {

        if (info == nullptr || infoLen < 7) {
            throw EvioException("bad arg or infoLen < 7");
        }

        size_t offset = buf.position();
        uint32_t totalCompressed = 0;
        uint32_t totalBytes = 0;

        while (true) {
            // Look at the record
            findRecordInfo(buf, offset, info, infoLen);

            // Total uncompressed length of record (with padding)
            totalBytes += info[3] + info[4] +
                          4*Util::getWords(info[5]) + // user array + padding
                          4*Util::getWords(info[6]);  // uncompressed data + padding

            // Track total compressed size (padded)
            totalCompressed += info[1];

            // Hop over record
            offset += info[1];

            // Quit after last record
            if (RecordHeader::isLastRecord(info[0])) break;
        }

        // No longer input, we now use array for output
        info[0] = totalCompressed;
        info[1] = totalBytes;

        return totalBytes;
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
     *      <li>compressed length in bytes (padded) </li>
     *      <li>uncompressed length in bytes (padded) </li>
     *  </ol>
     * @param infoLen number of elements in info array.
     * @return total uncompressed hipo/evio data in bytes (padded).
     * @throws underflow_error if not enough data in buffer.
     * @throws EvioException null info arg or infoLen &lt; 7.
     */
    uint32_t Reader::getTotalByteCounts(std::shared_ptr<ByteBuffer> & buf, uint32_t* info, uint32_t infoLen) {
        return getTotalByteCounts(*(buf.get()), info, infoLen);
    }


    // TODO: THIS method is inefficient as it copies data once too many times!!! FIX IT!!!

    /**
     * This method scans a buffer to find all records and store their position, length,
     * and event count. It also finds all events and creates & stores their associated
     * EvioNode objects.
     * The difficulty with doing this is that the buffer may contain compressed data.
     * It must then be uncompressed into a different buffer.
     * Handles zero length index array length if buffer in evio format,
     * else parsing will cause issues later.
     *
     * @return buffer containing uncompressed data. This buffer is different than the internal
     *         buffer. Ready to read.
     * @throws EvioException if buffer not in the proper format or earlier than version 6;
     *                       if 4 * event count != index array len;
     *                       if checkRecordNumberSequence is true and records are out of sequence;
     *                       if index_array_len != 4*event_count.
     * @throws underflow_error if not enough data in buffer.
     */
    std::shared_ptr<ByteBuffer> Reader::scanBuffer() {

        ByteBuffer & buf = *(buffer.get());

        // Quick check to see if data in buffer is compressed (pos/limit unchanged)
        if (!RecordHeader::isCompressed(buf, bufferOffset)) {
            // Since data is not compressed ...
            scanUncompressedBuffer();
            return buffer;
        }

        compressed = true;

        // The previous method call will set the endianness of the buffer properly.
        // Hop through ALL RECORDS to find their total lengths. This does NOT
        // change pos/limit of buffer. Results returned in headerInfo[0] & [1].
        // All other values in headerInfo reflect the LAST record (ususally trailer).
        uint32_t headerInfo[headerInfoLen];
        uint32_t totalUncompressedBytes = getTotalByteCounts(buf, headerInfo, headerInfoLen); // padded

//std::cout << "  scanBuffer: total UNcompressed bytes = " << totalUncompressedBytes <<
//             " >? cap - off = " << (buf.capacity() - bufferOffset) << std::endl;

        // Create buffer big enough to hold everything
        auto bigEnoughBuf = std::make_shared<ByteBuffer>(totalUncompressedBytes + bufferOffset + 1024);
        bigEnoughBuf->order(buf.order()).position(bufferOffset);
        // Copy over everything up to the current offset
        std::memcpy((void *)(bigEnoughBuf->array()),(const void *)(buf.array() + buf.arrayOffset()), bufferOffset);

        bool haveFirstRecordHeader = false;

        RecordHeader recordHeader(HeaderType::EVIO_RECORD);
        // Start at the buffer's initial position
        size_t position  = bufferOffset;
        size_t recordPos = bufferOffset;
        ssize_t bytesLeft = totalUncompressedBytes;

//std::cout << "  scanBuffer: buffer pos = " << bufferOffset << ", bytesLeft = " << bytesLeft << std::endl;

        // Keep track of the # of records, events, and valid words in file/buffer.
        // eventPlace is the place of each event (evio or not) with repect to each other (0, 1, 2 ...)
        uint32_t eventPlace = 0, byteLen;
        eventNodes.clear();
        recordPositions.clear();
        eventIndex.clear();
        // TODO: this should NOT change in records in 1 buffer, only BETWEEN buffers!!!!!!!!!!!!
        recordNumberExpected = 1;

        // Go through data record-by-record
        do {
            // If this is not the first record anymore, then the limit of bigEnoughBuf,
            // set above, may not be big enough.

            // Uncompress record in buffer & place into bigEnoughBuf, then READ RECORD HEADER
            int origRecordBytes = RecordInput::uncompressRecord(
                    buf, recordPos, *(bigEnoughBuf.get()), recordHeader);

            // The only certainty at this point about pos/limit is that
            // bigEnoughBuf->position = after header/index/user, just before data.
            // What exactly the decompression methods do is unknown.

            // recordHeader is now describing the uncompressed buffer, bigEnoughBuf
            uint32_t eventCount = recordHeader.getEntries();
            uint32_t recordHeaderLen = recordHeader.getHeaderLength();
            uint32_t recordBytes = recordHeader.getLength();
            uint32_t indexArrayLen = recordHeader.getIndexLength();  // bytes

            // Consistency check, index array length reflects # of events properly?
            if (indexArrayLen > 0 && (indexArrayLen != 4*eventCount)) {
                throw EvioException("index array len (" + std::to_string(indexArrayLen) +
                    ") and 4*eventCount (" + std::to_string(4*eventCount) + ") contradict each other");
            }

            // uncompressRecord(), above, read the header. Save the first header.
            if (!haveFirstRecordHeader) {
                // First time through, save byte order and version
                byteOrder = recordHeader.getByteOrder();
                buf.order(byteOrder);
                bigEnoughBuf->order(byteOrder);
                evioVersion = recordHeader.getVersion();
                firstRecordHeader = std::make_shared<RecordHeader>(recordHeader);
                haveFirstRecordHeader = true;
            }

//std::cout << "  scanBuffer: read header ->" << std::endl << recordHeader.toString() << std::endl;

            if (checkRecordNumberSequence) {
                if (recordHeader.getRecordNumber() != recordNumberExpected) {
                    //std::cout << "  scanBuffer: record # out of sequence, got " << recordHeader.getRecordNumber() <<
                    //                   " expecting " << recordNumberExpected << std::endl;
                    throw EvioException("bad record # sequence");
                }
                recordNumberExpected++;
            }

            // Check to see if the whole record is there
            if (recordBytes > bytesLeft) {
                std::cout << "    record size = " << recordBytes << " >? bytesLeft = " << bytesLeft <<
                     ", pos = " << buf.position() << std::endl;
                throw std::underflow_error("Bad hipo format: not enough data to read record");
            }

            // Create a new RecordPosition object and store in vector
//std::cout << "  *** scanBuffer: record position saved as " << position << std::endl;
            recordPositions.emplace_back(position, recordBytes, eventCount);
            // Track # of events in this record for event index handling
            eventIndex.addEventSize(eventCount);

            // If existing, use each event size in index array
            // as a check against the lengths found in the
            // first word in evio banks (their len) - must be identical for evio.
            // Unlike previously, this code handles 0 length index array.
            uint32_t hdrEventLen = 0, evioEventLen = 0;
            // Position in buffer to start reading event lengths, if any ...
            uint32_t lenIndex = position + recordHeaderLen;
            bool haveHdrEventLengths = (indexArrayLen > 0) && (eventCount > 0);

            // How many bytes left in the newly expanded buffer
            bytesLeft -= recordHeader.getUncompressedRecordLength();

            // After calling uncompressRecord(), bigEnoughBuf will be positioned
            // right before where the events start.
            position = bigEnoughBuf->position();
//std::cout << "  *** scanBuffer: (right before event) set position to " << position << std::endl;

            // End position of record is right after event data including padding.
            // Remember it has been uncompressed.
            uint32_t recordEndPos = position + 4*recordHeader.getDataLengthWords();

            //-----------------------------------------------------
            // For each event in record, store its location.
            // THIS ONLY WORKS AND IS ONLY NEEDED FOR EVIO DATA!
            // So the question is, how do we know when we've got
            // evio data and when we don't?
            //-----------------------------------------------------
            for (int i=0; i < eventCount; i++) {
                // Is the length we get from the first word of an evio bank/event (bytes)
                // the same as the length we got from the record header? If not, it's not evio.

                // Assume it's evio unless proven otherwise.
                // If index array missing, evio is the only option!
                bool isEvio = true;
                evioEventLen = 4 * (bigEnoughBuf->getUInt(position) + 1);

                if (haveHdrEventLengths) {
                    hdrEventLen = bigEnoughBuf->getUInt(lenIndex);
                    isEvio = (evioEventLen == hdrEventLen);
                    lenIndex += 4;

//                    if (!isEvio) {
//                        std::cout << "  *** scanBuffer: event " << i << " not evio format" << std::endl;
//                        std::cout << "  *** scanBuffer:   evio event len = " << evioEventLen << std::endl;
//                        std::cout << "  *** scanBuffer:   hdr  event len = " << hdrEventLen << std::endl;
//                    }
                }
                else {
                    // Check event len validity on upper bound as much as possible.
                    // Cannot extend beyond end of record, taking minimum len (headers)
                    // of remaining events into account.
                    uint32_t remainingEvioHdrBytes = 4*(2*(eventCount - (i + 1)));
                    if ((evioEventLen + position) > (recordEndPos - remainingEvioHdrBytes)) {
                        throw EvioException("Bad evio format: invalid event byte length, " +
                                            std::to_string(evioEventLen));
                    }
                }

                if (isEvio) {
                    try {
                        // If the event is in evio format, parse it a bit
//std::cout << "      try extracting event " << i << ", pos = " << position << ", record pos = " << recordPos <<
//             ", place = " << (eventPlace + i) << std::endl;
                        auto node = EvioNode::extractEventNode(bigEnoughBuf, recordPos,
                                                               position, eventPlace + i);
                        byteLen = node->getTotalBytes();
//std::cout << "      event (evio)" << i << ", pos = " << node->getPosition() <<
//             ", dataPos = " << node->getDataPosition() << ", recordPos = " << node->getRecordPosition() <<
//             ", ev # = " << (eventPlace + i + 1) << ", bytes = " << byteLen << std::endl;

                        eventNodes.push_back(node);

                        if (byteLen < 8) {
                            throw EvioException("Bad evio format: bad bank byte length, " +
                                                std::to_string(byteLen));
                        }
                    }
                    catch (std::exception & e) {
                        // If we're here, the event is not in evio format

                        // The problem with throwing an exception if the event is NOT is evio format,
                        // is that the exception throwing mechanism is not a good way to
                        // handle normal logic flow. But not sure what else can be done.
                        // This should only happen very, very seldom.

                        // Assume parsing in extractEventNode is faulty and go with hdr len if available.
                        // If not, go with len from first bank word.

                        if (haveHdrEventLengths) {
                            byteLen = hdrEventLen;
                        }
                        else {
                            byteLen = evioEventLen;
                        }
                        evioFormat = false;
//std::cout << "      event " << i << ", bytes = " << byteLen << " failed on evio parsing: " << e.what() << std::endl;
                    }
                }
                else {
                    // If we're here, the event is not in evio format, so just use the length we got
                    // previously from the record index (which, if we're here, exists).
                    byteLen = hdrEventLen;
                    evioFormat = false;
//std::cout << "      event " << i << ", bytes = " << byteLen << ", has bad evio format" << std::endl;
                }

                // Hop over event
                position += byteLen;
//std::cout << "  *** scanBuffer: after event, set position to " << position << std::endl;
            }

            bigEnoughBuf->position(position);
            eventPlace += eventCount;

            // Next record position
            recordPos += origRecordBytes;

            // If the evio buffer was written with the evio library,
            // there is only 1 record in the buffer containing events -- the first record.
            // It's followed by a single record with no events, the trailer.

            // Read the next record if this is not the last one and there's enough data to
            // read a header.

        } while (!recordHeader.isLastRecord() && bytesLeft >= RecordHeader::HEADER_SIZE_BYTES);

        bigEnoughBuf->flip();
        return bigEnoughBuf;
    }


    /**
      * Scan buffer containing uncompressed data to find all records and store their position,
      * length, and event count.
      * Also finds all events and creates & stores their associated EvioNode objects.
      * @throws EvioException if buffer too small, not in the proper format, or earlier than version 6;
      *                       if checkRecordNumberSequence is true and records are out of sequence.
      */
    void Reader::scanUncompressedBuffer() {

        ByteBuffer headerBuffer(RecordHeader::HEADER_SIZE_BYTES);
        auto headerBytes = headerBuffer.array();

        RecordHeader recordHeader;
        bool haveFirstRecordHeader = false;

        // Start at the buffer's initial position
        size_t position  = bufferOffset;
        size_t recordPos = bufferOffset;
        ssize_t bytesLeft = bufferLimit - bufferOffset;

        // Keep track of the # of records, events, and valid words in file/buffer
        uint32_t eventPlace = 0;
        eventNodes.clear();
        recordPositions.clear();
        eventIndex.clear();
        recordNumberExpected = 1;

        while (bytesLeft >= RecordHeader::HEADER_SIZE_BYTES) {
            // Read record header
            buffer->position(position);
            // This moves the buffer's position
            buffer->getBytes(headerBytes, RecordHeader::HEADER_SIZE_BYTES);
            // Only sets the byte order of headerBuffer
            recordHeader.readHeader(headerBuffer);
            uint32_t eventCount = recordHeader.getEntries();
            uint32_t recordHeaderLen = recordHeader.getHeaderLength();
            uint32_t recordBytes = recordHeader.getLength();
            uint32_t indexArrayLen = recordHeader.getIndexLength();  // bytes

            // Consistency check, index array length reflects # of events properly?
            if (indexArrayLen > 0 && (indexArrayLen != 4*eventCount)) {
                throw EvioException("index array len (" + std::to_string(indexArrayLen) +
                                    ") and 4*eventCount (" + std::to_string(4*eventCount) + ") contradict each other");
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

            if (checkRecordNumberSequence) {
                if (recordHeader.getRecordNumber() != recordNumberExpected) {
                    //std::cout << "  scanBuffer: record # out of sequence, got " << recordHeader.getRecordNumber() <<
                    //        " expecting " << recordNumberExpected << std::endl;
                    throw EvioException("bad record # sequence");
                }
                recordNumberExpected++;
            }

            // Check to see if the whole record is there
            if (recordBytes > bytesLeft) {
                std::cout << "    record size = " << recordBytes << " >? bytesLeft = " << bytesLeft <<
                     ", pos = " << buffer->position() << std::endl;
                throw EvioException("Bad hipo format: not enough data to read record");
            }

            recordPositions.emplace_back(position, recordBytes, eventCount);
            // Track # of events in this record for event index handling
            eventIndex.addEventSize(eventCount);

            // If existing, use each event size in index array
            // as a check against the lengths found in the
            // first word in evio banks (their len) - must be identical for evio.
            // Unlike previously, this code handles 0 length index array.
            uint32_t hdrEventLen = 0, evioEventLen = 0;
            // Position in buffer to start reading event lengths, if any ...
            uint32_t lenIndex = position + recordHeaderLen;
            bool haveHdrEventLengths = (indexArrayLen > 0) && (eventCount > 0);

            // Hop over record header, user header, and index to events
            uint32_t byteLen = recordHeaderLen +
                               4*recordHeader.getUserHeaderLengthWords() + // takes padding into account
                               indexArrayLen;
            position  += byteLen;
            bytesLeft -= byteLen;

            // Do this because extractEventNode uses the buffer position
            buffer->position(position);

            // End position of record is right after event data including padding.
            // Remember it has been uncompressed.
            uint32_t recordEndPos = position + 4*recordHeader.getDataLengthWords();

            // For each event in record, store its location
            for (int i=0; i < eventCount; i++) {
                // Is the length we get from the first word of an evio bank/event (bytes)
                // the same as the length we got from the record header? If not, it's not evio.

                // Assume it's evio unless proven otherwise.
                // If index array missing, evio is the only option!
                bool isEvio = true;
                evioEventLen = 4 * (buffer->getUInt(position) + 1);

                if (haveHdrEventLengths) {
                    hdrEventLen = buffer->getUInt(lenIndex);
                    isEvio = (evioEventLen == hdrEventLen);
                    lenIndex += 4;

//                    if (!isEvio) {
//                        std::cout << "  *** scanBuffer: event " << i << " not evio format" << std::endl;
//                        std::cout << "  *** scanBuffer:   evio event len = " << evioEventLen << std::endl;
//                        std::cout << "  *** scanBuffer:   hdr  event len = " << hdrEventLen << std::endl;
//                    }
                }
                else {
                    // Check event len validity on upper bound as much as possible.
                    // Cannot extend beyond end of record, taking minimum len (headers)
                    // of remaining events into account.
                    uint32_t remainingEvioHdrBytes = 4*(2*(eventCount - (i + 1)));
                    if ((evioEventLen + position) > (recordEndPos - remainingEvioHdrBytes)) {
                        throw EvioException("Bad evio format: invalid event byte length, " +
                                            std::to_string(evioEventLen));
                    }
                }

                if (isEvio) {
                    try {
                        // If the event is in evio format, parse it a bit
                        auto node = EvioNode::extractEventNode(buffer, recordPos,
                                                               position, eventPlace + i);
                        byteLen = node->getTotalBytes();
                        eventNodes.push_back(node);

                        if (byteLen < 8) {
                            throw EvioException("Bad evio format: bad bank byte length, " +
                                                std::to_string(byteLen));
                        }
                    }
                    catch (std::exception & e) {
                        // If we're here, the event is not in evio format

                        // The problem with throwing an exception if the event is NOT is evio format,
                        // is that the exception throwing mechanism is not a good way to
                        // handle normal logic flow. But not sure what else can be done.
                        // This should only happen very, very seldom.

                        // Assume parsing in extractEventNode is faulty and go with hdr len if available.
                        // If not, go with len from first bank word.

                        if (haveHdrEventLengths) {
                            byteLen = hdrEventLen;
                        }
                        else {
                            byteLen = evioEventLen;
                        }
                        evioFormat = false;
                    }
                }
                else {
                    // If we're here, the event is not in evio format, so just use the length we got
                    // previously from the record index (which, if we're here, exists).
                    byteLen = hdrEventLen;
                    evioFormat = false;
                }

                // Hop over event
                position  += byteLen;
                bytesLeft -= byteLen;

                if (bytesLeft < 0) {
                    throw EvioException("Bad data format: bad length");
                }
            }

            eventPlace += eventCount;
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

//std::cout << "\n\nforceScanFile ---> force a file scan" << std::endl;

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
//std::cout << "forceScanFile: file header -->" << std::endl << fileHeader.toString() << std::endl;

        int recordLen;
        eventIndex.clear();
        recordPositions.clear();
        recordNumberExpected = 1;
        RecordHeader recordHeader;
        bool haveFirstRecordHeader = false;

        // Scan file by reading each record header and
        // storing its position, length, and event count.

        // Don't go beyond 1 header length before EOF since we'll be reading in 1 header
        size_t maximumSize = fileSize - RecordHeader::HEADER_SIZE_BYTES;

        // First record position (past file's header + index + user header)
        size_t recordPosition = fileHeader.getHeaderLength() +
                                fileHeader.getUserHeaderLength() +
                                fileHeader.getIndexLength() +
                                fileHeader.getUserHeaderLengthPadding();

        int recordCount = 0;
        while (recordPosition < maximumSize) {
            inStreamRandom.seekg(recordPosition);
            inStreamRandom.read(headerBytes, RecordHeader::HEADER_SIZE_BYTES);
            recordHeader.readHeader(headerBuffer);
//std::cout << "forceScanFile: record header " << recordCount << " @ pos = " <<
//     recordPosition << " -->" << std::endl << recordHeader.toString() << std::endl;
            recordCount++;


            // Checking record # sequence does NOT make sense when reading a file.
            // It only makes sense when reading from a stream and checking to see
            // if the record id, set by the sender, is sequential.
            // So feature turned off if reading from file.
            if (checkRecordNumberSequence) {
                if (recordHeader.getRecordNumber() != recordNumberExpected) {
                    std::cout << "forceScanFile: record # out of sequence, got " << recordHeader.getRecordNumber() <<
                         " expecting " << recordNumberExpected << std::endl;

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
//std::cout << "NUMBER OF RECORDS " << recordPositions.size() << std::endl;
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
//std::cout << "\nscanFile ---> calling forceScanFile ..." << std::endl;
            forceScanFile();
            return;
        }

        eventIndex.clear();
        recordPositions.clear();
        // recordNumberExpected = 1;

//std::cout << "\n\nscanFile ---> scanning the file" << std::endl;
        auto headerBytes = new char[FileHeader::HEADER_SIZE_BYTES];
        ByteBuffer headerBuffer(headerBytes, FileHeader::HEADER_SIZE_BYTES);

        fileHeader = FileHeader();

        // Go to file beginning
        inStreamRandom.seekg(0L);

        // Read and parse file header
        inStreamRandom.read(headerBytes, FileHeader::HEADER_SIZE_BYTES);
        fileHeader.readHeader(headerBuffer);
        byteOrder = fileHeader.getByteOrder();
        evioVersion = fileHeader.getVersion();
//std::cout << "scanFile: file header: " << std::endl << fileHeader.toString() << std::endl;

        // Is there an existing record length index?
        // Index in trailer gets first priority.
        // Index in file header gets next priority.
        bool fileHasIndex = fileHeader.hasTrailerWithIndex() || (fileHeader.hasIndex());
//std::cout << "scanFile: file has index = " << fileHasIndex <<
//             ", has trailer with index =  " << fileHeader.hasTrailerWithIndex() <<
//             ", file header has index " << fileHeader.hasIndex() << std::endl;

        // If there is no index, scan file
        if (!fileHasIndex) {
//std::cout << "scanFile: CALL forceScanFile" << std::endl;
            forceScanFile();
            return;
        }

        // If we're using the trailer, make sure it's position is valid,
        // (ie 0 is NOT valid).
        bool useTrailer = fileHeader.hasTrailerWithIndex();
        if (useTrailer) {
            // If trailer position is NOT valid ...
            if (fileHeader.getTrailerPosition() < 1) {
//std::cout << "scanFile: bad trailer position, " << fileHeader.getTrailerPosition() << std::endl;
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
//std::cout << "scanFile: record position (past file's header + index + user header) = " << recordPosition << std::endl;

        // Move to first record and save the header
        inStreamRandom.seekg(recordPosition);
        inStreamRandom.read(headerBytes, RecordHeader::HEADER_SIZE_BYTES);
        firstRecordHeader = std::make_shared<RecordHeader>();
        firstRecordHeader->readHeader(headerBuffer);
        compressed = firstRecordHeader->getCompressionType() != Compressor::UNCOMPRESSED;
//std::cout << "scanFile: read first record header ->\n" << firstRecordHeader->toString() << std::endl;

        int indexLength;

        // If we have a trailer with indexes ...
        if (useTrailer) {
            // Position read right before trailing header
            inStreamRandom.seekg(fileHeader.getTrailerPosition());
//std::cout << "scanFile: position file to trailer = " << fileHeader.getTrailerPosition() << std::endl;
            // Read trailer
            inStreamRandom.read(headerBytes, RecordHeader::HEADER_SIZE_BYTES);
            RecordHeader recordHeader;
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
        char index[indexLength];
        inStreamRandom.read(index, indexLength);
        // Turn bytes into record lengths & event counts
        uint32_t intData[indexLength/4];

        try {
//std::cout << "scanFile: transform int array from " << fileHeader.getByteOrder().getName() << std::endl;
            Util::toIntArray(index, indexLength, fileHeader.getByteOrder(), intData);

            // Turn record lengths into file positions and store in list
            recordPositions.clear();
            for (int i=0; i < indexLength/4; i += 2) {
                uint32_t len = intData[i];
                uint32_t count = intData[i+1];
//std::cout << "scanFile: record pos = " << recordPosition << ", len = " << len << ", count = " << count << std::endl;
                // Create a new RecordPosition object and store in vector
                recordPositions.emplace_back(recordPosition, len, count);
                // Track # of events in this record for event index handling
//std::cout << "scanFile: add record's event count (" << count << ") to eventIndex" << std::endl;
                eventIndex.addEventSize(count);

//                // Move to record, read & print each header
//                inStreamRandom.seekg(recordPosition);
//                inStreamRandom.read(headerBytes, RecordHeader::HEADER_SIZE_BYTES);
//                RecordHeader recHeader;
//                recHeader.readHeader(headerBuffer);
//                std::cout << "scanFile: read record header ->\n" << recHeader.toString() << std::endl << std::endl;

                recordPosition += len;
            }
        }
        catch (EvioException & e) {/* never happen */}
    }


    /**
     * This method removes the data, represented by the given node, from the buffer.
     * It also marks all nodes taken from that buffer as obsolete.
     * They must not be used anymore.<p>
     *
     * I don't think this method works since it does not change evio header lengths
     * recursively, it doesn't change record headers, and it doesn't move records!
     *
     * @deprecated has not been correctly programmed.
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
        else if (removeNode->isObsolete()) {
            //std::cout << "removeStructure: node has already been removed" << std::endl;
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

            for (std::shared_ptr<EvioNode> const & nd : ev->getAllNodes()) {
                // The first node in allNodes is the event node
                if (removeNode == nd) {
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
        removeNode->setObsolete(true);

        //---------------------------------------------------
        // Remove structure. Keep using current buffer.
        // We'll move all data that came after removed node
        // to where removed node used to be.
        //---------------------------------------------------

        // Amount of data being removed
        uint32_t removeDataLen = removeNode->getTotalBytes();

        // Just after removed node (start pos of data being moved)
        uint32_t startPos = removeNode->getPosition() + removeDataLen;

        // Duplicate buffer shares data, but we need to copy it so use copy constructor.
        ByteBuffer moveBuffer(*(buffer.get()));
        // Prepare to move data currently sitting past the removed node
        moveBuffer.limit(bufferLimit).position(startPos);

        // Set place to put the data being moved - where removed node starts
        buffer->position(removeNode->getPosition());
        // Copy it over
        buffer->put(moveBuffer);

        // Reset some buffer values
        buffer->position(bufferOffset);
        bufferLimit -= removeDataLen;
        buffer->limit(bufferLimit);

        // Reduce lengths of parent node
        removeNode->getParentNode()->updateLengths(-removeDataLen);

        // Reduce containing record's length
        uint32_t pos = removeNode->getRecordPosition();
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
     * To produce such evio data use {@link EvioBank#write(ByteBuffer &)},
     * {@link EvioSegment#write(ByteBuffer &)} or
     * {@link EvioTagSegment#write(ByteBuffer &)} depending on whether
     * a bank, seg, or tagseg is being added.<p>
     *
     * The given buffer argument must be ready to read with its position and limit
     * defining the limits of the data to copy.<p>
     *
     * I don't think this method works since it does not change evio header lengths
     * recursively, it doesn't change record headers, and it doesn't move records!
     *
     * @deprecated has not been correctly programmed.
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
        for (auto const & ev : eventNodes) {
            ev->setObsolete(true);
        }

        // Now the evio data in buffer is in a valid state so rescan buffer to update everything
        scanBuffer();

        return buffer;
    }


    /** Print out all record position information.  */
    void Reader::show() const {
        std::cout << " ***** FILE: (info), RECORDS = " << recordPositions.size() << " *****" << std::endl;
        for (RecordPosition entry : this->recordPositions) {
            std::cout << entry.toString();
        }
    }

}


