//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100



#ifndef EVIO_6_0_EVIOREADERV4_H
#define EVIO_6_0_EVIOREADERV4_H


#include <fstream>
#include <vector>
#include <memory>
#include <mutex>
#include <stdexcept>


#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "IEvioReader.h"
#include "IBlockHeader.h"
#include "BlockHeaderV2.h"
#include "BlockHeaderV4.h"
#include "EventParser.h"


namespace evio {

    /**
     * This is a class of interest to the user. It is used to read any evio version
     * format file or buffer. Create an <code>EvioReader</code> object corresponding to an event
     * file or file-formatted buffer, and from this class you can test it
     * for consistency and, more importantly, you can call {@link #parseNextEvent} or
     * {@link #parseEvent(size_t)} to get new events and to stream the embedded structures
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
     */
    class EvioReaderV4 : public IEvioReader {

    public:

        /**  Offset to get magic number from start of file. */
        static const uint32_t MAGIC_OFFSET = 28;

        /** Offset to get version number from start of file. */
        static const uint32_t VERSION_OFFSET = 20;

        /** Offset to get block size from start of block. */
        static const uint32_t BLOCK_SIZE_OFFSET = 0;

        /** Mask to get version number from 6th int in block. */
        static const uint32_t VERSION_MASK = 0xff;

        /** Default size for a single file read in bytes when reading
         *  evio format 1-3. Equivalent to 500, 32,768 byte blocks.
         *  This constant <b>MUST BE</b> an integer multiple of 32768.*/
        static const uint32_t DEFAULT_READ_BYTES = 32768 * 500; // 16384000 bytes

    private:

        /** When doing a sequential read, used to assign a transient
         * number [1..n] to events as they are being read. */
        uint32_t eventNumber = 0;

        /**
         * This is the number of events in the file. It is not computed unless asked for,
         * and if asked for it is computed and cached in this variable.
         */
        int32_t eventCount = -1;

        /** Evio version number (1-4). Obtain this by reading first block header. */
        uint32_t evioVersion = 4;

        /** Endianness of the data being read. Initialize to local endian. */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LOCAL};

        /** Size of the first block in bytes. */
        uint32_t firstBlockSize = 0;

        /**
         * This is the number of blocks in the file including the empty block at the
         * end of the version 4 files. It is not computed unless asked for,
         * and if asked for it is computed and cached in this variable.
         */
        uint32_t blockCount = 0;

        /** The current block header for evio versions 1-3. */
        std::shared_ptr<BlockHeaderV2> blockHeader2;

        /** The current block header for evio version 4. */
        std::shared_ptr<BlockHeaderV4> blockHeader4;

        /** Reference to current block header, any version, through interface.
         *  This must be the same object as either blockHeader2 or blockHeader4
         *  depending on which evio format version the data is in. */
        std::shared_ptr<IBlockHeader> blockHeader;

        /** Reference to first block header. */
        std::shared_ptr<IBlockHeader> firstBlockHeader;

        /** Reference to first block header if evio versions 1-3. */
        std::shared_ptr<BlockHeaderV2> firstBlockHeader2;

        /** Reference to first block header if version 4. */
        std::shared_ptr<BlockHeaderV4> firstBlockHeader4;

        /** Block number expected when reading. Used to check sequence of blocks. */
        uint32_t blockNumberExpected = 1;

        /** If true, throw an exception if block numbers are out of sequence. */
        bool checkBlockNumSeq = false;

        /** Is this the last block in the file or buffer? */
        bool lastBlock = false;

        /** Is this library made completely thread-safe? */
        bool synchronized = false;

        /**
         * Version 4 files may have an xml format dictionary in the
         * first event of the first block.
         */
        std::string dictionaryXML;

        /** The buffer being read. */
        std::shared_ptr<ByteBuffer> byteBuffer;

        /** Parser object for this file/buffer. */
        std::shared_ptr<EventParser> parser;

        /** Initial position of buffer or mappedByteBuffer when reading a file. */
        size_t initialPosition = 0;

        /** Vector containing each event's position.
         * In Java this was contained in MemoryMappedHandler class. */
        std::vector<uint32_t> eventPositions;

        /** Mutex used for making thread safe. */
        std::mutex mtx;


        //------------------------
        // File specific members
        //------------------------


        /** Absolute path of the underlying file. */
        std::string path;

        /** File input stream. */
        std::ifstream file;

        /** File size in bytes. */
        size_t fileBytes = 0;

        /** Do we need to swap data from file? */
        bool swap = false;

        /**
         * Read this file sequentially and not using a memory mapped buffer.
         * If the file being read > 2.1 GBytes, then this is always true.
         */
        bool sequentialRead = false;


        //------------------------
        // EvioReader's state
        //------------------------


        /** Is this object currently closed? */
        bool closed = false;

        /**
         * This class stores the state of this reader so it can be recovered
         * after a state-changing method has been called -- like {@link #rewind()}.
         */
        class ReaderState {
          public:
            bool lastBlock;
            uint32_t eventNumber;
            size_t filePosition;
            size_t byteBufferLimit;
            size_t byteBufferPosition;
            uint32_t blockNumberExpected;
            std::shared_ptr<BlockHeaderV2> blockHeader2;
            std::shared_ptr<BlockHeaderV4> blockHeader4;
        };

        ReaderState * getState();
        void restoreState(ReaderState * state);

        //------------------------

        size_t generateEventPositions(std::shared_ptr<ByteBuffer> & byteBuffer);

    public:

        explicit EvioReaderV4(std::string const & path, bool checkBlkNumSeq = false, bool synced = false);
        explicit EvioReaderV4(std::shared_ptr<ByteBuffer> & byteBuffer, bool checkBlkNumSeq = false, bool synced = false);


        void setBuffer(std::shared_ptr<ByteBuffer> & buf) override;
        bool isClosed() override;
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
        std::shared_ptr<ByteBuffer> getByteBuffer() override ;
        size_t fileSize() override;
        std::shared_ptr<IBlockHeader> getFirstBlockHeader() override ;

    protected:

        void parseFirstHeader(std::shared_ptr<ByteBuffer> & headerBuf);
        IEvioReader::ReadWriteStatus processNextBlock();

    private:

        void prepareForSequentialRead();
        void prepareForBufferRead(std::shared_ptr<ByteBuffer> & buffer) const;

        void readDictionary(std::shared_ptr<ByteBuffer> & buffer);
        std::shared_ptr<EvioEvent> getEventV4(size_t index);

    public:

        std::shared_ptr<EvioEvent> getEvent(size_t index) override ;
        std::shared_ptr<EvioEvent> parseEvent(size_t index) override ;
        std::shared_ptr<EvioEvent> nextEvent() override ;
        std::shared_ptr<EvioEvent> parseNextEvent() override ;
        void parseEvent(std::shared_ptr<EvioEvent> evioEvent) override ;
        uint32_t getEventArray(size_t evNumber, std::vector<uint8_t> & vec) override;
        uint32_t getEventBuffer(size_t evNumber, ByteBuffer & buf) override;

    private:

        size_t bufferBytesRemaining() const;
        uint32_t blockBytesRemaining() const;
        std::shared_ptr<EvioEvent> gotoEventNumber(size_t evNumber, bool parse);

    public:

        void rewind() override ;
        ssize_t position() override;
        void close() override ;

        std::shared_ptr<IBlockHeader> getCurrentBlockHeader() override ;
        std::shared_ptr<EvioEvent> gotoEventNumber(size_t evNumber) override ;

        size_t getEventCount() override;
        size_t getBlockCount() override;
    };


}


#endif //EVIO_6_0_EVIOREADERV4_H
