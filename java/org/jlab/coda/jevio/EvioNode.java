/*
 * Copyright (c) 2013, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 */

package org.jlab.coda.jevio;


import java.nio.*;
import java.util.ArrayList;
import java.util.List;

/**
 * This class is used to store relevant info about an evio container
 * (bank, segment, or tag segment), without having
 * to de-serialize it into many objects and arrays.
 * It is not thread-safe and is designed for speed.
 *
 * @author timmer
 * Date: 11/13/12
 */
public class EvioNode implements Cloneable {

    /** Header's length value (32-bit words). */
    int len;
    /** Header's tag value. */
    int tag;
    /** Header's num value. */
    int num;
    /** Header's padding value. */
    int pad;
    /** Position of header in buffer in bytes.  */
    int pos;
    /** This node's (evio container's) type. Must be bank, segment, or tag segment. */
    int type;

    /** Length of node's data in 32-bit words. */
    int dataLen;
    /** Position of node's data in buffer in bytes. */
    int dataPos;
    /** Type of data stored in node. */
    int dataType;

    /** Position of the record in buffer containing this node in bytes
     *  @since version 6. */
    int recordPos;

    /** Store data in int array form if calculated. */
    int[] data;

    /** Does this node represent an event (top-level bank)? */
    boolean isEvent;

    /** If the data this node represents is removed from the buffer,
     *  then this object is obsolete. */
    boolean obsolete;

    /** Block containing this node. */
    public BlockNode blockNode;

    /** ByteBuffer that this node is associated with. */
    ByteBuffer buffer;

    /** List of child nodes ordered according to placement in buffer. */
    ArrayList<EvioNode> childNodes;

    //-------------------------------
    // For event-level node
    //-------------------------------

    /**
     * Place of containing event in file/buffer. First event = 0, second = 1, etc.
     * Useful for converting node to EvioEvent object (de-serializing).
     */
    int place;

    /**
     * If top-level event node, was I scanned and all my banks
     * already placed into a list?
     */
    boolean scanned;

    /** List of all nodes in the event including the top-level object
     *  ordered according to placement in buffer.
     *  Only created at the top-level (with constructor). All lower-level
     *  nodes are created with clone() so all nodes have a reference to
     *  the top-level allNodes object. */
    ArrayList<EvioNode> allNodes;

    //-------------------------------
    // For sub event-level node
    //-------------------------------

    /** Node of event containing this node. Is null if this is an event node. */
    EvioNode eventNode;

    /** Node containing this node. Is null if this is an event node. */
    EvioNode parentNode;

    //-------------------------------
    // For testing
    //-------------------------------
    /** If in pool, the pool's id. */
    int poolId = -1;


    //----------------------------------
    // Constructors (package accessible)
    //----------------------------------

    /** Constructor when fancy features not needed. */
    public EvioNode() {
        // Put this node in list of all nodes (evio banks, segs, or tagsegs)
        // contained in this event.
        allNodes = new ArrayList<>(50);
        allNodes.add(this);
    }

    /** Constructor used when swapping data. */
    EvioNode(EvioNode firstNode) {
        allNodes = new ArrayList<>(50);
        allNodes.add(this);
        scanned = true;
        eventNode = firstNode;
    }

    //----------------------------------

    /**
     * Constructor which creates an EvioNode associated with
     * an event (top level) evio container when parsing buffers
     * for evio data.
     *
     * @param pos        position of event in buffer (number of bytes)
     * @param place      containing event's place in buffer (starting at 0)
     * @param buffer buffer containing this event
     * @param blockNode  block containing this event
     */
    public EvioNode(int pos, int place, ByteBuffer buffer, BlockNode blockNode) {
        this.pos = pos;
        this.place = place;
        this.blockNode = blockNode;
        this.buffer = buffer;
        // This is an event by definition
        this.isEvent = true;
        // Event is a Bank by definition
        this.type = DataType.BANK.getValue();

        // Put this node in list of all nodes (evio banks, segs, or tagsegs)
        // contained in this event.
        allNodes = new ArrayList<>(50);
        allNodes.add(this);
    }


    /**
     * Constructor which creates an EvioNode associated with
     * an event (top level) evio container when parsing buffers
     * for evio data.
     *
     * @param pos        position of event in buffer (number of bytes).
     * @param place      containing event's place in buffer (starting at 0).
     * @param recordPos  position of record containing this node.
     * @param buffer buffer containing this event.
     */
    public EvioNode(int pos, int place, int recordPos, ByteBuffer buffer) {
        this.pos = pos;
        this.place = place;
        this.recordPos = recordPos;
        this.buffer = buffer;
        // This is an event by definition
        this.isEvent = true;
        // Event is a Bank by definition
        this.type = DataType.BANK.getValue();

        // Put this node in list of all nodes (evio banks, segs, or tagsegs)
        // contained in this event.
        allNodes = new ArrayList<>(50);
        allNodes.add(this);
    }


    /**
     * Constructor which creates an EvioNode in the CompactEventBuilder.
     *
     * @param tag        the tag for the event (or bank) header.
   	 * @param num        the num for the event (or bank) header.
     * @param pos        position of event in buffer (bytes).
     * @param dataPos    position of event's data in buffer (bytes.)
     * @param type       the type of this evio structure.
     * @param dataType   the data type contained in this evio event.
     * @param buffer buffer containing this event.
     */
    EvioNode(int tag, int num, int pos, int dataPos,
             DataType type, DataType dataType,
             ByteBuffer buffer) {

        this.tag = tag;
        this.num = num;
        this.pos = pos;
        this.dataPos = dataPos;

        this.type = type.getValue();
        this.dataType = dataType.getValue();
        this.buffer = buffer;
    }

    //-------------------------------
    // Methods
    //-------------------------------

    /**
     * Shift the positions (pos, dataPos, and recordPos) of this node and its
     * children by a fixed amount.
     * Useful, for example, when the contents of one buffer is copied into another.
     *
     * @param deltaPos number of bytes to add to existing positions.
     * @return this object.
     */
    final public EvioNode shift(int deltaPos) {
        pos += deltaPos;
        dataPos += deltaPos;
        recordPos += deltaPos;

        if (childNodes != null) {
            for (EvioNode kid : childNodes) {
                kid.shift(deltaPos);
            }
        }
        return this;
    }

    final public Object clone() {
        try {
            EvioNode result = (EvioNode)super.clone();
            // Doing things this way means we don't have to
            // copy references to the node & buffer object.
            // However, we do need a new list ...
            result.childNodes = null;
            result.data = null;
            return result;
        }
        catch (CloneNotSupportedException ex) {
            return null;    // never invoked
        }
    }


    /**
     * Return a String representation of this object.
     * @return a String representation of this object.
     */
    final public String toString() {
        StringBuilder builder = new StringBuilder(100);
        builder.append("tag = ");          builder.append(tag);
        builder.append(", num = ");        builder.append(num);
        builder.append(", type = ");       builder.append(getTypeObj());
        builder.append(", dataType = ");   builder.append(getDataTypeObj());
        builder.append(", pos = ");        builder.append(pos);
        builder.append(", dataPos = ");    builder.append(dataPos);
        builder.append(", len = ");        builder.append(len);
        builder.append(", dataLen = ");    builder.append(dataLen);
        builder.append(", recordPos = ");  builder.append(recordPos);

        return builder.toString();
    }

    /**
     * Copy parameters from a parent node when scanning evio data and
     * placing into EvioNode obtained from an EvioNodeSource.
     * @param parent parent of the object.
     */
    final private void copyParentForScan(EvioNode parent) {
        blockNode  = parent.blockNode;
        buffer     = parent.buffer;
        allNodes   = parent.allNodes;
        eventNode  = parent.eventNode;
        place      = parent.place;
        scanned    = parent.scanned;
        recordPos  = parent.recordPos;
        parentNode = parent;
    }

 //TODO: figure out which clears are needed
    /**
     * Clear childNodes.
     * Place only this or eventNode object into the allNodes list if it exists.
     */
    final public void clearLists() {
        if (childNodes != null) childNodes.clear();

        // Should only be defined if this is an event (isEvent == true)
        if (allNodes != null) {
            allNodes.clear();
            // Remember to add event's node into list
            if (eventNode == null) {
                allNodes.add(this);
            }
            else {
                allNodes.add(eventNode);
            }
        }
    }


    /** Clear all data in this object. */
    final public void clear() {
        if (allNodes != null) allNodes.clear();
        len = tag = num = pad = pos = type = dataLen = dataPos = dataType = place = recordPos = 0;
        clearObjects();
    }


    /** Empty all lists and remove all other objects from this object. */
    final public void clearObjects() {
        if (childNodes != null) childNodes.clear();

        isEvent = obsolete = scanned = false;
        data       = null;
        blockNode  = null;
        buffer     = null;
        eventNode  = null;
        parentNode = null;
    }

    /** Clear everything in this object and set allNodes object to null. */
    final public void clearAll() {
        clear();
        allNodes = null;
    }

    /** Only clear the data array. */
    final public void clearIntArray() {data = null;}


    //-------------------------------
    // Setters & Getters
    //-------------------------------

    /**
     * Set the buffer.
     * @param buf buffer associated with this object.
     */
    public void setBuffer(ByteBuffer buf) {buffer = buf;}

    /**
     * Once this node is cleared, it may be reused and then re-initialized
     * with this method.
     *
     * @param position   position in buffer
     * @param place      place of event in buffer (starting at 0)
     * @param buffer buffer to examine
     * @param blockNode  object holding data about header of block containing event
     */
    void setData(int position, int place,
                 ByteBuffer buffer, BlockNode blockNode) {
        this.buffer    = buffer;
        this.blockNode = blockNode;
        this.pos       = position;
        this.place     = place;
        this.isEvent   = true;
        this.type      = DataType.BANK.getValue();
        // The only time there is no allNodes is internally to CompactEventBuilder
        // so we're ok.
        allNodes.add(this);
    }

    /**
     * Once this node is cleared, it may be reused and then re-initialized
     * with this method.
     *
     * @param position   position in buffer
     * @param place      place of event in buffer (starting at 0)
     * @param recordPos  place of event in containing record (bytes)
     * @param buffer buffer to examine
     */
    void setData(int position, int place, int recordPos, ByteBuffer buffer) {
        this.buffer     = buffer;
        this.recordPos  = recordPos;
        this.pos        = position;
        this.place      = place;
        this.isEvent    = true;
        this.type       = DataType.BANK.getValue();
        // The only time there is no allNodes is internally to CompactEventBuilder
        // so we're ok.
        allNodes.add(this);
    }

    //-------------------------------
    // Static Methods
    //-------------------------------

    /**
     * This method extracts an EvioNode object representing an
     * evio event (top level evio bank) from a given buffer, a
     * location in the buffer, and a few other things. An EvioNode
     * object represents an evio container - either a bank, segment,
     * or tag segment.
     *
     * @param buffer     buffer to examine
     * @param pool       pool of EvioNode objects
     * @param blockNode  object holding data about block header
     * @param position   position in buffer
     * @param place      place of event in buffer (starting at 0)
     *
     * @return EvioNode object containing evio event information
     * @throws EvioException if file/buffer too small
     */
    static final public EvioNode extractEventNode(ByteBuffer buffer,
                                                  EvioNodeSource pool,
                                                  BlockNode blockNode,
                                                  int position, int place)
            throws EvioException {

        // Make sure there is enough data to at least read evio header
        if (buffer.remaining() < 8) {
            throw new EvioException("buffer underflow");
        }

        // Store evio event info, without de-serializing, into EvioNode object
        EvioNode node;
        if (pool != null) {
            node = pool.getNode();
            node.clear(); //node.clearIntArray();
            node.setData(position, place, buffer, blockNode);
        }
        else {
            node = new EvioNode(position, place, buffer, blockNode);
        }

        return extractNode(node, position);
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
     * @throws EvioException if file/buffer too small
     */
    static final public EvioNode extractEventNode(ByteBuffer buffer, EvioNodeSource pool,
                                                  int recPosition, int position, int place)
            throws EvioException {

        // Make sure there is enough data to at least read evio header
        if (buffer.remaining() < 8) {
            throw new EvioException("buffer underflow");
        }

        // Store evio event info, without de-serializing, into EvioNode object
        EvioNode node;
        if (pool != null) {
            node = pool.getNode();
            node.clear(); //node.clearIntArray();
            node.setData(position, place, recPosition, buffer);
        }
        else {
            node = new EvioNode(position, place, recPosition, buffer);
        }

        return extractNode(node, position);
    }

    /**
     * This method populates an EvioNode object that will represent an
     * evio bank from that same node containing a reference to the
     * backing buffer and given a position in that buffer.
     *
     * @param bankNode   EvioNode to represent a bank and containing,
     *                   at least, a reference to backing buffer.
     * @param position   position in backing buffer
     *
     * @return EvioNode bankNode arg filled with appropriate data.
     * @throws EvioException if file/buffer too small
     */
    static final public EvioNode extractNode(EvioNode bankNode, int position)
            throws EvioException {

        // Make sure there is enough data to at least read evio header
        ByteBuffer buffer = bankNode.buffer;
        if (buffer.remaining() < 8) {
            throw new EvioException("buffer underflow");
        }

        // Get length of current bank
        int len = buffer.getInt(position);
        bankNode.len = len;
        bankNode.pos = position;
        bankNode.type = DataType.BANK.getValue();

        // Position of data for a bank
        bankNode.dataPos = position + 8;
        // Len of data for a bank
        bankNode.dataLen = len - 1;
//System.out.println("extractNode: len = " + len + ", pos = " + position +
//                ", dataPos = " + bankNode.dataPos + ", dataLen = " + bankNode.dataLen);

        // Make sure there is enough data to read full bank
        // even though it is NOT completely read at this time.
        if (buffer.remaining() < 4*(len + 1)) {
System.out.println("ERROR: remaining = " + buffer.remaining() +
            ", node len bytes = " + ( 4*(len + 1)) + ", len = " + len);
            throw new EvioException("buffer underflow");
        }

        // Hop over length word
        position += 4;

        // Read and parse second header word
        int word = buffer.getInt(position);
        bankNode.tag = (word >>> 16);
        int dt = (word >> 8) & 0xff;
        bankNode.dataType = dt & 0x3f;
        bankNode.pad = dt >>> 6;
        // If only 7th bit set, it can be tag=0, num=0, type=0, padding=1.
        // This regularly happens with composite data.
        // However, it that MAY also be the legacy tagsegment type
        // with no padding information. Ignore this as having tag & num
        // in legacy code is probably rare.
        //if (dt == 0x40) {
        //    bankNode.dataType = DataType.TAGSEGMENT.getValue();
        //    bankNode.pad = 0;
        //}
        bankNode.num = word & 0xff;
//System.out.println("extractNode: tag = 0x" + Integer.toHexString(bankNode.tag) +
//                   ", num = " + Integer.toHexString(bankNode.num) +
//                   ", data type = " + Integer.toHexString(bankNode.dataType));

        return bankNode;
    }

    /**
     * This method recursively stores, in the given list, all the information
     * about an evio structure's children found in the given ByteBuffer object.
     * It uses absolute gets so buffer's position does <b>not</b> change.
     *
     * @param node node being scanned
     */
    static final void scanStructure(EvioNode node) {

        int dType = node.dataType;

        // If node does not contain containers, return since we can't drill any further down
        if (!DataType.isStructure(dType)) {
            return;
        }

        // Start at beginning position of evio structure being scanned
        int position = node.dataPos;
        // Don't go past the data's end which is (position + length)
        // of evio structure being scanned in bytes.
        int endingPos = position + 4*node.dataLen;
        // Buffer we're using
        ByteBuffer buffer = node.buffer;

        int dt, dataType, dataLen, len, word;

        // Do something different depending on what node contains
        if (DataType.isBank(dType)) {
            // Extract all the banks from this bank of banks.
            // Make allowance for reading header (2 ints).
            endingPos -= 8;
            while (position <= endingPos) {

                // Cloning is a fast copy that eliminates the need
                // for setting stuff that's the same as the parent.
                EvioNode kidNode = (EvioNode) node.clone();

                // Read first header word
                len = buffer.getInt(position);
                kidNode.pos = position;

                // Len of data (no header) for a bank
                dataLen = len - 1;
                position += 4;

                // Read and parse second header word
                word = buffer.getInt(position);
                position += 4;
                kidNode.tag = (word >>> 16);
                dt = (word >> 8) & 0xff;
                dataType = dt & 0x3f;
                kidNode.pad = dt >>> 6;
                // If only 7th bit set, it can be tag=0, num=0, type=0, padding=1.
                // This regularly happens with composite data.
                // However, it that MAY also be the legacy tagsegment type
                // with no padding information. Ignore this as having tag & num
                // in legacy code is probably rare.
                //if (dt == 0x40) {
                //    dataType = DataType.TAGSEGMENT.getValue();
                //    kidNode.pad = 0;
                //}
                kidNode.num = word & 0xff;


                kidNode.len = len;
                kidNode.type = DataType.BANK.getValue();  // This is a bank
                kidNode.dataLen = dataLen;
                kidNode.dataPos = position;
                kidNode.dataType = dataType;
                kidNode.isEvent = false;

                // Create the tree structure
                kidNode.parentNode = node;
                // Add this to list of children and to list of all nodes in the event
                node.addChild(kidNode);

                // Only scan through this child if it's a container
                if (DataType.isStructure(dataType)) {
                    scanStructure(kidNode);
                }

                // Set position to start of next header (hop over kid's data)
                position += 4 * dataLen;
            }
        }
        else if (DataType.isSegment(dType)) {

            // Extract all the segments from this bank of segments.
            // Make allowance for reading header (1 int).
            endingPos -= 4;
            while (position <= endingPos) {
                EvioNode kidNode = (EvioNode) node.clone();

                kidNode.pos = position;

                word = buffer.getInt(position);
                position += 4;
                kidNode.tag = word >>> 24;
                dt = (word >>> 16) & 0xff;
                dataType = dt & 0x3f;
                kidNode.pad = dt >>> 6;
                // If only 7th bit set, it can be tag=0, num=0, type=0, padding=1.
                // This regularly happens with composite data.
                // However, it that MAY also be the legacy tagsegment type
                // with no padding information. Ignore this as having tag & num
                // in legacy code is probably rare.
                //if (dt == 0x40) {
                //    dataType = DataType.TAGSEGMENT.getValue();
                //    kidNode.pad = 0;
                //}
                len = word & 0xffff;


                kidNode.num      = 0;
                kidNode.len      = len;
                kidNode.type     = DataType.SEGMENT.getValue();  // This is a segment
                kidNode.dataLen  = len;
                kidNode.dataPos  = position;
                kidNode.dataType = dataType;
                kidNode.isEvent  = false;

                kidNode.parentNode = node;
                node.addChild(kidNode);

                if (DataType.isStructure(dataType)) {
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

                EvioNode kidNode = (EvioNode) node.clone();

                kidNode.pos = position;

                word = buffer.getInt(position);
                position += 4;
                kidNode.tag =  word >>> 20;
                dataType    = (word >>> 16) & 0xf;
                len         =  word & 0xffff;

                kidNode.pad      = 0;
                kidNode.num      = 0;
                kidNode.len      = len;
                kidNode.type     = DataType.TAGSEGMENT.getValue();  // This is a tag segment
                kidNode.dataLen  = len;
                kidNode.dataPos  = position;
                kidNode.dataType = dataType;
                kidNode.isEvent  = false;

                kidNode.parentNode = node;
                node.addChild(kidNode);

                if (DataType.isStructure(dataType)) {
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
    static final void scanStructure(EvioNode node, EvioNodeSource nodeSource) {

        int dType = node.dataType;

        // If node does not contain containers, return since we can't drill any further down
        if (!DataType.isStructure(dType)) {
            return;
        }

        // Start at beginning position of evio structure being scanned
        int position = node.dataPos;
        // Don't go past the data's end which is (position + length)
        // of evio structure being scanned in bytes.
        int endingPos = position + 4*node.dataLen;
        // Buffer we're using
        ByteBuffer buffer = node.buffer;

        int dt, dataType, dataLen, len, word;

        // Do something different depending on what node contains
        if (DataType.isBank(dType)) {
            // Extract all the banks from this bank of banks.
            // Make allowance for reading header (2 ints).
            endingPos -= 8;
            while (position <= endingPos) {

                // Cloning is a fast copy that eliminates the need
                // for setting stuff that's the same as the parent.

                //EvioNode kidNode = (EvioNode) node.clone();
                EvioNode kidNode = nodeSource.getNode();
                kidNode.copyParentForScan(node);

                // Read first header word
                len = buffer.getInt(position);
                kidNode.pos = position;

                // Len of data (no header) for a bank
                dataLen = len - 1;
                position += 4;

                // Read and parse second header word
                word = buffer.getInt(position);
                position += 4;
                kidNode.tag = (word >>> 16);
                dt = (word >> 8) & 0xff;
                dataType = dt & 0x3f;
                kidNode.pad = dt >>> 6;
                // If only 7th bit set, it can be tag=0, num=0, type=0, padding=1.
                // This regularly happens with composite data.
                // However, it that MAY also be the legacy tagsegment type
                // with no padding information. Ignore this as having tag & num
                // in legacy code is probably rare.
                //if (dt == 0x40) {
                //    dataType = DataType.TAGSEGMENT.getValue();
                //    kidNode.pad = 0;
                //}
                kidNode.num = word & 0xff;


                kidNode.len = len;
                kidNode.type = DataType.BANK.getValue();  // This is a bank
                kidNode.dataLen = dataLen;
                kidNode.dataPos = position;
                kidNode.dataType = dataType;
                kidNode.isEvent = false;

                // Add this to list of children and to list of all nodes in the event
                node.addChild(kidNode);

                // Only scan through this child if it's a container
                if (DataType.isStructure(dataType)) {
                    scanStructure(kidNode, nodeSource);
                }

                // Set position to start of next header (hop over kid's data)
                position += 4 * dataLen;
            }
        }
        else if (DataType.isSegment(dType)) {

            // Extract all the segments from this bank of segments.
            // Make allowance for reading header (1 int).
            endingPos -= 4;
            while (position <= endingPos) {
                //EvioNode kidNode = (EvioNode) node.clone();
                EvioNode kidNode = nodeSource.getNode();
                kidNode.copyParentForScan(node);

                kidNode.pos = position;

                word = buffer.getInt(position);
                position += 4;
                kidNode.tag = word >>> 24;
                dt = (word >>> 16) & 0xff;
                dataType = dt & 0x3f;
                kidNode.pad = dt >>> 6;
                // If only 7th bit set, it can be tag=0, num=0, type=0, padding=1.
                // This regularly happens with composite data.
                // However, it that MAY also be the legacy tagsegment type
                // with no padding information. Ignore this as having tag & num
                // in legacy code is probably rare.
                //if (dt == 0x40) {
                //    dataType = DataType.TAGSEGMENT.getValue();
                //    kidNode.pad = 0;
                //}
                len = word & 0xffff;


                kidNode.num      = 0;
                kidNode.len      = len;
                kidNode.type     = DataType.SEGMENT.getValue();  // This is a segment
                kidNode.dataLen  = len;
                kidNode.dataPos  = position;
                kidNode.dataType = dataType;
                kidNode.isEvent  = false;

                node.addChild(kidNode);

                if (DataType.isStructure(dataType)) {
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

                //EvioNode kidNode = (EvioNode) node.clone();
                EvioNode kidNode = nodeSource.getNode();
                kidNode.copyParentForScan(node);

                kidNode.pos = position;

                word = buffer.getInt(position);
                position += 4;
                kidNode.tag =  word >>> 20;
                dataType    = (word >>> 16) & 0xf;
                len         =  word & 0xffff;

                kidNode.pad      = 0;
                kidNode.num      = 0;
                kidNode.len      = len;
                kidNode.type     = DataType.TAGSEGMENT.getValue();  // This is a tag segment
                kidNode.dataLen  = len;
                kidNode.dataPos  = position;
                kidNode.dataType = dataType;
                kidNode.isEvent  = false;

                node.addChild(kidNode);

                if (DataType.isStructure(dataType)) {
                    scanStructure(kidNode, nodeSource);
                }

                position += 4*len;
            }
        }
    }

    //-------------------------------
    // End of static methods
    //-------------------------------

    /**
     * Add a node to the end of the list of all nodes contained in event.
     * @param node child node to add to the list of all nodes
     */
    final private void addToAllNodes(EvioNode node) {
        if (allNodes == null || node == null) {
            return;
        }

        allNodes.add(node);
        // NOTE: do not have to do this recursively for all descendants.
        // That's because when events are scanned and EvioNode objects are created,
        // they are done so by using clone(). This means that all descendants have
        // references to the event level node's allNodes object and not their own
        // complete copy.
    }

    /**
     * Remove a node & all of its descendants from the list of all nodes
     * contained in event.
     * @param node node & descendants to remove from the list of all nodes
     */
    final private void removeFromAllNodes(EvioNode node) {
        if (allNodes == null || node == null) {
            return;
        }

        allNodes.remove(node);

        // Remove descendants also
        if (node.childNodes != null) {
            for (EvioNode n : node.childNodes) {
                removeFromAllNodes(n);
            }
        }

        // NOTE: only one "allNodes" exists - at event/top level
    }

    /**
     * Add a child node to the end of the child list and
     * to the list of all nodes contained in event.
     * This is called internally in sequence so every node ends up in the right
     * place in allNodes. When the user adds a structure by calling
     * {@link EvioCompactReader#addStructure(int, ByteBuffer)}, the structure or node
     * gets added at the very end - as the last child of the event.
     *
     * @param node child node to add to the end of the child list.
     */
    final void addChild(EvioNode node) {
        if (node == null) {
            return;
        }

        if (childNodes == null) {
            childNodes = new ArrayList<>(50);
        }

        childNodes.add(node);

        if (allNodes != null) allNodes.add(node);
    }

    /**
     * Remove a node from this child list and, along with its descendants,
     * from the list of all nodes contained in event.
     * If not a child, do nothing.
     * @param node node to remove from child & allNodes lists.
     */
    final void removeChild(EvioNode node) {
        if (node == null) {
            return;
        }

        boolean isChild = false;

        if (childNodes != null) {
            isChild = childNodes.remove(node);
        }

        if (isChild) {
            removeFromAllNodes(node);
        }
    }

    /**
     * Get the object representing the block header.
     * @return object representing the block header.
     */
    final BlockNode getBlockNode() {
        return blockNode;
    }

    /**
     * Has the data this node represents in the buffer been removed?
     * @return true if node no longer represents valid buffer data, else false.
     */
    final public boolean isObsolete() {
        return obsolete;
    }

    /**
     * Set whether this node and descendants are now obsolete because the
     * data they represent in the buffer has been removed.
     * Only for internal use.
     * @param obsolete true if node and descendants no longer represent valid
     *                 buffer data, else false.
     */
    final public void setObsolete(boolean obsolete) {
        this.obsolete = obsolete;

        // Set for all descendants.
        if (childNodes != null) {
            for (EvioNode n : childNodes) {
                n.setObsolete(obsolete);
            }
        }
    }

    /**
     * Get the list of all nodes that this node contains,
     * always including itself. This is meaningful only if this
     * node has been scanned, otherwise it contains only itself.
     *
     * @return list of all nodes that this node contains; null if not top-level node
     */
    final public ArrayList<EvioNode> getAllNodes() {return allNodes;}

    /**
     * Get the list of all child nodes that this node contains.
     * This is meaningful only if this node has been scanned,
     * otherwise it is null.
     *
     * @return list of all child nodes that this node contains;
     *         null if not scanned or no children
     */
    final public ArrayList<EvioNode> getChildNodes() {
        return childNodes;
    }

    /**
     * Get the list of all descendant nodes that this node contains -
     * not only the immediate children.
     * This is meaningful only if this node has been scanned,
     * otherwise nothing is added to the given list.
     *
     * @param descendants list to be filled with EvioNodes of all descendants
     */
    final public void getAllDescendants(List<EvioNode> descendants) {
        if (childNodes == null || descendants == null) return;

        // Add children recursively
        for (EvioNode n : childNodes) {
            descendants.add(n);
            n.getAllDescendants(descendants);
        }
    }

    /**
     * Get the child node at the given index (starts at 0).
     * This is meaningful only if this node has been scanned,
     * otherwise it is null.
     *
     * @param index index of child node
     * @return child node at the given index;
     *         null if not scanned or no child at that index
     */
    final public EvioNode getChildAt(int index) {
        if ((childNodes == null) || (childNodes.size() < index+1)) return null;
        return childNodes.get(index);
    }

    /**
     * Get the number all children that this node contains.
     * This is meaningful only if this node has been scanned,
     * otherwise it returns 0.
     *
     * @return number of children that this node contains;
     *         0 if not scanned
     */
    final public int getChildCount() {
        if (childNodes == null) return 0;
        return childNodes.size();
    }

    /**
     * Get the object containing the buffer that this node is associated with.
     * @return object containing the buffer that this node is associated with.
     */
    final public ByteBuffer getBuffer() {
        return buffer;
    }

    /**
     * Get the length of this evio structure (not including length word itself)
     * in 32-bit words.
     * @return length of this evio structure (not including length word itself)
     *         in 32-bit words
     */
    final public int getLength() {
        return len;
    }

    /**
     * Get the length of this evio structure including entire header in bytes.
     * @return length of this evio structure including entire header in bytes.
     */
    final public int getTotalBytes() {
        return 4*dataLen + dataPos - pos;
    }

    /**
     * Get the tag of this evio structure.
     * @return tag of this evio structure
     */
    final public int getTag() {
        return tag;
    }

    /**
     * Get the num of this evio structure.
     * Will be zero for tagsegments.
     * @return num of this evio structure
     */
    final public int getNum() {
        return num;
    }

    /**
     * Get the padding of this evio structure.
     * Will be zero for segments and tagsegments.
     * @return padding of this evio structure
     */
    final public int getPad() {
        return pad;
    }

    /**
     * Get the file/buffer byte position of this evio structure.
     * @return file/buffer byte position of this evio structure
     */
    final public int getPosition() {return pos;}

    /**
     * Get the evio type of this evio structure, not what it contains.
     * Call {@link DataType#getDataType(int)} on the
     * returned value to get the object representation.
     * @return evio type of this evio structure, not what it contains
     */
    final public int getType() {return type;}

    /**
     * Get the evio type of this evio structure as an object.
     * @return evio type of this evio structure as an object.
     */
    final public DataType getTypeObj() {
        return DataType.getDataType(type);
    }

    /**
     * Get the length of this evio structure's data only (no header words)
     * in 32-bit words.
     * @return length of this evio structure's data only (no header words)
     *         in 32-bit words.
     */
    final public int getDataLength() {
        return dataLen;
    }

    /**
     * Get the file/buffer byte position of this evio structure's data.
     * @return file/buffer byte position of this evio structure's data
     */
    final public int getDataPosition() {
        return dataPos;
    }

    /**
     * Get the evio type of the data this evio structure contains.
     * Call {@link DataType#getDataType(int)} on the
     * returned value to get the object representation.
     * @return evio type of the data this evio structure contains
     */
    final public int getDataType() {
        return dataType;
    }

    /**
     * Get the evio type of the data this evio structure contains as an object.
     * @return evio type of the data this evio structure contains as an object.
     */
    final public DataType getDataTypeObj() {return DataType.getDataType(dataType);}

    /**
     * Get the file/buffer byte position of the record containing this node.
     * @since version 6.
     * @return file/buffer byte position of the record containing this node.
     */
    public int getRecordPosition() {return recordPos;}

    /**
     * Get the place of containing event in file/buffer. First event = 0, second = 1, etc.
     * Only for internal use.
     * @return place of containing event in file/buffer.
     */
    final public int getPlace() {return place;}

    /**
     * Get this node's parent node.
     * @return this node's parent node or null if none.
     */
    final public EvioNode getParentNode() {return parentNode;}

    /**
     * If this object represents an event (top-level, evio bank),
     * then returns its number (place in file or buffer) starting
     * with 1. If not, return -1.
     * @return event number if representing an event, else -1
     */
    final public int getEventNumber() {
        // TODO: This does not seems right as place defaults to a value of 0!!!
        return (place + 1);
    }


    /**
     * Does this object represent an event?
     * @return <code>true</code> if this object represents an event,
     *         else <code>false</code>
     */
    final public boolean isEvent() {
        return isEvent;
    }


    /**
     * Update the length of this node in the buffer and all its parent nodes as well.
     * For internal use only.
     * @param deltaLen change in length (words). Negative value reduces lengths.
     */
    final public void updateLengths(int deltaLen) {

        EvioNode node = this;
        DataType typ = getTypeObj();
        int length;

        switch (typ) {
            case BANK:
            case ALSOBANK:
                length = buffer.getInt(node.pos) + deltaLen;
                buffer.putInt(node.pos, length);
                break;

            case SEGMENT:
            case ALSOSEGMENT:
            case TAGSEGMENT:
                if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                    length = (buffer.getShort(node.pos+2) & 0xffff) + deltaLen;
                    buffer.putShort(node.pos+2, (short)length);
                }
                else {
                    length = (buffer.getShort(node.pos) & 0xffff) + deltaLen;
                    buffer.putShort(node.pos, (short)length);
                }
                break;

            default:
        }
    }


    /**
     * Update, in the buffer, the tag of the structure header this object represents.
     * Sometimes it's necessary to go back and change the tag of an evio
     * structure that's already been written. This will do that.
     *
     * @param newTag new tag value
     */
    final public void updateTag(int newTag) {

        switch (DataType.getDataType(type)) {
            case BANK:
            case ALSOBANK:
                if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                    buffer.putShort(pos+4, (short) newTag);
                }
                else {
                    buffer.putShort(pos+6, (short) newTag);
                }
                return;

            case SEGMENT:
            case ALSOSEGMENT:
                if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                    buffer.put(pos, (byte)newTag);
                }
                else {
                    buffer.put(pos+3, (byte)newTag);
                }
                return;

            case TAGSEGMENT:
                short compositeWord = (short) ((tag << 4) | (dataType & 0xf));
                if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                    buffer.putShort(pos, compositeWord);
                }
                else {
                    buffer.putShort(pos+2, compositeWord);
                }
                return;

            default:
        }
    }


    /**
     * Update, in the buffer, the num of the bank header this object represents.
     * Sometimes it's necessary to go back and change the num of an evio
     * structure that's already been written. This will do that
     * .
     * @param newNum new num value
     */
    final public void updateNum(int newNum) {

        switch (DataType.getDataType(type)) {
            case BANK:
            case ALSOBANK:
                if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                    buffer.put(pos+7, (byte) newNum);
                }
                else {
                    buffer.put(pos+4, (byte)newNum);
                }
                return;

            default:
        }
    }


    /**
     * Get the data associated with this node in ByteBuffer form.
     * Depending on the copy argument, the returned buffer will either be
     * a copy of or a view into this node's buffer.
     * Position and limit are set for reading.<p>
     * This method is not synchronized.
     *
     * @param copy if <code>true</code>, then return a copy as opposed to a
     *             view into this node's buffer.
     * @return ByteBuffer containing data.
     *         Position and limit are set for reading.
     */
    final public ByteBuffer getByteData(boolean copy) {

        // The tricky thing to keep in mind is that the buffer
        // which this node uses may also be used by other nodes.
        // That means setting its limit and position may interfere
        // with other operations being done to it.
        // So even though it is less efficient, use a duplicate of the
        // buffer which gives us our own limit and position.
        ByteOrder order    = buffer.order();
        ByteBuffer buffer2 = buffer.duplicate().order(order);
        buffer2.limit(dataPos + 4*dataLen - pad).position(dataPos);

        if (copy) {
            ByteBuffer newBuf = ByteBuffer.allocate(4*dataLen - pad).order(order);
            newBuf.put(buffer2);
            newBuf.flip();
            return newBuf;
        }

        //return  buffer2.slice().order(order);
        return  buffer2;
    }


    /**
     * Get the data associated with this node as an 32-bit integer array.
     * Store it for future 2nd gets (like in event builder).
     * If data is of a type less than 32 bits, the last int will be junk.
     * Call this to avoid calling {@link #getByteData(boolean)} and converting
     * it to an int array which involves creating additional objects and calling
     * additional methods.
     * This method is not synchronized.
     *
     * @return integer array containing data.
     */
    final public int[] getIntData() {
        if (data != null) {
            return data;
        }

        data = new int[dataLen];

        for (int i = dataPos; i < dataPos + 4*dataLen; i+= 4) {
            data[(i-dataPos)/4] = buffer.getInt(i);
        }
        return data;
    }


    /**
     * Get the data associated with this node as an 32-bit integer array.
     * Place data in the given array. Only allocate memory if array is too small.
     * If data is of a type less than 32 bits, the last int will be junk.
     * Call this to avoid calling {@link #getByteData(boolean)} and converting
     * it to an int array which involves creating additional objects and calling
     * additional methods.
     * This method is not synchronized.
     * This method is only used in Evio.java in emu package.
     *
     * @param intData integer array in which to store data.
     * @param length  set first element to contain number of valid array elements of the returned array.
     * @return integer array containing data, or null if length array arg is null or 0 length.
     */
    final public int[] getIntData(int[] intData, int[] length) {
        if (length == null || length.length < 1) return null;

        // If supplied array is null or too small, create one
        if (intData == null || intData.length < dataLen) {
            intData = new int[dataLen];
        }

        for (int i = dataPos; i < dataPos + 4*dataLen; i+= 4) {
            intData[(i-dataPos)/4] = buffer.getInt(i);
        }

        // Return number of valid array elements through this array
        length[0] = dataLen;

        // Return data here
        return intData;
    }

    /**
     * Get the data associated with this node as an 64-bit integer array.
     * If data is of a type less than 64 bits, the last long will be junk.
     * Make sure we don't try to read beyond the end.
     * Call this to avoid calling {@link #getByteData(boolean)} and converting
     * it to a long array which involves creating additional objects and calling
     * additional methods.
     * This method is not synchronized.
     *
     * @return long array containing data.
     */
    final public long[] getLongData() {
        int numLongs = dataLen/2;

        long[] data = new long[numLongs];

        for (int i = dataPos; i < dataPos + 8*numLongs; i+= 8) {
            data[(i-dataPos)/8] = buffer.getLong(i);
        }
        return data;
    }


    /**
     * Get the data associated with this node as an 64-bit integer array.
     * Place data in the given array. Only allocate memory if array is too small.
     * If data is of a type less than 64 bits, the last element may be junk.
     * Call this to avoid calling {@link #getByteData(boolean)} and converting
     * it to an long array which involves creating additional objects and calling
     * additional methods.
     * This method is not synchronized.
     *
     * @param longData long array in which to store data.
     * @param length   set first element to contain number of valid array elements of the returned array.
     * @return long array containing data, or null if length array arg is null or 0 length.
     */
    final public long[] getLongData(long[] longData, int[] length) {
        if (length == null || length.length < 1) return null;

        int numLongs = dataLen/2;

        // If supplied array is null or too small, create one
        if (longData == null || longData.length < numLongs) {
            longData = new long[numLongs];
        }

        for (int i = dataPos; i < dataPos + 8*numLongs; i+= 8) {
            longData[(i-dataPos)/8] = buffer.getLong(i);
        }

        // Return number of valid array elements through this array
        length[0] = numLongs;

        // Return data here
        return longData;
    }


    /**
     * Get the data associated with this node as an 16-bit integer array.
     * If data is of a type less than 16 bits, the last int will be junk.
     * Call this to avoid calling {@link #getByteData(boolean)} and converting
     * it to a short array which involves creating additional objects and calling
     * additional methods.
     * This method is not synchronized.
     *
     * @return short array containing data.
     */
    final public short[] getShortData() {
        int numShorts = 2*dataLen;
        short[] data = new short[numShorts];

        for (int i = dataPos; i < dataPos + 2*numShorts; i+= 2) {
            data[(i-dataPos)/2] = buffer.getShort(i);
        }
        return data;
    }


    /**
     * Get the data associated with this node as an 16-bit integer array.
     * Place data in the given array. Only allocate memory if array is too small.
     * If data is of a type less than 16 bits, the last element may be junk.
     * Call this to avoid calling {@link #getByteData(boolean)} and converting
     * it to an short array which involves creating additional objects and calling
     * additional methods.
     * This method is not synchronized.
     *
     * @param shortData short array in which to store data.
     * @param length    set first element to contain number of valid array elements of the returned array.
     * @return short array containing data, or null if length array arg is null or 0 length.
     */
    final public short[] getShortData(short[] shortData, int[] length) {
        if (length == null || length.length < 1) return null;

        int numShorts = 2*dataLen;

        // If supplied array is null or too small, create one
        if (shortData == null || shortData.length < numShorts) {
            shortData = new short[numShorts];
        }

        for (int i = dataPos; i < dataPos + 2*numShorts; i+= 2) {
            shortData[(i-dataPos)/2] = buffer.getShort(i);
        }

        // Return number of valid array elements through this array
        length[0] = numShorts;

        // Return data here
        return shortData;
    }


    /**
     * Get this node's entire evio structure in ByteBuffer form.
     * Depending on the copy argument, the returned buffer will either be
     * a copy of or a view into the data of this node's buffer.
     * Position and limit are set for reading.<p>
     * This method is not synchronized.
     *
     * @param copy if <code>true</code>, then return a copy as opposed to a
     *        view into this node's buffer.
     * @return ByteBuffer object containing evio structure's bytes.
     *         Position and limit are set for reading.
     */
    final public ByteBuffer getStructureBuffer(boolean copy)  {

        // The tricky thing to keep in mind is that the buffer
        // which this node uses may also be used by other nodes.
        // That means setting its limit and position may interfere
        // with other operations being done to it.
        // So even though it is less efficient, use a duplicate of the
        // buffer which gives us our own limit and position.

        ByteOrder order    = buffer.order();
        ByteBuffer buffer2 = buffer.duplicate().order(order);
        buffer2.limit(dataPos + 4*dataLen).position(pos);

        if (copy) {
            ByteBuffer newBuf = ByteBuffer.allocate(getTotalBytes()).order(order);
            newBuf.put(buffer2);
            newBuf.flip();
            return newBuf;
        }

        //return buffer2.slice().order(order);
        return buffer2;
    }


}
