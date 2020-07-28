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
        if (order == ByteOrder::ENDIAN_BIG) {
            dest[0] = (uint8_t)tag;
            dest[1] = (uint8_t)((dataType.getValue() & 0x3f) | (padding << 6));
            Util::toBytes((uint16_t)length, order, dest+2);

        }
        else {
            Util::toBytes((uint16_t)length, order, dest);
            dest[2] = (uint8_t)((dataType.getValue() & 0x3f) | (padding << 6));
            dest[3] = (uint8_t)tag;
        }

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

        if (byteBuffer.order() == ByteOrder::ENDIAN_BIG) {
            byteBuffer.put((int8_t)tag);
            byteBuffer.put((int8_t)((dataType.getValue() & 0x3f) | (padding << 6)));
            byteBuffer.putShort((uint16_t)length);
        }
        else {
            byteBuffer.putShort((uint16_t)length);
            byteBuffer.put((int8_t)((dataType.getValue() & 0x3f) | (padding << 6)));
            byteBuffer.put((int8_t)tag);
        }
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
        ss << "       padding: " << padding << std::endl;

        return ss.str();
    }

}
