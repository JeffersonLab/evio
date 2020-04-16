//
// Created by Carl Timmer on 2020-04-02.
//

#include "BaseStructureHeader.h"

namespace evio {

    /**
     * Constructor
     * @param tag the tag for the header.
     * @param dataType the data type for the content of the structure.
     * @param num sometimes, but not necessarily, an ordinal enumeration.
     */
    BaseStructureHeader::BaseStructureHeader(uint32_t tag, DataType & dataType, uint32_t num) {
        this->tag = tag;
        this->dataType = dataType;
        setNumber(num);
    }

    /**
     * Get the number. Only Banks have a number field in their header, so this is only relevant for Banks.
     * @return the number.
     */
    uint32_t BaseStructureHeader::getNumber() {return number;}

    /**
     * Set the number. Only Banks have a number field in their header, so this is only relevant for Banks.
     * @param num the number.
     */
    void BaseStructureHeader::setNumber(uint32_t num) {number = num & 0xff;}

    /**
     * Get the data type for the structure.
     * @return the data type for the structure.
     */
    uint32_t BaseStructureHeader::getDataTypeValue() {return dataType.getValue();}

    /**
     * Set the numeric data type for the structure.
     * @param dataType the numeric data type for the structure.
     */
    void BaseStructureHeader::setDataType(uint32_t type) {dataType = DataType::getDataType(type);}

    /**
     * Set the numeric data type for the structure.
     * @param type the numeric data type for the structure.
     */
    void BaseStructureHeader::setDataType(DataType & type) {dataType = type;}

    /**
     * Returns the data type for data stored in this structure as a <code>DataType</code> enum.
     * @return the data type for data stored in this structure as a <code>DataType</code> enum.
     * @see DataType
     */
    DataType BaseStructureHeader::getDataType() {return dataType;}

    /**
     * Returns the data type as a string.
     * @return the data type as a string.
     */
    string BaseStructureHeader::getDataTypeName() {return dataType.getName();}

    /**
     * Get the amount of padding bytes when storing short or byte data.
     * Value is 0, 1, 2, or 3 for bytes and 0 or 2 for shorts.
     * @return number of padding bytes
     */
    uint32_t BaseStructureHeader::getPadding() {return padding;}

    /**
     * Set the amount of padding bytes when storing short or byte data.
     * Allowed value is 0, 1, 2, or 3 for bytes and 0 or 2 for shorts.
     * @param padding amount of padding bytes when storing short or byte data (0-3).
     */
    void BaseStructureHeader::setPadding(uint32_t pad) {padding = pad;}

    /**
     * Get the length of the structure in ints, not counting the length word.
     * @return Get the length of the structure in ints (not counting the length word).
     */
    uint32_t BaseStructureHeader::getLength() {return length;}

    /**
     * Set the length of the structure in ints, not counting the length word.
     * @param length the length of the structure in ints, not counting the length word.
     */
    void BaseStructureHeader::setLength(uint32_t len) {length = len;}

    /**
     * Get the length of the structure's header in ints. This includes the first header word itself
     * (which contains the length) and in the case of banks, it also includes the second header word.
     * This implementation is correct for banks but is overridden for segments and tagsegments.
     * @return Get the length of the structure's header in ints.
     */
    uint32_t BaseStructureHeader::getHeaderLength() {return 2;}

    /**
     * Write myself out as evio format data
     * into the given byte array in the specified byte order.
     * This implementation is correct for banks but is overridden for segments and tagsegments.
     *
	 * @param bArray array into which evio data is written (destination).
	 * @param offset offset into bArray at which to write.
	 * @param order  byte order in which to write the data.
     * @param destMaxSize max size in bytes of destination array.
     * @throws EvioException if destination array too small to hold data.
     */
    void BaseStructureHeader::toArray(uint8_t *bArray, uint32_t offset,
                                      ByteOrder & order, uint32_t destMaxSize) {
            // length first
            Util::toBytes(length, order, bArray, offset, destMaxSize);

            if (order == ByteOrder::ENDIAN_BIG) {
                Util::toBytes((uint16_t)tag, order, bArray, offset+4, destMaxSize);
                // lowest 6 bits are dataType, upper 2 bits are padding
                bArray[offset+6] = (uint8_t)((dataType.getValue() & 0x3f) | (padding << 6));
                bArray[offset+7] = (uint8_t)number;
            }
            else {
                bArray[offset+4] = (uint8_t)number;
                bArray[offset+5] = (uint8_t)((dataType.getValue() & 0x3f) | (padding << 6));
                Util::toBytes((uint16_t)tag, order, bArray, offset+6, destMaxSize);
            }
    }

    /**
     * Write myself out as evio format data
     * into the given vector of bytes in the specified byte order.
     * This implementation is correct for banks but is overridden for segments and tagsegments.
     *
	 * @param bArray vector into which evio data is written (destination).
	 * @param offset offset into bVec at which to write.
	 * @param order  byte order in which to write the data.
     */
    void BaseStructureHeader::toVector(std::vector<uint8_t> & bVec, uint32_t offset, ByteOrder & order) {

        Util::toBytes(length, order, bVec, offset);

        if (order == ByteOrder::ENDIAN_BIG) {
            Util::toBytes((uint16_t)tag, order, bVec, offset+4);
            bVec[offset+6] = (uint8_t)((dataType.getValue() & 0x3f) | (padding << 6));
            bVec[offset+7] = (uint8_t)number;
        }
        else {
            bVec[offset+4] = (uint8_t)number;
            bVec[offset+5] = (uint8_t)((dataType.getValue() & 0x3f) | (padding << 6));
            Util::toBytes((uint16_t)tag, order, bVec, offset+6);
        }
    }

    /**
     * Get the structure tag.
     * @return the structure tag.
     */
    uint32_t BaseStructureHeader::getTag() {return tag;}

    /**
     * Set the structure tag.
     * @param tag the structure tag.
     */
    void BaseStructureHeader::setTag(uint32_t tag) {this->tag = tag;}

    /**
     * Obtain a string representation of the structure header.
     * @return a string representation of the structure header.
     */
    string BaseStructureHeader::toString() {
        stringstream ss;

        ss << "structure length: " << length << endl;
        ss << "       data type: " << getDataTypeName() << endl;
        ss << "             tag: " << tag << endl;
        ss << "         padding: " << padding;

        return ss.str();
    }
}

