//
// Copyright (c) 2020, Jefferson Science Associates
//
// Thomas Jefferson National Accelerator Facility
// EPSCI Group
//
// 12000, Jefferson Ave, Newport News, VA 23606
// Phone : (757)-269-7100
//

#include "BaseStructureHeader.h"

namespace evio {

    /**
     * Constructor
     * @param tag the tag for the header.
     * @param dataType the data type for the content of the structure.
     * @param num sometimes, but not necessarily, an ordinal enumeration.
     */
    BaseStructureHeader::BaseStructureHeader(uint16_t tag, DataType const & dataType, uint8_t num) {
        this->tag = tag;
        this->dataType = dataType;
        setNumber(num);
    }

    /**
     * Get the number. Only Banks have a number field in their header, so this is only relevant for Banks.
     * @return the number.
     */
    uint8_t BaseStructureHeader::getNumber() const {return number;}

    /**
     * Set the number. Only Banks have a number field in their header, so this is only relevant for Banks.
     * @param num the number.
     */
    void BaseStructureHeader::setNumber(uint8_t num) {number = num;}

    /**
     * Get the data type for the structure.
     * @return the data type for the structure.
     */
    uint32_t BaseStructureHeader::getDataTypeValue() const {return dataType.getValue();}

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
    DataType BaseStructureHeader::getDataType() const {return dataType;}

    /**
     * Returns the data type as a string.
     * @return the data type as a string.
     */
    string BaseStructureHeader::getDataTypeName() const {return dataType.getName();}

    /**
     * Get the amount of padding bytes when storing short or byte data.
     * Value is 0, 1, 2, or 3 for bytes and 0 or 2 for shorts.
     * @return number of padding bytes
     */
    uint8_t BaseStructureHeader::getPadding() const {return padding;}

    /**
     * Set the amount of padding bytes when storing short or byte data.
     * Allowed value is 0, 1, 2, or 3 for bytes and 0 or 2 for shorts.
     * @param padding amount of padding bytes when storing short or byte data (0-3).
     */
    void BaseStructureHeader::setPadding(uint8_t pad) {padding = pad;}

    /**
     * Get the length of the structure in ints, not counting the length word.
     * @return Get the length of the structure in ints (not counting the length word).
     */
    uint32_t BaseStructureHeader::getLength() const {return length;}

    /**
     * Set the length of the structure in ints, not counting the length word.
     * @param length the length of the structure in ints, not counting the length word.
     */
    void BaseStructureHeader::setLength(uint32_t len) {length = len;}

    /**
     * Get the structure tag.
     * @return the structure tag.
     */
    uint16_t BaseStructureHeader::getTag() const {return tag;}

    /**
     * Set the structure tag.
     * @param t the structure tag.
     */
    void BaseStructureHeader::setTag(uint16_t t) {tag = t;}
}

