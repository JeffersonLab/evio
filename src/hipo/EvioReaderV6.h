//
// Created by Carl Timmer on 6/16/20.
//

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

        //------------------------
        // File specific members
        //------------------------

//        /** Absolute path of the underlying file. */
//        string path;
//
//        /** File input stream. */
//        ifstream file;
//
//        /** File size in bytes. */
//        size_t fileBytes = 0;
//
//        /** Do we need to swap data from file? */
//        bool swap = false;
//
//        /**
//         * Read this file sequentially and not using a memory mapped buffer.
//         * If the file being read > 2.1 GBytes, then this is always true.
//         */
//        bool sequentialRead = false;


        //------------------------

    public:

        explicit EvioReaderV6(string const & path, bool checkRecNumSeq = false, bool synced = false);
        explicit EvioReaderV6(std::shared_ptr<ByteBuffer> & byteBuffer, bool checkRecNumSeq = false, bool synced = false);


        /*synchronized */ void setBuffer(std::shared_ptr<ByteBuffer> & buf) override ;
        /*synchronized*/ bool isClosed() override ;
        bool checkBlockNumberSequence() override ;
        ByteOrder & getByteOrder() override ;
        uint32_t getEvioVersion() override ;
        string getPath()override ;

        std::shared_ptr<EventParser> & getParser() override ;
        void setParser(std::shared_ptr<EventParser> & parser) override ;

        string getDictionaryXML() override ;
        bool hasDictionaryXML() override ;

        size_t getNumEventsRemaining() override ;
        std::shared_ptr<ByteBuffer> getByteBuffer() override ;
        size_t fileSize() override ;
        std::shared_ptr<IBlockHeader> getFirstBlockHeader() override ;

        /*synchronized*/ std::shared_ptr<EvioEvent> getEvent(size_t index) override ;
        /*synchronized*/ std::shared_ptr<EvioEvent> parseEvent(size_t index) override ;
        /*synchronized*/ std::shared_ptr<EvioEvent> nextEvent() override ;
        /*synchronized*/ std::shared_ptr<EvioEvent> parseNextEvent() override ;
        void parseEvent(std::shared_ptr<EvioEvent> evioEvent) override ;

        uint32_t getEventArray(size_t evNumber, std::vector<uint8_t> & vec) override;
        uint32_t getEventBuffer(size_t evNumber, ByteBuffer & buf) override;

        void rewind() override ;
        /*synchronized */ size_t position() override ;
        /*synchronized*/ void close() override ;

        std::shared_ptr<IBlockHeader> getCurrentBlockHeader() override ;
        /*synchronized*/ std::shared_ptr<EvioEvent> gotoEventNumber(size_t evNumber) override ;

        /*synchronized*/ size_t getEventCount() override ;
        /*synchronized*/ size_t getBlockCount() override ;
    };

}


#endif //EVIO_6_0_EVIOREADERV6_H
