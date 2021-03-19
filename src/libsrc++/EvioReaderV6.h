//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_EVIOREADERV6_H
#define EVIO_6_0_EVIOREADERV6_H


#include <fstream>
#include <vector>
#include <memory>
#include <mutex>


#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "IEvioReader.h"
#include "EvioReader.h"
#include "IBlockHeader.h"
#include "BlockHeaderV2.h"
#include "BlockHeaderV4.h"
#include "EventParser.h"
#include "Reader.h"


namespace evio {

    /**
     * This class is used to read an evio version 6 format file or buffer.
     * It is called by an <code>EvioReader</code> object. This class is mostly
     * a wrapper to the new hipo library.<p>
     *
     * The streaming effect of parsing an event is that the parser will read the event and hand off structures,
     * such as banks, to any IEvioListeners. For those familiar with XML, the event is processed SAX-like.
     * It is up to the listener to decide what to do with the structures.
     * <p>
     *
     * As an alternative to stream processing, after an event is parsed, the user can use the events treeModel
     * for access to the structures. For those familiar with XML, the event is processed DOM-like.
     * <p>
     *
     * @since version 6
     * @author timmer
     * @date 6/16/2020
     */
    class EvioReaderV6 : public IEvioReader {

    private:

        /** The reader object which does all the work. */
        std::shared_ptr<Reader> reader;

        /** Is this object currently closed? */
        bool closed = false;

        /** Parser object for file/buffer. */
        std::shared_ptr<EventParser> parser;

        /** Is this library made completely thread-safe? */
        bool synchronized = false;

        /** Mutex used for making thread safe. */
        std::mutex mtx;


    public:


        explicit EvioReaderV6(std::string const & path, bool checkRecNumSeq = false, bool synced = false);
        explicit EvioReaderV6(std::shared_ptr<ByteBuffer> & byteBuffer, bool checkRecNumSeq = false, bool synced = false);


        void setBuffer(std::shared_ptr<ByteBuffer> & buf) override ;
        bool isClosed() override ;
        bool checkBlockNumberSequence() override ;
        ByteOrder & getByteOrder() override ;
        uint32_t getEvioVersion() override ;
        std::string getPath()override ;

        std::shared_ptr<EventParser> & getParser() override ;
        void setParser(std::shared_ptr<EventParser> & evParser) override ;

        std::string getDictionaryXML() override ;
        bool hasDictionaryXML() override ;
        std::shared_ptr<EvioEvent> getFirstEvent() override;
        bool hasFirstEvent() override;

        size_t getNumEventsRemaining() override ;
        std::shared_ptr<ByteBuffer> getByteBuffer() override ;
        size_t fileSize() override ;
        std::shared_ptr<IBlockHeader> getFirstBlockHeader() override ;

        std::shared_ptr<EvioEvent> getEvent(size_t index) override ;
        std::shared_ptr<EvioEvent> parseEvent(size_t index) override ;
        std::shared_ptr<EvioEvent> nextEvent() override ;
        std::shared_ptr<EvioEvent> parseNextEvent() override ;
        void parseEvent(std::shared_ptr<EvioEvent> evioEvent) override ;

        uint32_t getEventArray(size_t evNumber, std::vector<uint8_t> & vec) override;
        uint32_t getEventBuffer(size_t evNumber, ByteBuffer & buf) override;

        void rewind() override ;
        ssize_t position() override ;
        void close() override ;

        std::shared_ptr<IBlockHeader> getCurrentBlockHeader() override ;
        std::shared_ptr<EvioEvent> gotoEventNumber(size_t evNumber) override ;

        size_t getEventCount() override ;
        size_t getBlockCount() override ;
    };

}


#endif //EVIO_6_0_EVIOREADERV6_H
