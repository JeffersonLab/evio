package org.jlab.coda.jevio;

import java.nio.*;
import java.util.*;

/**
 * This class is used to read the bytes of just an evio structure (<b>NOT</b>
 * a full evio version 4 formatted file or buffer). It is also capable of
 * adding another structure to or removing it from that structure.
 * The {@link EvioCompactReader} class is similar but reads files and buffers
 * in the complete evio version 4 format.
 * It is theoretically thread-safe.
 * It is designed to be fast and does <b>NOT</b> do a deserialization
 * on the buffer examined.<p>
 *
 * @author timmer
 */
public class EvioCompactStructureHandler {

    /** Stores structure info. */
    private EvioNode node;

    /** The buffer being read. */
    private ByteBuffer byteBuffer;

    /** The byte order of the buffer being read. */
    private ByteOrder byteOrder;

    /** Is this object currently closed? */
    private boolean closed;


    //------------------------


    /**
     * Constructor for reading an EvioNode object.
     * The data represented by the given EvioNode object will be copied to a new
     * buffer (obtainable by calling {@link #getByteBuffer()}) and the node and
     * all of its descendants will switch to that new buffer.
     *
     * @param node the node to be analyzed.
     * @throws EvioException if node arg is null
     */
    public EvioCompactStructureHandler(EvioNode node) throws EvioException {

        if (node == null) {
            throw new EvioException("node arg is null");
        }

        // Node's backing buffer
        ByteBuffer byteBuffer = node.getBufferNode().getBuffer();

        // Duplicate backing buf cause we'll be changing pos & lim
        // and don't want to mess it up for someone else.
        byteOrder = byteBuffer.order();
        this.byteBuffer = byteBuffer.duplicate().order(byteOrder);

        // Copy data into a new internal buffer to allow modifications
        // to it without affecting the original buffer.
        bufferInit(node);
    }


    /**
     * Constructor for reading a buffer that contains 1 structure only (no block headers).
     * The data in the given ByteBuffer object will be copied to a new
     * buffer (obtainable by calling {@link #getByteBuffer()}).
     *
     * @param byteBuffer the buffer to be read that contains 1 structure only (no block headers).
     * @param type the type of outermost structure contained in buffer, may be {@link DataType#BANK},
     *             {@link DataType#SEGMENT}, {@link DataType#TAGSEGMENT} or equivalent.
     * @throws EvioException if byteBuffer arg is null;
     *                       if type arg is null or is not an evio structure;
     *                       if byteBuffer not in proper format;
     */
    public EvioCompactStructureHandler(ByteBuffer byteBuffer, DataType type) throws EvioException {
        setBuffer(byteBuffer, type);
    }


    /**
     * This method can be used to avoid creating additional EvioCompactEventReader
     * objects by reusing this one with another buffer.
     *
     * @param buf the buffer to be read that contains 1 structure only (no block headers).
     * @param type the type of outermost structure contained in buffer, may be {@link DataType#BANK},
     *             {@link DataType#SEGMENT}, {@link DataType#TAGSEGMENT} or equivalent.
     * @throws EvioException if buf arg is null;
     *                       if type arg is null or is not an evio structure;
     *                       if buf not in proper format;
     */
    public synchronized void setBuffer(ByteBuffer buf, DataType type) throws EvioException {

        if (buf == null) {
            throw new EvioException("buffer arg is null");
        }

        if (type == null || !type.isStructure()) {
            throw new EvioException("type arg is null or is not an evio structure");
        }

        if (buf.remaining() < 1) {
            throw new EvioException("buffer has too little data");
        }
        else if ( (type == DataType.BANK || type == DataType.ALSOBANK) &&
                   buf.remaining() < 2 ) {
            throw new EvioException("buffer has too little data");
        }

        closed = true;

        // Duplicate buf cause we'll be changing pos & lim and
        // don't want to mess up the original for someone else.
        byteOrder  = buf.order();
        byteBuffer = buf.duplicate().order(byteOrder);

        // Pull out all header info & place into EvioNode object
        node = extractNode(byteBuffer, null, null, type, byteBuffer.position(), 0, true);

        // See if the length given in header is consistent with buffer size
        if (node.len + 1 > byteBuffer.remaining()/4) {
            throw new EvioException("buffer has too little data");
        }

        // Copy data into a new internal buffer to allow modifications
        // to it without affecting the original buffer.
        bufferInit(node);

        closed = false;
    }


    /**
     * This method takes the EvioNode object representing the evio data of interest
     * and uses it to copy that data into a new local buffer. This allows changes to
     * be made to the data without affecting the original buffer. The new buffer is
     * 25% larger to accommodate any future additions to the data.
     *
     * @param node the node representing evio data.
     */
    private void bufferInit(EvioNode node) {

        // Position of evio structure in byteBuffer
        int endPos   = node.dataPos + 4*node.dataLen;
        int startPos = node.pos;

        // Copy data so we're not tied to byteBuffer arg.
        // Since this handler is often used to add banks to the node structure,
        // make the new buffer 25% bigger in anticipation.
        ByteBuffer newBuffer = ByteBuffer.allocate(5*(endPos - startPos)/4).order(byteOrder);

        // Copy evio data into new buffer
        byteBuffer.limit(endPos).position(startPos);
        newBuffer.put(byteBuffer);
        newBuffer.position(0).limit((endPos - startPos));

        // Update node & descendants

        // Can call the top node an "event", even if it isn't a bank,
        // and things should still work fine.
        node.isEvent    = true;
        node.allNodes   = new ArrayList<>();
        node.allNodes.add(node);
        node.parentNode = null;
        node.dataPos   -= startPos;
        node.pos       -= startPos;
        node.bufferNode.buffer = newBuffer;

        // Doing scan after above adjustments
        // will make other nodes come out right.
        scanStructure(node);
        node.scanned = true;

        byteBuffer = newBuffer;
        this.node = node;
    }


    /**
     * This method expands the data buffer, copies the data into it,
     * and updates all associated EvioNode objects.
     *
     * @param byteSize the number of bytes needed in new buffer.
     */
    private void expandBuffer(int byteSize) {
        // Since private method, forget about bounds checking of arg

        // Since this handler is often used to add banks to the node structure,
        // make the new buffer 25% bigger in anticipation.
        ByteBuffer newBuffer = ByteBuffer.allocate(5*byteSize/4).order(byteOrder);

        // Ending position of data in byteBuffer
        int endPos = node.dataPos + 4*node.dataLen;

        // Copy existing data into new buffer
        byteBuffer.position(0).limit(endPos);
        newBuffer.put(byteBuffer);
        newBuffer.position(0).limit(endPos);

        // Update node & descendants
        for (EvioNode n : node.allNodes) {
            // Using a new buffer now
            n.bufferNode.buffer = newBuffer;
        }

        byteBuffer = newBuffer;
    }



    /**
     * Has {@link #close()} been called (without reopening by calling
     * {@link #setBuffer(java.nio.ByteBuffer, DataType)})?
     *
     * @return {@code true} if this object closed, else {@code false}.
     */
    public synchronized boolean isClosed() {return closed;}


    /**
     * Get the byte buffer being read.
     * @return the byte buffer being read.
     */
    public ByteBuffer getByteBuffer() {
        return byteBuffer;
    }


    /**
     * Get the EvioNode object associated with the structure.
     * @return EvioNode object associated with the structure.
     */
    public EvioNode getStructure() {
        return node;
    }

    /**
     * Get the EvioNode object associated with a particular event number
     * which has been scanned so all substructures are contained in the
     * node.allNodes list.
     * @return  EvioNode object associated with a particular event number,
     *
     *         or null if there is none.
     */
    public EvioNode getScannedStructure() {
        scanStructure(node);
        return node;
    }


    /**
     * This method extracts an EvioNode object from a given buffer, a
     * location in the buffer, and a few other things. An EvioNode
     * object represents an evio container - either a bank, segment,
     * or tag segment.
     *
     * @param buffer    buffer to examine
     * @param blockNode object holding data about block header
     * @param eventNode object holding data about containing event (null if isEvent = true)
     * @param type      type of evio structure to extract
     * @param position  position in buffer
     * @param place     place of event in buffer (starting at 0)
     * @param isEvent   is the node being extracted an event (top-level bank)?
     *
     * @return          EvioNode object containing evio structure information
     * @throws          EvioException if file/buffer not in evio format
     */
    static private EvioNode extractNode(ByteBuffer buffer, BlockNode blockNode,
                                 EvioNode eventNode, DataType type,
                                 int position, int place, boolean isEvent)
            throws EvioException {

        // Store current evio info without de-serializing
        EvioNode node  = new EvioNode();
        node.pos        = position;
        node.place      = place;      // Which # event from beginning am I?
        node.blockNode  = blockNode;  // Block associated with this event, always null
        node.eventNode  = eventNode;
        node.isEvent    = isEvent;
        node.type       = type.getValue();
        node.bufferNode = new BufferNode(buffer);
        if (eventNode != null) node.allNodes = eventNode.allNodes;

        try {

            switch (type) {

                case BANK:
                case ALSOBANK:

                    // Get length of current event
                    node.len = buffer.getInt(position);
                    // Position of data for a bank
                    node.dataPos = position + 8;
                    // Len of data for a bank
                    node.dataLen = node.len - 1;

                    // Hop over length word
                    position += 4;

                    // Read and parse second header word
                    int word = buffer.getInt(position);
                    node.tag = (word >>> 16);
                    int dt = (word >> 8) & 0xff;
                    node.dataType = dt & 0x3f;
                    node.pad = dt >>> 6;
                    // If only 7th bit set, that can only be the legacy tagsegment type
                    // with no padding information - convert it properly.
                    if (dt == 0x40) {
                        node.dataType = DataType.TAGSEGMENT.getValue();
                        node.pad = 0;
                    }
                    node.num = word & 0xff;

                    break;

                case SEGMENT:
                case ALSOSEGMENT:

                    node.dataPos = position + 4;

                    word = buffer.getInt(position);
                    node.tag = word >>> 24;
                    dt = (word >>> 16) & 0xff;
                    node.dataType = dt & 0x3f;
                    node.pad = dt >>> 6;
                    // If only 7th bit set, that can only be the legacy tagsegment type
                    // with no padding information - convert it properly.
                    if (dt == 0x40) {
                        node.dataType = DataType.TAGSEGMENT.getValue();
                        node.pad = 0;
                    }
                    node.len = word & 0xffff;
                    node.dataLen = node.len;

                    break;

                case TAGSEGMENT:

                    node.dataPos = position + 4;

                    word = buffer.getInt(position);
                    node.tag      = word >>> 20;
                    node.dataType = (word >>> 16) & 0xf;
                    node.len      = word & 0xffff;
                    node.dataLen  = node.len;

                    break;

                default:
                    throw new EvioException("File/buffer bad format");
            }
        }
        catch (BufferUnderflowException a) {
            throw new EvioException("File/buffer bad format");
        }

        return node;
    }


    /**
     * This method recursively stores, in the given list, all the information
     * about an evio structure's children found in the given ByteBuffer object.
     * It uses absolute gets so buffer's position does <b>not</b> change.
     *
     * @param node node being scanned
     */
    static private void scanStructure(EvioNode node) {

        // Type of evio structure being scanned
        DataType type = node.getDataTypeObj();

        // If node does not contain containers, return since we can't drill any further down
        if (!type.isStructure()) {
//System.out.println("scanStructure: NODE is not a structure, return");
            return;
        }
 //System.out.println("scanStructure: scanning evio struct with len = " + node.dataLen);
 //System.out.println("scanStructure: data type of node to be scanned = " + type);

        // Start at beginning position of evio structure being scanned
        int position = node.dataPos;
        // Length of evio structure being scanned in bytes
        int dataBytes = 4*node.dataLen;
        // Don't go past the data's end
        int endingPos = position + dataBytes;
        // Buffer we're using
        ByteBuffer buffer = node.bufferNode.buffer;

        int dt, dataType, dataLen, len, pad, tag, num, word;

        // Do something different depending on what node contains
        switch (type) {
            case BANK:
            case ALSOBANK:

                // Extract all the banks from this bank of banks.
                while (position < endingPos) {
//System.out.println("scanStructure: start at pos " + position + " < end " + endingPos + " ?");

//System.out.println("scanStructure: buf is at pos " + buffer.position() +
//                   ", limit =  " + buffer.limit() + ", remaining = " + buffer.remaining() +
//                   ", capacity = " + buffer.capacity());

                    // Read first header word
                    len = buffer.getInt(position);
                    dataLen = len - 1; // Len of data (no header) for a bank
                    position += 4;

                    // Read and parse second header word
                    word = buffer.getInt(position);
                    position += 4;
                    tag = (word >>> 16);
                    dt = (word >> 8) & 0xff;
                    dataType = dt & 0x3f;
                    pad = dt >>> 6;
                    // If only 7th bit set, that can only be the legacy tagsegment type
                    // with no padding information - convert it properly.
                    if (dt == 0x40) {
                        dataType = DataType.TAGSEGMENT.getValue();
                        pad = 0;
                    }
                    num = word & 0xff;

                    // Cloning is a fast copy that eliminates the need
                    // for setting stuff that's the same as the parent.
                    EvioNode kidNode = (EvioNode)node.clone();

                    kidNode.len  = len;
                    kidNode.pos  = position - 8;
                    kidNode.type = DataType.BANK.getValue();  // This is a bank

                    kidNode.dataLen  = dataLen;
                    kidNode.dataPos  = position;
                    kidNode.dataType = dataType;

                    kidNode.pad = pad;
                    kidNode.tag = tag;
                    kidNode.num = num;

                    // Create the tree structure
                    kidNode.isEvent = false;
                    kidNode.parentNode = node;

                    // Add this to list of children and to list of all nodes in the event
                    node.addChild(kidNode);
//System.out.println("scanStructure: kid bank, scanned val = " + kidNode.scanned);

//System.out.println("scanStructure: kid bank at pos = " + kidNode.pos +
//                    " with type " +  DataType.getDataType(dataType) + ", tag/num = " + kidNode.tag +
//                    "/" + kidNode.num + ", list size = " + node.eventNode.allNodes.size());

                    // Only scan through this child if it's a container
                    if (DataType.isStructure(dataType)) {
                        scanStructure(kidNode);
                    }

                    // Set position to start of next header (hop over kid's data)
                    position += 4*dataLen;
                }

                break; // structure contains banks

            case SEGMENT:
            case ALSOSEGMENT:

                // Extract all the segments from this bank of segments.
                while (position < endingPos) {

                    word = buffer.getInt(position);
                    position += 4;
                    tag = word >>> 24;
                    dt = (word >>> 16) & 0xff;
                    dataType = dt & 0x3f;
                    pad = dt >>> 6;
                    // If only 7th bit set, that can only be the legacy tagsegment type
                    // with no padding information - convert it properly.
                    if (dt == 0x40) {
                        dataType = DataType.TAGSEGMENT.getValue();
                        pad = 0;
                    }
                    len = word & 0xffff;

                    EvioNode kidNode = (EvioNode)node.clone();

                    kidNode.len  = len;
                    kidNode.pos  = position - 4;
                    kidNode.type = DataType.SEGMENT.getValue();  // This is a segment

                    kidNode.dataLen  = len;
                    kidNode.dataPos  = position;
                    kidNode.dataType = dataType;

                    kidNode.pad = pad;
                    kidNode.tag = tag;
                    kidNode.num = 0;

                    kidNode.isEvent = false;
                    kidNode.parentNode = node;

                    node.addChild(kidNode);
//System.out.println("scanStructure: kid bank, scanned val = " + kidNode.scanned);

// System.out.println("scanStructure: kid seg at pos = " + kidNode.pos +
//                    " with type " +  DataType.getDataType(dataType) + ", tag/num = " + kidNode.tag +
//                    "/" + kidNode.num + ", list size = " + node.eventNode.allNodes.size());
                    if (DataType.isStructure(dataType)) {
                        scanStructure(kidNode);
                    }

                    position += 4*len;
                }

                break; // structure contains segments

            case TAGSEGMENT:

                // Extract all the tag segments from this bank of tag segments.
                while (position < endingPos) {

                    word = buffer.getInt(position);
                    position += 4;
                    tag      = word >>> 20;
                    dataType = (word >>> 16) & 0xf;
                    len      = word & 0xffff;

                    EvioNode kidNode = (EvioNode)node.clone();

                    kidNode.len  = len;
                    kidNode.pos  = position - 4;
                    kidNode.type = DataType.TAGSEGMENT.getValue();  // This is a tag segment

                    kidNode.dataLen  = len;
                    kidNode.dataPos  = position;
                    kidNode.dataType = dataType;

                    kidNode.pad = 0;
                    kidNode.tag = tag;
                    kidNode.num = 0;

                    kidNode.isEvent = false;
                    kidNode.parentNode = node;

                    node.addChild(kidNode);
//System.out.println("scanStructure: kid bank, scanned val = " + kidNode.scanned);

// System.out.println("scanStructure: kid tagseg at pos = " + kidNode.pos +
//                    " with type " +  DataType.getDataType(dataType) + ", tag/num = " + kidNode.tag +
//                    "/" + kidNode.num + ", list size = " + node.eventNode.allNodes.size());
                   if (DataType.isStructure(dataType)) {
                        scanStructure(kidNode);
                    }

                    position += 4*len;
                }

                break;

            default:
        }
    }


    /**
     * This method scans the event in the buffer.
     * The results are stored for future reference so the
     * event is only scanned once. It returns a list of EvioNode
     * objects representing evio structures (banks, segs, tagsegs).
     *
     * @return the list of objects (evio structures containing data)
     *         obtained from the scan
     */
    private ArrayList<EvioNode> scanStructure() {
//        if (node.scanned) {
//            node.clearLists();
//        }


        if (!node.scanned) {
            // Do this before actual scan so clone() sets all "scanned" fields
            // of child nodes to "true" as well.
            node.scanned = true;

//System.out.println("scanStructure: not scanned do so now");
            scanStructure(node);
        }
//        else {
//            System.out.println("scanStructure: already scanned, skip it");
//            scanStructure(node);
//        }

        // Return results of this or a previous scan
        return node.allNodes;
    }


    /**
     * This method searches the event and
     * returns a list of objects each of which contain information
     * about a single evio structure which matches the given tag and num.
     *
     * @param tag tag to match
     * @param num num to match
     * @return list of EvioNode objects corresponding to matching evio structures
     *         (empty if none found)
     * @throws EvioException if bad arg value(s);
     *                       if object closed
     */
    public synchronized List<EvioNode> searchStructure(int tag, int num)
                                 throws EvioException {

        // check args
        if (tag < 0 || num < 0) {
            throw new EvioException("bad arg value(s)");
        }

        if (closed) {
            throw new EvioException("object closed");
        }

        ArrayList<EvioNode> returnList = new ArrayList<>(100);

        // If event has been previously scanned we get that list,
        // otherwise we scan the node and store the results for future use.
        ArrayList<EvioNode> list = scanStructure();
//System.out.println("searchEvent: list size = " + list.size() +
//" for tag/num = " + tag + "/" + num);

        // Now look for matches in this event
        for (EvioNode enode: list) {
//System.out.println("searchEvent: desired tag = " + tag + " found " + enode.tag);
//System.out.println("           : desired num = " + num + " found " + enode.num);
            if (enode.tag == tag && enode.num == num) {
//System.out.println("           : found node at pos = " + enode.pos + " len = " + enode.len);
                returnList.add(enode);
            }
        }

        return returnList;
    }


    /**
     * This method searches the event and
     * returns a list of objects each of which contain information
     * about a single evio structure which matches the given dictionary
     * entry name.
     *
     * @param  dictName name of dictionary entry to search for
     * @param  dictionary dictionary to use
     * @return list of EvioNode objects corresponding to matching evio structures
     *         (empty if none found)
     * @throws EvioException if either dictName or dictionary arg is null;
     *                       if dictName is an invalid dictionary entry;
     *                       if object closed
     */
    public List<EvioNode> searchStructure(String dictName,
                                          EvioXMLDictionary dictionary)
                                 throws EvioException {

        if (dictName == null || dictionary == null) {
            throw new EvioException("null dictionary and/or entry name");
        }

        int tag = dictionary.getTag(dictName);
        int num = dictionary.getNum(dictName);
        if (tag == -1 || num == -1) {
            throw new EvioException("no dictionary entry for " + dictName);
        }

        return searchStructure(tag, num);
    }


    /**
     * This method adds a bank, segment, or tagsegment onto the end of a
     * structure which contains banks, segments, or tagsegments respectively.
     * It is the responsibility of the caller to make sure that
     * the buffer argument contains valid evio data (only data representing
     * the bank or structure to be added - not in file format with bank
     * header and the like) which is compatible with the type of data
     * structure stored in the parent structure. This means it is not
     * possible to add to structures which contain only a primitive data type.<p>
     *
     * To produce properly formatted evio data, use
     * {@link EvioBank#write(java.nio.ByteBuffer)},
     * {@link EvioSegment#write(java.nio.ByteBuffer)} or
     * {@link EvioTagSegment#write(java.nio.ByteBuffer)} depending on whether
     * a bank, seg, or tagseg is being added.<p>
     *
     * The given buffer argument must be ready to read with its position and limit
     * defining the limits of the data to copy.
     * This method is synchronized due to the bulk, relative puts.
     *
     * @param addBuffer buffer containing evio data to add (<b>not</b> evio file format,
     *                  i.e. no bank headers)
     * @return a new ByteBuffer object which is created and filled with all the data
     *         including what was just added.
     * @throws EvioException if addBuffer is null;
     *                       if addBuffer arg is empty or has non-evio format;
     *                       if addBuffer is opposite endian to current event buffer;
     *                       if added data is not the proper length (i.e. multiple of 4 bytes);
     *                       if there is an internal programming error;
     *                       if object closed
     */
    public synchronized ByteBuffer addStructure(ByteBuffer addBuffer) throws EvioException {

        if (!node.getDataTypeObj().isStructure()) {
            throw new EvioException("cannot add structure to bank of primitive type");
        }

        if (addBuffer == null || addBuffer.remaining() < 4) {
            throw new EvioException("null, empty, or non-evio format buffer arg");
        }

        if (addBuffer.order() != byteOrder) {
            throw new EvioException("trying to add wrong endian buffer");
        }

        if (closed) {
            throw new EvioException("object closed");
        }

        // Position in byteBuffer just past end of event
        int endPos = node.dataPos + 4*node.dataLen;

        // Original position of buffer being added
        int origAddBufPos = addBuffer.position();

        // How many bytes are we adding?
        int appendDataLen = addBuffer.remaining();

        // Make sure it's a multiple of 4
        if (appendDataLen % 4 != 0) {
            throw new EvioException("data added is not in evio format");
        }

        // Since we're changing node's data, get rid of stored data in int[] format
        node.clearIntArray();

        // Data length in 32-bit words
        int appendDataWordLen = appendDataLen / 4;

        // Event contains structures of this type
        DataType eventDataType = node.getDataTypeObj();

        // Is there enough space to fit it?
        if ((byteBuffer.capacity() - byteBuffer.limit()) < appendDataLen) {
            // Not enough room so expand the buffer to fit + 25%.
            // Data copy and node adjustments too.
            expandBuffer(byteBuffer.limit() + appendDataLen);
        }

        //--------------------------------------------------------
        // Add new structure to end of event (nothing comes after)
        //--------------------------------------------------------

        // Position existing buffer at very end
        byteBuffer.limit(byteBuffer.capacity()).position(endPos);
        // Copy new data into buffer
        byteBuffer.put(addBuffer);
        // Get buffer ready for reading
        byteBuffer.flip();
        // Restore original position of add buffer
        addBuffer.position(origAddBufPos);

        //--------------------------------------------------------
        // Adjust event sizes in both node object and buffer.
        //--------------------------------------------------------

        // Position in byteBuffer of top level evio structure (event)
        int eventLenPos = 0;

        // Increase event size
        node.len     += appendDataWordLen;
        node.dataLen += appendDataWordLen;

        switch (eventDataType) {
            case BANK:
            case ALSOBANK:
//System.out.println("event pos = " + eventLenPos + ", len = " + (node.len - appendDataWordLen) +
//                   ", set to " + (node.len));
                byteBuffer.putInt(eventLenPos, node.len);
                break;

            case SEGMENT:
            case ALSOSEGMENT:
            case TAGSEGMENT:
//System.out.println("event SEG/TAGSEG pos = " + eventLenPos + ", len = " + (node.len - appendDataWordLen) +
//                   ", set to " + (node.len));
                if (byteBuffer.order() == ByteOrder.BIG_ENDIAN) {
                    byteBuffer.putShort(eventLenPos+2, (short)(node.len));
                }
                else {
                    byteBuffer.putShort(eventLenPos,   (short)(node.len));
                }
                break;

            default:
                throw new EvioException("internal programming error");
        }

        // Create a node object from the data we just added to the byteBuffer
        EvioNode newNode = extractNode(byteBuffer, null, node,
                                       eventDataType, endPos, 0, false);

        // Add this new node to child and allNodes lists
        node.addChild(newNode);

        // Add its children into those lists too
        scanStructure(newNode);

        return byteBuffer;
    }


    /**
     * This method removes the data, represented by the given node, from the buffer.
     * It also marks the node and its descendants as obsolete. They must not be used
     * anymore.<p>
     *
     * @param removeNode  evio structure to remove from buffer
     * @return backing buffer updated to reflect the node removal
     * @throws EvioException if object closed;
     *                       if node was not found in any event;
     *                       if internal programming error
     */
    public synchronized ByteBuffer removeStructure(EvioNode removeNode) throws EvioException {

        // If we're removing nothing, then DO nothing
        if (removeNode == null) {
            return byteBuffer;
        }

        if (closed) {
            throw new EvioException("object closed");
        }
        else if (removeNode.isObsolete()) {
            return byteBuffer;
        }

        boolean foundNode = false;
        // Place of the removed node in allNodes list. 0 means top level.
        int removeNodePlace = 0;

        // Locate the node to be removed ...
        for (EvioNode n : node.allNodes) {
            // The first node in allNodes is the event node,
            // so do not increment removeNodePlace now.

            if (removeNode == n) {
//System.out.println("removeStruct: found node to remove in struct, " + removeNodePlace);
                foundNode = true;
                break;
            }

            // Keep track of where inside the event it is
            removeNodePlace++;
        }

        if (!foundNode) {
            throw new EvioException("removeNode not found");
        }

        // The data these nodes represent will be removed from the buffer,
        // so the node will be obsolete along with all its descendants.
        removeNode.setObsolete(true);

        //---------------------------------------------------
        // Move all data that came after removed
        // node to where removed node used to be.
        //---------------------------------------------------

        // Amount of data being removed
        int removeDataLen = removeNode.getTotalBytes();
        int removeWordLen = removeDataLen / 4;

        // Copy existing data from after the node being removed,
        int copyFromPos = removeNode.pos + removeDataLen;
        // To the beginning of the node to be removed
        int copyToPos = removeNode.pos;

        // If it's the last structure, things are real easy
        if (copyFromPos == byteBuffer.limit()) {
            // Just reset the limit
            byteBuffer.limit(copyToPos);
        }
        else {
            // Duplicate buffer to use a data source,
            // (limit is already set to end of valid data).
            ByteBuffer srcBuf = byteBuffer.duplicate().order(byteOrder);
            srcBuf.position(copyFromPos);

            // Get buffer ready to receiving data
            byteBuffer.limit(byteBuffer.capacity()).position(copyToPos);

            // Copy
            byteBuffer.put(srcBuf);
            byteBuffer.flip();
        }

        // Reset some buffer position & limit values

        //-------------------------------------
        // By removing a structure, we need to shift the POSITIONS of all
        // structures that follow by the size of the deleted chunk.
        //-------------------------------------
        int i=0;

        for (EvioNode n : node.allNodes) {
            // Removing one node may remove others, skip them all
            if (n.obsolete) {
                i++;
                continue;
            }

            // For events that come after, move all contained nodes
            if (i > removeNodePlace) {
                n.pos     -= removeDataLen;
                n.dataPos -= removeDataLen;
            }
            i++;
        }

        //--------------------------------------------
        // We need to update the lengths of all the
        // removed node's parent structures as well.
        //--------------------------------------------

        // Position of parent in new byteBuffer of event
        int parentPos;
        EvioNode removeParent, parent;
        removeParent = parent = removeNode.parentNode;

        while (parent != null) {
            // Update event size
            parent.len     -= removeWordLen;
            parent.dataLen -= removeWordLen;
            parentPos = parent.pos;
            // Since we're changing parent's data, get rid of stored data in int[] format
            parent.clearIntArray();

            // Parent contains data of this type
            switch (parent.getDataTypeObj()) {
                case BANK:
                case ALSOBANK:
//System.out.println("parent bank pos = " + parentPos + ", len was = " + (parent.len + removeWordLen) +
//                   ", now set to " + parent.len);
                    byteBuffer.putInt(parentPos, parent.len);
                    break;

                case SEGMENT:
                case ALSOSEGMENT:
                case TAGSEGMENT:
//System.out.println("parent seg/tagseg pos = " + parentPos + ", len was = " + (parent.len + removeWordLen) +
//                   ", now set to " + parent.len);
                    if (byteOrder == ByteOrder.BIG_ENDIAN) {
                        byteBuffer.putShort(parentPos + 2, (short) (parent.len));
                    }
                    else {
                        byteBuffer.putShort(parentPos, (short) (parent.len));
                    }
                    break;

                default:
                    throw new EvioException("internal programming error");
            }

            parent = parent.parentNode;
        }

        // Remove node and node's children from lists
        if (removeParent != null) {
            removeParent.removeChild(removeNode);
        }

        return byteBuffer;
    }


    /**
     * Get the data associated with an evio structure in ByteBuffer form.
     * The returned buffer is a view into this reader's buffer (no copy done).
     * Changes in one will affect the other.
     *
     * @param node evio structure whose data is to be retrieved
     * @throws EvioException if object closed
     * @return ByteBuffer object containing data. Position and limit are
     *         set for reading.
     */
    public ByteBuffer getData(EvioNode node) throws EvioException {
        return getData(node, false);
    }


    /**
     * Get the data associated with an evio structure in ByteBuffer form.
     * Depending on the copy argument, the returned buffer will either be
     * a copy of or a view into the data of this reader's buffer.<p>
     * This method is synchronized due to the bulk, relative gets &amp; puts.
     *
     * @param node evio structure whose data is to be retrieved
     * @param copy if <code>true</code>, then return a copy as opposed to a
     *        view into this reader object's buffer.
     * @throws EvioException if object closed
     * @return ByteBuffer object containing data. Position and limit are
     *         set for reading.
     */
    public synchronized ByteBuffer getData(EvioNode node, boolean copy)
                                    throws EvioException {
        if (closed) {
            throw new EvioException("object closed");
        }

        int lim = byteBuffer.limit();
        byteBuffer.limit(node.dataPos + 4*node.dataLen - node.pad).position(node.dataPos);

        if (copy) {
            ByteBuffer newBuf = ByteBuffer.allocate(4*node.dataLen - node.pad).order(byteOrder);
            // Relative get and put changes position in both buffers
            newBuf.put(byteBuffer);
            newBuf.flip();
            byteBuffer.limit(lim).position(0);
            return newBuf;
        }

        ByteBuffer buf = byteBuffer.slice().order(byteOrder);
        byteBuffer.limit(lim).position(0);
        return buf;
    }


    /**
     * Get an evio structure (bank, seg, or tagseg) in ByteBuffer form.
     * The returned buffer is a view into the data of this reader's buffer.<p>
     * This method is synchronized due to the bulk, relative gets &amp; puts.
     *
     * @param node node object representing evio structure of interest
     * @return ByteBuffer object containing bank's/event's bytes. Position and limit are
     *         set for reading.
     * @throws EvioException if node is null;
     */
    public ByteBuffer getStructureBuffer(EvioNode node) throws EvioException {
        return getStructureBuffer(node, false);
    }


    /**
     * Get an evio structure (bank, seg, or tagseg) in ByteBuffer form.
     * Depending on the copy argument, the returned buffer will either be
     * a copy of or a view into the data of this reader's buffer.<p>
     * This method is synchronized due to the bulk, relative gets &amp; puts.
     *
     * @param node node object representing evio structure of interest
     * @param copy if <code>true</code>, then return a copy as opposed to a
     *        view into this reader object's buffer.
     * @return ByteBuffer object containing structure's bytes. Position and limit are
     *         set for reading.
     * @throws EvioException if node is null;
     *                       if object closed
     */
    public synchronized ByteBuffer getStructureBuffer(EvioNode node, boolean copy)
            throws EvioException {

        if (node == null) {
            throw new EvioException("node arg is null");
        }

        if (closed) {
            throw new EvioException("object closed");
        }

        int lim = byteBuffer.limit();
        byteBuffer.limit(node.dataPos + 4*node.dataLen).position(node.pos);

        if (copy) {
            ByteBuffer newBuf = ByteBuffer.allocate(node.getTotalBytes()).order(byteOrder);
            // Relative get and put changes position in both buffers
            newBuf.put(byteBuffer);
            newBuf.flip();
            byteBuffer.limit(lim).position(0);
            return newBuf;
        }

        ByteBuffer buf = byteBuffer.slice().order(byteOrder);
        byteBuffer.limit(lim).position(0);
        return buf;
    }


    /**
     * This method returns an unmodifiable list of all
     * evio structures in buffer as EvioNode objects.
     *
     * @throws EvioException if object closed
     * @return unmodifiable list of all evio structures in buffer as EvioNode objects.
     */
    public synchronized List<EvioNode> getNodes() throws EvioException {
        if (closed) {
            throw new EvioException("object closed");
        }
        return Collections.unmodifiableList(scanStructure());
    }


    /**
     * This method returns an unmodifiable list of all
     * evio structures in buffer as EvioNode objects.
     *
     * @throws EvioException if object closed
     * @return unmodifiable list of all evio structures in buffer as EvioNode objects.
     */
    public synchronized List<EvioNode> getChildNodes() throws EvioException {
        if (closed) {
            throw new EvioException("object closed");
        }
        scanStructure();
        return Collections.unmodifiableList(node.childNodes);
    }


	/**
	 * This only sets the position to its initial value.
	 */
    public synchronized void close() {
        byteBuffer.position(0);
        closed = true;
	}

}