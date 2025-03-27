//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_BASESTRUCTUREHEADER_H
#define EVIO_6_0_BASESTRUCTUREHEADER_H


#include <vector>
#include <cstring>
#include <cstdint>
#include <sstream>


#include "Util.h"
#include "ByteOrder.h"
#include "DataType.h"
#include "EvioException.h"


namespace evio {

    /**
     * This the  header for the base structure (<code>BaseStructure</code>).
     * It does not contain the raw data, just the header.
     * The three headers for the actual structures found in evio
     * (BANK, SEGMENT, and TAGSEMENT) all extend this.
     *
     * @author heddle (original Java version)
     * @author timmer
     * @date 4/2/2020
     */
    class BaseStructureHeader {

        friend class BaseStructure;
        friend class CompositeData;
        friend class EvioReader;
        friend class EvioReaderV4;
        friend class EventHeaderParser;
        friend class StructureTransformer;

    protected:

        /**
         * The length of the structure in 32-bit words. This never includes the
         * first header word itself (which contains the length).
         */
        uint32_t length = 0;

        /** The structure tag. */
        uint32_t tag = 0;

        /** The data type of the structure. */
        DataType dataType {DataType::UNKNOWN32};

        /**
         * The amount of padding bytes when storing short or byte data.
         * Allowed value is 0, 1, 2, or 3 (0,2 for shorts and 0-3 for bytes)
         * and is stored in the upper 2 bits of the dataType when written out.
         */
        uint8_t padding = 0;

        /**
         * The number represents an unsigned byte. Only Banks have a number
         * field in their header, so this is only relevant for Banks.
         */
        uint8_t number = 0;

    protected:

        void setPadding(uint8_t pad);
        void copy(std::shared_ptr<BaseStructureHeader> head);

    public:

        BaseStructureHeader() = default;
        BaseStructureHeader(uint16_t tag, DataType const & dataType, uint8_t num = 0);
        virtual ~BaseStructureHeader() = default;

        uint8_t getNumber() const;
        void setNumber(uint8_t number);

        uint32_t getDataTypeValue() const;
        void setDataType(uint32_t type);

        void setDataType(DataType const & type);
        DataType getDataType() const;

        std::string getDataTypeName() const;
        uint8_t getPadding() const;

        uint32_t getLength() const;
        void setLength(uint32_t len);

        uint16_t getTag() const;
        void setTag(uint16_t tag);

        /**
         * Get the length of the structure's data in 32 bit ints (not counting the header words).
         * @return Get the length of the structure's data in 32 bit ints (not counting the header words).
         */
        virtual uint32_t getDataLength()   {return 0;};

        /**
         * Get the length of the structure's header in ints. This includes the first header word itself
         * (which contains the length) and in the case of banks, it also includes the second header word.
         * @return  length of the structure's header in ints (2 for banks, 1 for segments and tagsegments).
         */
        virtual uint32_t getHeaderLength() {return 0;};

        /**
         * Obtain a string representation of the base structure header.
         * @return a string representation of the base structure header.
         */
        virtual std::string   toString() {return "BaseStructureHeader";};


        /**
         * Write myself out into a byte buffer.
         * This write is relative - i.e., it uses the current position of the buffer.
         *
         * @param dest the byteBuffer to write to.
         * @return the number of bytes written, which for a BankHeader is 8 and for
         *         a SegmentHeader or TagSegmentHeader is 4.
         */
        virtual size_t write(std::shared_ptr<ByteBuffer> & dest) {return 0;}

        /**
        * Write myself out into a byte buffer.
        * This write is relative - i.e., it uses the current position of the buffer.
        *
        * @param dest the byteBuffer to write to.
         * @return the number of bytes written, which for a BankHeader is 8 and for
         *         a SegmentHeader or TagSegmentHeader is 4.
        */
        virtual size_t write(ByteBuffer & dest) {return 0;};

        /**
         * Write myself out as evio format data
         * into the given byte array in the specified byte order.
         *
         * @param dest array into which evio data is written (destination).
         * @param order  byte order in which to write the data.
         * @return the number of bytes written, which for a BankHeader is 8 and for
         *         a SegmentHeader or TagSegmentHeader is 4.
         * @throws EvioException if destination array too small to hold data.
         */
        virtual size_t write(uint8_t *dest, ByteOrder const & order) {return 0;};

    };
}

#endif //EVIO_6_0_BASESTRUCTUREHEADER_H
