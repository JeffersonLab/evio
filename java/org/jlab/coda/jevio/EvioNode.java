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

import com.sun.prism.PixelFormat;

import javax.xml.stream.XMLOutputFactory;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamWriter;
import java.io.StringWriter;
import java.nio.*;
import java.util.ArrayList;

/**
 * This class is used to store relevant info about an evio container
 * (bank, segment, or tag segment), without having
 * to de-serialize it into many objects and arrays.
 *
 * @author timmer
 * Date: 11/13/12
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

    //----------------------------------
    // Constructors (package accessible)
    //----------------------------------

    /** Constructor when fancy features not needed. */
    EvioNode() {
        // Put this node in list of all nodes (evio banks, segs, or tagsegs)
        // contained in this event.
        allNodes = new ArrayList<EvioNode>(5);
        allNodes.add(this);
    }

    /** Constructor used when swapping data. */
    EvioNode(EvioNode firstNode) {
        allNodes = new ArrayList<EvioNode>(5);
        allNodes.add(this);
        scanned = true;
        eventNode = firstNode;
    }


    /**
     * Constructor which creates an EvioNode associated with
     * an event (top level) evio container when parsing buffers
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
        allNodes = new ArrayList<EvioNode>(5);
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
     * @param bufferNode buffer containing this event.
     */
    EvioNode(int tag, int num, int pos, int dataPos,
             DataType type, DataType dataType,
             BufferNode bufferNode) {

        this.tag = tag;
        this.num = num;
        this.pos = pos;
        this.dataPos = dataPos;

        this.type = type.getValue();
        this.dataType = dataType.getValue();
        this.bufferNode = bufferNode;
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
                allNodes.add(this);
            }
            else {
                allNodes.add(eventNode);
            }
        }
    }

    //-------------------------------
    // Setters
    //-------------------------------

    /**
     * Add a child node to the end of the child list and
     * to the list of all events.
     * @param childNode child node to add to the end of the child list.
     */
    synchronized public void addChild(EvioNode childNode) {
        if (childNodes == null) {
            childNodes = new ArrayList<EvioNode>(100);
        }
        childNodes.add(childNode);

        if (allNodes != null) allNodes.add(childNode);
    }



//    /**
//     * Remove a node from this child list.
//     * @param childNode node to remove from child list.
//     */
//    synchronized public void removeChild(EvioNode childNode) {
//        if (childNodes != null) {
//            childNodes.remove(childNode);
//        }
//
//        if (allNodes != null) allNodes.remove(childNode);
//    }

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
     * Get the list of all nodes that this node contains,
     * always including itself. This is meaningful only if this
     * node has been scanned, otherwise it contains only itself.
     *
     * @return list of all nodes that this node contains; null if not top-level node
     */
    public ArrayList<EvioNode> getAllNodes() {return allNodes;}

    /**
     * Get the list of all child nodes that this node contains.
     * This is meaningful only if this node has been scanned,
     * otherwise it is null.
     *
     * @return list of all child nodes that this node contains;
     *         null if not scanned or no children
     */
    public ArrayList<EvioNode> getChildNodes() {
        return childNodes;
    }

    /**
     * Get the child node at the given index (starts at 0).
     * This is meaningful only if this node has been scanned,
     * otherwise it is null.
     *
     * @return child node at the given index;
     *         null if not scanned or no child at that index
     */
    public EvioNode getChildAt(int index) {
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
    public int getChildCount() {
        if (childNodes == null) return 0;
        return childNodes.size();
    }

    /**
     * Get the object containing the buffer that this node is associated with.
     * @return object containing the buffer that this node is associated with.
     */
    public BufferNode getBufferNode() {
        return bufferNode;
    }

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
     * @return <code>true</code> if this object represents an event,
     *         else <code>false</code>
     */
    public boolean isEvent() {
        return isEvent;
    }


    /**
     * Update, in the buffer, the tag of the structure header this object represents.
     * Sometimes it's necessary to go back and change the tag of an evio
     * structure that's already been written. This will do that.
     *
     * @param newTag new tag value
     */
    public void updateTag(int newTag) {

        ByteBuffer buffer = bufferNode.buffer;

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
    public void updateNum(int newNum) {

        ByteBuffer buffer = bufferNode.buffer;

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
    public ByteBuffer getByteData(boolean copy) {

        // The tricky thing to keep in mind is that the buffer
        // which this node uses may also be used by other nodes.
        // That means setting its limit and position may interfere
        // with other operations being done to it.
        // So even though it is less efficient, use a duplicate of the
        // buffer which gives us our own limit and position.
        ByteOrder order   = bufferNode.buffer.order();
        ByteBuffer buffer = bufferNode.buffer.duplicate().order(order);
        buffer.limit(dataPos + 4*dataLen - pad).position(dataPos);

        if (copy) {
            ByteBuffer newBuf = ByteBuffer.allocate(4*dataLen - pad).order(order);
            newBuf.put(buffer);
            newBuf.flip();
            return newBuf;
        }

        return  buffer.slice().order(order);
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
    public ByteBuffer getStructureBuffer(boolean copy)  {

        // The tricky thing to keep in mind is that the buffer
        // which this node uses may also be used by other nodes.
        // That means setting its limit and position may interfere
        // with other operations being done to it.
        // So even though it is less efficient, use a duplicate of the
        // buffer which gives us our own limit and position.

        ByteOrder order   = bufferNode.buffer.order();
        ByteBuffer buffer = bufferNode.buffer.duplicate().order(order);
        buffer.limit(dataPos + 4*dataLen).position(pos);

        if (copy) {
            ByteBuffer newBuf = ByteBuffer.allocate(getTotalBytes()).order(order);
            newBuf.put(buffer);
            newBuf.flip();
            return newBuf;
        }

        return buffer.slice().order(order);
    }


     /*---------------------------------------------------------------------------
     *  XML output
     *--------------------------------------------------------------------------*/

    /**
     * Increase the indentation of the given XML indentation.
     * @param xmlIndent String of spaces to increase.
     * @return increased indentation String
     */
    static private String increaseXmlIndent(String xmlIndent) {
        xmlIndent += "   ";
        return xmlIndent;
    }


    /**
     * Decrease the indentation of the given XML indentation.
     * @param xmlIndent String of spaces to increase.
     * @return Decreased indentation String
     */
    static private String decreaseXmlIndent(String xmlIndent) {
        xmlIndent = xmlIndent.substring(0, xmlIndent.length() - 3);
        return xmlIndent;
    }


    /**
     * All structures have a common start to their xml writing
     * @param xmlWriter the writer used to write the node.
     * @param xmlElementName name of xml element to start.
     * @param xmlIndent String of spaces.
     */
    static private void commonXMLStart(XMLStreamWriter xmlWriter, String xmlElementName,
                                       String xmlIndent) {
        try {
            xmlWriter.writeCharacters("\n");
            xmlWriter.writeCharacters(xmlIndent);
            xmlWriter.writeStartElement(xmlElementName);
        }
        catch (XMLStreamException e) {
            e.printStackTrace();
        }
    }


    /**
     * All structures close their xml writing.
     * @param xmlWriter the writer used to write the node.
     * @param xmlIndent String of spaces.
     */
    static private void commonXMLClose(XMLStreamWriter xmlWriter, String xmlIndent) {
        try {
            xmlWriter.writeCharacters("\n");
            xmlWriter.writeCharacters(xmlIndent);
            xmlWriter.writeEndElement();
        }
        catch (XMLStreamException e) {
            e.printStackTrace();
        }
    }


    /**
     * This method returns an XML element name given an evio data type.
     * @param type evio data type
     * @return XML element name used in evio event xml output
     */
    static private String getTypeName(DataType type) {

        if (type == null) return null;

        switch (type) {
            case CHAR8:
                return "int8";
            case UCHAR8:
                return "uint8";
            case SHORT16:
                return "int16";
            case USHORT16:
                return "uint16";
            case INT32:
                return "int32";
            case UINT32:
                return "uint32";
            case LONG64:
                return "int64";
            case ULONG64:
                return "uint64";
            case FLOAT32:
                return "float32";
            case DOUBLE64:
                return "float64";
            case CHARSTAR8:
                return "string";
            case COMPOSITE:
                return "composite";
            case UNKNOWN32:
                return "unknown32";

            case TAGSEGMENT:
                return "tagsegment";

            case SEGMENT:
            case ALSOSEGMENT:
                return "segment";

            case BANK:
            case ALSOBANK:
                return "bank";

            default:
                return "unknown";
        }
    }


    /**
     * This method converts this object into a readable, XML String.
     * @throws EvioException if node is not a bank or cannot parse node's buffer
     */
    public String toXML() throws EvioException {

        StringWriter sWriter = null;
        XMLStreamWriter xmlWriter = null;
        try {
            sWriter = new StringWriter();
            xmlWriter = XMLOutputFactory.newInstance().createXMLStreamWriter(sWriter);
            String xmlIndent = "";
            nodeToString(xmlIndent, xmlWriter);
            xmlWriter.flush();
        }
        catch (XMLStreamException e) {
            e.printStackTrace();
        }


        return sWriter.toString();
    }


    /**
     * This method converts this object into a readable, XML format String.
     *
     * @param xmlIndent String of spaces.
     * @param xmlWriter the writer used to write the node.
     */
    private void nodeToString(String xmlIndent, XMLStreamWriter xmlWriter) {

        DataType nodeTypeObj = getTypeObj();
        DataType dataTypeObj = getDataTypeObj();

        try {
            // If top level ...
            if (isEvent) {
                int totalLen = len + 1;
                xmlIndent = increaseXmlIndent(xmlIndent);
                xmlWriter.writeCharacters(xmlIndent);
                xmlWriter.writeComment(" Buffer " + getEventNumber() + " contains " +
                                        totalLen + " words (" + 4*totalLen + " bytes)");
                commonXMLStart(xmlWriter, "event", xmlIndent);
                xmlWriter.writeAttribute("format", "evio");
                xmlWriter.writeAttribute("count", "" + getEventNumber());
                if (dataTypeObj.isStructure()) {
                    xmlWriter.writeAttribute("content", getTypeName(dataTypeObj));
                }
                xmlWriter.writeAttribute("data_type", String.format("0x%x", dataType));
                xmlWriter.writeAttribute("tag", "" + tag);
                xmlWriter.writeAttribute("num", "" + num);
                xmlWriter.writeAttribute("length", "" + len);
                xmlIndent = increaseXmlIndent(xmlIndent);

                for (EvioNode n : childNodes) {
                    n.nodeToString(xmlIndent, xmlWriter);
                }

                xmlIndent = decreaseXmlIndent(xmlIndent);
                commonXMLClose(xmlWriter, xmlIndent);
                xmlWriter.writeCharacters("\n");
            }

            // If bank, segment, or tagsegment ...
            else if (dataTypeObj.isStructure()) {
                xmlWriter.writeCharacters("\n");
                xmlWriter.writeCharacters(xmlIndent);
                xmlWriter.writeStartElement(getTypeName(nodeTypeObj));

                if (dataTypeObj.isStructure()) {
                    xmlWriter.writeAttribute("content",  getTypeName(dataTypeObj));
                }
                xmlWriter.writeAttribute("data_type", String.format("0x%x", dataType));
                xmlWriter.writeAttribute("tag", "" + tag);
                if (nodeTypeObj == DataType.BANK || nodeTypeObj == DataType.ALSOBANK) {
                    xmlWriter.writeAttribute("num", "" + num);
                }
                xmlWriter.writeAttribute("length", "" + len);
                xmlWriter.writeAttribute("ndata", "" + getNumberDataItems());

                if (childNodes != null) {
                    xmlIndent = increaseXmlIndent(xmlIndent);

                    for (EvioNode n : childNodes) {
                        n.nodeToString(xmlIndent, xmlWriter);
                    }

                    xmlIndent = decreaseXmlIndent(xmlIndent);
                }
                commonXMLClose(xmlWriter, xmlIndent);
            }

            // If structure containing data ...
            else {
                int count;
                xmlWriter.writeCharacters("\n");
                xmlWriter.writeCharacters(xmlIndent);
                xmlWriter.writeStartElement(getTypeName(dataTypeObj));
                xmlWriter.writeAttribute("data_type", String.format("0x%x", dataType));
                xmlWriter.writeAttribute("tag", "" + tag);
                if (nodeTypeObj == DataType.BANK || nodeTypeObj == DataType.ALSOBANK) {
                    xmlWriter.writeAttribute("num", "" + num);
                }
                xmlWriter.writeAttribute("length", "" + len);
                count = getNumberDataItems();
                xmlWriter.writeAttribute("ndata", "" + count);

                xmlIndent = increaseXmlIndent(xmlIndent);
                writeXmlData(count, xmlIndent, xmlWriter);
                xmlIndent = decreaseXmlIndent(xmlIndent);

                commonXMLClose(xmlWriter, xmlIndent);
            }
        }
        catch (XMLStreamException e) {
            e.printStackTrace();
        }
    }


    /**
     * Get the number of stored data items like number of banks, ints, floats, etc.
     * (not the size in ints or bytes). Some items may be padded such as shorts
     * and bytes. This will tell the meaningful number of such data items.
     * In the case of containers, returns number of 32-bit words not in header.
     *
     * @return number of stored data items (not size or length),
     *         or number of bytes if container
     */
    private int getNumberDataItems() {
        int numberDataItems = 0;

        DataType type = getDataTypeObj();

        if (type.isStructure()) {
            numberDataItems = dataLen;
        }
        else {
            // We can figure out how many data items
            // based on the size of the raw data
            switch (type) {
                case UNKNOWN32:
                case CHAR8:
                case UCHAR8:
                    numberDataItems = (4*dataLen - pad);
                    break;
                case SHORT16:
                case USHORT16:
                    numberDataItems = (4*dataLen - pad)/2;
                    break;
                case INT32:
                case UINT32:
                case FLOAT32:
                    numberDataItems = dataLen;
                    break;
                case LONG64:
                case ULONG64:
                case DOUBLE64:
                    numberDataItems = dataLen/2;
                    break;
                case CHARSTAR8:
                    String[] s = BaseStructure.unpackRawBytesToStrings(
                                    bufferNode.buffer, dataPos, 4*dataLen);

                    if (s == null) {
                        numberDataItems = 0;
                        break;
                    }
                    numberDataItems = s.length;
                    break;
                case COMPOSITE:
                    // For this type, numberDataItems is NOT used to
                    // calculate the data length so we're OK returning
                    // any reasonable value here.
                    numberDataItems = 1;
                    CompositeData[] compositeData = null;
                    try {
                        // Data needs to be in a byte array
                        byte[] rawBytes = new byte[4*dataLen];
                        // Wrap array with ByteBuffer for easy transfer of data
                        ByteBuffer rawBuffer = ByteBuffer.wrap(rawBytes);

                        // The node's backing buffer may or may not have a backing array.
                        // Just assume it doesn't and proceed from there.
                        ByteBuffer buf = getByteData(true);

                        // Write data into array
                        rawBuffer.put(buf).order(buf.order());

                        compositeData = CompositeData.parse(rawBytes, rawBuffer.order());
                    }
                    catch (EvioException e) {
                        e.printStackTrace();
                    }
                    if (compositeData != null) numberDataItems = compositeData.length;
                    break;
                default:
            }
        }

        return numberDataItems;
    }


    /**
  	 * Write data as XML output for EvioNode.
     * @param count number of data items (shorts, longs, doubles, etc.) in this node
     *              (not children)
     * @param xmlIndent String of spaces.
     * @param xmlWriter the writer used to write the node.
  	 */
  	private void writeXmlData(int count, String xmlIndent, XMLStreamWriter xmlWriter) {

  		// Only leaves write data
        DataType dataTypeObj = getDataTypeObj();

  		if (dataTypeObj.isStructure()) {
  			return;
  		}

        try {
            String s;
            String indent = String.format("\n%s", xmlIndent);

            ByteBuffer buf = getByteData(true);

  			switch (dataTypeObj) {
  			case DOUBLE64:
  				DoubleBuffer dbuf = buf.asDoubleBuffer();
                for (int i=0; i < count; i++) {
                    if (i%2 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }
                    s = String.format("%24.16e  ", dbuf.get(i));
                    xmlWriter.writeCharacters(s);
                }
  				break;

  			case FLOAT32:
                FloatBuffer fbuf = buf.asFloatBuffer();
                for (int i=0; i < count; i++) {
                    if (i%2 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }
                    s = String.format("%14.7e  ", fbuf.get(i));
                    xmlWriter.writeCharacters(s);
                }
  				break;

  			case LONG64:
  			case ULONG64:
                LongBuffer lbuf = buf.asLongBuffer();
                for (int i=0; i < count; i++) {
                    if (i%2 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }
                    s = String.format("%20d  ", lbuf.get(i));
                    xmlWriter.writeCharacters(s);
                }
  				break;

  			case INT32:
  			case UINT32:
                IntBuffer ibuf = buf.asIntBuffer();
                for (int i=0; i < count; i++) {
                    if (i%5 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }
                    s = String.format("%11d  ", ibuf.get(i));
                    xmlWriter.writeCharacters(s);
                }
  				break;

  			case SHORT16:
  			case USHORT16:
                ShortBuffer sbuf = buf.asShortBuffer();
                for (int i=0; i < count; i++) {
                    if (i%5 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }
                    s = String.format("%6d  ", sbuf.get(i));
                    xmlWriter.writeCharacters(s);
                }
  				break;

            case UNKNOWN32:
  			case CHAR8:
  			case UCHAR8:
                for (int i=0; i < count; i++) {
                    if (i%5 == 0) {
                        xmlWriter.writeCharacters(indent);
                    }
                    s = String.format("%4d  ", buf.get(i));
                    xmlWriter.writeCharacters(s);
                }
  				break;

  			case CHARSTAR8:
                String[] stringdata = BaseStructure.unpackRawBytesToStrings(
                        bufferNode.buffer, dataPos, 4*dataLen);

                if (stringdata == null) {
                    break;
                }

                for (String str : stringdata) {
                    xmlWriter.writeCharacters("\n");
                    xmlWriter.writeCData(str);
                }
  				break;

              case COMPOSITE:
                  CompositeData[] compositeData;
                  // Data needs to be in a byte array
                  byte[] rawBytes = new byte[4*dataLen];
                  // Wrap array with ByteBuffer for easy transfer of data
                  ByteBuffer rawBuffer = ByteBuffer.wrap(rawBytes);

                  // Write data into array
                  rawBuffer.put(buf).order(buf.order());

                  compositeData = CompositeData.parse(rawBytes, buf.order());
                  if (compositeData != null) {
                      for (CompositeData cd : compositeData) {
                          cd.toXML(xmlWriter, xmlIndent, true);
                      }
                  }
                  break;

              default:
              }
  		}
  		catch (Exception e) {
  			e.printStackTrace();
  		}
  	}



}
