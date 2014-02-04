package org.jlab.coda.jevio;

import java.nio.ByteBuffer;
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
public final class EvioNode {

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
    ByteBuffer buffer;

    //-------------------------------
    // For event-level node
    //-------------------------------

    /**
     * Place in file/buffer. First event = 0, second = 1, etc.
     * Useful for converting node to EvioEvent object (de-serializing).
     */
    int place;

    /**
     * If top-level event node, was I scanned and all my banks
     * already placed into a list?
     */
    boolean scanned;

    //-------------------------------
    // For sub event-level node
    //-------------------------------

    /** Node of event containing this node. Is null if this is an event node. */
    EvioNode eventNode;

    /** Node containing this node. Is null if this is an event node. */
    EvioNode parentNode;

    /** List of child nodes ordered according to placement in buffer. */
    LinkedList<EvioNode> childNodes;

    //-------------------------------
    // Methods
    //-------------------------------

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

    public void clear() {
        len = 0;
        tag = 0;
        num = 0;
        pad = 0;
        pos = 0;
        type = 0;
        place = 0;
        dataLen = 0;
        dataPos = 0;
        dataType = 0;

        isEvent   = false;
        scanned   = false;
        buffer    = null;
        blockNode = null;
        eventNode = null;
        if (childNodes != null) childNodes.clear();
    }

    //-------------------------------
    // Getters
    //-------------------------------

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
    public int getType() {
        return type;
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
