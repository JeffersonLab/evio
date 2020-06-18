//
// Created by Carl Timmer on 6/18/20.
//

#ifndef EVIO_6_0_EVIOCOMPACTREADER_H
#define EVIO_6_0_EVIOCOMPACTREADER_H


#include <fstream>
#include <stdexcept>


#include "IEvioCompactReader.h"
#include "EvioReader.h"
#include "Util.h"


namespace evio {



    /**
     * This class is used to read an evio version 4 formatted file or buffer
     * and extract specific evio containers (bank, seg, or tagseg)
     * with actual data in them given a tag/num pair. It is theoretically thread-safe
     * if synced is true. It is designed to be fast and does <b>NOT</b> do a full deserialization
     * on each event examined.<p>
     *
     * @author timmer
     */
    class EvioCompactReader : public IEvioCompactReader {

    private:

        /** Evio version number (1-4, 6). Obtain this by reading first header. */
        uint32_t evioVersion = 4;

        /**
         * Endianness of the data being read, either
         * {@link java.nio.ByteOrder#BIG_ENDIAN} or
         * {@link java.nio.ByteOrder#LITTLE_ENDIAN}.
         */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LITTLE};

        /** The buffer being read. */
        std::shared_ptr<ByteBuffer> byteBuffer;

        /** Initial position of buffer (0 if file). */
        size_t initialPosition = 0UL;

        //------------------------
        // Object to delegate to
        //------------------------
        std::shared_ptr<IEvioCompactReader> reader;


    public:

        EvioCompactReader(string const & path, bool synced = false);
        EvioCompactReader(std::shared_ptr<ByteBuffer> & byteBuffer, bool synced = false) ;
        EvioCompactReader(std::shared_ptr<ByteBuffer> & byteBuffer, EvioNodeSource & pool, bool synced) ;

    public:

        bool isFile() override;
        bool isCompressed() override;

        void setBuffer(std::shared_ptr<ByteBuffer> & buf) override;
        void setBuffer(std::shared_ptr<ByteBuffer> & buf, EvioNodeSource & pool) override;
        std::shared_ptr<ByteBuffer> setCompressedBuffer(std::shared_ptr<ByteBuffer> & buf, EvioNodeSource & pool) override;

        /*synchronized*/bool isClosed() override;

        ByteOrder getByteOrder() override;
        uint32_t getEvioVersion() override;
        string getPath() override;
        ByteOrder getFileByteOrder() override;

        /*synchronized*/string getDictionaryXML() override ;
        /*synchronized*/EvioXMLDictionary getDictionary() override ;

        bool hasDictionary() override;

//    /** {@inheritDoc} */
//    MappedByteBuffer getMappedByteBuffer() {return reader->getMappedByteBuffer();}

        std::shared_ptr<ByteBuffer> getByteBuffer() override;

        size_t fileSize() override;

        EvioNode getEvent(size_t eventNumber) override;
        EvioNode getScannedEvent(size_t eventNumber) override;
        EvioNode getScannedEvent(size_t eventNumber, EvioNodeSource & nodeSource) override ;

        std::shared_ptr<IBlockHeader> getFirstBlockHeader() override;

        /*synchronized*/void searchEvent(size_t evNumber, uint16_t tag, uint8_t num, std::vector<EvioNode> & vec) override ;

        /*synchronized*/void searchEvent(size_t eventNumber, string const & dictName,
                                 EvioXMLDictionary & dictionary, std::vector<EvioNode> & vec) override ;

        /*synchronized*/ByteBuffer removeEvent(size_t eventNumber) override;
        /*synchronized*/ByteBuffer removeStructure(EvioNode & removeNode) override;
        /*synchronized*/ByteBuffer addStructure(size_t eventNumber, ByteBuffer & addBuffer) override;

        ByteBuffer getData(EvioNode & node) override;
        /*synchronized*/ByteBuffer getData(EvioNode & node, bool copy) override;

        ByteBuffer getEventBuffer(size_t eventNumber) override;
        /*synchronized*/ByteBuffer getEventBuffer(size_t eventNumber, bool copy) override;

        ByteBuffer getStructureBuffer(EvioNode & node) override;
        /*synchronized*/ByteBuffer getStructureBuffer(EvioNode & node, bool copy) override;

        /*synchronized*/void close() override;

        uint32_t getEventCount() override;
        uint32_t getBlockCount() override;

        void toFile(string fileName) override;

//        /** {@inheritDoc} */
//        /*synchronized*/void toFile(File file) {
//                reader->toFile(file);
//        }

    };



}

#endif //EVIO_6_0_EVIOCOMPACTREADER_H