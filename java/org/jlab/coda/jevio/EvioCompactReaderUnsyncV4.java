/*
 * Copyright (c) 2018, Jefferson Science Associates, all rights reserved.
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 */

package org.jlab.coda.jevio;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/**
 * <p>This class is used to read an evio format version 4 formatted file or buffer
 * and extract specific evio containers (bank, seg, or tagseg)
 * with actual data in them given a tag/num pair. It is NOT thread-safe since
 * all synchronization is removed in order to gain speed.</p>
 *
 * <p>Although this class can be used directly, it's generally used by
 * using {@link EvioCompactReader} which, in turn, uses this class.</p>
 *
 * For a list of methods that cannot be run simultaneously, look at
 * the synchronized methods in EvioCompactReaderV4.
 *
 * @author timmer
 */
class EvioCompactReaderUnsyncV4 implements IEvioCompactReader {

    /** Offset to get block size from start of block. */
    static final int BLOCK_SIZE_OFFSET = 0;

    /** Offset to get block number from start of block. */
    static final int BLOCK_NUMBER = 4;

    /** Offset to get block header size from start of block. */
    static final int BLOCK_HEADER_SIZE_OFFSET = 8;

    /** Offset to get block event count from start of block. */
    static final int BLOCK_EVENT_COUNT = 12;

    /** Offset to get block size from start of block. */
    static final int BLOCK_RESERVED_1 = 16;

    /** Mask to get version number from 6th int in block. */
    static final int VERSION_MASK = 0xff;


    /** Stores info of all the (top-level) events. */
    protected final ArrayList<EvioNode> eventNodes = new ArrayList<>(1000);

    /**
     * Are we using and storing block header objects or not? If user will be calling the
     * {@link #addStructure(int, ByteBuffer)} or {@link #removeEvent(int)}, or
     * {@link #removeEvent(int)} methods, then block header objects need to be
     * created and stored.
     * For fast parsing of streamed data, because the storage of block headers
     * generates garbage, it's performant to turn it off. Can't be messing with
     * the structure of events though.
     * Keep it on by default to avoid surprises.
     */
    protected boolean usingBlockHdrs = true;

    /** Store info of all block headers. */
    protected final HashMap<Integer, BlockNode> blockNodes = new HashMap<>(20);

    /** Source (pool) of EvioNode objects used for parsing Evio data in buffer. */
    protected EvioNodeSource nodePool;


    /**
     * This is the number of events in the file. It is not computed unless asked for,
     * and if asked for it is computed and cached in this variable.
     */
    protected int eventCount = -1;

    /** Evio version number (1-4). Obtain this by reading first block header. */
    protected int evioVersion;

    /**
     * Endianness of the data being read, either
     * {@link ByteOrder#BIG_ENDIAN} or
     * {@link ByteOrder#LITTLE_ENDIAN}.
     */
    protected ByteOrder byteOrder;

    /**
     * This is the number of blocks in the file including the empty block at the
     * end of the version 4 files. It is not computed unless asked for,
     * and if asked for it is computed and cached in this variable.
     */
    protected int blockCount = -1;

    /** Size of the first block header in 32-bit words. Used to read dictionary. */
    protected int firstBlockHeaderWords;

    /** The current block header. */
    protected BlockHeaderV4 blockHeader = new BlockHeaderV4();

    /** Does the file/buffer have a dictionary? */
    protected boolean hasDictionary;

    /**
     * Version 4 files may have an xml format dictionary in the
     * first event of the first block.
     */
    protected String dictionaryXML;

    /** Dictionary object created from dictionaryXML string. */
    protected EvioXMLDictionary dictionary;

    /** The buffer being read. */
    protected ByteBuffer byteBuffer;

    /** Initial position of buffer (mappedByteBuffer if reading a file). */
    protected int initialPosition;

    /** How much of the buffer being read is valid evio data (in 32bit words)?
     *  The valid data begins at initialPosition and ends after this length.*/
    protected int validDataWords;

    /** Is this object currently closed? */
    protected boolean closed;

    //------------------------
    // File specific members
    //------------------------

    /** Are we reading a file or buffer? */
    protected boolean isFile;

    /**
     * The buffer representing a map of the input file which is also
     * accessed through {@link #byteBuffer}.
     */
    protected MappedByteBuffer mappedByteBuffer;

    /** Absolute path of the underlying file. */
    protected String path;

    /** File size in bytes. */
    protected long fileSize;



    /**
     * Constructor for reading an event file.
     *
     * @param path the full path to the file that contains events.
     *             For writing event files, use an <code>EventWriter</code> object.
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null
     */
    public EvioCompactReaderUnsyncV4(String path) throws EvioException, IOException {
        this(new File(path));
    }

    /**
     * Constructor for reading an event file.
     *
     * @param file the file that contains events.
     *
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null; file is too large;
     */
    public EvioCompactReaderUnsyncV4(File file) throws EvioException, IOException {
        if (file == null) {
            throw new EvioException("File arg is null");
        }

        FileInputStream fileInputStream = new FileInputStream(file);
        path = file.getAbsolutePath();
        FileChannel fileChannel = fileInputStream.getChannel();
        fileSize = fileChannel.size();

        // Is the file byte size > max int value?
        // If so we cannot use a memory mapped file.
        if (fileSize > Integer.MAX_VALUE) {
            throw new EvioException("file too large (must be < 2.1475GB)");
        }

        mapFile(fileChannel);
        fileChannel.close(); // this object is no longer needed since we have the map

        initialPosition = 0;

        // Read first block header and find the file's endianness & evio version #.
        // If there's a dictionary, read that too.
        if (readFirstHeader() != ReadStatus.SUCCESS) {
            throw new IOException("Failed reading first block header/dictionary");
        }

        // Generate a table of all event positions in buffer for random access.
        generateEventPositionTable();

        isFile = true;
    }

    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     *
     * @see EventWriter
     * @throws EvioException if buffer arg is null;
     *                       failure to read first block header
     */
    public EvioCompactReaderUnsyncV4(ByteBuffer byteBuffer) throws EvioException {
        this(byteBuffer, null, true);
    }


    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     * @param pool pool of EvioNode objects to use when parsing buf.
     *
     * @see EventWriter
     * @throws EvioException if buffer arg is null;
     *                       failure to read first block header
     */
    public EvioCompactReaderUnsyncV4(ByteBuffer byteBuffer, EvioNodeSource pool) throws EvioException {
        this(byteBuffer, pool, true);
    }


    /**
     * Constructor for reading a buffer.
     * Turning off the use of BlockNode objects results in NOT being able to call the
     * {@link #addStructure(int, ByteBuffer)} {@link #removeEvent(int)}, or
     * {@link #removeEvent(int)} methods. It does reduce garbage generation for
     * applications that do not need to call these (e.g streaming data in CODA).
     *
     * @param byteBuffer the buffer that contains events.
     * @param pool pool of EvioNode objects to use when parsing buf.
     * @param useBlockHeaders if false, do not create and store BlockNode objects.
     *
     * @see EventWriter
     * @throws EvioException if buffer arg is null;
     *                       failure to read first block header
     */
    public EvioCompactReaderUnsyncV4(ByteBuffer byteBuffer, EvioNodeSource pool, boolean useBlockHeaders)
            throws EvioException {

        if (byteBuffer == null) {
            throw new EvioException("Buffer arg is null");
        }

        initialPosition = byteBuffer.position();
        this.byteBuffer = byteBuffer;
        nodePool = pool;
        usingBlockHdrs = useBlockHeaders;

        // Read first block header and find the file's endianness & evio version #.
        // If there's a dictionary, read that too.
        if (readFirstHeader() != ReadStatus.SUCCESS) {
            throw new EvioException("Failed reading first block header/dictionary");
        }

        // Generate a table of all event positions in buffer for random access.
        generateEventPositionTable();
    }


    /**
     * This method can be used to avoid creating additional EvioCompactReader
     * objects by reusing this one with another buffer. The method
     * {@link #close()} is called before anything else.
     *
     * @param buf ByteBuffer to be read
     * @throws EvioException if arg is null;
     *                       if failure to read first block header
     */
    public void setBuffer(ByteBuffer buf) throws EvioException {
        if (buf == null) {
            throw new EvioException("arg is null");
        }

        blockNodes.clear();
        eventNodes.clear();

        blockCount      = -1;
        eventCount      = -1;
        dictionaryXML   = null;
        initialPosition = buf.position();
        this.byteBuffer = buf;

        if (readFirstHeader() != ReadStatus.SUCCESS) {
            throw new EvioException("Failed reading first block header/dictionary");
        }

        generateEventPositionTable();
        closed = false;
    }


    /**
     * This method can be used to avoid creating additional EvioCompactReader
     * objects by reusing this one with another buffer. The method
     * {@link #close()} is called before anything else.
     *
     * @param buf ByteBuffer to be read
     * @throws EvioException if arg is null;
     *                       if failure to read first block header
     */
    public void setBuffer(ByteBuffer buf, EvioNodeSource pool) throws EvioException {
        if (buf == null) {
            throw new EvioException("arg is null");
        }

        blockNodes.clear();
        eventNodes.clear();
        nodePool = pool;

        blockCount      = -1;
        eventCount      = -1;
        dictionaryXML   = null;
        initialPosition = buf.position();
        this.byteBuffer = buf;

        if (readFirstHeader() != ReadStatus.SUCCESS) {
            throw new EvioException("Failed reading first block header/dictionary");
        }

        generateEventPositionTable();
        closed = false;
    }

    /** {@inheritDoc} */
    public ByteBuffer setCompressedBuffer(ByteBuffer buf, EvioNodeSource pool)
            throws EvioException {
        setBuffer(buf, pool);
        return buf;
    }

    /** {@inheritDoc} */
    public boolean isFile() {return isFile;}

    /** {@inheritDoc} */
    public boolean isCompressed() {return false;}

    /**
     * Has {@link #close()} been called (without reopening by calling
     * {@link #setBuffer(ByteBuffer)})?
     *
     * @return {@code true} if this object closed, else {@code false}.
     */
    public boolean isClosed() {return closed;}

    /**
     * Get the byte order of the file/buffer being read.
     * @return byte order of the file/buffer being read.
     */
    public ByteOrder getByteOrder() {return byteOrder;}

    /**
     * Get the evio version number.
     * @return evio version number.
     */
    public int getEvioVersion() {
        return evioVersion;
    }

    /**
      * Get the path to the file.
      * @return path to the file
      */
     public String getPath() { return null; }

    /**
     * Since we're not reading a file, always return null.
     * @return null.
     */
    public ByteOrder getFileByteOrder() {return null;}

    /**
     * Get the XML format dictionary is there is one.
     * @throws EvioException if object closed and dictionary still unread
     * @return XML format dictionary, else null.
     */
    public  String getDictionaryXML() throws EvioException {
        if (dictionaryXML != null) return dictionaryXML;

        if (closed) {
            throw new EvioException("object closed");
        }

        // Is there a dictionary? If so, read it now if we haven't already.
        if (hasDictionary) {
            try {
                readDictionary();
            }
            catch (Exception e) {}
        }

        return dictionaryXML;
    }

     /**
      * Get the evio dictionary if is there is one.
      * @throws EvioException if object closed and dictionary still unread
      * @return evio dictionary if exists, else null.
      */
     public  EvioXMLDictionary getDictionary() throws EvioException {
         if (dictionary != null) return dictionary;

         if (closed) {
             throw new EvioException("object closed");
         }

         // Is there a dictionary? If so, read it now if we haven't already.
         if (hasDictionary) {
             try {
                 if (dictionaryXML == null) {
                     readDictionary();
                 }
                 dictionary = new EvioXMLDictionary(dictionaryXML);
             }
             catch (Exception e) {}
         }

         return dictionary;
     }

    /**
     * Does this evio file have an associated XML dictionary?
     *
     * @return <code>true</code> if this evio file has an associated XML dictionary,
     *         else <code>false</code>
     */
    public boolean hasDictionary() {
         return hasDictionary;
    }

    /**
 	 * Maps the file into memory. The data are not actually loaded in memory-- subsequent reads will read
 	 * random-access-like from the file.
 	 *
      * @param inputChannel the input channel.
      * @throws IOException if file cannot be opened
 	 */
 	protected void mapFile(FileChannel inputChannel) throws IOException {
 		long sz = inputChannel.size();
 		mappedByteBuffer = inputChannel.map(FileChannel.MapMode.READ_ONLY, 0L, sz);
        byteBuffer = mappedByteBuffer;
 	}

    /**
     * Get the memory mapped buffer corresponding to the event file.
     * @return the memory mapped buffer corresponding to the event file.
     */
    public MappedByteBuffer getMappedByteBuffer() {
        return mappedByteBuffer;
    }

    /**
     * Get the byte buffer being read directly or corresponding to the event file.
     * @return the byte buffer being read directly or corresponding to the event file.
     */
    public ByteBuffer getByteBuffer() {
        return byteBuffer;
    }

    /**
     * Get the size of the file being read, in bytes.
     * For small files, obtain the file size using the memory mapped buffer's capacity.
     * @return the file size in bytes
     */
    public long fileSize() {
        return fileSize;
    }

    /**
     * Get the EvioNode object associated with a particular event number.
     * @param eventNumber number of event (place in file/buffer) starting at 1.
     * @return  EvioNode object associated with a particular event number,
     *          or null if there is none.
     */
    public EvioNode getEvent(int eventNumber) {
        try {
            return eventNodes.get(eventNumber - 1);
        }
        catch (IndexOutOfBoundsException e) { }
        return null;
    }


    /**
     * Get the EvioNode object associated with a particular event number
     * which has been scanned so all substructures are contained in the
     * node.allNodes list.
     * @param eventNumber number of event (place in file/buffer) starting at 1.
     * @return  EvioNode object associated with a particular event number,
     *
     *         or null if there is none.
     */
    public EvioNode getScannedEvent(int eventNumber) {
        try {
            return scanStructure(eventNumber);
        }
        catch (IndexOutOfBoundsException e) { }
        return null;
    }


    /**
     * Get the EvioNode object associated with a particular event number
     * which has been scanned so all substructures are contained in the
     * node.allNodes list.
     * @param eventNumber number of event (place in file/buffer) starting at 1.
     * @param nodeSource  source of EvioNode objects to use while parsing evio data.
     * @return  EvioNode object associated with a particular event number,
     *          or null if there is none.
     */
    public EvioNode getScannedEvent(int eventNumber, EvioNodeSource nodeSource) {
        try {
            return scanStructure(eventNumber, nodeSource);
        }
        catch (IndexOutOfBoundsException e) {}
        return null;
    }


    /**
     * Generate a table (ArrayList) of positions of events in file/buffer.
     * This method does <b>not</b> affect the byteBuffer position, eventNumber,
     * or lastBlock values. Uses only absolute gets so byteBuffer position
     * does not change. Called only in constructors and
     * {@link #setBuffer(ByteBuffer)}.
     *
     * @throws EvioException if bytes not in evio format
     */
    void generateEventPositionTable() throws EvioException {

        int      byteInfo, byteLen, blockHdrSize, blockSize, blockEventCount, magicNum;
        boolean  firstBlock=true, hasDictionary=false, isLastBlock;

//        long t2, t1 = System.currentTimeMillis();

        // Start at the beginning of byteBuffer without changing
        // its current position. Do this with absolute gets.
        int position  = initialPosition;
        int bytesLeft = byteBuffer.limit() - position;

        // Keep track of the # of blocks, events, and valid words in file/buffer
        blockCount = 0;
        eventCount = 0;
        validDataWords = 0;
        BlockNode blockNode = null;

//        int blockCounter = 0;
//        System.out.println("generateEventPositionTable:");

        try {

            while (bytesLeft > 0) {
                // Need enough data to at least read 1 block header (32 bytes)
                if (bytesLeft < 32) {
                    throw new EvioException("Bad evio format: extra " + bytesLeft +
                                                    " bytes at file end");
                }

                // Swapping is taken care of
                blockSize       = byteBuffer.getInt(position);
                byteInfo        = byteBuffer.getInt(position + 4*BlockHeaderV4.EV_VERSION);
                blockHdrSize    = byteBuffer.getInt(position + 4*BlockHeaderV4.EV_HEADERSIZE);
                blockEventCount = byteBuffer.getInt(position + 4*BlockHeaderV4.EV_COUNT);
                magicNum        = byteBuffer.getInt(position + 4*BlockHeaderV4.EV_MAGIC);
                isLastBlock     = BlockHeaderV4.isLastBlock(byteInfo);

//                System.out.println("    read block header " + blockCounter++ + ": " +
//                                           "ev count = " + blockEventCount +
//                                           ", blockSize = " + blockSize +
//                                           ", blockHdrSize = " + blockHdrSize +
//                                           ", byteInfo/ver  = 0x" + Integer.toHexString(byteInfo) +
//                                           ", magicNum = 0x" + Integer.toHexString(magicNum));

                // If magic # is not right, file is not in proper format
                if (magicNum != BlockHeaderV4.MAGIC_NUMBER) {
                    throw new EvioException("Bad evio format: block header magic # incorrect");
                }

                if (blockSize < 8 || blockHdrSize < 8) {
                    throw new EvioException("Bad evio format (block: len = " +
                                                    blockSize + ", blk header len = " + blockHdrSize + ")" );
                }

                // Check to see if the whole block is there
                if (4*blockSize > bytesLeft) {
//System.out.println("    4*blockSize = " + (4*blockSize) + " >? bytesLeft = " + bytesLeft +
//                   ", pos = " + position);
                    throw new EvioException("Bad evio format: not enough data to read block");
                }

                if (usingBlockHdrs) {
                    // File is now positioned before block header.
                    // Look at block header to get info.
                    blockNode = new BlockNode();

                    blockNode.pos = position;
                    blockNode.len = blockSize;
                    blockNode.count = blockEventCount;

                    blockNodes.put(blockCount, blockNode);
//                bufferNode.blockNodes.add(blockNode);

                    blockNode.place = blockCount;

//                // Make linked list of blocks
//                if (previousBlockNode != null) {
//                    previousBlockNode.nextBlock = blockNode;
//                }
//                else {
//                    previousBlockNode = blockNode;
//                }
                }
                blockCount++;

                validDataWords += blockSize;
                if (firstBlock) hasDictionary = BlockHeaderV4.hasDictionary(byteInfo);

                // Hop over block header to events
                position  += 4*blockHdrSize;
                bytesLeft -= 4*blockHdrSize;

//System.out.println("    hopped blk hdr, pos = " + position + ", is last blk = " + isLastBlock);

                // Check for a dictionary - the first event in the first block.
                // It's not included in the header block count, but we must take
                // it into account by skipping over it.
                if (firstBlock && hasDictionary) {
                    firstBlock = false;

                    // Get its length - bank's len does not include itself
                    byteLen = 4*(byteBuffer.getInt(position) + 1);

                    // Skip over dictionary
                    position  += byteLen;
                    bytesLeft -= byteLen;
//System.out.println("    hopped dict, pos = " + position);
                }

                // For each event in block, store its location
                for (int i=0; i < blockEventCount; i++) {
                    // Sanity check - must have at least 1 header's amount left
                    if (bytesLeft < 8) {
                        throw new EvioException("Bad evio format: not enough data to read event (bad bank len?)");
                    }

                    EvioNode node = EvioNode.extractEventNode(byteBuffer, nodePool, blockNode,
                                                              position, eventCount + i);
//System.out.println("      event "+i+" in block: pos = " + node.pos +
//                           ", dataPos = " + node.dataPos + ", ev # = " + (eventCount + i + 1));
                    eventNodes.add(node);
//                    if (storeBlockHdrs) {
//                        blockNode.allEventNodes.add(node);
//                    }

                    // Hop over header + data
                    byteLen = 8 + 4*node.dataLen;
                    position  += byteLen;
                    bytesLeft -= byteLen;

                    if (byteLen < 8 || bytesLeft < 0) {
                        throw new EvioException("Bad evio format: bad bank length");
                    }

//System.out.println("    hopped event " + (i+1) + ", bytesLeft = " + bytesLeft + ", pos = " + position + "\n");
                }

                eventCount += blockEventCount;
                
                if (isLastBlock) break;
            }
        }
        catch (IndexOutOfBoundsException e) {
            throw new EvioException("Bad evio format", e);
        }

//        t2 = System.currentTimeMillis();
//        System.out.println("Time to scan file = " + (t2-t1) + " milliseconds");
    }


    /**
     * Reads the first block (physical record) header in order to determine
     * characteristics of the file or buffer in question. These things
     * do <b>not</b> need to be examined in subsequent block headers.
     * File or buffer must be evio version 4 or greater.
     * Called only in constructors and setBuffer.
     *
     * @return status of read attempt
     */
    protected ReadStatus readFirstHeader() {
        // Get first block header
        int pos = initialPosition;

        // Have enough remaining bytes to read header?
        if (byteBuffer.limit() - pos < 32) {
System.out.println("     readFirstHeader: EOF, remaining = " + (byteBuffer.limit() - pos));
            byteBuffer.clear();
            return ReadStatus.END_OF_FILE;
        }

        try {
            // Set the byte order to match the file's ordering.

            // Check the magic number for endianness (buffer defaults to big endian)
            byteOrder = byteBuffer.order();

            int magicNumber = byteBuffer.getInt(pos + MAGIC_OFFSET);

            if (magicNumber != IBlockHeader.MAGIC_NUMBER) {

                if (byteOrder == ByteOrder.BIG_ENDIAN) {
                    byteOrder = ByteOrder.LITTLE_ENDIAN;
                }
                else {
                    byteOrder = ByteOrder.BIG_ENDIAN;
                }
                byteBuffer.order(byteOrder);

                // Reread magic number to make sure things are OK
                magicNumber = byteBuffer.getInt(pos + MAGIC_OFFSET);
                if (magicNumber != IBlockHeader.MAGIC_NUMBER) {
System.out.println("     readFirstHeader: reread magic # (" + magicNumber + ") & still not right");
Utilities.printBuffer(byteBuffer, 0, 8, "Tried to parse this as block header");
                    return ReadStatus.EVIO_EXCEPTION;
                }
            }

            // Check the version number
            int bitInfo = byteBuffer.getInt(pos + VERSION_OFFSET);
            evioVersion = bitInfo & VERSION_MASK;
            if (evioVersion < 4)  {
System.out.println("     readFirstHeader: unsupported evio version (" + evioVersion + ")");
                return ReadStatus.EVIO_EXCEPTION;
            }

            // Does this file/buffer have a dictionary?
            hasDictionary = BlockHeaderV4.hasDictionary(bitInfo);

            // # of words in first block header
            firstBlockHeaderWords = byteBuffer.getInt(pos + BLOCK_HEADER_SIZE_OFFSET);

            // Store first header data
            blockHeader.setSize(byteBuffer.getInt(pos + BLOCK_SIZE_OFFSET));
            blockHeader.setNumber(byteBuffer.getInt(pos + BLOCK_NUMBER));
            blockHeader.setHeaderLength(firstBlockHeaderWords);
            blockHeader.setEventCount(byteBuffer.getInt(pos + BLOCK_EVENT_COUNT));
            blockHeader.setReserved1(byteBuffer.getInt(pos + BLOCK_RESERVED_1));   // used from ROC

// TODO: do the equivalent in hipo library ...
            if (blockHeader.getSize() < 8) {
System.out.println("     readFirstHeader: block size too small, " + blockHeader.getSize());
                byteBuffer.clear();
                Utilities.printBuffer(byteBuffer, 0, 8, "Tried to parse this as block header");
                return ReadStatus.EVIO_EXCEPTION;
            }

            if (blockHeader.getHeaderLength() < 8) {
System.out.println("     readFirstHeader: block header too small, " + blockHeader.getHeaderLength());
                byteBuffer.clear();
                Utilities.printBuffer(byteBuffer, 0, 8, "Tried to parse this as block header");
                return ReadStatus.EVIO_EXCEPTION;
            }

            // Use 6th word to set bit info & version
            blockHeader.parseToBitInfo(bitInfo);
            blockHeader.setVersion(evioVersion);
            blockHeader.setReserved2(0);  // not used
            blockHeader.setMagicNumber(magicNumber);
            blockHeader.setByteOrder(byteOrder);
        }
        catch (EvioException a) {
            byteBuffer.clear();
            Utilities.printBuffer(byteBuffer, 0, 8, "Tried to parse this as block header");
            return ReadStatus.EVIO_EXCEPTION;
        }
        catch (BufferUnderflowException a) {
System.err.println("     readFirstHeader: end of Buffer: " + a.getMessage());
            byteBuffer.clear();
            return ReadStatus.UNKNOWN_ERROR;
        }

        return ReadStatus.SUCCESS;
    }


    /** {@inheritDoc} */
    public IBlockHeader getFirstBlockHeader() {return blockHeader;}


    /**
     * This method is only called if the user wants to look at the dictionary.
     * This method is called by  code because of its use of a bulk,
     * relative get method.
     *
     * @throws EvioException if failed read due to bad buffer format
     */
     protected void readDictionary() throws EvioException {

         // Where are we?
         int originalPos = byteBuffer.position();
         int pos = initialPosition + 4*firstBlockHeaderWords;

         // How big is the event?
         int length;
         length = byteBuffer.getInt(pos);
         if (length < 1) {
             throw new EvioException("Bad value for dictionary length");
         }
         pos += 4;

         // Since we're only interested in length, read but ignore rest of the header.
         byteBuffer.getInt(pos);
         pos += 4;

         // Get the raw data
         int eventDataSizeBytes = 4*(length - 1);
         byte bytes[] = new byte[eventDataSizeBytes];

         // Read in dictionary data (using a relative get)
         try {
             byteBuffer.position(pos);
             byteBuffer.get(bytes, 0, eventDataSizeBytes);
         }
         catch (Exception e) {
             throw new EvioException("Problems reading buffer");
         }

         // Unpack array into dictionary
         String[] strs = BaseStructure.unpackRawBytesToStrings(bytes, 0);
         if (strs == null) {
             throw new EvioException("Data in bad format");
         }
         dictionaryXML = strs[0];

         byteBuffer.position(originalPos);
     }


    /**
     * This method scans the given event number in the buffer.
     * It returns an EvioNode object representing the event.
     * All the event's substructures, as EvioNode objects, are
     * contained in the node.allNodes list (including the event itself).
     *
     * @param eventNumber number of the event to be scanned starting at 1
     * @return the EvioNode object corresponding to the given event number
     */
    protected EvioNode scanStructure(int eventNumber) {

        // Node corresponding to event
        EvioNode node = eventNodes.get(eventNumber - 1);

        if (node.scanned) {
            node.clearLists();
        }

        // Do this before actual scan so clone() sets all "scanned" fields
        // of child nodes to "true" as well.
        node.scanned = true;

        EvioNode.scanStructure(node);

        return node;
    }


    /**
     * This method scans the given event number in the buffer.
     * It returns an EvioNode object representing the event.
     * All the event's substructures, as EvioNode objects, are
     * contained in the node.allNodes list (including the event itself).
     *
     * @param eventNumber number of the event to be scanned starting at 1
     * @param nodeSource  source of EvioNode objects to use while parsing evio data.
     * @return the EvioNode object corresponding to the given event number
     */
    protected EvioNode scanStructure(int eventNumber, EvioNodeSource nodeSource) {

        // Node corresponding to event
        EvioNode node = eventNodes.get(eventNumber - 1);

        if (node.scanned) {
            node.clearLists();
        }

        // Do this before actual scan so clone() sets all "scanned" fields
        // of child nodes to "true" as well.
        node.scanned = true;

        EvioNode.scanStructure(node, nodeSource);

        return node;
    }


    /**
     * This method searches the specified event in a file/buffer and
     * returns a list of objects each of which contain information
     * about a single evio structure which matches the given tag and num.
     *
     * @param eventNumber place of event in buffer (starting with 1)
     * @param tag tag to match
     * @param num num to match
     * @return list of EvioNode objects corresponding to matching evio structures
     *         (empty if none found)
     * @throws EvioException if bad arg value(s);
     *                       if object closed
     */
    public  List<EvioNode> searchEvent(int eventNumber, int tag, int num)
                                 throws EvioException {

        // check args
        if (tag < 0 || num < 0 || eventNumber < 1 || eventNumber > eventCount) {
            throw new EvioException("bad arg value(s)");
        }

        if (closed) {
            throw new EvioException("object closed");
        }

        ArrayList<EvioNode> returnList = new ArrayList<EvioNode>(100);

        // Scan the node
        ArrayList<EvioNode> list = scanStructure(eventNumber).allNodes;
//System.out.println("searchEvent: ev# = " + eventNumber + ", list size = " + list.size() +
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
     * This method searches the specified event in a file/buffer and
     * returns a list of objects each of which contain information
     * about a single evio structure which matches the given dictionary
     * entry name.
     *
     * @param  eventNumber place of event in buffer (starting with 1)
     * @param  dictName name of dictionary entry to search for
     * @param  dictionary dictionary to use; if null, use dictionary with file/buffer
     *
     * @return list of EvioNode objects corresponding to matching evio structures
     *         (empty if none found)
     * @throws EvioException if dictName is null;
     *                       if dictName is an invalid dictionary entry;
     *                       if dictionary is null and none provided in file/buffer being read;
     *                       if object closed
     */
    public  List<EvioNode> searchEvent(int eventNumber, String dictName,
                                      EvioXMLDictionary dictionary)
                                 throws EvioException {

        if (dictName == null) {
            throw new EvioException("null dictionary entry name");
        }

        if (closed) {
            throw new EvioException("object closed");
        }

        // If no dictionary is specified, use the one provided with the
        // file/buffer. If that does not exist, throw an exception.
        Integer tag, num;

        if (dictionary == null && hasDictionary)  {
            dictionary = getDictionary();
        }

        if (dictionary != null) {
            tag = dictionary.getTag(dictName);
            num = dictionary.getNum(dictName);
            if (tag == null || num == null) {
                throw new EvioException("no dictionary entry for " + dictName);
            }
        }
        else {
            throw new EvioException("no dictionary available");
        }

        return searchEvent(eventNumber, tag, num);
    }


    /**
     * This method removes the data of the given event from the buffer.
     * It also marks any existing EvioNodes representing the event and its
     * descendants as obsolete. They must not be used anymore.<p>
     *
     * @param eventNumber number of event to remove from buffer
     * @return new ByteBuffer created and updated to reflect the event removal
     * @throws EvioException if eventNumber &lt; 1;
     *                       if event number does not correspond to existing event;
     *                       if object closed;
     *                       if node was not found in any event;
     *                       if internal programming error;
     *                       if not using block header objects
     */
    public  ByteBuffer removeEvent(int eventNumber) throws EvioException {

        if (eventNumber < 1) {
            throw new EvioException("event number must be > 0");
        }

        if (closed) {
            throw new EvioException("object closed");
        }

        EvioNode eventNode;
        try {
            eventNode = eventNodes.get(eventNumber - 1);
        }
        catch (IndexOutOfBoundsException e) {
            throw new EvioException("event " + eventNumber + " does not exist", e);
        }

        return removeStructure(eventNode);
    }


    /**
     * This method removes the data, represented by the given node, from the buffer.
     * It also marks the node and its descendants as obsolete. They must not be used
     * anymore.<p>
     *
     * @param removeNode  evio structure to remove from buffer
     * @return ByteBuffer updated to reflect the node removal
     * @throws EvioException if object closed;
     *                       if node was not found in any event;
     *                       if internal programming error;
     *                       if not using block header objects
     */
    public  ByteBuffer removeStructure(EvioNode removeNode) throws EvioException {

        if (!usingBlockHdrs) {
            throw new EvioException("need to be using block headers to call this method");
        }

        // If we're removing nothing, then DO nothing
        if (removeNode == null) {
            return byteBuffer;
        }

        if (closed) {
            throw new EvioException("object closed");
        }
        else if (removeNode.isObsolete()) {
            //System.out.println("removeStructure: node has already been removed");
            return byteBuffer;
        }

        EvioNode eventNode = null;
        boolean isEvent = false, foundNode = false;
        // Place of the removed node in allNodes list.
        // 0 means event itself (top level).
        int removeNodePlace = 0;

        // Locate the node to be removed ...
        outer:
        for (EvioNode ev : eventNodes) {
            removeNodePlace = 0;

            // See if it's an event ...
            if (removeNode == ev) {
                eventNode = ev;
                isEvent = true;
                foundNode = true;
                break;
            }

            for (EvioNode n : ev.allNodes) {
                // The first node in allNodes is the event node,
                // so do not increment removeNodePlace now.

                if (removeNode == n) {
                    eventNode = ev;
                    foundNode = true;
                    break outer;
                }

                // Keep track of where inside the event it is
                removeNodePlace++;
            }
        }

        if (!foundNode) {
            throw new EvioException("removeNode not found in any event");
        }

        // The data these nodes represent will be removed from the buffer,
        // so the node will be obsolete along with all its descendants.
        removeNode.setObsolete(true);

        // If we started out by reading a file, now we switch to using a buffer
        if (isFile) {
            isFile = false;
            mappedByteBuffer = null;

            // Create a new buffer by duplicating existing one
            ByteBuffer newBuffer = ByteBuffer.allocate(byteBuffer.capacity());
            newBuffer.order(byteOrder).position(byteBuffer.position()).limit(byteBuffer.limit());

            // Copy data into new buffer
            newBuffer.put(byteBuffer);
            newBuffer.position(initialPosition);

            // Use new buffer from now on
            byteBuffer = newBuffer;

            // All nodes need to use this new buffer
            for (EvioNode ev : eventNodes) {
                for (EvioNode n : ev.allNodes) {
                    n.setBuffer(byteBuffer);
                }
            }
        }

        //---------------------------------------------------
        // Remove structure. Keep using current buffer.
        // We'll move all data that came after removed node
        // to where removed node used to be.
        //---------------------------------------------------

        // Amount of data being removed
        int removeDataLen = removeNode.getTotalBytes();
        int removeWordLen = removeDataLen / 4;

        // Just after removed node (start pos of data being moved)
        int startPos = removeNode.pos + removeDataLen;
        // Length of data to move in bytes
        int moveLen = initialPosition + 4*validDataWords - startPos;


        // Can't use duplicate(), must copy the backing buffer
        ByteBuffer moveBuffer = ByteBuffer.allocate(moveLen).order(byteBuffer.order());
        int bufferLim = byteBuffer.limit();
        byteBuffer.limit(startPos + moveLen).position(startPos);
        moveBuffer.put(byteBuffer);
        byteBuffer.limit(bufferLim);

        // Prepare to move data currently sitting past the removed node
        moveBuffer.clear();

        // Set place to put the data being moved - where removed node starts
        byteBuffer.position(removeNode.pos);
        // Copy it over
        byteBuffer.put(moveBuffer);

        // Reset some buffer values
        validDataWords -= removeWordLen;
        byteBuffer.position(initialPosition);
        byteBuffer.limit(4*validDataWords + initialPosition);

        //-------------------------------------
        // By removing a structure, we need to shift the POSITIONS of all
        // structures that follow by the size of the deleted chunk.
        //-------------------------------------
        ArrayList<EvioNode> nodeList;
        int place = eventNode.place;

        for (int i=0; i < eventCount; i++) {
            int level = 0;
            nodeList = eventNodes.get(i).allNodes;

            for (EvioNode n : nodeList) {
                // For events that come after, move all contained nodes
                if (i > place) {
                    n.pos -= removeDataLen;
                    n.dataPos -= removeDataLen;
                }
                // For the event in which the removed node existed ...
                else if (i == place && !isEvent) {
                    // There may be structures that came after the removed node,
                    // but within the same event. They need to be moved too.
                    if (level > removeNodePlace) {
                        n.pos -= removeDataLen;
                        n.dataPos -= removeDataLen;
                    }
                }
                level++;
            }
        }

        place = eventNode.blockNode.place;
        for (int i=0; i < blockCount; i++) {
            if (i > place) {
                blockNodes.get(i).pos -= removeDataLen;
            }
        }

        //--------------------------------------------
        // We need to update the lengths of all the
        // removed node's parent structures as well as
        // the length of the block containing it.
        //--------------------------------------------

//System.out.println("block object len = " +  eventNode.blockNode.len +
//                   ", set to " + (eventNode.blockNode.len - removeWordLen));
        // If removing entire event ...
        if (isEvent) {
            // Decrease total event count
            eventCount--;
            // Decrease block count
            eventNode.blockNode.count--;
            // Skip over 3 ints to update the block header's event count
            byteBuffer.putInt(eventNode.blockNode.pos + 12, eventNode.blockNode.count);
        }
        eventNode.blockNode.len -= removeWordLen;
        byteBuffer.putInt(eventNode.blockNode.pos, eventNode.blockNode.len);

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

        if (isEvent) {
            eventNodes.remove(removeNode);
        }

        return byteBuffer;
    }

// TODO: This method will change a memory mapped buffer into one this is NOT!!!
// Map is opened as READ_ONLY

    /**
     * This method adds an evio container (bank, segment, or tag segment) as the last
     * structure contained in an event. It is the responsibility of the caller to make
     * sure that the buffer argument contains valid evio data (only data representing
     * the structure to be added - not in file format with block header and the like)
     * which is compatible with the type of data stored in the given event.<p>
     *
     * To produce such evio data use {@link EvioBank#write(ByteBuffer)},
     * {@link EvioSegment#write(ByteBuffer)} or
     * {@link EvioTagSegment#write(ByteBuffer)} depending on whether
     * a bank, seg, or tagseg is being added.<p>
     *
     * The given buffer argument must be ready to read with its position and limit
     * defining the limits of the data to copy.
     *
     * @param eventNumber number of event to which addBuffer is to be added
     * @param addBuffer buffer containing evio data to add (<b>not</b> evio file format,
     *                  i.e. no block headers)
     * @return a new ByteBuffer object which is created and filled with all the data
     *         including what was just added.
     * @throws EvioException if eventNumber &lt; 1;
     *                       if addBuffer is null;
     *                       if addBuffer arg is empty or has non-evio format;
     *                       if addBuffer is opposite endian to current event buffer;
     *                       if added data is not the proper length (i.e. multiple of 4 bytes);
     *                       if the event number does not correspond to an existing event;
     *                       if there is an internal programming error;
     *                       if object closed;
     *                       if not using block header objects.
     */
    public  ByteBuffer addStructure(int eventNumber, ByteBuffer addBuffer) throws EvioException {

        if (addBuffer == null || addBuffer.remaining() < 8) {
            throw new EvioException("null, empty, or non-evio format buffer arg");
        }

        if (addBuffer.order() != byteOrder) {
            throw new EvioException("trying to add wrong endian buffer");
        }

        if (eventNumber < 1) {
            throw new EvioException("event number must be > 0");
        }

        if (closed) {
            throw new EvioException("object closed");
        }

        if (!usingBlockHdrs) {
            throw new EvioException("need to be using block headers to call this method");
        }

        EvioNode eventNode;
        try {
            eventNode = eventNodes.get(eventNumber - 1);
        }
        catch (IndexOutOfBoundsException e) {
            throw new EvioException("event " + eventNumber + " does not exist", e);
        }

        // Position in byteBuffer just past end of event
        int endPos = eventNode.dataPos + 4*eventNode.dataLen;

        // Original position of buffer being added
        int origAddBufPos = addBuffer.position();

        // How many bytes are we adding?
        int appendDataLen = addBuffer.remaining();

        // Make sure it's a multiple of 4
        if (appendDataLen % 4 != 0) {
            throw new EvioException("data added is not in evio format");
        }

        // Since we're changing node's data, get rid of stored data in int[] format
        eventNode.clearIntArray();

        // Data length in 32-bit words
        int appendDataWordLen = appendDataLen / 4;

        // Event contains structures of this type
        DataType eventDataType = eventNode.getDataTypeObj();

        //--------------------------------------------
        // Add new structure to end of specified event
        //--------------------------------------------

        // Create a new buffer
        ByteBuffer newBuffer = ByteBuffer.allocate(4*validDataWords + appendDataLen);
        newBuffer.order(byteOrder);

        // Copy beginning part of existing buffer into new buffer
        byteBuffer.limit(endPos).position(initialPosition);
        newBuffer.put(byteBuffer);

        // Copy new structure into new buffer
        int newBankBufPos = newBuffer.position();
        newBuffer.put(addBuffer);

        // Copy ending part of existing buffer into new buffer
        byteBuffer.limit(4*validDataWords + initialPosition).position(endPos);
        newBuffer.put(byteBuffer);

        // Get new buffer ready for reading
        newBuffer.flip();

        // Restore original positions of buffers
        byteBuffer.position(initialPosition);
        addBuffer.position(origAddBufPos);

        //-------------------------------------
        // By inserting a structure, we've definitely changed the positions of all
        // structures that follow. Everything downstream gets shifted by appendDataLen
        // bytes.
        // And, if initialPosition was not 0 (in the new buffer it always is),
        // then ALL nodes need their position members shifted by initialPosition
        // bytes upstream.
        //-------------------------------------
        int place = eventNode.place;

        for (int i=0; i < eventCount; i++) {
            for (EvioNode n : eventNodes.get(i).allNodes) {
                // Make sure nodes are using the new buffer
                n.setBuffer(newBuffer);

                //System.out.println("Event node " + (i+1) + ", pos = " + n.pos + ", dataPos = " + n.dataPos);
                if (i > place) {
                    n.pos     += appendDataLen - initialPosition;
                    n.dataPos += appendDataLen - initialPosition;
                    //System.out.println("      pos -> " + n.pos + ", dataPos -> " + n.dataPos);
                }
                else {
                    n.pos     -= initialPosition;
                    n.dataPos -= initialPosition;
                }
            }
        }

        place = eventNode.blockNode.place;
        for (int i=0; i < blockCount; i++) {
            if (i > place) {
                blockNodes.get(i).pos += appendDataLen - initialPosition;
            }
            else {
                blockNodes.get(i).pos -= initialPosition;
            }
        }

        // At this point all EvioNode objects (including those in
        // user's possession) should be updated.

        // This reader object is NOW using the new buffer
        byteBuffer      = newBuffer;
        initialPosition = newBuffer.position();
        validDataWords += appendDataWordLen;

        // If we started out by reading a file, now we are using the new buffer.
        if (isFile) {
            isFile = false;
            mappedByteBuffer = null;
        }

        //--------------------------------------------
        // Adjust event and block header sizes in both
        // block/event node objects and in new buffer.
        //--------------------------------------------

        // Position in new byteBuffer of event
        int eventLenPos = eventNode.pos;

        // Increase block size
        //System.out.println("block object len = " +  eventNode.blockNode.len +
        //                   ", set to " + (eventNode.blockNode.len + appendDataWordLen));
        eventNode.blockNode.len += appendDataWordLen;
        newBuffer.putInt(eventNode.blockNode.pos, eventNode.blockNode.len);

        // Increase event size
        eventNode.len     += appendDataWordLen;
        eventNode.dataLen += appendDataWordLen;

        switch (eventDataType) {
            case BANK:
            case ALSOBANK:
                //System.out.println("event pos = " + eventLenPos + ", len = " + (eventNode.len - appendDataWordLen) +
                //                   ", set to " + (eventNode.len));

                newBuffer.putInt(eventLenPos, eventNode.len);
                break;

            case SEGMENT:
            case ALSOSEGMENT:
            case TAGSEGMENT:
                //System.out.println("event SEG/TAGSEG pos = " + eventLenPos + ", len = " + (eventNode.len - appendDataWordLen) +
                //                   ", set to " + (eventNode.len));
                if (byteOrder == ByteOrder.BIG_ENDIAN) {
                    newBuffer.putShort(eventLenPos+2, (short)(eventNode.len));
                }
                else {
                    newBuffer.putShort(eventLenPos,   (short)(eventNode.len));
                }
                break;

            default:
                throw new EvioException("internal programming error");
        }

        // Since the event's values (positions and lengths) have now been set properly,
        // we can now rescan the event to update all the sub-structure info, thereby
        // including the newly add structure. Problem is, that invalidates all existing
        // node objects for this event and users may try to continue using those - BAD.
        //
        // Instead, create a single new node by cloning the event object and resetting
        // all its internal values by parsing (or extracting from) the buffer.
        if (eventNode.scanned) {

            // Cloning is a fast copy & an empty childNodes list
            EvioNode newNode = (EvioNode) eventNode.clone();
            newNode.isEvent = false;
            newNode.eventNode = newNode.parentNode = eventNode;

            // Extract data from buffer (not children data)
            EvioNode.extractNode(newNode, newBankBufPos);

            // Now that we have this new node, we must place it in the correct order
            // in both the child & allNodes lists. This is easy since we are inserting
            // the bank as the last bank of this event.
            eventNode.addChild(newNode);

            // This node may contain other nodes. Find those by scanning this one.
            // This will add all nodes in this tree to all lists.
            EvioNode.scanStructure(newNode);
        }

        return newBuffer;
    }


    /**
     * Get the data associated with an evio structure in ByteBuffer form.
     * The returned buffer is a view into this reader's buffer (no copy done).
     * Changes in one will affect the other.
     *
     * @param node evio structure whose data is to be retrieved
     * @throws EvioException if object closed or node arg is null.
     * @return ByteBuffer object containing data. Position and limit are
     *         set for reading.
     */
    public ByteBuffer getData(EvioNode node) throws EvioException {
        return getData(node, false);
    }


    /**
     * <p>Get the data associated with an evio structure in ByteBuffer form.
     * Depending on the copy argument, the returned buffer will either be
     * a copy of or a view into the data of this reader's buffer.</p>
     *
     * @param node evio structure whose data is to be retrieved
     * @param copy if <code>true</code>, then return a copy as opposed to a
     *             view into this reader object's buffer.
     * @throws EvioException if object closed or node arg is null.
     * @return ByteBuffer object containing data. Position and limit are
     *         set for reading.
     */
    public  ByteBuffer getData(EvioNode node, boolean copy)
                            throws EvioException {
        if (closed) {
            throw new EvioException("object closed");
        }
        else if (node == null) {
            throw new EvioException("node arg is null");
        }
        return node.getByteData(copy);
    }


    /**
     * <p>Get an evio bank or event in ByteBuffer form.
     * The returned buffer is a view into the data of this reader's buffer.</p>
     *
     * @param eventNumber number of event of interest
     * @return ByteBuffer object containing bank's/event's bytes. Position and limit are
     *         set for reading.
     * @throws EvioException if eventNumber &lt; 1;
     *                       if the event number does not correspond to an existing event;
     *                       if object closed
     */
    public ByteBuffer getEventBuffer(int eventNumber) throws EvioException {
        return getEventBuffer(eventNumber, false);
    }


    /**
     * <p>Get an evio bank or event in ByteBuffer form.
     * Depending on the copy argument, the returned buffer will either be
     * a copy of or a view into the data of this reader's buffer.</p>
     *
     * @param eventNumber number of event of interest
     * @param copy if <code>true</code>, then return a copy as opposed to a
     *        view into this reader object's buffer.
     * @return ByteBuffer object containing bank's/event's bytes. Position and limit are
     *         set for reading.
     * @throws EvioException if eventNumber &lt; 1;
     *                       if the event number does not correspond to an existing event;
     *                       if object closed
     */
    public  ByteBuffer getEventBuffer(int eventNumber, boolean copy)
            throws EvioException {

        if (eventNumber < 1) {
            throw new EvioException("event number must be > 0");
        }

        if (closed) {
            throw new EvioException("object closed");
        }

        EvioNode node;
        try {
            node = eventNodes.get(eventNumber - 1);
        }
        catch (IndexOutOfBoundsException e) {
            throw new EvioException("event " + eventNumber + " does not exist");
        }

        return node.getStructureBuffer(copy);
    }


    /**
     * <p>Get an evio structure (bank, seg, or tagseg) in ByteBuffer form.
     * The returned buffer is a view into the data of this reader's buffer.</p>
     *
     * @param node node object representing evio structure of interest
     * @return ByteBuffer object containing bank's/event's bytes. Position and limit are
     *         set for reading.
     * @throws EvioException if node is null;
     *                       if object closed
     */
    public ByteBuffer getStructureBuffer(EvioNode node) throws EvioException {
        return getStructureBuffer(node, false);
    }


    /**
     * <p>Get an evio structure (bank, seg, or tagseg) in ByteBuffer form.
     * Depending on the copy argument, the returned buffer will either be
     * a copy of or a view into the data of this reader's buffer.</p>
     *
     * @param node node object representing evio structure of interest
     * @param copy if <code>true</code>, then return a copy as opposed to a
     *        view into this reader object's buffer.
     * @return ByteBuffer object containing structure's bytes. Position and limit are
     *         set for reading.
     * @throws EvioException if node is null;
     *                       if object closed
     */
    public  ByteBuffer getStructureBuffer(EvioNode node, boolean copy)
            throws EvioException {

        if (node == null) {
            throw new EvioException("node arg is null");
        }

        if (closed) {
            throw new EvioException("object closed");
        }

        return node.getStructureBuffer(copy);
    }



	/**
	 * This only sets the position to its initial value.
	 */
    public  void close() {
        byteBuffer.position(initialPosition);
        closed = true;
	}


    /**
     * This is the number of events in the file/buffer. Any dictionary event is <b>not</b>
     * included in the count. In versions 3 and earlier, it is not computed unless
     * asked for, and if asked for it is computed and cached.
     *
     * @return the number of events in the file.
     */
    public int getEventCount() {
        return eventCount;
    }

    /**
     * This is the number of blocks in the file including the empty
     * block usually at the end of version 4 files/buffers.
     * For version 3 files, a block size read from the first block is used
     * to calculate the result.
     * It is not computed unless in random access mode or is
     * asked for, and if asked for it is computed and cached.
     *
     * @return the number of blocks in the file (estimate for version 3 files)
     */
    public int getBlockCount() {
        return blockCount;
    }

    /**
     * Save the internal byte buffer to the given file
     * (overwrites existing file).
     *
     * @param fileName  name of file to write
     * @throws IOException if error writing to file
     * @throws EvioException if fileName arg is null;
     *                       if object closed
     */
    public void toFile(String fileName) throws EvioException, IOException {
        if (fileName == null) {
            throw new EvioException("null fileName arg");
        }
        File f = new File(fileName);
        toFile(f);
    }

    /**
     * Save the internal byte buffer to the given file
     * (overwrites existing file).
     *
     * @param file  object of file to write
     * @throws EvioException if file arg is null;
     *                       if object closed
     * @throws IOException if error writing to file
     */
    public  void toFile(File file) throws EvioException, IOException {
        if (file == null) {
            throw new EvioException("null file arg");
        }

        if (closed) {
            throw new EvioException("object closed");
        }

        // Remember where we were
        int pos = byteBuffer.position();

        // Write the file
        FileOutputStream fos = new FileOutputStream(file);
        FileChannel channel = fos.getChannel();
        channel.write(byteBuffer);
        channel.close();

        // Go back to where we were
        byteBuffer.position(pos);
    }

}