//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_EVENTHEADERPARSER_H
#define EVIO_6_0_EVENTHEADERPARSER_H


#include <cstring>
#include <memory>
#include <cstring>


#include "ByteOrder.h"
#include "BankHeader.h"
#include "SegmentHeader.h"
#include "TagSegmentHeader.h"
#include "EvioNode.h"


namespace evio {


    /**
     * The createXXX methods exist is in the EventParser class in the original Java, but must be moved in C++
     * to avoid a circular reference to BaseStructure. Also methods for swapping headers was moved
     * here from Java's ByteDataTransformer class. Although they would fit in the Util class, it
     * seems more appropriate to put them here.
     *
     * @author heddle (original java in EventParser  & ByteDataTransformer classes)
     * @author timmer
     * @date 5/27/2020
     */
    class EventHeaderParser {

    public:

        /**
         * Create a bank header from the first eight bytes of the data array.
         *
         * @param bytes the byte array, probably from a bank that encloses this new bank.
         * @param byteOrder byte order of array, {@link ByteOrder#ENDIAN_BIG} or {@link ByteOrder#ENDIAN_LITTLE}.
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
         * @param byteOrder byte order of array, {@link ByteOrder#ENDIAN_BIG} or {@link ByteOrder#ENDIAN_LITTLE}.
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
         * @param byteOrder byte order of array, {@link ByteOrder#ENDIAN_BIG} or {@link ByteOrder#ENDIAN_LITTLE}.
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


        /**
         * This method reads and swaps an evio bank header.
         * It can also return information about the bank.
         * Position and limit of neither buffer argument is changed.<p></p>
         * <b>This only swaps data if buffer arguments have opposite byte order!</b>
         *
         * @param node       object in which to store data about the bank
         *                   in destBuffer after swap.
         * @param srcBuffer  buffer containing bank header to be swapped.
         * @param destBuffer buffer in which to place swapped bank header.
         * @param srcPos     position in srcBuffer to start reading bank header.
         * @param destPos    position in destBuffer to start writing swapped bank header.
         *
         * @throws EvioException if srcBuffer data underflow;
         *                       if destBuffer is too small to contain swapped data;
         *                       srcBuffer and destBuffer have same byte order.
         */
        static void swapBankHeader(std::shared_ptr<EvioNode> & node,
                                   std::shared_ptr<ByteBuffer> & srcBuffer,
                                   std::shared_ptr<ByteBuffer> & destBuffer,
                                   uint32_t srcPos, uint32_t destPos) {
            swapBankHeader(*(node.get()), *(srcBuffer.get()), *(destBuffer.get()), srcPos, destPos);
        }


        /**
         * This method reads and swaps an evio bank header.
         * It can also return information about the bank.
         * Position and limit of neither buffer argument is changed.<p></p>
         * <b>This only swaps data if buffer arguments have opposite byte order!</b>
         *
         * @param node       object in which to store data about the bank
         *                   in destBuffer after swap.
         * @param srcBuffer  buffer containing bank header to be swapped.
         * @param destBuffer buffer in which to place swapped bank header.
         * @param srcPos     position in srcBuffer to start reading bank header.
         * @param destPos    position in destBuffer to start writing swapped bank header.
         *
         * @throws EvioException if srcBuffer data underflow;
         *                       if destBuffer is too small to contain swapped data;
         *                       srcBuffer and destBuffer have same byte order.
         */
        static void swapBankHeader(EvioNode & node, ByteBuffer & srcBuffer, ByteBuffer & destBuffer,
                                   uint32_t srcPos, uint32_t destPos) {

            // Check endianness
            if (srcBuffer.order() == destBuffer.order()) {
                throw evio::EvioException("src & dest buffers need different byte order for swapping");
            }

            // Read & swap first bank header word
            uint32_t length = srcBuffer.getInt(srcPos);
            destBuffer.putInt(destPos, length);
            srcPos  += 4;
            destPos += 4;

            // Read & swap second bank header word
            uint32_t word = srcBuffer.getInt(srcPos);
            destBuffer.putInt(destPos, word);

            node.tag      = (word >> 16) & 0xffff;
            uint32_t dt   = (word >> 8) & 0xff;

            node.dataType = dt & 0x3f;
            node.pad      = dt >> 6;
            node.num      = word & 0xff;
            node.len      = length;
            node.pos      = destPos - 4;
            node.dataPos  = destPos + 4;
            node.dataLen  = length - 1;
        }


        /**
         * This method reads and swaps an evio segment header.
         * It can also return information about the segment.
         * Position and limit of neither buffer argument is changed.<p></p>
         * <b>This only swaps data if buffer arguments have opposite byte order!</b>
         *
         * @param node       object in which to store data about the segment
         *                   in destBuffer after swap; may be null
         * @param srcBuffer  buffer containing segment header to be swapped
         * @param destBuffer buffer in which to place swapped segment header
         * @param srcPos     position in srcBuffer to start reading segment header
         * @param destPos    position in destBuffer to start writing swapped segment header
         *
         * @throws EvioException if srcBuffer data underflow;
         *                       if destBuffer is too small to contain swapped data;
         *                       srcBuffer and destBuffer have same byte order.
         */
        static void swapSegmentHeader(std::shared_ptr<EvioNode> & node,
                                      std::shared_ptr<ByteBuffer> & srcBuffer,
                                      std::shared_ptr<ByteBuffer> & destBuffer,
                                      uint32_t srcPos, uint32_t destPos) {
            swapSegmentHeader(*(node.get()), *(srcBuffer.get()), *(destBuffer.get()), srcPos, destPos);
        }


        /**
         * This method reads and swaps an evio segment header.
         * It can also return information about the segment.
         * Position and limit of neither buffer argument is changed.<p></p>
         * <b>This only swaps data if buffer arguments have opposite byte order!</b>
         *
         * @param node       object in which to store data about the segment
         *                   in destBuffer after swap; may be null
         * @param srcBuffer  buffer containing segment header to be swapped
         * @param destBuffer buffer in which to place swapped segment header
         * @param srcPos     position in srcBuffer to start reading segment header
         * @param destPos    position in destBuffer to start writing swapped segment header
         *
         * @throws EvioException if srcBuffer data underflow;
         *                       if destBuffer is too small to contain swapped data;
         *                       srcBuffer and destBuffer have same byte order.
         */
        static void swapSegmentHeader(EvioNode & node, ByteBuffer & srcBuffer, ByteBuffer & destBuffer,
                                      uint32_t srcPos, uint32_t destPos) {

            if (srcBuffer.order() == destBuffer.order()) {
                throw evio::EvioException("src & dest buffers need different byte order for swapping");
            }

            // Read & swap segment header word
            uint32_t word = srcBuffer.getInt(srcPos);
            destBuffer.putInt(destPos, word);

            node.tag      = (word >> 24) & 0xff;
            uint32_t dt   = (word >> 16) & 0xff;
            node.dataType = dt & 0x3f;
            node.pad      = dt >> 6;
            node.len      = word & 0xffff;
            node.num      = 0;
            node.pos      = destPos;
            node.dataPos  = destPos + 4;
            node.dataLen  = node.len;
        }


        /**
         * This method reads and swaps an evio tagsegment header.
         * It can also return information about the tagsegment.
         * Position and limit of neither buffer argument is changed.<p></p>
         * <b>This only swaps data if buffer arguments have opposite byte order!</b>
         *
         * @param node       object in which to store data about the tagsegment
         *                   in destBuffer after swap; may be null
         * @param srcBuffer  buffer containing tagsegment header to be swapped
         * @param destBuffer buffer in which to place swapped tagsegment header
         * @param srcPos     position in srcBuffer to start reading tagsegment header
         * @param destPos    position in destBuffer to start writing swapped tagsegment header
         *
         * @throws EvioException if srcBuffer is not properly formatted;
         *                       if destBuffer is too small to contain swapped data
         */
        static void swapTagSegmentHeader(std::shared_ptr<EvioNode> & node,
                                         std::shared_ptr<ByteBuffer> & srcBuffer,
                                         std::shared_ptr<ByteBuffer> & destBuffer,
                                         uint32_t srcPos, uint32_t destPos) {
            swapTagSegmentHeader(*(node.get()), *(srcBuffer.get()), *(destBuffer.get()), srcPos, destPos);
        }


        /**
         * This method reads and swaps an evio tagsegment header.
         * It can also return information about the tagsegment.
         * Position and limit of neither buffer argument is changed.<p></p>
         * <b>This only swaps data if buffer arguments have opposite byte order!</b>
         *
         * @param node       object in which to store data about the tagsegment
         *                   in destBuffer after swap; may be null
         * @param srcBuffer  buffer containing tagsegment header to be swapped
         * @param destBuffer buffer in which to place swapped tagsegment header
         * @param srcPos     position in srcBuffer to start reading tagsegment header
         * @param destPos    position in destBuffer to start writing swapped tagsegment header
         *
         * @throws EvioException if srcBuffer is not properly formatted;
         *                       if destBuffer is too small to contain swapped data
         */
        static void swapTagSegmentHeader(EvioNode & node, ByteBuffer & srcBuffer, ByteBuffer & destBuffer,
                                         uint32_t srcPos, uint32_t destPos) {

            if (srcBuffer.order() == destBuffer.order()) {
                throw evio::EvioException("src & dest buffers need different byte order for swapping");
            }

            // Read & swap tagsegment header word
            uint32_t word = srcBuffer.getInt(srcPos);
            destBuffer.putInt(destPos, word);

            node.tag      = (word >> 20) & 0xfff;
            node.dataType = (word >> 16) & 0xf;
            node.len      = word & 0xffff;
            node.num      = 0;
            node.pad      = 0;
            node.pos      = destPos;
            node.dataPos  = destPos + 4;
            node.dataLen  = node.len;
        }


    };


}

#endif //EVIO_6_0_EVENTHEADERPARSER_H
