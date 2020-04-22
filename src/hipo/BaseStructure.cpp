//
// Created by Carl Timmer on 2020-04-02.
//

#include "BaseStructure.h"
#include "StructureType.h"


namespace evio {

    /** Bytes with which to pad short and byte data. */
    uint8_t padValues[4] = {0, 0, 0};

    /** Number of bytes to pad short and byte data. */
    uint32_t padCount[4] = {0, 3, 2, 1};

    /**
     * Constructor using a provided header
     *k
     * @param header the header to use.
     * @see BaseStructureHeader
     */
    BaseStructure::BaseStructure(BaseStructureHeader & hdr, ByteOrder order) :
                                    header(hdr), byteOrder(order) {
    }

    /**
     * This method does a partial copy and is designed to help convert
     * between banks, segments,and tagsegments in the {@link StructureTransformer}
     * class (hence the name "transform").
     * It copies all the data from another BaseStructure object.
     * Children are <b>not</b> copied in the deep clone way,
     * but their references are added to this structure.
     * It does <b>not</b> copy header data or the parent either.
     *
     * @param structure BaseStructure from which to copy data.
     */
    void BaseStructure::transform(BaseStructure & structure) {

        //if (structure == nullptr) return;

        // reinitialize this base structure first
        rawBytes.clear();
        stringList.clear();
        children.clear();
        stringEnd = 0;

        // copy over some stuff from other structure
        isLeaf = structure.isLeaf;
        lengthsUpToDate = structure.lengthsUpToDate;
        byteOrder = structure.byteOrder;
        numberDataItems = structure.numberDataItems;

        size_t rawSize = structure.rawBytes.size();
        if (rawSize > 0) {
            rawBytes.reserve(rawSize);
            std::memcpy(rawBytes.data(), structure.rawBytes.data(), rawSize);
        }

        DataType type = structure.getHeader().getDataType();

        charData      = reinterpret_cast<char *>(rawBytes.data());
        intData       = reinterpret_cast<int32_t *>(rawBytes.data());
        longData      = reinterpret_cast<int64_t *>(rawBytes.data());
        shortData     = reinterpret_cast<int16_t *>(rawBytes.data());
        doubleData    = reinterpret_cast<double *>(rawBytes.data());
        floatData     = reinterpret_cast<float *>(rawBytes.data());
        compositeData = reinterpret_cast<CompositeData *>(rawBytes.data());

        if (type == DataType::CHARSTAR8) {
                if (structure.stringsList.size() > 0) {
                    stringList.reserve(structure.stringsList.size());
                    stringList.addAll(structure.stringsList);
                    //stringData = new StringBuilder(structure.stringData);
                    stringEnd = structure.stringEnd;
                }
        }
        else if (type == DataType::COMPOSITE) {
            if (structure.compositeData != nullptr) {
                int len = structure.compositeData.length;
                compositeData = new CompositeData[len];
                for (int i = 0; i < len; i++) {
                    compositeData[i] = (CompositeData) structure.compositeData[i].clone();
                }
            }
        }
        else if (type.isStructure()) {
                for (BaseStructure kid : structure.children) {
                    children.add(kid);
                }
        }
    }


///**
// * Clone this object. First call the Object class's clone() method
// * which creates a bitwise copy. Then clone all the mutable objects
// * so that this method does a safe (not deep) clone. This means all
// * children get cloned as well.
// */
//Object BaseStructure::clone() {
//    try {
//        // "bs" is a bitwise copy. Do a deep clone of all object references.
//        BaseStructure bs = (BaseStructure)super.clone();
//
//        // Clone the header
//        bs.header = (BaseStructureHeader) header.clone();
//
//        // Clone raw bytes
//        if (rawBytes != null) {
//            bs.rawBytes = rawBytes.clone();
//        }
//
//        // Clone data
//        switch (header.getDataType())  {
//            case SHORT16:
//            case USHORT16:
//                if (shortData != null) {
//                    bs.shortData = shortData.clone();
//                }
//                break;
//
//            case INT32:
//            case UINT32:
//                if (intData != null) {
//                    bs.intData = intData.clone();
//                }
//                break;
//
//            case LONG64:
//            case ULONG64:
//                if (longData != null) {
//                    bs.longData = longData.clone();
//                }
//                break;
//
//            case FLOAT32:
//                if (floatData != null) {
//                    bs.floatData = floatData.clone();
//                }
//                break;
//
//            case DOUBLE64:
//                if (doubleData != null) {
//                    bs.doubleData = doubleData.clone();
//                }
//                break;
//
//            case UNKNOWN32:
//            case CHAR8:
//            case UCHAR8:
//                if (charData != null) {
//                    bs.charData = charData.clone();
//                }
//                break;
//
//            case CHARSTAR8:
//                if (stringsList != null) {
//                    bs.stringsList = new ArrayList<String>(stringsList.size());
//                    bs.stringsList.addAll(stringsList);
//                    bs.stringData = new StringBuilder(stringData);
//                    bs.stringEnd  = stringEnd;
//                }
//                break;
//
//            case COMPOSITE:
//                if (compositeData != null) {
//                    int len = compositeData.length;
//                    bs.compositeData = new CompositeData[len];
//                    for (int i=0; i < len; i++) {
//                        bs.compositeData[i] = (CompositeData) compositeData[i].clone();
//                    }
//                }
//                break;
//
//            case ALSOBANK:
//            case ALSOSEGMENT:
//            case BANK:
//            case SEGMENT:
//            case TAGSEGMENT:
//                // Create kid storage since we're a container type
//                bs.children = new ArrayList<BaseStructure>(10);
//
//                // Clone kids
//                for (BaseStructure kid : children) {
//                    bs.children.add((BaseStructure)kid.clone());
//                }
//                break;
//
//            default:
//        }
//
//        // Do NOT clone the parent, just keep it as a reference.
//
//        return bs;
//    }
//    catch (CloneNotSupportedException e) {
//        return null;
//    }
//}

    /** Clear all existing data from a non-container structure. */
    void BaseStructure::clearData() {

        if (header.getDataType().isStructure()) return;

        rawBytes.clear();
        shortData.clear();
        ushortData.clear();
        intData.clear();
        uintData.clear();
        longData.clear();
        ulongData.clear();
        doubleData.clear();
        floatData.clear();
        compositeData.clear();

        charData.clear();
        ucharData.clear();
//        stringData.clear();
        stringList.clear();
        stringEnd = 0;

        numberDataItems = 0;
    }


/**
 * What is the byte order of this data?
 * @return {@link ByteOrder#BIG_ENDIAN} or {@link ByteOrder#LITTLE_ENDIAN}
 */
ByteOrder BaseStructure::getByteOrder() {return byteOrder;}

/**
 * Set the byte order of this data. This method <b>cannot</b> be used to swap data.
 * It is only used to describe the endianness of the rawdata contained.
 * @param byteOrder {@link ByteOrder#BIG_ENDIAN} or {@link ByteOrder#LITTLE_ENDIAN}
 */
void BaseStructure::setByteOrder(ByteOrder & order) {byteOrder = order;}

/**
 * Is a byte swap required? This is java and therefore big endian. If data is
 * little endian, then a swap is required.
 *
 * @return <code>true</code> if byte swapping is required (data is little endian).
 */
bool BaseStructure::needSwap() {
    return byteOrder != ByteOrder::ENDIAN_LOCAL;
}

/**
 * Get the description from the name provider (dictionary), if there is one.
 *
 * @return the description from the name provider (dictionary), if there is one. If not, return
 *         NameProvider.NO_NAME_STRING.
 */
string BaseStructure::getDescription() {
    return NameProvider.getName(this);
}


/**
 * Obtain a string representation of the structure.
 * @return a string representation of the structure.
 */
string BaseStructure::toString() {

    stringstream ss;

    // show 0x for hex
    ss << showbase;

    StructureType stype = getStructureType();
    DataType dtype = header.getDataType();

    string description = getDescription();
    if (INameProvider.NO_NAME_STRING.equals(description)) {
        description = "";
    }

    string sb;
    sb.reserve(100);

    if (!description.empty()) {
        ss << "<html><b>" << description << "</b>";
    }
    else {
        ss << stype.toString() << " of " << dtype.toString() << "s:  tag=" << header.getTag();
        ss << hex << "(" << header.getTag() << ")" << dec;

        if (stype == StructureType::STRUCT_BANK) {
            ss << "  num=" << header.getNumber() << hex << "(" << header.getNumber() << ")" << dec;
        }
    }

    if (rawBytes.empty()) {
        ss << "  dataLen=" << ((header.getLength() - (header.getHeaderLength() - 1))/4);
    }
    else {
        ss << "  dataLen=" << (rawBytes.size()/4);
    }

    if (header.getPadding() != 0) {
        ss << "  pad=" << header.getPadding();
    }

    int numChildren = children.size();

    if (numChildren > 0) {
        ss << "  children=" << numChildren;
    }

    if (!description.empty()) {
        ss << "</html>";
    }

    return ss.str();
}

/**
 * This is a method from the IEvioStructure Interface. Return the header for this structure.
 *
 * @return the header for this structure.
 */
BaseStructureHeader BaseStructure::getHeader() {return header;}

/**
 * Get the number of stored data items like number of banks, ints, floats, etc.
 * (not the size in ints or bytes). Some items may be padded such as shorts
 * and bytes. This will tell the meaningful number of such data items.
 * In the case of containers, returns number of 32-bit words not in header.
 *
 * @return number of stored data items (not size or length),
 *         or number of bytes if container
 */
uint32_t BaseStructure::getNumberDataItems() {
    if (isContainer()) {
        numberDataItems = header.getLength() + 1 - header.getHeaderLength();
    }

    // if the calculation has not already been done ...
    if (numberDataItems < 1) {
        // When parsing a file or byte array, it is not fully unpacked until data
        // is asked for specifically, for example as an int array or a float array.
        // Thus we don't know how many of a certain item (say doubles) there is.
        // But we can figure that out now based on the size of the raw data byte array.
        int divisor = 0;
        int padding = 0;
        DataType type = header.getDataType();
        int numBytes = type.getBytes();

        switch (numBytes) {
            case 2:
                padding = header.getPadding();
                divisor = 2; break;
            case 4:
                divisor = 4; break;
            case 8:
                divisor = 8; break;
            default:
                padding = header.getPadding();
                divisor = 1; break;
        }

        // Special cases:
        if (type == DataType::CHARSTAR8) {
            auto sd = getStringData();
            numberDataItems = sd.size();
        }
        else if (type == DataType::COMPOSITE) {
            // For this type, numberDataItems is NOT used to
            // calculate the data length so we're OK returning
            // any reasonable value here.
            numberDataItems = 1;
            if (compositeData != nullptr) numberDataItems = compositeData.length;
        }

        if (divisor > 0 && !rawBytes.empty()) {
            numberDataItems = (rawBytes.size() - padding)/divisor;
        }
    }

    return numberDataItems;
}

    /**
     * Get the length of this structure in bytes, including the header.
     * @return the length of this structure in bytes, including the header.
     */
    uint32_t BaseStructure::getTotalBytes() {return 4*(header.getLength() + 1);}

    /**
     * Get the raw data of the structure.
     *
     * @return the raw data of the structure.
     */
    std::vector<uint8_t> & BaseStructure::getRawBytes() {return rawBytes;}

    /**
     * Set the data for the structure.
     *
     * @param bytes pointer to the data to be copied.
     * @param len number of bytes to be copied.
     */
    void BaseStructure::setRawBytes(uint8_t *bytes, uint32_t len) {
        std::memcpy(rawBytes.data(), bytes, len);
    }

    /**
     * This is a method from the IEvioStructure Interface. Gets the raw data as an int16_t vector
     * if the content type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateShortData} <b>MUST</b> be called!
     *
     * @return a reference to the data as an int16_t array.
     * @throws EvioException if contained data type is not int16_t.
     */
    std::vector<int16_t> & BaseStructure::getShortData() {
        // If we're asking for the data type actually contained ...
        if (header.getDataType() == DataType::SHORT16) {
            // If int data has not been transformed from the raw bytes yet ...
            if (shortData.empty() && (!rawBytes.empty())) {

                // Fill int vector with transformed raw data
                auto pInt = reinterpret_cast<int16_t *>(rawBytes.data());
                uint32_t numInts = (rawBytes.size() - header.getPadding()) / sizeof(int16_t);
                shortData.reserve(numInts);

                for (int i=0; i < numInts; ++i) {
                    int16_t dat = *(pInt++);
                    if (needSwap()) {
                        dat = SWAP_16(dat);
                    }
                    shortData[i] = dat;
                }
            }

            return shortData;
        }

        throw EvioException("wrong data type");
    }

    /**
     * This is a method from the IEvioStructure Interface. Gets the raw data as an uint16_t vector
     * if the content type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateUShortData} <b>MUST</b> be called!
     *
     * @return a reference to the data as an uint16_t array.
     * @throws EvioException if contained data type is not uint16_t.
     */
    std::vector<uint16_t> & BaseStructure::getUShortData() {
        if (header.getDataType() == DataType::USHORT16) {
            if (ushortData.empty() && (!rawBytes.empty())) {

                auto pInt = reinterpret_cast<uint16_t *>(rawBytes.data());
                uint32_t numInts = (rawBytes.size() - header.getPadding()) / sizeof(uint16_t);
                ushortData.reserve(numInts);

                for (int i=0; i < numInts; ++i) {
                    int16_t dat = *(pInt++);
                    if (needSwap()) {
                        dat = SWAP_16(dat);
                    }
                    ushortData[i] = dat;
                }
            }
            return ushortData;
        }
        throw EvioException("wrong data type");
    }


    /**
     * This is a method from the IEvioStructure Interface. Gets the raw data as an int32_t vector
     * if the content type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateIntData} <b>MUST</b> be called!
     *
     * @return a reference to the data as an int32_t array.
     * @throws EvioException if contained data type is not int32_t.
     */
    std::vector<int32_t> & BaseStructure::getIntData() {
        if (header.getDataType() == DataType::INT32) {
            if (intData.empty() && (!rawBytes.empty())) {

                auto pInt = reinterpret_cast<int32_t *>(rawBytes.data());
                uint32_t numInts = (rawBytes.size() - header.getPadding()) / sizeof(int32_t);
                intData.reserve(numInts);

                for (int i=0; i < numInts; ++i) {
                    int32_t dat = *(pInt++);
                    if (needSwap()) {
                        dat = SWAP_32(dat);
                    }
                    intData[i] = dat;
                }
            }
            return intData;
        }
        throw EvioException("wrong data type");
    }

    /**
     * This is a method from the IEvioStructure Interface. Gets the raw data as an uint32_t vector
     * if the content type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateUIntData} <b>MUST</b> be called!
     *
     * @return a reference to the data as an uint32_t array.
     * @throws EvioException if contained data type is not uint32_t.
     */
    std::vector<uint32_t> & BaseStructure::getUIntData() {
        if (header.getDataType() == DataType::UINT32) {
            if (uintData.empty() && (!rawBytes.empty())) {

                auto pInt = reinterpret_cast<uint32_t *>(rawBytes.data());
                uint32_t numInts = (rawBytes.size() - header.getPadding()) / sizeof(uint32_t);
                uintData.reserve(numInts);

                for (int i=0; i < numInts; ++i) {
                    uint32_t dat = *(pInt++);
                    if (needSwap()) {
                        dat = SWAP_32(dat);
                    }
                    uintData[i] = dat;
                }
            }
            return uintData;
        }
        throw EvioException("wrong data type");
    }

    /**
     * This is a method from the IEvioStructure Interface. Gets the raw data as an int64_t vector
     * if the content type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateLongData} <b>MUST</b> be called!
     *
     * @return a reference to the data as an int64_t array.
     * @throws EvioException if contained data type is not int64_t.
     */
    std::vector<int64_t> & BaseStructure::getLongData() {
        if (header.getDataType() == DataType::LONG64) {
            if (longData.empty() && (!rawBytes.empty())) {

                auto pLong = reinterpret_cast<int64_t *>(rawBytes.data());
                uint32_t numLongs = (rawBytes.size() - header.getPadding()) / sizeof(int64_t);
                longData.reserve(numLongs);

                for (int i=0; i < numLongs; ++i) {
                    int64_t dat = *(pLong++);
                    if (needSwap()) {
                        dat = SWAP_64(dat);
                    }
                    longData[i] = dat;
                }
            }
            return longData;
        }
        throw EvioException("wrong data type");
    }


    /**
     * This is a method from the IEvioStructure Interface. Gets the raw data as an uint64_t vector
     * if the content type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateULongData} <b>MUST</b> be called!
     *
     * @return a reference to the data as an uint64_t array.
     * @throws EvioException if contained data type is not uint64_t.
     */
    std::vector<uint64_t> & BaseStructure::getULongData() {
        if (header.getDataType() == DataType::ULONG64) {
            if (ulongData.empty() && (!rawBytes.empty())) {

                auto pLong = reinterpret_cast<uint64_t *>(rawBytes.data());
                uint32_t numLongs = (rawBytes.size() - header.getPadding()) / sizeof(uint64_t);
                ulongData.reserve(numLongs);

                for (int i=0; i < numLongs; ++i) {
                    uint64_t dat = *(pLong++);
                    if (needSwap()) {
                        dat = SWAP_64(dat);
                    }
                    ulongData[i] = dat;
                }
            }
            return ulongData;
        }
        throw EvioException("wrong data type");
    }


    /**
     * This is a method from the IEvioStructure Interface. Gets the raw data as a float vector
     * if the content type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateFloatData} <b>MUST</b> be called!
     *
     * @return a reference to the data as a float array.
     * @throws EvioException if contained data type is not float.
     */
    std::vector<float> & BaseStructure::getFloatData() {
        if (header.getDataType() == DataType::FLOAT32) {
            if (floatData.empty() && (!rawBytes.empty())) {

                auto pFlt = reinterpret_cast<float *>(rawBytes.data());
                uint32_t numReals = (rawBytes.size() - header.getPadding()) / sizeof(float);
                floatData.reserve(numReals);

                for (int i=0; i < numReals; ++i) {
                    float dat = *(pFlt++);
                    if (needSwap()) {
                        dat = SWAP_32(dat);
                    }
                    floatData[i] = dat;
                }
            }
            return floatData;
        }
        throw EvioException("wrong data type");
    }

    /**
     * This is a method from the IEvioStructure Interface. Gets the raw data as a double vector
     * if the content type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateDoubleData} <b>MUST</b> be called!
     *
     * @return a reference to the data as a double array.
     * @throws EvioException if contained data type is not double.
     */
    std::vector<double> & BaseStructure::getDoubleData() {
        if (header.getDataType() == DataType::DOUBLE64) {
            if (doubleData.empty() && (!rawBytes.empty())) {

                auto pFlt = reinterpret_cast<float *>(rawBytes.data());
                uint32_t numReals = (rawBytes.size() - header.getPadding()) / sizeof(double);
                doubleData.reserve(numReals);

                for (int i=0; i < numReals; ++i) {
                    double dat = *(pFlt++);
                    if (needSwap()) {
                        dat = SWAP_64(dat);
                    }
                    doubleData[i] = dat;
                }
            }
            return doubleData;
        }
        throw EvioException("wrong data type");
    }


///**
// * This is a method from the IEvioStructure Interface. Gets the composite data as
// * an array of CompositeData objects, if the content type as indicated by the header
// * is appropriate.<p>
// *
// * @return the data as an array of CompositeData objects, or <code>null</code>
// *         if this makes no sense for the given content type.
// * @throws EvioException if the data is internally inconsistent
// */
//CompositeData[] getCompositeData() throws EvioException {
//
//        switch (header.getDataType()) {
//            case COMPOSITE:
//                if (compositeData == null) {
//                    if (rawBytes == null) {
//                        return null;
//                    }
//                    compositeData = CompositeData.parse(rawBytes, byteOrder);
//                }
//            return compositeData;
//            default:
//                return null;
//        }
//}

    /**
     * This is a method from the IEvioStructure Interface. Gets the raw data as an signed char array,
     * if the contents type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateCharData} <b>MUST</b> be called!
     *
     * @return the data as an byte array, or <code>null</code> if this makes no sense for the given contents type.
     */
    std::vector<signed char> & BaseStructure::getCharData() {
        if (header.getDataType() == DataType::CHAR8) {
            if (charData.empty() && (!rawBytes.empty())) {

                uint32_t numBytes = (rawBytes.size() - header.getPadding());
                charData.reserve(numBytes);

                std::memcpy(reinterpret_cast<void *>(charData.data()),
                            reinterpret_cast<void *>(rawBytes.data()), numBytes);
            }
            return charData;
        }
        throw EvioException("wrong data type");
    }

    /**
     * This is a method from the IEvioStructure Interface. Gets the raw data as an signed char array,
     * if the contents type as indicated by the header is appropriate.<p>
     * If the returned vector's data is modified, then {@link #updateCharData} <b>MUST</b> be called!
     *
     * @return the data as an byte array, or <code>null</code> if this makes no sense for the given contents type.
     */
    std::vector<unsigned char> & BaseStructure::getUCharData() {
        if (header.getDataType() == DataType::UCHAR8) {
            if (ucharData.empty() && (!rawBytes.empty())) {

                uint32_t numBytes = (rawBytes.size() - header.getPadding());
                ucharData.reserve(numBytes);

                std::memcpy(reinterpret_cast<void *>(ucharData.data()),
                            reinterpret_cast<void *>(rawBytes.data()), numBytes);
            }
            return ucharData;
        }
        throw EvioException("wrong data type");
    }


    /**
     * This is a method from the IEvioStructure Interface. Gets the raw data (ascii) as an
     * array of String objects, if the contents type as indicated by the header is appropriate.
     * For any other behavior, the user should retrieve the data as a byte array and
     * manipulate it in the exact manner desired. If there are non ascii or non-printing ascii
     * chars or the bytes or not in evio format, a single String containing everything is returned.<p>
     *
     * Originally, in evio versions 1, 2 and 3, only one string was stored. Recent changes allow
     * an array of strings to be stored and retrieved. The changes are backwards compatible.
     *
     * The following is true about the string raw data format:
     * <ul>
     * <li>All strings are immediately followed by an ending null (0).
     * <li>All string arrays are further padded/ended with at least one 0x4 valued ASCII
     *     char (up to 4 possible).
     * <li>The presence of 1 to 4 ending 4's distinguishes the recent string array version from
     *     the original, single string version.
     * <li>The original version string may be padded with anything after its ending null.
     * </ul>
     *
     * @return the data as an array of String objects if DataType is CHARSTAR8, or <code>null</code>
     *         if this makes no sense for the given type.
     *
     */
    std::vector<string> & BaseStructure::getStringData() {
        if (header.getDataType() == DataType::CHARSTAR8) {
            if (!stringList.empty()) {
                return stringList;
            }

            if (rawBytes.empty()) {
                stringList.clear();
                return stringList;
            }

            unpackRawBytesToStrings();
            return stringList;
        }
        throw EvioException("wrong data type");
    }


    /**
     * This method returns the number of bytes in a raw
     * evio format of the given string array, not including header.
     *
     * @param strings vector of strings to size
     * @return the number of bytes in a raw evio format of the given strings
     * @return 0 if vector empty.
     */
    uint32_t BaseStructure::stringsToRawSize(std::vector<string> const & strings) {

        if (strings.empty()) {
            return 0;
        }

        uint32_t dataLen = 0;
        for (string const & s : strings) {
            dataLen += s.length() + 1; // don't forget the null char after each string
        }

        // Add any necessary padding to 4 byte boundaries.
        // IMPORTANT: There must be at least one '\004'
        // character at the end. This distinguishes evio
        // string array version from earlier version.
        int pads[] = {4,3,2,1};
        dataLen += pads[dataLen%4];

        return dataLen;
    }


    /**
     * This method returns the number of bytes in a raw
     * evio format of the given string array, not including header.
     *
     * @param strings vector of strings to size
     * @return the number of bytes in a raw evio format of the given strings
     * @return 0 if vector empty.
     */
    uint32_t BaseStructure::stringToRawSize(const string & str) {

        if (str.empty()) {
            return 0;
        }

        uint32_t dataLen = str.length() + 1; // don't forget the null char after each string

        // Add any necessary padding to 4 byte boundaries.
        // IMPORTANT: There must be at least one '\004'
        // character at the end. This distinguishes evio
        // string array version from earlier version.
        int pads[] = {4,3,2,1};
        dataLen += pads[dataLen%4];

        return dataLen;
    }


    /**
     * This method transforms an array/vector of strings into raw evio format data,
     * not including header.
     *
     * @param strings vector of strings to transform.
     * @param bytes   vector of bytes to contain evio formatted strings.
     */
    void BaseStructure::stringsToRawBytes(std::vector<string> & strings,
                                          std::vector<uint8_t> & bytes) {

        if (strings.empty()) {
            bytes.clear();
            return;
        }

        // create some storage
        int dataLen = stringsToRawSize(strings);
        string strData;
        strData.reserve(dataLen);

        for (string const & s : strings) {
            // add string
            strData.append(s);
            // add ending null
            strData.append(1, '\000');
        }

        // Add any necessary padding to 4 byte boundaries.
        // IMPORTANT: There must be at least one '\004'
        // character at the end. This distinguishes evio
        // string array version from earlier version.
        int pads[] = {4,3,2,1};
        switch (pads[strData.length()%4]) {
            case 4:
                strData.append(4, '\004');
                break;
            case 3:
                strData.append(3, '\004');
                break;
            case 2:
                strData.append(2, '\004');
                break;
            case 1:
                strData.append(1, '\004');
        }

        // Transform to ASCII
        bytes.resize(dataLen);
        for (int i=0; i < strData.length(); i++) {
            bytes[i] = strData[i];
        }
    }

    /**
     * This method transforms the internal vector of strings into internal rawBytes vector
     * of evio format data not including header.
     *
     * @param strings vector of strings to transform.
     * @param bytes   vector of bytes to contain evio formatted strings.
     */
    void BaseStructure::stringsToRawBytes() {

        if (stringList.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
            header.setPadding(0);
            return;
        }

        // create some storage
        int dataLen = stringsToRawSize(stringList);
        string strData;
        strData.reserve(dataLen);

        for (string const & s : stringList) {
            // add string
            strData.append(s);
            // add ending null
            strData.append(1, '\000');
        }

        // Add any necessary padding to 4 byte boundaries.
        // IMPORTANT: There must be at least one '\004'
        // character at the end. This distinguishes evio
        // string array version from earlier version.
        int pads[] = {4,3,2,1};
        switch (pads[strData.length()%4]) {
            case 4:
                strData.append(4, '\004');
                break;
            case 3:
                strData.append(3, '\004');
                break;
            case 2:
                strData.append(2, '\004');
                break;
            case 1:
                strData.append(1, '\004');
        }

        // Transform to ASCII
        rawBytes.resize(dataLen);
        for (int i=0; i < strData.length(); i++) {
            rawBytes[i] = strData[i];
        }

        numberDataItems = stringList.size();
    }


    /**
     * This method extracts an array of strings from byte array of raw evio string data.
     *
     * @param bytes raw evio string data.
     * @param offset offset into raw data array.
     * @param strData vector in which to place extracted strings.
     */
    void BaseStructure::unpackRawBytesToStrings(std::vector<uint8_t> & bytes, size_t offset,
                                                std::vector<string> & strData) {
        int length = bytes.size() - offset;
        if (bytes.empty() || (length < 4)) return;

        string sData(reinterpret_cast<const char *>(bytes.data()) + offset, length);
        return stringBuilderToStrings(sData, false, strData);
    }


    /**
     * This method extracts an array of strings from byte array of raw evio string data.
     * Don't go beyond the specified max character limit and stop at the first
     * non-character value.
     *
     * @param bytes       raw evio string data
     * @param offset      offset into raw data array
     * @param maxLength   max length in bytes of valid data in bytes array
     * @param strData     vector in which to place extracted strings.
     */
    void BaseStructure::unpackRawBytesToStrings(std::vector<uint8_t> &  bytes,
                                                size_t offset, size_t maxLength,
                                                std::vector<string> & strData) {
        int length = bytes.size() - offset;
        if (bytes.empty() || (length < 4)) return;

        // Don't read read more than maxLength ASCII characters
        length = length > maxLength ? maxLength : length;

        string sData(reinterpret_cast<const char *>(bytes.data()) + offset, length);
        return stringBuilderToStrings(sData, true, strData);
    }


    /**
     * This method extracts an array of strings from buffer containing raw evio string data.
     *
     * @param buffer  buffer containing evio string data
     * @param pos     position of string data in buffer
     * @param length  length of string data in buffer in bytes
     * @param strData vector in which to place extracted strings.
     */
    void BaseStructure::unpackRawBytesToStrings(ByteBuffer & buffer,
                                                size_t pos, size_t length,
                                                std::vector<string> & strData) {

        if (length < 4) return;

        string sData(reinterpret_cast<const char *>(buffer.array() + buffer.arrayOffset()) + pos, length);
        return stringBuilderToStrings(sData, false, strData);
    }


    /**
     * This method extracts an array of strings from a string containing evio string data.
     * If non-printable chars are found (besides those used to terminate strings),
     * then 1 string with all characters will be returned. However, if the "onlyGoodChars"
     * flag is true, 1 string is returned in truncated form without
     * the bad characters at the end.<p>
     * The name of this method is taken from the java and has little to do with C++.
     * That's done for ease of code maintenance.
     *
     * @param strData        containing string data
     * @param onlyGoodChars  if true and non-printable chars found,
     *                       only 1 string with printable ASCII chars will be returned.
     * @param strData        vector in which to place extracted strings.
     * @return array of Strings or null if processing error
     */
    void BaseStructure::stringBuilderToStrings(std::string const & strData, bool onlyGoodChars,
                                               std::vector<std::string> & strings) {

        // Each string is terminated with a null (char val = 0)
        // and in addition, the end is padded by ASCII 4's (char val = 4).
        // However, in the legacy versions of evio, there is only one
        // null-terminated string and anything as padding. To accommodate legacy evio, if
        // there is not an ending ASCII value 4, anything past the first null is ignored.
        // After doing so, split at the nulls. Do not use the String
        // method "split" as any empty trailing strings are unfortunately discarded.

        char c;
        std::vector<int> nullIndexList(10);
        int nullCount = 0, goodChars = 0;
        bool badFormat = true;

        int length = strData.length();
        bool noEnding4 = false;
        if (strData[length - 1] != '\004') {
            noEnding4 = true;
        }

        for (int i=0; i < length; i++) {
            c = strData[i];

            // If char is a null
            if (c == 0) {
                nullCount++;
                nullIndexList.push_back(i);
                // If evio v2 or 3, only 1 null terminated string exists
                // and padding is just junk or nonexistent.
                if (noEnding4) {
                    badFormat = false;
                    break;
                }
            }
            // Look for any non-printing/control characters (not including null)
            // and end the string there. Allow tab & newline.
            else if ((c < 32 || c > 126) && c != 9 && c != 10) {
                if (nullCount < 1) {
                    badFormat = true;
                    // Getting garbage before first null.
                    break;
                }

                // Already have at least one null & therefore a String.
                // Now we have junk or non-printing ascii which is
                // possibly the ending 4.

                // If we have a 4, investigate further to see if format
                // is entirely valid.
                if (c == '\004') {
                    // How many more chars are there?
                    int charsLeft = length - (i+1);

                    // Should be no more than 3 additional 4's before the end
                    if (charsLeft > 3) {
                        badFormat = true;
                        break;
                    }
                    else {
                        // Check to see if remaining chars are all 4's. If not, bad.
                        for (int j=1; j <= charsLeft; j++) {
                            c = strData[i+j];
                            if (c != '\004') {
                                badFormat = true;
                                goto pastOuterLoop;
                            }
                        }
                        badFormat = false;
                        break;
                     }
                }
                else {
                    badFormat = true;
                    break;
                }
            }

            pastOuterLoop:

            // Number of good ASCII chars we have
            goodChars++;
        }

        strings.clear();

        if (badFormat) {
            if (onlyGoodChars) {
                // Return everything in one String WITHOUT garbage
                string goodStr(strData.data(), goodChars);
                strings.push_back(goodStr);
                return;
            }
            // Return everything in one String including possible garbage
            strings.push_back(strData);
            return;
        }

        // If here, raw bytes are in the proper format

        int firstIndex = 0;
        for (int nullIndex : nullIndexList) {
            string str(strData.data() + firstIndex, (nullIndex - firstIndex));
            strings.push_back(str);
            firstIndex = nullIndex + 1;
        }
    }



    /**
     * Extract string data from rawBytes array.
     * @return number of strings extracted from bytes
     */
    uint32_t BaseStructure::unpackRawBytesToStrings() {

        badStringFormat = true;

        if (rawBytes.size() < 4) {
            stringList.clear();
            return 0;
        }

        // Each string is terminated with a null (char val = 0)
        // and in addition, the end is padded by ASCII 4's (char val = 4).
        // However, in the legacy versions of evio, there is only one
        // null-terminated string and anything as padding. To accommodate legacy evio, if
        // there is not an ending ASCII value 4, anything past the first null is ignored.
        // After doing so, split at the nulls.

        char c;
        int nullCount = 0;
        std::vector<int> nullIndexList(10);

        int rawLength = rawBytes.size();
        bool noEnding4 = false;
        if (rawBytes[rawLength - 1] != 4) {
            noEnding4 = true;
        }

        for (int i=0; i < rawLength; i++) {
            c = rawBytes[i];

            // If char is a null
            if (c == 0) {
                nullCount++;
                nullIndexList.push_back(i);
                // If evio v2 or 3, only 1 null terminated string exists
                // and padding is just junk or nonexistent.
                if (noEnding4) {
                    badStringFormat = false;
                    break;
                }
            }

            // Look for any non-printing/control characters (not including null)
            // and end the string there. Allow tab and newline whitespace.
            else if ((c < 32 || c > 126) && c != 9 && c != 10) {
// cout << "unpackRawBytesToStrings: found non-printing c = 0x" << hex << ((int)c) << dec << " at i = " << i << endl;
                if (nullCount < 1) {
                    // Getting garbage before first null.
//cout << "BAD FORMAT 1: garbage char before null" << endl;
                    break;
                }

                // Already have at least one null & therefore a String.
                // Now we have junk or non-printing ascii which is
                // possibly the ending 4.

                // If we have a 4, investigate further to see if format
                // is entirely valid.
                if (c == '\004') {
                    // How many more chars are there?
                    int charsLeft = rawLength - (i+1);

                    // Should be no more than 3 additional 4's before the end
                    if (charsLeft > 3) {
    //System.out.println("BAD FORMAT 2: too many chars, " + charsLeft + ", after 4");
                        break;
                    }
                    else {
                        // Check to see if remaining chars are all 4's. If not, bad.
                        for (int j=1; j <= charsLeft; j++) {
                            c = rawBytes[i+j];
                            if (c != '\004') {
    //System.out.println("BAD FORMAT 3: padding chars are not all 4's");
                                goto pastOuterLoop;
                            }
                        }
                        badStringFormat = false;
                        break;
                    }
                }
                else {
    //System.out.println("BAD FORMAT 4: got bad char, ascii val = " + c);
                    break;
                }
            }
        }
        pastOuterLoop:


        // What if the raw bytes are all valid ascii with no null or other non-printing chars?
        // Then format is bad so return everything as one string.

        // If error, return everything in one String including possible garbage
        if (badStringFormat) {
    //cout << "unpackRawBytesToStrings: bad format, return all chars in 1 string" << endl;
            string everything(reinterpret_cast<char *>(rawBytes.data()), rawLength);
            stringList.push_back(everything);
            return 1;
        }

        // If here, raw bytes are in the proper format

    //cout << "  split into " << nullCount << " strings" << endl;
        int firstIndex=0;
        for (int nullIndex : nullIndexList) {
            string subString(reinterpret_cast<char *>(rawBytes.data()) + firstIndex, (nullIndex-firstIndex));
            stringList.push_back(subString);
    //cout << "    add " << subString << endl;
            firstIndex = nullIndex + 1;
        }

        // Set length of everything up to & including last null (not padding)
        stringEnd = firstIndex;
        //stringData.setLength(stringEnd);
    //cout << "    good string len = " << stringEnd << endl;
        return stringList.size();
    }


//TODO: Defined in TreeNode !!!!!! How do we handle this????

//    /**
//     * Get the parent of this structure. The outer event bank is the only structure with a <code>null</code>
//     * parent (the only orphan). All other structures have non-null parent giving the container in which they
//     * were embedded. Part of the <code>MutableTreeNode</code> interface.
//     *
//     * @return the parent of this structure.
//     */
//    BaseStructure & BaseStructure::getParent() {return parent;}
//
///**
// * Get an enumeration of all the children of this structure. If none, it returns a constant,
// * empty Enumeration. Part of the <code>MutableTreeNode</code> interface.
// */
//public Enumeration<?> children() {
//    if (children == null) {
//        return DefaultMutableTreeNode.EMPTY_ENUMERATION;
//    }
//    return Collections.enumeration(children);
//}
//
///**
// * Add a child at the given index.
// *
// * @param child the child to add.
// * @param index the target index. Part of the <code>MutableTreeNode</code> interface.
// */
//public void insert(MutableTreeNode child, int index) {
//    if (children == null) {
//        children = new ArrayList<BaseStructure>(10);
//    }
//    children.add(index, (BaseStructure) child);
//    child.setParent(this);
//    isLeaf = false;
//    lengthsUpToDate(false);
//}
//
///**
// * Convenience method to add a child at the end of the child list. In this application
// * where are not concerned with the ordering of the children.
// *
// * @param child the child to add. It will be added to the end of child list.
// */
//public void insert(MutableTreeNode child) {
//    if (children == null) {
//        children = new ArrayList<BaseStructure>(10);
//    }
//    //add to end
//    insert(child, children.size());
//}
//
///**
// * Removes the child at index from the receiver. Part of the <code>MutableTreeNode</code> interface.
// *
// * @param index the target index for removal.
// */
//public void remove(int index) {
//    if (children == null) return;
//    BaseStructure bs = children.remove(index);
//    bs.setParent(null);
//    if (children.size() < 1) isLeaf = true;
//    lengthsUpToDate(false);
//}
//
///**
// * Removes the child. Part of the <code>MutableTreeNode</code> interface.
// *
// * @param child the child node being removed.
// */
//public void remove(MutableTreeNode child) {
//    if (children == null || child == null) return;
//    children.remove(child);
//    child.setParent(null);
//    if (children.size() < 1) isLeaf = true;
//    lengthsUpToDate(false);
//}
//
///**
// * Remove this node from its parent. Part of the <code>MutableTreeNode</code> interface.
// */
//public void removeFromParent() {
//    if (parent != null) parent.remove(this);
//}
//
///**
// * Set the parent for this node. Part of the <code>MutableTreeNode</code> interface.
// */
//public void setParent(MutableTreeNode parent) {
//    this.parent = (BaseStructure) parent;
//}



    /**
     * Visit all the structures in this structure (including the structure itself --
     * which is considered its own descendant).
     * This is similar to listening to the event as it is being parsed,
     * but is done to a complete (already) parsed event.
     *
     * @param listener an listener to notify as each structure is visited.
     */
    public void vistAllStructures(IEvioListener listener) {
        visitAllDescendants(this, listener, null);
    }

    /**
     * Visit all the structures in this structure (including the structure itself --
     * which is considered its own descendant) in a depth first manner.
     *
     * @param listener an listener to notify as each structure is visited.
     * @param filter an optional filter that must "accept" structures before
     *               they are passed to the listener. If <code>null</code>, all
     *               structures are passed. In this way, specific types of
     *               structures can be captured.
     */
    public void vistAllStructures(IEvioListener listener, IEvioFilter filter) {
        visitAllDescendants(this, listener, filter);
    }

    /**
     * Visit all the descendants of a given structure
     * (which is considered a descendant of itself.)
     *
     * @param structure the starting structure.
     * @param listener an listener to notify as each structure is visited.
     * @param filter an optional filter that must "accept" structures before
     *               they are passed to the listener. If <code>null</code>, all
     *               structures are passed. In this way, specific types of
     *               structures can be captured.
     */
    private void visitAllDescendants(BaseStructure structure, IEvioListener listener, IEvioFilter filter) {
        if (listener != null) {
            bool accept = true;
            if (filter != null) {
                accept = filter.accept(structure.getStructureType(), structure);
            }

            if (accept) {
                listener.gotStructure(this, structure);
            }
        }

        if (!(structure.isLeaf())) {
            for (BaseStructure child : structure.getChildren()) {
                visitAllDescendants(child, listener, filter);
            }
        }
    }

    /**
     * Visit all the descendant structures, and collect those that pass a filter.
     * @param filter the filter that must be passed. If <code>null</code>,
     *               this will return all the structures.
     * @return a collection of all structures that are accepted by a filter.
     */
    public List<BaseStructure> getMatchingStructures(IEvioFilter filter) {
        final Vector<BaseStructure> structures = new Vector<BaseStructure>(25, 10);

        IEvioListener listener = new IEvioListener() {
            public void startEventParse(BaseStructure structure) { }

            public void endEventParse(BaseStructure structure) { }

            public void gotStructure(BaseStructure topStructure, IEvioStructure structure) {
                structures.add((BaseStructure)structure);
            }
        };

        vistAllStructures(listener, filter);

        if (structures.size() == 0) {
            return null;
        }
        return structures;
    }


//
///**
// * This method is not relevant for this implementation.
// * An empty implementation is provided to satisfy the interface.
// * Part of the <code>MutableTreeNode</code> interface.
// */
//public void setUserObject(Object arg0) {
//}
//
///**
// * Checks whether children are allowed. Structures of structures can have children.
// * Structures of primitive data can not. Thus is is entirely determined by the content type.
// * Part of the <code>MutableTreeNode</code> interface.
// *
// * @return <code>true</code> if this node does not hold primitive data,
// *         i.e., if it is a structure of structures (a container).
// */
//public bool getAllowsChildren() {
//    return header.getDataType().isStructure();
//}
//
///**
// * Obtain the child at the given index. Part of the <code>MutableTreeNode</code> interface.
// *
// * @param index the target index.
// * @return the child at the given index or null if none
// */
//public TreeNode getChildAt(int index) {
//    if (children == null) return null;
//
//    BaseStructure b = null;
//    try {
//        b = children.get(index);
//    }
//    catch (ArrayIndexOutOfBoundsException e) { }
//    return b;
//}
//
///**
// * Get the count of the number of children. Part of the <code>MutableTreeNode</code> interface.
// *
// * @return the number of children.
// */
//public int getChildCount() {
//    if (children == null) {
//        return 0;
//    }
//    return children.size();
//}
//
///**
// * Get the index of a node. Part of the <code>MutableTreeNode</code> interface.
// *
// * @return the index of the target node or -1 if no such node in tree
// */
//public int getIndex(TreeNode node) {
//    if (children == null || node == null) return -1;
//    return children.indexOf(node);
//}
//
///**
// * Checks whether this is a leaf. Leaves are structures with no children.
// * All structures that contain primitive data are leaf structures.
// * Part of the <code>MutableTreeNode</code> interface.<br>
// * Note: this means that an empty container, say a Bank of Segments that have no segments, is a leaf.
// *
// * @return <code>true</code> if this is a structure with a primitive data type, i.e., it is not a container
// *         structure that contains other structures.
// */
//public bool isLeaf() {
//    return isLeaf;
//}


    /**
     * Checks whether this structure is a container, i.e. a structure of structures.
     * @return <code>true</code> if this structure is a container. This is the same check as
     * {@link #getAllowsChildren}.
     */
    bool BaseStructure::isContainer() {return header.getDataType().isStructure();}


    /**
     * Compute the dataLength in 32-bit words. This is the amount of data needed
     * by a leaf of primitives. For non-leafs (structures of
     * structures) this returns 0. For data types smaller than an
     * int, e.g. a short, it computes assuming padding to an
     * integer number of ints. For example, if we are writing a byte
     * array of length 3 or 4, the it would return 1. If
     * the byte array is 5,6,7 or 8 it would return 2;
     *
     * @return the amount of 32-bit words (ints) needed by a leaf of primitives, else 0.
     */
    uint32_t BaseStructure::dataLength() {

        uint32_t datalen = 0;

        // only leafs write data
        if (isLeaf()) {

            DataType type = header.getDataType();
            int numBytes = type.getBytes();

            switch (numBytes) {
                case 8:
                    datalen = 2 * getNumberDataItems();
                    break;

                case 4:
                    datalen = getNumberDataItems();
                    break;

                case 2:
                    int items = getNumberDataItems();
                    if (items == 0) {
                        datalen = 0;
                        break;
                    }
                    datalen = 1 + (items - 1) / 2;
            }

            // Special cases:
            if (type == DataType::CHARSTAR8 || type == DataType::COMPOSITE) {
                if (!rawBytes.empty()) {
                    datalen = 1 + ((rawBytes.size() - 1) / 4);
                }
            }
            else if (type == DataType::CHAR8 || type == DataType::UCHAR8 || type == DataType::UNKNOWN32) {
                int items = getNumberDataItems();
                if (items == 0) {
                    datalen = 0;
                }
                else {
                    datalen = 1 + ((items - 1) / 4);
                }
            }
        } // isleaf

        return datalen;
    }


//
//
///**
//    * Get the children of this structure.
// * Use {@link #getChildrenList()} instead.
//    *
// * @deprecated child structures are no longer keep in a Vector
//    * @return the children of this structure.
//    */
//public Vector<BaseStructure> getChildren() {
//    if (children == null) return null;
//    return new Vector<BaseStructure>(children);
//}
//
///**
//    * Get the children of this structure.
//    *
//    * @return the children of this structure.
//    */
//public List<BaseStructure> getChildrenList() {return children;}
//




    /**
     * Get whether the lengths of all header fields for this structure
     * and all it descendants are up to date or not.
     *
     * @return whether the lengths of all header fields for this structure
     *         and all it descendants are up to date or not.
     */
    bool BaseStructure::getLengthsUpToDate() {return lengthsUpToDate;}

    /**
     * Set whether the lengths of all header fields for this structure
     * and all it descendants are up to date or not.
     *
     * @param lenUpToDate are the lengths of all header fields for this structure
     *                    and all it descendants up to date or not.
     */
    void BaseStructure::setLengthsUpToDate(bool lenUpToDate) {
        lengthsUpToDate = lenUpToDate;

        // propagate back up the tree if lengths have been changed
        if (!lenUpToDate) {
            if (parent != nullptr) parent.lengthsUpToDate(false);
        }
    }

    /**
     * Compute and set length of all header fields for this structure and all its descendants.
     * For writing events, this will be crucial for setting the values in the headers.
     *
     * @return the length that would go in the header field (for a leaf).
     * @throws EvioException if the length is too large (&gt; {@link Integer#MAX_VALUE}).
     */
    uint32_t BaseStructure::setAllHeaderLengths() {
            // if length info is current, don't bother to recalculate it
            if (lengthsUpToDate) {
                return header.getLength();
            }

            uint32_t datalen, len;

            if (isLeaf()) {
                // # of 32 bit ints for leaves, 0 for empty containers (also considered leaves)
                datalen = dataLength();
            }
            else {
                datalen = 0;

                for (auto child : children) {
                    len = child->setAllHeaderLengths();
                    // Add this check to make sure structure is not being overfilled
                    if (std::numeric_limits<uint32_t>::max() - datalen < len) {
                        throw EvioException("added data overflowed containing structure");
                    }
                    datalen += len + 1;  // + 1 for the header length word of each child
                }
            }

            len = header.getHeaderLength() - 1;  // - 1 for length header word
            if (std::numeric_limits<uint32_t>::max() - datalen < len) {
                throw EvioException("added data overflowed containing structure");
            }

            datalen += len;

            // set the datalen for the header
            header.setLength(datalen);
            setLengthsUpToDate(true);
            return datalen;
    }



    // TODO: Throw exception if byteBuffer is too small !!!
    /**
     * Write myself out a byte buffer with fastest algorithms I could find.
     *
     * @param byteBuffer the byteBuffer to write to.
     * @return the number of bytes written.
     */
    size_t BaseStructure::write(ByteBuffer & byteBuffer) {

        int startPos = byteBuffer.position();

        // write the header
        header.write(byteBuffer);

        int curPos = byteBuffer.position();

        if (isLeaf()) {

            DataType type = header.getDataType();

            // If we have raw bytes which do NOT need swapping, this is fastest ..
            if (!rawBytes.empty() && (byteOrder == byteBuffer.order())) {
                byteBuffer.put(rawBytes, 0, rawBytes.size());
            }
            else if (type == DataType::DOUBLE64) {
                // if data sent over wire or read from file ...
                if (!rawBytes.empty()) {
                    // and need swapping ...
                    ByteOrder::byteSwap64(rawBytes.data(), rawBytes.size()/8,
                                          byteBuffer.array() + byteBuffer.arrayOffset());
                    byteBuffer.position(curPos + rawBytes.size());
                }
                // else if user set data thru API (can't-rely-on / no rawBytes array) ...
                else {
                    ByteOrder::byteSwap64(doubleData.data(), doubleData.size(),
                                          byteBuffer.array() + byteBuffer.arrayOffset());
                    byteBuffer.position(curPos + 8 * doubleData.size());
                }
            }
            else if (type == DataType::FLOAT32) {
                if (!rawBytes.empty()) {
                    ByteOrder::byteSwap32(rawBytes.data(), rawBytes.size()/4,
                                          byteBuffer.array() + byteBuffer.arrayOffset());
                    byteBuffer.position(curPos + rawBytes.size());
                }
                else {
                    ByteOrder::byteSwap64(floatData.data(), floatData.size(),
                                          byteBuffer.array() + byteBuffer.arrayOffset());
                    byteBuffer.position(curPos + 4 * floatData.size());
                }
            }
            else if (type == DataType::LONG64 || type == DataType::ULONG64) {
                if (!rawBytes.empty()) {
                    ByteOrder::byteSwap64(rawBytes.data(), rawBytes.size()/8,
                                          byteBuffer.array() + byteBuffer.arrayOffset());
                    byteBuffer.position(curPos + rawBytes.size());
                }
                else {
                    ByteOrder::byteSwap64(longData.data(), longData.size(),
                                          byteBuffer.array() + byteBuffer.arrayOffset());
                    byteBuffer.position(curPos + 8 * longData.size());
                }
            }
            else if (type == DataType::INT32 || type == DataType::UINT32) {
                if (!rawBytes.empty()) {
                    ByteOrder::byteSwap32(rawBytes.data(), rawBytes.size()/4,
                                          byteBuffer.array() + byteBuffer.arrayOffset());
                    byteBuffer.position(curPos + rawBytes.size());
                }
                else {
                    ByteOrder::byteSwap32(intData.data(), intData.size(),
                                          byteBuffer.array() + byteBuffer.arrayOffset());
                    byteBuffer.position(curPos + 4 * intData.size());
                }
            }
            else if (type == DataType::SHORT16 || type == DataType::USHORT16) {
                if (!rawBytes.empty()) {
                    ByteOrder::byteSwap16(rawBytes.data(), rawBytes.size()/2,
                                          byteBuffer.array() + byteBuffer.arrayOffset());
                    byteBuffer.position(curPos + rawBytes.size());
                }
                else {
                    ByteOrder::byteSwap32(shortData.data(), shortData.size(),
                                          byteBuffer.array() + byteBuffer.arrayOffset());
                    byteBuffer.position(curPos + 2 * shortData.size());

                    // might have to pad to 4 byte boundary
                    if (shortData.size() % 2 > 0) {
                        byteBuffer.putShort((short) 0);
                    }
                }
            }
            else if (type == DataType::CHAR8 || type == DataType::UCHAR8 || type == DataType::UNKNOWN32) {
                if (!rawBytes.empty()) {
                    byteBuffer.put(rawBytes, 0, rawBytes.size());
                } else {
                    byteBuffer.put(reinterpret_cast<uint8_t*>(charData.data()), 0, charData.size());

                    // might have to pad to 4 byte boundary
                    byteBuffer.put(padValues, 0, padCount[charData.size() % 4]);
                }
            }
            else if (type == DataType::CHARSTAR8) {
                // rawbytes contains ascii, already padded
                if (!rawBytes.empty()) {
                    byteBuffer.put(rawBytes, 0, rawBytes.size());
                }
            }
            else if (type == DataType::COMPOSITE) {
                // compositeData object always has rawBytes defined
                if (!rawBytes.empty()) {
                    // swap rawBytes
                    byteBuffer.put(rawBytes, 0, rawBytes.size());

                    uint8_t swappedRaw[rawBytes.size()];

                    try {
                        CompositeData.swapAll(rawBytes, 0, swappedRaw, 0,
                                              rawBytes.size() / 4, byteOrder);
                    }
                    catch (EvioException & e) { /* never happen */ }

                    // write them to buffer
                    byteBuffer.put(swappedRaw, 0, swappedRaw.length);
                }
            }
        } // isLeaf
        else if (!children.empty()) {
            for (BaseStructure child : children) {
                child.write(byteBuffer);
            }
        } // not leaf

        return byteBuffer.position() - startPos;
    }


//----------------------------------------------------------------------
// Methods to append to exising data if any or to set the data if none.
//----------------------------------------------------------------------


    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getIntData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateIntData() {

        // Make sure the structure is set to hold this kind of data
        DataType dataType = header.getDataType();
        if (dataType != DataType::INT32) {
            throw EvioException("cannot update int data when type = " + dataType.toString());
        }

        // if data was cleared ...
        if (intData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
        }
            // make rawBytes consistent with what's in the int vector
        else {
            numberDataItems = intData.size();
            rawBytes.resize(4 * numberDataItems);

            if (ByteOrder::needToSwap(byteOrder)) {
                ByteOrder::byteSwap32(intData.data(), numberDataItems, rawBytes.data());
            }
            else {
                std::memcpy(rawBytes.data(), intData.data(), 4 * numberDataItems);
            }
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }

    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getUIntData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateUIntData() {

        DataType dataType = header.getDataType();
        if (dataType != DataType::UINT32) {
            throw EvioException("cannot update uint data when type = " + dataType.toString());
        }

        if (uintData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
        }
        else {
            numberDataItems = uintData.size();
            rawBytes.resize(4 * numberDataItems);

            if (ByteOrder::needToSwap(byteOrder)) {
                ByteOrder::byteSwap32(uintData.data(), numberDataItems, rawBytes.data());
            }
            else {
                std::memcpy(rawBytes.data(), uintData.data(), 4 * numberDataItems);
            }
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }

    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getShortData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateShortData() {

        DataType dataType = header.getDataType();
        if (dataType != DataType::SHORT16) {
            throw EvioException("cannot update short data when type = " + dataType.toString());
        }

        uint32_t pad = 0;

        if (shortData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
            header.setPadding(0);
        }
        else {
            numberDataItems = shortData.size();
            size_t itemBytes = 2 * numberDataItems;

            // If odd # of shorts, there are 2 bytes of padding.
            if (numberDataItems % 2 != 0) {
                pad = 2;
                rawBytes.resize(itemBytes + pad);
                // set padding bytes to val = 0
                rawBytes[itemBytes]   = 0;
                rawBytes[itemBytes+1] = 0;
            }
            else {
                rawBytes.resize(itemBytes);
            }
            header.setPadding(pad);

            if (ByteOrder::needToSwap(byteOrder)) {
                ByteOrder::byteSwap16(shortData.data(), numberDataItems, rawBytes.data());
            }
            else {
                std::memcpy(rawBytes.data(), shortData.data(), itemBytes);
            }
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }

    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getUShortData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateUShortData() {

        DataType dataType = header.getDataType();
        if (dataType != DataType::USHORT16) {
            throw EvioException("cannot update ushort data when type = " + dataType.toString());
        }

        uint32_t pad = 0;

        if (ushortData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
            header.setPadding(0);
        }
        else {
            numberDataItems = ushortData.size();
            size_t itemBytes = 2 * numberDataItems;

            if (numberDataItems % 2 != 0) {
                pad = 2;
                rawBytes.resize(itemBytes + pad);
                rawBytes[itemBytes]   = 0;
                rawBytes[itemBytes+1] = 0;
            }
            else {
                rawBytes.resize(itemBytes);
            }
            header.setPadding(pad);

            if (ByteOrder::needToSwap(byteOrder)) {
                ByteOrder::byteSwap16(ushortData.data(), numberDataItems, rawBytes.data());
            }
            else {
                std::memcpy(rawBytes.data(), ushortData.data(), itemBytes);
            }
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }

    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getLongData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateLongData() {

        DataType dataType = header.getDataType();
        if (dataType != DataType::LONG64) {
            throw EvioException("cannot update long data when type = " + dataType.toString());
        }

        if (longData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
        }
        else {
            numberDataItems = longData.size();
            rawBytes.resize(8 * numberDataItems);

            if (ByteOrder::needToSwap(byteOrder)) {
                ByteOrder::byteSwap64(longData.data(), numberDataItems, rawBytes.data());
            }
            else {
                std::memcpy(rawBytes.data(), longData.data(), 8 * numberDataItems);
            }
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }

    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getULongData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateULongData() {

        DataType dataType = header.getDataType();
        if (dataType != DataType::ULONG64) {
            throw EvioException("cannot update ulong data when type = " + dataType.toString());
        }

        if (ulongData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
        }
        else {
            numberDataItems = ulongData.size();
            rawBytes.resize(8 * numberDataItems);

            if (ByteOrder::needToSwap(byteOrder)) {
                ByteOrder::byteSwap64(ulongData.data(), numberDataItems, rawBytes.data());
            }
            else {
                std::memcpy(rawBytes.data(), ulongData.data(), 8 * numberDataItems);
            }
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }


    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getCharData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateCharData() {

        DataType dataType = header.getDataType();
        if (dataType != DataType::CHAR8) {
            throw EvioException("cannot update char data when type = " + dataType.toString());
        }

        if (charData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
            header.setPadding(0);
        }
        else {
            numberDataItems = charData.size();

            // necessary padding to 4 byte boundaries.
            uint32_t pad = padCount[numberDataItems%4];
            header.setPadding(pad);

            rawBytes.resize((numberDataItems + pad));

            std::memcpy(rawBytes.data(), charData.data(), numberDataItems);
            std::memset(rawBytes.data()+numberDataItems, 0, pad);
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }

    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getUCharData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateUCharData() {

        DataType dataType = header.getDataType();
        if (dataType != DataType::UCHAR8) {
            throw EvioException("cannot update uchar data when type = " + dataType.toString());
        }

        if (ucharData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
            header.setPadding(0);
        }
        else {
            numberDataItems = ucharData.size();

            uint32_t pad = padCount[numberDataItems%4];
            header.setPadding(pad);

            rawBytes.resize((numberDataItems + pad));

            std::memcpy(rawBytes.data(), ucharData.data(), numberDataItems);
            std::memset(rawBytes.data()+numberDataItems, 0, pad);
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }


    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getFloatData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateFloatData() {

        DataType dataType = header.getDataType();
        if (dataType != DataType::FLOAT32) {
            throw EvioException("cannot update float data when type = " + dataType.toString());
        }

        if (floatData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
        }
        else {
            numberDataItems = floatData.size();
            rawBytes.resize(4 * numberDataItems);

            if (ByteOrder::needToSwap(byteOrder)) {
                ByteOrder::byteSwap32(floatData.data(), numberDataItems, rawBytes.data());
            }
            else {
                std::memcpy(rawBytes.data(), floatData.data(), 4 * numberDataItems);
            }
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }

    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getDoubleData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateDoubleData() {

        DataType dataType = header.getDataType();
        if (dataType != DataType::DOUBLE64) {
            throw EvioException("cannot update double data when type = " + dataType.toString());
        }

        if (doubleData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
        }
        else {
            numberDataItems = doubleData.size();
            rawBytes.resize(8 * numberDataItems);

            if (ByteOrder::needToSwap(byteOrder)) {
                ByteOrder::byteSwap64(doubleData.data(), numberDataItems, rawBytes.data());
            }
            else {
                std::memcpy(rawBytes.data(), doubleData.data(), 8 * numberDataItems);
            }
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }

    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getStringData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateStringData() {

        DataType dataType = header.getDataType();
        if (dataType != DataType::CHARSTAR8) {
            throw EvioException("cannot update string data when type = " + dataType.toString());
        }

        stringsToRawBytes();

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }


// TODO: this needs attention!!!
    /**
     * If data in this structure was changed by modifying the vector returned from
     * {@link #getCompositeData()}, then this method needs to be called in order to make this
     * object internally consistent.
     *
     * @throws EvioException if this object corresponds to a different data type.
     */
    void BaseStructure::updateCompositeData() {

        DataType dataType = header.getDataType();
        if (dataType != DataType::COMPOSITE) {
            throw EvioException("cannot update composite data when type = " + dataType.toString());
        }

        if (compositeData.empty()) {
            rawBytes.clear();
            numberDataItems = 0;
        }
        else {
            numberDataItems = compositeData.size();
            rawBytes.resize(8 * numberDataItems);

            if (ByteOrder::needToSwap(byteOrder)) {
                ByteOrder::byteSwap64(compositeData.data(), numberDataItems, rawBytes.data());
            }
            else {
                std::memcpy(rawBytes.data(), compositeData.data(), 8 * numberDataItems);
            }
        }

        setLengthsUpToDate(false);
        setAllHeaderLengths();
    }


    /**
     * Appends CompositeData objects to the structure. If the structure has no data, then this
     * is the same as setting the data.
     * @param data the CompositeData objects to append, or set if there is no existing data.
     * @throws EvioException if adding data to a structure of a different data type;
     *                       if data takes up too much memory to store in raw byte array (JVM limit)
     */
    public void appendCompositeData(CompositeData data[]) throws EvioException {

            DataType dataType = header.getDataType();
            if (dataType != DataType.COMPOSITE) {
                throw new EvioException("Tried to set composite data in a structure of type: " + dataType);
            }

            if (data == null || data.length < 1) {
                return;
            }

            // Composite data is always in the local (in this case, BIG) endian
            // because if generated in JVM that's true, and if read in, it is
            // swapped to local if necessary. Either case it's big endian.
            if (compositeData == null) {
                if (rawBytes == null) {
                    compositeData   = data;
                    numberDataItems = data.length;
                }
                else {
                    // Decode the raw data we have
                    CompositeData[] cdArray = CompositeData.parse(rawBytes, byteOrder);
                    if (cdArray == null) {
                        compositeData   = data;
                        numberDataItems = data.length;
                    }
                    else {
                        // Allocate array to hold everything
                        int len1 = cdArray.length, len2 = data.length;
                        int totalLen = len1 + len2;

                        if (Integer.MAX_VALUE - len1 < len2) {
                            throw new EvioException("added data overflowed containing structure");
                        }
                        compositeData = new CompositeData[totalLen];

                        // Fill with existing object first
                        for (int i = 0; i < len1; i++) {
                            compositeData[i] = cdArray[i];
                        }
                        // Append new objects
                        for (int i = len1; i < totalLen; i++) {
                            compositeData[i] = cdArray[i];
                        }
                        numberDataItems = totalLen;
                    }
                }
            }
            else {
                int len1 = compositeData.length, len2 = data.length;
                int totalLen = len1 + len2;

                if (Integer.MAX_VALUE - len1 < len2) {
                    throw new EvioException("added data overflowed containing structure");
                }

                CompositeData[] cdArray = compositeData;
                compositeData = new CompositeData[totalLen];

                // Fill with existing object first
                for (int i=0; i < len1; i++) {
                    compositeData[i] = cdArray[i];
                }
                // Append new objects
                for (int i=len1; i < totalLen; i++) {
                    compositeData[i] = cdArray[i];
                }
                numberDataItems = totalLen;
            }

            rawBytes  = CompositeData.generateRawBytes(compositeData);
//        int[] intA = ByteDataTransformer.getAsIntArray(rawBytes, ByteOrder.BIG_ENDIAN);
//        for (int i : intA) {
//            System.out.println("Ox" + Integer.toHexString(i));
//        }
            byteOrder = data[0].getByteOrder();

            lengthsUpToDate(false);
            setAllHeaderLengths();
    }



}