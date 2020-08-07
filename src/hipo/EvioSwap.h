//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_EVIOSWAP_H
#define EVIO_6_0_EVIOSWAP_H


#include <cstdio>
#include <memory>


#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "DataType.h"
#include "EvioNode.h"
#include "CompositeData.h"
#include "BaseStructure.h"


namespace evio {


    /**
     * Class to hold static methods used to swap evio data.
     * @date 7/17/2020
     * @author timmer
     */
    class EvioSwap {

        public:

        /**
         * Method to swap the endianness of an evio event (bank).
         *
         * @author: Elliott Wolin, 21-nov-2003
         * @author: Carl Timmer, jan-2012
         *
         * @param buf     buffer of evio event data to be swapped
         * @param tolocal if 0 buf contains data of same endian as local host,
         *                else buf has data of opposite endian.
         * @param dest    buffer to place swapped data into.
         *                If this is null, then dest = buf.
         */
        static void swapEvent(uint32_t *buf, int tolocal, uint32_t *dest) {
            swapBank(buf, tolocal, dest);
        }


        /**
         * Routine to swap the endianness of an evio bank.
         * Null buf argument does nothing.
         *
         * @author: Elliott Wolin, 21-nov-2003
         * @author: Carl Timmer, jan-2012
         *
         * @param buf     buffer of evio bank data to be swapped.
         * @param toLocal if false buf contains data of same endian as local host,
         *                else buf has data of opposite endian.
         * @param dest    buffer to place swapped data into.
         *                If this is null, then dest = buf.
         */
        static void swapBank(uint32_t *buf, bool toLocal, uint32_t *dest) {
            if (buf == nullptr) return;

            uint32_t dataLength, dataType;
            uint32_t *p = buf;

            // Swap header to get length and contained type if buf NOT local endian
            if (toLocal) {
                p = ByteOrder::byteSwap32(buf, 2, dest);
            }

            dataLength = p[0] - 1;
            dataType = (p[1] >> 8) & 0x3f; // padding info in top 2 bits of type byte

            // Swap header if buf is local endian
            if (!toLocal) {
                ByteOrder::byteSwap32(buf, 2, dest);
            }

            // Swap non-header bank data
            swapData(&buf[2], dataType, dataLength, toLocal, ((dest == nullptr) ? nullptr : &dest[2]));
        }


        /**
         * Routine to swap the endianness of an evio segment.
         * Null buf argument does nothing.
         *
         * @author: Elliott Wolin, 21-nov-2003
         * @author: Carl Timmer, jan-2012
         *
         * @param buf     buffer of evio segment data to be swapped
         * @param toLocal if false buf contains data of same endian as local host,
         *                else buf has data of opposite endian
         * @param dest    buffer to place swapped data into.
         *                If this is NULL, then dest = buf.
         */
        static void swapSegment(uint32_t *buf, bool toLocal, uint32_t *dest) {
            if (buf == nullptr) return;

            uint32_t dataLength, dataType;
            uint32_t *p = buf;

            // Swap header to get length and contained type if buf NOT local endian
            if (toLocal) {
                p = ByteOrder::byteSwap32(buf, 1, dest);
            }

            dataLength = p[0] & 0xffff;
            dataType = (p[0] >> 16) & 0x3f; /* padding info in top 2 bits of type byte */

            // Swap header if buf is local endian
            if (!toLocal) {
                ByteOrder::byteSwap32(buf, 1, dest);
            }

            // Swap non-header segment data
            swapData(&buf[1], dataType, dataLength, toLocal, ((dest == nullptr) ? nullptr : &dest[1]));
        }


        /**
         * Routine to swap the endianness of an evio tagsegment.
         * Null buf argument does nothing.
         *
         * @author: Elliott Wolin, 21-nov-2003
         * @author: Carl Timmer, jan-2012
         *
         * @param buf     buffer of evio tagsegment data to be swapped
         * @param toLocal if false buf contains data of same endian as local host,
         *                else buf has data of opposite endian
         * @param dest    buffer to place swapped data into.
         *                If this is NULL, then dest = buf.
         */
        static void swapTagsegment(uint32_t *buf, bool toLocal, uint32_t *dest) {
            if (buf == nullptr) return;

            uint32_t dataLength, dataType;
            uint32_t *p = buf;

            // Swap header to get length and contained type if buf NOT local endian
            if (toLocal) {
                p = ByteOrder::byteSwap32(buf, 1, dest);
            }

            dataLength = p[0] & 0xffff;
            dataType = (p[0] >> 16) & 0xf; // no padding info in tagsegments

            // Swap header if buf is local endian
            if (!toLocal) {
                ByteOrder::byteSwap32(buf, 1, dest);
            }

            // Swap non-header tagsegment data
            swapData(&buf[1], dataType, dataLength, toLocal, ((dest == nullptr) ? nullptr : &dest[1]));
        }

// TODO: look at when exception are thrown for javadoc

        /**
         * This method swaps the data of an evio leaf structure. In other words the
         * structure being swapped does not contain evio structures.
         * It does nothing for container types.
         *
         * @param type       type of data being swapped
         * @param srcBuf     buffer containing data to be swapped
         * @param destBuf    buffer in which to place swapped data
         * @param srcPos     position in srcBuffer to start reading data
         * @param destPos    position in destBuffer to start writing swapped data
         * @param len        length of data in 32 bit words
         *
         * @throws EvioException if srcBuffer not in evio format;
         *                       if destBuffer too small;
         *                       if bad values for srcPos and/or destPos;
         */
        static void swapLeafData(DataType const & type,
                                 std::shared_ptr<ByteBuffer> & srcBuf,
                                 std::shared_ptr<ByteBuffer> & destBuf,
                                 size_t srcPos, size_t destPos, size_t len) {
            swapLeafData(type, *(srcBuf.get()), *(destBuf.get()), srcPos, destPos, len);
        }


        /**
         * This method swaps the data of an evio leaf structure. In other words the
         * structure being swapped does not contain evio structures.
         * It does nothing for container types.
         *
         * @param type       type of data being swapped
         * @param srcBuf     buffer containing data to be swapped
         * @param destBuf    buffer in which to place swapped data
         * @param srcPos     position in srcBuffer to start reading data
         * @param destPos    position in destBuffer to start writing swapped data
         * @param len        length of data in 32 bit words
         *
         * @throws EvioException if srcBuffer not in evio format;
         *                       if destBuffer too small;
         *                       if bad values for srcPos and/or destPos;
         */
        static void swapLeafData(DataType const & type, ByteBuffer & srcBuf,
                                 ByteBuffer & destBuf, size_t srcPos, size_t destPos,
                                 size_t len) {

            // We end here
            size_t endPos = srcPos + 4*len;

            // 64 bit swap
            if (type == DataType::LONG64  ||
                type == DataType::ULONG64 ||
                type ==  DataType::DOUBLE64) {

                    // When only swapping, no need to convert to double & back
                    for (; srcPos < endPos; srcPos += 8, destPos += 8) {
                        destBuf.putLong(destPos, srcBuf.getLong(srcPos));
                    }

            }
            // 32 bit swap
            else if (type == DataType::INT32  ||
                     type == DataType::UINT32 ||
                     type == DataType::FLOAT32) {
                // When only swapping, no need to convert to float & back
                for (; srcPos < endPos; srcPos += 4, destPos += 4) {
                    destBuf.putInt(destPos, srcBuf.getInt(srcPos));
                }
            }
            // 16 bit swap
            else if (type == DataType::SHORT16  ||
                     type == DataType::USHORT16) {
                for (; srcPos < endPos; srcPos += 2, destPos += 2) {
                    destBuf.putShort(destPos, srcBuf.getShort(srcPos));
                }
            }
            // no swap
            else if (type == DataType::UNKNOWN32 ||
                     type == DataType::CHAR8     ||
                     type == DataType::UCHAR8    ||
                     type == DataType::CHARSTAR8) {
                // 8 bit swap - no swap needed, but need to copy if destBuf != srcBuf
                for (; srcPos < endPos; srcPos++, destPos++) {
                   destBuf.put(destPos, srcBuf.getByte(srcPos));
                }
            }
            else if (type == DataType::COMPOSITE) {
                // new composite type
                CompositeData::swapAll(srcBuf, destBuf, srcPos, destPos, len);
            }
        }


        /**
         * Routine to swap any type of evio data.
         * This only swaps data associated with an evio structure; i.e. it completely ignores
         * the header associated with it. If this data consists of structures like banks & segments,
         * it will swap them completely.
         * Null src argument does nothing.
         *
         * @author: Elliott Wolin, 21-nov-2003
         * @author: Carl Timmer, jan-2012
         *
         * @param src     source of evio data to be swapped (after evio header).
         * @param type    type of evio data.
         * @param length  length of evio data in 32 bit words
         * @param toLocal if false data is of same endian as local host,
         *                else data is of opposite endian.
         * @param dst     destination of swapped data.
         *                If this is null, then dst = src.
         */
        static void swapData(uint32_t *src, uint32_t type, uint32_t length, bool toLocal, uint32_t *dst) {
            if (src == nullptr) return;

            uint32_t fraglen, l = 0;

            // Swap the data or call swap_fragment
            switch (type) {

                // 32-bit types: uint, float, or int
                case 0x1:
                case 0x2:
                case 0xb:
                    ByteOrder::byteSwap32(src, length, dst);
                    break;

                // unknown or 8-bit types: string array, char, or uchar ... no swap
                case 0x0:
                case 0x3:
                case 0x6:
                case 0x7:
                    ByteOrder::byteNoSwap32(src, length, dst);
                    break;

                // 16-bit types: short or ushort
                case 0x4:
                case 0x5:
                    ByteOrder::byteSwap16(reinterpret_cast<uint16_t *>(src), 2 * length,
                                          reinterpret_cast<uint16_t *>(dst));
                    break;

                // 64-bit types: double, int, or uint
                case 0x8:
                case 0x9:
                case 0xa:
                    ByteOrder::byteSwap64(reinterpret_cast<uint64_t *>(src), length / 2,
                                          reinterpret_cast<uint64_t *>(dst));
                    break;

                // Composite type
                case 0xf:
                    CompositeData::swapAll(reinterpret_cast<uint8_t *>(src),
                                           reinterpret_cast<uint8_t *>(dst),
                                           length, !toLocal);
                    break;

                // Bank
                case 0xe:
                case 0x10:
                    while (l < length) {
                        // src is opposite local endian
                        if (toLocal) {
                            // swap bank
                            swapBank(&src[l], toLocal, (dst == nullptr) ? nullptr : &dst[l]);
                            // bank was this long (32 bit words) including header
                            fraglen = (dst == nullptr) ? src[l] + 1 : dst[l] + 1;
                        }
                        else {
                            fraglen = src[l] + 1;
                            swapBank(&src[l], toLocal, (dst == nullptr) ? nullptr : &dst[l]);
                        }
                        l += fraglen;
                    }
                    break;

                // Segment
                case 0xd:
                case 0x20:
                    while (l < length) {
                        if (toLocal) {
                            swapSegment(&src[l], toLocal, (dst == nullptr) ? nullptr : &dst[l]);
                            fraglen = (dst == nullptr) ? (src[l] & 0xffff) + 1 : (dst[l] & 0xffff) + 1;
                        }
                        else {
                            fraglen = (src[l] & 0xffff) + 1;
                            swapSegment(&src[l], toLocal, (dst == nullptr) ? nullptr : &dst[l]);
                        }
                        l += fraglen;
                    }
                    break;

                // Tagsegment
                case 0xc:
                    while (l < length) {
                        if (toLocal) {
                            swapTagsegment(&src[l], toLocal, (dst == nullptr) ? nullptr : &dst[l]);
                            fraglen = (dst == nullptr) ? (src[l] & 0xffff) + 1 : (dst[l] & 0xffff) + 1;
                        }
                        else {
                            fraglen = (src[l] & 0xffff) + 1;
                            swapTagsegment(&src[l], toLocal, (dst == nullptr) ? nullptr : &dst[l]);
                        }
                        l += fraglen;
                    }
                    break;

                // unknown type, just copy
                default:
                    ByteOrder::byteNoSwap32(src, length, dst);
                    break;
            }
        }


        /**
         * Routine to swap the endianness of an evio structure's (bank, seg, tagseg) data in place,
         * including descendants' data.<p>
         * The endianness of the given structure, obtained through
         * {@link BaseStructure#getByteOrder()}, does <b>NOT</b> change.
         * The caller must explicitly call {@link BaseStructure#setByteOrder()} to do that.<p>
         * If this structure contains Composite data, and since it is stored as a vector of shared pointers
         * to CompositeData objects, it is only serialized into bytes when written out. Thus the only way
         * to switch it's endianness is for the user to call {@link BaseStructure#setByteOrder()} before
         * writing it out as bytes. In other words, this method does <b>NOT</b> swap Composite data.
         *
         * @param strc evio structure in which to swap all data.
         * @author: Carl Timmer, 7/28/2020
         */
        static void swapData(std::shared_ptr<BaseStructure> strc) {
             auto type       = strc->getHeader()->getDataType();
             uint32_t length = strc->getHeader()->getDataLength();
             bool srcIsLocal = strc->getByteOrder().isLocalEndian();

             if (type == DataType::UINT32) {
                 auto & vec = strc->getUIntData();
                 ByteOrder::byteSwap32(vec.data(), length, nullptr);
                 strc->updateUIntData();
             }
             else if (type == DataType::INT32) {
                 auto & vec = strc->getIntData();
                 ByteOrder::byteSwap32(reinterpret_cast<uint32_t*>(vec.data()), length, nullptr);
                 strc->updateIntData();
             }
             else if (type == DataType::FLOAT32) {
                 auto & vec = strc->getFloatData();
                 ByteOrder::byteSwap32(reinterpret_cast<uint32_t*>(vec.data()), length, nullptr);
                 strc->updateFloatData();
             }
             else if (type == DataType::SHORT16) {
                 auto & vec = strc->getShortData();
                 ByteOrder::byteSwap16(reinterpret_cast<uint16_t*>(vec.data()), length, nullptr);
                 strc->updateShortData();
             }
             else if (type == DataType::USHORT16) {
                 auto & vec = strc->getUShortData();
                 ByteOrder::byteSwap16(vec.data(), length, nullptr);
                 strc->updateUShortData();
             }
             else if (type == DataType::LONG64) {
                 auto & vec = strc->getLongData();
                 ByteOrder::byteSwap64(reinterpret_cast<uint64_t*>(vec.data()), length, nullptr);
                 strc->updateLongData();
             }
             else if (type == DataType::ULONG64) {
                 auto & vec = strc->getULongData();
                 ByteOrder::byteSwap64(vec.data(), length, nullptr);
                 strc->updateULongData();
             }
             else if (type == DataType::DOUBLE64) {
                 auto & vec = strc->getDoubleData();
                 ByteOrder::byteSwap64(reinterpret_cast<uint64_t*>(vec.data()), length, nullptr);
                 strc->updateDoubleData();
             }
//             else if (type == DataType::COMPOSITE) {
//             }
             // For containers, just iterate over their children recursively
             else if (type.isBank() || type.isSegment() || type.isTagSegment()) {
                 for (auto kid : strc->getChildren()) {
                     swapData(kid);
                 }
             }

         }


    };


}

#endif //EVIO_6_0_EVIOSWAP_H
