//
// Created by timmer on 9/17/19.
//

#ifndef EVIO_6_0_UTIL_H
#define EVIO_6_0_UTIL_H

#endif //EVIO_6_0_UTIL_H

#include "EvioNode.h"
#include "EvioNodeSource.h"


/**
 * Methods to parse evio into EvioNode objects.
 * These static methods are in this class to avoid C++ self-referential limitations
 * that are not present in Java.
 */
class Util {

public:


    /**
     * This method recursively stores, in the given list, all the information
     * about an evio structure's children found in the given ByteBuffer object.
     * It uses absolute gets so buffer's position does <b>not</b> change.
     *
     * @param node node being scanned
     */
    static void EvioNode::scanStructure(EvioNode & node) {

        uint32_t dType = node.dataType;

        // If node does not contain containers, return since we can't drill any further down
        if (!DataType::isStructure(dType)) {
            return;
        }

        // Start at beginning position of evio structure being scanned
        uint32_t position = node.dataPos;
        // Don't go past the data's end which is (position + length)
        // of evio structure being scanned in bytes.
        uint32_t endingPos = position + 4*node.dataLen;
        // Buffer we're using
        ByteBuffer & buffer = node.buffer;

        uint32_t dt, dataType, dataLen, len, word;

        // Do something different depending on what node contains
        if (DataType::isBank(dType)) {
            // Extract all the banks from this bank of banks.
            // Make allowance for reading header (2 ints).
            endingPos -= 8;
            while (position <= endingPos) {

                // Copy node for setting stuff that's the same as the parent
                auto pkidNode = new EvioNode(node);
                EvioNode & kidNode = *pkidNode;
                // Clear children & data
                kidNode.childNodes.clear();
                kidNode.data.clear();
                // EvioNode kidNode = (EvioNode) node.clone();

                // Read first header word
                len = buffer.getInt(position);
                kidNode.pos = position;

                // Len of data (no header) for a bank
                dataLen = len - 1;
                position += 4;

                // Read and parse second header word
                word = buffer.getInt(position);
                position += 4;
                kidNode.tag = (word >> 16) & 0xffff;
                dt = (word >> 8) & 0xff;
                dataType = dt & 0x3f;
                kidNode.pad = dt >> 6;
                kidNode.num = word & 0xff;


                kidNode.len = len;
                kidNode.type = DataType::BANK.getValue();  // This is a bank
                kidNode.dataLen = dataLen;
                kidNode.dataPos = position;
                kidNode.dataType = dataType;
                kidNode.izEvent = false;

                // Create the tree structure
                kidNode.parentNode = shared_ptr<EvioNode>(&node);
                // Add this to list of children and to list of all nodes in the event
                node.addChild(pkidNode);

                // Only scan through this child if it's a container
                if (DataType::isStructure(dataType)) {
                    scanStructure(kidNode);
                }

                // Set position to start of next header (hop over kid's data)
                position += 4 * dataLen;
            }
        }
        else if (DataType::isSegment(dType)) {

            // Extract all the segments from this bank of segments.
            // Make allowance for reading header (1 int).
            endingPos -= 4;
            while (position <= endingPos) {
                // Copy node for setting stuff that's the same as the parent
                auto pkidNode = new EvioNode(node);
                EvioNode & kidNode = *pkidNode;
                // Clear children & data
                kidNode.childNodes.clear();
                kidNode.data.clear();

                kidNode.pos = position;

                word = buffer.getInt(position);
                position += 4;
                kidNode.tag = (word >> 24) & 0xff;
                dt = (word >> 16) & 0xff;
                dataType = dt & 0x3f;
                kidNode.pad = dt >> 6;
                len = word & 0xffff;


                kidNode.num      = 0;
                kidNode.len      = len;
                kidNode.type     = DataType::SEGMENT.getValue();  // This is a segment
                kidNode.dataLen  = len;
                kidNode.dataPos  = position;
                kidNode.dataType = dataType;
                kidNode.izEvent  = false;

                kidNode.parentNode = shared_ptr<EvioNode>(&node);
                node.addChild(pkidNode);

                if (DataType::isStructure(dataType)) {
                    scanStructure(kidNode);
                }

                position += 4*len;
            }
        }
            // Only one type of structure left - tagsegment
        else {

            // Extract all the tag segments from this bank of tag segments.
            // Make allowance for reading header (1 int).
            endingPos -= 4;
            while (position <= endingPos) {
                // Copy node for setting stuff that's the same as the parent
                auto pkidNode = new EvioNode(node);
                EvioNode & kidNode = *pkidNode;
                // Clear children & data
                kidNode.childNodes.clear();
                kidNode.data.clear();

                kidNode.pos = position;

                word = buffer.getInt(position);
                position += 4;
                kidNode.tag = (word >> 20) & 0xfff;
                dataType    = (word >> 16) & 0xf;
                len         =  word & 0xffff;

                kidNode.pad      = 0;
                kidNode.num      = 0;
                kidNode.len      = len;
                kidNode.type     = DataType::TAGSEGMENT.getValue();  // This is a tag segment
                kidNode.dataLen  = len;
                kidNode.dataPos  = position;
                kidNode.dataType = dataType;
                kidNode.izEvent  = false;

                kidNode.parentNode = shared_ptr<EvioNode>(&node);
                node.addChild(pkidNode);

                if (DataType::isStructure(dataType)) {
                    scanStructure(kidNode);
                }

                position += 4*len;
            }
        }
    }


    /**
     * This method recursively stores, in the given list, all the information
     * about an evio structure's children found in the given ByteBuffer object.
     * It uses absolute gets so buffer's position does <b>not</b> change.
     *
     * @param node node being scanned
     * @param nodeSource source of EvioNode objects to use while parsing evio data.
     */
    static void EvioNode::scanStructure(EvioNode & node, EvioNodeSource & nodeSource) {

        int dType = node.dataType;

        // If node does not contain containers, return since we can't drill any further down
        if (!DataType::isStructure(dType)) {
            return;
        }

        // Start at beginning position of evio structure being scanned
        int position = node.dataPos;
        // Don't go past the data's end which is (position + length)
        // of evio structure being scanned in bytes.
        int endingPos = position + 4*node.dataLen;
        // Buffer we're using
        ByteBuffer & buffer = node.buffer;

        uint32_t dt, dataType, dataLen, len, word;

        // Do something different depending on what node contains
        if (DataType::isBank(dType)) {
            // Extract all the banks from this bank of banks.
            // Make allowance for reading header (2 ints).
            endingPos -= 8;
            while (position <= endingPos) {

                EvioNode kidNode = nodeSource.getNode();
                kidNode.copyParentForScan(&node);

                // Read first header word
                len = buffer.getInt(position);
                kidNode.pos = position;

                // Len of data (no header) for a bank
                dataLen = len - 1;
                position += 4;

                // Read and parse second header word
                word = buffer.getInt(position);
                position += 4;
                kidNode.tag = (word >> 16) & 0xffff;
                dt = (word >> 8) & 0xff;
                dataType = dt & 0x3f;
                kidNode.pad = dt >> 6;
                kidNode.num = word & 0xff;


                kidNode.len = len;
                kidNode.type = DataType::BANK.getValue();  // This is a bank
                kidNode.dataLen = dataLen;
                kidNode.dataPos = position;
                kidNode.dataType = dataType;
                kidNode.izEvent = false;

                // Add this to list of children and to list of all nodes in the event
                node.addChild(&kidNode);

                // Only scan through this child if it's a container
                if (DataType::isStructure(dataType)) {
                    scanStructure(kidNode, nodeSource);
                }

                // Set position to start of next header (hop over kid's data)
                position += 4 * dataLen;
            }
        }
        else if (DataType::isSegment(dType)) {

            // Extract all the segments from this bank of segments.
            // Make allowance for reading header (1 int).
            endingPos -= 4;
            while (position <= endingPos) {
                EvioNode kidNode = nodeSource.getNode();
                kidNode.copyParentForScan(&node);

                kidNode.pos = position;

                word = buffer.getInt(position);
                position += 4;
                kidNode.tag = (word >> 24) & 0xff;
                dt = (word >> 16) & 0xff;
                dataType = dt & 0x3f;
                kidNode.pad = dt >> 6;
                len = word & 0xffff;


                kidNode.num      = 0;
                kidNode.len      = len;
                kidNode.type     = DataType::SEGMENT.getValue();  // This is a segment
                kidNode.dataLen  = len;
                kidNode.dataPos  = position;
                kidNode.dataType = dataType;
                kidNode.izEvent  = false;

                node.addChild(&kidNode);

                if (DataType::isStructure(dataType)) {
                    scanStructure(kidNode, nodeSource);
                }

                position += 4*len;
            }
        }
            // Only one type of structure left - tagsegment
        else {

            // Extract all the tag segments from this bank of tag segments.
            // Make allowance for reading header (1 int).
            endingPos -= 4;
            while (position <= endingPos) {

                EvioNode kidNode = nodeSource.getNode();
                kidNode.copyParentForScan(&node);

                kidNode.pos = position;

                word = buffer.getInt(position);
                position += 4;
                kidNode.tag = (word >> 20) & 0xfff;
                dataType    = (word >> 16) & 0xf;
                len         =  word & 0xffff;

                kidNode.pad      = 0;
                kidNode.num      = 0;
                kidNode.len      = len;
                kidNode.type     = DataType::TAGSEGMENT.getValue();  // This is a tag segment
                kidNode.dataLen  = len;
                kidNode.dataPos  = position;
                kidNode.dataType = dataType;
                kidNode.izEvent  = false;

                node.addChild(&kidNode);

                if (DataType::isStructure(dataType)) {
                    scanStructure(kidNode, nodeSource);
                }

                position += 4*len;
            }
        }
    }


    /**
     * This method extracts an EvioNode object representing an
     * evio event (top level evio bank) from a given buffer, a
     * location in the buffer, and a few other things. An EvioNode
     * object represents an evio container - either a bank, segment,
     * or tag segment.
     *
     * @param buffer     buffer to examine
     * @param recordNode object holding data about block header
     * @param position   position in buffer
     * @param place      place of event in buffer (starting at 0)
     *
     * @return EvioNode object containing evio event information
     * @throws EvioException if not enough data in buffer to read evio bank header (8 bytes).
     */
    static EvioNode & EvioNode::extractEventNode(ByteBuffer & buffer,
                                                 RecordNode & recNode,
                                                 uint32_t position, uint32_t place) {

        // Make sure there is enough data to at least read evio header
        if (buffer.remaining() < 8) {
            throw EvioException("buffer underflow");
        }

        // Store evio event info, without de-serializing, into EvioNode object
        // Create node here and pass reference back
        auto pNode = new EvioNode(position, place, buffer, recNode);
        return extractNode(*pNode, position);
    }


    /**
 * This method extracts an EvioNode object representing an
 * evio event (top level evio bank) from a given buffer, a
 * location in the buffer, and a few other things. An EvioNode
 * object represents an evio container - either a bank, segment,
 * or tag segment.
 *
 * @param buffer     buffer to examine
 * @param pool       pool of EvioNode objects
 * @param recordNode object holding data about block header
 * @param position   position in buffer
 * @param place      place of event in buffer (starting at 0)
 *
 * @return EvioNode object containing evio event information
 * @throws EvioException if not enough data in buffer to read evio bank header (8 bytes).
 */
    static EvioNode & EvioNode::extractEventNode(ByteBuffer & buffer,
                                                 EvioNodeSource & pool,
                                                 RecordNode & recNode,
                                                 uint32_t position, uint32_t place) {

        // Make sure there is enough data to at least read evio header
        if (buffer.remaining() < 8) {
            throw EvioException("buffer underflow");
        }

        // Store evio event info, without de-serializing, into EvioNode object
        EvioNode & node = pool.getNode();
        node.clear(); //node.clearIntArray();
        node.setData(position, place, buffer, recNode);
        return extractNode(node, position);
    }



//TODO: these may have to return shared pointers!

    /**
     * This method extracts an EvioNode object representing an
     * evio event (top level evio bank) from a given buffer, a
     * location in the buffer, and a few other things. An EvioNode
     * object represents an evio container - either a bank, segment,
     * or tag segment.
     *
     * @param buffer       buffer to examine
     * @param recPosition  position of containing record
     * @param position     position in buffer
     * @param place        place of event in buffer (starting at 0)
     *
     * @return EvioNode object containing evio event information
     * @throws EvioException if not enough data in buffer to read evio bank header (8 bytes).
     */
    static EvioNode & EvioNode::extractEventNode(ByteBuffer & buffer,
                                                 uint32_t recPosition, uint32_t position, uint32_t place) {

        // Make sure there is enough data to at least read evio header
        if (buffer.remaining() < 8) {
            throw EvioException("buffer underflow");
        }

        // Store evio event info, without de-serializing, into EvioNode object
        // Create node here and pass reference back
        auto pNode = new EvioNode(position, place, recPosition, buffer);
        return extractNode(*pNode, position);
    }

    /**
      * This method extracts an EvioNode object representing an
      * evio event (top level evio bank) from a given buffer, a
      * location in the buffer, and a few other things. An EvioNode
      * object represents an evio container - either a bank, segment,
      * or tag segment.
      *
      * @param buffer       buffer to examine
      * @param pool         pool of EvioNode objects
      * @param recPosition  position of containing record
      * @param position     position in buffer
      * @param place        place of event in buffer (starting at 0)
      *
      * @return EvioNode object containing evio event information
      * @throws EvioException if not enough data in buffer to read evio bank header (8 bytes).
      */
    static EvioNode & EvioNode::extractEventNode(ByteBuffer & buffer, EvioNodeSource & pool,
                                                 uint32_t recPosition, uint32_t position, uint32_t place) {

        // Make sure there is enough data to at least read evio header
        if (buffer.remaining() < 8) {
            throw EvioException("buffer underflow");
        }

        // Store evio event info, without de-serializing, into EvioNode object
        EvioNode & node = pool.getNode();
        node.clear(); //node.clearIntArray();
        node.setData(position, place, recPosition, buffer);
        // This will return a reference to the actual node in the pool
        return extractNode(node, position);
    }



};