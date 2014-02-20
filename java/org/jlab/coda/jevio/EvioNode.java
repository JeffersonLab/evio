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

import java.util.LinkedList;

/**
 * This class is used to store relevant info about an evio container
 * (bank, segment, or tag segment), without having
 * to de-serialize it into many objects and arrays.
 *
 * @author timmer
 * Date: 11/13/12
 * Time: 11:06 AM
 */
public final class EvioNode implements Cloneable {

    /** Header's length value (32-bit words). */
    int len;
    /** Header's tag value. */
    int tag;
    /** Header's num value. */
    int num;
    /** Header's padding value. */
    int pad;
    /** Position of header in file/buffer in bytes.  */
    int pos;
    /** This node's (evio container's) type. Must be bank, segment, or tag segment. */
    int type;

    /** Length of node's data in 32-bit words. */
    int dataLen;
    /** Position of node's data in file/buffer in bytes. */
    int dataPos;
    /** Type of data stored in node. */
    int dataType;

    /** Does this node represent an event (top-level bank)? */
    boolean isEvent;

    /** Block containing this node. */
    BlockNode blockNode;

    /** ByteBuffer that this node is associated with. */
    BufferNode bufferNode;

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

    /** List of all nodes in the event including this (top level) object. */
    LinkedList<EvioNode> allNodes;

    //-------------------------------
    // For sub event-level node
    //-------------------------------

    /** Node of event containing this node. Is null if this is an event node. */
    EvioNode eventNode;

    /** Node containing this node. Is null if this is an event node. */
    EvioNode parentNode;

    /** List of child nodes ordered according to placement in buffer. */
    LinkedList<EvioNode> childNodes;

    //----------------------------------
    // Constructors (package accessible)
    //----------------------------------

    /** Constructor when fancy features not needed. */
    EvioNode() {}


    /**
     * Constructor which creates an EvioNode associated with
     * an event (top) from scratch in the CompactBuilder.
     *
     * @param tag      the tag for the event (or bank) header.
   	 * @param dataType the data type contained in the event.
   	 * @param num      the num for the event (or bank) header.
     */
    EvioNode(int tag, DataType dataType, int num) {
        this.tag = tag;
        this.num = num;
        this.dataType = dataType.getValue();

        // Put this node in list of all nodes (evio banks, segs, or tagsegs)
        // contained in this event.
        allNodes = new LinkedList<EvioNode>();
        allNodes.addFirst(this);
    }


    /**
     * Constructor which creates an EvioNode associated with
     * an event (top) level evio container when parsing buffers
     * for evio data.
     *
     * @param pos        position of event in buffer (number of bytes)
     * @param place      containing event's place in buffer (starting at 1)
     * @param bufferNode buffer containing this event
     * @param blockNode  block containing this event
     */
    EvioNode(int pos, int place, BufferNode bufferNode, BlockNode blockNode) {
        this.pos = pos;
        this.place = place;
        this.blockNode = blockNode;
        this.bufferNode = bufferNode;
        // This is an event by definition
        this.isEvent = true;
        // Event is a Bank by definition
        this.type = DataType.BANK.getValue();

        // Put this node in list of all nodes (evio banks, segs, or tagsegs)
        // contained in this event.
        allNodes = new LinkedList<EvioNode>();
        allNodes.addFirst(this);
    }

    //-------------------------------
    // Methods
    //-------------------------------

    public Object clone() {
        try {
            EvioNode result = (EvioNode)super.clone();
            // Doing things this way means we don't have to
            // copy references to the node & buffer object.
            // However, we do need a new list ...
            result.childNodes = null;
            return result;
        }
        catch (CloneNotSupportedException ex) {
            return null;    // never invoked
        }
    }

    public String toString() {
        StringBuilder builder = new StringBuilder(100);
        builder.append("tag = ");   builder.append(tag);
        builder.append(", num = "); builder.append(num);
        builder.append(", type = "); builder.append(type);
        builder.append(", dataType = "); builder.append(dataType);
        builder.append(", pos = "); builder.append(pos);
        builder.append(", dataPos = "); builder.append(dataPos);
        builder.append(", len = "); builder.append(len);
        builder.append(", dataLen = "); builder.append(dataLen);

        return builder.toString();
    }


    public void clearLists() {
        if (childNodes != null) childNodes.clear();

        // Should only be defined if this is an event (isEvent == true)
        if (allNodes != null) {
            allNodes.clear();
            // Remember to add event's node into list
            if (eventNode == null) {
                allNodes.addFirst(this);
            }
            else {
                allNodes.addFirst(eventNode);
            }
        }
    }

    //-------------------------------
    // Setters
    //-------------------------------

    /**
     * Add a child node to the end of the child list.
     * @param childNode child node to add to the end of the child list.
     */
    synchronized public void addChild(EvioNode childNode) {
        if (childNodes == null) {
            childNodes = new LinkedList<EvioNode>();
        }
        childNodes.addLast(childNode);

        if (eventNode != null && allNodes != null) allNodes.addLast(childNode);
    }

    /**
     * Remove a node from this child list.
     * @param childNode node to remove from child list.
     */
    synchronized public void removeChild(EvioNode childNode) {
        if (childNodes != null) {
            childNodes.remove(childNode);
        }

        if (eventNode != null && allNodes != null) allNodes.remove(childNode);
    }

    //-------------------------------
    // Getters
    //-------------------------------

//    /**
//     * Get the object representing the block header.
//     * @return object representing the block header.
//     */
//    public BlockNode getBlockNode() {
//        return blockNode;
//    }

    /**
     * Get the length of this evio structure (not including length word itself)
     * in 32-bit words.
     * @return length of this evio structure (not including length word itself)
     *         in 32-bit words
     */
    public int getLength() {
        return len;
    }

    /**
     * Get the length of this evio structure including entire header in bytes.
     * @return length of this evio structure including entire header in bytes.
     */
    public int getTotalBytes() {
        return 4*dataLen + dataPos - pos;
    }

    /**
     * Get the tag of this evio structure.
     * @return tag of this evio structure
     */
    public int getTag() {
        return tag;
    }

    /**
     * Get the num of this evio structure.
     * Will be zero for tagsegments.
     * @return num of this evio structure
     */
    public int getNum() {
        return num;
    }

    /**
     * Get the padding of this evio structure.
     * Will be zero for segments and tagsegments.
     * @return padding of this evio structure
     */
    public int getPad() {
        return pad;
    }

    /**
     * Get the file/buffer byte position of this evio structure.
     * @return file/buffer byte position of this evio structure
     */
    public int getPosition() {
        return pos;
    }

    /**
     * Get the evio type of this evio structure, not what it contains.
     * Call {@link DataType#getDataType(int)} on the
     * returned value to get the object representation.
     * @return evio type of this evio structure, not what it contains
     */
    public int getType() {return type;}

    /**
     * Get the evio type of this evio structure as an object.
     * @return evio type of this evio structure as an object.
     */
    public DataType getTypeObj() {
        return DataType.getDataType(type);
    }

    /**
     * Get the length of this evio structure's data only (no header words)
     * in 32-bit words.
     * @return length of this evio structure's data only (no header words)
     *         in 32-bit words.
     */
    public int getDataLength() {
        return dataLen;
    }

    /**
     * Get the file/buffer byte position of this evio structure's data.
     * @return file/buffer byte position of this evio structure's data
     */
    public int getDataPosition() {
        return dataPos;
    }

    /**
     * Get the evio type of the data this evio structure contains.
     * Call {@link DataType#getDataType(int)} on the
     * returned value to get the object representation.
     * @return evio type of the data this evio structure contains
     */
    public int getDataType() {
        return dataType;
    }

    /**
     * Get the evio type of the data this evio structure contains as an object.
     * @return evio type of the data this evio structure contains as an object.
     */
    public DataType getDataTypeObj() {
        return DataType.getDataType(dataType);
    }

    /**
     * If this object represents an event (top-level, evio bank),
     * then returns its number (place in file or buffer) starting
     * with 1. If not, return -1.
     * @return event number if representing an event, else -1
     */
    public int getEventNumber() {
        // TODO: This does not seems right as place defaults to a value of 0!!!
        return (place + 1);
    }

    /**
     * Does this object represent an event?
     * @return <code>true</code> if this object represents and event,
     *         else <code>false</code>
     */
    public boolean isEvent() {
        return isEvent;
    }



}
