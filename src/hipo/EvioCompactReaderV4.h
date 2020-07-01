/**
 * Copyright (c) 2020, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * EPSCI Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 07/01/2020
 * @author timmer
 */

#include <vector>
#include <memory>
#include <string>


#include "ByteBuffer.h"
#include "ByteOrder.h"
#include "IEvioCompactReader.h"
#include "IEvioReader.h"
#include "IBlockHeader.h"
#include "EvioNode.h"


#ifndef EVIO_6_0_EVIOCOMPACTREADERV4_H
#define EVIO_6_0_EVIOCOMPACTREADERV4_H


namespace evio {



    /**
     * This class is used to read an evio format version 4 formatted file or buffer
     * and extract specific evio containers (bank, seg, or tagseg)
     * with actual data in them given a tag/num pair.<p>
     *
     * @author timmer
     */
    class EvioCompactReaderV4 : IEvioCompactReader {

    public:

        /** Offset to get block size from start of block. */
        static const int BLOCK_SIZE_OFFSET = 0;

        /** Offset to get block number from start of block. */
        static const int BLOCK_NUMBER = 4;

        /** Offset to get block header size from start of block. */
        static const int BLOCK_HEADER_SIZE_OFFSET = 8;

        /** Offset to get block event count from start of block. */
        static const int BLOCK_EVENT_COUNT = 12;

        /** Offset to get block size from start of block. */
        static const  int BLOCK_RESERVED_1 = 16;

        /** Mask to get version number from 6th int in block. */
        static const int VERSION_MASK = 0xff;

    private:

        /** Stores info of all the (top-level) events. */
        std::vector<EvioNode> eventNodes;

        /** Store info of all block headers. */
        unordered_map<uint32_t, BlockNode> blockNodes = new HashMap<>(20);

        /** Source (pool) of EvioNode objects used for parsing Evio data in buffer. */
        EvioNodeSource nodePool;

        /**
         * This is the number of events in the file. It is not computed unless asked for,
         * and if asked for it is computed and cached in this variable.
         */
        int eventCount = -1;

        /** Evio version number (1-4). Obtain this by reading first block header. */
        uint32_t evioVersion = 4;

        /**
         * Endianness of the data being read, either
         * {@link ByteOrder#BIG_ENDIAN} or
         * {@link ByteOrder#LITTLE_ENDIAN}.
         */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LITTLE};

        /**
         * This is the number of blocks in the file including the empty block at the
         * end of the version 4 files. It is not computed unless asked for,
         * and if asked for it is computed and cached in this variable.
         */
        int blockCount = -1;

        /** Size of the first block header in 32-bit words. Used to read dictionary. */
        int firstBlockHeaderWords;

        /** The current block header. */
        BlockHeaderV4 blockHeader;

        /** Does the file/buffer have a dictionary? */
        bool hasDict;

        /**
         * Version 4 files may have an xml format dictionary in the
         * first event of the first block.
         */
        std::string dictionaryXML;

        /** Dictionary object created from dictionaryXML string. */
        EvioXMLDictionary dictionary;

        /** The buffer being read. */
        ByteBuffer byteBuffer;

        /** Initial position of buffer (mappedByteBuffer if reading a file). */
        size_t initialPosition = 0;

        /** How much of the buffer being read is valid evio data (in 32bit words)?
         *  The valid data begins at initialPosition and ends after this length.*/
        size_t validDataWords = 0;

        /** Is this object currently closed? */
        bool closed = false;

        //------------------------
        // File specific members
        //------------------------

        /** Are we reading a file or buffer? */
        bool readingFile = false;

        ifstream inStreamRandom;

        /**
         * The buffer representing a map of the input file which is also
         * accessed through {@link #byteBuffer}.
         */
        MappedByteBuffer mappedByteBuffer;

        /** Absolute path of the underlying file. */
        std::string path = "";

        /** File size in bytes. */
        size_t fileBytes = 0;


    public:

        EvioCompactReaderV4(std::string const & path);
        EvioCompactReaderV4(std::shared_ptr<ByteBuffer> &byteBuffer);
        EvioCompactReaderV4(std::shared_ptr<ByteBuffer> & byteBuffer, EvioNodeSource & pool) ;

        void setBuffer(std::shared_ptr<ByteBuffer> buf);
        void setBuffer(std::shared_ptr<ByteBuffer> buf, EvioNodeSource & pool);

        std::shared_ptr<ByteBuffer> setCompressedBuffer(std::shared_ptr<ByteBuffer> buf, EvioNodeSource & pool);

        bool isFile() override ;
        bool isCompressed() override ;
        bool isClosed() override ;
        ByteOrder getByteOrder() override ;
        uint32_t getEvioVersion() override ;
        std::string getPath() override ;
        ByteOrder getFileByteOrder() override ;
        std::string getDictionaryXML() override ;
        EvioXMLDictionary getDictionary() override ;
        bool hasDictionary() override ;


    private:

        void mapFile(FileChannel  & inputChannel);
        void generateEventPositionTable();

    public:

        // MappedByteBuffer getMappedByteBuffer();
        std::shared_ptr<ByteBuffer> getByteBuffer() override;
        size_t fileSize() override ;

        EvioNode getEvent(size_t eventNumber) override ;
        EvioNode getScannedEvent(size_t eventNumber) override ;
        EvioNode getScannedEvent(size_t eventNumber, EvioNodeSource & nodeSource) override ;


        std::shared_ptr<IBlockHeader> getFirstBlockHeader() override ;


    private:


        IEvioReader::ReadWriteStatus readFirstHeader();
        void readDictionary();
        EvioNode scanStructure(size_t eventNumber);
        EvioNode scanStructure(size_t eventNumber, EvioNodeSource & nodeSource);


    public:


        void searchEvent(size_t eventNumber, uint16_t tag, uint8_t num, std::vector<EvioNode> & vec) override ;
        void searchEvent(size_t eventNumber, std::string const & dictName,
                         EvioXMLDictionary & dictionary, std::vector<EvioNode> & vec) override ;


        std::shared_ptr<ByteBuffer> removeEvent(size_t eventNumber) override ;

        std::shared_ptr<ByteBuffer> removeStructure(EvioNode & removeNode) override ;
        std::shared_ptr<ByteBuffer> addStructure(size_t eventNumber, ByteBuffer & addBuffer) override ;

        std::shared_ptr<ByteBuffer> getData(EvioNode & node) override ;
        std::shared_ptr<ByteBuffer> getData(EvioNode & node, bool copy) override ;

        std::shared_ptr<ByteBuffer> getEventBuffer(size_t eventNumber) override ;
        std::shared_ptr<ByteBuffer> getEventBuffer(size_t eventNumber, bool copy) override ;

        std::shared_ptr<ByteBuffer> getStructureBuffer(EvioNode & node) override ;
        std::shared_ptr<ByteBuffer> getStructureBuffer(EvioNode & node, bool copy) override ;


        void close() override ;

        uint32_t  getEventCount() override ;
        uint32_t getBlockCount() override ;

        void toFile(std::string const & fileName) override ;

    };

}

#endif //EVIO_6_0_EVIOCOMPACTREADERV4_H
