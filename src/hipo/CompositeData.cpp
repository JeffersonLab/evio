//
// Created by Carl Timmer on 2020-04-17.
//

#include "CompositeData.h"

namespace evio {


    /**
     * Constructor used for creating this object from scratch.
     *
     * @param format format String defining data
     * @param data data in given format
     *
     * @throws EvioException data or format arg = null;
     *                       if improper format string
     */
    CompositeData::CompositeData(string & format, const CompositeData::Data & data) :
            CompositeData(format, data, data.formatTag, data.dataTag, data.dataNum) {}


    /**
     * Constructor used for creating this object from scratch.
     *
     * @param format    format String defining data.
     * @param formatTag tag used in tagsegment containing format.
     * @param data      data in given format.
     * @param dataTag   tag used in bank containing data.
     * @param dataNum   num used in bank containing data.
     * @param order     byte order in which data is stored in internal buffer.
     *
     * @throws EvioException data or format arg = null;
     *                       if improper format string
     */
    CompositeData::CompositeData(string & format, const CompositeData::Data & data, uint16_t formatTag,
                                 uint16_t dataTag, uint8_t dataNum, ByteOrder const & order) {

        bool debug = false;

        this->format = format;
        byteOrder = order;

        if (debug) std::cout << "Analyzing composite data:" << std::endl;

        // Check args
        if (format.empty()) {
            throw EvioException("format arg is empty");
        }

        // Analyze format string
        compositeFormatToInt(format, formatInts);
        if (formatInts.empty()) {
            throw EvioException("format string is empty");
        }

        items = data.dataItems;
        types = data.dataTypes;
        NList = data.Nlist;
        nList = data.nlist;
        mList = data.mlist;

        // CANNOT create EvioTagSegment or EvioBank objects since that will create a
        // circular dependency between this class and BaseStructure!
        // That, in turn, will not allow EvioBank or EvioTagSegment to inherit from BaseStructure.
        // This is where Java really surpasses C++. Find another way write data.

        std::vector<string> strings;
        strings.push_back(format);

        tsHeader = std::make_shared<TagSegmentHeader>(0, format);
        bHeader  = std::make_shared<BankHeader>(dataTag, DataType::COMPOSITE, dataNum);
        bHeader->setPadding(data.getPadding());

        // How many bytes do we skip over at the end?
        dataPadding = data.getPadding();

        // How big is the data in bytes (including padding) ?
        dataBytes = data.getDataSize();

        // Set data length in bank header (includes 2nd bank header word)
        bHeader->setLength(1 + dataBytes/4);

        // Length of everything except data (32 bit words)
        dataOffset =  bHeader->getHeaderLength() +
                     tsHeader->getHeaderLength() +
                     tsHeader->getLength();

        // Length of everything in bytes
        int totalByteLen = dataBytes + 4*dataOffset;

        // Create a big enough array to hold everything
        rawBytes.reserve(totalByteLen);

        // Copy tag segment header (4 bytes) to rawBytes
        tsHeader->write(rawBytes.data(), byteOrder);

        // Change tag seg data (format string) into evio format
        std::vector<uint8_t> bytes;
        size_t tsBytes = 4*(tsHeader->getLength());
        bytes.reserve(tsBytes);
        Util::stringsToRawBytes(strings, bytes);

        // Copy tag segment data to rawBytes vector (char data doesn't need swapping)
        std::memcpy(rawBytes.data() + 4, bytes.data(), tsBytes);

        // Write bank header to buffer
        bHeader->write(rawBytes.data() + 4 + tsBytes, byteOrder);

        // Turn CompositeData::Data into evio bytes and copy (without using EvioBank object)
        ByteBuffer dataBuf(dataBytes);
        dataToRawBytes(dataBuf, data, formatInts);
        std::memcpy(rawBytes.data() + 12 + tsBytes, dataBuf.array(), dataBytes);

        // How big is the data in bytes (without padding) ?
        dataBytes -= dataPadding;
    }


    /**
     * Constructor used when reading existing data.
     * Data is copied in.
     *
     * @param bytes  raw data defining this composite type item
     * @param byteOrder byte order of bytes
     */
    CompositeData::CompositeData(uint8_t *bytes, ByteOrder const & byteOrder) {

        bool debug = false;

        if (bytes == nullptr) {
            throw EvioException("null argument(s)");
        }

        if (debug) std::cout << "CompositeData constr: incoming byte order = " << byteOrder.getName() << std::endl;
        if (debug) std::cout << "Analyzing composite data:" << std::endl;

        this->byteOrder = byteOrder;

        // First read the tag segment header
        tsHeader = EventHeaderParser::createTagSegmentHeader(bytes, byteOrder);

        if (debug) {
            std::cout << "    tagseg: type = 0x" << hex << tsHeader->getDataTypeValue() << dec <<
                         ", tag = " << tsHeader->getTag() << ", len = " << tsHeader->getLength() << std::endl;
        }

        // Hop over tagseg header
        dataOffset = tsHeader->getHeaderLength();

        // Read the format string it contains
        std::vector<string> strs;
        Util::unpackRawBytesToStrings(bytes, 4*(tsHeader->getLength()), strs);

        if (strs.empty()) {
            throw EvioException("bad format string data");
        }
        format = strs[0];

        if (debug) {
            std::cout << "    format: " << format << std::endl;
        }

        // Transform string format into int array format
        std::vector<uint16_t> fmtInts;
        CompositeData::compositeFormatToInt(format, formatInts);
        if (formatInts.empty()) {
            throw EvioException("bad format string data");
        }

        // Hop over tagseg data
        dataOffset += tsHeader->getLength() - (tsHeader->getHeaderLength() - 1);

        // Read the data bank header
        bHeader = EventHeaderParser::createBankHeader(bytes + 4*dataOffset, byteOrder);

        // Hop over bank header
        dataOffset += bHeader->getHeaderLength();

        // How many bytes do we skip over at the end?
        dataPadding = bHeader->getPadding();

        // How much data do we have?
        dataBytes = 4*(bHeader->getLength() - (bHeader->getHeaderLength() - 1)) - dataPadding;
        if (dataBytes < 2) {
            throw EvioException("no composite data");
        }

        int words = (tsHeader->getLength() + bHeader->getLength() + 2);

        if (debug) {
            std::cout << "    bank: type = 0x" << hex << bHeader->getDataTypeValue() << dec <<
                               ", tag = " << bHeader->getTag() << ", num = " << bHeader->getNumber() << std::endl;
            std::cout << "    bank: len (words) = " << bHeader->getLength() <<
                               ", padding = " << dataPadding <<
                               ", data len - padding = " << dataBytes <<
                               ", total printed words = " << words << std::endl;
        }

        // Copy data over
        size_t totalBytes = 4*dataOffset + dataBytes + dataPadding;
        rawBytes.reserve(totalBytes);
        std::memcpy(rawBytes.data(), bytes, totalBytes);

        // Turn raw bytes into list of items and their types
        process();
    }


    /**
     * This method parses an array of raw bytes into an vector of CompositeData objects.
     * Vector is initially cleared.
     *
     * @param bytes     array of raw bytes to parse.
     * @param bytesSize size in bytes of bytes.
     * @param order     byte order of raw bytes.
     * @param list      vector that will hold all parsed CompositeData objects.
     * @return array of CompositeData objects obtained from parsing bytes. If none, return null.
     * @throws EvioException if null args or bad format of raw data
     */
    void CompositeData::parse(uint8_t *bytes, size_t bytesSize, ByteOrder const & order,
                              std::vector<std::shared_ptr<CompositeData>> & list) {

        bool debug = false;

        if (bytes == nullptr) {
            throw EvioException("null argument(s)");
        }

        if (debug) std::cout << "CompositeData parse: incoming byte order = " << order.getName() << std::endl;
        if (debug) std::cout << "Analyzing composite data:" << std::endl;

        // Get list ready
        list.clear();
        list.reserve(100);

        // How many bytes have we read for the current CompositeData object?
        size_t byteCount;
        // Offset into the given bytes array for the current CompositeData object
        size_t rawBytesOffset = 0;
        // How many unused bytes are left in the given bytes array?
        size_t rawBytesLeft = bytesSize;

        if (debug) std::cout << "    CD raw byte count = " << rawBytesLeft << std::endl;

        // Parse while we still have bytes to read ...
        while (rawBytesLeft > 0) {

            // bytes read for the current CD object
            byteCount = 0;

            // Create and fill new CompositeData object
            auto cd = std::make_shared<CompositeData>();
            cd->byteOrder = order;

            // First read the tag segment header
            cd->tsHeader = EventHeaderParser::createTagSegmentHeader(bytes + rawBytesOffset, order);
            byteCount += 4*(cd->tsHeader->getLength() + 1);

            if (debug) {
                std::cout << "    tagseg: type = " << cd->tsHeader->getDataType().toString() <<
                             ", tag = " << cd->tsHeader->getTag() <<
                             ", len = " << cd->tsHeader->getLength() << std::endl;
            }

            // Hop over tagseg header (use cd->dataOffset for other purposes temporarily)
            cd->dataOffset = cd->tsHeader->getHeaderLength();

            // Read the format string it contains
            std::vector<string> strs;
            Util::unpackRawBytesToStrings(bytes + rawBytesOffset + 4*(cd->dataOffset),
                                          4*(cd->tsHeader->getLength()), strs);
            if (strs.empty()) {
                throw EvioException("bad format string data");
            }
            cd->format = strs[0];

            if (debug) {
                std::cout << "    format: " << cd->format << std::endl;
            }

            // Chew on format string & spit out array of ints
            CompositeData::compositeFormatToInt(cd->format, cd->formatInts);
            if (cd->formatInts.empty()) {
                throw EvioException("bad format string data");
            }

            // Hop over tagseg (format string) data
            cd->dataOffset = cd->tsHeader->getLength() + 1;

            // Read the data bank header
            cd->bHeader = EventHeaderParser::createBankHeader(bytes + rawBytesOffset + 4*(cd->dataOffset), order);
            byteCount += 4*(cd->bHeader->getLength() + 1);

            // Hop over bank header
            cd->dataOffset += cd->bHeader->getHeaderLength();

            // How many bytes do we skip over at the end?
            cd->dataPadding = cd->bHeader->getPadding();

            // How much real data do we have (without padding)?
            cd->dataBytes = 4*(cd->bHeader->getLength() - (cd->bHeader->getHeaderLength() - 1)) - cd->dataPadding;
            if (cd->dataBytes < 2) {
                throw EvioException("no composite data");
            }

            if (debug) {
                std::cout << "    bank: type = " << cd->bHeader->getDataType().toString() <<
                             ", tag = " << cd->bHeader->getTag() <<
                             ", num = " << cd->bHeader->getNumber() <<
                             ", pad = " << cd->dataPadding << std::endl;
                std::cout << "    bank: len (words) = " << cd->bHeader->getLength() <<
                             ", data len - padding (bytes) = " << cd->dataBytes << std::endl;
            }

            // Make copy of only the rawbytes for this CompositeData object (including padding)
            cd->rawBytes.reserve(byteCount);
            std::memcpy(cd->rawBytes.data(), bytes + rawBytesOffset, byteCount);

            // Turn dataBuffer into list of items and their types
            cd->process();

            // Add to this CompositeData object to list
            list.push_back(cd);

            // Track how many raw bytes we have left to parse
            rawBytesLeft -= byteCount;

            // Offset into "bytes" of next CompositeData object
            rawBytesOffset += byteCount;
            if (debug) {
                std::cout << "    CD raw byte count = " << rawBytesLeft <<
                             ", raw byte offset = " << rawBytesOffset << std::endl;
            }
        }
    }

    /**
     * This method generates raw bytes of evio format from a vector of CompositeData objects.
     * The returned vector consists of gluing together all the individual objects' rawByte arrays.
     * All CompositeData element must be of the same byte order.
     *
     * @param data     vector of CompositeData objects to turn into bytes.
     * @param rawBytes vector of raw, evio format bytes.
     * @throws EvioException if data takes up too much memory to store in raw byte array (JVM limit);
     *                       array elements have different byte order.
     */
    void CompositeData::generateRawBytes(std::vector<std::shared_ptr<CompositeData>> & data,
                                         std::vector<uint8_t> & rawBytes) {

        if (data.empty()) {
            return;
        }

        ByteOrder order = data[0]->byteOrder;

        // Get a total length (# bytes)
        size_t totalLen = 0, len;
        for (auto const & cd : data) {
            if (cd->byteOrder != order) {
                throw EvioException("all array elements must have same byte order");
            }
            len = cd->getRawBytes().size();
            totalLen += len;
        }

        // Prepare vector
        rawBytes.clear();
        rawBytes.reserve(totalLen);

        // Copy everything in
        int offset = 0;
        for (auto const & cd : data) {
            len = cd->getRawBytes().size();
            std::memcpy(rawBytes.data() + offset, cd->rawBytes.data(), len);
            offset += len;
        }
    }

//
///**
// * Method to clone a CompositeData object.
// * Deep cloned so no connection exists between
// * clone and object cloned.
// *
// * @return cloned CompositeData object.
// */
//Object CompositeData::clone() {
//
//    CompositeData cd = new CompositeData();
//
//    cd.getIndex   = getIndex;
//    cd.byteOrder  = byteOrder;
//    cd.dataBytes  = dataBytes;
//    cd.dataOffset = dataOffset;
//    cd.rawBytes   = rawBytes.clone();
//
//    // copy tagSegment header
//    cd.tsHeader = new TagSegmentHeader(tsHeader.getTag(),
//                                       tsHeader.getDataType());
//
//    // copy bank header
//    cd.bHeader = new BankHeader(bHeader.getTag(),
//                                bHeader.getDataType(),
//                                bHeader.getNumber());
//
//    // copy ByteBuffer
//    cd.dataBuffer = ByteBuffer.wrap(rawBytes, 4*dataOffset, 4*bHeader.getLength()).slice();
//    cd.dataBuffer.order(byteOrder);
//
//    // copy format ints
//    cd.formatInts = new ArrayList<Integer>(formatInts.size());
//    cd.formatInts.addAll(formatInts);
//
//    // copy type items
//    cd.types = new ArrayList<DataType>(types.size());
//    cd.types.addAll(types);
//
//    //-----------------
//    // copy data items
//    //-----------------
//
//    // create new list of correct length
//    int listLength = items.size();
//    cd.items = new ArrayList<Object>(listLength);
//
//    // copy each item, cloning those that are mutable
//    for (int i=0; i < listLength; i++) {
//        switch (types.get(i)) {
//            // this is the only mutable type and so
//            // it needs to be cloned specifically
//            case CHARSTAR8:
//                String[] strs = (String[]) items.get(i);
//                cd.items.add(strs.clone());
//                break;
//            default:
//                cd.items.add(items.get(i));
//        }
//    }
//
//    return cd;
//}
//



/**
 * This method helps the CompositeData object creator by
 * finding the proper format string parameter for putting
 * this array of Strings into its data.
 * The format is in the form "Ma" where M is an actual integer.
 * Warning, in this case, M may not be greater than 15.
 * If you want a longer string or array of strings, use the
 * format "Na" with a literal N. The N value can be added
 * through {@link org.jlab.coda.jevio.CompositeData.Data#addN(int)}
 *
 * @param strings array of strings to eventually put into a
 *                CompositeData object.
 * @return string representing its format to be used in the
 *                CompositeData object's format string
 * @return null if arg is null or has 0 length
 */
string CompositeData::stringsToFormat(std::vector<string> strings) {

    std::vector<uint8_t> bytes;
    Util::stringsToRawBytes(strings, bytes);
    if (!bytes.empty()) {
        return std::to_string(bytes.size()) + "a";
    }

    return "";
}

/**
 * Get the data padding (0, 1, 2, or 3 bytes).
 * @return data padding.
 */
uint32_t CompositeData::getPadding() {return dataPadding;}

/**
 * This method gets the format string.
 * @return format string.
 */
string CompositeData::getFormat() {return format;}

/**
 * This method gets the raw data byte order.
 * @return raw data byte order.
 */
ByteOrder CompositeData::getByteOrder() {return byteOrder;}

/**
 * This method gets the raw byte representation of this object's data.
 * @return raw byte representation of this object's data.
 */
std::vector<uint8_t> CompositeData::getRawBytes() {return rawBytes;}

/**
 * This method gets a list of all the data items inside the composite.
 * @return list of all the data items inside the composite.
 */
std::vector<CompositeData::DataItem> CompositeData::getItems() {return items;}

/**
 * This method gets a list of all the types of the data items inside the composite.
 * @return list of all the types of the data items inside the composite.
 */
std::vector<DataType> CompositeData::getTypes() {return types;}

/**
 * This method gets a list of all the N values of the data items inside the composite.
 * @return list of all the N values of the data items inside the composite.
 */
std::vector<int32_t> CompositeData::getNValues() {return NList;}

/**
 * This method gets a list of all the n values of the data items inside the composite.
 * @return list of all the n values of the data items inside the composite.
 */
std::vector<int16_t> CompositeData::getnValues() {return nList;}

/**
 * This method gets a list of all the m values of the data items inside the composite.
 * @return list of all the m values of the data items inside the composite.
 */
std::vector<int8_t> CompositeData::getmValues() {return mList;}

/**
 * This methods returns the index of the data item to be returned
 * on the next call to one of the get&lt;Type&gt;() methods
 * (e.g. {@link #getInt()}.
 * @return returns the index of the data item to be returned
 */
int CompositeData::index() {return getIndex;}

/**
 * This methods sets the index of the data item to be returned
 * on the next call to one of the get&lt;Type&gt;() methods
 * (e.g. {@link #getInt()}.
 * @param index the index of the next data item to be returned
 */
void CompositeData::index(int index) {this->getIndex = index;}



/**
 * This method gets the next N value data item if it's the correct type.
 * @return data item value.
 * @throws underflow_error if at end of data.
 * @throws EvioException if data is not NValue.
 */
int32_t CompositeData::getNValue() {
    if (getIndex > types.size()) {throw underflow_error("end of data");}
    if (types[getIndex] != DataType::NVALUE) {throw EvioException("wrong data type");}
    return (items[getIndex++].item.i32);
}

/**
 * This method gets the next n value data item if it's the correct type.
 * @return data item value.
 * @throws underflow_error if at end of data.
 * @throws EvioException if data is not nValue.
 */
int16_t CompositeData::getnValue() {
    if (getIndex > types.size()) {throw underflow_error("end of data");}
    if (types[getIndex] != DataType::nVALUE) {throw EvioException("wrong data type");}
    return (items[getIndex++].item.s16);
}

/**
* This method gets the next m value data item if it's the correct type.
* @return data item value.
* @throws underflow_error if at end of data.
* @throws EvioException if data is not mValue.
*/
int8_t CompositeData::getmValue() {
    if (getIndex > types.size()) {throw underflow_error("end of data");}
    if (types[getIndex] != DataType::mVALUE) {throw EvioException("wrong data type");}
    return (items[getIndex++].item.b8);
}

/**
* This method gets the next HOLLERIT data item if it's the correct type.
* @return data item value.
* @throws underflow_error if at end of data.
* @throws EvioException if data is not HOLLERIT.
*/
int32_t CompositeData::getHollerit() {
    if (getIndex > types.size()) {throw underflow_error("end of data");}
    if (types[getIndex] != DataType::HOLLERIT) {throw EvioException("wrong data type");}
    return (items[getIndex++].item.i32);
}


/**
 * This method gets the next data item as a byte/char if it's the correct type.
 * @return data item value.
 * @throws underflow_error if at end of data.
 * @throws EvioException if data is not int8_t.
 */
int8_t CompositeData::getChar() {
    if (getIndex > types.size()) {throw underflow_error("end of data");}
    if (types[getIndex] != DataType::CHAR8) {throw EvioException("wrong data type");}
    return (items[getIndex++].item.b8);
}

/**
 * This method gets the next data item as an unsigned byte/char if it's the correct type.
 * @return data item value.
 * @throws underflow_error if at end of data.
 * @throws EvioException if data is not uint8_t.
 */
uint8_t CompositeData::getUChar() {
    if (getIndex > types.size()) {throw underflow_error("end of data");}
    if (types[getIndex] != DataType::UCHAR8) {throw EvioException("wrong data type");}
    return (items[getIndex++].item.ub8);
}

/**
 * This method gets the next data item as a short if it's the correct type.
 * @return data item value.
 * @throws underflow_error if at end of data.
 * @throws EvioException if data is not int16_t.
 */
int16_t CompositeData::getShort() {
    if (getIndex > types.size()) {throw underflow_error("end of data");}
    if (types[getIndex] != DataType::SHORT16) {throw EvioException("wrong data type");}
    return (items[getIndex++].item.s16);
}

/**
 * This method gets the next data item as an unsigned short if it's the correct type.
 * @return data item value.
 * @throws underflow_error if at end of data.
 * @throws EvioException if data is not uint16_t.
 */
uint16_t CompositeData::getUShort() {
    if (getIndex > types.size()) {throw underflow_error("end of data");}
    if (types[getIndex] != DataType::USHORT16) {throw EvioException("wrong data type");}
    return (items[getIndex++].item.us16);
}

/**
 * This method gets the next data item as an int if it's the correct type.
 * @return data item value.
 * @throws underflow_error if at end of data.
 * @throws EvioException if data is not int32_t.
 */
int32_t CompositeData::getInt() {
    if (getIndex > types.size()) {throw underflow_error("end of data");}
    if (types[getIndex] != DataType::INT32) {throw EvioException("wrong data type");}
    return (items[getIndex++].item.i32);
}

/**
 * This method gets the next data item as an unsigned int if it's the correct type.
 * @return data item value.
 * @throws underflow_error if at end of data.
 * @throws EvioException if data is not uint32_t.
 */
uint32_t CompositeData::getUInt() {
    if (getIndex > types.size()) {throw underflow_error("end of data");}
    if (types[getIndex] != DataType::UINT32) {throw EvioException("wrong data type");}
    return (items[getIndex++].item.ui32);
}

/**
 * This method gets the next data item as a long if it's the correct type.
 * @return data item value.
 * @throws underflow_error if at end of data.
 * @throws EvioException if data is not int64_t.
 */
int64_t CompositeData::getLong() {
    if (getIndex > types.size()) {throw underflow_error("end of data");}
    if (types[getIndex] != DataType::LONG64) {throw EvioException("wrong data type");}
    return (items[getIndex++].item.l64);
}

/**
 * This method gets the next data item as an unsigned long if it's the correct type.
 * @return data item value.
 * @throws underflow_error if at end of data.
 * @throws EvioException if data is not uint64_t.
 */
uint64_t CompositeData::getULong() {
    if (getIndex > types.size()) {throw underflow_error("end of data");}
    if (types[getIndex] != DataType::ULONG64) {throw EvioException("wrong data type");}
    return (items[getIndex++].item.ul64);
}

/**
 * This method gets the next data item as a float if it's the correct type.
 * @return data item value.
 * @throws underflow_error if at end of data.
 * @throws EvioException if data is not float.
 */
float CompositeData::getFloat() {
    if (getIndex > types.size()) {throw underflow_error("end of data");}
    if (types[getIndex] != DataType::FLOAT32) {throw EvioException("wrong data type");}
    return (items[getIndex++].item.flt);
}

/**
 * This method gets the next data item as a double if it's the correct type.
 * @return data item value.
 * @throws underflow_error if at end of data.
 * @throws EvioException if data is not double.
 */
double CompositeData::getDouble() {
    if (getIndex > types.size()) {throw underflow_error("end of data");}
    if (types[getIndex] != DataType::DOUBLE64) {throw EvioException("wrong data type");}
    return (items[getIndex++].item.dbl);
}

/**
 * This method gets the next data item as a vector of strings if it's the correct type.
 * @return data item value.
 * @throws underflow_error if at end of data.
 * @throws EvioException if data is not vector of strings.
 */
std::vector<string> CompositeData::getStrings() {
    if (getIndex > types.size()) {throw underflow_error("end of data");}
    if (types[getIndex] != DataType::CHARSTAR8) {throw EvioException("wrong data type");}
    items[getIndex].item.str = true;
    return (items[getIndex++].strVec);
 }


/**
 * <pre>
 *  This routine transforms a composite, format-containing
 *  ASCII string to a vector of integer codes. It is to be used
 *  in conjunction with {@link #eviofmtswap} to swap the endianness of
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
int CompositeData::compositeFormatToInt(const string & formatStr, std::vector<uint16_t> & ifmt) {

    char ch;
    int  l, n, kf, lev, nr, nn, nb;
    size_t fmt_len;

    ifmt.clear();
    ifmt.reserve(40);

    n   = 0; /* ifmt[] index */
    nr  = 0;
    nn  = 1;
    lev = 0;
    nb  = 0; /* the number of bytes in length taken from data */

#ifdef COMPOSITE_DEBUG
    std::cout << std::endl << "=== eviofmt start, fmt >" << formatStr << "< ===" << std::endl;
#endif

    /* loop over format string */
    fmt_len = formatStr.size();
    if (fmt_len > INT_MAX) return (-1);
    for (l=0; l < (int)fmt_len; l++)
    {
        ch = formatStr[l];
        if (ch == ' ') continue;
#ifdef COMPOSITE_DEBUG
        std::cout << ch << std::endl;
#endif
        /* if digit, we have hard coded 'number', following before komma will be repeated 'number' times;
        forming 'nr' */
        if (isdigit(ch))
        {
            if (nr < 0) return(-1);
            nr = 10*MAX(0,nr) + atoi((char *)&ch);
            if (nr > 15) return(-2);
#ifdef COMPOSITE_DEBUG
            std::cout << "the number of repeats nr=" << nr << std::endl;
#endif
        }

            /* a left parenthesis -> 16*nr + 0 */
        else if (ch == '(')
        {
            if (nr < 0) return(-3);
            lev++;
#ifdef COMPOSITE_DEBUG
            std::cout << "111: nn=" << nn << " nr=" << nr << std::endl;
#endif

            if (nn == 0) /*special case: if #repeats is in data, set bits [15:14] */
            {
                if(nb==4)      {ifmt.push_back(1<<14); ++n;}
                else if(nb==2) {ifmt.push_back(2<<14); ++n;}
                else if(nb==1) {ifmt.push_back(3<<14); ++n;}
                else {printf("eviofmt ERROR: unknown nb=%d\n",nb);exit(0);}
            }
            else /* #repeats hardcoded */
            {
                ifmt.push_back((MAX(nn,nr)&0x3F) << 8);
                ++n;
            }

            nn = 1;
            nr = 0;
#ifdef COMPOSITE_DEBUG
            debugprint(n-1);
#endif
        }
            /* a right parenthesis -> (0<<8) + 0 */
        else if (ch == ')')
        {
            if (nr >= 0) return(-4);
            lev--;
            ifmt.push_back(0);
            ++n;
            nr = -1;
#ifdef COMPOSITE_DEBUG
            debugprint(n-1);
#endif
        }
            /* a komma, reset nr */
        else if (ch == ',')
        {
            if (nr >= 0) return(-5);
            nr = 0;
#ifdef COMPOSITE_DEBUG
            std::cout << "komma, nr=" << nr << std::endl;
#endif
        }
            /* variable length format (int32) */
        else if (ch == 'N')
        {
            nn = 0;
            nb = 4;
#ifdef COMPOSITE_DEBUG
            std::cout << "N, nb=" << nb << std::endl;
#endif
        }
            /* variable length format (int16) */
        else if (ch == 'n')
        {
            nn = 0;
            nb = 2;
#ifdef COMPOSITE_DEBUG
            std::cout << "n, nb=" << nb << std::endl;
#endif
        }
            /* variable length format (int8) */
        else if (ch == 'm')
        {
            nn = 0;
            nb = 1;
#ifdef COMPOSITE_DEBUG
            std::cout << "m, nb=" << nb << std::endl;
#endif
        }
            /* actual format */
        else
        {
            if(     ch == 'i') kf = 1;  /* 32 */
            else if(ch == 'F') kf = 2;  /* 32 */
            else if(ch == 'a') kf = 3;  /*  8 */
            else if(ch == 'S') kf = 4;  /* 16 */
            else if(ch == 's') kf = 5;  /* 16 */
            else if(ch == 'C') kf = 6;  /*  8 */
            else if(ch == 'c') kf = 7;  /*  8 */
            else if(ch == 'D') kf = 8;  /* 64 */
            else if(ch == 'L') kf = 9;  /* 64 */
            else if(ch == 'l') kf = 10; /* 64 */
            else if(ch == 'I') kf = 11; /* 32 */
            else if(ch == 'A') kf = 12; /* 32 */
            else               kf = 0;

            if (kf != 0)
            {
                if (nr < 0) return(-6);
#ifdef COMPOSITE_DEBUG
                std::cout << "222: nn=" << nn << " nr=" << nr << std::endl;
#endif

                int ifmtVal = ((MAX(nn,nr) & 0x3F) << 8) + kf;

                if (nb > 0) {
                    if (nb==4)      ifmtVal |= (1 << 14);
                    else if (nb==2) ifmtVal |= (2 << 14);
                    else if (nb==1) ifmtVal |= (3 << 14);
                    else {throw EvioException("unknown nb=" + std::to_string(nb));}
                }

                ifmt.push_back(ifmtVal);
                ++n;
                nn=1;
#ifdef COMPOSITE_DEBUG
                debugprint(n-1);
#endif
            }
            else
            {
                /* illegal character */
                return(-7);
            }
            nr = -1;
        }
    }

    if (lev != 0) return(-8);

#ifdef COMPOSITE_DEBUG
    std::cout << "=== eviofmt end ===" << std::endl;
#endif

    return(n);
}


    /**
      * This method swaps the data of this composite type between big &
      * little endian. It swaps the entire type including the beginning tagsegment
      * header, the following format string it contains, the data's bank header,
      * and finally the data itself.
      *
      * @throws EvioException if internal error
      */
    void CompositeData::swap() {
        swapAll(rawBytes.data(), nullptr, rawBytes.size()/4, byteOrder.isLocalEndian());
        byteOrder = byteOrder.getOppositeEndian();
    }


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
     * @throws EvioException if offsets or length &lt; 0; if src = null;
     *                       if src or dest is too small
     */
    void CompositeData::swapAll(uint8_t *src, uint8_t *dest, size_t length, bool srcIsLocal) {

        if (src == nullptr) {
            throw EvioException("src pointer null");
        }

        bool inPlace = false;
        if (dest == nullptr || dest == src) {
            dest = src;
            inPlace = true;
        }

        if (length < 4) {
            throw EvioException("length must be >= 4");
        }

        size_t srcOff=0, destOff=0;

        // Byte order
        ByteOrder srcOrder = ByteOrder::ENDIAN_LOCAL;
        if (!srcIsLocal) {
            srcOrder = srcOrder.getOppositeEndian();
        }
        ByteOrder destOrder = srcOrder.getOppositeEndian();

        // How many unused bytes are left in the src array?
        size_t totalBytes   = 4*length;
        size_t srcBytesLeft = totalBytes;

        // How many bytes taken for this CompositeData object?
        size_t dataOff = 0;


        while (srcBytesLeft > 0) {
//System.out.println("start src offset = " + (srcOff + dataOff));

            // First read the tag segment header
            auto tsegHeader = EventHeaderParser::createTagSegmentHeader(src + srcOff, srcOrder);
            uint32_t headerLen  = tsegHeader->getHeaderLength();
            uint32_t dataLength = tsegHeader->getLength() - (headerLen - 1);

//System.out.println("tag len = " + tsegHeader.getLength() + ", dataLen = " + dataLength);

            // Oops, no format data
            if (dataLength < 1) {
                throw EvioException("no format data");
            }

            // Got all we needed from the tagseg header, now swap as it's written out.
            tsegHeader->write(dest + destOff + 4, destOrder);

            // Move to beginning of string data
            srcOff  += 4*headerLen;
            destOff += 4*headerLen;
            dataOff += 4*headerLen;

            // Read the format string it contains
            std::vector<string> strs;
            Util::unpackRawBytesToStrings(src + srcOff, 4*(tsegHeader->getLength()), strs);

            if (strs.empty()) {
                throw EvioException("bad format string data");
            }
            string fmt = strs[0];

            std::vector<uint16_t> fmtInts;
            CompositeData::compositeFormatToInt(fmt, fmtInts);
            if (fmtInts.empty()) {
                throw EvioException("bad format string data");
            }

            // Char data does not get swapped but needs
            // to be copied if not swapping in place.
            if (!inPlace) {
                std::memcpy(dest + destOff, src + srcOff, 4*dataLength);
            }

            // Move to beginning of bank header
            srcOff  += 4*dataLength;
            destOff += 4*dataLength;
            dataOff += 4*dataLength;

            // Read the data bank header
            auto bnkHeader = EventHeaderParser::createBankHeader(src + srcOff, srcOrder);
            headerLen  = bnkHeader->getHeaderLength();
            dataLength = bnkHeader->getLength() - (headerLen - 1);

//System.out.println("swapAll: bank len = " + bnkHeader.getLength() + ", dataLen = " + dataLength +
//", tag = " + bnkHeader.getTag() + ", num = " + bnkHeader.getNumber() + ", type = " + bnkHeader.getDataTypeName() +
//", pad = " + bnkHeader.getPadding());

            // Oops, no data
            if (dataLength < 1) {
                throw EvioException("no data");
            }

            uint32_t padding = bnkHeader->getPadding();

            // Got all we needed from the bank header, now swap as it's written out.
            bnkHeader->write(dest + destOff + 8, destOrder);

            // Move to beginning of data
            srcOff  += 4*headerLen;
            destOff += 4*headerLen;
            dataOff += 4*headerLen;

            // Swap data
            CompositeData::swapData(reinterpret_cast<int32_t *>(src), reinterpret_cast<int32_t *>(dest),
                                    dataLength, fmtInts, padding, srcIsLocal);

            // Adjust data length by switching units from
            // ints to bytes and accounting for padding.
            dataLength = 4*dataLength;

            // Set buffer positions and offset
            srcOff  += dataLength;
            destOff += dataLength;
            dataOff += dataLength;

            srcBytesLeft = totalBytes - dataOff;

//System.out.println("bytes left = " + srcBytesLeft + ",offset = " + dataOff + ", padding = " + padding);
//System.out.println("src pos = " + srcBuffer.position() + ", dest pos = " + destBuffer.position());
        }

        // Oops, things aren't coming out evenly
        if (srcBytesLeft != 0) {
            throw EvioException("bad format");
        }
    }



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
     * @param inPlace     if true, swap in place.
     *
     * @throws EvioException if srcBuffer not in evio format;
     *                       if destBuffer too small;
     *                       if bad values for srcPos/destPos/len args;
     */
    void CompositeData::swapAll(ByteBuffer & srcBuffer, ByteBuffer & destBuffer,
                                uint32_t srcPos, uint32_t destPos, uint32_t len, bool inPlace) {

        // Minimum size of 4 words for composite data
        if (len < 4) {
            throw EvioException("len arg must be >= 4");
        }

        if (4*len > destBuffer.limit() - destPos) {
            throw EvioException("not enough room in destination buffer");
        }

        // Bytes to swap
        uint32_t totalBytes   = 4*len;
        uint32_t srcBytesLeft = totalBytes;
        uint32_t dataOff, byteLen;
        // Initialize
        dataOff = 0;

        while (srcBytesLeft > 0) {

            // Read & swap string tagsegment header
            EvioNode node;
            Util::swapTagSegmentHeader(node, srcBuffer, destBuffer, srcPos, destPos);

            // Move to beginning of string data
            srcPos  += 4;
            destPos += 4;
            dataOff += 4;

            // String data length in bytes
            byteLen = 4*node.getDataLength();

            // Read the format string it contains
            std::vector<string> strs;
            Util::unpackRawBytesToStrings(srcBuffer, srcPos, byteLen, strs);

            if (strs.empty()) {
                throw EvioException("bad format string data");
            }
            string fmt = strs[0];

            // Transform string format into int array format
            std::vector<uint16_t> fmtInts;
            CompositeData::compositeFormatToInt(fmt, fmtInts);
            if (fmtInts.empty()) {
                throw EvioException("bad format string data");
            }

            // Char data does not get swapped but needs
            // to be copied if not swapping in place.
            if (!inPlace) {
                for (int i=0; i < byteLen; i++) {
                    destBuffer.put(destPos+i, srcBuffer.getByte(srcPos+i));
                }
            }

            // Move to beginning of bank header
            srcPos  += byteLen;
            destPos += byteLen;
            dataOff += byteLen;

            // Read & swap data bank header
            Util::swapBankHeader(node, srcBuffer, destBuffer, srcPos, destPos);

            // Oops, no data
            if (node.getDataLength() < 1) {
                throw EvioException("no data");
            }

            // Move to beginning of bank data
            srcPos  += 8;
            destPos += 8;
            dataOff += 8;

            // Bank data length in bytes
            byteLen = 4*node.getDataLength();

            // Swap data (accounting for padding)
            CompositeData::swapData(srcBuffer, destBuffer, srcPos, destPos, (byteLen - node.getPad()), fmtInts);

            // Move past bank data
            srcPos    += byteLen;
            destPos   += byteLen;
            dataOff   += byteLen;
            srcBytesLeft  = totalBytes - dataOff;

            //std::cout << "bytes left = " << srcBytesLeft << std::endl;
            //std::cout << "src pos = " << srcBuffer.position() << ", dest pos = " << destBuffer.position() << std::endl;
        }

        // Oops, things aren't coming out evenly
        if (srcBytesLeft != 0) {
            throw EvioException("bad format");
        }
    }


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
     * @param ifmt     format list as produced by {@link #compositeFormatToInt(String)}
     *
     * @throws EvioException if ifmt empty or nBytes &lt; 8;
     *                       srcBuf or destBuf limit/position combo too small;
     *                       if src & dest not identical but overlap.
     */
    void CompositeData::swapData(ByteBuffer & srcBuf, ByteBuffer & destBuf,
                                 size_t nBytes, const std::vector<uint16_t> & ifmt) {

        swapData(srcBuf, destBuf, srcBuf.position(), destBuf.position(), nBytes, ifmt);
    }



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
     * @param ifmt     format list as produced by {@link #compositeFormatToInt(String)}
     *
     * @throws EvioException if ifmt empty or nBytes &lt; 8;
     *                       srcBuf or destBuf limit/position combo too small;
     *                       if src & dest not identical but overlap.
     */
    void CompositeData::swapData(ByteBuffer & srcBuf, ByteBuffer & destBuf,
                                 size_t srcPos, size_t destPos, size_t nBytes,
                                 const std::vector<uint16_t> & ifmt) {

        // check args
        if (ifmt.empty()) {
            throw EvioException("ifmt arg empty");
        }

        if (nBytes < 8) {
            throw EvioException("nBytes < 8, too small");
        }

        // size of int list
        size_t nfmt = ifmt.size();
        if (nfmt <= 0) throw EvioException("empty format list");

        bool inPlace = false;
        ByteOrder destOrder = destBuf.order();

        if (srcBuf == destBuf) {
            // The trick is to swap in place and that can only be done if
            // we have 2 separate buffers with different endianness.
            // So duplicate the buffer which shares content but allows
            // different endianness.
            srcBuf.duplicate(destBuf);
            // Byte order of swapped data
            destOrder = (srcBuf.order() == ByteOrder::ENDIAN_BIG) ?
                        ByteOrder::ENDIAN_LITTLE : ByteOrder::ENDIAN_BIG;

            destBuf.order(destOrder);
            destPos = srcPos;
            inPlace = true;
        }
        else {
            // Check to see if memory overlaps
            uint8_t *srcPtr  = srcBuf.array();
            uint8_t *destPtr = destBuf.array();

            if (((destPtr + nBytes) > srcPtr) && (destPtr < (srcPtr + nBytes))) {
                throw EvioException("src and dest memory not identical but overlaps");
            }
        }

        // Check to see if buffers are too small to handle this operation
        if ((srcBuf.limit() < nBytes + srcPos) || (destBuf.limit() < nBytes + destPos)) {
            throw EvioException("buffer(s) too small to handle swap, decrease pos or increase limit");
        }

        int kcnf, mcnf;

        int imt   = 0;  // ifmt[] index
        int lev   = 0;  // parenthesis level
        int ncnf  = 0;  // how many times must repeat a format
        int iterm = 0;

        LV lv[10];

        // just past end of src data
        size_t srcEndIndex = srcPos + nBytes;

        // Sergey's original comments:
        //
        //   Converts the data of array (iarr[i], i=0...nwrd-1)
        //   using the format code      (ifmt[j], j=0...nfmt-1) .
        //
        //   Algorithm description:
        //      Data processed inside while(ib < nwrd) loop, where 'ib' is iarr[] index;
        //      loop breaks when 'ib' reaches the number of elements in iarr[]
        //
        //   Timmer's note: nwrd = # of 32 bit words in data,
        //                  but that's now been replaced by the
        //                  num of bytes since data may not be an
        //                  even multiple of 4 bytes.

        while (srcPos < srcEndIndex) {

#ifdef COMPOSITE_DEBUG
            std::cout << "+++ begin = " << srcPos << ", end = " << srcEndIndex << std::endl;
#endif
            // get next format code
            while (true) {
                imt++;
                // end of format statement reached, back to iterm - last parenthesis or format begining
                if (imt > nfmt) {
                    //imt = iterm;
                    imt = 0;
#ifdef COMPOSITE_DEBUG
                    std::cout << "1" << std::endl;
#endif
                }
                // meet right parenthesis, so we're finished processing format(s) in parenthesis
                else if (ifmt[imt-1] == 0) {

                    // right parenthesis without preceding left parenthesis? -> illegal format
                    if (lev == 0) {
                        throw EvioException("illegal format");
                    }

                    // increment counter
                    lv[lev-1].irepeat++;

                    // if format in parenthesis was processed the required number of times
                    if (lv[lev-1].irepeat >= lv[lev-1].nrepeat) {
                        // store left parenthesis index minus 1
                        // (if will meet end of format, will start from format index imt=iterm;
                        // by default we continue from the beginning of the format (iterm=0)) */
                        //iterm = lv[lev-1].left - 1;
                        // done with this level - decrease parenthesis level
                        lev--;

#ifdef COMPOSITE_DEBUG
                        std::cout << "2" << std::endl;
#endif
                    }
                    // go for another round of processing by setting 'imt' to the left parenthesis
                    else {
                        // points ifmt[] index to the left parenthesis from the same level 'lev'
                        imt = lv[lev-1].left;
#ifdef COMPOSITE_DEBUG
                        std::cout << "3" << std::endl;
#endif
                    }
                }
                else {
                    ncnf = (ifmt[imt-1] >> 8) & 0x3F;  /* how many times to repeat format code */
                    kcnf =  ifmt[imt-1] & 0xFF;        /* format code */
                    mcnf = (ifmt[imt-1] >> 14) & 0x3;  /* repeat code */

                    // left parenthesis - set new lv[]
                    if (kcnf == 0) {

                        // check repeats for left parenthesis

                        // left parenthesis, SPECIAL case: #repeats must be taken from data as int32
                        if (mcnf == 1) {
                            // set it to regular left parenthesis code
                            mcnf = 0;

                            // read "N" value from buffer
                            // (srcBuf automagically swaps in the getInt)
                            ncnf = srcBuf.getInt(srcPos);

                            // put swapped val back into buffer
                            // (destBuf automagically swaps in the putInt)
                            destBuf.putInt(destPos, ncnf);

                            srcPos  += 4;
                            destPos += 4;

#ifdef COMPOSITE_DEBUG
                            std::cout << "ncnf from data = " << ncnf << " (code 15)" << std::endl;
#endif
                        }

                        // left parenthesis, SPECIAL case: #repeats must be taken from data as int16
                        if (mcnf == 2) {
                            mcnf = 0;
                            ncnf = srcBuf.getShort(srcPos) & 0xffff;
                            destBuf.putShort(destPos, (short)ncnf);
                            srcPos  += 2;
                            destPos += 2;
#ifdef COMPOSITE_DEBUG
                            std::cout << "ncnf from data = " << ncnf << " (code 14)" << std::endl;
#endif
                        }

                        // left parenthesis, SPECIAL case: #repeats must be taken from data as int8
                        if (mcnf == 3) {
                            mcnf = 0;
                            ncnf = srcBuf.getByte(srcPos) & 0xff;
                            destBuf.put(destPos, (int8_t)ncnf);
                            srcPos++;
                            destPos++;
#ifdef COMPOSITE_DEBUG
                            std::cout << "ncnf from data = " << ncnf << " (code 13)" << std::endl;
#endif
                        }

                        // store ifmt[] index
                        lv[lev].left = imt;
                        // how many time will repeat format code inside parenthesis
                        lv[lev].nrepeat = ncnf;
                        // cleanup place for the right parenthesis (do not get it yet)
                        lv[lev].irepeat = 0;
                        // increase parenthesis level
                        lev++;
#ifdef COMPOSITE_DEBUG
                        std::cout << "4" << std::endl;
#endif
                    }
                    // format F or I or ...
                    else {
                        // there are no parenthesis, just simple format, go to processing
                        if (lev == 0) {
#ifdef COMPOSITE_DEBUG
                            std::cout << "51" << std::endl;
#endif
                        }
                        // have parenthesis, NOT the pre-last format element (last assumed ')' ?)
                        else if (imt != (nfmt-1)) {
#ifdef COMPOSITE_DEBUG
                            std::cout << "52: " << imt << " " << (nfmt-1) << std::endl;
#endif
                        }
                        // have parenthesis, NOT the first format after the left parenthesis
                        else if (imt != lv[lev-1].left+1) {
#ifdef COMPOSITE_DEBUG
                            std::cout << "53: " << imt << " " << (nfmt-1) << std::endl;
#endif
                        }
                        else {
                            // If none of above, we are in the end of format
                            // so set format repeat to a big number.
                            ncnf = 999999999;
#ifdef COMPOSITE_DEBUG
                            std::cout << "54" << std::endl;
#endif
                        }
                        break;
                    }
                }
            } // while(true)


            // if 'ncnf' is zero, get it from data
            if (ncnf == 0) {

                if (mcnf == 1) {
                    // read "N" value from buffer
                    // (srcBuf automagically swaps in the getInt)
                    ncnf = srcBuf.getInt(srcPos);

                    // put swapped val back into buffer
                    // (destBuf automagically swaps in the putInt)
                    destBuf.putInt(destPos, ncnf);

                    srcPos += 4;
                    destPos += 4;
                }
                else if (mcnf == 2) {
                    ncnf = srcBuf.getShort(srcPos) & 0xffff;
                    destBuf.putShort(destPos, (int16_t)ncnf);
                    srcPos += 2;
                    destPos += 2;
                }
                else if (mcnf == 3) {
                    ncnf = srcBuf.getByte(srcPos) & 0xff;
                    destBuf.put(destPos, (int8_t)ncnf);
                    srcPos++;
                    destPos++;
                }
#ifdef COMPOSITE_DEBUG
                std::cout << "ncnf from data = " << ncnf << std::endl;
#endif
            }


            // At this point we are ready to process next piece of data;.
            // We have following entry info:
            //     kcnf - format type
            //     ncnf - how many times format repeated
            //     b8   - starting data pointer (it comes from previously processed piece)

            // Convert data in according to type kcnf

            // If 64-bit
            if (kcnf == 8 || kcnf == 9 || kcnf == 10) {
                size_t b64EndIndex = srcPos + 8*ncnf;
                // make sure we don't go past end of data
                if (b64EndIndex > srcEndIndex) b64EndIndex = srcEndIndex;
                // swap all 64 bit items
                while (srcPos < b64EndIndex) {
                    // buffers are of opposite endianness so putLong will automagically swap
                    destBuf.putLong(destPos, srcBuf.getLong(srcPos));
                    srcPos  += 8;
                    destPos += 8;
                }

#ifdef COMPOSITE_DEBUG
                std::cout << "64bit: " << ncnf << " elements" << std::endl;
#endif
            }
            // 32-bit
            else if (kcnf == 1 || kcnf == 2 || kcnf == 11 || kcnf == 12) {
                size_t b32EndIndex = srcPos + 4*ncnf;
                // make sure we don't go past end of data
                if (b32EndIndex > srcEndIndex) b32EndIndex = srcEndIndex;
                // swap all 32 bit items
                while (srcPos < b32EndIndex) {
                    // buffers are of opposite endianness so putInt will automagically swap
                    destBuf.putInt(destPos, srcBuf.getInt(srcPos));
                    srcPos  += 4;
                    destPos += 4;
                }

#ifdef COMPOSITE_DEBUG
                std::cout << "32bit: " << ncnf << " elements" << std::endl;
#endif
            }
            // 16 bits
            else if (kcnf == 4 || kcnf == 5) {
                size_t b16EndIndex = srcPos + 2*ncnf;
                // make sure we don't go past end of data
                if (b16EndIndex > srcEndIndex) b16EndIndex = srcEndIndex;
                // swap all 16 bit items
                while (srcPos < b16EndIndex) {
                    // buffers are of opposite endianness so putShort will automagically swap
                    destBuf.putShort(destPos, srcBuf.getShort(srcPos));
                    srcPos  += 2;
                    destPos += 2;
                }

#ifdef COMPOSITE_DEBUG
                std::cout << "16bit: " << ncnf << " elements" << std::endl;
#endif
            }
            // 8 bits
            else if (kcnf == 6 || kcnf == 7 || kcnf == 3) {
                // copy characters if dest != src
                if (!inPlace) {
                    // Use absolute gets & puts
                    for (int j=0; j < ncnf; j++) {
                        destBuf.put(destPos+j, srcBuf.getByte(srcPos+j));
                    }
                }
                srcPos  += ncnf;
                destPos += ncnf;
#ifdef COMPOSITE_DEBUG
                std::cout << "8bit: " << ncnf << " elements" << std::endl;
#endif
            }

        } //while

#ifdef COMPOSITE_DEBUG
        std::cout << std::endl << "=== eviofmtswap end ===" << std::endl;
#endif

        if (inPlace) {
            // If swapped in place, we need to update the buffer's byte order
            srcBuf.order(destOrder);
        }
    }


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

 * @param srcBuf     data source data pointer.
 * @param destBuf    destination pointer or can be null if swapping in place.
 * @param nwrd       number of data words (32-bit ints) to be swapped.
 * @param ifmt       format list as produced by {@link #compositeFormatToInt(String)}.
 * @param padding    number of bytes to ignore in last data word (starting from data end).
 * @param srcIsLocal true if src is local endian, else false.
 *
 * @throws EvioException if ifmt empty or nwrd &lt; 2;
 *                       src pointer is null;
 *                       if src & dest are not identical but overlap.
 */
void CompositeData::swapData(int32_t *src, int32_t *dest, size_t nwrd,
                             const std::vector<uint16_t> & ifmt,
                             uint32_t padding, bool srcIsLocal) {

    // check args
    if (src == nullptr) {
        throw EvioException("src pointer null");
    }

    if (ifmt.empty()) {
        throw EvioException("ifmt arg empty");
    }
    // size of int list
    size_t nfmt = ifmt.size();

    if (nwrd < 2) {
        throw EvioException("number of words to swap must be >= 2");
    }

    bool inPlace = false;
    if (src == dest || dest == nullptr) {
        // We're swapping in place
        inPlace = true;
        dest = src;
    }
    else {
        // Check to see if src & dest memories overlap
        if (((dest + nwrd) > src) && (dest < (src + nwrd))) {
            throw EvioException("src & dest memories not identical but overlap");
        }
    }

    int imt  = 0;   /* ifmt[] index */
    int ncnf = 0;   /* how many times must repeat a format */
    int lev  = 0;   /* parenthesis level */
    int kcnf, mcnf, iterm = 0;
    int64_t *b64, *b64end, *b64dest;
    int32_t *b32, *b32end, *b32dest;
    int16_t *b16, *b16end, *b16dest;
    auto b8     = reinterpret_cast<int8_t *>(&src[0]);   /* beginning of data */
    auto b8end  = reinterpret_cast<int8_t *>(&src[nwrd]) - padding; /* end of data + 1 - padding */
    auto b8dest = reinterpret_cast<int8_t *>(dest);
    LV lv[10];


    while (b8 < b8end) {

#ifdef COMPOSITE_DEBUG
        std::cout << ""+++ begin = " << std::showbase << std::hex << std::setw(8) << b8 <<
                                   ", end = " << b8end << dec << std::endl;
#endif
        // get next format code
        while (true) {
            imt++;
            // end of format statement reached, back to iterm - last parenthesis or format begining
            if (imt > nfmt) {
                //imt = iterm;
                imt = 0;
#ifdef COMPOSITE_DEBUG
                std::cout << "1" << std::endl;
#endif
            }
                // meet right parenthesis, so we're finished processing format(s) in parenthesis
            else if (ifmt[imt-1] == 0) {

                // right parenthesis without preceding left parenthesis? -> illegal format
                if (lev == 0) {
                    throw EvioException("illegal format");
                }

                // increment counter
                lv[lev-1].irepeat++;

                // if format in parenthesis was processed the required number of times
                if (lv[lev-1].irepeat >= lv[lev-1].nrepeat) {
                    // store left parenthesis index minus 1
                    // (if will meet end of format, will start from format index imt=iterm;
                    // by default we continue from the beginning of the format (iterm=0)) */
                    //iterm = lv[lev-1].left - 1;
                    // done with this level - decrease parenthesis level
                    lev--;

#ifdef COMPOSITE_DEBUG
                    std::cout << "2" << std::endl;
#endif
                }
                    // go for another round of processing by setting 'imt' to the left parenthesis
                else {
                    // points ifmt[] index to the left parenthesis from the same level 'lev'
                    imt = lv[lev-1].left;
#ifdef COMPOSITE_DEBUG
                    std::cout << "3" << std::endl;
#endif
                }
            }
            else {
                ncnf = (ifmt[imt-1] >> 8) & 0x3F;  /* how many times to repeat format code */
                kcnf =  ifmt[imt-1] & 0xFF;        /* format code */
                mcnf = (ifmt[imt-1] >> 14) & 0x3;  /* repeat code */

                // left parenthesis - set new lv[]
                if (kcnf == 0) {

                    // check repeats for left parenthesis

                    // left parenthesis, SPECIAL case: #repeats must be taken from data as int32
                    if (mcnf == 1) {
                        // set it to regular left parenthesis code
                        mcnf = 0;

                        b32     = reinterpret_cast<int32_t *>(b8);
                        b32dest = reinterpret_cast<int32_t *>(b8dest);

                        if (srcIsLocal) {
                            // read #repeats (N) from data buffer
                            ncnf = *b32;
                            // swap data
                            *b32dest = SWAP_32(ncnf);
                        }
                        else {
                            ncnf = *b32dest = SWAP_32(*b32);
                        }

                        b8 += 4;
                        b8dest += 4;

#ifdef COMPOSITE_DEBUG
                        std::cout << "ncnf from data = " << ncnf << " (code 15)" << std::endl;
#endif
                    }

                    // left parenthesis, SPECIAL case: #repeats must be taken from data as int16
                    if (mcnf == 2) {
                        mcnf = 0;

                        b16     = reinterpret_cast<int16_t *>(b8);
                        b16dest = reinterpret_cast<int16_t *>(b8dest);

                        if (srcIsLocal) {
                            ncnf = *b16;
                            *b16dest = SWAP_16(ncnf);
                        }
                        else {
                            ncnf = *b16dest = SWAP_16(*b16);
                        }

                        b8 += 2;
                        b8dest += 2;

#ifdef COMPOSITE_DEBUG
                        std::cout << "ncnf from data = " << ncnf << " (code 14)" << std::endl;
#endif
                    }

                    // left parenthesis, SPECIAL case: #repeats must be taken from data as int8
                    if (mcnf == 3) {
                        mcnf = 0;
                        ncnf = *b8;
                        *b8dest = ncnf;
                        b8++;
                        b8dest++;

#ifdef COMPOSITE_DEBUG
                        std::cout << "ncnf from data = " << ncnf << " (code 13)" << std::endl;
#endif
                    }

                    // store ifmt[] index
                    lv[lev].left = imt;
                    // how many time will repeat format code inside parenthesis
                    lv[lev].nrepeat = ncnf;
                    // cleanup place for the right parenthesis (do not get it yet)
                    lv[lev].irepeat = 0;
                    // increase parenthesis level
                    lev++;
#ifdef COMPOSITE_DEBUG
                    std::cout << "4" << std::endl;
#endif
                }
                // format F or I or ...
                else {
                    // there are no parenthesis, just simple format, go to processing
                    if (lev == 0) {
#ifdef COMPOSITE_DEBUG
                        std::cout << "51" << std::endl;
#endif
                    }
                        // have parenthesis, NOT the pre-last format element (last assumed ')' ?)
                    else if (imt != (nfmt-1)) {
#ifdef COMPOSITE_DEBUG
                        std::cout << "52: " << imt << " " << (nfmt-1) << std::endl;
#endif
                    }
                    // have parenthesis, NOT the first format after the left parenthesis
                    else if (imt != lv[lev-1].left+1) {
#ifdef COMPOSITE_DEBUG
                        std::cout << "53: " << imt << " " << (nfmt-1) << std::endl;
#endif
                    }
                    else {
                        // If none of above, we are in the end of format
                        // so set format repeat to a big number.
                        ncnf = 999999999;
#ifdef COMPOSITE_DEBUG
                        std::cout << "54" << std::endl;
#endif
                    }
                    break;
                }
            }
        } // while(true)


        // if 'ncnf' is zero, get it from data
        if (ncnf == 0) {

            if (mcnf == 1) {
                // read "N" value from buffer
                b32     = reinterpret_cast<int32_t *>(b8);
                b32dest = reinterpret_cast<int32_t *>(b8dest);

                if (srcIsLocal) {
                    ncnf = *b32;
                    *b32dest = SWAP_32(ncnf);
                }
                else {
                    ncnf = *b32dest = SWAP_32(*b32);
                }

                b8 += 4;
                b8dest += 4;
            }
            else if (mcnf == 2) {
                b16     = reinterpret_cast<int16_t *>(b8);
                b16dest = reinterpret_cast<int16_t *>(b8dest);

                if (srcIsLocal) {
                    ncnf = *b16;
                    *b16dest = SWAP_16(ncnf);
                }
                else {
                    ncnf = *b16dest = SWAP_16(*b16);
                }

                b8 += 2;
                b8dest += 2;

            }
            else if (mcnf == 3) {
                *b8dest = ncnf = *b8;
                b8++;
                b8dest++;
            }
#ifdef COMPOSITE_DEBUG
            std::cout << "ncnf from data = " << ncnf << std::endl;
#endif
        }


        // At this point we are ready to process next piece of data;.
        // We have following entry info:
        //     kcnf - format type
        //     ncnf - how many times format repeated
        //     b8   - starting data pointer (it comes from previously processed piece)

        // Convert data in according to type kcnf

        // If 64-bit
        if (kcnf == 8 || kcnf == 9 || kcnf == 10) {
            b64     = reinterpret_cast<int64_t *>(b8);
            b64dest = reinterpret_cast<int64_t *>(b8dest);

            b64end = b64 + ncnf;
            if (b64end > reinterpret_cast<int64_t *>(b8end)) {
                b64end = reinterpret_cast<int64_t *>(b8end);
            }

            while (b64 < b64end) {
                *b64dest = SWAP_64(*b64);
                b64++;
                b64dest++;
            }

            b8     = reinterpret_cast<int8_t *>(b64);
            b8dest = reinterpret_cast<int8_t *>(b64dest);

#ifdef COMPOSITE_DEBUG
            std::cout << "64bit: " << ncnf << " elements" << std::endl;
#endif
        }
        // 32-bit
        else if (kcnf == 1 || kcnf == 2 || kcnf == 11 || kcnf == 12) {
            b32     = reinterpret_cast<int32_t *>(b8);
            b32dest = reinterpret_cast<int32_t *>(b8dest);

            b32end = b32 + ncnf;
            if (b32end > reinterpret_cast<int32_t *>(b8end)) {
                b32end = reinterpret_cast<int32_t *>(b8end);
            }

            while (b32 < b32end) {
                *b32dest = SWAP_32(*b32);
                b32++;
                b32dest++;
            }

            b8     = reinterpret_cast<int8_t *>(b32);
            b8dest = reinterpret_cast<int8_t *>(b32dest);

#ifdef COMPOSITE_DEBUG
            std::cout << "32bit: " << ncnf << " elements" << std::endl;
#endif
        }
        // 16 bits
        else if (kcnf == 4 || kcnf == 5) {
            b16     = reinterpret_cast<int16_t *>(b8);
            b16dest = reinterpret_cast<int16_t *>(b8dest);

            b16end = b16 + ncnf;
            if (b16end > reinterpret_cast<int16_t *>(b8end)) {
                b16end = reinterpret_cast<int16_t *>(b8end);
            }

            while (b16 < b16end) {
                *b16dest = SWAP_16(*b16);
                b16++;
                b16dest++;
            }

            b8     = reinterpret_cast<int8_t *>(b16);
            b8dest = reinterpret_cast<int8_t *>(b16dest);

#ifdef COMPOSITE_DEBUG
            std::cout << "16bit: " << ncnf << " elements" << std::endl;
#endif
        }
        // 8 bits
        else if (kcnf == 6 || kcnf == 7 || kcnf == 3) {
            // copy characters if dest != src
            if (!inPlace) {
                std::memcpy((void *)b8dest, (const void *)b8, ncnf);
            }

            b8     += ncnf;
            b8dest += ncnf;

#ifdef COMPOSITE_DEBUG
            std::cout << "8bit: " << ncnf << " elements" << std::endl;
#endif
        }

    } //while

#ifdef COMPOSITE_DEBUG
    std::cout << std::endl << "=== eviofmtswap end ===" << std::endl;
#endif

}


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
void CompositeData::swapData(int32_t *iarr, int nwrd, const std::vector<uint16_t> & ifmt,
                            uint32_t padding) {
    swapData(iarr, iarr, nwrd, ifmt, padding, false);
}

// TODO: Following is Sergui's original function (eviofmtswap) which is now implemented immediately above.

///**
// * This method converts (swaps) an array of EVIO composite type data
// * between IEEE (big endian) and DECS (little endian) in place. This
// * data does <b>NOT</b> include the composite type's beginning tagsegment and
// * the format string it contains. It also does <b>NOT</b> include the data's
// * bank header words.
// *
// * Converts the data of array (iarr[i], i=0...nwrd-1)
// * using the format code      (ifmt[j], j=0...nfmt-1) .
// *
// * <p>
// * Algorithm description:<p>
// * Data processed inside while (ib &lt; nwrd)
// * loop, where 'ib' is iarr[] index;
// * loop breaks when 'ib' reaches the number of elements in iarr[]
// *
// *
// * @param iarr    pointer to data to be swapped
// * @param nwrd    number of data words (32-bit ints) to be swapped
// * @param ifmt    unsigned short vector holding translated format
// * @param padding number of bytes to ignore in last data word (starting from data end)
// *
// * @return  0 if success
// * @return -1 if nwrd &lt; 0, or ifmt vector empty.
// */
//int CompositeData::swapData(int32_t *iarr, int nwrd, const std::vector<uint16_t> & ifmt,
//                            uint32_t padding) {
//
//    if (nwrd <= 0 || ifmt.empty()) return(-1);
//    int nfmt = ifmt.size();
//
//    int imt  = 0;   /* ifmt[] index */
//    int ncnf = 0;   /* how many times must repeat a format */
//    int lev  = 0;   /* parenthesis level */
//    int kcnf, mcnf, iterm = 0;
//    int64_t *b64, *b64end;
//    int32_t *b32, *b32end;
//    int16_t *b16, *b16end;
//    auto b8    = reinterpret_cast<int8_t *>(&iarr[0]);   /* beginning of data */
//    auto b8end = reinterpret_cast<int8_t *>(&iarr[nwrd]) - padding; /* end of data + 1 - padding */
//    LV lv[10];
//
//#ifdef COMPOSITE_DEBUG
//    std::cout << std::endl << "=== eviofmtswap start ===" << std::endl;
//#endif
//
//    while (b8 < b8end)
//    {
//#ifdef COMPOSITE_DEBUG
//        std::cout << ""+++ begin = " << std::showbase << std::hex << std::setw(8) << b8 <<
//                                   ", end = " << b8end << dec << std::endl;
//#endif
//        /* get next format code */
//        while (true)
//        {
//            imt++;
//            /* end of format statement reached, back to iterm - last parenthesis or format begining */
//            if (imt > nfmt)
//            {
//                imt = 0/*iterm*/; /* sergey: will always start format from the begining - for now ...*/
//#ifdef COMPOSITE_DEBUG
//                std::cout << "1" << std::endl;
//#endif
//            }
//                /* meet right parenthesis, so we're finished processing format(s) in parenthesis */
//            else if (ifmt[imt-1] == 0)
//            {
//                /* right parenthesis without preceding left parenthesis? -> illegal format */
//                if (lev == 0)
//                    return -1;
//
//                /* increment counter */
//                lv[lev-1].irepeat++;
//
//                /* if format in parenthesis was processed */
//                if (lv[lev-1].irepeat >= lv[lev-1].nrepeat)
//                {
//                    /* required number of times */
//
//                    /* store left parenthesis index minus 1
//                       (if will meet end of format, will start from format index imt=iterm;
//                       by default we continue from the beginning of the format (iterm=0)) */
//                    //iterm = lv[lev-1].left - 1;
//                    lev--; /* done with this level - decrease parenthesis level */
//#ifdef COMPOSITE_DEBUG
//                    std::cout << "2" << std::endl;
//#endif
//                }
//                    /* go for another round of processing by setting 'imt' to the left parenthesis */
//                else
//                {
//                    /* points ifmt[] index to the left parenthesis from the same level 'lev' */
//                    imt = lv[lev-1].left;
//#ifdef COMPOSITE_DEBUG
//                    std::cout << "3" << std::endl;
//#endif
//                }
//            }
//            else
//            {
//                ncnf = (ifmt[imt-1] >> 8) & 0x3F; /* how many times to repeat format code */
//                kcnf = ifmt[imt-1] & 0xFF;        /* format code */
//                mcnf = (ifmt[imt-1] >> 14) & 0x3; /* repeat code */
//
//                /* left parenthesis - set new lv[] */
//                if (kcnf == 0)
//                {
//                    /* check repeats for left parenthesis */
//
//                    if(mcnf==1) /* left parenthesis, SPECIAL case: #repeats must be taken from int32 data */
//                    {
//                        mcnf = 0;
//                        b32 = reinterpret_cast<int32_t *>(b8);
//                        ncnf = *b32 = SWAP_32(*b32); /* get #repeats from data */
//#ifdef COMPOSITE_PRINT
//                        std::cout << "ncnf(: " << ncnf << std::endl;
//#endif
//                        b8 += 4;
//#ifdef COMPOSITE_DEBUG
//                        std::cout << "ncnf from data = " << ncnf << " (code 15)" << std::endl;
//#endif
//                    }
//
//                    if(mcnf==2) /* left parenthesis, SPECIAL case: #repeats must be taken from int16 data */
//                    {
//                        mcnf = 0;
//                        b16 = reinterpret_cast<int16_t *>(b8);
//                        ncnf = *b16 = SWAP_16(*b16); /* get #repeats from data */
//#ifdef COMPOSITE_PRINT
//                        std::cout << "ncnf(: " << ncnf << std::endl;
//#endif
//                        b8 += 2;
//#ifdef COMPOSITE_DEBUG
//                        std::cout << "ncnf from data = " << ncnf << " (code 14)" << std::endl;
//#endif
//                    }
//
//                    if(mcnf==3) /* left parenthesis, SPECIAL case: #repeats must be taken from int8 data */
//                    {
//                        mcnf = 0;
//                        ncnf = *((uint8_t *)b8); /* get #repeats from data */
//#ifdef COMPOSITE_PRINT
//                        std::cout << "ncnf(: " << ncnf << std::endl;
//#endif
//                        b8++;
//#ifdef COMPOSITE_DEBUG
//                        std::cout << "ncnf from data = " << ncnf << " (code 13)" << std::endl;
//#endif
//                    }
//
//
//                    /* store ifmt[] index */
//                    lv[lev].left = imt;
//                    /* how many time will repeat format code inside parenthesis */
//                    lv[lev].nrepeat = ncnf;
//                    /* cleanup place for the right parenthesis (do not get it yet) */
//                    lv[lev].irepeat = 0;
//                    /* increase parenthesis level */
//                    lev++;
//#ifdef COMPOSITE_DEBUG
//                    std::cout << "4" << std::endl;
//#endif
//                }
//                /* format F or I or ... */
//                else
//                {
//                    /* there are no parenthesis, just simple format, go to processing */
//                    if (lev == 0) {
//#ifdef COMPOSITE_DEBUG
//                        std::cout << "51" << std::endl;
//#endif
//                    }
//                    /* have parenthesis, NOT the pre-last format element (last assumed ')' ?) */
//                    else if (imt != (nfmt-1)) {
//#ifdef COMPOSITE_DEBUG
//                        std::cout << "52: " << imt << " " << (nfmt-1) << std::endl;
//#endif
//                    }
//                    /* have parenthesis, NOT the first format after the left parenthesis */
//                    else if (imt != lv[lev-1].left+1) {
//#ifdef COMPOSITE_DEBUG
//                        std::cout << "53: " << imt << " " << (nfmt-1) << std::endl;
//#endif
//                    }
//                    /* if none of above, we are in the end of format */
//                    else
//                    {
//                        /* set format repeat to the big number */
//                        ncnf = 999999999;
//#ifdef COMPOSITE_DEBUG
//                        std::cout << "54" << std::endl;
//#endif
//                    }
//                    break;
//                }
//            }
//        } /* while(1) */
//
//        /* if 'ncnf' is zero, get it from data */
//        /*printf("ncnf=%d\n",ncnf);fflush(stdout);*/
//        if(ncnf==0)
//        {
//            if(mcnf==1)
//            {
//                b32 = (int *)b8;
//                ncnf = *b32 = SWAP_32(*b32);
//                /*printf("ncnf32=%d\n",ncnf);fflush(stdout);*/
//                b8 += 4;
//            }
//            else if(mcnf==2)
//            {
//                b16 = (short *)b8;
//                ncnf = *b16 = SWAP_16(*b16);
//                /*printf("ncnf16=%d\n",ncnf);fflush(stdout);*/
//                b8 += 2;
//            }
//            else if(mcnf==3)
//            {
//                ncnf = *b8;
//                /*printf("ncnf08=%d\n",ncnf);fflush(stdout);*/
//                b8 += 1;
//            }
//            /*else printf("ncnf00=%d\n",ncnf);fflush(stdout);*/
//
//#ifdef COMPOSITE_DEBUG
//            std::cout << "ncnf from data = " << ncnf << std::endl;
//#endif
//        }
//
//
//        /* At this point we are ready to process next piece of data.
//           We have following entry info:
//           kcnf - format type
//           ncnf - how many times format repeated
//           b8   - starting data pointer (it comes from previously processed piece)
//        */
//
//        /* Convert data in according to type kcnf */
//
//        /* 64-bits */
//        if (kcnf == 8 || kcnf == 9 || kcnf == 10) {
//            b64 = (int64_t *)b8;
//            b64end = b64 + ncnf;
//            if (b64end > (int64_t *)b8end) b64end = (int64_t *)b8end;
//            while (b64 < b64end) {
//                *b64 = SWAP_64(*b64);
//                b64++;
//            }
//            b8 = (int8_t *)b64;
//#ifdef COMPOSITE_DEBUG
//            std::cout << "64bit: " << ncnf << " elements" << std::endl;
//#endif
//        }
//        /* 32-bits */
//        else if ( kcnf == 1 || kcnf == 2 || kcnf == 11 || kcnf == 12) {
//            b32 = (int32_t *)b8;
//            b32end = b32 + ncnf;
//            if (b32end > (int32_t *)b8end) b32end = (int32_t *)b8end;
//            while (b32 < b32end) {
//                *b32 = SWAP_32(*b32);
//                b32++;
//            }
//            b8 = (int8_t *)b32;
//#ifdef COMPOSITE_DEBUG
//            std::cout << "32bit: " << ncnf << " elements, b8 = " << std::showbase <<
//                         std::hex << std::setw(8) << b8 << dec << std::endl;
//#endif
//        }
//        /* 16 bits */
//        else if ( kcnf == 4 || kcnf == 5) {
//            b16 = (int16_t *)b8;
//            b16end = b16 + ncnf;
//            if (b16end > (int16_t *)b8end) b16end = (int16_t *)b8end;
//            while (b16 < b16end) {
//                *b16 = SWAP_16(*b16);
//                b16++;
//            }
//            b8 = (int8_t *)b16;
//#ifdef COMPOSITE_DEBUG
//            std::cout << "16bit: " << ncnf << " elements" << std::endl;
//#endif
//        }
//        /* 8 bits */
//        else if ( kcnf == 6 || kcnf == 7 || kcnf == 3) {
//            /* do nothing for characters */
//            b8 += ncnf;
//#ifdef COMPOSITE_DEBUG
//            std::cout << "8bit: " << ncnf << " elements" << std::endl;
//#endif
//        }
//
//    } /* while(b8 < b8end) */
//
//#ifdef COMPOSITE_DEBUG
//    std::cout << std::endl << "=== eviofmtswap end ===" << std::endl;
//#endif
//
//    return(0);
//}


    /**
     * This method takes a CompositeData object and a transformed format string
     * and uses that to write data into a buffer/array in raw form.
     *
     * @param rawBuf   data buffer in which to put the raw bytes
     * @param data     data to convert to raw bytes
     * @param ifmt     format list as produced by {@link #compositeFormatToInt(String)}
     *
     * @throws EvioException if ifmt size <= 0; if srcBuf or destBuf is too
     *                       small; not enough dataItems for the given format
     */
    void CompositeData::dataToRawBytes(ByteBuffer & rawBuf, CompositeData::Data const & data,
            std::vector<uint16_t> & ifmt) {

            bool debug = false;
            int kcnf, mcnf;

            // check args

            // size of format list
            int nfmt = ifmt.size();
            if (ifmt.empty()) {
                throw EvioException("empty format list");
            }

            LV lv[10];

            int imt   = 0;  // ifmt[] index
            int lev   = 0;  // parenthesis level
            int ncnf  = 0;  // how many times must repeat a format
            int iterm = 0;

            int itemIndex = 0;
            int itemCount = data.dataItems.size();

            // write each item
            for (itemIndex = 0; itemIndex < itemCount;) {

                // get next format code
                while (true) {
                    imt++;
                    // end of format statement reached, back to iterm - last parenthesis or format begining
                    if (imt > nfmt) {
                        imt = 0;
                    }
                        // meet right parenthesis, so we're finished processing format(s) in parenthesis
                    else if (ifmt[imt-1] == 0) {
                        // increment counter
                        lv[lev-1].irepeat++;

                        // if format in parenthesis was processed the required number of times
                        if (lv[lev-1].irepeat >= lv[lev-1].nrepeat) {
                            // store left parenthesis index minus 1
                            iterm = lv[lev-1].left - 1;
                            // done with this level - decrease parenthesis level
                            lev--;
                        }
                            // go for another round of processing by setting 'imt' to the left parenthesis
                        else {
                            // points ifmt[] index to the left parenthesis from the same level 'lev'
                            imt = lv[lev-1].left;
                        }
                    }
                    else {
                        ncnf = (ifmt[imt-1] >> 8) & 0x3F;  /* how many times to repeat format code */
                        kcnf =  ifmt[imt-1] & 0xFF;        /* format code */
                        mcnf = (ifmt[imt-1] >> 14) & 0x3;  /* repeat code */

                        // left parenthesis - set new lv[]
                        if (kcnf == 0) {

                            // check repeats for left parenthesis

                            // left parenthesis, SPECIAL case: #repeats must be taken from data as int
                            if (mcnf == 1) {
                                // set it to regular left parenthesis code
                                mcnf = 0;

                                // get "N" value from List
                                if (data.dataTypes[itemIndex] != DataType::INT32) {
                                    throw EvioException("Data type mismatch");
                                }
                                ncnf = data.dataItems[itemIndex++].item.i32;
#ifdef COMPOSITE_DEBUG
                                std::cout << "ncnf from list = " << ncnf << " (code 15)" << std::endl;
#endif

                                // put into buffer (relative put)
                                rawBuf.putInt(ncnf);
                            }

                            // left parenthesis, SPECIAL case: #repeats must be taken from data as short
                            if (mcnf == 2) {
                                mcnf = 0;

                                // get "n" value from List
                                if (data.dataTypes[itemIndex] != DataType::SHORT16) {
                                    throw EvioException("Data type mismatch");
                                }
                                // Get rid of sign extension to allow n to be unsigned
                                ncnf = data.dataItems[itemIndex++].item.s16;
#ifdef COMPOSITE_DEBUG
                                std::cout << "ncnf from list = " << ncnf << " (code 14)" << std::endl;
#endif

                                // put into buffer (relative put)
                                rawBuf.putShort((int16_t)ncnf);
                            }

                            // left parenthesis, SPECIAL case: #repeats must be taken from data as byte
                            if (mcnf == 3) {
                                mcnf = 0;

                                // get "m" value from List
                                if (data.dataTypes[itemIndex] != DataType::CHAR8) {
                                    throw EvioException("Data type mismatch");
                                }
                                // Get rid of sign extension to allow m to be unsigned
                                ncnf = data.dataItems[itemIndex++].item.b8;

#ifdef COMPOSITE_DEBUG
                                std::cout << "ncnf from list = " << ncnf << " (code 13)" << std::endl;
#endif

                                // put into buffer (relative put)
                                rawBuf.put((uint8_t)ncnf);
                            }

                            // store ifmt[] index
                            lv[lev].left = imt;
                            // how many time will repeat format code inside parenthesis
                            lv[lev].nrepeat = ncnf;
                            // cleanup place for the right parenthesis (do not get it yet)
                            lv[lev].irepeat = 0;
                            // increase parenthesis level
                            lev++;
                        }
                        // format F or I or ...
                        else {
                            // there are no parenthesis, just simple format, go to processing
                            if (lev == 0) {
                            }
                            // have parenthesis, NOT the pre-last format element (last assumed ')' ?)
                            else if (imt != (nfmt-1)) {
                            }
                            // have parenthesis, NOT the first format after the left parenthesis
                            else if (imt != lv[lev-1].left+1) {
                            }
                            else {
                                // If none of above, we are in the end of format
                                // so set format repeat to a big number.
                                ncnf = 999999999;
                            }
                            break;
                        }
                    }
                } // while(true)


                // if 'ncnf' is zero, get N,n,m from list
                if (ncnf == 0) {

                    if (mcnf == 1) {
                        // get "N" value from List
                        if (data.dataTypes[itemIndex] != DataType::INT32) {
                            throw EvioException("Data type mismatch");
                        }
                        ncnf = data.dataItems[itemIndex++].item.i32;

                        // put into buffer (relative put)
                        rawBuf.putInt(ncnf);
                    }
                    else if (mcnf == 2) {
                        // get "n" value from List
                        if (data.dataTypes[itemIndex] != DataType::SHORT16) {
                            throw EvioException("Data type mismatch");
                        }
                        ncnf = data.dataItems[itemIndex++].item.s16;
                        rawBuf.putShort((int16_t)ncnf);
                    }
                    else if (mcnf == 3) {
                        // get "m" value from List
                        if (data.dataTypes[itemIndex] != DataType::CHAR8) {
                            throw EvioException("Data type mismatch");
                        }
                        ncnf = data.dataItems[itemIndex++].item.b8;
                        rawBuf.put((int8_t)ncnf);
                    }

#ifdef COMPOSITE_DEBUG
            std::cout << "ncnf from data = " << ncnf << std::endl;
#endif
                }


                // At this point we are ready to process next piece of data;.
                // We have following entry info:
                //     kcnf - format type
                //     ncnf - how many times format repeated

                // Convert data type kcnf
#ifdef COMPOSITE_DEBUG
                std::cout << "Convert data of type = " << kcnf << ", itemIndex = " << itemIndex << std::endl;
#endif
                switch (kcnf) {
                    // 64 Bits
                    case 8:
                        for (int j=0; j < ncnf; j++) {
                            if (data.dataTypes[itemIndex] != DataType::DOUBLE64) {
                                throw EvioException("Data type mismatch, expecting DOUBLE64, got " +
                                                    data.dataTypes[itemIndex].toString());
                            }
                            rawBuf.putDouble(data.dataItems[itemIndex++].item.dbl);
                        }
                        break;

                    case 9:
                        for (int j=0; j < ncnf; j++) {
                            if (data.dataTypes[itemIndex] != DataType::LONG64) {
                                throw EvioException("Data type mismatch, expecting LONG64, got " +
                                                     data.dataTypes[itemIndex].toString());
                            }
                            rawBuf.putLong(data.dataItems[itemIndex++].item.l64);
                        }
                        break;

                    case 10:
                        for (int j=0; j < ncnf; j++) {
                            if (data.dataTypes[itemIndex] != DataType::ULONG64) {
                                throw EvioException("Data type mismatch, expecting ULONG64, got " +
                                                     data.dataTypes[itemIndex].toString());
                            }
                            rawBuf.putLong(data.dataItems[itemIndex++].item.ul64);
                        }
                        break;

                    // 32 Bits
                    case 11:
                        for (int j=0; j < ncnf; j++) {
                            if (data.dataTypes[itemIndex] != DataType::INT32) {
                                throw EvioException("Data type mismatch, expecting INT32, got " +
                                                                data.dataTypes[itemIndex].toString());
                            }
                            rawBuf.putInt(data.dataItems[itemIndex++].item.i32);
                        }
                        break;

                    case 1:
                        for (int j=0; j < ncnf; j++) {
                            if (data.dataTypes[itemIndex] != DataType::UINT32) {
                                throw EvioException("Data type mismatch, expecting UINT32, got " +
                                                                data.dataTypes[itemIndex].toString());
                            }
                            rawBuf.putInt(data.dataItems[itemIndex++].item.ui32);
                        }
                        break;

                    case 2:
                        for (int j=0; j < ncnf; j++) {
                            if (data.dataTypes[itemIndex] != DataType::FLOAT32) {
                                throw EvioException("Data type mismatch, expecting FLOAT32, got " +
                                                                data.dataTypes[itemIndex].toString());
                            }
                            rawBuf.putFloat(data.dataItems[itemIndex++].item.flt);
                        }
                        break;

                    case 12:
                        for (int j=0; j < ncnf; j++) {
                            if (data.dataTypes[itemIndex] != DataType::HOLLERIT) {
                                throw EvioException("Data type mismatch, expecting HOLLERIT, got " +
                                                                data.dataTypes[itemIndex].toString());
                            }
                            rawBuf.putInt(data.dataItems[itemIndex++].item.i32);
                        }
                        break;

                    // 16 bits
                    case 4:
                        for (int j=0; j < ncnf; j++) {
                            if (data.dataTypes[itemIndex] != DataType::SHORT16) {
                                throw EvioException("Data type mismatch, expecting SHORT16, got " +
                                                                data.dataTypes[itemIndex].toString());
                            }
                            rawBuf.putShort(data.dataItems[itemIndex++].item.s16);
                        }
                        break;

                    case 5:
                        for (int j=0; j < ncnf; j++) {
                            if (data.dataTypes[itemIndex] != DataType::USHORT16) {
                                throw EvioException("Data type mismatch, expecting USHORT16, got " +
                                                                data.dataTypes[itemIndex].toString());
                            }
                            rawBuf.putShort(data.dataItems[itemIndex++].item.us16);
                        }
                        break;

                    // 8 bits
                    case 6:
                        for (int j=0; j < ncnf; j++) {
                            if (data.dataTypes[itemIndex] != DataType::CHAR8) {
                                throw EvioException("Data type mismatch, expecting CHAR8, got " +
                                                                data.dataTypes[itemIndex].toString());
                            }
                            rawBuf.put(data.dataItems[itemIndex++].item.b8);
                        }
                        break;

                    case 7:
                        for (int j=0; j < ncnf; j++) {
                            if (data.dataTypes[itemIndex] != DataType::UCHAR8) {
                                throw EvioException("Data type mismatch, expecting UCHAR8, got " +
                                                                data.dataTypes[itemIndex].toString());
                            }
                            rawBuf.put(data.dataItems[itemIndex++].item.ub8);
                        }
                        break;

                    // String array
                    case 3:
                        if (data.dataTypes[itemIndex] != DataType::CHARSTAR8) {
                            throw EvioException("Data type mismatch, expecting string, got " +
                                                            data.dataTypes[itemIndex].toString());
                        }

                        // Convert String array into evio byte representation
                        auto strs = data.dataItems[itemIndex++].strVec;
                        std::vector<uint8_t> rb;
                        Util::stringsToRawBytes(strs, rb);
                        rawBuf.put(rb, 0, rb.size());

                        if (ncnf != rb.size()) {
                            throw EvioException("String format mismatch with string (array)");
                        }
                        //break;

                    //default:

                }
            }
    }



    /**
     * This method extracts and stores all the data items and their types in various lists.
     */
    void CompositeData::process() {

        int kcnf, mcnf;

        // size of int list
        int nfmt = formatInts.size();

        items.reserve(100);
        types.reserve(100);
        NList.reserve(100);
        nList.reserve(100);
        mList.reserve(100);

        items.clear();
        types.clear();
        NList.clear();
        nList.clear();
        mList.clear();

        LV lv[10];

        int imt   = 0;  // formatInts[] index
        int lev   = 0;  // parenthesis level
        int ncnf  = 0;  // how many times must repeat a format
        int iterm = 0;

        // start of data, index into data array
        int dataIndex = 0;
        // just past end of data -> size of data in bytes
        int endIndex = dataBytes;

        // This is where translating from Java to C++ loses something, like speed & memory.
        // In Java, the rawBytes and dataBuffer are parts of the same object. Here, to make
        // the code work, we need to copy the data from rawBytes into a local ByteBuffer.
        // Fortunately, this is only called once, from one constructor.
        ByteBuffer dataBuffer(dataBytes);
        std::memcpy(dataBuffer.array(), rawBytes.data() + 4*dataOffset, dataBytes);
        dataBuffer.order(byteOrder);


        while (dataIndex < endIndex) {

            while (true) {
                imt++;
                if (imt > nfmt) {
                    imt = 0;
                }
                else if (formatInts[imt-1] == 0) {
                    lv[lev-1].irepeat++;

                    if (lv[lev-1].irepeat >= lv[lev-1].nrepeat) {
                        iterm = lv[lev-1].left - 1;
                        lev--;
                    }
                    else {
                        imt = lv[lev-1].left;
                    }
                }
                else {
                    ncnf = (formatInts[imt-1] >> 8) & 0x3F;
                    kcnf =  formatInts[imt-1] & 0xFF;
                    mcnf = (formatInts[imt-1] >> 14) & 0x3;

                    if (kcnf == 0) {

                        if (mcnf == 1) {
                            mcnf = 0;
                            ncnf = dataBuffer.getInt(dataIndex);

                            NList.push_back(ncnf);
                            DataItem mem;
                            mem.item.i32 = ncnf;
                            items.push_back(mem);
                            types.push_back(DataType::NVALUE);

                            dataIndex += 4;
                        }

                        if (mcnf == 2) {
                            mcnf = 0;
                            ncnf = dataBuffer.getShort(dataIndex);

                            nList.push_back((int16_t)ncnf);
                            DataItem mem;
                            mem.item.s16 = ncnf;
                            items.push_back(mem);
                            types.push_back(DataType::nVALUE);

                            dataIndex += 2;
                        }

                        if (mcnf == 3) {
                            mcnf = 0;
                            ncnf = dataBuffer.getByte(dataIndex);

                            mList.push_back((int8_t)ncnf);
                            DataItem mem;
                            mem.item.b8 = ncnf;
                            items.push_back(mem);
                            types.push_back(DataType::mVALUE);

                            dataIndex++;
                        }

                        lv[lev].left = imt;
                        lv[lev].nrepeat = ncnf;
                        lv[lev].irepeat = 0;
                        lev++;
                    }
                    else {
                        if (lev == 0) {
                        }
                        else if (imt != (nfmt-1)) {
                        }
                        else if (imt != lv[lev-1].left+1) {
                        }
                        else {
                            ncnf = 999999999;
                        }
                        break;
                    }
                }
            } // while(true)


            // if 'ncnf' is zero, get N,n,m from list
            if (ncnf == 0) {
                if (mcnf == 1) {
                    // read "N" value from buffer
                    ncnf = dataBuffer.getInt(dataIndex);
                    NList.push_back(ncnf);
                    DataItem mem;
                    mem.item.i32 = ncnf;
                    items.push_back(mem);
                    types.push_back(DataType::NVALUE);
                    dataIndex += 4;
                }
                else if (mcnf == 2) {
                    // read "n" value from buffer
                    ncnf = dataBuffer.getShort(dataIndex);
                    nList.push_back((int16_t)ncnf);
                    DataItem mem;
                    mem.item.s16 = ncnf;
                    items.push_back(mem);
                    types.push_back(DataType::nVALUE);
                    dataIndex += 2;
                }
                else if (mcnf == 3) {
                    // read "m" value from buffer
                    ncnf = dataBuffer.getByte(dataIndex);
                    mList.push_back((int8_t)ncnf);
                    DataItem mem;
                    mem.item.b8 = ncnf;
                    items.push_back(mem);
                    types.push_back(DataType::mVALUE);
                    dataIndex++;
                }
            }


            // At this point we are ready to process next piece of data;.
            // We have following entry info:
            //     kcnf - format type
            //     ncnf - how many times format repeated
            //     b8   - starting data pointer (it comes from previously processed piece)

            // Convert data in according to type kcnf

            // If 64-bit
            if (kcnf == 8 || kcnf == 9 || kcnf == 10) {
                int b64EndIndex = dataIndex + 8*ncnf;
                // make sure we don't go past end of data
                if (b64EndIndex > endIndex) b64EndIndex = endIndex;

                while (dataIndex < b64EndIndex) {
                    // store its data type
                    types.push_back(DataType::getDataType(kcnf));

                    DataItem mem;

                    // double
                    if (kcnf == 8) {
                        mem.item.dbl = dataBuffer.getDouble(dataIndex);
                        items.push_back(mem);
                    }
                    // 64 bit int/uint
                    else if (kcnf == 9) {
                        mem.item.l64 = dataBuffer.getLong(dataIndex);
                        items.push_back(mem);
                    }
                    else {
                        mem.item.ul64 = dataBuffer.getULong(dataIndex);;
                        items.push_back(mem);
                    }

                    dataIndex += 8;
                }
            }
            // 32-bit
            else if (kcnf == 1 || kcnf == 2 || kcnf == 11 || kcnf == 12) {
                int b32EndIndex = dataIndex + 4*ncnf;
                // make sure we don't go past end of data
                if (b32EndIndex > endIndex) b32EndIndex = endIndex;

                while (dataIndex < b32EndIndex) {
                    DataItem mem;

                    // Hollerit
                    if (kcnf == 12) {
                        mem.item.i32 = dataBuffer.getInt(dataIndex);
                        items.push_back(mem);
                        types.push_back(DataType::HOLLERIT);
                    }
                    // 32 bit float
                    else if (kcnf == 2) {
                        mem.item.flt = dataBuffer.getFloat(dataIndex);
                        items.push_back(mem);
                        types.push_back(DataType::getDataType(kcnf));
                    }
                    // 32 bit int/uint
                    else if (kcnf == 1) {
                        mem.item.ui32 = dataBuffer.getUInt(dataIndex);
                        items.push_back(mem);
                        types.push_back(DataType::getDataType(kcnf));
                    }
                    else {
                        mem.item.i32 = dataBuffer.getInt(dataIndex);
                        items.push_back(mem);
                        types.push_back(DataType::getDataType(kcnf));
                    }

                    dataIndex += 4;
                }
            }
            // 16 bits
            else if (kcnf == 4 || kcnf == 5) {
                int b16EndIndex = dataIndex + 2*ncnf;
                // make sure we don't go past end of data
                if (b16EndIndex > endIndex) b16EndIndex = endIndex;

                // swap all 16 bit items
                while (dataIndex < b16EndIndex) {
                    types.push_back(DataType::getDataType(kcnf));

                    DataItem mem;

                    if (kcnf == 5) {
                        mem.item.us16 = dataBuffer.getUShort(dataIndex);
                        items.push_back(mem);
                    }
                    else {
                        mem.item.s16 = dataBuffer.getShort(dataIndex);
                        items.push_back(mem);
                    }

                    dataIndex += 2;
                }
            }
            // 8 bits
            else if (kcnf == 6 || kcnf == 7 || kcnf == 3) {
                int b8EndIndex = dataIndex + ncnf;
                // make sure we don't go past end of data
                if (b8EndIndex > endIndex) {
                    //b8EndIndex = endIndex;
                    //ncnf = endIndex - b8EndIndex; // BUG found 9/5/2018
                    ncnf = endIndex - dataIndex;
                }

                dataBuffer.position(dataIndex);
                DataItem mem;

                std::vector<uint8_t> bytes(ncnf);
                // relative read
                dataBuffer.getBytes(bytes, 0, ncnf);

                // string array
                if (kcnf == 3) {
                    std::vector<string> strs;
                    Util::unpackRawBytesToStrings(bytes, 0, ncnf, strs);
                    mem.item.str = true;
                    mem.strVec = strs;
                    items.push_back(mem);
                }
                // char
                else if (kcnf == 6) {
                    for (int i=0; i < ncnf; i++) {
                        mem.item.b8 = bytes[i];
                        items.push_back(mem);
                    }
                }
                // uchar
                else {
                    for (int i=0; i < ncnf; i++) {
                        mem.item.ub8 = bytes[i];
                        items.push_back(mem);
                    }
                }

                // reset position
                dataBuffer.position(0);

                types.push_back(DataType::getDataType(kcnf));
                dataIndex += ncnf;
            }

        } //while

    }


    /**
     * Obtain a string representation of the composite data.
     * @return a string representation of the composite data.
     */
    string CompositeData::toString() const {return toString("");}


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
    string CompositeData::toString(const string & indent, bool hex) {
        stringstream ss;
        if (hex) {
            ss << hex << showbase;
        }

        int numItems = items.size();

        uint32_t getIndexOrig = getIndex;

        for (int i=0; i < items.size(); i++) {
            if (i%5 == 0) ss << indent;

            DataType typ = types[i];

            if (typ == DataType::NVALUE) {
                ss << "N=" << getNValue();
            }
            else if (typ == DataType::nVALUE) {
                ss << "n=" << getnValue();
            }
            else if (typ == DataType::mVALUE) {
                ss << "m=" << getmValue();
            }
            else if (typ == DataType::INT32) {
                ss << "I=" << getInt();
            }
            else if (typ == DataType::UINT32) {
                ss << "i=" << getUInt();
            }
            else if (typ == DataType::HOLLERIT) {
                ss << "A=" << getHollerit();
            }
            else if (typ == DataType::CHAR8) {
                ss << "C=" << getChar();
            }
            else if (typ == DataType::UCHAR8) {
                ss << "c=" << getUChar();
            }
            else if (typ == DataType::SHORT16) {
                ss << "S=" << getShort();
            }
            else if (typ == DataType::USHORT16) {
                ss << "s=" << getUShort();
            }
            else if (typ == DataType::LONG64) {
                ss << "L=" << getLong();
            }
            else if (typ == DataType::ULONG64) {
                ss << "l=" << getULong();
            }
            else if (typ == DataType::DOUBLE64) {
                ss << "D=" << getDouble();
            }
            else if (typ == DataType::FLOAT32) {
                ss << "F=" << getFloat();
            }
            else if (typ == DataType::CHARSTAR8) {
                ss << "a=" << getFloat();

                auto strs = getStrings();
                for (int j=0; j < strs.size(); j++) {
                    ss << strs[j];
                    if (j < strs.size() - 1) {
                        ss << ",";
                    }
                }
            }

            if (i < numItems - 1) ss << ", ";
            if (((i+1)%5 == 0) && (i < numItems - 1)) ss << std::endl;
        }

        getIndex = getIndexOrig;

        return ss.str();
    }

    /**
     * This method returns a string representation of this CompositeData object
     * suitable for displaying in {@docRoot org.jlab.coda.jevio.graphics.EventTreeFrame}
     * gui. Each data item is separated from those before and after by a line.
     * Non-parenthesis repeats are printed together.
     *
     * @param hex if <code>true</code> then print integers in hexadecimal
     * @return  a string representation of this CompositeData object.
     */
    string CompositeData::toString(bool hex) const {
        stringstream ss;
        if (hex) {
            ss << hex << showbase;
        }

        // size of int list
        int nfmt = formatInts.size();

        LV lv[10];
        int imt   = 0;  // formatInts[] index
        int lev   = 0;  // parenthesis level
        int ncnf  = 0;  // how many times must repeat a format
        int iterm = 0;
        int kcnf, mcnf;

        // start of data, index into data array
        int dataIndex = 0;
        // just past end of data -> size of data in bytes
        int endIndex = dataBytes;

        // This is where translating from Java to C++ loses something, like speed & memory.
        // In Java, the rawBytes and dataBuffer are parts of the same object. Here, to make
        // the code work, we need to copy the data from rawBytes into a local ByteBuffer.
        ByteBuffer dataBuffer(dataBytes);
        std::memcpy(dataBuffer.array(), rawBytes.data() + 4*dataOffset, dataBytes);
        dataBuffer.order(byteOrder);

        //---------------------------------
        // First we have the format string
        //---------------------------------
        ss << format;
        ss << std::endl;

        while (dataIndex < endIndex) {

            // get next format code
            while (true) {
                imt++;
                // end of format statement reached, back to format beginning
                if (imt > nfmt) {
                    imt = 0;
                    ss << std::endl;  // end of one & beginning of another row
                }
                    // right parenthesis, so we're finished processing format(s) in parenthesis
                else if (formatInts[imt-1] == 0) {
                    lv[lev-1].irepeat++;
                    // if format in parenthesis was processed
                    if (lv[lev-1].irepeat >= lv[lev-1].nrepeat) {
                        iterm = lv[lev-1].left - 1;
                        lev--;
                    }
                        // go for another round of processing by setting 'imt' to the left parenthesis
                    else {
                        imt = lv[lev-1].left;
                        ss << std::endl;
                    }
                }
                else {
                    ncnf = (formatInts[imt-1] >> 8) & 0x3F;  /* how many times to repeat format code */
                    kcnf =  formatInts[imt-1] & 0xFF;        /* format code */
                    mcnf = (formatInts[imt-1] >> 14) & 0x3;  /* repeat code */

                    if (kcnf == 0) {

                        if (mcnf == 1) {
                            mcnf = 0;
                            // read "N" value from buffer
                            ncnf = dataBuffer.getInt(dataIndex);
                            dataIndex += 4;
                        }

                        if (mcnf == 2) {
                            mcnf = 0;
                            // read "N" value from buffer
                            ncnf = dataBuffer.getShort(dataIndex);
                            dataIndex += 2;
                        }
                        if (mcnf == 3) {
                            mcnf = 0;
                            // read "N" value from buffer
                            ncnf = dataBuffer.getByte(dataIndex);
                            dataIndex++;
                        }

                        // left parenthesis - set new lv[]
                        lv[lev].left = imt;
                        lv[lev].nrepeat = ncnf;
                        lv[lev].irepeat = 0;

                        ss << std::endl; // start of a repeat
                        lev++;
                    }
                    // single format (e.g. F, I, etc.)
                    else {
                        if ((lev == 0) ||
                            (imt != (nfmt-1)) ||
                            (imt != lv[lev-1].left+1)) {
                        }
                        else {
                            // If none of above, we are in the end of format
                            // so set format repeat to a big number.
                            ncnf = 999999999;
                        }
                        break;
                    }
                }
            }

            // if 'ncnf' is zero, get N,n,m from list
            if (ncnf == 0) {
                if (mcnf == 1) {
                    // read "N" value from buffer
                    ncnf = dataBuffer.getInt(dataIndex);
                    dataIndex += 4;
                }
                else if (mcnf == 2) {
                    // read "n" value from buffer
                    ncnf = dataBuffer.getShort(dataIndex);
                    dataIndex += 2;
                }
                else if (mcnf == 3) {
                    // read "m" value from buffer
                    ncnf = dataBuffer.getByte(dataIndex);
                    dataIndex++;
                }
            }

            // If 64-bit
            if (kcnf == 8 || kcnf == 9 || kcnf == 10) {
                int count=1, itemsOnLine=2;

                ss << std::endl;

                int b64EndIndex = dataIndex + 8*ncnf;
                if (b64EndIndex > endIndex) b64EndIndex = endIndex;

                while (dataIndex < b64EndIndex) {
                    // double
                    if (kcnf == 8) {
                        double d = dataBuffer.getDouble(dataIndex);
                        ss << scientific << std::setprecision(std::numeric_limits<double>::digits10 + 1) << d;
                    }
                    // 64 bit int
                    else if (kcnf == 9) {
                        int64_t lng = dataBuffer.getLong(dataIndex);
                        if (hex) {
                            ss << hex << std::setprecision(std::numeric_limits<int64_t>::digits10 + 1) << lng;
                        }
                        else {
                            ss << dec << std::setprecision(std::numeric_limits<int64_t>::digits10 + 1) << lng;
                        }
                    }
                    // 64 bit uint
                    else {
                        uint64_t lng = dataBuffer.getULong(dataIndex);
                        if (hex) {
                            ss << hex << std::setprecision(std::numeric_limits<int64_t>::digits10 + 1) << lng;
                        }
                        else {
                            ss << dec << std::setprecision(std::numeric_limits<int64_t>::digits10 + 1) << lng;
                        }
                    }

                    if (count++ % itemsOnLine == 0) {
                        ss << std::endl;
                    }
                    dataIndex += 8;
                }

                if ((count - 1) % itemsOnLine != 0) {
                    ss << std::endl;
                }
            }
            // 32-bit
            else if (kcnf == 1 || kcnf == 2 || kcnf == 11 || kcnf == 12) {
                int count=1, itemsOnLine=4;

                ss << std::endl;

                int b32EndIndex = dataIndex + 4*ncnf;
                if (b32EndIndex > endIndex) b32EndIndex = endIndex;

                while (dataIndex < b32EndIndex) {

                    // Hollerit, 32 bit int
                    if (kcnf == 12 || kcnf == 11) {
                        int32_t i = dataBuffer.getInt(dataIndex);
                        if (hex) {
                            ss << hex << std::setprecision(std::numeric_limits<int32_t>::digits10 + 1) << i;
                        }
                        else {
                            ss << dec << std::setprecision(std::numeric_limits<int32_t>::digits10 + 1) << i;
                        }
                    }
                    // float
                    else if (kcnf == 2) {
                        float f = dataBuffer.getFloat(dataIndex);
                        ss << scientific << std::setprecision(std::numeric_limits<float>::digits10 + 1) << f;
                    }
                    // 32 bit uint
                    else {
                        int32_t i = dataBuffer.getUInt(dataIndex);
                        if (hex) {
                            ss << hex << std::setprecision(std::numeric_limits<uint32_t>::digits10 + 1) << i;
                        }
                        else {
                            ss << dec << std::setprecision(std::numeric_limits<uint32_t>::digits10 + 1) << i;
                        }
                    }

                    if (count++ % itemsOnLine == 0) {
                        ss << std::endl;
                    }
                    dataIndex += 4;
                }

                if ((count - 1) % itemsOnLine != 0) {
                    ss << std::endl;
                }
            }
            // 16 bits
            else if (kcnf == 4 || kcnf == 5) {
                int count=1, itemsOnLine=6;

                ss << std::endl;

                int b16EndIndex = dataIndex + 2*ncnf;
                if (b16EndIndex > endIndex) b16EndIndex = endIndex;

                // swap all 16 bit items
                while (dataIndex < b16EndIndex) {

                    // 16 bit int
                    if (kcnf == 4) {
                        int16_t s = dataBuffer.getShort(dataIndex);
                        if (hex) {
                            ss << hex << std::setprecision(std::numeric_limits<int16_t>::digits10 + 1) << s;
                        }
                        else {
                            ss << dec << std::setprecision(std::numeric_limits<int16_t>::digits10 + 1) << s;
                        }
                    }
                    // 16 bit uint
                    else {
                        uint16_t s = dataBuffer.getUShort(dataIndex);
                        if (hex) {
                            ss << hex << std::setprecision(std::numeric_limits<uint16_t>::digits10 + 1) << s;
                        }
                        else {
                            ss << dec << std::setprecision(std::numeric_limits<uint16_t>::digits10 + 1) << s;
                        }
                    }

                    if (count++ % itemsOnLine == 0) {
                        ss << std::endl;
                    }
                    dataIndex += 2;
                }

                if ((count - 1) % itemsOnLine != 0) {
                    ss << std::endl;
                }
            }
            // 8 bits
            else if (kcnf == 6 || kcnf == 7 || kcnf == 3) {
                dataBuffer.position(dataIndex);

                std::vector<uint8_t> bytes(ncnf);
                // relative read
                dataBuffer.getBytes(bytes, 0, ncnf);


                // string array
                if (kcnf == 3) {
                    ss << std::endl;

                    std::vector<string> strs;
                    Util::unpackRawBytesToStrings(bytes, 0, ncnf, strs);
                    for (string const & s: strs) {
                        ss << s << std::endl;
                    }
                }
                // char
                else if (kcnf == 6) {
                    int count=1, itemsOnLine=8;
                    ss << std::endl;

                    for (int i=0; i < ncnf; i++) {
                        if (hex) {
                            ss << hex << std::setprecision(2) << (int8_t)(bytes[i]);
                        }
                        else {
                            ss << dec << std::setprecision(4) << (int8_t)(bytes[i]);
                        }

                        if (count++ % itemsOnLine == 0) {
                            ss << std::endl;
                        }
                    }

                    if ((count - 1) % itemsOnLine != 0) {
                        ss << std::endl;
                    }
                }
                // uchar
                else {
                    int count=1, itemsOnLine=8;
                    ss << std::endl;

                    for (int i=0; i < ncnf; i++) {
                        if (hex) {
                            ss << hex << std::setprecision(2) << bytes[i];
                        }
                        else {
                            ss << dec << std::setprecision(4) << bytes[i];
                        }

                        if (count++ % itemsOnLine == 0) {
                            ss << std::endl;
                        }
                    }

                    if ((count - 1) % itemsOnLine != 0) {
                        ss << std::endl;
                    }
                }

                // reset position
                dataBuffer.position(0);

                dataIndex += ncnf;
            }

        } //while

        ss << std::endl;
        return ss.str();
    }



}