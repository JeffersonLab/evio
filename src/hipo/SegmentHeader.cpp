//
// Created by Carl Timmer on 2020-04-23.
//

#include "BankHeader.h"


namespace evio {


    /**
     * Constructor.
     * @param tag the tag for the bank header.
     * @param dataType the data type for the content of the bank.
     * @param num sometimes, but not necessarily, an ordinal enumeration.
     */
    BankHeader::BankHeader(uint32_t tag, DataType & dataType, uint32_t num) :
                    BaseStructureHeader(tag, dataType, num) {}


    /**
      * Get the length of the structure's header in ints. This includes the first header word itself
      * (which contains the length) and in the case of banks, it also includes the second header word.
      * @return Get the length of the structure's header in ints.
      */
    uint32_t BankHeader::getHeaderLength() {return 2;}


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
    void BankHeader::toArray(uint8_t *bArray, uint32_t offset,
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
    void BankHeader::toVector(std::vector<uint8_t> & bVec, uint32_t offset, ByteOrder & order) {

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
     * Write myself out a byte buffer.
     * This write is relative - i.e., it uses the current position of the buffer.
     *
     * @param byteBuffer the byteBuffer to write to.
     * @return the number of bytes written, which for a BankHeader is 8.
     */
    uint32_t BankHeader::write(ByteBuffer & byteBuffer) {
        byteBuffer.putInt(length);

        if (byteBuffer.order() == ByteOrder::ENDIAN_BIG) {
            byteBuffer.putShort((int16_t) tag);
            byteBuffer.put((int8_t)((dataType.getValue() & 0x3f) | (padding << 6)));
            byteBuffer.put((int8_t) number);
        }
        else {
            byteBuffer.put((int8_t) number);
            byteBuffer.put((int8_t)((dataType.getValue() & 0x3f) | (padding << 6)));
            byteBuffer.putShort((int16_t) tag);
        }

        return 8;
    }



    /**
     * Obtain a string representation of the bank header.
     * @return a string representation of the bank header.
     */
    string BankHeader::toString() {
        stringstream ss;

        ss << "bank length: " << length << endl;
        ss << "     number: " << number << endl;
        ss << "  data type: " << getDataTypeName() << endl;
        ss << "        tag: " << tag << endl;
        ss << "        tag: " << tag << endl;

        return ss.str();
    }




}
