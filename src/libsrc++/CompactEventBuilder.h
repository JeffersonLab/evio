//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_COMPACTEVENTBUILDER_H
#define EVIO_6_0_COMPACTEVENTBUILDER_H


#include <vector>
#include <memory>
#include <cstring>


#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "DataType.h"
#include "EvioNode.h"
#include "EvioSwap.h"
#include "Util.h"
#include "FileHeader.h"
#include "RecordHeader.h"


namespace evio {

    /**
     * This class is used for creating events
     * and their substructures while minimizing use of objects.
     * Evio format data of a single event is written directly,
     * and sequentially, into a buffer. The buffer contains
     * only the single event, not the full, evio file format.<p>
     *
     * The methods of this class are not synchronized so it is NOT
     * threadsafe. This is done for speed. The buffer retrieved by
     * {@link #getBuffer()} is ready to read.
     *
     * @author timmer
     * @date 2/6/2014 (Java)
     * @date 7/5/2020 (C++)
     */
    class CompactEventBuilder {

    private:

        /** Buffer in which to write. */
        std::shared_ptr<ByteBuffer> buffer = nullptr;

        /** Byte array which backs the buffer. */
        uint8_t * array = nullptr;

        /** Offset into backing array. */
        size_t arrayOffset = 0;

        /** Current writing position in the buffer. */
        size_t position = 0;

        /** Byte order of the buffer, convenience variable. */
        ByteOrder order {ByteOrder::ENDIAN_LOCAL};

        /** Did this object create the byte buffer? */
        bool createdBuffer = false;

        /** When writing to buffer, generate EvioNode objects as evio
         *  structures are being created. */
        bool generateNodes = false;

        /** If {@link #generateNodes} is {@code true}, then store
         *  generated node objects in this list (in buffer order. */
        std::vector<std::shared_ptr<EvioNode>> nodes;

        /** Maximum allowed evio structure levels in buffer. */
        static const uint32_t MAX_LEVELS = 50;

        /** Number of bytes to pad short and byte data. */
        static const uint32_t padCount[];


        /**
         * This class stores information about an evio structure
         * (bank, segment, or tagsegment) which allows us to write
         * a length or padding in that structure in the buffer.
         */
        class StructureContent {
          public:

            /** Starting position of this structure in the buffer. */
            size_t pos = 0;
            /** Keep track of amount of primitive data written for finding padding.
             *  Can be either bytes or shorts. */
            uint32_t dataLen = 0;
            /** Padding for byte and short data. */
            uint32_t padding = 0;
            /** Type of evio structure this is. */
            DataType type {DataType::BANK};
            /** Type of evio structures or data contained. */
            DataType dataType {DataType::UNKNOWN32};

            StructureContent() = default;

            void setData(size_t pos, DataType const & type, DataType const & dataType) {
                this->pos = pos;
                this->type = type;
                this->dataType = dataType;
                padding = dataLen = 0;
            }
        };


        /** The top (first element) of the vector is the first structure
         *  created or the event bank.
         *  Each level is the parent of the next one down (index + 1) */
        std::vector<std::shared_ptr<StructureContent>> stackArray;

        /** Each element of the vector is the total length of all evio
         *  data in a structure including the full length of that
         *  structure's header. There is one length for each level
         *  of evio structures with the 0th element being the top
         *  structure (or event) level. */
        std::vector<uint32_t> totalLengths;

        /** Current evio structure being created. */
        std::shared_ptr<StructureContent> currentStructure = nullptr;

        /** Level of the evio structure currently being created.
         *  Starts at 0 for the event bank. */
        int32_t currentLevel = -1;


    public:

        CompactEventBuilder(size_t bufferSize, ByteOrder const & order, bool generateNodes = false);
        explicit CompactEventBuilder(std::shared_ptr<ByteBuffer> buffer, bool generateNodes = false);

        void setBuffer(std::shared_ptr<ByteBuffer> buffer, bool generateNodes = false);

    private:

        void initBuffer(std::shared_ptr<ByteBuffer> buffer, bool generateNodes);

    public:

        std::shared_ptr<ByteBuffer> getBuffer();

        size_t getTotalBytes() const;

        std::shared_ptr<EvioNode> openSegment(uint16_t tag, DataType const & dataType);
        std::shared_ptr<EvioNode> openTagSegment(int tag, DataType dataType);
        std::shared_ptr<EvioNode> openBank(uint16_t tag, DataType const & dataType, uint8_t num);

        bool closeStructure();
        void closeAll();

        void setTopLevelTag(short tag);

    private:

        void addToAllLengths(uint32_t len);

        void setCurrentHeaderLength(uint32_t len);
        void setCurrentHeaderPadding(uint32_t padding);

        void writeHeader(std::shared_ptr<EvioNode> & node);
        void writeNode(std::shared_ptr<EvioNode> & node, bool swapData);

    public:

        void addEvioNode(std::shared_ptr<EvioNode> node);

        void addByteData(uint8_t * data, uint32_t len);
        void addIntData(uint32_t * data, uint32_t len);
        void addShortData(uint16_t * data, uint32_t len);
        void addLongData(uint64_t * data, uint32_t len);
        void addFloatData(float * data, uint32_t len);
        void addDoubleData(double * data, uint32_t len);
        void addStringData(std::vector<std::string> & strings);
        void addCompositeData(std::vector<std::shared_ptr<CompositeData>> & data);

        void toFile(std::string const & fileName);

    };


}


#endif //EVIO_6_0_COMPACTEVENTBUILDER_H
