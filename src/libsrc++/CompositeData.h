//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_COMPOSITEDATA_H
#define EVIO_6_0_COMPOSITEDATA_H


#include <vector>
#include <cstring>
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <climits>
#include <ios>
#include <iomanip>
#include <exception>
#include <stdexcept>
#include <sstream>
#include <memory>
#include <algorithm>


#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "DataType.h"
#include "BankHeader.h"
#include "SegmentHeader.h"
#include "TagSegmentHeader.h"
#include "EventHeaderParser.h"
#include "EvioNode.h"


#undef COMPOSITE_DEBUG
#undef COMPOSITE_PRINT

// /////////////////////
// Doxygen:
// For some reason doxygen does NOT extract the documentation for the methods of CompositeData class.
// To get around this, just copied the doxygen doc from .cpp to this file.
// ////////////////////

namespace evio {

   /**
    * COMPOSITE DATA:<p>
    *
    * This is a new type of data (value = 0xf) which originated with Hall B.
    * It is a composite type and allows for possible expansion in the future
    * if there is a demand. Basically it allows the user to specify a custom
    * format by means of a string - stored in a tagsegment. The data in that
    * format follows in a bank. The routine to swap this data must be provided
    * by the definer of the composite type - in this case Hall B. The swapping
    * function is plugged into this evio library's swapping routine.<p>
    *
    * Here's what the data looks like.
    * <pre>
    *
    * MSB(31)                          LSB(0)
    * <---  32 bits ------------------------>
    * _______________________________________
    * |  tag    | type |    length          | --> tagsegment header
    * |_________|______|____________________|
    * |        Data Format String           |
    * |                                     |
    * |_____________________________________|
    * |              length                 | \
    * |_____________________________________|  \  bank header
    * |       tag      |  type   |   num    |  /
    * |________________|_________|__________| /
    * |               Data                  |
    * |                                     |
    * |_____________________________________|
    * </pre>
    *
    * The beginning tagsegment is a normal evio tagsegment containing a string
    * (type = 0x3). Currently its type and tag are not used - at least not for
    * data formatting.
    * The bank is a normal evio bank header with data following.
    * The format string is used to read/write this data so that takes care of any
    * padding that may exist. As with the tagsegment, the tags and type are ignored.<p>
    *
    * This is the class defining the composite data type.
    * It is a mixture of header and raw data.
    * This class is <b>NOT</b> thread safe.
    *
    * @author timmer
    * @date 4/17/2020
    */
    class CompositeData {


        /** Structure for internal use only. */
        typedef struct {
            /** Index of ifmt[] element containing left parenthesis. */
            int left;
            /** How many times format in parenthesis must be repeated. */
            int nrepeat;
            /* Right parenthesis counter, or how many times format in parenthesis already repeated. */
            int irepeat;
        } LV;


    public:

        /** This class holds a single, primitive type data item. */
        union SingleMember {

        public:

            // Data being stored

            /** Float value. */
            float     flt;
            /** Double value. */
            double    dbl;
            /** Unsigned long value. */
            uint64_t ul64;
            /** Long value. */
            int64_t   l64;
            /** Unsigned int value. */
            uint32_t ui32;
            /** Signed int value. Used for N, Hollerit. */
            int32_t   i32;
            /** Unsigned short value. */
            uint16_t us16;
            /** Signed int value. Used for n. */
            int16_t   s16;
            /** Unsigned byte value. */
            uint8_t   ub8;
            /** Signed byte value. Used for m. */
            int8_t     b8;
            /** Are we storing strings? */
            bool      str;
        };


        /**
         * This class defines an individual data item.
         *  The class, {@link CompositeData#Data}, which defines all data in a composite data object,
         *  contains a vector of these, individual data items.
         *  There is a separation between members of primitive types which are stored as
         *  {@link CompositeData#SingleMember} objects, from those of strings which are stored in a vector of strings.
         *  This is done since it doesn't make sense to include a vector in the union with primitives.
         */
        class DataItem {

        public:

            /** Place for holding a primitive type. */
            SingleMember item = {0};
            /** Place for holding strings, NOT a primitive type. */
            std::vector<std::string> strVec {};

            /** No arg constructor. */
            DataItem() = default;

            /**
             * Copy constructor.
             * @param other object to copy.
             */
            DataItem(DataItem const &other) {
                item   = other.item;
                strVec = other.strVec;
            };

        };



        /**
         * This class is used to provide all data when constructing a CompositeData object.
         * Doing things this way keeps all internal data members self-consistent.
         */
        class Data  {

        private:

            /** Encapsulating class needs access. */
            friend class CompositeData;

            /** Keep a running total of how many bytes the data take without padding.
             *  This includes both the dataItems and N values and thus assumes all N
             *  values will be written. */
            uint32_t dataBytes = 0;

            /** The number of bytes needed to complete a 4-byte boundary. */
            uint32_t paddingBytes = 0;

            /** Convenient way to calculate padding. */
            uint32_t pads[4] = {0,3,2,1};

            /** List of data objects. */
            std::vector<DataItem> dataItems;

            /** List of types of data objects. */
            std::vector<DataType> dataTypes;

            /** List of "N" (32 bit) values - multiplier values read from data instead
             *  of being part of the format string. */
            std::vector<int32_t> Nlist;

            /** List of "n" (16 bit) values - multiplier values read from data instead
            *  of being part of the format string. */
            std::vector<int16_t> nlist;

            /** List of "m" (8 bit) values - multiplier values read from data instead
             *  of being part of the format string. */
            std::vector<int8_t> mlist;

            /** Though currently not used, this is the tag in the segment containing format string. */
            uint16_t formatTag = 0;

            /** Though currently not used, this is the tag in the bank containing data. */
            uint16_t dataTag = 0;

            /** Though currently not used, this is the num in the bank containing data. */
            uint8_t dataNum = 0;

        public:

            /** Constructor. */
            Data() {
                dataItems.reserve(200);
                dataTypes.reserve(200);
                Nlist.reserve(100);
                nlist.reserve(100);
                mlist.reserve(100);
            }

        private:

            /**
             * This method keeps track of the raw data size in bytes.
             * Also keeps track of padding.
             * @param bytes number of bytes to add.
             */
            void addBytesToData(uint32_t bytes) {
                dataBytes += bytes;
                // Pad to 4 byte boundaries.
                paddingBytes = pads[dataBytes%4];
            }

        public:
            /**
             * This method sets the tag in the segment containing the format string.
             * @param tag tag in segment containing the format string.
             */
            void setFormatTag(uint16_t tag) {formatTag = tag;}

            /**
             * This method sets the tag in the bank containing the data.
             * @param tag tag in bank containing the data.
             */
            void setDataTag(uint16_t tag) {dataTag = tag;}

            /**
             * This method sets the num in the bank containing the data.
             * @param num num in bank containing the data.
             */
            void setDataNum(uint8_t num) {dataNum = num;}

            /**
             * This method gets the tag in the segment containing the format string.
             * @return tag in segment containing the format string.
             */
            uint16_t getFormatTag() const {return formatTag;}

            /**
             * This method gets the tag in the bank containing the data.
             * @return tag in bank containing the data.
             */
            uint16_t getDataTag() const {return dataTag;}

            /**
             * This method gets the num in the bank containing the data.
             * @return num in bank containing the data.
             */
            uint8_t getDataNum() const {return dataNum;}

            /**
             * This method gets the raw data size in bytes.
             * @return raw data size in bytes.
             */
            uint32_t getDataSize() const {return (dataBytes + paddingBytes);}

            /**
             * This method gets the padding (in bytes).
             * @return padding (in bytes).
             */
            uint32_t getPadding() const {return paddingBytes;}

            /**
             * This method adds an "N" or multiplier value to the data.
             * It needs to be added in sequence with other data.
             * @param N  N or multiplier value
             */
            void addN(uint32_t N) {
                Nlist.push_back(N);
                DataItem mem;
                mem.item.ui32 = N;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::UINT32);
                addBytesToData(4);
            }

            /**
             * This method adds an "n" or multiplier value to the data.
             * It needs to be added in sequence with other data.
             * @param n  n or multiplier value
             */
            void addn(uint16_t n) {
                nlist.push_back(n);
                DataItem mem;
                mem.item.us16 = n;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::USHORT16);
                addBytesToData(2);
            }

            /**
             * This method adds an "m" or multiplier value to the data.
             * It needs to be added in sequence with other data.
             * @param m  m or multiplier value
             */
            void addm(uint8_t m) {
                mlist.push_back(m);
                DataItem mem;
                mem.item.ub8 = m;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::UCHAR8);
                addBytesToData(1);
            }

            //-----------------------------------------------------

            /**
             * Add a signed 32 bit integer to the data.
             * @param i integer to add.
             */
            void addInt(int32_t i) {
                DataItem mem;
                mem.item.i32 = i;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::INT32);
                addBytesToData(4);
            }

            /**
             * Add an vector of signed 32 bit integers to the data.
             * @param i vector of integers to add.
             */
            void addInt(std::vector<int32_t> const & i) {
                for (auto ii : i) {
                    DataItem mem;
                    mem.item.i32 = ii;
                    dataItems.push_back(mem);
                    dataTypes.push_back(DataType::INT32);
                }
                addBytesToData(4 * i.size());
            }

            /**
             * Add an unsigned 32 bit integer to the data.
             * @param i unsigned integer to add.
             */
            void addUint(uint32_t i) {
                DataItem mem;
                mem.item.ui32 = i;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::UINT32);
                addBytesToData(4);
            }

            /**
             * Add an vector of unsigned 32 bit integers to the data.
             * @param i vector of unsigned integers to add.
             */
            void addUint(std::vector<uint32_t> const & i) {
                for (auto ii : i) {
                    DataItem mem;
                    mem.item.ui32 = ii;
                    dataItems.push_back(mem);
                    dataTypes.push_back(DataType::UINT32);
                }
                addBytesToData(4 * i.size());
            }

            //-----------------------------------------------------

            /**
             * Add a 16 bit short to the data.
             * @param s short to add.
             */
            void addShort(int16_t s) {
                DataItem mem;
                mem.item.s16 = s;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::SHORT16);
                addBytesToData(2);
            }

            /**
             * Add an vector of 16 bit shorts to the data.
             * @param s vector of shorts to add.
             */
            void addShort(std::vector<int16_t> const & s) {
                for (auto ii : s) {
                    DataItem mem;
                    mem.item.s16 = ii;
                    dataItems.push_back(mem);
                    dataTypes.push_back(DataType::SHORT16);
                }
                addBytesToData(2 * s.size());
            }

            /**
             * Add an unsigned 16 bit short to the data.
             * @param s unsigned short to add.
             */
            void addUShort(uint16_t s) {
                DataItem mem;
                mem.item.us16 = s;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::USHORT16);
                addBytesToData(2);
            }

            /**
             * Add an vector of unsigned 16 bit shorts to the data.
             * @param s vector of unsigned shorts to add.
             */
            void addUShort(std::vector<uint16_t> const & s) {
                for (auto ii : s) {
                    DataItem mem;
                    mem.item.us16 = ii;
                    dataItems.push_back(mem);
                    dataTypes.push_back(DataType::USHORT16);
                }
                addBytesToData(2 * s.size());
            }

            //-----------------------------------------------------

            /**
             * Add a 64 bit long to the data.
             * @param l long to add.
             */
            void addLong(int64_t l) {
                DataItem mem;
                mem.item.l64 = l;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::LONG64);
                addBytesToData(8);
            }

            /**
             * Add an vector of 64 bit longs to the data.
             * @param l vector of longs to add.
             */
            void addLong(std::vector<int64_t> const & l) {
                for (auto ii : l) {
                    DataItem mem;
                    mem.item.l64 = ii;
                    dataItems.push_back(mem);
                    dataTypes.push_back(DataType::LONG64);
                }
                addBytesToData(8 * l.size());
            }

            /**
             * Add an unsigned 64 bit long to the data.
             * @param l unsigned long to add.
             */
            void addULong(uint64_t l) {
                DataItem mem;
                mem.item.ul64 = l;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::ULONG64);
                addBytesToData(8);
            }

            /**
             * Add an vector of unsigned 64 bit longs to the data.
             * @param l vector of unsigned longs to add.
             */
            void addULong(std::vector<uint64_t> const & l) {
                for (auto ii : l) {
                    DataItem mem;
                    mem.item.ul64 = ii;
                    dataItems.push_back(mem);
                    dataTypes.push_back(DataType::ULONG64);
                }
                addBytesToData(8 * l.size());
            }

            //-----------------------------------------------------

            /**
             * Add an 8 bit byte (char) to the data.
             * @param b byte to add.
             */
            void addChar(int8_t b) {
                DataItem mem;
                mem.item.b8 = b;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::CHAR8);
                addBytesToData(1);
            }

            /**
             * Add an vector of 8 bit bytes (chars) to the data.
             * @param b vector of bytes to add.
             */
            void addChar(std::vector<int8_t> const & b) {
                for (auto ii : b) {
                    DataItem mem;
                    mem.item.b8 = ii;
                    dataItems.push_back(mem);
                    dataTypes.push_back(DataType::CHAR8);
                }
                addBytesToData(b.size());

            }

            /**
             * Add an unsigned 8 bit byte (uchar) to the data.
             * @param b unsigned byte to add.
             */
            void addUChar(uint8_t b) {
                DataItem mem;
                mem.item.ub8 = b;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::UCHAR8);
                addBytesToData(1);
            }

            /**
             * Add an vector of unsigned 8 bit bytes (uchars) to the data.
             * @param b vector of unsigned bytes to add.
             */
            void addUChar(std::vector<uint8_t> const & b) {
                for (auto ii : b) {
                    DataItem mem;
                    mem.item.ub8 = ii;
                    dataItems.push_back(mem);
                    dataTypes.push_back(DataType::UCHAR8);
                }
                addBytesToData(b.size());
            }

            //-----------------------------------------------------

            /**
             * Add a 32 bit float to the data.
             * @param f float to add.
             */
            void addFloat(float f) {
                DataItem mem;
                mem.item.flt = f;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::FLOAT32);
                addBytesToData(4);
            }

            /**
             * Add an vector of 32 bit floats to the data.
             * @param f vector of floats to add.
             */
            void addFloat(std::vector<float> const & f) {
                for (auto ff : f) {
                    DataItem mem;
                    mem.item.flt = ff;
                    dataItems.push_back(mem);
                    dataTypes.push_back(DataType::FLOAT32);
                }
                addBytesToData(4 * f.size());
            }

            /**
             * Add a 64 bit double to the data.
             * @param d double to add.
             */
            void addDouble(double d) {
                DataItem mem;
                mem.item.dbl = d;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::DOUBLE64);
                addBytesToData(8);
            }

            /**
              * Add an vector of 64 bit doubles to the data.
              * @param d vector of doubles to add.
              */
            void addDouble(std::vector<double> const & d) {
                for (auto dd : d) {
                    DataItem mem;
                    mem.item.dbl = dd;
                    dataItems.push_back(mem);
                    dataTypes.push_back(DataType::DOUBLE64);
                }
                addBytesToData(8 * d.size());
            }

            //-----------------------------------------------------

            /**
             * Add a single string to the data.
             * @param s string to add.
             */
            void addString(std::string const & s) {
                std::vector<std::string> v {s};
                DataItem mem;
                mem.item.str = true;
                mem.strVec = v;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::CHARSTAR8);
                addBytesToData(Util::stringsToRawSize(v));
            }

            /**
             * Add an vector of strings to the data.
             * @param s vector of strings to add.
             */
            void addString(std::vector<std::string> const & s) {
                DataItem mem;
                mem.item.str = true;
                mem.strVec = s;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::CHARSTAR8);
                addBytesToData(Util::stringsToRawSize(s));
            }
        };


    private:

        /** String containing data format. */
        std::string format;

        /** List of unsigned shorts obtained from transforming format string. */
        std::vector<uint16_t> formatInts;

        /** List of extracted data items from raw bytes. */
        std::vector<DataItem> items;

        /** List of the types of the extracted data items. */
        std::vector<DataType> types;

        /** List of the "N" (32 bit) values extracted from the raw data. */
        std::vector<int32_t> NList;

        /** List of the "n" (16 bit) values extracted from the raw data. */
        std::vector<int16_t> nList;

        /** List of the "m" (8 bit) values extracted from the raw data. */
        std::vector<int8_t> mList;

        /** Tagsegment header of tagsegment containing format string. */
        std::shared_ptr<TagSegmentHeader> tsHeader;

        /** Bank header of bank containing data. */
        std::shared_ptr<BankHeader> bHeader;

        /** The entire raw data of the composite item - both tagsegment and data bank. */
        std::vector<uint8_t> rawBytes;

        /** Length of only data in bytes (not including padding). */
        uint32_t dataBytes = 0;

        /** Length of only data padding in bytes. */
        uint32_t dataPadding = 0;

        /** Offset (in 32bit words) in rawBytes to place of data. */
        uint32_t dataOffset = 0;

        /** Byte order of raw bytes. */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LOCAL};

        /** Index used in getting data items from the {@link #items} list. */
        /* mutable ? */
        uint32_t getIndex = 0;


//        // Only constructors with raw data or a vector of Data as an arg
//        // are used. That way there is never an "empty" CompositeData object.
//        // Thus, each object will have a raw byte representation generated.

        CompositeData() = default;

        /**
         * Constructor used for creating this object from scratch.
         *
         * @param format format string defining data.
         * @param data data in given format.
         *
         * @throws EvioException if improper format string.
         */
        CompositeData(std::string const & format, const Data & data);

        /**
         * Constructor used for creating this object from scratch.
         *
         * @param format    format string defining data.
         * @param data      data in given format.
         * @param formatTag tag used in tagsegment containing format.
         * @param dataTag   tag used in bank containing data.
         * @param dataNum   num used in bank containing data.
         * @param order     byte order in which data is stored in internal buffer.
         *                  Defaults to local endian.
         *
         * @throws EvioException data or format arg = null,
         *                       if improper format string.
         */
        CompositeData(std::string const & format,
                      const Data & data,
                      uint16_t formatTag,
                      uint16_t dataTag, uint8_t dataNum,
                      ByteOrder const & order = ByteOrder::ENDIAN_LOCAL);

        /**
         * Constructor used when reading existing data. Data is copied in.
         *
         * @param bytes  raw data defining this composite type item.
         * @param byteOrder byte order of bytes.
         */
        CompositeData(uint8_t *bytes, ByteOrder const & byteOrder);

        /**
         * Constructor used when reading existing data. Data is copied in.
         * @param bytes ByteBuffer of raw data.
         */
        CompositeData(ByteBuffer & bytes);

        /**
         * Method to return a shared pointer to a constructed object of this class.
         * @return a shared pointer to a constructed object of this class.
         */
        static std::shared_ptr<CompositeData> getInstance();


    public:


        /**
         * Method to return a shared pointer to a constructed object of this class.
         * @param format data format string.
         * @param data object containing composite data description.
         * @return shared pointer of CompositeData object.
         */
        static std::shared_ptr<CompositeData> getInstance(std::string & format, const Data & data);

        /**
         * Method to return a shared pointer to a constructed object of this class.
         * @param format data format string.
         * @param data object containing composite data description.
         * @param formatTag tag of evio segment containing format.
         * @param dataTag   tag of evio bank containing data.
         * @param dataNum   num of evio bank containing data.
         * @param order desired byteOrder of generated raw data. Defaults to local endian.
         * @return shared pointer of CompositeData object.
         */
        static std::shared_ptr<CompositeData> getInstance(std::string & format,
                                                          const Data & data,
                                                          uint16_t formatTag,
                                                          uint16_t dataTag, uint8_t dataNum,
                                                          ByteOrder const & order = ByteOrder::ENDIAN_LOCAL);

        /**
         * Method to return a shared pointer to a constructed object of this class.
         * @param bytes pointer to raw data.
         * @param order byte order of raw data.
         * @return shared pointer of CompositeData object.
         */
        static std::shared_ptr<CompositeData> getInstance(uint8_t *bytes,
                                                          ByteOrder const & order = ByteOrder::ENDIAN_LOCAL);

        /**
         * Method to return a shared pointer to a constructed object of this class.
         * @param bytes ByteBuffer of raw data.
         * @return shared pointer of CompositeData object.
         */
        static std::shared_ptr<CompositeData> getInstance(ByteBuffer & bytes);

        /**
         * This method parses an array of raw bytes into an vector of CompositeData objects.
         * Vector is initially cleared.
         *
         * @param bytes     array of raw bytes to parse.
         * @param bytesSize size in bytes of bytes.
         * @param order     byte order of raw bytes.
         * @param list      vector that will hold all parsed CompositeData objects.
         * @throws EvioException if null args or bad format of raw data.
         */
        static void parse(uint8_t *bytes, size_t bytesSize, ByteOrder const & order,
                          std::vector<std::shared_ptr<CompositeData>> & list);

        /**
         * This method generates raw bytes of evio format from a vector of CompositeData objects.
         * The returned vector consists of gluing together all the individual objects' rawByte arrays.
         * All CompositeData element must be of the same byte order.
         *
         * @param data     vector of CompositeData objects to turn into bytes (input).
         * @param rawBytes vector of raw, evio format bytes (output).
         * @param order    byte order of output.
         */
        static void generateRawBytes(std::vector<std::shared_ptr<CompositeData>> & data,
                                     std::vector<uint8_t> & rawBytes, ByteOrder & order);

        /**
         * This method helps the CompositeData object creator by
         * finding the proper format string parameter for putting
         * this array of Strings into its data.
         * The format is in the form "Ma" where M is an actual integer.
         * Warning, in this case, M may not be greater than 15.
         * If you want a longer string or array of strings, use the
         * format "Na" with a literal N. The N value can be added
         * through {@link CompositeData::Data#addN(uint32_t)}
         *
         * @param strings array of strings to eventually put into a
         *                CompositeData object.
         * @return string representing its format to be used in the
         *                CompositeData object's format string; empty string if arg has 0 length.
         */
        static std::string stringsToFormat(std::vector<std::string> strings);

        /**
         * <pre>
         *  This method was originally called called "eviofmt".
         *  It transforms a composite, format-containing
         *  ASCII string to a vector of shorts codes. It is to be used
         *  in conjunction with {@link #swapData} to swap the endianness of
         *  composite data.
         *
         *   format code bits <- format in ascii form
         *    [15:14] [13:8] [7:0]
         *      Nnm      #     0           #'('
         *        0      0     0            ')'
         *      Nnm      #     1           #'i'   unsigned int
         *      Nnm      #     2           #'F'   floating point
         *      Nnm      #     3           #'a'   8-bit char (C++)
         *      Nnm      #     4           #'S'   short
         *      Nnm      #     5           #'s'   unsigned short
         *      Nnm      #     6           #'C'   char
         *      Nnm      #     7           #'c'   unsigned char
         *      Nnm      #     8           #'D'   double (64-bit float)
         *      Nnm      #     9           #'L'   long long (64-bit int)
         *      Nnm      #    10           #'l'   unsigned long long (64-bit int)
         *      Nnm      #    11           #'I'   int
         *      Nnm      #    12           #'A'   hollerit (4-byte char with int endining)
         *
         *   NOTES:
         *    1. The number of repeats '#' must be the number between 2 and 63, number 1 assumed by default
         *    2. If the number of repeats is symbol 'N' instead of the number, it will be taken from data assuming 'int32' format;
         *       if the number of repeats is symbol 'n' instead of the number, it will be taken from data assuming 'int16' format;
         *       if the number of repeats is symbol 'm' instead of the number, it will be taken from data assuming 'int8' format;
         *       Two bits Nnm [15:14], if not zero, requires to take the number of repeats from data in appropriate format:
         *            [01] means that number is integer (N),
         *            [10] - short (n),
         *            [11] - char (m)
         *    3. If format ends but end of data did not reach, format in last parenthesis
         *       will be repeated until all data processed; if there are no parenthesis
         *       in format, data processing will be started from the beginnig of the format
         *       (FORTRAN agreement)
         * </pre>
         *
         *  @param formatStr composite data format string
         *  @param ifmt      unsigned short vector to hold transformed format
         *
         *  @return the number of shorts in ifmt[] (positive)
         *  @return -1 to -8 for improper format string
         *
         *  @author Sergey Boiarinov
         */
        static int compositeFormatToInt(const std::string & formatStr, std::vector<uint16_t> & ifmt);



        /**
         * Get the data padding (0, 1, 2, or 3 bytes).
         * @return data padding.
         */
        uint32_t getPadding() const;
        /**
         * This method gets the format string.
         * @return format string.
         */
        std::string getFormat() const;
        /**
         * This method gets the raw data byte order.
         * @return raw data byte order.
         */
        ByteOrder getByteOrder() const;


        /**
         * This method gets a vector of the raw byte representation of this object's data.
         * <b>Do not change the vector contents.</b>
         * @return reference to vector of raw bytes representing of this object's data.
         */
        std::vector<uint8_t> & getRawBytes();
        /**
         * This method gets a vector of all the data items inside the composite.
         * <b>Do not change the vector contents.</b>
         * @return reference to vector of all the data items inside the composite.
         */
        std::vector<DataItem> & getItems();


        /**
         * This method gets a vector of all the types of the data items inside the composite.
         * @return reference to vector of all the types of the data items inside the composite.
         */
        std::vector<DataType> & getTypes();
        /**
         * This method gets a vector of all the N values of the data items inside the composite.
         * @return reference to vector of all the N values of the data items inside the composite.
         */
        std::vector<int32_t>  & getNValues();
        /**
         * This method gets a vector of all the n values of the data items inside the composite.
         * @return reference to vector of all the n values of the data items inside the composite.
         */
        std::vector<int16_t>  & getnValues();
        /**
         * This method gets a vector of all the m values of the data items inside the composite.
         * @return reference to vector of all the m values of the data items inside the composite.
         */
        std::vector<int8_t>   & getmValues();


        /**
         * This methods returns the index of the data item to be returned
         * on the next call to one of the get&lt;Type&gt;() methods
         * (e.g. {@link #getInt()}.
         * @return returns the index of the data item to be returned
         */
        uint32_t index() const;
        /**
         * This methods sets the index of the data item to be returned
         * on the next call to one of the get&lt;Type&gt;() methods
         * (e.g. {@link #getInt()}.
         * @param index the index of the next data item to be returned
         */
        void index(uint32_t index);


        /**
         * This method gets the next N value data item if it's the correct type.
         * @return data item value.
         * @throws std::underflow_error if at end of data.
         * @throws EvioException if data is not NValue.
         */
        int32_t getNValue();
        /**
         * This method gets the next n value data item if it's the correct type.
         * @return data item value.
         * @throws std::underflow_error if at end of data.
         * @throws EvioException if data is not nValue.
         */
        int16_t getnValue();
        /**
        * This method gets the next m value data item if it's the correct type.
        * @return data item value.
        * @throws std::underflow_error if at end of data.
        * @throws EvioException if data is not mValue.
        */
        int8_t  getmValue();
        /**
        * This method gets the next HOLLERIT data item if it's the correct type.
        * @return data item value.
        * @throws std::underflow_error if at end of data.
        * @throws EvioException if data is not HOLLERIT.
        */
        int32_t getHollerit();


        /**
         * This method gets the next data item as a byte/char if it's the correct type.
         * @return data item value.
         * @throws std::underflow_error if at end of data.
         * @throws EvioException if data is not int8_t.
         */
        int8_t  getChar();
        /**
         * This method gets the next data item as an unsigned byte/char if it's the correct type.
         * @return data item value.
         * @throws std::underflow_error if at end of data.
         * @throws EvioException if data is not uint8_t.
         */
        uint8_t getUChar();


        /**
         * This method gets the next data item as a short if it's the correct type.
         * @return data item value.
         * @throws std::underflow_error if at end of data.
         * @throws EvioException if data is not int16_t.
         */
        int16_t  getShort();
        /**
         * This method gets the next data item as an unsigned short if it's the correct type.
         * @return data item value.
         * @throws std::underflow_error if at end of data.
         * @throws EvioException if data is not uint16_t.
         */
        uint16_t getUShort();
        /**
         * This method gets the next data item as an int if it's the correct type.
         * @return data item value.
         * @throws std::underflow_error if at end of data.
         * @throws EvioException if data is not int32_t.
         */
        int32_t  getInt();
        /**
         * This method gets the next data item as an unsigned int if it's the correct type.
         * @return data item value.
         * @throws std::underflow_error if at end of data.
         * @throws EvioException if data is not uint32_t.
         */
        uint32_t getUInt();


        /**
         * This method gets the next data item as a long if it's the correct type.
         * @return data item value.
         * @throws std::underflow_error if at end of data.
         * @throws EvioException if data is not int64_t.
         */
        int64_t  getLong();
        /**
         * This method gets the next data item as an unsigned long if it's the correct type.
         * @return data item value.
         * @throws std::underflow_error if at end of data.
         * @throws EvioException if data is not uint64_t.
         */
        uint64_t getULong();


        /**
         * This method gets the next data item as a float if it's the correct type.
         * @return data item value.
         * @throws underflow_error if at end of data.
         * @throws EvioException if data is not float.
         */
        float  getFloat();
        /**
         * This method gets the next data item as a double if it's the correct type.
         * @return data item value.
         * @throws std::underflow_error if at end of data.
         * @throws EvioException if data is not double.
         */
        double getDouble();


        /**
         * This method gets the next data item as a vector of strings if it's the correct type.
         * @return reference to vector of strings in data item.
         * @throws std::underflow_error if at end of data.
         * @throws EvioException if data is not vector of strings.
         */
        std::vector<std::string> & getStrings();


        /**
          * This method swaps the data of this composite type between big &
          * little endian. It swaps the entire type including the beginning tagsegment
          * header, the following format string it contains, the data's bank header,
          * and finally the data itself.
          *
          * @throws EvioException if internal error
          */
        void swap();


        /**
         * This method converts (swaps) a buffer of EVIO composite type between big &
         * little endian. It swaps the entire type including the beginning tagsegment
         * header, the following format string it contains, the data's bank header,
         * and finally the data itself. The src array may contain an array of
         * composite type items and all will be swapped.
         *
         * @param src      source data pointer.
         * @param dest     destination data pointer.
         * @param length   length of data array in 32 bit words.
         * @param srcIsLocal true if the byte order of src data is the same as the node's.
         *
         * @throws EvioException if src = null; if len is too small
         */
        static void swapAll(uint8_t *src, uint8_t *dest, size_t length, bool srcIsLocal);


        // Swap in place
        /**
         * This method swaps the data in a buffer, containing EVIO composite type,
         * between big & little endian. It swaps the entire type including the beginning
         * tagsegment header, the following format string it contains, the data's bank header,
         * and finally the data itself. The src buffer may contain an array of
         * composite type items and all will be swapped.<p>
         *
         * @param buf      source and destination data buffer.
         * @param srcPos   position in srcBuffer to beginning swapping
         * @param len      length of data in buf to swap in 32 bit words
         *
         * @throws std::out_of_range if srcPos or len too large; if len too small.
         * @throws EvioException if srcBuffer not in evio format;
         */
        static void swapAll(ByteBuffer & buf, uint32_t srcPos, uint32_t len);
        /**
         * This method swaps the data in a buffer, containing EVIO composite type,
         * between big & little endian. It swaps the entire type including the beginning
         * tagsegment header, the following format string it contains, the data's bank header,
         * and finally the data itself. The src buffer may contain an array of
         * composite type items and all will be swapped.<p>
         *
         * @param buf      source and destination data buffer.
         * @param srcPos   position in srcBuffer to beginning swapping
         * @param len      length of data in buf to swap in 32 bit words
         *
         * @throws std::out_of_range if srcPos or len too large; if len too small.
         * @throws EvioException if srcBuffer not in evio format;
         */
        static void swapAll(std::shared_ptr<ByteBuffer> & buf, uint32_t srcPos, uint32_t len);


        // Swap elsewhere

        /**
         * This method converts (swaps) a buffer, containing EVIO composite type,
         * between big & little endian. It swaps the entire type including the beginning
         * tagsegment header, the following format string it contains, the data's bank header,
         * and finally the data itself. The src buffer may contain an array of
         * composite type items and all will be swapped.<p>
         * <b>This only swaps data if buffer arguments have opposite byte order!</b>
         *
         * @param srcBuf      source data buffer
         * @param destBuf     destination data buffer
         * @param srcPos      position in srcBuffer to beginning swapping
         * @param destPos     position in destBuffer to beginning writing swapped data
         * @param len         length of data in srcBuffer to swap in 32 bit words
         *
         * @throws EvioException if srcBuffer not in evio format;
         *                       if destBuffer too small;
         *                       if bad values for srcPos/destPos/len args;
         */
        static void swapAll(std::shared_ptr<ByteBuffer> & srcBuf,
                            std::shared_ptr<ByteBuffer> & destBuf,
                            uint32_t srcPos, uint32_t destPos, uint32_t len);
        /**
         * This method converts (swaps) a buffer, containing EVIO composite type,
         * between big & little endian. It swaps the entire type including the beginning
         * tagsegment header, the following format string it contains, the data's bank header,
         * and finally the data itself. The src buffer may contain an array of
         * composite type items and all will be swapped.<p>
         * <b>This only swaps data if buffer arguments have opposite byte order!</b>
         *
         * @param srcBuffer   source data buffer
         * @param destBuffer  destination data buffer
         * @param srcPos      position in srcBuffer to beginning swapping
         * @param destPos     position in destBuffer to beginning writing swapped data
         * @param len         length of data in srcBuffer to swap in 32 bit words
         *
         * @throws EvioException if srcBuffer not in evio format;
         *                       if destBuffer too small;
         *                       if bad values for srcPos/destPos/len args;
         */
        static void swapAll(ByteBuffer & srcBuffer, ByteBuffer & destBuffer,
                            uint32_t srcPos, uint32_t destPos, uint32_t len);



        /**
         * This method converts (swaps) EVIO composite type data
         * between Big endian and Little endian. This
         * data does <b>NOT</b> include the composite type's beginning tagsegment and
         * the format string it contains. It also does <b>NOT</b> include the data's
         * bank header words. Caller must be sure the endian value of the srcBuf
         * is set properly before the call.<p>
         * The destBuf can be the same as srcBuf in which case data
         * is swapped in place and the srcBuf byte order is switched in this method.
         * Swap starts at srcBuf's current position. If data is swapped in place,
         * destBuf pos is set to srcBuf pos.
         *
         * @param srcBuf   source data buffer.
         * @param destBuf  destination data buffer.
         * @param nBytes   length of data to swap in bytes
         * @param ifmt     format list as produced by {@link #compositeFormatToInt(const std::string &, std::vector<uint16_t> &)}
         *
         * @throws EvioException if ifmt empty or nBytes &lt; 8;
         *                       srcBuf or destBuf limit/position combo too small;
         *                       if src & dest not identical but overlap.
         */
        static void swapData(ByteBuffer & srcBuf, ByteBuffer & destBuf,
                             size_t nBytes, const std::vector<uint16_t> & ifmt);
        /**
         * This method converts (swaps) EVIO composite type data
         * between Big endian and Little endian. This
         * data does <b>NOT</b> include the composite type's beginning tagsegment and
         * the format string it contains. It also does <b>NOT</b> include the data's
         * bank header words. Caller must be sure the endian value of the srcBuf
         * is set properly before the call.<p>
         * The destBuf can be the same as srcBuf in which case data
         * is swapped in place and the srcBuf byte order is switched in this method.
         *
         * @param srcBuf   source data buffer.
         * @param destBuf  destination data buffer.
         * @param srcPos   position in srcBuf to beginning swapping
         * @param destPos  position in destBuf to beginning writing swapped data unless
         *                 data is swapped in place (then set to srcPos).
         * @param nBytes   length of data to swap in bytes (be sure to account for padding)
         * @param ifmt     format list as produced by {@link #compositeFormatToInt(const std::string &, std::vector<uint16_t> &)}
         *
         * @throws EvioException if ifmt empty or nBytes &lt; 8;
         *                       srcBuf or destBuf limit/position combo too small;
         *                       if src & dest not identical but overlap.
         */
        static void swapData(std::shared_ptr<ByteBuffer> & srcBuf,
                             std::shared_ptr<ByteBuffer> & destBuf,
                             size_t srcPos, size_t destPos, size_t nBytes,
                             const std::vector<uint16_t> & ifmt);
        /**
         * This method converts (swaps) EVIO composite type data
         * between Big endian and Little endian. This
         * data does <b>NOT</b> include the composite type's beginning tagsegment and
         * the format string it contains. It also does <b>NOT</b> include the data's
         * bank header words. Caller must be sure the endian value of the srcBuf
         * is set properly before the call.<p>
         * The destBuf can be the same as srcBuf in which case data
         * is swapped in place and the srcBuf byte order is switched in this method.
         *
         * @param srcBuf   source data buffer.
         * @param destBuf  destination data buffer.
         * @param srcPos   position in srcBuf to beginning swapping
         * @param destPos  position in destBuf to beginning writing swapped data unless
         *                 data is swapped in place (then set to srcPos).
         * @param nBytes   length of data to swap in bytes (be sure to account for padding)
         * @param ifmt     format list as produced by {@link #compositeFormatToInt(const std::string &, std::vector<uint16_t> &)}
         *
         * @throws EvioException if ifmt empty or nBytes &lt; 8;
         *                       srcBuf or destBuf limit/position combo too small;
         *                       if src & dest not identical but overlap.
         */
        static void swapData(ByteBuffer & srcBuf, ByteBuffer & destBuf,
                             size_t srcPos, size_t destPos, size_t nBytes,
                             const std::vector<uint16_t> & ifmt);
        /**
         * This method converts (swaps) EVIO composite type data
         * between Big endian and Little endian. This
         * data does <b>NOT</b> include the composite type's beginning tagsegment and
         * the format string it contains. It also does <b>NOT</b> include the data's
         * bank header words. The dest can be the same as src in which case data is swapped in place.<p>
         *
         * Converts the data of array (src[i], i=0...nwrd-1)
         * using the format code      (ifmt[j], j=0...nfmt-1) .
         *
         * <p>
         * Algorithm description:<p>
         * Data processed inside while (ib &lt; nwrd) loop, where 'ib' is src[] index;
         * loop breaks when 'ib' reaches the number of elements in src[]

         * @param src        data source data pointer.
         * @param dest       destination pointer or can be null if swapping in place.
         * @param nwrd       number of data words (32-bit ints) to be swapped.
         * @param ifmt       format list as produced by {@link #compositeFormatToInt(const std::string &, std::vector<uint16_t> &)}.
         * @param padding    number of bytes to ignore in last data word (starting from data end).
         * @param srcIsLocal true if src is local endian, else false.
         *
         * @throws EvioException if ifmt empty or nwrd &lt; 2;
         *                       src pointer is null;
         *                       if src & dest are not identical but overlap.
         */
        static void swapData(int32_t *src, int32_t *dest, size_t nwrd,
                             const std::vector<uint16_t> & ifmt,
                             uint32_t padding, bool srcIsLocal);

        // This is an in-place swap

        /**
         * This method converts (swaps) an array of EVIO composite type data
         * between IEEE (big endian) and DECS (little endian) in place. This
         * data does <b>NOT</b> include the composite type's beginning tagsegment and
         * the format string it contains. It also does <b>NOT</b> include the data's
         * bank header words.
         *
         * Converts the data of array (iarr[i], i=0...nwrd-1)
         * using the format code      (ifmt[j], j=0...nfmt-1) .
         *
         * <p>
         * Algorithm description:<p>
         * Data processed inside while (ib &lt; nwrd)
         * loop, where 'ib' is iarr[] index;
         * loop breaks when 'ib' reaches the number of elements in iarr[]
         *
         *
         * @param iarr    pointer to data to be swapped.
         * @param nwrd    number of data words (32-bit ints) to be swapped.
         * @param ifmt    unsigned short vector holding translated format.
         * @param padding number of bytes to ignore in last data word (starting from data end).
         *
         * @throws EvioException if ifmt empty or nwrd &lt; 2.
         */
        static void swapData(int32_t *iarr, int nwrd, const std::vector<uint16_t> & ifmt,
                             uint32_t padding);


        /**
         * This method takes a CompositeData object and a transformed format string
         * and uses that to write data into a buffer/array in raw form.
         *
         * @param rawBuf   data buffer in which to put the raw bytes
         * @param data     data to convert to raw bytes
         * @param ifmt     format list as produced by {@link #compositeFormatToInt(const std::string &, std::vector<uint16_t> &)}
         *
         * @throws EvioException if ifmt size <= 0; if srcBuf or destBuf is too
         *                       small; not enough dataItems for the given format
         */
        static void dataToRawBytes(ByteBuffer & rawBuf, Data const & data,
                                   std::vector<uint16_t> & ifmt);


        /**
         * This method extracts and stores all the data items and their types in various lists.
         */
        void process();


        /**
         * Obtain a string representation of the composite data.
         * This string has an indent inserted in front of each group
         * of 5 items. After each group of 5, a newline is inserted.
         * Useful for writing data in xml format.
         *
         * @param  indent a string to insert in front of each group of 5 items
         * @param  hex    if true, display numbers in hexadecimal.
         * @return a string representation of the composite data.
         */
        std::string toString(const std::string & indent, bool hex);
        /**
         * Obtain a string representation of the composite data.
         * @return a string representation of the composite data.
         */
        std::string toString() const;
        /**
         * This method returns a string representation of this CompositeData object
         * suitable for displaying in org.jlab.coda.jevio.graphics.EventTreeFrame
         * gui. Each data item is separated from those before and after by a line.
         * Non-parenthesis repeats are printed together.
         *
         * @param hex if <code>true</code> then print integers in hexadecimal
         * @return  a string representation of this CompositeData object.
         */
        std::string toString(bool hex) const;
    };


}


#endif //EVIO_6_0_COMPOSITEDATA_H
