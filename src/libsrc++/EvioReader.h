//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_EVIOREADER_H
#define EVIO_6_0_EVIOREADER_H


#include <memory>
#include <fstream>


#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "IEvioReader.h"
#include "EvioReaderV4.h"
#include "EvioReaderV6.h"
#include "IBlockHeader.h"
#include "BlockHeaderV2.h"
#include "BlockHeaderV4.h"
#include "RecordHeader.h"

namespace evio {


    /**
     * This is a class of interest to the user. It is used to read any evio version
     * format file or buffer. Create an <code>EvioReader</code> object corresponding to an event
     * file or file-formatted buffer, and from this class you can test it
     * for consistency and, more importantly, you can call {@link #parseNextEvent} or
     * {@link #parseEvent(size_t)} to get new events and to stream the embedded structures
     * to an IEvioListener.<p>
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
     */
    class EvioReader : public IEvioReader {

    private:

        /** Evio version number (1-4, 6). Obtain this by reading first header. */
        uint32_t evioVersion = 4;

        /** Endianness of the data being read. Initialize to local endian. */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LOCAL};

        /** The buffer being read. */
        std::shared_ptr<ByteBuffer> byteBuffer;

        /** Initial position of buffer or mappedByteBuffer when reading a file. */
        size_t initialPosition = 0;

        /** Object to delegate to */
        std::shared_ptr<IEvioReader> reader;

    public:

        //   File constructor
        explicit EvioReader(std::string const & path, bool checkRecNumSeq = false, bool synced = false);

        //   Buffer constructor
        explicit EvioReader(std::shared_ptr<ByteBuffer> & bb, bool checkRecNumSeq = false, bool synced = false);

        //------------------------------------------

        void setBuffer(std::shared_ptr<ByteBuffer> & buf) override;
        bool isClosed () override;
        bool checkBlockNumberSequence() override;
        ByteOrder & getByteOrder() override;
        uint32_t getEvioVersion() override;
        std::string getPath() override;
        std::shared_ptr<EventParser> & getParser() override;
        void setParser(std::shared_ptr<EventParser> & evParser) override;

        std::string getDictionaryXML() override;
        bool hasDictionaryXML() override;
        std::shared_ptr<EvioEvent> getFirstEvent() override;
        bool hasFirstEvent() override;

        size_t getNumEventsRemaining() override;
        std::shared_ptr<ByteBuffer> getByteBuffer() override;
        size_t fileSize() override;
        std::shared_ptr<IBlockHeader> getFirstBlockHeader() override;


        std::shared_ptr<EvioEvent> getEvent(size_t index) override;
        std::shared_ptr<EvioEvent> parseEvent(size_t index) override;
        std::shared_ptr<EvioEvent> nextEvent() override;
        std::shared_ptr<EvioEvent> parseNextEvent() override;
        void parseEvent(std::shared_ptr<EvioEvent> evioEvent) override;


        static std::shared_ptr<EvioEvent> getEvent(uint8_t * src, size_t len, ByteOrder const & order);
        static std::shared_ptr<EvioEvent> parseEvent(uint8_t * src, size_t len, ByteOrder const & order) ;


        uint32_t getEventArray(size_t evNumber, std::vector<uint8_t> & vec) override;
        uint32_t getEventBuffer(size_t evNumber, ByteBuffer & buf) override;

        void rewind() override;
        ssize_t position() override;
        void close() override;

        std::shared_ptr<IBlockHeader> getCurrentBlockHeader() override;
        std::shared_ptr<EvioEvent> gotoEventNumber(size_t evNumber) override;
        size_t getEventCount() override;
        size_t getBlockCount() override;
    };

}


#endif //EVIO_6_0_EVIOREADER_H
