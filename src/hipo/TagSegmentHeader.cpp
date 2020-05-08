//
// Copyright (c) 2020, Jefferson Science Associates
//
// Thomas Jefferson National Accelerator Facility
// EPSCI Group
//
// 12000, Jefferson Ave, Newport News, VA 23606
// Phone : (757)-269-7100
//

#include "TagSegmentHeader.h"

namespace evio {

    /**
     * Constructor.
     * @param tag the tag for the tagsegment header.
     * @param dataType the data type for the content of the tagsegment.
     * @param num sometimes, but not necessarily, an ordinal enumeration.
     */
    TagSegmentHeader::TagSegmentHeader(uint32_t tag, DataType & dataType) :
                    BaseStructureHeader(tag, dataType) {}


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
	 * @param bArray array into which evio data is written (destination).
	 * @param offset offset into bArray at which to write.
	 * @param order  byte order in which to write the data.
     * @param destMaxSize max size in bytes of destination array.
     * @return the number of bytes written, which for a BankHeader is 8.
     * @throws EvioException if destination array too small to hold data.
     */
    size_t TagSegmentHeader::write(uint8_t *dest, size_t destMaxSize, ByteOrder & order) {
        auto compositeWord = (uint16_t) ((tag << 4) | (dataType.getValue() & 0xf));

        if (order == ByteOrder::ENDIAN_BIG) {
            Util::toBytes(compositeWord, order, dest, destMaxSize);
            Util::toBytes((uint16_t)length, order, dest + 2, destMaxSize);
        }
        else {
            Util::toBytes((uint16_t)length, order, dest, destMaxSize);
            Util::toBytes(compositeWord, order, dest + 2, destMaxSize);
        }
        return 4;
    }

    /**
     * Write myself out a byte buffer.
     * This write is relative - i.e., it uses the current position of the buffer.
     *
     * @param byteBuffer the byteBuffer to write to.
     * @return the number of bytes written, which for a TagSegmentHeader is 4.
     */
    size_t TagSegmentHeader::write(ByteBuffer & byteBuffer) {

        auto compositeWord = (uint16_t) ((tag << 4) | (dataType.getValue() & 0xf));

        if (byteBuffer.order() == ByteOrder::ENDIAN_BIG) {
            byteBuffer.putShort(compositeWord);
            byteBuffer.putShort((uint16_t)length);
        }
        else {
            byteBuffer.putShort((uint16_t)length);
            byteBuffer.putShort(compositeWord);
        }
        return 4;
    }

    /**
     * Obtain a string representation of the tagsegment header.
     * @return a string representation of the tagsegment header.
     */
    string TagSegmentHeader::toString() {
        stringstream ss;

        ss << "tag-seg length: " << length << endl;
        ss << "     data type: " << getDataTypeName() << endl;
        ss << "           tag: " << tag << endl;

        return ss.str();
    }




}
