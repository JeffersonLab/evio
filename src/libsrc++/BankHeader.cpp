//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "BankHeader.h"


namespace evio {


    /**
     * Constructor.
     * @param tag the tag for the bank header.
     * @param dataType the data type for the content of the bank.
     * @param num sometimes, but not necessarily, an ordinal enumeration.
     */
    BankHeader::BankHeader(uint16_t tag, DataType const & dataType, uint8_t num) :
                    BaseStructureHeader(tag, dataType, num) {length = 1;}


    uint32_t BankHeader::getDataLength() {return length - 1;}

    uint32_t BankHeader::getHeaderLength() {return 2;}

    size_t BankHeader::write(uint8_t *dest, ByteOrder const & order) {
        // length first
        Util::toBytes(length, order, dest);
        auto word = (uint32_t) (tag << 16 | ((dataType.getValue() & 0x3f) | (padding << 6)) << 8 | number);
        Util::toBytes(word, order, dest+4);
        return 8;
    }

    size_t BankHeader::write(std::shared_ptr<ByteBuffer> byteBuffer) {
        return write(*(byteBuffer.get()));
    }

    size_t BankHeader::write(ByteBuffer & dest) {
        dest.putInt(length);

        auto word = (uint32_t) (tag << 16 | ((dataType.getValue() & 0x3f) | (padding << 6)) << 8 | number);
        dest.putInt(word);
        return 8;
    }


    /**
     * Obtain a string representation of the bank header.
     * @return a string representation of the bank header.
     */
    std::string BankHeader::toString() {
        std::stringstream ss;

        ss << "bank length: " << length << std::endl;
        ss << "     number: " << +number << std::endl;
        ss << "  data type: " << getDataTypeName() << std::endl;
        ss << "        tag: " << tag << std::endl;
        ss << "    padding: " << +padding << std::endl;

        return ss.str();
    }

}
