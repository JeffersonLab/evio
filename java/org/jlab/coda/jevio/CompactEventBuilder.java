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
import java.util.LinkedList;

/**
 * This class is used for creating events
 * and their substructures while minimizing use of Java objects.
 * Evio format data of a single event is written directly,
 * and sequentially, into a buffer. The buffer contains
 * only the single event, not the full file format.
 *
 * @author timmer
 * Feb 6 2014
 */
public class CompactEventBuilder {

//    private EvioNode eventNode;

    /** Buffer in which to write. */
    private ByteBuffer buffer;

    /** Byte array which backs the buffer. */
    private byte[] array;

    /** Current writing position in the buffer. */
    private int position;

    /** Byte order of the buffer, convenience variable. */
    private ByteOrder order;

    /** Test 2 alternate way of writing data, one which is direct to the
     *  backing array and the other using ByteBuffer object methods. */
    private boolean useByteBuffer = false;

    /** Maximum allowed evio structure levels in buffer. */
    private static final int MAX_LEVELS = 100;

    /** Number of bytes to pad short and byte data. */
    private static final int[] padCount = {0,3,2,1};


    /** This class stores information about an evio structure
     * (bank, segment, or tagsegment) which allows us to write
     * a length or padding in that structure in the buffer.
     */
    private class StructureContent {
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


        StructureContent(int pos, DataType type, DataType dataType) {
            this.pos = pos;
            this.type = type;
            this.dataType = dataType;
        }
    }

    /** The top of the stack is parent of the current structure
     *  and the next one down is the parent of the parent, etc. */
    private LinkedList<StructureContent> stack;

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
        position = 0;

        // Init variables
        stack = new LinkedList<StructureContent>();
        totalLengths = new int[MAX_LEVELS];
        currentLevel = -1;
        currentStructure = null;
   	}


    /**
     * This is the constructor to use for building a new event
     * (just the event in a buffer, not the full evio file format).
     *
     * @param buffer the byte buffer to write into.
     *
     * @throws EvioException if arg is null;
     *                       if buffer is too small
     */
    public CompactEventBuilder(ByteBuffer buffer, boolean useBuffer)
                    throws EvioException {

        if (buffer == null) {
            throw new EvioException("null arg(s)");
        }

        // Prepare buffer
        this.buffer = buffer;
        buffer.clear();
        order = buffer.order();
        position = 0;

        if (buffer.limit() < 2) {
            throw new EvioException("buffer too small");
        }

        if (useBuffer) {
            useByteBuffer = true;
        }
        else {
            try {
                array = buffer.array();
            }
            catch (Exception e) {
                // If there is no backing array, use only the buffer
                useByteBuffer = true;
            }
        }

        // Init variables
        stack = new LinkedList<StructureContent>();
        totalLengths = new int[MAX_LEVELS];
        currentLevel = -1;
        currentStructure = null;
   	}




    /**
   	 * Get the underlying event.
   	 * @return the underlying event.
   	 */
   	public EvioNode getEvent() {
   		return null;
   	}


    /**
     * This method adds an evio segment structure to the buffer.
     *
     * @param tag tag of segment header
     * @param dataType type of data to be contained by this segment
     * @throws EvioException if containing structure does not hold segments;
     *                       if no room in buffer for segment header;
     *                       if too many nested evio structures;
     *                       if top-level bank has not been added first;
     */
    public void openSegment(int tag, DataType dataType)
        throws EvioException {

        if (currentStructure == null) {
            throw new EvioException("add a bank (event) first");
        }

        // Segments not allowed if parent holds different type
        if (currentStructure != null &&
                (currentStructure.dataType != DataType.SEGMENT &&
                 currentStructure.dataType != DataType.ALSOSEGMENT)) {
            throw new EvioException("may NOT add segment type");
        }

        if (buffer.remaining() < 4) {
            throw new EvioException("no room in buffer");
        }

        // For now, assume length and padding = 0
        if (useByteBuffer) {
            if (order == ByteOrder.BIG_ENDIAN) {
                buffer.put(position, (byte)tag);
                buffer.put(position+1, (byte)(dataType.getValue() & 0x3f));
            }
            else {
                buffer.put(position+2, (byte)(dataType.getValue() & 0x3f));
                buffer.put(position+3, (byte)tag);
            }
        }
        else {
            if (order == ByteOrder.BIG_ENDIAN) {
                array[position]   = (byte)tag;
                array[position+1] = (byte)(dataType.getValue() & 0x3f);
            }
            else {
                array[position+2] = (byte)(dataType.getValue() & 0x3f);
                array[position+3] = (byte)tag;
            }
        }

        // Put current structure, if any, onto stack as it's now a parent
        if (currentStructure != null) {
            stack.addFirst(currentStructure);
        }

        // Since we're adding a structure, we're going down a level (like push)
        currentLevel++;
        if (currentLevel >= MAX_LEVELS) {
            throw new EvioException("too many nested evio structures, increase MAX_LEVELS");
        }

        // We just wrote a segment header so this and all
        // containing levels increase in length by 1.
        addToAllLengths(1);

        // Current structure is the segment we are creating right now
        currentStructure = new StructureContent(position, DataType.SEGMENT, dataType);

        position += 4;
        buffer.position(position);
    }


    /**
     * This method adds an evio tagsegment structure to the buffer.
     *
     * @param tag tag of tagsegment header
     * @param dataType type of data to be contained by this tagsegment
     * @throws EvioException if containing structure does not hold tagsegments;
     *                       if no room in buffer for tagsegment header;
     *                       if too many nested evio structures;
     *                       if top-level bank has not been added first;
     */
    public void openTagSegment(int tag, DataType dataType)
            throws EvioException {

        if (currentStructure == null) {
            throw new EvioException("add a bank (event) first");
        }

        // Tagsegments not allowed if parent holds different type
        if (currentStructure != null &&
            currentStructure.dataType != DataType.TAGSEGMENT) {
            throw new EvioException("may NOT add tagsegment type");
        }

        if (buffer.remaining() < 4) {
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
                array[position]   = (byte) (compositeWord >> 8);
                array[position+1] = (byte) compositeWord;
            }
            else {
                array[position+2] = (byte) compositeWord;
                array[position+3] = (byte) (compositeWord >> 8);
            }
        }

        // Put current structure, if any, onto stack as it's now a parent
        if (currentStructure != null) {
            stack.addFirst(currentStructure);
        }

        // Since we're adding a structure, we're going down a level (like push)
        currentLevel++;
        if (currentLevel >= MAX_LEVELS) {
            throw new EvioException("too many nested evio structures, increase MAX_LEVELS");
        }

        // We just wrote a tagsegment header so this and all
        // containing levels increase in length by 1.
        addToAllLengths(1);

        // Current structure is the tagsegment we are creating right now
        currentStructure = new StructureContent(position, DataType.TAGSEGMENT, dataType);

        position += 4;
        buffer.position(position);
    }


    /**
     * This method adds an evio bank to the buffer.
     *
     * @param tag tag of bank header
     * @param num num of bank header
     * @param dataType data type to be contained in this bank
     * @throws EvioException if containing structure does not hold banks;
     *                       if no room in buffer for bank header;
     *                       if too many nested evio structures;
     */
    public void openBank(int tag, int num, DataType dataType)
            throws EvioException {

        // Banks not allowed if parent holds different type
        if (currentStructure != null &&
                (currentStructure.dataType != DataType.BANK &&
                 currentStructure.dataType != DataType.ALSOBANK)) {
            throw new EvioException("may NOT add bank type");
        }

        if (buffer.remaining() < 8) {
            throw new EvioException("no room in buffer");
        }

        // Write bank header into buffer, assuming padding = 0
        if (useByteBuffer) {
            // Bank w/ no data has len = 1
            buffer.putInt(1);

            if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                buffer.putShort(position + 4, (short)tag);
                buffer.put(position + 6,(byte)(dataType.getValue() & 0x3f));
                buffer.put(position + 7,(byte)num);
            }
            else {
                buffer.put(position + 4, (byte)num);
                buffer.put(position + 5, (byte)(dataType.getValue() & 0x3f));
                buffer.putShort(position + 6, (short)tag);
            }
        }
        else {
            // Bank w/ no data has len = 1
            ByteDataTransformer.toBytes(1, order, array, position);

            if (order == ByteOrder.BIG_ENDIAN) {
                array[position+4] = (byte)(tag >> 8);
                array[position+5] = (byte)tag;
                array[position+6] = (byte)(dataType.getValue() & 0x3f);
                array[position+7] = (byte)num;
            }
            else {
                array[position+4] = (byte)num;
                array[position+5] = (byte)(dataType.getValue() & 0x3f);
                array[position+6] = (byte)tag;
                array[position+7] = (byte)(tag >> 8);
            }
        }

        // Put current structure, if any, onto stack as it's now a parent
        if (currentStructure != null) {
            stack.addFirst(currentStructure);
        }

        // Since we're adding a structure, we're going down a level (like push)
        currentLevel++;
        if (currentLevel >= MAX_LEVELS) {
            throw new EvioException("too many nested evio structures, increase MAX_LEVELS");
        }

        // We just wrote a bank header so this and all
        // containing levels increase in length by 2.
        addToAllLengths(2);

        // Current structure is the bank we are creating right now
        currentStructure = new StructureContent(position, DataType.BANK, dataType);

        position += 8;
        buffer.position(position);
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
        if (currentStructure == null) {
            return true;
        }

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
        currentLevel--;

        // The new current structure is ..
        currentStructure = stack.poll();

        // If structure is null, we've reached the top and we're done
        return (currentStructure == null);
    }


    /**
     * This method finishes the event writing by setting all the
     * proper lengths & padding and ends up at the event or top level.
     */
    public void closeAll() {
        while (!closeStructure()) {}
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
                        ByteDataTransformer.toBytes(len, order, array, currentStructure.pos);
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
                                                        array, currentStructure.pos+2);
                        }
                        else {
                            ByteDataTransformer.toBytes((short)len, order,
                                                        array, currentStructure.pos);
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
                        buffer.put(currentStructure.pos+6,b);
                    }
                    else {
                        buffer.put(currentStructure.pos+5, b);
                    }
                }
                else {
                    if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                        array[currentStructure.pos+6] = b;
                    }
                    else {
                        array[currentStructure.pos+5] = b;
                    }
                }

                return;

            case SEGMENT:
            case ALSOSEGMENT:

                if (useByteBuffer) {
                    if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                        buffer.put(currentStructure.pos+1,b);
                    }
                    else {
                        buffer.put(currentStructure.pos+2, b);
                    }
                }
                else {
                    if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                        array[currentStructure.pos+1] = b;
                    }
                    else {
                        array[currentStructure.pos+2] = b;
                    }
                }
                return;

            default:
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
     *                       if no room in buffer for data;
     *                       if node's buffer is opposite endian.
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

        if (node.bufferNode.buffer.order() != buffer.order()) {
            throw new EvioException("node has opposite endian");
        }

        int len = node.getTotalBytes();
        if (buffer.remaining() < len) {
            throw new EvioException("no room in buffer");
        }

        addToAllLengths(len/4);  // # 32-bit words

//        ByteBuffer nodeBuf = node.bufferNode.getBuffer().duplicate();
//        nodeBuf.position(node.getPosition());
//        nodeBuf.limit(node.getDataPosition() + 4 * node.getDataLength());
//        buffer.put(nodeBuf);

        // Better performance not to duplicate buffer (5 - 10% faster)
        ByteBuffer buf = node.bufferNode.getBuffer();
        int oldPos = buf.position();
        int oldLimit = buf.limit();
        buf.position(node.getPosition());
        buf.limit(node.getPosition() + node.getTotalBytes());
        buffer.put(buf);
        buf.position(oldPos);
        buf.limit(oldLimit);

        // If reading a file, byte buffer is from mem mapped file, so NO backing array.
//        System.arraycopy(node.bufferNode.getBuffer().array(), node.getPosition(),
//                         array, position, node.getTotalBytes());



        position += len;     // # bytes
        buffer.position(position);
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
        if (data == null || data.length < 1) {
            throw new EvioException("no data to add");
        }

        if (currentStructure == null) {
            throw new EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure.dataType != DataType.CHAR8) {
            throw new EvioException("may only add " + currentStructure.dataType + " data");
        }

        int len = data.length;
        if (buffer.remaining() < len) {
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
            buffer.put(data);
        }
        else {
            System.arraycopy(data, 0, array, position, len);
        }

        // Calculate the padding
        currentStructure.padding = padCount[currentStructure.dataLen % 4];

        // Advance buffer position
        position += len + currentStructure.padding;
        buffer.position(position);
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
        if (data == null || data.length < 1) {
            throw new EvioException("no data to add");
        }

        if (currentStructure == null) {
            throw new EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure.dataType != DataType.INT32) {
            throw new EvioException("may only add " + currentStructure.dataType + " data");
        }

        int len = data.length;
        if (buffer.remaining() < 4*len) {
            throw new EvioException("no room in buffer");
        }

        addToAllLengths(len);  // # 32-bit words

        if (useByteBuffer) {
            IntBuffer db = buffer.asIntBuffer();
            db.put(data, 0, len);
        }
        else {
            int pos = position;
            if (order == ByteOrder.BIG_ENDIAN) {
                for (int aData : data) {
                    array[pos++] = (byte) (aData >> 24);
                    array[pos++] = (byte) (aData >> 16);
                    array[pos++] = (byte) (aData >> 8);
                    array[pos  ] = (byte) (aData);
                }
            }
            else {
                for (int aData : data) {
                    array[pos++] = (byte) (aData);
                    array[pos++] = (byte) (aData >> 8);
                    array[pos++] = (byte) (aData >> 16);
                    array[pos  ] = (byte) (aData >> 24);
                }
            }
        }

        position += 4*len;     // # bytes
        buffer.position(position);
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
        if (data == null || data.length < 1) {
            throw new EvioException("no data to add");
        }

        if (currentStructure == null) {
            throw new EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure.dataType != DataType.SHORT16) {
            throw new EvioException("may only add " + currentStructure.dataType + " data");
        }

        int len = data.length;
        if (buffer.remaining() < 2*len) {
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
            ShortBuffer db = buffer.asShortBuffer();
            db.put(data, 0, len);
        }
        else {
            int pos = position;
            if (order == ByteOrder.BIG_ENDIAN) {
                for (short aData : data) {
                    array[pos++] = (byte) (aData >> 8);
                    array[pos  ] = (byte) (aData);
                }
            }
            else {
                for (short aData : data) {
                    array[pos++] = (byte) (aData);
                    array[pos  ] = (byte) (aData >> 8);
                }
            }
        }

        currentStructure.padding = 2*(currentStructure.dataLen % 2);
//System.out.println("short padding = " + currentStructure.padding);
        // Advance buffer position
        position += 2*len + currentStructure.padding;
        buffer.position(position);
//System.out.println("set pos = " + position);
//System.out.println("");
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
        if (data == null || data.length < 1) {
            throw new EvioException("no data to add");
        }

        if (currentStructure == null) {
            throw new EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure.dataType != DataType.LONG64) {
            throw new EvioException("may only add " + currentStructure.dataType + " data");
        }

        int len = data.length;
        if (buffer.remaining() < 8*len) {
            throw new EvioException("no room in buffer");
        }

        addToAllLengths(2*len);  // # 32-bit words

        if (useByteBuffer) {
            LongBuffer db = buffer.asLongBuffer();
            db.put(data, 0, len);
        }
        else {
            int pos = position;
            if (order == ByteOrder.BIG_ENDIAN) {
                for (long aData : data) {
                    array[pos++] = (byte) (aData >> 56);
                    array[pos++] = (byte) (aData >> 48);
                    array[pos++] = (byte) (aData >> 40);
                    array[pos++] = (byte) (aData >> 32);
                    array[pos++] = (byte) (aData >> 24);
                    array[pos++] = (byte) (aData >> 16);
                    array[pos++] = (byte) (aData >> 8);
                    array[pos  ] = (byte) (aData);
                }
            }
            else {
                for (long aData : data) {
                    array[pos++] = (byte) (aData);
                    array[pos++] = (byte) (aData >> 8);
                    array[pos++] = (byte) (aData >> 16);
                    array[pos++] = (byte) (aData >> 24);
                    array[pos++] = (byte) (aData >> 32);
                    array[pos++] = (byte) (aData >> 40);
                    array[pos++] = (byte) (aData >> 48);
                    array[pos  ] = (byte) (aData >> 56);
                }
            }
        }

        position += 8*len;       // # bytes
        buffer.position(position);
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
            throw new EvioException("may only add " + currentStructure.dataType + " data");
        }

        int len = data.length;
        if (buffer.remaining() < 4*len) {
            throw new EvioException("no room in buffer");
        }

        addToAllLengths(len);  // # 32-bit words

        if (useByteBuffer) {
            FloatBuffer db = buffer.asFloatBuffer();
            db.put(data, 0, len);
        }
        else {
            int aData, pos = position;
            if (order == ByteOrder.BIG_ENDIAN) {
                for (float fData : data) {
                    aData = Float.floatToRawIntBits(fData);
                    array[pos++] = (byte) (aData >> 24);
                    array[pos++] = (byte) (aData >> 16);
                    array[pos++] = (byte) (aData >> 8);
                    array[pos  ] = (byte) (aData);
                }
            }
            else {
                for (float fData : data) {
                    aData = Float.floatToRawIntBits(fData);
                    array[pos++] = (byte) (aData);
                    array[pos++] = (byte) (aData >> 8);
                    array[pos++] = (byte) (aData >> 16);
                    array[pos  ] = (byte) (aData >> 24);
                }
            }
        }

        position += 4*len;     // # bytes
        buffer.position(position);
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
            throw new EvioException("may only add " + currentStructure.dataType + " data");
        }

        int len = data.length;
        if (buffer.remaining() < 8*len) {
            throw new EvioException("no room in buffer");
        }

        addToAllLengths(2*len);  // # 32-bit words

        if (useByteBuffer) {
            DoubleBuffer db = buffer.asDoubleBuffer();
            db.put(data, 0, len);
        }
        else {
            long aData;
            int pos = position;
            if (order == ByteOrder.BIG_ENDIAN) {
                for (double dData : data) {
                    aData = Double.doubleToRawLongBits(dData);
                    array[pos++] = (byte) (aData >> 56);
                    array[pos++] = (byte) (aData >> 48);
                    array[pos++] = (byte) (aData >> 40);
                    array[pos++] = (byte) (aData >> 32);
                    array[pos++] = (byte) (aData >> 24);
                    array[pos++] = (byte) (aData >> 16);
                    array[pos++] = (byte) (aData >> 8);
                    array[pos  ] = (byte) (aData);
                }
            }
            else {
                for (double dData : data) {
                    aData = Double.doubleToRawLongBits(dData);
                    array[pos++] = (byte) (aData);
                    array[pos++] = (byte) (aData >> 8);
                    array[pos++] = (byte) (aData >> 16);
                    array[pos++] = (byte) (aData >> 24);
                    array[pos++] = (byte) (aData >> 32);
                    array[pos++] = (byte) (aData >> 40);
                    array[pos++] = (byte) (aData >> 48);
                    array[pos  ] = (byte) (aData >> 56);
                }
            }
        }

        position += 8*len;     // # bytes
        buffer.position(position);
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

        int len = data.length;
        if (buffer.remaining() < len) {
            throw new EvioException("no room in buffer");
        }

        // Things get tricky if this method is called multiple times in succession.
        // In fact, it's too difficult to bother with so just throw an exception.
        if (currentStructure.dataLen > 0) {
            throw new EvioException("addStringData() may only be called once per structure");
        }

        if (useByteBuffer) {
            buffer.put(data);
        }
        else {
            System.arraycopy(data, 0, array, position, len);
        }
        currentStructure.dataLen += len;
        addToAllLengths(len/4);

        position += len;
        buffer.position(position);
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
            buffer.flip();

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
    }

}
