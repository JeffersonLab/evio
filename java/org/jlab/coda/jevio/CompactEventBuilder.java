/*
 * Copyright (c) 2014, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 */

package org.jlab.coda.jevio;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.*;
import java.util.ArrayList;
import java.util.Arrays;




/**
 * This class is used for creating events
 * and their substructures while minimizing use of Java objects.
 * Evio format data of a single event is written directly,
 * and sequentially, into a buffer. The buffer contains
 * only the single event, not the full, evio file format.<p>
 *
 * The methods of this class are not synchronized so it is NOT
 * threadsafe. This is done for speed. The buffer retrieved by
 * {@link #getBuffer()} is ready to read.
 *
 * @author timmer
 * (Feb 6 2014)
 */
public final class CompactEventBuilder {

    /** Buffer in which to write. */
    private ByteBuffer buffer;

    /** Byte array which backs the buffer. */
    private byte[] array;

    /** Offset into backing array. */
    private int arrayOffset;

    /** Current writing position in the buffer. */
    private int position;

    /** Byte order of the buffer, convenience variable. */
    private ByteOrder order;

    /** Two alternate ways of writing data, one which is direct to the
     *  backing array and the other using ByteBuffer object methods. */
    private boolean useByteBuffer;

    /** Did this object create the byte buffer? */
    private boolean createdBuffer;

    /** When writing to buffer, generate EvioNode objects as evio
     *  structures are being created. */
    private boolean generateNodes;

    /** If {@link #generateNodes} is {@code true}, then store
     *  generated node objects in this list (in buffer order. */
    private ArrayList<EvioNode> nodes;

    /** Maximum allowed evio structure levels in buffer. */
    private static final int MAX_LEVELS = 50;

    /** Number of bytes to pad short and byte data. */
    private static final int[] padCount = {0,3,2,1};


    /** This class stores information about an evio structure
     * (bank, segment, or tagsegment) which allows us to write
     * a length or padding in that structure in the buffer.
     */
    private final class StructureContent {
        /** Starting position of this structure in the buffer. */
        int pos;
        /** Keep track of amount of primitive data written for finding padding.
         *  Can be either bytes or shorts. */
        int dataLen;
        /** Padding for byte and short data. */
        int padding;
        /** Type of evio structure this is. */
        DataType type;
        /** Type of evio structures or data contained. */
        DataType dataType;


        void setData(int pos, DataType type, DataType dataType) {
            this.pos = pos;
            this.type = type;
            this.dataType = dataType;
            padding = dataLen = 0;
        }
    }


    /** The top (first element) of the stack is the first structure
     *  created or the event bank.
     *  Each level is the parent of the next one down (index + 1) */
    private StructureContent[] stackArray;

    /** Each element of the array is the total length of all evio
     *  data in a structure including the full length of that
     *  structure's header. There is one length for each level
     *  of evio structures with the 0th element being the top
     *  structure (or event) level. */
    private int[] totalLengths;

    /** Current evio structure being created. */
    private StructureContent currentStructure;

    /** Level of the evio structure currently being created.
     *  Starts at 0 for the event bank. */
    private int currentLevel;


    /**
     * This is the constructor to use for building a new event
     * (just the event in a buffer, not the full evio file format).
     * A buffer is created in this constructor.
     *
     * @param bufferSize size of byte buffer (in bytes) to create.
     * @param order      byte order of created buffer.
     *
     * @throws EvioException if arg is null;
     *                       if bufferSize arg too small
     */
    public CompactEventBuilder(int bufferSize, ByteOrder order)
                    throws EvioException {
        this(bufferSize, order, false);
   	}


    /**
     * This is the constructor to use for building a new event
     * (just the event in a buffer, not the full evio file format).
     * A buffer is created in this constructor.
     *
     * @param bufferSize size of byte buffer (in bytes) to create.
     * @param order      byte order of created buffer.
     * @param generateNodes generate and store an EvioNode object
     *                      for each evio structure created.
     *
     * @throws EvioException if arg is null;
     *                       if bufferSize arg too small
     */
    public CompactEventBuilder(int bufferSize, ByteOrder order, boolean generateNodes)
                    throws EvioException {

        if (order == null) {
            throw new EvioException("null arg(s)");
        }

        if (bufferSize < 8) {
            throw new EvioException("bufferSize arg too small");
        }

        // Create buffer
        array = new byte[bufferSize];
        buffer = ByteBuffer.wrap(array);
        buffer.order(order);
        this.order = order;
        this.generateNodes = generateNodes;
        position = 0;

        // Init variables
        nodes = null;
        currentLevel = -1;
        createdBuffer = true;
        currentStructure = null;

        // Creation of these 2 arrays must only be done once to reduce garbage
        totalLengths = new int[MAX_LEVELS];
        stackArray   = new StructureContent[MAX_LEVELS];

        // Fill stackArray array with pre-allocated objects so each openBank/Seg/TagSeg
        // doesn't have to create a new StructureContent object each time they're called.
        for (int i=0; i < MAX_LEVELS; i++) {
            stackArray[i] = new StructureContent();
        }
   	}


    /**
     * <p>This is the constructor to use for building a new event
     * (just the event in a buffer, not the full evio file format)
     * with a user-given buffer.</p>
     * <b>NOTE: it is assumed that if the given buffer is backed by a byte array,
     * the offset of that array (given by buffer.arrayOffset()) is ZERO!</b>
     *
     * @param buffer the byte buffer to write into.
     *
     * @throws EvioException if arg is null;
     *                       if buffer is too small
     */
    public CompactEventBuilder(ByteBuffer buffer)
                    throws EvioException {
        this(buffer,false);
   	}


    /**
     * <p>This is the constructor to use for building a new event
     * (just the event in a buffer, not the full evio file format)
     * with a user-given buffer.</p>
     * <b>NOTE: it is assumed that if the given buffer is backed by a byte array,
     * the offset of that array (given by buffer.arrayOffset()) is ZERO!</b>
     *
     * @param buffer the byte buffer to write into.
     * @param generateNodes generate and store an EvioNode object
     *                      for each evio structure created.
     *
     * @throws EvioException if arg is null;
     *                       if buffer is too small
     */
    public CompactEventBuilder(ByteBuffer buffer, boolean generateNodes)
                    throws EvioException {

        if (buffer == null) {
            throw new EvioException("null arg(s)");
        }

        totalLengths = new int[MAX_LEVELS];
        stackArray   = new StructureContent[MAX_LEVELS];

        for (int i=0; i < MAX_LEVELS; i++) {
            stackArray[i] = new StructureContent();
        }

        initBuffer(buffer, generateNodes);
   	}


    /**
     * Set the buffer to be written into.
     * <b>NOTE: it is assumed that if the given buffer is backed by a byte array,
     * the offset of that array (given by buffer.arrayOffset()) is ZERO!</b>
     * @param buffer buffer to be written into.
     * @throws EvioException if arg is null;
     *                       if buffer is too small
     */
    public void setBuffer(ByteBuffer buffer) throws EvioException {
        setBuffer(buffer, false);
    }


    /**
     * Set the buffer to be written into.
     * <b>NOTE: it is assumed that if the given buffer is backed by a byte array,
     * the offset of that array (given by buffer.arrayOffset()) is ZERO!</b>
     * @param buffer buffer to be written into.
     * @param generateNodes generate and store an EvioNode object
     *                      for each evio structure created.
     * @throws EvioException if arg is null;
     *                       if buffer is too small
     */
    public void setBuffer(ByteBuffer buffer, boolean generateNodes)
                          throws EvioException {

        if (buffer == null) {
            throw new EvioException("null arg(s)");
        }

        initBuffer(buffer, generateNodes);
    }


    /**
     * Set the buffer to be written into but do NOT reallocate
     * stackArray and totalLengths arrays and do NOT refill
     * stackArray with objects. This avoid unnecessary garbage
     * generation.
     * <b>NOTE: it is assumed that if the given buffer is backed by a byte array,
     * the offset of that array (given by buffer.arrayOffset()) is ZERO!</b>
     *
     * @param buffer buffer to be written into.
     * @param generateNodes generate and store an EvioNode object
     *                      for each evio structure created.
     * @throws EvioException if arg is null;
     *                       if buffer is too small
     */
    private void initBuffer(ByteBuffer buffer, boolean generateNodes)
                          throws EvioException {

        this.generateNodes = generateNodes;

        // Prepare buffer
        this.buffer = buffer;
        buffer.clear();
        Arrays.fill(totalLengths, 0);
        order = buffer.order();
        position = 0;

        if (buffer.limit() < 8) {
            throw new EvioException("compact buffer too small");
        }

        // Protect against using the backing array of slices
//        if (buffer.hasArray() && buffer.array().length == buffer.capacity()) {
//            array = buffer.array();
//        }
        if (buffer.hasArray()) {
            array = buffer.array();
            arrayOffset = buffer.arrayOffset();
        }
        else {
            useByteBuffer = true;
        }

        // Init variables
        if (nodes != null) {
            nodes.clear();
        }
        currentLevel = -1;
        createdBuffer = false;
        currentStructure = null;
    }


    /**
     * Get the buffer being written into.
     * The buffer is ready to read.
     * @return buffer being written into.
     */
    public ByteBuffer getBuffer() {
        buffer.limit(position);
        buffer.position(0);
        return buffer;
    }


    /**
     * Get the total number of bytes written into the buffer.
     * @return total number of bytes written into the buffer.
     */
    public int getTotalBytes() {return position;}


    /** Reset all members for reuse of this object. */
    public void reset() {
        position = 0;
        currentLevel = -1;
        currentStructure = null;

        buffer.clear();

        if (nodes != null) {
            nodes.clear();
        }

        if (array != null) {
            Arrays.fill(array, (byte)0);
        }

        if (totalLengths != null) {
            Arrays.fill(totalLengths, 0);
        }
    }


    /**
     * This method adds an evio segment structure to the buffer.
     *
     * @param tag tag of segment header
     * @param dataType type of data to be contained by this segment
     * @return if told to generate nodes in constructor, then return
     *         the node created here; else return null
     * @throws EvioException if containing structure does not hold segments;
     *                       if no room in buffer for segment header;
     *                       if too many nested evio structures;
     *                       if top-level bank has not been added first.
     */
    public EvioNode openSegment(int tag, DataType dataType)
        throws EvioException {

        if (currentStructure == null) {
            throw new EvioException("add a bank (event) first");
        }

        // Segments not allowed if parent holds different type
        if (currentStructure.dataType != DataType.SEGMENT &&
            currentStructure.dataType != DataType.ALSOSEGMENT) {
            throw new EvioException("may NOT add segment type, expecting " +
                                            currentStructure.dataType);
        }

        // Make sure we can use all of the buffer in case external changes
        // were made to it (e.g. by doing buffer.flip() in order to read).
        // All this does is set pos = 0, limit = capacity, it does NOT
        // clear the data. We're keep track of the position to write at
        // in our own variable, "position".
        buffer.clear();

        if (buffer.limit() - position < 4) {
            throw new EvioException("no room in buffer");
        }

        // For now, assume length and padding = 0
        if (useByteBuffer) {
            if (order == ByteOrder.BIG_ENDIAN) {
                buffer.put(position,   (byte)tag);
                buffer.put(position+1, (byte)(dataType.getValue() & 0x3f));
            }
            else {
                buffer.put(position+2, (byte)(dataType.getValue() & 0x3f));
                buffer.put(position+3, (byte)tag);
            }
        }
        else {
            if (order == ByteOrder.BIG_ENDIAN) {
                array[arrayOffset + position]   = (byte)tag;
                array[arrayOffset + position+1] = (byte)(dataType.getValue() & 0x3f);
            }
            else {
                array[arrayOffset + position+2] = (byte)(dataType.getValue() & 0x3f);
                array[arrayOffset + position+3] = (byte)tag;
            }
        }

        // currentLevel starts at -1
        if (++currentLevel >= MAX_LEVELS) {
            throw new EvioException("too many nested evio structures, increase MAX_LEVELS from " +
                                    MAX_LEVELS);
        }

        // currentStructure is the bank we are creating right now
        currentStructure = stackArray[currentLevel];
        currentStructure.setData(position, DataType.SEGMENT, dataType);

        // We just wrote a bank header so this and all
        // containing levels increase in length by 1.
        addToAllLengths(1);

        // Do we generate an EvioNode object corresponding to this segment?
        EvioNode node = null;
        if (generateNodes) {
            if (nodes == null) {
                nodes = new ArrayList<>(100);
            }

            // This constructor does not have lengths or create allNodes list, blockNode
            node = new EvioNode(tag, 0, position, position+4,
                                         DataType.SEGMENT, dataType,
                                         buffer);
            nodes.add(node);
        }

        position += 4;

        return node;
    }


    /**
     * This method adds an evio tagsegment structure to the buffer.
     *
     * @param tag tag of tagsegment header
     * @param dataType type of data to be contained by this tagsegment
     * @return if told to generate nodes in constructor, then return
     *         the node created here; else return null
     * @throws EvioException if containing structure does not hold tagsegments;
     *                       if no room in buffer for tagsegment header;
     *                       if too many nested evio structures;
     *                       if top-level bank has not been added first.
     */
    public EvioNode openTagSegment(int tag, DataType dataType)
            throws EvioException {

        if (currentStructure == null) {
            throw new EvioException("add a bank (event) first");
        }

        // Tagsegments not allowed if parent holds different type
        if (currentStructure.dataType != DataType.TAGSEGMENT) {
            throw new EvioException("may NOT add tagsegment type, expecting " +
                                            currentStructure.dataType);
        }

        // Make sure we can use all of the buffer in case external changes
        // were made to it (e.g. by doing buffer.flip() in order to read).
        // All this does is set pos = 0, limit = capacity, it does NOT
        // clear the data. We're keep track of the position to write at
        // in our own variable, "position".
        buffer.clear();

        if (buffer.limit() - position < 4) {
            throw new EvioException("no room in buffer");
        }

        // Because Java automatically sets all members of a new
        // byte array to zero, we don't have to specifically write
        // a length of zero into the new tagsegment header (which is
        // what the length is if no data written).

        // For now, assume length = 0
        short compositeWord = (short) ((tag << 4) | (dataType.getValue() & 0xf));

        if (useByteBuffer) {
            if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                buffer.putShort(position, compositeWord);
            }
            else {
                buffer.putShort(position+2, compositeWord);
            }
        }
        else {
            if (order == ByteOrder.BIG_ENDIAN) {
                array[arrayOffset + position]   = (byte) (compositeWord >> 8);
                array[arrayOffset + position+1] = (byte)  compositeWord;
            }
            else {
                array[arrayOffset + position+2] = (byte)  compositeWord;
                array[arrayOffset + position+3] = (byte) (compositeWord >> 8);
            }
        }


        // currentLevel starts at -1
        if (++currentLevel >= MAX_LEVELS) {
            throw new EvioException("too many nested evio structures, increase MAX_LEVELS from " +
                                    MAX_LEVELS);
        }

        // currentStructure is the bank we are creating right now
        currentStructure = stackArray[currentLevel];
        currentStructure.setData(position, DataType.TAGSEGMENT, dataType);

        // We just wrote a bank header so this and all
        // containing levels increase in length by 1.
        addToAllLengths(1);

        // Do we generate an EvioNode object corresponding to this segment?
        EvioNode node = null;
        if (generateNodes) {
            if (nodes == null) {
                nodes = new ArrayList<EvioNode>(100);
            }

            // This constructor does not have lengths or create allNodes list, blockNode
            node = new EvioNode(tag, 0, position, position+4,
                                         DataType.TAGSEGMENT, dataType,
                                         buffer);
            nodes.add(node);
        }

        position += 4;

        return node;
    }


    /**
     * This method adds an evio bank to the buffer.
     *
     * @param tag tag of bank header
     * @param num num of bank header
     * @param dataType data type to be contained in this bank
     * @return if told to generate nodes in constructor, then return
     *         the node created here; else return null
     * @throws EvioException if containing structure does not hold banks;
     *                       if no room in buffer for bank header;
     *                       if too many nested evio structures;
     */
    public EvioNode openBank(int tag, int num, DataType dataType)
            throws EvioException {

        // Banks not allowed if parent holds different type
        if (currentStructure != null &&
                (currentStructure.dataType != DataType.BANK &&
                 currentStructure.dataType != DataType.ALSOBANK)) {
            throw new EvioException("may NOT add bank type, expecting " +
                                            currentStructure.dataType);
        }

        // Make sure we can use all of the buffer in case external changes
        // were made to it (e.g. by doing buffer.flip() in order to read).
        // All this does is set pos = 0, limit = capacity, it does NOT
        // clear the data. We're keeping track of the position to write at
        // in our own variable, "position".
        buffer.clear();

        if (buffer.limit() - position < 8) {
            throw new EvioException("no room in buffer");
        }

        // Write bank header into buffer, assuming padding = 0
        if (useByteBuffer) {
            // Bank w/ no data has len = 1
            buffer.putInt(position, 1);

            if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                buffer.putShort(position + 4, (short)tag);
                buffer.put(position + 6, (byte)(dataType.getValue() & 0x3f));
                buffer.put(position + 7, (byte)num);
            }
            else {
                buffer.put(position + 4, (byte)num);
                buffer.put(position + 5, (byte)(dataType.getValue() & 0x3f));
                buffer.putShort(position + 6, (short)tag);
            }
        }
        else {
            // Bank w/ no data has len = 1

            if (order == ByteOrder.BIG_ENDIAN) {
                // length word
                array[arrayOffset + position]   = (byte)0;
                array[arrayOffset + position+1] = (byte)0;
                array[arrayOffset + position+2] = (byte)0;
                array[arrayOffset + position+3] = (byte)1;

                array[arrayOffset + position+4] = (byte)(tag >> 8);
                array[arrayOffset + position+5] = (byte)tag;
                array[arrayOffset + position+6] = (byte)(dataType.getValue() & 0x3f);
                array[arrayOffset + position+7] = (byte)num;
            }
            else {
                array[arrayOffset + position]   = (byte)1;
                array[arrayOffset + position+1] = (byte)0;
                array[arrayOffset + position+2] = (byte)0;
                array[arrayOffset + position+3] = (byte)0;

                array[arrayOffset + position+4] = (byte)num;
                array[arrayOffset + position+5] = (byte)(dataType.getValue() & 0x3f);
                array[arrayOffset + position+6] = (byte)tag;
                array[arrayOffset + position+7] = (byte)(tag >> 8);
            }
        }

        // currentLevel starts at -1
        if (++currentLevel >= MAX_LEVELS) {
            throw new EvioException("too many nested evio structures, increase MAX_LEVELS from " +
                                    MAX_LEVELS);
        }

        // currentStructure is the bank we are creating right now
        currentStructure = stackArray[currentLevel];
        currentStructure.setData(position, DataType.BANK, dataType);

        // We just wrote a bank header so this and all
        // containing levels increase in length by 2.
        addToAllLengths(2);

        // Do we generate an EvioNode object corresponding to this segment?
        EvioNode node = null;
        if (generateNodes) {
            if (nodes == null) {
                nodes = new ArrayList<>(100);
            }

            // This constructor does not have lengths or create allNodes list, blockNode
            node = new EvioNode(tag, 0, position, position+8,
                                         DataType.BANK, dataType,
                                         buffer);
            nodes.add(node);
        }

        position += 8;

        return node;
    }


    /**
   	 * This method ends the writing of the current evio structure and
     * makes sure the length and padding fields are properly set.
     * Its parent, if any, then becomes the current structure.
     *
     * @return {@code true} if top structure was reached (is current), else {@code false}.
   	 */
   	public boolean closeStructure() {
        // Cannot go up any further
        if (currentLevel < 0) {
            return true;
        }

//System.out.println("closeStructure: set currentLevel(" + currentLevel +
//        ") len to " + (totalLengths[currentLevel] - 1));

        // Set the length of current structure since we're done adding to it.
        // The length contained in an evio header does NOT include itself,
        // thus the -1.
        setCurrentHeaderLength(totalLengths[currentLevel] - 1);

        // Set padding if necessary
        if (currentStructure.padding > 0) {
            setCurrentHeaderPadding(currentStructure.padding);
        }

        // Clear out old data
        totalLengths[currentLevel] = 0;

        // Go up a level
        if (--currentLevel > -1) {
            currentStructure = stackArray[currentLevel];
        }

        // Have we reached the top ?
        return (currentLevel < 0);
    }


    /**
     * This method finishes the event writing by setting all the
     * proper lengths &amp; padding and ends up at the event or top level.
     */
    public void closeAll() {
        while (!closeStructure()) {}
    }


    /**
     * In the emu software package, it's necessary to change the top level
     * tag after the top level bank has already been created. This method
     * allows changing it after initially written.
     * @param tag  new tag value of top-level, event bank.
     */
    public void setTopLevelTag(short tag) {

        if (useByteBuffer) {
            if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                buffer.putShort(4, tag);
            }
            else {
                buffer.putShort(6, tag);
            }
        }
        else {
            if (order == ByteOrder.BIG_ENDIAN) {
                array[arrayOffset + 4] = (byte)(tag >> 8);
                array[arrayOffset + 5] = (byte)tag;
            }
            else {
                array[arrayOffset + 6] = (byte)tag;
                array[arrayOffset + 7] = (byte)(tag >> 8);
            }
        }
    }

    
    /**
     * This method adds the given length to the current structure's existing
     * length and all its ancestors' lengths. Nothing is written into the
     * buffer. The lengths are keep in an array for later use in writing to
     * the buffer.
     *
     * @param len length to add
     */
    private void addToAllLengths(int len) {
        // Go up the chain of structures from current structure, inclusive
        for (int i=currentLevel; i >= 0; i--) {
            totalLengths[i] += len;
//System.out.println("Set len at level " + i + " to " + totalLengths[i]);
        }
//        System.out.println("");
    }


    /**
     * This method sets the length of the current evio structure in the buffer.
     * @param len length of current structure
     */
    private void setCurrentHeaderLength(int len) {

        switch (currentStructure.type) {
            case BANK:
            case ALSOBANK:
                if (useByteBuffer) {
                    buffer.putInt(currentStructure.pos, len);
                }
                else {
                    try {
                        ByteDataTransformer.toBytes(len, order, array,arrayOffset + currentStructure.pos);
                    }
                    catch (EvioException e) {/* never happen*/}
                }

                return;

            case SEGMENT:
            case ALSOSEGMENT:
            case TAGSEGMENT:
                if (useByteBuffer) {
                    if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                        buffer.putShort(currentStructure.pos+2, (short)len);
                    }
                    else {
                        buffer.putShort(currentStructure.pos, (short)len);
                    }
                }
                else {
                    try {
                        if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                            ByteDataTransformer.toBytes((short)len, order,
                                                        array, arrayOffset + currentStructure.pos+2);
                        }
                        else {
                            ByteDataTransformer.toBytes((short)len, order,
                                                        array, arrayOffset + currentStructure.pos);
                        }
                    }
                    catch (EvioException e) {/* never happen*/}
                }
                return;

            default:
        }
    }



    /**
     * This method sets the padding of the current evio structure in the buffer.
     * @param padding padding of current structure's data
     */
    private void setCurrentHeaderPadding(int padding) {

        // byte containing padding
        byte b = (byte)((currentStructure.dataType.getValue() & 0x3f) | (padding << 6));

        switch (currentStructure.type) {
            case BANK:
            case ALSOBANK:
                if (useByteBuffer) {
                    if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                        buffer.put(currentStructure.pos+6, b);
                    }
                    else {
                        buffer.put(currentStructure.pos+5, b);
                    }
                }
                else {
                    if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                        array[arrayOffset + currentStructure.pos+6] = b;
                    }
                    else {
                        array[arrayOffset + currentStructure.pos+5] = b;
                    }
                }

                return;

            case SEGMENT:
            case ALSOSEGMENT:

                if (useByteBuffer) {
                    if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                        buffer.put(currentStructure.pos+1, b);
                    }
                    else {
                        buffer.put(currentStructure.pos+2, b);
                    }
                }
                else {
                    if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                        array[arrayOffset + currentStructure.pos+1] = b;
                    }
                    else {
                        array[arrayOffset + currentStructure.pos+2] = b;
                    }
                }
                return;

            default:
        }
    }


    /**
     * This method takes the node (which has been scanned and that
     * info stored in that object) and rewrites it into the buffer.
     * This is equivalent to a swap if the node is opposite endian in
     * its original buffer. If the node is not opposite endian,
     * there is no point in calling this method.
     *
     * @param node node whose header data must be written
     */
    private void writeHeader(EvioNode node) {

        switch (currentStructure.type) {

            case BANK:
            case ALSOBANK:

                if (useByteBuffer) {
                    buffer.putInt(position, node.len);
                    if (order == ByteOrder.BIG_ENDIAN) {
                        buffer.putShort(position+4, (short)node.tag);
                        buffer.put     (position+6, (byte)((node.dataType & 0x3f) | (node.pad << 6)));
                        buffer.put     (position+7, (byte)node.num);
                    }
                    else {
                        buffer.put     (position+4, (byte)node.num);
                        buffer.put     (position+5, (byte)((node.dataType & 0x3f) | (node.pad << 6)));
                        buffer.putShort(position+6, (short)node.tag);
                    }
                }
                else {
                    if (order == ByteOrder.BIG_ENDIAN) {
                        array[arrayOffset + position]   = (byte)(node.len >> 24);
                        array[arrayOffset + position+1] = (byte)(node.len >> 16);
                        array[arrayOffset + position+2] = (byte)(node.len >> 8);
                        array[arrayOffset + position+3] = (byte) node.len;

                        array[arrayOffset + position+4] = (byte)(node.tag >>> 8);
                        array[arrayOffset + position+5] = (byte) node.tag;
                        array[arrayOffset + position+6] = (byte)((node.dataType & 0x3f) | (node.pad << 6));
                        array[arrayOffset + position+7] = (byte) node.num;
                    }
                    else {
                        array[arrayOffset + position]   = (byte)(node.len);
                        array[arrayOffset + position+1] = (byte)(node.len >> 8);
                        array[arrayOffset + position+2] = (byte)(node.len >> 16);
                        array[arrayOffset + position+3] = (byte)(node.len >> 24);

                        array[arrayOffset + position+4] = (byte) node.num;
                        array[arrayOffset + position+5] = (byte)((node.dataType & 0x3f) | (node.pad << 6));
                        array[arrayOffset + position+6] = (byte) node.tag;
                        array[arrayOffset + position+7] = (byte)(node.tag >>> 8);
                    }
                }

                position += 8;
                break;

            case SEGMENT:
            case ALSOSEGMENT:

                if (useByteBuffer) {
                    if (order == ByteOrder.BIG_ENDIAN) {
                        buffer.put     (position,   (byte)node.tag);
                        buffer.put     (position+1, (byte)((node.dataType & 0x3f) | (node.pad << 6)));
                        buffer.putShort(position+2, (short)node.len);
                    }
                    else {
                        buffer.putShort(position,(short)node.len);
                        buffer.put(position + 1, (byte) ((node.dataType & 0x3f) | (node.pad << 6)));
                        buffer.put(position + 2, (byte) node.tag);
                    }
                }
                else {
                    if (order == ByteOrder.BIG_ENDIAN) {
                        array[arrayOffset + position]   = (byte)  node.tag;
                        array[arrayOffset + position+1] = (byte)((node.dataType & 0x3f) | (node.pad << 6));
                        array[arrayOffset + position+2] = (byte) (node.len >> 8);
                        array[arrayOffset + position+3] = (byte)  node.len;
                    }
                    else {
                        array[arrayOffset + position]   = (byte)  node.len;
                        array[arrayOffset + position+1] = (byte) (node.len >> 8);
                        array[arrayOffset + position+2] = (byte)((node.dataType & 0x3f) | (node.pad << 6));
                        array[arrayOffset + position+3] = (byte)  node.tag;
                    }
                }

                position += 4;
                break;

            case TAGSEGMENT:

                short compositeWord = (short) ((node.tag << 4) | (node.dataType & 0xf));

                if (useByteBuffer) {
                    if (order == ByteOrder.BIG_ENDIAN) {
                        buffer.putShort(position, compositeWord);
                        buffer.putShort(position+2, (short)node.len);
                    }
                    else {
                        buffer.putShort(position,  (short)node.len);
                        buffer.putShort(position + 2, compositeWord);
                   }
                }
                else {
                    if (order == ByteOrder.BIG_ENDIAN) {
                        array[arrayOffset + position]   = (byte) (compositeWord >> 8);
                        array[arrayOffset + position+1] = (byte)  compositeWord;
                        array[arrayOffset + position+2] = (byte) (node.len >> 8);
                        array[arrayOffset + position+3] = (byte)  node.len;
                    }
                    else {
                        array[arrayOffset + position]   = (byte)  node.len;
                        array[arrayOffset + position+1] = (byte) (node.len >> 8);
                        array[arrayOffset + position+2] = (byte)  compositeWord;
                        array[arrayOffset + position+3] = (byte) (compositeWord >> 8);
                    }
                }

                position += 4;
                break;

            default:
        }
    }


    /**
     * Take the node object and write its data into buffer in the
     * proper endian while also swapping primitive data if desired.
     *
     * @param node node to be written (is never null)
     * @param swapData do we swap the primitive data or not?
     */
    private void writeNode(EvioNode node, boolean swapData) {

        // Write header in endianness of buffer
        writeHeader(node);

        // Does this node contain other containers?
        if (node.getDataTypeObj().isStructure()) {
            // Iterate through list of children
            ArrayList<EvioNode> kids = node.getChildNodes();
            if (kids == null) return;
            for (EvioNode child : kids) {
                writeNode(child, swapData);
            }
        }
        else {
            ByteBuffer nodeBuf = node.buffer;

            if (swapData) {
                try {
                    ByteDataTransformer.swapData(node.getDataTypeObj(), nodeBuf,
                                                 buffer, node.dataPos, position,
                                                 node.dataLen, false);
                }
                catch (EvioException e) {
                    e.printStackTrace();
                }
            }
            else {
                if (!useByteBuffer && nodeBuf.hasArray()) {
                    System.arraycopy(nodeBuf.array(), nodeBuf.arrayOffset() + node.dataPos,
                                     array, arrayOffset + position, 4*node.dataLen);
                }
                else {
// TODO: IS THIS NECESSARY????
                    ByteBuffer duplicateBuf = nodeBuf.duplicate();
                    duplicateBuf.limit(node.dataPos + 4*node.dataLen).position(node.dataPos);

                    buffer.position(position);
                    buffer.put(duplicateBuf); // this method is relative to position
                    buffer.position(0);       // get ready for external reading
                }
            }

            position += 4*node.dataLen;
        }
    }


    /**
     * Adds the evio structure represented by the EvioNode object
     * into the buffer.
     *
     * @param node the EvioNode object representing the evio structure to data.
     * @throws EvioException if data is null or empty;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    public void addEvioNode(EvioNode node) throws EvioException {
        if (node == null) {
            throw new EvioException("no data to add");
        }

        if (currentStructure == null) {
            throw new EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure.dataType != node.getTypeObj()) {
            throw new EvioException("may only add " + currentStructure.dataType + " data");
        }

        // Sets pos = 0, limit = capacity, & does NOT clear data
        buffer.clear();

        int len = node.getTotalBytes();
//System.out.println("addEvioNode: buf lim = " + buffer.limit() +
//                           " - pos = " + position + " = (" + (buffer.limit() - position) +
//                           ") must be >= " + node.getTotalBytes() + " node total bytes");
        if (buffer.limit() - position < len) {
            throw new EvioException("no room in buffer");
        }

        addToAllLengths(len/4);  // # 32-bit words

        ByteBuffer nodeBuf = node.buffer;

        if (nodeBuf.order() == buffer.order()) {
            if (!useByteBuffer && nodeBuf.hasArray()) {
//System.out.println("addEvioNode: arraycopy node (same endian)");
                System.arraycopy(nodeBuf.array(), nodeBuf.arrayOffset() + node.pos,
                        array, arrayOffset + position, node.getTotalBytes());
            }
            else {
//System.out.println("addEvioNode: less efficient node copy (same endian)");
                // Better performance not to slice/duplicate buffer (5 - 10% faster)
                ByteBuffer duplicateBuf = nodeBuf.duplicate();
                duplicateBuf.limit(node.pos + node.getTotalBytes()).position(node.pos);

                buffer.position(position);
                buffer.put(duplicateBuf); // this method is relative to position
                buffer.position(0);       // get ready for external reading
            }

            position += len;
        }
        else {
            // If node is opposite endian as this buffer,
            // rewrite all evio header data, but leave
            // primitive data alone.
            writeNode(node, false);
        }
    }


    /**
     * Appends byte data to the structure.
     *
     * @param data the byte data to append.
     * @throws EvioException if data is null or empty;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    public void addByteData(byte data[]) throws EvioException {
        addCharData(data);
    }


    /**
     * Appends byte data to the structure.
     *
     * @param data the byte data to append.
     * @throws EvioException if data is null or empty;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    public void addCharData(byte data[]) throws EvioException {
        if (data == null || data.length < 1) {
            throw new EvioException("no data to add");
        }

        if (currentStructure == null) {
            throw new EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure.dataType != DataType.CHAR8  &&
            currentStructure.dataType != DataType.UCHAR8)  {
            throw new EvioException("may NOT add " + currentStructure.dataType + " data");
        }

        // Sets pos = 0, limit = capacity, & does NOT clear data
        buffer.clear();

        int len = data.length;
        if (buffer.limit() - position < len) {
            throw new EvioException("no room in buffer");
        }

        // Things get tricky if this method is called multiple times in succession.
        // Keep track of how much data we write each time so length and padding
        // are accurate.

        // Last increase in length
        int lastWordLen = (currentStructure.dataLen + 3)/4;

        // If we are adding to existing data,
        // place position before previous padding.
        if (currentStructure.dataLen > 0) {
            position -= currentStructure.padding;
        }

        // Total number of bytes to write & already written
        currentStructure.dataLen += len;

        // New total word len of this data
        int totalWordLen = (currentStructure.dataLen + 3)/4;

        // Increase lengths by the difference
        addToAllLengths(totalWordLen - lastWordLen);

        // Copy the data in one chunk
        if (useByteBuffer) {
            buffer.position(position);
            buffer.put(data);
            buffer.position(0);
        }
        else {
            System.arraycopy(data, 0, array, arrayOffset + position, len);
        }

        // Calculate the padding
        currentStructure.padding = padCount[currentStructure.dataLen % 4];

        // Advance buffer position
        position += len + currentStructure.padding;
    }


    /**
     * Appends int data to the structure.
     *
     * @param data the int data to append.
     * @throws EvioException if data is null or empty;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    public void addIntData(int data[]) throws EvioException {
        addIntData(data, 0, data.length);
    }


    /**
     * Appends int data to the structure.
     *
     * @param data the int data to append.
     * @param offset offset into data array or number of integers at
     *               beginning of data to skip (not to add to event).
     * @throws EvioException if data is null or empty;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    public void addIntData(int data[], int offset) throws EvioException {
        addIntData(data, offset, data.length);
    }


    /**
     * Appends int data to the structure.
     *
     * @param data   the int data to append.
     * @param offset offset into data array.
     * @param len    the number of ints from data to append.
     * @throws EvioException if data is null;
     *                       if count/offset negative or too large;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    public void addIntData(int data[], int offset, int len) throws EvioException {

        if (data == null) {
            throw new EvioException("data arg null");
        }

        if (len < 0 || len > data.length) {
            throw new EvioException("len must be >= 0 and <= data length");
        }

        if (offset < 0 || offset + len > data.length) {
            throw new EvioException("offset must be >= 0 and <= (data length - len)");
        }

        if (len == 0) return;

        if (currentStructure == null) {
            throw new EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure.dataType != DataType.INT32  &&
            currentStructure.dataType != DataType.UINT32 &&
            currentStructure.dataType != DataType.UNKNOWN32)  {
            throw new EvioException("may NOT add " + currentStructure.dataType + " data");
        }

        // Sets pos = 0, limit = capacity, & does NOT clear data
        buffer.clear();

        if (buffer.limit() - position < 4*len) {
            throw new EvioException("no room in buffer");
        }

        addToAllLengths(len);  // # 32-bit words

        if (useByteBuffer) {
            buffer.position(position);
            IntBuffer db = buffer.asIntBuffer();
            db.put(data, offset, len);
            buffer.position(0);
        }
        else {
            int j, pos = position;
            if (order == ByteOrder.BIG_ENDIAN) {
                for (int i=offset; i < len + offset; i++) {
                    j = data[i];
                    array[arrayOffset + pos++] = (byte) (j >> 24);
                    array[arrayOffset + pos++] = (byte) (j >> 16);
                    array[arrayOffset + pos++] = (byte) (j >> 8);
                    array[arrayOffset + pos++] = (byte) (j);
                }
            }
            else {
                for (int i=offset; i < len + offset; i++) {
                    j = data[i];
                    array[arrayOffset + pos++] = (byte) (j);
                    array[arrayOffset + pos++] = (byte) (j >> 8);
                    array[arrayOffset + pos++] = (byte) (j >> 16);
                    array[arrayOffset + pos++] = (byte) (j >> 24);
                }
            }
        }

        position += 4*len;     // # bytes
    }


    /**
     * Appends short data to the structure.
     *
     * @param data the short data to append.
     * @throws EvioException if data is null or empty;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    public void addShortData(short data[]) throws EvioException {
        addShortData(data, 0, data.length);
    }


    /**
     * Appends short data to the structure.
     *
     * @param data    the short data to append.
     * @param offset  offset into data array.
     * @param len     the number of shorts from data to append.
     * @throws EvioException if data is null;
     *                       if count/offset negative or too large;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    public void addShortData(short data[], int offset, int len) throws EvioException {

        if (data == null) {
            throw new EvioException("data arg null");
        }

        if (len < 0 || len > data.length) {
            throw new EvioException("len must be >= 0 and <= data length");
        }

        if (offset < 0 || offset + len > data.length) {
            throw new EvioException("offset must be >= 0 and <= (data length - len)");
        }

        if (len == 0) return;

        if (currentStructure == null) {
            throw new EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure.dataType != DataType.SHORT16  &&
            currentStructure.dataType != DataType.USHORT16)  {
            throw new EvioException("may NOT add " + currentStructure.dataType + " data");
        }

        // Sets pos = 0, limit = capacity, & does NOT clear data
        buffer.clear();

        if (buffer.limit() - position < 2*len) {
            throw new EvioException("no room in buffer");
        }

        // Things get tricky if this method is called multiple times in succession.
        // Keep track of how much data we write each time so length and padding
        // are accurate.

        // Last increase in length
        int lastWordLen = (currentStructure.dataLen + 1)/2;
//System.out.println("lastWordLen = " + lastWordLen);

        // If we are adding to existing data,
        // place position before previous padding.
        if (currentStructure.dataLen > 0) {
            position -= currentStructure.padding;
//System.out.println("Back up before previous padding of " + currentStructure.padding);
        }

        // Total number of bytes to write & already written
        currentStructure.dataLen += len;
//System.out.println("Total bytes to write = " + currentStructure.dataLen);

        // New total word len of this data
        int totalWordLen = (currentStructure.dataLen + 1)/2;
//System.out.println("Total word len = " + totalWordLen);

        // Increase lengths by the difference
        addToAllLengths(totalWordLen - lastWordLen);

        if (useByteBuffer) {
            buffer.position(position);
            ShortBuffer db = buffer.asShortBuffer();
            db.put(data, offset, len);
            buffer.position(0);
        }
        else {
            int pos = position;
            if (order == ByteOrder.BIG_ENDIAN) {
                for (int i=offset; i < len + offset; i++) {
                    short aData = data[i];
                    array[arrayOffset + pos++] = (byte) (aData >> 8);
                    array[arrayOffset + pos++] = (byte) (aData);
                }
            }
            else {
                for (int i=offset; i < len + offset; i++) {
                    short aData = data[i];
                    array[arrayOffset + pos++] = (byte) (aData);
                    array[arrayOffset + pos++] = (byte) (aData >> 8);
                }
            }
        }

        currentStructure.padding = 2*(currentStructure.dataLen % 2);
        // Advance position
        position += 2*len + currentStructure.padding;
    }


    /**
     * Appends a short integer to the structure.
     *
     * @param data the short data to append.
     * @throws EvioException if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    public void addShortData(short data) throws EvioException {

        if (currentStructure == null) {
            throw new EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure.dataType != DataType.SHORT16  &&
            currentStructure.dataType != DataType.USHORT16)  {
            throw new EvioException("may NOR add " + currentStructure.dataType + " data");
        }

        // Sets pos = 0, limit = capacity, & does NOT clear data
        buffer.clear();

        if (buffer.limit() - position < 2) {
            throw new EvioException("no room in buffer");
        }

        // Things get tricky if this method is called multiple times in succession.
        // Keep track of how much data we write each time so length and padding
        // are accurate.

        // Last increase in length
        int lastWordLen = (currentStructure.dataLen + 1)/2;

        // If we are adding to existing data,
        // place position before previous padding.
        if (currentStructure.dataLen > 0) {
            position -= currentStructure.padding;
        }

        // Total number of bytes to write & already written
        currentStructure.dataLen += 1;

        // New total word len of this data
        int totalWordLen = (currentStructure.dataLen + 1)/2;

        // Increase lengths by the difference
        addToAllLengths(totalWordLen - lastWordLen);

        if (useByteBuffer) {
            buffer.position(position);
            buffer.putShort(data);
            buffer.position(0);
        }
        else {
            int pos = position;
            if (order == ByteOrder.BIG_ENDIAN) {
                array[arrayOffset + pos++] = (byte) (data >> 8);
                array[arrayOffset + pos]   = (byte) (data);
            }
            else {
                array[arrayOffset + pos++] = (byte) (data);
                array[arrayOffset + pos]   = (byte) (data >> 8);
            }
        }

        currentStructure.padding = 2*(currentStructure.dataLen % 2);

        // Advance position
        position += 2 + currentStructure.padding;
    }


    /**
     * Appends long data to the structure.
     *
     * @param data the long data to append.
     * @throws EvioException if data is null or empty;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    public void addLongData(long data[]) throws EvioException {
        addLongData(data, 0, data.length);
    }


    /**
     * Appends long data to the structure.
     *
     * @param data    the long data to append.
     * @param offset  offset into data array.
     * @param len     the number of longs from data to append.
     * @throws EvioException if data is null;
     *                       if count/offset negative or too large;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    public void addLongData(long data[], int offset, int len) throws EvioException {

        if (data == null) {
            throw new EvioException("data arg null");
        }

        if (len < 0 || len > data.length) {
            throw new EvioException("len must be >= 0 and <= data length");
        }

        if (offset < 0 || offset + len > data.length) {
            throw new EvioException("offset must be >= 0 and <= (data length - len)");
        }

        if (len == 0) return;

        if (currentStructure == null) {
            throw new EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure.dataType != DataType.LONG64  &&
            currentStructure.dataType != DataType.ULONG64)  {
            throw new EvioException("may NOT add " + currentStructure.dataType + " data");
        }

        // Sets pos = 0, limit = capacity, & does NOT clear data
        buffer.clear();

        if (buffer.limit() - position < 8*len) {
            throw new EvioException("no room in buffer");
        }

        addToAllLengths(2*len);  // # 32-bit words

        if (useByteBuffer) {
            buffer.position(position);
            LongBuffer db = buffer.asLongBuffer();
            db.put(data, offset, len);
            buffer.position(0);
        }
        else {
            int pos = position;
            if (order == ByteOrder.BIG_ENDIAN) {
                for (int i=offset; i < len + offset; i++) {
                    long aData = data[i];
                    array[arrayOffset + pos++] = (byte) (aData >> 56);
                    array[arrayOffset + pos++] = (byte) (aData >> 48);
                    array[arrayOffset + pos++] = (byte) (aData >> 40);
                    array[arrayOffset + pos++] = (byte) (aData >> 32);
                    array[arrayOffset + pos++] = (byte) (aData >> 24);
                    array[arrayOffset + pos++] = (byte) (aData >> 16);
                    array[arrayOffset + pos++] = (byte) (aData >> 8);
                    array[arrayOffset + pos++] = (byte) (aData);
                }
            }
            else {
                for (int i=offset; i < len + offset; i++) {
                    long aData = data[i];
                    array[arrayOffset + pos++] = (byte) (aData);
                    array[arrayOffset + pos++] = (byte) (aData >> 8);
                    array[arrayOffset + pos++] = (byte) (aData >> 16);
                    array[arrayOffset + pos++] = (byte) (aData >> 24);
                    array[arrayOffset + pos++] = (byte) (aData >> 32);
                    array[arrayOffset + pos++] = (byte) (aData >> 40);
                    array[arrayOffset + pos++] = (byte) (aData >> 48);
                    array[arrayOffset + pos++] = (byte) (aData >> 56);
                }
            }
        }

        position += 8*len;       // # bytes
    }


    /**
     * Appends float data to the structure.
     *
     * @param data the float data to append.
     * @throws EvioException if data is null or empty;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    public void addFloatData(float data[]) throws EvioException {
        if (data == null || data.length < 1) {
            throw new EvioException("no data to add");
        }

        if (currentStructure == null) {
            throw new EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure.dataType != DataType.FLOAT32) {
            throw new EvioException("may NOT add " + currentStructure.dataType + " data");
        }

        // Sets pos = 0, limit = capacity, & does NOT clear data
        buffer.clear();

        int len = data.length;
        if (buffer.limit() - position < 4*len) {
            throw new EvioException("no room in buffer");
        }

        addToAllLengths(len);  // # 32-bit words

        if (useByteBuffer) {
            buffer.position(position);
            FloatBuffer db = buffer.asFloatBuffer();
            db.put(data, 0, len);
            buffer.position(0);
        }
        else {
            int aData, pos = position;
            if (order == ByteOrder.BIG_ENDIAN) {
                for (float fData : data) {
                    aData = Float.floatToRawIntBits(fData);
                    array[arrayOffset + pos++] = (byte) (aData >> 24);
                    array[arrayOffset + pos++] = (byte) (aData >> 16);
                    array[arrayOffset + pos++] = (byte) (aData >> 8);
                    array[arrayOffset + pos++] = (byte) (aData);
                }
            }
            else {
                for (float fData : data) {
                    aData = Float.floatToRawIntBits(fData);
                    array[arrayOffset + pos++] = (byte) (aData);
                    array[arrayOffset + pos++] = (byte) (aData >> 8);
                    array[arrayOffset + pos++] = (byte) (aData >> 16);
                    array[arrayOffset + pos++] = (byte) (aData >> 24);
                }
            }
        }

        position += 4*len;     // # bytes
    }




    /**
     * Appends double data to the structure.
     *
     * @param data the double data to append.
     * @throws EvioException if data is null or empty;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    public void addDoubleData(double data[]) throws EvioException {
        if (data == null || data.length < 1) {
            throw new EvioException("no data to add");
        }

        if (currentStructure == null) {
            throw new EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure.dataType != DataType.DOUBLE64) {
            throw new EvioException("may NOT add " + currentStructure.dataType + " data");
        }

        // Sets pos = 0, limit = capacity, & does NOT clear data
        buffer.clear();

        int len = data.length;
        if (buffer.limit() - position < 8*len) {
            throw new EvioException("no room in buffer");
        }

        addToAllLengths(2*len);  // # 32-bit words

        if (useByteBuffer) {
            buffer.position(position);
            DoubleBuffer db = buffer.asDoubleBuffer();
            db.put(data, 0, len);
            buffer.position(0);
        }
        else {
            long aData;
            int pos = position;
            if (order == ByteOrder.BIG_ENDIAN) {
                for (double dData : data) {
                    aData = Double.doubleToRawLongBits(dData);
                    array[arrayOffset + pos++] = (byte) (aData >> 56);
                    array[arrayOffset + pos++] = (byte) (aData >> 48);
                    array[arrayOffset + pos++] = (byte) (aData >> 40);
                    array[arrayOffset + pos++] = (byte) (aData >> 32);
                    array[arrayOffset + pos++] = (byte) (aData >> 24);
                    array[arrayOffset + pos++] = (byte) (aData >> 16);
                    array[arrayOffset + pos++] = (byte) (aData >> 8);
                    array[arrayOffset + pos++] = (byte) (aData);
                }
            }
            else {
                for (double dData : data) {
                    aData = Double.doubleToRawLongBits(dData);
                    array[arrayOffset + pos++] = (byte) (aData);
                    array[arrayOffset + pos++] = (byte) (aData >> 8);
                    array[arrayOffset + pos++] = (byte) (aData >> 16);
                    array[arrayOffset + pos++] = (byte) (aData >> 24);
                    array[arrayOffset + pos++] = (byte) (aData >> 32);
                    array[arrayOffset + pos++] = (byte) (aData >> 40);
                    array[arrayOffset + pos++] = (byte) (aData >> 48);
                    array[arrayOffset + pos++] = (byte) (aData >> 56);
                }
            }
        }

        position += 8*len;     // # bytes
    }




    /**
     * Appends string array to the structure.
     *
     * @param strings the strings to append.
     * @throws EvioException if data is null or empty;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    public void addStringData(String strings[]) throws EvioException {
        if (strings == null || strings.length < 1) {
            throw new EvioException("no data to add");
        }

        if (currentStructure == null) {
            throw new EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure.dataType != DataType.CHARSTAR8) {
            throw new EvioException("may only add " + currentStructure.dataType + " data");
        }

        // Convert strings into byte array (already padded)
        byte[] data = BaseStructure.stringsToRawBytes(strings);

        // Sets pos = 0, limit = capacity, & does NOT clear data
        buffer.clear();

        int len = data.length;
        if (buffer.limit() - position < len) {
            throw new EvioException("no room in buffer");
        }

        // Things get tricky if this method is called multiple times in succession.
        // In fact, it's too difficult to bother with so just throw an exception.
        if (currentStructure.dataLen > 0) {
            throw new EvioException("addStringData() may only be called once per structure");
        }

        if (useByteBuffer) {
            buffer.position(position);
            buffer.put(data);
            buffer.position(0);
        }
        else {
            System.arraycopy(data, 0, array, arrayOffset + position, len);
        }
        currentStructure.dataLen += len;
        addToAllLengths(len/4);

        position += len;
    }


    /**
     * Appends CompositeData objects to the structure.
     *
     * @param data the CompositeData objects to append, or set if there is no existing data.
     * @throws EvioException if data is null or empty;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
	public void addCompositeData(CompositeData[] data) throws EvioException {

        if (data == null || data.length < 1) {
            throw new EvioException("no data to add");
        }

        if (currentStructure == null) {
            throw new EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure.dataType != DataType.COMPOSITE) {
            throw new EvioException("may only add " + currentStructure.dataType + " data");
        }

        // Composite data is always in the local (in this case, BIG) endian
        // because if generated in JVM that's true, and if read in, it is
        // swapped to local if necessary. Either case it's big endian.

        // Convert composite data into byte array (already padded)
        byte[] rawBytes = CompositeData.generateRawBytes(data, order);

        // Sets pos = 0, limit = capacity, & does NOT clear data
        buffer.clear();

        int len = rawBytes.length;
        if (buffer.limit() - position < len) {
            throw new EvioException("no room in buffer");
        }

        // This method cannot be called multiple times in succession.
        if (currentStructure.dataLen > 0) {
            throw new EvioException("addCompositeData() may only be called once per structure");
        }

        if (useByteBuffer) {
            buffer.position(position);
            buffer.put(rawBytes);
            buffer.position(0);
        }
        else {
            System.arraycopy(rawBytes, 0, array, arrayOffset + position, len);
        }
        currentStructure.dataLen += len;
        addToAllLengths(len/4);

        position += len;
	}


    /**
     * This method writes a file in proper evio format with block header
     * containing the single event constructed by this object. This is
     * useful as a test to see if event is being properly constructed.
     * Use this in conjunction with the event viewer in order to check
     * that format is correct. This only works with array backed buffers.
     *
     * @param filename name of file
     */
    public void toFile(String filename) {
        try {
            File file = new File(filename);
            FileOutputStream fos = new FileOutputStream(file);
            java.io.DataOutputStream dos = new java.io.DataOutputStream(fos);

            boolean writeLastBlock = false;

            // Get buffer ready to read
            buffer.limit(position);
            buffer.position(0);

            // Write beginning 8 word header
            // total length
            int len = buffer.remaining();
//System.out.println("Buf len = " + len + " bytes");
            int len2 = len/4 + 8;
//System.out.println("Tot len = " + len2 + " words");
            // blk #
            int blockNum = 1;
            // header length
            int headerLen = 8;
            // event count
            int evCount = 1;
            // bit & version (last block, evio version 4 */
            int info = 0x204;
            if (writeLastBlock) {
                info = 0x4;
            }

            // endian checker
            int magicNum = IBlockHeader.MAGIC_NUMBER;

            if (order == ByteOrder.LITTLE_ENDIAN) {
                len2      = Integer.reverseBytes(len2);
                blockNum  = Integer.reverseBytes(blockNum);
                headerLen = Integer.reverseBytes(headerLen);
                evCount   = Integer.reverseBytes(evCount);
                info      = Integer.reverseBytes(info);
                magicNum  = Integer.reverseBytes(magicNum);
            }

            // Write the block header

            dos.writeInt(len2);       // len
            dos.writeInt(blockNum);   // block num
            dos.writeInt(headerLen);  // header len
            dos.writeInt(evCount);    // event count
            dos.writeInt(0);          // reserved 1
            dos.writeInt(info);       // bit & version info
            dos.writeInt(0);          // reserved 2
            dos.writeInt(magicNum);   // magic #

            // Now write the event data
            dos.write(array, 0, len);

            dos.close();
        }
        catch (IOException e) {
            e.printStackTrace();
        }

        // Reset position & limit
        buffer.clear();
    }

}
