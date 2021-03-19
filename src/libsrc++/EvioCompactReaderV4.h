//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100



#ifndef EVIO_6_0_EVIOCOMPACTREADERV4_H
#define EVIO_6_0_EVIOCOMPACTREADERV4_H


#include <stdexcept>
#include <vector>
#include <memory>
#include <limits>
#include <climits>
#include <string>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <sys/mman.h>
#include <mutex>
#include <sys/mman.h>


#include "ByteBuffer.h"
#include "ByteOrder.h"
#include "BaseStructure.h"
#include "IEvioCompactReader.h"
#include "IEvioReader.h"
#include "EvioReaderV4.h"
#include "IBlockHeader.h"
#include "EvioNode.h"
#include "RecordNode.h"


namespace evio {


    /**
     * This class is used to read an evio format version 4 formatted file or buffer
     * and extract specific evio containers (bank, seg, or tagseg)
     * with actual data in them given a tag/num pair.<p>
     *
     * @date 07/01/2020
     * @author timmer
     */
    class EvioCompactReaderV4 : public IEvioCompactReader {

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
        std::vector<std::shared_ptr<EvioNode>> eventNodes;

        /** Store info of all block headers. */
        std::unordered_map<uint32_t, std::shared_ptr<RecordNode>> blockNodes;

        /**
         * This is the number of events in the file. It is not computed unless asked for,
         * and if asked for it is computed and cached in this variable.
         */
        int32_t eventCount = -1;

        /** Evio version number (1-4). Obtain this by reading first block header. */
        uint32_t evioVersion = 4;

        /** Endianness of the data being read. Initialize to local endian. */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LOCAL};

        /**
         * This is the number of blocks in the file including the empty block at the
         * end of the version 4 files. It is not computed unless asked for,
         * and if asked for it is computed and cached in this variable.
         */
        int32_t blockCount = -1;

        /** Size of the first block header in 32-bit words. Used to read dictionary. */
        uint32_t firstBlockHeaderWords = 0;

        /** The current block header. */
        std::shared_ptr<BlockHeaderV4> blockHeader;

        /** Does the file/buffer have a dictionary? */
        bool hasDict = false;

        /**
         * Version 4 files may have an xml format dictionary in the
         * first event of the first block.
         */
        std::string dictionaryXML;

        /** Dictionary object created from dictionaryXML string. */
        std::shared_ptr<EvioXMLDictionary> dictionary = nullptr;

        /** The buffer being read. */
        std::shared_ptr<ByteBuffer> byteBuffer = nullptr;

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

        /** Object to talk to file. */
        std::ifstream file;

        /** Absolute path of the underlying file. */
        std::string path = "";

        /** File size in bytes. */
        size_t fileBytes = 0;

    public:

        explicit EvioCompactReaderV4(std::string const & path);
        explicit EvioCompactReaderV4(std::shared_ptr<ByteBuffer> & byteBuffer);

        void setBuffer(std::shared_ptr<ByteBuffer> & buf) override ;

        bool isFile() override ;
        bool isCompressed() override ;
        bool isClosed() override ;
        ByteOrder getByteOrder() override ;
        uint32_t getEvioVersion() override ;
        std::string getPath() override ;
        ByteOrder getFileByteOrder() override ;
        std::string getDictionaryXML() override ;
        std::shared_ptr<EvioXMLDictionary> getDictionary() override ;
        bool hasDictionary() override ;

    private:

        void mapFile(std::string const & filename, size_t fileS);
        void generateEventPositionTable();

    public:

        std::shared_ptr<ByteBuffer> getByteBuffer() override;
        size_t fileSize() override ;

        std::shared_ptr<EvioNode> getEvent(size_t eventNumber) override ;
        std::shared_ptr<EvioNode> getScannedEvent(size_t eventNumber) override ;

        std::shared_ptr<IBlockHeader> getFirstBlockHeader() override ;

    private:

        IEvioReader::ReadWriteStatus readFirstHeader();
        void readDictionary();
        std::shared_ptr<EvioNode> scanStructure(size_t eventNumber);

    public:

        void searchEvent(size_t eventNumber, uint16_t tag, uint8_t num,
                         std::vector<std::shared_ptr<EvioNode>> & vec) override ;
        void searchEvent(size_t eventNumber, std::string const & dictName,
                         std::shared_ptr<EvioXMLDictionary> & dictionary,
                         std::vector<std::shared_ptr<EvioNode>> & vec) override ;


        std::shared_ptr<ByteBuffer> removeEvent(size_t eventNumber) override ;

        std::shared_ptr<ByteBuffer> removeStructure(std::shared_ptr<EvioNode> & removeNode) override ;
        std::shared_ptr<ByteBuffer> addStructure(size_t eventNumber, ByteBuffer & addBuffer) override ;

        std::shared_ptr<ByteBuffer> getData(std::shared_ptr<EvioNode> & node) override ;
        std::shared_ptr<ByteBuffer> getData(std::shared_ptr<EvioNode> & node, bool copy) override ;

        std::shared_ptr<ByteBuffer> getData(std::shared_ptr<EvioNode> & node,
                                            std::shared_ptr<ByteBuffer> & buf) override ;
        std::shared_ptr<ByteBuffer> getData(std::shared_ptr<EvioNode> & node,
                                            std::shared_ptr<ByteBuffer> & buf, bool copy) override ;

        std::shared_ptr<ByteBuffer> getEventBuffer(size_t eventNumber) override ;
        std::shared_ptr<ByteBuffer> getEventBuffer(size_t eventNumber, bool copy) override ;

        std::shared_ptr<ByteBuffer> getStructureBuffer(std::shared_ptr<EvioNode> & node) override ;
        std::shared_ptr<ByteBuffer> getStructureBuffer(std::shared_ptr<EvioNode> & node, bool copy) override ;

        void close() override ;

        uint32_t  getEventCount() override ;
        uint32_t getBlockCount() override ;

        void toFile(std::string const & fileName) override ;
    };

}

#endif //EVIO_6_0_EVIOCOMPACTREADERV4_H
