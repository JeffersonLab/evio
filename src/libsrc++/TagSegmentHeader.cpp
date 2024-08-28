//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "TagSegmentHeader.h"


namespace evio {

    /**
     * Constructor.
     * @param tag the tag for the tagsegment header.
     * @param dataType the data type for the content of the tagsegment.
     */
    TagSegmentHeader::TagSegmentHeader(uint16_t tag, DataType const & dataType) :
            BaseStructureHeader(tag, dataType) {}

    /**
     * Constructor for a string data type holding the single string given.
     * Used in CompositeData class.
     * @param tag the tag for the tagsegment header.
     * @param str  data type for the content of the tagsegment.
     */
    TagSegmentHeader::TagSegmentHeader(uint16_t tag, std::string const & str) :
            BaseStructureHeader(tag, DataType::CHARSTAR8) {

        // Find out how much space, in 32-bit words, the string will take:
        length = Util::stringToRawSize(str)/4;
    }


    /**
      * Get the length of the structure's data in 32 bit ints (not counting the header words).
      * @return Get the length of the structure's data in 32 bit ints (not counting the header words).
      */
    uint32_t TagSegmentHeader::getDataLength() {return length;}


    /**
      * Get the length of the structure's header in ints. This includes the first header word itself
      * (which contains the length) and in the case of banks, it also includes the second header word.
      * @return Get the length of the structure's header in ints.
      */
    uint32_t TagSegmentHeader::getHeaderLength() {return 1;}

    /**
     * Write myself out as evio format data
     * into the given byte array in the specified byte order.
     *
	 * @param dest array into which evio data is written (destination).
	 * @param order  byte order in which to write the data.
     * @return the number of bytes written, which for a TagSegmentHeader is 4.
     * @throws EvioException if destination array too small to hold data.
     */
    size_t TagSegmentHeader::write(uint8_t *dest, ByteOrder const & order) {
        auto compositeWord = (uint32_t) (tag << 20 | ((dataType.getValue() & 0xf)) << 16 | (length & 0xffff));
        Util::toBytes(compositeWord, order, dest);
        return 4;
    }


    /**
     * Write myself out a byte buffer.
     * This write is relative - i.e., it uses the current position of the buffer.
     *
     * @param byteBuffer the byteBuffer to write to.
     * @return the number of bytes written, which for a TagSegmentHeader is 4.
     */
    size_t TagSegmentHeader::write(std::shared_ptr<ByteBuffer> byteBuffer) {
        return write(*(byteBuffer.get()));
    }


    /**
     * Write myself out a byte buffer.
     * This write is relative - i.e., it uses the current position of the buffer.
     *
     * @param byteBuffer the byteBuffer to write to.
     * @return the number of bytes written, which for a TagSegmentHeader is 4.
     */
    size_t TagSegmentHeader::write(ByteBuffer & byteBuffer) {
        auto compositeWord = (uint32_t) (tag << 20 | ((dataType.getValue() & 0xf)) << 16 | (length & 0xffff));
        byteBuffer.putInt(compositeWord);
        return 4;
    }

    /**
     * Obtain a string representation of the tagsegment header.
     * @return a string representation of the tagsegment header.
     */
    std::string TagSegmentHeader::toString() {
        std::stringstream ss;

        ss << "tag-seg length: " << length << std::endl;
        ss << "     data type: " << getDataTypeName() << std::endl;
        ss << "           tag: " << tag << std::endl;

        return ss.str();
    }




}
