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
#include <cstdint>
#include <cstddef>
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
         * Swap byte array in place assuming bytes are 32 bit ints.
         * Number of array elements must be multiple of 4.
         *
         * @param data       byte array to convert.
         * @param dataLen    number of bytes to convert.
         * @throws EvioException if data is null;
         *                       dataLen is not a multiple of 4.
         */
        static void swapArray32(uint8_t *data, uint32_t dataLen) {

            if (data == nullptr || (dataLen % 4 != 0)) {
                throw EvioException("bad arg");
            }

            uint32_t index;
            uint8_t b1,b2,b3,b4;

            for (uint32_t i=0; i < dataLen; i+=4) {
                index = i;

                b1 = (data[index]);
                b2 = (data[index+1]);
                b3 = (data[index+2]);
                b4 = (data[index+3]);

                data[index]   = b4;
                data[index+1] = b3;
                data[index+2] = b2;
                data[index+3] = b1;
            }
        }


        // =========================
        // Swapping Evio Data
        // =========================


        /**
         * This method swaps the byte order of an entire evio event or bank.
         * The byte order of the swapped buffer will be opposite to the byte order
         * of the source buffer argument. If the swap is done in place, the
         * byte order of the source buffer will be switched upon completion and
         * the destPos arg will be set equal to the srcPos arg.
         * The positions of the source and destination buffers are not changed.
         * A ByteBuffer's current byte order can be found by calling
         * {@link java.nio.ByteBuffer#order()}.<p>
         *
         * The data to be swapped must <b>not</b> be in the evio file format (with
         * record headers). Data must only consist of bytes representing a single event/bank.
         * Position and limit of neither buffer is changed.
         *
         * @param srcBuffer   buffer containing event to swap.
         * @param destBuffer  buffer in which to place the swapped event.
         *                    If null, or identical to srcBuffer, the data is swapped in place.
         * @param nodeList    if not null, generate and store node objects here -
         *                    one for each swapped evio structure in destBuffer.
         * @param storeNodes  if true, store generated EvioNodes in nodeList.
         * @param swapData    if false, do NOT swap data, else swap data too.
         * @param srcPos      position in srcBuffer to start reading event.
         * @param destPos     position in destBuffer to start writing swapped event.
         *
         * @throws EvioException if srcBuffer arg is null;
         *                       if any buffer position is not zero
         */
        static void swapEvent(std::shared_ptr<ByteBuffer> & srcBuffer,
                              std::shared_ptr<ByteBuffer> & destBuffer,
                              std::vector<std::shared_ptr<EvioNode>> & nodeList,
                              bool storeNodes=false, bool swapData=true,
                              size_t srcPos=0, size_t destPos=0) {

                if (srcBuffer == nullptr) {
                    throw EvioException("Null event in swapEvent");
                }

                // Find the destination byte order and if it is to be swapped in place
                bool inPlace = false;
                ByteOrder srcOrder  = srcBuffer->order();
                ByteOrder destOrder = srcOrder.getOppositeEndian();

                if (destBuffer == nullptr || srcBuffer == destBuffer) {
                    // Do this so we can treat the source buffer as if it were a
                    // completely different destination buffer with its own byte order.
                    destBuffer = srcBuffer->duplicate();
                    destPos = srcPos;
                    inPlace = true;
                }
                destBuffer->order(destOrder);

                // Check position args
                if (srcPos > srcBuffer->capacity() - 8) {
                    throw EvioException("bad value for srcPos arg");
                }

                if (destPos > destBuffer->capacity() - 8) {
                    throw EvioException("bad value for destPos arg");
                }

                // Create node
                auto node = EvioNode::createEvioNode();

                // We know events are banks, so start by reading & swapping a bank header
                swapBankHeader(node, srcBuffer, destBuffer, srcPos, destPos);

                // Store all nodes here
                if (storeNodes) {
                    // Set a few special members for an event
                    node->eventNode = node;
                    node->scanned   = true;
                    node->izEvent   = true;
                    node->type = DataType::BANK.getValue();
                    nodeList.push_back(node);
                }
//                std::cout << "Created top node containing " << node->getDataTypeObj().toString() << std::endl;

                // The event is an evio bank so recursively swap it as such
                swapStructure(node, srcBuffer, destBuffer, nodeList,
                              storeNodes, swapData, inPlace,
                              srcPos + 8, destPos + 8);

                if (inPlace) {
                    srcBuffer->order(destOrder);
                }
        }


        /**
          * <p>This method reads and swaps an evio bank header.
          * It can also return information about the bank.
          * Position and limit of neither buffer argument is changed.</p>
          * <b>This only swaps data if buffer arguments have opposite byte order!</b>
          *
          * @param node       object in which to store data about the bank
          *                   in destBuffer after swap; may be null
          * @param srcBuffer  buffer containing bank header to be swapped
          * @param destBuffer buffer in which to place swapped bank header
          * @param srcPos     position in srcBuffer to start reading bank header
          * @param destPos    position in destBuffer to start writing swapped bank header
          *
          * @throws EvioException if srcBuffer too little data;
          *                       if destBuffer is too small to contain swapped data
          */
        static void swapBankHeader(std::shared_ptr<EvioNode> & node, std::shared_ptr<ByteBuffer> & srcBuffer,
                                   std::shared_ptr<ByteBuffer> & destBuffer, size_t srcPos=0, size_t destPos=0) {

                try {
                    // Read & swap first bank header word
                    int length = srcBuffer->getInt(srcPos);
                    destBuffer->putInt(destPos, length);
                    srcPos  += 4;
                    destPos += 4;

                    // Read & swap second bank header word
                    uint32_t word = srcBuffer->getUInt(srcPos);
                    destBuffer->putInt(destPos, word);

                    // node is never null
                    node->tag = word >> 16;

                    uint32_t dt = (word >> 8) & 0xff;
                    node->dataType = dt & 0x3f;
                    node->pad = dt >> 6;

                    // If only 7th bit set, it can be tag=0, num=0, type=0, padding=1.
                    // This regularly happens with composite data.
                    // However, it that MAY also be the legacy tagsegment type
                    // with no padding information. Ignore this as having tag & num
                    // in legacy code is probably rare.
                    //if (dt == 0x40) {
                    //    node->dataType = DataType::TAGSEGMENT.getValue();
                    //    node->pad = 0;
                    //}

                    node->num     = word & 0xff;
                    node->len     = length;
                    node->pos     = destPos - 4;
                    node->dataPos = destPos + 4;
                    node->dataLen = length - 1;
                }
                catch (std::overflow_error & e) {
                    throw EvioException("destBuffer too small to hold swapped data");
                }
                catch (std::underflow_error & e) {
                    throw EvioException("srcBuffer data underflow");
                }
        }


        /**
         * <p>This method reads and swaps an evio segment header.
         * It can also return information about the segment.
         * Position and limit of neither buffer argument is changed.</p>
         * <b>This only swaps data if buffer arguments have opposite byte order!</b>
         *
         * @param node       object in which to store data about the segment
         *                   in destBuffer after swap; may be null
         * @param srcBuffer  buffer containing segment header to be swapped
         * @param destBuffer buffer in which to place swapped segment header
         * @param srcPos     position in srcBuffer to start reading segment header
         * @param destPos    position in destBuffer to start writing swapped segment header
         *
         * @throws EvioException if srcBuffer too little data;
         *                       if destBuffer is too small to contain swapped data
         */
        static void swapSegmentHeader(std::shared_ptr<EvioNode> & node, std::shared_ptr<ByteBuffer> & srcBuffer,
                                      std::shared_ptr<ByteBuffer> & destBuffer, size_t srcPos=0, size_t destPos=0) {

                try {
                    // Read & swap segment header word
                    uint32_t word = srcBuffer->getUInt(srcPos);
                    destBuffer->putInt(destPos, word);

                    // node is never null
                    node->tag = word >> 24;

                    uint32_t dt = (word >> 16) & 0xff;
                    node->dataType = dt & 0x3f;
                    node->pad = dt >> 6;

                    node->len     = word & 0xffff;
                    node->num     = 0;
                    node->pos     = destPos;
                    node->dataPos = destPos + 4;
                    node->dataLen = node->len;
                }
                catch (std::overflow_error & e) {
                    throw EvioException("destBuffer too small to hold swapped data");
                }
                catch (std::underflow_error & e) {
                    throw EvioException("srcBuffer data underflow");
                }
        }


        /**
         * <p>This method reads and swaps an evio tagsegment header.
         * It can also return information about the tagsegment.
         * Position and limit of neither buffer argument is changed.</p>
         * <b>This only swaps data if buffer arguments have opposite byte order!</b>
         *
         * @param node       object in which to store data about the tagsegment
         *                   in destBuffer after swap; may be null
         * @param srcBuffer  buffer containing tagsegment header to be swapped
         * @param destBuffer buffer in which to place swapped tagsegment header
         * @param srcPos     position in srcBuffer to start reading tagsegment header
         * @param destPos    position in destBuffer to start writing swapped tagsegment header
         *
         * @throws EvioException if srcBuffer too little data;
         *                       if destBuffer is too small to contain swapped data
         */
        static void swapTagSegmentHeader(std::shared_ptr<EvioNode> & node, std::shared_ptr<ByteBuffer> & srcBuffer,
                                         std::shared_ptr<ByteBuffer> & destBuffer, size_t srcPos=0, size_t destPos=0) {

                try {
                    // Read & swap tagsegment header word
                    uint32_t word = srcBuffer->getUInt(srcPos);
                    destBuffer->putInt(destPos, word);

                    // node is never null
                    node->tag      = word >> 20;
                    node->dataType = (word >> 16) & 0xf;
                    node->len      = word & 0xffff;
                    node->num      = 0;
                    node->pad      = 0;
                    node->pos      = destPos;
                    node->dataPos  = destPos + 4;
                    node->dataLen  = node->len;
                }
                catch (std::overflow_error & e) {
                    throw EvioException("destBuffer too small to hold swapped data");
                }
                catch (std::underflow_error & e) {
                    throw EvioException("srcBuffer data underflow");
                }
        }


        /**
         * This method swaps the data of an evio leaf structure. In other words the
         * structure being swapped does not contain evio structures.
         *
         * @param type       type of data being swapped
         * @param srcBuffer  buffer containing data to be swapped
         * @param destBuffer buffer in which to place swapped data
         * @param srcPos     position in srcBuffer to start reading data
         * @param destPos    position in destBuffer to start writing swapped data
         * @param len        length of data in 32 bit words
         * @param inPlace    if true, data is swapped in srcBuffer
         *
         * @throws EvioException if srcBuffer not in evio format;
         *                       if destBuffer too small;
         *                       if bad values for srcPos and/or destPos;
         */
        static void swapData(DataType const & type, std::shared_ptr<ByteBuffer> & srcBuffer,
                             std::shared_ptr<ByteBuffer> & destBuffer,
                             uint32_t len, bool inPlace = false,
                             size_t srcPos=0, size_t destPos=0) {

            // We end here
            uint32_t endPos = srcPos + 4*len;

            // 64 bit swap
            if (type == DataType::LONG64  ||
                type == DataType::ULONG64 ||
                type ==  DataType::DOUBLE64) {

                // When only swapping, no need to convert to double & back
                for (; srcPos < endPos; srcPos += 8, destPos += 8) {
                    destBuffer->putLong(destPos, srcBuffer->getLong(srcPos));
                }
            }
            // 32 bit swap
            else if (type == DataType::INT32  ||
                     type == DataType::UINT32 ||
                     type == DataType::FLOAT32) {
                // When only swapping, no need to convert to float & back
                for (; srcPos < endPos; srcPos += 4, destPos += 4) {
                    destBuffer->putInt(destPos, srcBuffer->getInt(srcPos));
                }
            }
            // 16 bit swap
            else if (type == DataType::SHORT16  ||
                     type == DataType::USHORT16) {
                for (; srcPos < endPos; srcPos += 2, destPos += 2) {
                    destBuffer->putShort(destPos, srcBuffer->getShort(srcPos));
                }
            }
            // no swap
            else if (type == DataType::UNKNOWN32 ||
                     type == DataType::CHAR8     ||
                     type == DataType::UCHAR8    ||
                     type == DataType::CHARSTAR8) {

                // 8 bit swap - no swap needed, but need to copy if destBuf != srcBuf
                if (!inPlace) {
                    for (; srcPos < endPos; srcPos++, destPos++) {
                        destBuffer->put(destPos, srcBuffer->getByte(srcPos));
                    }
                }
            }
            else if (type == DataType::COMPOSITE) {
                // new composite type
                CompositeData::swapAll(srcBuffer, destBuffer, srcPos, destPos, len);

            }
        }


        /**
         * Swap an evio structure. If it is a structure of structures,
         * such as a bank of banks, swap recursively.
         *
         * @param node       info from parsed header
         * @param srcBuffer  buffer containing structure to swap.
         * @param destBuffer buffer in which to place the swapped structure.
         * @param nodeList   if not null, store all node objects here -
         *                   one for each swapped evio structure in destBuffer.
         * @param storeNodes if true, store generated EvioNodes in nodeList.
         * @param swapData   if false, do NOT swap data, else swap data too.
         * @param inPlace    if true, data is swapped in srcBuffer
         * @param srcPos     position in srcBuffer to start reading structure
         * @param destPos    position in destBuffer to start writing swapped structure
         *
         * @throws EvioException if args are null;
         *                       IF srcBuffer not in evio format;
         *                       if destBuffer too small;
         *                       if bad values for srcPos and/or destPos;
         */
        static void swapStructure(std::shared_ptr<EvioNode> & node,
                                  std::shared_ptr<ByteBuffer> & srcBuffer,
                                  std::shared_ptr<ByteBuffer> & destBuffer,
                                  std::vector<std::shared_ptr<EvioNode>> & nodeList,
                                  bool storeNodes=false, bool swapData=true, bool inPlace=false,
                                  size_t srcPos=0, size_t destPos=0) {

            if (srcBuffer == nullptr || destBuffer == nullptr || node == nullptr) {
                throw EvioException("arg(s) are null");
            }

//            std::cout << "swapStructure: storeNodes = " << storeNodes << ", swapData = " << swapData <<
//            ", inPlace = " << inPlace << ", srcPos = " << srcPos << ", destPos = " << destPos << std::endl;

            // Pass in header info through node object.
            // Reuse this node object if not storing in a list of all nodes.
            DataType dataType = node->getDataTypeObj();
            uint32_t length = node->dataLen;
//            std::cout << "swapStructure: dataType = " << dataType.toString() << ", len = " << length << std::endl;

            // If not a structure of structures, swap the data and return - no more recursion.
            if (!dataType.isStructure()) {
                // swap raw data here
                if (swapData) EvioSwap::swapData(dataType, srcBuffer, destBuffer, length, inPlace, srcPos, destPos);
                //std::cout << "HIT END_OF_LINE" << std::endl;
                return;
            }

            uint32_t offset = 0;
            size_t sPos = srcPos, dPos = destPos;

            std::shared_ptr<EvioNode> firstNode = nullptr;
            if (!nodeList.empty()) {
                try {
                    firstNode = nodeList.at(0);
                    node = EvioNode::createEvioNode(firstNode);
                } catch (const std::out_of_range &e) {
                    std::cerr << "Exception: " << e.what() << std::endl;
                    throw EvioException(e.what());
                }
            }

            // change 32-bit words to bytes
            length *= 4;

            // This structure contains banks
            if (dataType.isBank()) {
                // Swap all banks contained in this structure
                while (offset < length) {
                    swapBankHeader(node, srcBuffer, destBuffer, sPos, dPos);

                    // Position offset to start of next header
                    offset += 4 * (node->len + 1); // plus 1 for length word

                    // Recursive call, reuse node object if not storing
                    swapStructure(node, srcBuffer, destBuffer, nodeList,
                                  storeNodes, swapData, inPlace,
                                  sPos + 8, dPos + 8);

                    sPos = srcPos  + offset;
                    dPos = destPos + offset;

                    // Store node objects
                    if (storeNodes) {
                        node->type = DataType::BANK.getValue();
                        nodeList.push_back(node);
                        if (offset < length) {
                            node = EvioNode::createEvioNode(firstNode);
                        }
                    }
                }
            }
            else if (dataType.isSegment()) {
                // extract all the segments from this bank.
                while (offset < length) {
                    swapSegmentHeader(node, srcBuffer, destBuffer, sPos, dPos);

                    offset += 4 * (node->len + 1);

                    swapStructure(node, srcBuffer, destBuffer, nodeList,
                                  storeNodes, swapData, inPlace,
                                  sPos + 4, dPos + 4);

                    sPos = srcPos + offset;
                    dPos = destPos + offset;

                    // Store node objects
                    if (storeNodes) {
                        node->type = DataType::SEGMENT.getValue();
                        nodeList.push_back(node);
                        if (offset < length) {
                            node = EvioNode::createEvioNode(firstNode);
                        }
                    }
                }
            }
            else if (dataType.isTagSegment()) {
                // extract all the tag segments from this bank.
                while (offset < length) {

                    swapTagSegmentHeader(node, srcBuffer, destBuffer, sPos, dPos);

                    offset += 4 * (node->len + 1);

                    swapStructure(node, srcBuffer, destBuffer, nodeList,
                                  storeNodes, swapData, inPlace,
                                  sPos + 4, dPos + 4);

                    sPos = srcPos + offset;
                    dPos = destPos + offset;

                    if (storeNodes) {
                        node->type = DataType::TAGSEGMENT.getValue();
                        nodeList.push_back(node);
                        if (offset < length) {
                            node = EvioNode::createEvioNode(firstNode);
                        }
                    }
                }
            }
        }





























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
          * Method to swap the endianness of an evio event (bank).
          *
          * @author: Carl Timmer, mar-2025
          *
          * @param buf     buffer of evio event data to be swapped
          * @param dest    buffer to place swapped data into.
          *                If this is null, then dest = buf.
          */
        static void swapEvent(ByteBuffer & buf, ByteBuffer & dest) {

            bool tolocal = buf.order() != ByteOrder::nativeOrder();
            swapBank(reinterpret_cast<uint32_t *>(buf.array()), tolocal,
                     reinterpret_cast<uint32_t *>(dest.array()));
        }


        /**
          * Method to swap the endianness of an evio event (bank).
          *
          * @author: Carl Timmer, mar-2025
          *
          * @param buf     buffer of evio event data to be swapped
          * @param dest    buffer to place swapped data into.
          *                If this is null, then dest = buf.
          */
        static void swapEvent(std::shared_ptr<ByteBuffer> & buf,
                              std::shared_ptr<ByteBuffer> & dest) {

            bool tolocal = buf->order() != ByteOrder::nativeOrder();
            swapBank(reinterpret_cast<uint32_t *>(buf->array()), tolocal,
                     reinterpret_cast<uint32_t *>(dest->array()));
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
std::cout << "swapBank: dataLen = " << dataLength << std::endl;

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
            swapLeafData(type, *srcBuf, *destBuf, srcPos, destPos, len);
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
                 for (auto & kid : strc->getChildren()) {
                     swapData(kid);
                 }
             }

         }


    };


}

#endif //EVIO_6_0_EVIOSWAP_H
