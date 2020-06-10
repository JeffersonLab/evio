/**
 * Copyright (c) 2020, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 06/8/2020
 * @author timmer
 */

#ifndef EVIO_6_0_EVIOREADER_H
#define EVIO_6_0_EVIOREADER_H

#include <memory>
#include <fstream>

#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "IEvioReader.h"
#include "IBlockHeader.h"
#include "RecordHeader.h"

namespace evio {


    /**
     * This is a class of interest to the user. It is used to read any evio version
     * format file or buffer. Create an <code>EvioReader</code> object corresponding to an event
     * file or file-formatted buffer, and from this class you can test it
     * for consistency and, more importantly, you can call {@link #parseNextEvent} or
     * {@link #parseEvent(int)} to get new events and to stream the embedded structures
     * to an IEvioListener.<p>
     *
     * A word to the wise, constructors for reading a file in random access mode
     * (by setting "sequential" arg to false), will memory map the file. This is
     * <b>not</b> a good idea if the file is not on a local disk.<p>
     *
     * The streaming effect of parsing an event is that the parser will read the event and hand off structures,
     * such as banks, to any IEvioListeners. For those familiar with XML, the event is processed SAX-like.
     * It is up to the listener to decide what to do with the structures.
     * <p>
     *
     * As an alternative to stream processing, after an event is parsed, the user can use the events' tree
     * structure for access its nodes. For those familiar with XML, the event is processed DOM-like.
     * <p>
     *
     * @author heddle (original java version)
     * @author timmer
     *
     */
    class EvioReader : public IEvioReader {

    private:

        /** Evio version number (1-4, 6). Obtain this by reading first header. */
        uint32_t evioVersion = 4;

        /**
         * Endianness of the data being read, either
         * {@link ByteOrder::ENDIAN_BIG} or {@link ByteOrder::ENDIAN_LITTLE}.
         */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LITTLE};

        /** The buffer being read. */
        ByteBuffer & byteBuffer;

        /** Initial position of buffer or mappedByteBuffer when reading a file. */
        size_t initialPosition = 0;

        /** Object to delegate to */
        std::shared_ptr<IEvioReader> reader;

    public:

        /**
          * This <code>enum</code> denotes the status of a read. <b>Used internally.</b><br>
          * SUCCESS indicates a successful read. <br>
          * END_OF_FILE indicates that we cannot read because an END_OF_FILE has occurred.
          * Technically this means that what
          * ever we are trying to read is larger than the buffer's unread bytes.<br>
          * EVIO_EXCEPTION indicates that an EvioException was thrown during a read, possibly
          * due to out of range values.<br>
          * UNKNOWN_ERROR indicates that an unrecoverable error has occurred.
          */
        enum ReadStatus {
            SUCCESS = 0, END_OF_FILE, EVIO_EXCEPTION, UNKNOWN_ERROR
        };


        //   File constructor
        explicit EvioReader(string const & path, bool checkRecNumSeq = false,
                            bool sequential = false, bool synced = false);

        //   Buffer constructor
        explicit EvioReader(ByteBuffer byteBuffer, bool checkRecNumSeq = false, bool synced = false);

        //------------------------------------------

        void setBuffer(ByteBuffer & buf) override;
        bool isClosed () override;
        bool checkBlockNumberSequence() override;
        ByteOrder & getByteOrder() override;
        uint32_t getEvioVersion() override;
        string getPath() override;
        std::shared_ptr<EventParser> & getParser() override;
        void setParser(std::shared_ptr<EventParser> & parser) override;
        string getDictionaryXML() override;
        bool hasDictionaryXML() override;
        size_t getNumEventsRemaining() override;
        ByteBuffer & getByteBuffer() override;
        size_t fileSize() override;
        std::shared_ptr<IBlockHeader> getFirstBlockHeader() override;

    private:

        ReadStatus findEvioVersion();

    public:

        std::shared_ptr<EvioEvent> getEvent(size_t index) override;
        std::shared_ptr<EvioEvent> parseEvent(size_t index) override;
        std::shared_ptr<EvioEvent> nextEvent() override;
        std::shared_ptr<EvioEvent> parseNextEvent() override;
        void parseEvent(std::shared_ptr<EvioEvent> evioEvent) override;


        static std::shared_ptr<EvioEvent> getEvent(uint8_t * dest, size_t len, ByteOrder const & order);
        static std::shared_ptr<EvioEvent> parseEvent(uint8_t * dest, size_t len, ByteOrder const & order) ;


        std::vector<uint8_t> getEventArray(size_t eventNumber) override;
        ByteBuffer & getEventBuffer(size_t eventNumber) override;

        void rewind() override;
        size_t position() override;
        void close() override;

        std::shared_ptr<IBlockHeader> getCurrentBlockHeader() override;
        std::shared_ptr<EvioEvent> gotoEventNumber(size_t evNumber) override;
        size_t getEventCount() override;
        size_t getBlockCount() override;
    };

}


#endif //EVIO_6_0_EVIOREADER_H
