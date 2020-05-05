//
// Copyright (c) 2020, Jefferson Science Associates
//
// Thomas Jefferson National Accelerator Facility
// EPSCI Group
//
// 12000, Jefferson Ave, Newport News, VA 23606
// Phone : (757)-269-7100
//

#ifndef EVIO_6_0_BASESTRUCTUREHEADER_H
#define EVIO_6_0_BASESTRUCTUREHEADER_H

#include <vector>
#include <cstring>
#include <cstdint>
#include <sstream>

#include "Util.h"
#include "ByteOrder.h"
#include "TreeNode.h"
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
    class BaseStructureHeader /*: IEvioWriter */ {

        friend class BaseStructure;

      protected:

        /**
         * The length of the structure in ints. This never includes the
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
        uint32_t padding = 0;

        /**
         * The number represents an unsigned byte. Only Banks have a number
         * field in their header, so this is only relevant for Banks.
         */
        uint32_t number = 0;



        void setPadding(uint32_t padding);

        virtual void toArray(uint8_t *bArray, uint32_t offset, ByteOrder & order, uint32_t destMaxSize) {};
        virtual void toVector(std::vector<uint8_t> & bArray, uint32_t offset, ByteOrder & order) {};

      public:

        BaseStructureHeader() = default;
        BaseStructureHeader(uint32_t tag, DataType & dataType, uint32_t num = 0);

        uint32_t getNumber();
        void setNumber(uint32_t number);

        uint32_t getDataTypeValue();
        void setDataType(uint32_t dataType);

        void setDataType(DataType & dataType);
        DataType getDataType();

        string getDataTypeName();
        uint32_t getPadding();

        uint32_t getLength();
        void setLength(uint32_t length);

        uint32_t getTag();
        void setTag(uint32_t tag);

        virtual uint32_t getHeaderLength() {return 0;};
        virtual string   toString() {return "BaseStructureHeader";};
        virtual uint32_t write(ByteBuffer & byteBuffer) {return 0;};
    };
}

#endif //EVIO_6_0_BASESTRUCTUREHEADER_H
