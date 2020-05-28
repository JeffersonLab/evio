//
// Copyright (c) 2020, Jefferson Science Associates
//
// Thomas Jefferson National Accelerator Facility
// EPSCI Group
//
// 12000, Jefferson Ave, Newport News, VA 23606
// Phone : (757)-269-7100
//

#ifndef EVIO_6_0_EVENTHEADERPARSER_H
#define EVIO_6_0_EVENTHEADERPARSER_H


#include <cstring>
#include <memory>
#include <cstring>


#include "ByteOrder.h"
#include "BankHeader.h"
#include "SegmentHeader.h"
#include "TagSegmentHeader.h"


namespace evio {


    /**
     * This code is in the EventParser class in the original Java, but must be moved in C++
     * to avoid a circular reference to BaseStructure.
     *
     * @author heddle (original java in EventParser class)
     * @author timmer
     * @date 5/27/2020
     */
    class EventHeaderParser {

    public: /* package */


        /**
         * Create a bank header from the first eight bytes of the data array.
         *
         * @param bytes the byte array, probably from a bank that encloses this new bank.
         * @param byteOrder byte order of array, {@link ByteOrder#BIG_ENDIAN} or {@link ByteOrder#LITTLE_ENDIAN}
         *
         * @throws EvioException if data not in evio format.
         * @return the new bank header.
         */
        static std::shared_ptr<BankHeader> createBankHeader(uint8_t * bytes, ByteOrder const & byteOrder) {

            std::shared_ptr<BankHeader> header = std::make_shared<BankHeader>();

            // Does the length make sense?
            uint32_t len = 0;
            Util::toIntArray(reinterpret_cast<char *>(bytes), 4, byteOrder, &len);

            header->setLength(len);
            bytes += 4;

            // Read and parse second header word
            uint32_t word = 0;
            Util::toIntArray(reinterpret_cast<char *>(bytes), 4, byteOrder, &word);

            header->setTag(word >> 16);
            int dt = (word >> 8) & 0xff;
            int type = dt & 0x3f;
            uint8_t padding = dt >> 6;
            header->setDataType(type);
            header->setPadding(padding);
            header->setNumber(word);

            return header;
        }


        /**
         * Create a segment header from the first four bytes of the data array.
         *
         * @param bytes the byte array, probably from a bank that encloses this new segment.
         * @param byteOrder byte order of array, {@link ByteOrder#BIG_ENDIAN} or {@link ByteOrder#LITTLE_ENDIAN}
         *
         * @throws EvioException if data not in evio format.
         * @return the new segment header.
         */
        static std::shared_ptr<SegmentHeader> createSegmentHeader(uint8_t * bytes, ByteOrder const & byteOrder) {

            std::shared_ptr<SegmentHeader> header = std::make_shared<SegmentHeader>();

            // Read and parse header word
            uint32_t word = 0;
            Util::toIntArray(reinterpret_cast<char *>(bytes), 4, byteOrder, &word);

            uint32_t len = word & 0xffff;
            header->setLength(len);

            int dt = (word >> 16) & 0xff;
            int type = dt & 0x3f;
            int padding = dt >> 6;
            header->setDataType(type);
            header->setPadding(padding);
            header->setTag(word >> 24);

            return header;
        }


        /**
         * Create a tag segment header from the first four bytes of the data array.
         *
         * @param bytes the byte array, probably from a bank that encloses this new tag segment.
         * @param byteOrder byte order of array, {@link ByteOrder#BIG_ENDIAN} or {@link ByteOrder#LITTLE_ENDIAN}
         *
         * @throws EvioException if data not in evio format.
         * @return the new tagsegment header.
         */
        static std::shared_ptr<TagSegmentHeader> createTagSegmentHeader(uint8_t * bytes, ByteOrder const & byteOrder) {

            std::shared_ptr<TagSegmentHeader> header = std::make_shared<TagSegmentHeader>();

            // Read and parse header word
            uint32_t word = 0;
            Util::toIntArray(reinterpret_cast<char *>(bytes), 4, byteOrder, &word);

            uint32_t len = word & 0xffff;
            header->setLength(len);
            header->setDataType((word >> 16) & 0xf);
            header->setTag(word >> 20);

            return header;
        }

    };


}

#endif //EVIO_6_0_EVENTHEADERPARSER_H
