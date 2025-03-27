//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "EvioReaderV6.h"


namespace evio {


    /**
     * Constructor for reading an event file.
     *
     * @param path the full path to the file that contains events.
     * @param checkSeq if <code>true</code> check the record number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1, defaults false.
     * @param forceScan if true, force a scan of file, else use existing indexes first, defaults true,
     * @param synced if true, this class's methods are mutex protected for thread safety, defaults false.
     *
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null;
     *                       if file is too small to have valid evio format data
     *                       if first record number != 1 when checkRecNumSeq arg is true
     */
    EvioReaderV6::EvioReaderV6(std::string const & path, bool checkSeq, bool forceScan, bool synced) {
        if (path.empty()) {
            throw EvioException("path is empty");
        }
        synchronized = synced;
        reader = std::make_shared<Reader>(path, checkSeq, forceScan);
        parser = std::make_shared<EventParser>();
     }


    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     * @param checkRecNumSeq if <code>true</code> check the record number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1, defaults false.
     * @param synced if true, this class's methods are mutex protected for thread safety, defaults false.
     * @see EventWriter
     * @throws EvioException if buffer arg is null;
     *                       if first record number != 1 when checkRecNumSeq arg is true;
     *                       if buffer data not in evio format.
     */
    EvioReaderV6::EvioReaderV6(std::shared_ptr<ByteBuffer> byteBuffer, bool checkRecNumSeq, bool synced) {
        synchronized = synced;
        reader = std::make_shared<Reader>(byteBuffer, checkRecNumSeq);
        parser = std::make_shared<EventParser>();

        if (!reader->isEvioFormat()) {
            std::cout << "EvioReaderV6: buffer is NOT in evio format" << std::endl;
            throw EvioException("buffer not in evio format");
        }
    }


    /** {@inheritDoc} */
    void EvioReaderV6::setBuffer(std::shared_ptr<ByteBuffer> buf) {
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }

        reader->setBuffer(buf);

        if (!reader->isEvioFormat()) {
            std::cout << "EvioReaderV6: buffer is NOT in evio format" << std::endl;
            throw EvioException("buffer not in evio format");
        }
    }


    /** {@inheritDoc} */
    bool EvioReaderV6::isClosed() {return reader->isClosed();}


    /** {@inheritDoc} */
    bool EvioReaderV6::checkBlockNumberSequence() {return reader->getCheckRecordNumberSequence();}


    /** {@inheritDoc} */
    ByteOrder & EvioReaderV6::getByteOrder() {return reader->getByteOrder();}


    /** {@inheritDoc} */
    uint32_t EvioReaderV6::getEvioVersion() {return reader->getVersion();}


    /** {@inheritDoc} */
    std::string EvioReaderV6::getPath() {return reader->getFileName();}


    /** {@inheritDoc} */
    std::shared_ptr<EventParser> EvioReaderV6::getParser() {return parser;}


    /** {@inheritDoc} */
    void EvioReaderV6::setParser(std::shared_ptr<EventParser> evParser) {
        if (evParser != nullptr) {
            this->parser = evParser;
        }
    }


    /** {@inheritDoc} */
    std::string EvioReaderV6::getDictionaryXML() {return reader->getDictionary();}

    /** {@inheritDoc} */
    bool EvioReaderV6::hasDictionaryXML() {return reader->hasDictionary();}

    /** {@inheritDoc} */
    std::shared_ptr<EvioEvent> EvioReaderV6::getFirstEvent() {
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }

        uint32_t len;
        std::shared_ptr<uint8_t> feBuf = reader->getFirstEvent(&len);
        if (feBuf == nullptr) {
            return nullptr;
        }

        // Turn this buffer into an EvioEvent object

        std::vector<uint8_t> vec;
        vec.reserve(len);

        // copy data into provided vector ...
        std::memcpy(vec.data(), feBuf.get(), len);

        return EvioReader::parseEvent(vec.data(), len, reader->getByteOrder());
    }

    /** {@inheritDoc} */
    bool EvioReaderV6::hasFirstEvent() {return reader->hasFirstEvent();}


    /** {@inheritDoc} */
    size_t EvioReaderV6::getNumEventsRemaining() {return  reader->getNumEventsRemaining();}


    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioReaderV6::getByteBuffer() {return reader->getBuffer();}


    /** {@inheritDoc} */
    size_t EvioReaderV6::fileSize() {return reader->getFileSize();}


    /** {@inheritDoc} */
    std::shared_ptr<IBlockHeader> EvioReaderV6::getFirstBlockHeader() {return reader->getFirstRecordHeader();}


    /** {@inheritDoc} */
    std::shared_ptr<EvioEvent> EvioReaderV6::getEvent(size_t index) {
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }

        std::vector<uint8_t> vec;
        uint32_t len = getEventArray(index, vec);
        return EvioReader::getEvent(vec.data(), len, reader->getByteOrder());
    }


    /** {@inheritDoc} */
    std::shared_ptr<EvioEvent> EvioReaderV6::parseEvent(size_t index) {
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }
        auto event = getEvent(index);
        if (event != nullptr) parseEvent(event);
        return event;
    }


    /** {@inheritDoc} */
    std::shared_ptr<EvioEvent> EvioReaderV6::nextEvent() {
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }

        if (closed) {
            throw EvioException("object closed");
        }

        uint32_t len;
        std::shared_ptr<uint8_t> bytes = reader->getNextEvent(&len);
        if (bytes == nullptr) {
            return nullptr;
        }
        return EvioReader::getEvent(bytes.get(), len, reader->getByteOrder());
    }


    /** {@inheritDoc} */
    std::shared_ptr<EvioEvent> EvioReaderV6::parseNextEvent() {
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
    void EvioReaderV6::parseEvent(std::shared_ptr<EvioEvent> evioEvent) {
        // This method is synchronized too
        parser->parseEvent(evioEvent);
    }


    /** {@inheritDoc} */
    uint32_t EvioReaderV6::getEventArray(size_t evNumber, std::vector<uint8_t> & vec) {
        if (closed) {
            throw EvioException("object closed");
        }

        uint32_t len;
        std::shared_ptr<uint8_t> evBytes = reader->getEvent(evNumber - 1, &len);
        if (evBytes == nullptr) {
            throw EvioException("eventNumber (" + std::to_string(evNumber) + ") is out of bounds");
        }

        vec.resize(len, 0);

        // copy data into provided vector ...
        std::memcpy(vec.data(), evBytes.get(), len);
        return len;
    }


    /** {@inheritDoc} */
    uint32_t EvioReaderV6::getEventBuffer(size_t evNumber, ByteBuffer & buf) {
        uint32_t len;
        std::shared_ptr<uint8_t> evBytes = reader->getEvent(evNumber - 1, &len);
        if (evBytes == nullptr) {
            throw EvioException("eventNumber (" + std::to_string(evNumber) + ") is out of bounds");
        }

        buf.clear();
        buf.expand(len);

        // copy data over
        std::memcpy(buf.array() + buf.arrayOffset(), evBytes.get(), len);
        return len;
    }


    /**
     * The equivalent of rewinding the file. What it actually does
     * is set the position of the sequential index back to the beginning.
     * This allows a mix of sequential calls with those that are not sequential.
     */
    void EvioReaderV6::rewind() {reader->rewind();}


    /**
     * This method is not relevant in evio 6, does nothing, and returns 0.
     * @return 0
     */
    ssize_t EvioReaderV6::position() {return 0UL;}


    /**
     * This is closes the file, but for buffers it only sets the position to 0.
     * @throws IOException if error accessing file
     */
    void EvioReaderV6::close() {
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }

        if (closed) {
            return;
        }

        reader->close();
        closed = true;
    }


    /** {@inheritDoc} */
    std::shared_ptr<IBlockHeader> EvioReaderV6::getCurrentBlockHeader() {
        return reader->getCurrentRecordStream().getHeader();
    }


    /**
     * In this version, this method is a wrapper on {@link #parseEvent(size_t)}.
     *
     * @deprecated use {@link #parseEvent(size_t)}.
     * @param evNumber the event number from the start of the file starting at 1.
     * @return the specified event in file or null if there's an error or nothing at that event #.
     * @throws EvioException if object closed
     */
    std::shared_ptr<EvioEvent> EvioReaderV6::gotoEventNumber(size_t evNumber) {
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }

        if (closed) {
            throw EvioException("object closed");
        }

        try {
            return parseEvent(evNumber);
        }
        catch (EvioException & e) {
            return nullptr;
        }
    }


    /** {@inheritDoc} */
    size_t EvioReaderV6::getEventCount() {
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }

        if (closed) {
            throw EvioException("object closed");
        }
        return reader->getEventCount();
    }


    /** {@inheritDoc} */
    size_t EvioReaderV6::getBlockCount() {
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }

        if (closed) {
            throw EvioException("object closed");
        }
        return reader->getRecordCount();
    }


}