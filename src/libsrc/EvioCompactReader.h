//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_EVIOCOMPACTREADER_H
#define EVIO_6_0_EVIOCOMPACTREADER_H


#include <fstream>
#include <stdexcept>
#include <mutex>


#include "IEvioCompactReader.h"
#include "EvioCompactReaderV4.h"
#include "EvioCompactReaderV6.h"
#include "EvioReader.h"
#include "Util.h"


namespace evio {



    /**
     * This class is used to read an evio formatted file or buffer
     * and extract specific evio containers (bank, seg, or tagseg)
     * with actual data in them given a tag/num pair. It is theoretically thread-safe
     * if sync is true. It is designed to be fast and does <b>NOT</b> do a full deserialization
     * on each event examined.<p>
     *
     * @date 06/18/2020
     * @author timmer
     */
    class EvioCompactReader : public IEvioCompactReader {

    private:

        /** Evio version number (1-4, 6). Obtain this by reading first header. */
        uint32_t evioVersion = 4;

        /** Endianness of the data being read. Initialize to local endian. */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LOCAL};

        /** The buffer being read. */
        std::shared_ptr<ByteBuffer> byteBuffer;

        /** Initial position of buffer (0 if file). */
        size_t initialPosition = 0UL;

        /** File size in bytes. */
        bool synced = false;

        /** Mutex for thread safety. */
        std::recursive_mutex mtx;

        //------------------------
        // Object to delegate to
        //------------------------
        std::shared_ptr<IEvioCompactReader> reader;


    public:

        EvioCompactReader(std::string const & path, bool sync = false);
        EvioCompactReader(std::shared_ptr<ByteBuffer> & byteBuffer, bool sync = false) ;

    public:

        bool isFile() override;
        bool isCompressed() override;

        void setBuffer(std::shared_ptr<ByteBuffer> & buf) override;

        bool isClosed() override;

        ByteOrder getByteOrder() override;
        uint32_t getEvioVersion() override;
        std::string getPath() override;
        ByteOrder getFileByteOrder() override;

        std::string getDictionaryXML() override ;
        std::shared_ptr<EvioXMLDictionary> getDictionary() override ;
        bool hasDictionary() override;

        std::shared_ptr<ByteBuffer> getByteBuffer() override;

        size_t fileSize() override;

        std::shared_ptr<EvioNode> getEvent(size_t eventNumber) override;
        std::shared_ptr<EvioNode> getScannedEvent(size_t eventNumber) override;
        std::shared_ptr<IBlockHeader> getFirstBlockHeader() override;

        void searchEvent(size_t eventNumber, uint16_t tag, uint8_t num, std::vector<std::shared_ptr<EvioNode>> & vec) override ;
        void searchEvent(size_t eventNumber, std::string const & dictName,
                         std::shared_ptr<EvioXMLDictionary> & dictionary,
                         std::vector<std::shared_ptr<EvioNode>> & vec) override ;

        std::shared_ptr<ByteBuffer> removeEvent(size_t eventNumber) override;
        std::shared_ptr<ByteBuffer> removeStructure(std::shared_ptr<EvioNode> & removeNode) override;
        std::shared_ptr<ByteBuffer> addStructure(size_t eventNumber, ByteBuffer & addBuffer) override;

        std::shared_ptr<ByteBuffer> getData(std::shared_ptr<EvioNode> & node) override;
        std::shared_ptr<ByteBuffer> getData(std::shared_ptr<EvioNode> & node, bool copy) override;

        std::shared_ptr<ByteBuffer> getData(std::shared_ptr<EvioNode> & node,
                                            std::shared_ptr<ByteBuffer> & buf) override;
        std::shared_ptr<ByteBuffer> getData(std::shared_ptr<EvioNode> & node,
                                            std::shared_ptr<ByteBuffer> & buf, bool copy) override;

        std::shared_ptr<ByteBuffer> getEventBuffer(size_t eventNumber) override;
        std::shared_ptr<ByteBuffer> getEventBuffer(size_t eventNumber, bool copy) override;

        std::shared_ptr<ByteBuffer> getStructureBuffer(std::shared_ptr<EvioNode> & node) override;
        std::shared_ptr<ByteBuffer> getStructureBuffer(std::shared_ptr<EvioNode> & node, bool copy) override;

        void close() override;

        uint32_t getEventCount() override;
        uint32_t getBlockCount() override;

        void toFile(std::string const & fileName) override;
    };



}

#endif //EVIO_6_0_EVIOCOMPACTREADER_H
