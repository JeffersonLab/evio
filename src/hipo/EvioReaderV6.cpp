//
// Created by Carl Timmer on 6/16/20.
//

#include "EvioReaderV6.h"

namespace evio {


    //------------------------


    /**
     * Constructor for reading an event file.
     *
     * @param path the full path to the file that contains events.
     * @param checkSeq if <code>true</code> check the record number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1
     *
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null;
     *                       if file is too small to have valid evio format data
     *                       if first record number != 1 when checkRecNumSeq arg is true
     */
    EvioReaderV6::EvioReaderV6(string const & path, bool checkSeq, bool synced) {
        if (path.empty()) {
            throw EvioException("path is empty");
        }
        synchronized = synced;
        reader = std::make_shared<Reader>(path);
        parser = std::make_shared<EventParser>();
     }


    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     * @param checkRecNumSeq if <code>true</code> check the record number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1
     * @see EventWriter
     * @throws EvioException if buffer arg is null;
     *                       if first record number != 1 when checkRecNumSeq arg is true
     */
    EvioReaderV6::EvioReaderV6(std::shared_ptr<ByteBuffer> & byteBuffer, bool checkRecNumSeq, bool synced) {
        synchronized = synced;
        reader = std::make_shared<Reader>(byteBuffer);
        parser = std::make_shared<EventParser>();
    }

    /**
     * This method can be used to avoid creating additional EvioReader
     * objects by reusing this one with another buffer. The method
     * {@link #close()} is called before anything else.
     *
     * @param buf ByteBuffer to be read
     * @throws IOException   if read failure
     * @throws EvioException if buf is null;
     *                       if first record number != 1 when checkRecNumSeq arg is true
     */
    void EvioReaderV6::setBuffer(std::shared_ptr<ByteBuffer> & buf) {
        if (synchronized) {
            const std::lock_guard<std::mutex> lock(mtx);
        }
        reader->setBuffer(buf);
    }

    /** {@inheritDoc} */
    bool  /* sync */ EvioReaderV6::isClosed() {return reader->isClosed();}

    /** {@inheritDoc} */
    bool EvioReaderV6::checkBlockNumberSequence() {return reader->getCheckRecordNumberSequence();}

    /** {@inheritDoc} */
    ByteOrder & EvioReaderV6::getByteOrder() {return reader->getByteOrder();}

    /** {@inheritDoc} */
    uint32_t EvioReaderV6::getEvioVersion() {return reader->getVersion();}

    /** {@inheritDoc} */
    string EvioReaderV6::getPath() {return reader->getFileName();}

    /**
     * Get the file/buffer parser.
     * @return file/buffer parser.
     */
    std::shared_ptr<EventParser> & EvioReaderV6::getParser() {return parser;}

    /**
     * Set the file/buffer parser.
     * @param p file/buffer parser.
     */
    void EvioReaderV6::setParser(std::shared_ptr<EventParser> & p) {
        if (p != nullptr) {
            this->parser = p;
        }
    }

    /** {@inheritDoc} */
    string EvioReaderV6::getDictionaryXML() {return reader->getDictionary();}

    /** {@inheritDoc} */
    bool EvioReaderV6::hasDictionaryXML() {return reader->hasDictionary();}

    /**
     * Get the number of events remaining in the file.
     * Useful only if doing a sequential read.
     *
     * @return number of events remaining in the file
     */
    size_t EvioReaderV6::getNumEventsRemaining() {return  reader->getNumEventsRemaining();}

    /** {@inheritDoc} */
    std::shared_ptr<ByteBuffer> EvioReaderV6::getByteBuffer() {return reader->getBuffer();}

    /** {@inheritDoc} */
    size_t EvioReaderV6::fileSize() {return reader->getFileSize();}


    /** {@inheritDoc} */
    std::shared_ptr<IBlockHeader> EvioReaderV6::getFirstBlockHeader() {return reader->getFirstRecordHeader();}


    /**
     * Get the event in the file/buffer at a given index (starting at 1).
     * As useful as this sounds, most applications will probably call
     * {@link #parseNextEvent()} or {@link #parseEvent(int)} instead,
     * since it combines combines getting an event with parsing it.<p>
     *
     * @param  index the event number in a 1,2,..N counting sense, from beginning of file/buffer.
     * @return the event in the file/buffer at the given index or null if none
     * @throws EvioException if failed file access;
     *                       if failed read due to bad file/buffer format;
     *                       if object closed
     */
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
        shared_ptr<uint8_t> bytes = reader->getNextEvent(&len);
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
        shared_ptr<uint8_t> evBytes = reader->getEvent(evNumber - 1, &len);
        if (evBytes == nullptr) {
            throw EvioException("eventNumber (" + std::to_string(evNumber) + ") is out of bounds");
        }

        vec.clear();
        vec.reserve(len);

        // copy data into provided vector ...
        std::memcpy(vec.data(), evBytes.get(), len);
        return len;
    }


    /** {@inheritDoc} */
    uint32_t EvioReaderV6::getEventBuffer(size_t evNumber, ByteBuffer & buf) {
        uint32_t len;
        shared_ptr<uint8_t> evBytes = reader->getEvent(evNumber - 1, &len);
        if (evBytes == nullptr) {
            throw EvioException("eventNumber (" + std::to_string(evNumber) + ") is out of bounds");
        }

        buf.clear();
        buf.expand(len);

        // copy data over
        std::memcpy(buf.array() + buf.arrayOffset(), evBytes.get(), len);
        return len;
    }


    /** This method is not relevant in evio 6 and does nothing. */
    void EvioReaderV6::rewind() {}


    /** This method is not relevant in evio 6, does nothing, and returns 0. */
    size_t EvioReaderV6::position() {return 0UL;}


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
     * In this version, this method is a wrapper on {@link #parseEvent(int)}.
     *
     * @deprecated use {@link #parseEvent(int)}.
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