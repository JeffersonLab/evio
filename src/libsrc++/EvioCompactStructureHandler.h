//
// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_EVIOCOMPACTSTRUCTUREHANDLER_H
#define EVIO_EVIOCOMPACTSTRUCTUREHANDLER_H


#include <fstream>
#include <stdexcept>
#include <mutex>
#include <cinttypes>


#include "IEvioCompactReader.h"
#include "EvioCompactReaderV4.h"
#include "EvioCompactReaderV6.h"
#include "EvioReader.h"
#include "Util.h"


namespace evio {


     /**
      * This class is used to read the bytes of just an evio structure (<b>NOT</b>
      * a full evio formatted file or buffer). It is also capable of
      * adding another structure to or removing it from that structure.
      * The {@link EvioCompactReader} class is similar but reads files and buffers
      * in the complete evio version format.
      * It is theoretically thread-safe.
      * It is designed to be fast and does <b>NOT</b> do a deserialization
      * on the buffer examined.<p>
      *
      * @author timmer
      */
    class EvioCompactStructureHandler {

    private:

        /** Stores structure info. */
        std::shared_ptr<EvioNode> node;

        /** The buffer being read. */
        std::shared_ptr<ByteBuffer> byteBuffer;

        /** Endianness of the data being read. Initialize to local endian. */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LOCAL};

        /** Is this object currently closed? */
        bool closed;

    public:

        explicit EvioCompactStructureHandler(std::shared_ptr<EvioNode> node);
        EvioCompactStructureHandler(std::shared_ptr<ByteBuffer> byteBuffer, DataType & type) ;

        void setBuffer(std::shared_ptr<ByteBuffer> buf, DataType & type);

        bool isClosed();

        std::shared_ptr<ByteBuffer> getByteBuffer() ;
        std::shared_ptr<EvioNode> getStructure();
        std::shared_ptr<EvioNode> getScannedStructure();

        std::vector<std::shared_ptr<EvioNode>> scanStructure();

        std::vector<std::shared_ptr<EvioNode>> searchStructure(uint16_t tag, uint8_t num);
        std::vector<std::shared_ptr<EvioNode>> searchStructure(std::string const & dictName,
                                                               std::shared_ptr<EvioXMLDictionary> dictionary);

        std::shared_ptr<ByteBuffer> removeStructure(std::shared_ptr<EvioNode> removeNode) ;
        std::shared_ptr<ByteBuffer> addStructure(std::shared_ptr<ByteBuffer> addBuffer);

        std::shared_ptr<ByteBuffer> getData(std::shared_ptr<EvioNode> node) ;
        std::shared_ptr<ByteBuffer> getData(std::shared_ptr<EvioNode> node, bool copy) ;

        std::shared_ptr<ByteBuffer> getStructureBuffer(std::shared_ptr<EvioNode> node) ;
        std::shared_ptr<ByteBuffer> getStructureBuffer(std::shared_ptr<EvioNode> node, bool copy) ;

        std::vector<std::shared_ptr<EvioNode>> getNodes();
        std::vector<std::shared_ptr<EvioNode>> getChildNodes();

        ByteOrder getByteOrder();
        void close();

    private:

        void expandBuffer(size_t byteSize);
        void bufferInit(std::shared_ptr<EvioNode> node);
        static std::shared_ptr<EvioNode> extractNode(std::shared_ptr<ByteBuffer> buffer,
                                                     std::shared_ptr<EvioNode> eventNode, DataType & type,
                                                     size_t position, int place, bool isEvent);



    };

}


#endif //EVIO_EVIOCOMPACTSTRUCTUREHANDLER_H
