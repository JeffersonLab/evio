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

#ifndef EVIO_6_0_EVIOREADERV4_H
#define EVIO_6_0_EVIOREADERV4_H

#include <fstream>
#include <vector>
#include <memory>


#include "IEvioReader.h"
#include "IBlockHeader.h"
#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "EvioReader.h"
#include "EventParser.h"


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
    class EvioReaderV4 : public IEvioReader {

    private:

        /**  Offset to get magic number from start of file. */
        static const uint32_t MAGIC_OFFSET = 28;

        /** Offset to get version number from start of file. */
        static const uint32_t VERSION_OFFSET = 20;

        /** Offset to get block size from start of block. */
        static const uint32_t BLOCK_SIZE_OFFSET = 0;

        /** Mask to get version number from 6th int in block. */
        static const uint32_t VERSION_MASK = 0xff;

//        /** Root element tag for XML file */
//        static constexpr string ROOT_ELEMENT {"evio-data"};

        /** Default size for a single file read in bytes when reading
         *  evio format 1-3. Equivalent to 500, 32,768 byte blocks.
         *  This constant <b>MUST BE</b> an integer multiple of 32768.*/
        static const uint32_t DEFAULT_READ_BYTES = 32768 * 500; // 16384000 bytes



        /** When doing a sequential read, used to assign a transient
         * number [1..n] to events as they are being read. */
        uint32_t eventNumber = 0;

        /**
         * This is the number of events in the file. It is not computed unless asked for,
         * and if asked for it is computed and cached in this variable.
         */
        int32_t eventCount = -1;

        /** Evio version number (1-4). Obtain this by reading first block header. */
        uint32_t evioVersion;

        /**
         * Endianness of the data being read, either
         * {@link ByteOrder#BIG_ENDIAN} or
         * {@link ByteOrder#LITTLE_ENDIAN}.
         */
        ByteOrder byteOrder;

        /** Size of the first block in bytes. */
        uint32_t firstBlockSize;

        /**
         * This is the number of blocks in the file including the empty block at the
         * end of the version 4 files. It is not computed unless asked for,
         * and if asked for it is computed and cached in this variable.
         */
        uint32_t blockCount = -1;

        /** The current block header for evio versions 1-3. */
        BlockHeaderV2 blockHeader2 = new BlockHeaderV2();

        /** The current block header for evio version 4. */
        BlockHeaderV4 blockHeader4 = new BlockHeaderV4();

        /** Reference to current block header, any version, through interface.
         *  This must be the same object as either blockHeader2 or blockHeader4
         *  depending on which evio format version the data is in. */
        std::shared_ptr<IBlockHeader> blockHeader;

        /** Reference to first block header. */
        std::shared_ptr<IBlockHeader> firstBlockHeader;

        /** Block number expected when reading. Used to check sequence of blocks. */
        int blockNumberExpected = 1;

        /** If true, throw an exception if block numbers are out of sequence. */
        bool checkBlockNumSeq;

        /** Is this the last block in the file or buffer? */
        bool lastBlock;

        /**
         * Version 4 files may have an xml format dictionary in the
         * first event of the first block.
         */
        string dictionaryXML;

        /** The buffer being read. */
        ByteBuffer byteBuffer;

        /** Parser object for this file/buffer. */
        std::shared_ptr<EventParser> parser;

        /** Initial position of buffer or mappedByteBuffer when reading a file. */
        size_t initialPosition;

        //------------------------
        // File specific members
        //------------------------

//        /** Use this object to handle files > 2.1 GBytes but still use memory mapping. */
//        MappedMemoryHandler mappedMemoryHandler;

        /** Absolute path of the underlying file. */
        string path;

        /** File input stream. */
        ifstream file;

        /** File size in bytes. */
        size_t fileBytes;

        /** File channel used to read data and access file position. */
        FileChannel fileChannel;

        /** Data stream used to read data. */
        DataInputStream dataStream;

        /** Do we need to swap data from file? */
        bool swap;

        /**
         * Read this file sequentially and not using a memory mapped buffer.
         * If the file being read > 2.1 GBytes, then this is always true.
         */
        bool sequentialRead;


        //------------------------
        // EvioReader's state
        //------------------------

        /** Is this object currently closed? */
        bool closed;

        /**
         * This class stores the state of this reader so it can be recovered
         * after a state-changing method has been called -- like {@link #rewind()}.
         */
        static class ReaderState {
          public:
            bool lastBlock;
            int eventNumber;
            long filePosition;
            int byteBufferLimit;
            int byteBufferPosition;
            int blockNumberExpected;
            BlockHeaderV2 blockHeader2;
            BlockHeaderV4 blockHeader4;
        };

        ReaderState * getState();
        void restoreState(ReaderState * state);

        //------------------------

    public:

        explicit EvioReaderV4(string const & path, bool checkBlkNumSeq = false, bool sequential = false);

        explicit EvioReaderV4(ByteBuffer & byteBuffer, bool checkBlkNumSeq = false);


        /*synchronized*/ void setBuffer(ByteBuffer & buf);

        /*synchronized*/ bool isClosed() const;

        bool checkBlockNumberSequence() const;

        ByteOrder getByteOrder() const;
        uint32_t getEvioVersion() const;

        string getPath() const;

        std::shared_ptr<EventParser> getParser() const;
        void setParser(std::shared_ptr<EventParser> & evParser);

        string getDictionaryXML() const;
        bool hasDictionaryXML() const;

        size_t getNumEventsRemaining() const;

        ByteBuffer & getByteBuffer();

        size_t fileSize() const;

        std::shared_ptr<IBlockHeader> getFirstBlockHeader();

    protected:

        void parseFirstHeader(ByteBuffer & headerBuf);
        EvioReader::ReadStatus processNextBlock();

    private:

        void prepareForSequentialRead();
        void prepareForBufferRead(ByteBuffer & buffer) const;

        void readDictionary(ByteBuffer & buffer);
        /*synchronized*/ std::shared_ptr<EvioEvent> getEventV4(size_t index);

    public:

        std::shared_ptr<EvioEvent> getEvent(size_t index);
        /*synchronized*/ std::shared_ptr<EvioEvent> parseEvent(size_t index);
        /*synchronized*/ std::shared_ptr<EvioEvent> nextEvent();
        /*synchronized*/ std::shared_ptr<EvioEvent> parseNextEvent();
        void parseEvent(std::shared_ptr<EvioEvent> evioEvent);
        std::vector<uint8_t> getEventArray(size_t eventNumber);
        ByteBuffer & getEventBuffer(size_t eventNumber);

    private:

        size_t bufferBytesRemaining() const;
        size_t blockBytesRemaining() const;
        /*synchronized*/ std::shared_ptr<EvioEvent> gotoEventNumber(size_t evNumber, bool parse);

    public:

        /*synchronized*/ void rewind();
        /*synchronized*/ size_t position() const;

        /*synchronized*/ void close();
        std::shared_ptr<IBlockHeader> getCurrentBlockHeader();
        std::shared_ptr<EvioEvent> gotoEventNumber(size_t evNumber);

        /*synchronized*/ size_t getEventCount() const;
        /*synchronized*/ size_t getBlockCount() const;
    };


}


#endif //EVIO_6_0_EVIOREADERV4_H
