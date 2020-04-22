
#include <vector>
#include <cstring>
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ios>
#include <iomanip>
#include <exception>
#include <stdexcept>

#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "DataType.h"
#include "BaseStructure.h"

//#include <ctype.h>
//#include <limits.h>

#undef COMPOSITE_DEBUG
#undef COMPOSITE_PRINT


#define MAX(a,b)  ( (a) > (b) ? (a) : (b) )
#define MIN(a,b)  ( (a) < (b) ? (a) : (b) )


#define debugprint(ii) \
  printf("  [%3d] %3d (16 * %2d + %2d), nr=%d\n",ii,ifmt[ii],ifmt[ii]/16,ifmt[ii]-(ifmt[ii]/16)*16,nr)



#ifndef EVIO_6_0_COMPOSITEDATA_H
#define EVIO_6_0_COMPOSITEDATA_H

namespace evio {

/**
* ################################
* COMPOSITE DATA:
* ################################
* This is a new type of data (value = 0xf) which originated with Hall B.
* It is a composite type and allows for possible expansion in the future
* if there is a demand. Basically it allows the user to specify a custom
* format by means of a string - stored in a tagsegment. The data in that
* format follows in a bank. The routine to swap this data must be provided
* by the definer of the composite type - in this case Hall B. The swapping
* function is plugged into this evio library's swapping routine.
*
* Here's what the data looks like.
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
*
* The beginning tagsegment is a normal evio tagsegment containing a string
* (type = 0x3). Currently its type and tag are not used - at least not for
* data formatting.
* The bank is a normal evio bank header with data following.
* The format string is used to read/write this data so that takes care of any
* padding that may exist. As with the tagsegment, the tags and type are ignored.
*
* ################################
*
* This is the class defining the composite data type.
* It is a mixture of header and raw data.
 * This class is <b>NOT</b> thread safe.
*
* @author timmer
* @date 4/17/2020
*/
class CompositeData {

    union DataItemMember {
        // Data being stored
        float     flt;
        double    dbl;
        uint64_t ul64;
        int64_t   l64;
        uint32_t ui32;
        int32_t   i32;
        uint16_t us16;
        int16_t   s16;
        uint8_t   ub8;
        int8_t     b8;

        std::vector<string> strVec;

        DataItemMember() {};

        ~DataItemMember() {};
    };


    typedef struct {
        int left;    /* index of ifmt[] element containing left parenthesis */
        int nrepeat; /* how many times format in parenthesis must be repeated */
        int irepeat; /* right parenthesis counter, or how many times format
                    in parenthesis already repeated */
    } LV;


    /**
     * This class is used to provide all data when constructing a CompositeData object.
     * Doing things this way keeps all internal data members self-consistent.
     */
    class Data  {

    private:

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
        std::vector<DataItemMember> dataItems;

        /** List of types of data objects. */
        std::vector<DataType> dataTypes;

        /** List of "N" (32 bit) values - multiplier values read from data instead
         *  of being part of the format string. */
        std::vector<uint32_t> Nlist;

        /** List of "n" (16 bit) values - multiplier values read from data instead
        *  of being part of the format string. */
        std::vector<uint16_t> nlist;

        /** List of "m" (8 bit) values - multiplier values read from data instead
         *  of being part of the format string. */
        std::vector<uint8_t> mlist;

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
        uint16_t getFormatTag() {return formatTag;}

        /**
         * This method gets the tag in the bank containing the data.
         * @return tag in bank containing the data.
         */
        uint16_t getDataTag() {return dataTag;}

        /**
         * This method gets the num in the bank containing the data.
         * @return num in bank containing the data.
         */
        uint8_t getDataNum() {return dataNum;}

        /**
         * This method gets the raw data size in bytes.
         * @return raw data size in bytes.
         */
        uint32_t getDataSize() {return (dataBytes + paddingBytes);}

        /**
         * This method gets the padding (in bytes).
         * @return padding (in bytes).
         */
        uint32_t getPadding() {return paddingBytes;}

        /**
         * This method adds an "N" or multiplier value to the data.
         * It needs to be added in sequence with other data.
         * @param N  N or multiplier value
         */
        void addN(uint32_t N) {
            Nlist.push_back(N);
            DataItemMember mem;
            mem.ui32 = N;
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
            DataItemMember mem;
            mem.us16 = n;
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
            DataItemMember mem;
            mem.ub8 = m;
            dataItems.push_back(mem);
            dataTypes.push_back(DataType::UCHAR8);
            addBytesToData(1);
        }

        //////////////////////////////////////

        /**
         * Add a signed 32 bit integer to the data.
         * @param i integer to add.
         */
        void addInt(int32_t i) {
            DataItemMember mem;
            mem.i32 = i;
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
                DataItemMember mem;
                mem.i32 = ii;
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
            DataItemMember mem;
            mem.ui32 = i;
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
                DataItemMember mem;
                mem.ui32 = ii;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::UINT32);
            }
            addBytesToData(4 * i.size());
        }

        /////////////////////////////////////

        /**
         * Add a 16 bit short to the data.
         * @param s short to add.
         */
        void addShort(int16_t s) {
            DataItemMember mem;
            mem.s16 = s;
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
                DataItemMember mem;
                mem.s16 = ii;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::SHORT16);
            }
            addBytesToData(2 * s.size());
        }

        /**
         * Add an unsigned 16 bit short to the data.
         * @param s unsigned short to add.
         */
        void addUshort(uint16_t s) {
            DataItemMember mem;
            mem.us16 = s;
            dataItems.push_back(mem);
            dataTypes.push_back(DataType::USHORT16);
            addBytesToData(2);
        }

        /**
         * Add an vector of unsigned 16 bit shorts to the data.
         * @param s vector of unsigned shorts to add.
         */
        void addUshort(std::vector<uint16_t> const & s) {
            for (auto ii : s) {
                DataItemMember mem;
                mem.us16 = ii;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::USHORT16);
            }
            addBytesToData(2 * s.size());
        }

        /////////////////////////////////////

        /**
         * Add a 64 bit long to the data.
         * @param l long to add.
         */
        void addLong(int64_t l) {
            DataItemMember mem;
            mem.l64 = l;
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
                DataItemMember mem;
                mem.l64 = ii;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::LONG64);
            }
            addBytesToData(8 * l.size());
        }

        /**
         * Add an unsigned 64 bit long to the data.
         * @param l unsigned long to add.
         */
        void addUlong(uint64_t l) {
            DataItemMember mem;
            mem.ul64 = l;
            dataItems.push_back(mem);
            dataTypes.push_back(DataType::ULONG64);
            addBytesToData(8);
        }

        /**
         * Add an vector of unsigned 64 bit longs to the data.
         * @param l vector of unsigned longs to add.
         */
        void addUlong(std::vector<uint64_t> const & l) {
            for (auto ii : l) {
                DataItemMember mem;
                mem.ul64 = ii;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::ULONG64);
            }
            addBytesToData(8 * l.size());
        }

        ////////////////////////////////////

        /**
         * Add an 8 bit byte (char) to the data.
         * @param b byte to add.
         */
        void addChar(int8_t b) {
            DataItemMember mem;
            mem.b8 = b;
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
                DataItemMember mem;
                mem.b8 = ii;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::CHAR8);
            }
            addBytesToData(b.size());

        }

        /**
         * Add an unsigned 8 bit byte (uchar) to the data.
         * @param b unsigned byte to add.
         */
        void addUchar(uint8_t b) {
            DataItemMember mem;
            mem.ub8 = b;
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
                DataItemMember mem;
                mem.ub8 = ii;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::UCHAR8);
            }
            addBytesToData(b.size());
        }

        ////////////////////////////////////

        /**
         * Add a 32 bit float to the data.
         * @param f float to add.
         */
        void addFloat(float f) {
            DataItemMember mem;
            mem.flt = f;
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
                DataItemMember mem;
                mem.flt = ff;
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
            DataItemMember mem;
            mem.dbl = d;
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
                DataItemMember mem;
                mem.dbl = dd;
                dataItems.push_back(mem);
                dataTypes.push_back(DataType::DOUBLE64);
            }
            addBytesToData(8 * d.size());
        }

        ///////////////////////////////////////////////

        /**
         * Add a single string to the data.
         * @param s string to add.
         */
        void addString(string const & s) {
            std::vector<string> v {s};
            DataItemMember mem;
            mem.strVec = v;
            dataItems.push_back(mem);
            dataTypes.push_back(DataType::CHARSTAR8);
            addBytesToData(BaseStructure::stringsToRawSize(v));

        }

        /**
         * Add an vector of strings to the data.
         * @param s vector of strings to add.
         */
         void addString(std::vector<string> const & s) {
            DataItemMember mem;
            mem.strVec = s;
            dataItems.push_back(mem);
            dataTypes.push_back(DataType::CHARSTAR8);
            addBytesToData(BaseStructure::stringsToRawSize(s));
        }
    };


private:

    /** String containing data format. */
    string format;

    /** List of ints obtained from transforming format string. */
    std::vector<int32_t> formatInts;

    /** List of extracted data items from raw bytes. */
    std::vector<CompositeData::DataItemMember> items;

    /** List of the types of the extracted data items. */
    std::vector<DataType> types;

    /** List of the "N" (32 bit) values extracted from the raw data. */
    std::vector<int32_t> NList;

    /** List of the "n" (16 bit) values extracted from the raw data. */
    std::vector<int16_t> nList;

    /** List of the "m" (8 bit) values extracted from the raw data. */
    std::vector<uint8_t> mList;

    /** Tagsegment header of tagsegment containing format string. */
    TagSegmentHeader tsHeader;

    /** Bank header of bank containing data. */
    BankHeader bHeader;

    /** The entire raw data of the composite item - both tagsegment and data bank. */
    std::vector<uint8_t> rawBytes;

    /** Buffer containing only the data of the composite item (no headers). */
    ByteBuffer dataBuffer;

    /** Length of only data in bytes (not including padding). */
    uint32_t dataBytes = 0;

    /** Length of only data padding in bytes. */
    uint32_t dataPadding = 0;

    /** Offset (in 32bit words) in rawBytes to place of data. */
    uint32_t dataOffset = 0;

    /** Byte order of raw bytes. */
    ByteOrder byteOrder {ByteOrder::ENDIAN_LITTLE};

    /** Index used in getting data items from the {@link #items} list. */
    uint32_t getIndex = 0;



    CompositeData();


public:

    CompositeData(string & format, CompositeData::Data const & data);

    CompositeData(string & format, uint16_t formatTag,
                         CompositeData::Data data, uint16_t dataTag, uint8_t dataNum);
    CompositeData(uint8_t rawBytes[], ByteOrder byteOrder);

    CompositeData* parse(uint8_t rawBytes[], ByteOrder byteOrder);

    static uint8_t * generateRawBytes(CompositeData data[]);

    //Object clone();

    static string stringsToFormat(std::vector<string> strings);
    //eviofmt  ---> compositeFormatToInt
    static int compositeFormatToInt(const string & formatStr, std::vector<uint16_t> & ifmt);

    void swap();


    uint32_t getPadding();
    string getFormat();
    ByteOrder getByteOrder();

    std::vector<uint8_t> getRawBytes();
    std::vector<CompositeData::DataItemMember> getItems();

    std::vector<DataType> getTypes();
    std::vector<int32_t>  getNValues();
    std::vector<int16_t>  getnValues();
    std::vector<uint8_t>  getmValues();

    int index();
    void index(int index);


    int32_t getNValue();
    int16_t getnValue();
    int8_t  getmValue();
    int32_t getHollerit();

    int8_t  getChar();
    uint8_t getUChar();

    int16_t  getShort();
    uint16_t getUShort();
    int32_t  getInt();
    uint32_t getUInt();

    int64_t  getLong();
    uint64_t getULong();

     float  getFloat() ;
     double getDouble();

     std::vector<string> getStrings();


    static void swapAll (byte[] src, int srcOff, byte[] dest, int destOff,
                                int length, ByteOrder srcOrder);


    static void swapAll (ByteBuffer srcBuffer, ByteBuffer destBuffer,
                         int srcPos, int destPos, int len, bool inPlace);



//////////////


        static void swapData(ByteBuffer & srcBuf, ByteBuffer & destBuf,
                             size_t nBytes, const std::vector<uint16_t> & ifmt);

        // Make it more general ...
        static void swapData(ByteBuffer & srcBuf, ByteBuffer & destBuf,
                             size_t srcPos, size_t destPos, size_t nBytes,
                             const std::vector<uint16_t> & ifmt);

        static void swapData(int32_t *src, int32_t *dest, size_t nwrd,
                             const std::vector<uint16_t> & ifmt,
                             uint32_t padding, bool srcIsLocal);

        // eviofmtswap --->
        // This is an in-place swap
        static void swapData(int32_t *iarr, int nwrd, const std::vector<uint16_t> & ifmt,
                             uint32_t padding);


        static void dataToRawBytes(ByteBuffer rawBuf, CompositeData::Data data,
                                   List<Integer> ifmt);


     void process();

    string toString();
    string toString(string indent);
    string toString(bool hex);


    };


}


#endif //EVIO_6_0_COMPOSITEDATA_H