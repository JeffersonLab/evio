//
// Created by Carl Timmer on 2020-04-02.
//

#ifndef EVIO_6_0_BASESTRUCTURE_H
#define EVIO_6_0_BASESTRUCTURE_H

#include <vector>
#include <cstring>
#include <cstdint>
#include <sstream>
#include <string>
#include <iomanip>
#include <limits>

#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "TreeNode.h"
#include "DataType.h"
#include "StructureType.h"
#include "EvioException.h"
#include "BaseStructureHeader.h"


namespace evio {


    /**
     * This is the base class for all evio structures: Banks, Segments, and TagSegments.
     * It implements <code>MutableTreeNode</code> because a tree representation of
     * events is created when a new event is parsed.<p>
     * Note that using an EventBuilder for the same event in more than one thread
     * can cause problems. For example the boolean lengthsUpToDate in this class
     * would need to be volatile.
     *
     * @author heddle
     * @author timmer - add byte order tracking, make setAllHeaderLengths more efficient
     *
     */
    class BaseStructure : TreeNode<int> {

    protected:

        /** Holds the header of the bank. */
        BaseStructureHeader header;

        /** The raw data of the structure. May contain padding. */
        std::vector<uint8_t> rawBytes;

        /** Used if raw data should be interpreted as shorts. */
        std::vector<int16_t> shortData;

        /** Used if raw data should be interpreted as unsigned shorts. */
        std::vector<uint16_t> ushortData;

        /** Used if raw data should be interpreted as ints. */
        std::vector<int32_t> intData;

        /** Used if raw data should be interpreted as unsigned ints. */
        std::vector<uint32_t> uintData;

        /** Used if raw data should be interpreted as longs. */
        std::vector<int64_t> longData;

        /** Used if raw data should be interpreted as unsigned longs. */
        std::vector<uint64_t> ulongData;

        /** Used if raw data should be interpreted as doubles. */
        std::vector<double> doubleData;

        /** Used if raw data should be interpreted as floats. */
        std::vector<float> floatData;

        /** Used if raw data should be interpreted as composite type. */
        CompositeData *compositeData;

        /**
         * Used if raw data should be interpreted as signed chars.
         * The reason rawBytes is not used directly is because
         * it may be padded and it may not, depending on whether
         * this object was created by EvioReader or by EventBuilder, etc., etc.
         * We don't want to return rawBytes when a user calls getCharData() if there
         * are padding bytes in it.
         */
        std::vector<signed char> charData;

        /** Used if raw data should be interpreted as unsigned chars. */
        std::vector<unsigned char> ucharData;

        //------------------- STRING STUFF -------------------

        /** Used if raw data should be interpreted as a string. */
//        std::vector<char> stringData;

        /** Used if raw data should be interpreted as a string. */
        std::vector<std::string > stringList;

        /**
         * Keep track of end of the last string added to stringData
         * (including null but not padding).
         */
        int stringEnd = 0;

        /**
         * True if char data has non-ascii or non-printable characters,
         * or has too little data to be in proper format.
         */
        bool badStringFormat = false;

        //----------------------------------------------------

        /**
         * The number of stored data items like number of banks, ints, floats, etc.
         * (not the size in ints or bytes). Some items may be padded such as shorts
         * and bytes. This will tell the meaningful number of such data items.
         * In other words, no padding is included.
         * In the case of containers, returns number of bytes not in header.
         */
        size_t numberDataItems = 0;

        /**
         * Endianness of the raw data if appropriate,
         * either {@link ByteOrder#BIG_ENDIAN} or {@link ByteOrder#LITTLE_ENDIAN}.
         */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LITTLE};

        /**
         * Holds the children of this structure. This is used for creating trees
         * for a variety of purposes (not necessarily graphical.)
         */
        std::vector<BaseStructure> children;

        /** Keep track of whether header length data is up-to-date or not. */
        bool lengthsUpToDate = false;

        /** Is this structure a leaf? Leaves are structures with no children. */
        bool isLeaf = true;

    private:

        /**
         * The parent of the structure. If it is an "event", the parent is null.
         * This is used for creating trees for a variety of purposes
         * (not necessarily graphical.)
         */
        BaseStructure parent;

        /** Bytes with which to pad short and byte data. */
        static uint8_t padValues[3];

        /** Number of bytes to pad short and byte data. */
        static uint32_t padCount[4];

        void clearData();

    protected:

        bool getLengthsUpToDate();
        void setLengthsUpToDate(bool lengthsUpToDate);
        uint32_t dataLength();
        void stringsToRawBytes();

    public:

        /**
         * Constructor using a provided header
         *
         * @param header the header to use.
         * @see BaseStructureHeader
         */
        explicit BaseStructure(BaseStructureHeader & header, ByteOrder order = ByteOrder::ENDIAN_LITTLE);

        void transform(BaseStructure &structure);
        BaseStructure & getParent();

        virtual StructureType getStructureType() = 0;

        ByteOrder getByteOrder();

        void setByteOrder(ByteOrder &order);

        bool needSwap();

        string getDescription();

        string toString();

        BaseStructureHeader getHeader();

        size_t write(ByteBuffer & byteBuffer);
        uint32_t getNumberDataItems();
        size_t write(ByteBuffer & byteBuffer);

        uint32_t setAllHeaderLengths();
        bool isContainer();
        uint32_t getTotalBytes();

        std::vector<uint8_t> &getRawBytes();
        void setRawBytes(uint8_t *bytes, uint32_t len);

        std::vector<int16_t>  &getShortData();
        std::vector<uint16_t> &getUShortData();

        std::vector<int32_t>  &getIntData();
        std::vector<uint32_t> &getUIntData();

        std::vector<int64_t>  &getLongData();
        std::vector<uint64_t> &getULongData();

        std::vector<float>  &getFloatData();
        std::vector<double> &getDoubleData();

        std::vector<signed char> & getCharData();
        std::vector<unsigned char> & getUCharData();

        std::vector<string> & getStringData();
        uint32_t unpackRawBytesToStrings();

        static uint32_t stringToRawSize(const string & str);
        static uint32_t stringsToRawSize(std::vector<std::string> const & strings);

        static void stringsToRawBytes(std::vector<std::string> & strings,
                                      std::vector<uint8_t> & bytes);

        static void unpackRawBytesToStrings(std::vector<uint8_t> & bytes, size_t offset,
                                            std::vector<string> & strData);
        static void unpackRawBytesToStrings(std::vector<uint8_t> &  bytes,
                                            size_t offset, size_t maxLength,
                                            std::vector<string> & strData);

        static void unpackRawBytesToStrings(ByteBuffer & buffer,
                                            size_t pos, size_t length,
                                            std::vector<string> & strData);

        void updateIntData();
        void updateUIntData();
        void updateShortData();
        void updateUShortData();
        void updateLongData();
        void updateULongData();
        void updateCharData();
        void updateUCharData();
        void updateFloatData();
        void updateDoubleData();
        void updateStringData();




            private:

        static void stringBuilderToStrings(std::string const & strData, bool onlyGoodChars,
                                           std::vector<std::string> & strings);


        };
}


#endif //EVIO_6_0_BASESTRUCTURE_H
