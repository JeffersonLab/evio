//
// Copyright (c) 2020, Jefferson Science Associates
//
// Thomas Jefferson National Accelerator Facility
// EPSCI Group
//
// 12000, Jefferson Ave, Newport News, VA 23606
// Phone : (757)-269-7100
//

#include "SegmentHeader.h"

namespace evio {


    /**
     * Constructor.
     * @param tag the tag for the segment header.
     * @param dataType the data type for the content of the segment.
     * @param num sometimes, but not necessarily, an ordinal enumeration.
     */
    SegmentHeader::SegmentHeader(uint32_t tag, DataType & dataType) :
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
	 * @param offset offset into bArray at which to write.
	 * @param order  byte order in which to write the data.
     * @param destMaxSize max size in bytes of destination array.
     * @throws EvioException if destination array too small to hold data.
     */
    void SegmentHeader::toArray(uint8_t *bArray, uint32_t offset,
                                ByteOrder & order, uint32_t destMaxSize) {

        if (order == ByteOrder::ENDIAN_BIG) {
            bArray[offset]   = (uint8_t)tag;
            bArray[offset+1] = (uint8_t)((dataType.getValue() & 0x3f) | (padding << 6));
            Util::toBytes((uint16_t)length, order, bArray, offset+2, destMaxSize);

        }
        else {
            Util::toBytes((uint16_t)length, order, bArray, offset, destMaxSize);
            bArray[offset+2] = (uint8_t)((dataType.getValue() & 0x3f) | (padding << 6));
            bArray[offset+3] = (uint8_t)tag;
        }
    }

    /**
     * Write myself out as evio format data
     * into the given vector of bytes in the specified byte order.
     *
	 * @param bArray vector into which evio data is written (destination).
	 * @param offset offset into bVec at which to write.
	 * @param order  byte order in which to write the data.
     */
    void SegmentHeader::toVector(std::vector<uint8_t> & bVec, uint32_t offset, ByteOrder & order) {

        if (order == ByteOrder::ENDIAN_BIG) {
            bVec[offset]   = (uint8_t)tag;
            bVec[offset+1] = (uint8_t)((dataType.getValue() & 0x3f) | (padding << 6));
            Util::toBytes((uint16_t)length, order, bVec, offset+2);

        }
        else {
            Util::toBytes((uint16_t)length, order, bVec, offset);
            bVec[offset+2] = (uint8_t)((dataType.getValue() & 0x3f) | (padding << 6));
            bVec[offset+3] = (uint8_t)tag;
        }

    }

    /**
     * Write myself out a byte buffer.
     * This write is relative - i.e., it uses the current position of the buffer.
     *
     * @param byteBuffer the byteBuffer to write to.
     * @return the number of bytes written, which for a SegmentHeader is 4.
     */
    uint32_t SegmentHeader::write(ByteBuffer & byteBuffer) {

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
    string SegmentHeader::toString() {
        stringstream ss;

        ss << "segment length: " << length << endl;
        ss << "     data type: " << getDataTypeName() << endl;
        ss << "           tag: " << tag << endl;
        ss << "       padding: " << padding << endl;

        return ss.str();
    }




}
