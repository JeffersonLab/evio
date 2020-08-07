//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "SegmentHeader.h"


namespace evio {


    /**
     * Constructor.
     * @param tag the tag for the segment header.
     * @param dataType the data type for the content of the segment.
     */
    SegmentHeader::SegmentHeader(uint16_t tag, DataType const & dataType) :
                    BaseStructureHeader(tag, dataType) {}


    /**
     * Get the length of the structure's data in 32 bit ints (not counting the header words).
     * @return Get the length of the structure's data in 32 bit ints (not counting the header words).
     */
    uint32_t SegmentHeader::getDataLength() {return length;}


    /**
      * Get the length of the structure's header in ints. This includes the first header word itself
      * (which contains the length) and in the case of banks, it also includes the second header word.
      * @return Get the length of the structure's header in ints.
      */
    uint32_t SegmentHeader::getHeaderLength() {return 1;}


    /**
     * Write myself out as evio format data
     * into the given byte array in the specified byte order.
     *
	 * @param bArray array into which evio data is written (destination).
	 * @param order  byte order in which to write the data.
     * @return the number of bytes written, which for a SegmentHeader is 4.
     * @throws EvioException if destination array too small to hold data.
     */
    size_t SegmentHeader::write(uint8_t *dest, ByteOrder const & order) {

        auto word = (uint32_t) (tag << 24 | (uint8_t)((dataType.getValue() & 0x3f) |
                                (padding << 6)) << 16 | (length & 0xffff));
        Util::toBytes(word, order, dest);
        return 4;
    }


    /**
     * Write myself out a byte buffer.
     * This write is relative - i.e., it uses the current position of the buffer.
     *
     * @param byteBuffer the byteBuffer to write to.
     * @return the number of bytes written, which for a SegmentHeader is 4.
     */
    size_t SegmentHeader::write(std::shared_ptr<ByteBuffer> & byteBuffer) {
        return write(*(byteBuffer.get()));
    }


    /**
     * Write myself out a byte buffer.
     * This write is relative - i.e., it uses the current position of the buffer.
     *
     * @param byteBuffer the byteBuffer to write to.
     * @return the number of bytes written, which for a SegmentHeader is 4.
     */
    size_t SegmentHeader::write(ByteBuffer & byteBuffer) {

        auto word = (uint32_t) (tag << 24 | (uint8_t)((dataType.getValue() & 0x3f) |
                                (padding << 6)) << 16 | (length & 0xffff));
        byteBuffer.putInt(word);
        return 4;
    }


    /**
     * Obtain a string representation of the segment header.
     * @return a string representation of the segment header.
     */
    std::string SegmentHeader::toString() {
        std::stringstream ss;

        ss << "segment length: " << length << std::endl;
        ss << "     data type: " << getDataTypeName() << std::endl;
        ss << "           tag: " << tag << std::endl;
        ss << "       padding: " << +padding << std::endl;

        return ss.str();
    }

}
