//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100



#ifndef EVIO_6_0_EVIOCOMPACTREADERV6_H
#define EVIO_6_0_EVIOCOMPACTREADERV6_H


#include <stdexcept>
#include <vector>
#include <memory>
#include <mutex>
#include <limits>
#include <string>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <sys/mman.h>


#include "ByteBuffer.h"
#include "ByteOrder.h"
#include "BaseStructure.h"
#include "IEvioCompactReader.h"
#include "IEvioReader.h"
#include "Reader.h"
#include "IBlockHeader.h"
#include "EvioNode.h"
#include "RecordNode.h"


namespace evio {


    /**
     * This class is used to read an evio format version 6 formatted file or buffer.
     * It's essentially a wrapper for the Reader class so the user can have
     * access to the EvioCompactReader methods. It is NOT thread-safe.<p>
     *
     * @date 07/09/2020
     * @author timmer
     */
    class EvioCompactReaderV6 : public IEvioCompactReader {

    private:

        /** The reader object which does all the work. */
        Reader reader;

        /** Is this object currently closed? */
        bool closed = false;

        /** Dictionary object created from dictionaryXML string. */
        std::shared_ptr<EvioXMLDictionary> dictionary = nullptr;

        /** File name if any. */
        std::string path;

    public:

        explicit EvioCompactReaderV6(std::string const & path);
        explicit EvioCompactReaderV6(std::shared_ptr<ByteBuffer> & byteBuffer);

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


    public:

        std::shared_ptr<ByteBuffer> getByteBuffer() override;
        size_t fileSize() override ;

        std::shared_ptr<EvioNode> getEvent(size_t eventNumber) override ;
        std::shared_ptr<EvioNode> getScannedEvent(size_t eventNumber) override ;
        std::shared_ptr<IBlockHeader> getFirstBlockHeader() override ;

    private:

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


#endif //EVIO_6_0_EVIOCOMPACTREADERV6_H
