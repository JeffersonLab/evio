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

import javax.xml.stream.FactoryConfigurationError;
import javax.xml.stream.XMLOutputFactory;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamWriter;
import java.io.*;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.nio.channels.FileChannel;

/**
 * This is a class of interest to the user. It is used to read an evio version 4 or earlier
 * format file or buffer. Create an <code>EvioReader</code> object corresponding to an event
 * file or file-formatted buffer, and from this class you can test it
 * for consistency and, more importantly, you can call {@link #parseNextEvent} or
 * {@link #parseEvent(int)} to get new events and to stream the embedded structures
 * to an IEvioListener.<p>
 *
 * A word to the wise, constructors for reading a file in random access mode
 * (by setting "sequential" arg to false), will memory map the file. This is
 * <b>not</b> a good idea if the file is not on a local disk. Due to java
 * restrictions, files over 2.1GB will require multiple memory maps.<p>
 *
 * The streaming effect of parsing an event is that the parser will read the event and hand off structures,
 * such as banks, to any IEvioListeners. For those familiar with XML, the event is processed SAX-like.
 * It is up to the listener to decide what to do with the structures.
 * <p>
 *
 * As an alternative to stream processing, after an event is parsed, the user can use the events treeModel
 * for access to the structures. For those familiar with XML, the event is processed DOM-like.
 * <p>
 *
 * This class is identical to EvioReaderV4 except that no methods or code are synchronized,
 * therefore it's not thread safe.
 * <p>
 *
 * Although this class can be used directly, it's generally used by
 * using {@link EvioReader} which, in turn, uses this class.<p>
 *
 * @author heddle
 * @author timmer
 *
 */
public class EvioReaderUnsyncV4 implements IEvioReader {

    /**  Offset to get magic number from start of file. */
    protected static final int MAGIC_OFFSET = 28;

    /** Offset to get version number from start of file. */
    protected static final int VERSION_OFFSET = 20;

    /** Offset to get block size from start of block. */
    protected static final int BLOCK_SIZE_OFFSET = 0;

    /** Mask to get version number from 6th int in block. */
    protected static final int VERSION_MASK = 0xff;

    /** Root element tag for XML file */
    protected static final String ROOT_ELEMENT = "evio-data";

    /** Default size for a single file read in bytes when reading
     *  evio format 1-3. Equivalent to 500, 32,768 byte blocks.
     *  This constant <b>MUST BE</b> an integer multiple of 32768.*/
    protected static final int DEFAULT_READ_BYTES = 32768 * 500; // 16384000 bytes



    /** When doing a sequential read, used to assign a transient
     * number [1..n] to events as they are being read. */
    protected int eventNumber = 0;

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

    /** Size of the first block in bytes. */
    protected int firstBlockSize;

    /**
     * This is the number of blocks in the file including the empty block at the
     * end of the version 4 files. It is not computed unless asked for,
     * and if asked for it is computed and cached in this variable.
     */
    protected int blockCount = -1;

	/** The current block header for evio versions 1-3. */
    protected BlockHeaderV2 blockHeader2 = new BlockHeaderV2();

    /** The current block header for evio version 4. */
    protected BlockHeaderV4 blockHeader4 = new BlockHeaderV4();

    /** Reference to current block header, any version, through interface.
     *  This must be the same object as either blockHeader2 or blockHeader4
     *  depending on which evio format version the data is in. */
    protected IBlockHeader blockHeader;

    /** Reference to first block header. */
    protected IBlockHeader firstBlockHeader;

    /** Block number expected when reading. Used to check sequence of blocks. */
    protected int blockNumberExpected = 1;

    /** If true, attempt header recovery when block headers aren’t where expected
     *    currently, having extraneous words is supported
     *    it doesn't handle missing header words yet (more difficult)
     *    also ignores lastBlock flag
      */
    protected boolean doHeaderRecoveryCheck = false;

    /** If true, throw an exception if block numbers are out of sequence. */
    protected boolean checkBlockNumberSequence;

    /** Is this the last block in the file or buffer? */
    protected boolean lastBlock;

    /**
     * Version 4 files may have an xml format dictionary in the
     * first event of the first block.
     */
    protected String dictionaryXML;

    /** The buffer being read. */
    protected ByteBuffer byteBuffer;

    /** Parser object for this file/buffer. */
    protected EventParser parser;

    /** Initial position of buffer or mappedByteBuffer when reading a file. */
    protected int initialPosition;

    //------------------------
    // File specific members
    //------------------------

    /** Use this object to handle files > 2.1 GBytes but still use memory mapping. */
    protected MappedMemoryHandler mappedMemoryHandler;

    /** Absolute path of the underlying file. */
    protected String path;

    /** File object. */
    protected File file;

    /** File size in bytes. */
    protected long fileSize;

    /** File channel used to read data and access file position. */
    protected FileChannel fileChannel;

    /** Data stream used to read data. */
    protected DataInputStream dataStream;

    /** Do we need to swap data from file? */
    protected boolean swap;

    /**
     * Read this file sequentially and not using a memory mapped buffer.
     * If the file being read > 2.1 GBytes, then this is always true.
     */
    protected boolean sequentialRead;


    //------------------------
    // EvioReader's state
    //------------------------

    /** Is this object currently closed? */
    protected boolean closed;

    /**
     * This class stores the state of this reader so it can be recovered
     * after a state-changing method has been called -- like {@link #rewind()}.
     */
    protected static final class ReaderState {
        private boolean lastBlock;
        private int eventNumber;
        private long filePosition;
        private int byteBufferLimit;
        private int byteBufferPosition;
        private int blockNumberExpected;
        private BlockHeaderV2 blockHeader2;
        private BlockHeaderV4 blockHeader4;
    }


    /**
     * This method saves the current state of this EvioReader object.
     * @return the current state of this EvioReader object.
     */
    protected ReaderState getState() {
        ReaderState currentState = new ReaderState();
        currentState.lastBlock   = lastBlock;
        currentState.eventNumber = eventNumber;
        currentState.blockNumberExpected = blockNumberExpected;

        if (sequentialRead) {
            try {
                currentState.filePosition = fileChannel.position();
                currentState.byteBufferLimit = byteBuffer.limit();
                currentState.byteBufferPosition = byteBuffer.position();
            }
            catch (IOException e) {/* should be OK */}
        }
        else {
            if (byteBuffer != null) {
                currentState.byteBufferLimit = byteBuffer.limit();
                currentState.byteBufferPosition = byteBuffer.position();
            }
        }

        if (evioVersion > 3) {
            currentState.blockHeader4 = (BlockHeaderV4)blockHeader4.clone();
        }
        else {
            currentState.blockHeader2 = (BlockHeaderV2)blockHeader2.clone();
        }

        return currentState;
    }


    /**
     * This method restores a previously saved state of this EvioReader object.
     * @param state a previously stored state of this EvioReader object.
     */
    protected void restoreState(ReaderState state) {
        lastBlock   = state.lastBlock;
        eventNumber = state.eventNumber;
        blockNumberExpected = state.blockNumberExpected;

        if (sequentialRead) {
            try {
                fileChannel.position(state.filePosition);
                byteBuffer.limit(state.byteBufferLimit);
                byteBuffer.position(state.byteBufferPosition);
            }
            catch (IOException e) {/* should be OK */}
        }
        else {
            if (byteBuffer != null) {
                byteBuffer.limit(state.byteBufferLimit);
                byteBuffer.position(state.byteBufferPosition);
            }
        }

        if (evioVersion > 3) {
            blockHeader = blockHeader4 = state.blockHeader4;
        }
        else {
            blockHeader = blockHeader2 = state.blockHeader2;
        }
    }


    /**
     * This method prints out a portion of a given ByteBuffer object
     * in hex representation of ints.
     *
     * @param buf buffer to be printed out
     * @param lenInInts length of data in ints to be printed
     */
    protected void printBuffer(ByteBuffer buf, int lenInInts) {
        IntBuffer ibuf = buf.asIntBuffer();
        lenInInts = lenInInts > ibuf.capacity() ? ibuf.capacity() : lenInInts;
        for (int i=0; i < lenInInts; i++) {
            System.out.println("  Buf(" + i + ") = 0x" + Integer.toHexString(ibuf.get(i)));
        }
    }

    //------------------------

    /**
     * Constructor for reading an event file.
     *
     * @param path the full path to the file that contains events.
     *             For writing event files, use an <code>EventWriter</code> object.
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null
     */
    public EvioReaderUnsyncV4(String path) throws EvioException, IOException {
        this(new File(path));
    }

    /**
     * Constructor for reading an event file.
     *
     * @param path the full path to the file that contains events.
     *             For writing event files, use an <code>EventWriter</code> object.
     * @param checkBlkNumSeq if <code>true</code> check the block number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null;
     *                       if first block number != 1 when checkBlkNumSeq arg is true
     */
    public EvioReaderUnsyncV4(String path, boolean checkBlkNumSeq) throws EvioException, IOException {
        this(new File(path), checkBlkNumSeq);
    }

    /**
     * Constructor for reading an event file.
     * Sequential reading and not memory-mapped buffer.
     *
     * @param file the file that contains events.
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null
     */
    public EvioReaderUnsyncV4(File file) throws EvioException, IOException {
        this(file, false);
    }


    /**
     * Constructor for reading an event file.
     * Sequential reading and not memory-mapped buffer.
     *
     * @param file the file that contains events.
     * @param checkBlkNumSeq if <code>true</code> check the block number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null;
     *                       if first block number != 1 when checkBlkNumSeq arg is true
     */
    public EvioReaderUnsyncV4(File file, boolean checkBlkNumSeq)
                                        throws EvioException, IOException {
        this(file, checkBlkNumSeq, true);
    }


    /**
     * Constructor for reading an event file.
     * Do <b>not</b> set sequential to false for remote files.
     *
     * @param path the full path to the file that contains events.
     *             For writing event files, use an <code>EventWriter</code> object.
     * @param checkBlkNumSeq if <code>true</code> check the block number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1
     * @param sequential     if <code>true</code> read the file sequentially,
     *                       else use memory mapped buffers. If file &gt; 2.1 GB,
     *                       reads are always sequential for the older evio format.
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null;
     *                       if first block number != 1 when checkBlkNumSeq arg is true
     */
    public EvioReaderUnsyncV4(String path, boolean checkBlkNumSeq, boolean sequential)
            throws EvioException, IOException {
        this(new File(path), checkBlkNumSeq, sequential);
    }


    /**
     * Constructor for reading an event file.
     * Do <b>not</b> set sequential to false for remote files.
     *
     * @param file the file that contains events.
     * @param checkBlkNumSeq if <code>true</code> check the block number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1
     * @param sequential     if <code>true</code> read the file sequentially,
     *                       else use memory mapped buffers. If file &gt; 2.1 GB,
     *                       reads are always sequential for the older evio format.
     *
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null;
     *                       if file is too small to have valid evio format data
     *                       if first block number != 1 when checkBlkNumSeq arg is true
     */
    public EvioReaderUnsyncV4(File file, boolean checkBlkNumSeq, boolean sequential)
                                        throws EvioException, IOException {
        if (file == null) {
            throw new EvioException("File arg is null");
        }
        this.file = file;

        checkBlockNumberSequence = checkBlkNumSeq;
        sequentialRead = sequential;
        initialPosition = 0;

        FileInputStream fileInputStream = new FileInputStream(file);
        path = file.getAbsolutePath();
        fileChannel = fileInputStream.getChannel();
        fileSize = fileChannel.size();

        if (fileSize < 40) {
            throw new EvioException("File too small to have valid evio data");
        }

        // Look at the first block header to get various info like endianness and version.
        // Store it for later reference in blockHeader2,4 and in other variables.
        ByteBuffer headerBuf = ByteBuffer.allocate(32);
        int bytesRead = 0;
        while (bytesRead < 32) {
            bytesRead += fileChannel.read(headerBuf);
        }
        parseFirstHeader(headerBuf);
        fileChannel.position(0);
        parser = new EventParser();

        // What we do from here depends on the evio format version.
        // If we've got the old version, don't memory map big (> 2.1 GB) files,
        // use sequential reading instead.
        if (evioVersion < 4) {
            // Remember, no dictionaries exist for these early versions

            // Got a big file? If so we cannot use a memory mapped file.
            if (fileSize > Integer.MAX_VALUE) {
                sequentialRead = true;
            }

            if (sequentialRead) {
                // Taken from FileInputStream javadoc:
                //
                // "Reading bytes from this [fileInput] stream will increment the channel's
                //  position.  Changing the channel's position, either explicitly or by
                //  reading, will change this stream's file position".
                //
                // So reading from either the dataStream or fileChannel object will
                // change both positions.

                dataStream = new DataInputStream(fileInputStream);
//System.out.println("Big file or reading sequentially for evio versions 2,3");
                prepareForSequentialRead();
            }
            else {
                byteBuffer = fileChannel.map(FileChannel.MapMode.READ_ONLY, 0L, fileSize);
                byteBuffer.order(byteOrder);
                prepareForBufferRead(byteBuffer);
//System.out.println("Memory Map versions 2,3");
            }
        }
        // For the new version ...
        else {
            if (sequentialRead) {
                dataStream = new DataInputStream(fileInputStream);
                prepareForSequentialRead();
                if (blockHeader4.hasDictionary()) {
                    // Dictionary is always the first event
                    EvioEvent dict = parseNextEvent();
                    if (dict != null) {
                        String[] strs = dict.getStringData();
                        dictionaryXML = strs[0];
                    }
                }
            }
            else {
                // Memory map the file - even the big ones
                mappedMemoryHandler = new MappedMemoryHandler(fileChannel, byteOrder);
                if (blockHeader4.hasDictionary()) {
                    ByteBuffer buf = mappedMemoryHandler.getFirstMap();
                    // Jump to the first event
                    prepareForBufferRead(buf);
                    // Dictionary is always the first event
                    readDictionary(buf);
                }
            }
        }

    }


    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     * @see EventWriter
     * @throws EvioException if buffer arg is null
     */
    public EvioReaderUnsyncV4(ByteBuffer byteBuffer) throws EvioException {
        this(byteBuffer, false);
    }

    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     * @param checkBlkNumSeq if <code>true</code> check the block number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1
     * @see EventWriter
     * @throws EvioException if buffer arg is null;
     *                       if first block number != 1 when checkBlkNumSeq arg is true
     */
    public EvioReaderUnsyncV4(ByteBuffer byteBuffer, boolean checkBlkNumSeq) throws EvioException {

        if (byteBuffer == null) {
            throw new EvioException("Buffer arg is null");
        }

        checkBlockNumberSequence = checkBlkNumSeq;
        this.byteBuffer = byteBuffer.slice(); // remove necessity to track initial position

        // Look at the first block header to get various info like endianness and version.
        // Store it for later reference in blockHeader2,4 and in variables.
        // Position is moved past header.
        parseFirstHeader(byteBuffer);
        // Move position back to beginning
        byteBuffer.position(0);

        // For the latest evio format, generate a table
        // of all event positions in buffer for random access.
        if (evioVersion > 3) {
// System.out.println("EvioReader const: evioVersion = " + evioVersion + ", create mem handler");
            mappedMemoryHandler = new MappedMemoryHandler(byteBuffer);
            if (blockHeader4.hasDictionary()) {
                ByteBuffer buf = mappedMemoryHandler.getFirstMap();
                // Jump to the first event
                prepareForBufferRead(buf);
                // Dictionary is the first event
                readDictionary(buf);
            }
        }
        else {
            // Setting the byte order is only necessary if someone hands
            // this method a buffer in which the byte order is improperly set.
            byteBuffer.order(byteOrder);
            prepareForBufferRead(byteBuffer);
        }

        parser = new EventParser();
    }

    /**
     * This method can be used to avoid creating additional EvioReader
     * objects by reusing this one with another buffer. The method
     * {@link #close()} is called before anything else.
     *
     * @param buf ByteBuffer to be read
     * @throws IOException   if read failure
     * @throws EvioException if buf is null;
     *                       if first block number != 1 when checkBlkNumSeq arg is true
     */
    @Override
    public  void setBuffer(ByteBuffer buf) throws EvioException, IOException {

        if (buf == null) {
            throw new EvioException("arg is null");
        }

        close();

        lastBlock           =  false;
        eventNumber         =  0;
        blockCount          = -1;
        eventCount          = -1;
        blockNumberExpected =  1;
        dictionaryXML       =  null;
        initialPosition     =  buf.position();
        byteBuffer          =  buf.slice();
        sequentialRead      = false;

        parseFirstHeader(byteBuffer);
        byteBuffer.position(0);

        if (evioVersion > 3) {
            mappedMemoryHandler = new MappedMemoryHandler(byteBuffer);
            if (blockHeader4.hasDictionary()) {
                ByteBuffer bb = mappedMemoryHandler.getFirstMap();
                // Jump to the first event
                prepareForBufferRead(bb);
                // Dictionary is the first event
                readDictionary(bb);
            }
        }
        else {
            byteBuffer.order(byteOrder);
            prepareForBufferRead(byteBuffer);
        }

        closed = false;
    }

    /**
     * Has {@link #close()} been called (without reopening by calling
     * {@link #setBuffer(ByteBuffer)})?
     *
     * @return {@code true} if this object closed, else {@code false}.
     */
    @Override
    public boolean isClosed() { return closed; }

    /**
     * Is this reader checking the block number sequence and
     * throwing an exception is it's not sequential and starting with 1?
     * @return <code>true</code> if checking block number sequence, else <code>false</code>
     */
    @Override
    public boolean checkBlockNumberSequence() { return checkBlockNumberSequence; }

    /** Invoke to try header recovery when block headers aren’t where expected
     *    currently, having extraneous words is supported
     *    it doesn't handle missing header words yet (more difficult)
     */
    @Override
    public void addHeaderRecoveryCheck() {this.doHeaderRecoveryCheck=true;}

    /**
     * Get the byte order of the file/buffer being read.
     * @return byte order of the file/buffer being read.
     */
    @Override
    public ByteOrder getByteOrder() { return byteOrder; }

    /**
     * Get the evio version number.
     * @return evio version number.
     */
    @Override
    public int getEvioVersion() {
        return evioVersion;
    }

    /**
      * Get the path to the file.
      * @return path to the file
      */
     @Override
     public String getPath() {
         return path;
     }

    /**
     * Get the file/buffer parser.
     * @return file/buffer parser.
     */
    @Override
    public EventParser getParser() {
        return parser;
    }

    /**
     * Set the file/buffer parser.
     * @param parser file/buffer parser.
     */
    @Override
    public void setParser(EventParser parser) {
        if (parser != null) {
            this.parser = parser;
        }
    }

     /**
     * Get the XML format dictionary if there is one.
     *
     * @return XML format dictionary, else null.
     */
    @Override
    public String getDictionaryXML() {
        return dictionaryXML;
    }

    /**
     * Does this evio file have an associated XML dictionary?
     *
     * @return <code>true</code> if this evio file has an associated XML dictionary,
     *         else <code>false</code>
     */
    @Override
    public boolean hasDictionaryXML() {
        return dictionaryXML != null;
    }

    /** {@inheritDoc} */
    @Override
    public EvioEvent getFirstEvent() {
        try {
            if (hasFirstEvent()) {
                return parseEvent(1);
            }
        }
        catch (Exception e) {}

        return null;
    }

    /** {@inheritDoc} */
    @Override
    public boolean hasFirstEvent() {return firstBlockHeader.hasFirstEvent();}

    /**
     * Get the number of events remaining in the file.
     *
     * @return number of events remaining in the file
     * @throws IOException if failed file access
     * @throws EvioException if failed reading from coda v3 file
     */
    @Override
    public int getNumEventsRemaining() throws IOException, EvioException {
        return getEventCount() - eventNumber;
    }

    /**
     * Get the byte buffer being read directly or corresponding to the event file.
     * Not a very useful method. For files, it works only for evio format versions 2,3 and
     * returns the internal buffer containing an evio block if using sequential access
     * (for example files &gt; 2.1 GB). It returns the memory mapped buffer otherwise.
     * For reading buffers it returns the buffer being read.
     * @return the byte buffer being read (in certain cases).
     */
    @Override
    public ByteBuffer getByteBuffer() {
        return byteBuffer;
    }

    /**
     * Get the size of the file being read, in bytes.
     * For small files, obtain the file size using the memory mapped buffer's capacity.
     * @return the file size in bytes
     */
    @Override
    public long fileSize() {
        return fileSize;
    }


    /**
     * This returns the FIRST block (physical record) header.
     *
     * @return the first block header.
     */
    @Override
    public IBlockHeader getFirstBlockHeader() {
        return firstBlockHeader;
    }


    /**
     * Reads 8 words of the first block (physical record) header in order to determine
     * the evio version # and endianness of the file or buffer in question. These things
     * do <b>not</b> need to be examined in subsequent block headers.
     *
     * @param headerBuf buffer to parse header from.
     * @throws EvioException if buffer too small, contains invalid data,
     *                       or bad block # sequence
     */
    protected void parseFirstHeader(ByteBuffer headerBuf)
            throws EvioException {

        // Check buffer length
        headerBuf.position(0);
        if (headerBuf.remaining() < 32) {
            throw new EvioException("buffer too small");
        }

        // Get the file's version # and byte order
        byteOrder = headerBuf.order();

        int magicNumber = headerBuf.getInt(MAGIC_OFFSET);

        if (magicNumber != IBlockHeader.MAGIC_NUMBER) {
            swap = true;

            if (byteOrder == ByteOrder.BIG_ENDIAN) {
                byteOrder = ByteOrder.LITTLE_ENDIAN;
            }
            else {
                byteOrder = ByteOrder.BIG_ENDIAN;
            }
            headerBuf.order(byteOrder);

            // Reread magic number to make sure things are OK
            magicNumber = headerBuf.getInt(MAGIC_OFFSET);
            if (magicNumber != IBlockHeader.MAGIC_NUMBER) {
System.out.println("ERROR reread magic # (" + magicNumber + ") & still not right");
                throw new EvioException("bad magic #");
            }
        }

        // Check the version number. This requires peeking ahead 5 ints or 20 bytes.
        evioVersion = headerBuf.getInt(VERSION_OFFSET) & VERSION_MASK;
        if (evioVersion < 1)  {
            throw new EvioException("bad version");
        }
//System.out.println("Evio version# = " + evioVersion);

        if (evioVersion >= 4) {
            blockHeader4.setBufferStartingPosition(0);

//                    int pos = 0;
//System.out.println("BlockHeader v4:");
//System.out.println("   block length  = 0x" + Integer.toHexString(headerBuf.getInt(pos))); pos+=4;
//System.out.println("   block number  = 0x" + Integer.toHexString(headerBuf.getInt(pos))); pos+=4;
//System.out.println("   header length = 0x" + Integer.toHexString(headerBuf.getInt(pos))); pos+=4;
//System.out.println("   event count   = 0x" + Integer.toHexString(headerBuf.getInt(pos))); pos+=8;
//System.out.println("   version       = 0x" + Integer.toHexString(headerBuf.getInt(pos))); pos+=8;
//System.out.println("   magic number  = 0x" + Integer.toHexString(headerBuf.getInt(pos))); pos+=4;
//System.out.println();

            // Read the header data
            blockHeader4.setSize(        headerBuf.getInt());
            blockHeader4.setNumber(      headerBuf.getInt());
            blockHeader4.setHeaderLength(headerBuf.getInt());
            blockHeader4.setEventCount(  headerBuf.getInt());
            blockHeader4.setReserved1(   headerBuf.getInt());

            // Use 6th word to set bit info & version
            blockHeader4.parseToBitInfo(headerBuf.getInt());
            blockHeader4.setVersion(evioVersion);
            lastBlock = blockHeader4.getBitInfo(1);
            blockHeader4.setReserved2(headerBuf.getInt());
            blockHeader4.setMagicNumber(headerBuf.getInt());
            blockHeader4.setByteOrder(byteOrder);
            blockHeader = blockHeader4;
            firstBlockHeader = new BlockHeaderV4(blockHeader4);

            // Deal with non-standard header lengths here
            int headerLenDiff = blockHeader4.getHeaderLength() - BlockHeaderV4.HEADER_SIZE;
            // If too small quit with error since headers have a minimum size
            if (headerLenDiff < 0) {
                throw new EvioException("header size too small");
            }

//System.out.println("BlockHeader v4:");
//System.out.println("   block length  = " + blockHeader4.getSize() + " ints");
//System.out.println("   block number  = " + blockHeader4.getNumber());
//System.out.println("   header length = " + blockHeader4.getHeaderLength() + " ints");
//System.out.println("   event count   = " + blockHeader4.getEventCount());
//System.out.println("   version       = " + blockHeader4.getVersion());
//System.out.println("   has Dict      = " + blockHeader4.getBitInfo(0));
//System.out.println("   is End        = " + lastBlock);
//System.out.println("   magic number  = " + Integer.toHexString(blockHeader4.getMagicNumber()));
//System.out.println();
        }
        else {
            // Cache the starting position
            blockHeader2.setBufferStartingPosition(0);

            // read the header data.
            blockHeader2.setSize(        headerBuf.getInt());
            blockHeader2.setNumber(      headerBuf.getInt());
            blockHeader2.setHeaderLength(headerBuf.getInt());
            blockHeader2.setStart(       headerBuf.getInt());
            blockHeader2.setEnd(         headerBuf.getInt());
            // skip version
            headerBuf.getInt();
            blockHeader2.setVersion(evioVersion);
            blockHeader2.setReserved1(   headerBuf.getInt());
            blockHeader2.setMagicNumber( headerBuf.getInt());
            blockHeader2.setByteOrder(byteOrder);
            blockHeader = blockHeader2;

            firstBlockHeader = new BlockHeaderV2(blockHeader2);
        }

        // Store this for later regurgitation of blockCount
        firstBlockSize = 4*blockHeader.getSize();

        // check block number if so configured
        if (checkBlockNumberSequence) {
            if (blockHeader.getNumber() != blockNumberExpected) {
System.out.println("block # out of sequence, got " + blockHeader.getNumber() +
                   " expecting " + blockNumberExpected);

                throw new EvioException("bad block # sequence");
            }
            blockNumberExpected++;
        }
    }


    /**
     * Reads the first block header into a buffer and gets that
     * buffer ready for first-time read.
     *
     * @throws IOException if file access problems
     */
    protected void prepareForSequentialRead() throws IOException {
        // Create a buffer to hold a chunk of data.
        int bytesToRead;

        // Evio format version 4 or greater has a large enough default block size
        // so that reading a single block at a time is not inefficient.
        if (evioVersion > 3) {
            bytesToRead = 4*firstBlockHeader.getSize();
        }
        // Reading data by 32768 byte blocks in older versions is inefficient,
        // so read in 500 block (16MB) chunks.
        else {
            long bytesLeftInFile = fileSize - fileChannel.position();
            bytesToRead = DEFAULT_READ_BYTES < bytesLeftInFile ?
                          DEFAULT_READ_BYTES : (int) bytesLeftInFile;
        }

        if (byteBuffer == null || byteBuffer.capacity() < bytesToRead) {
            byteBuffer = ByteBuffer.allocate(bytesToRead);
            byteBuffer.order(byteOrder);
        }
        byteBuffer.clear().limit(bytesToRead);

        // Read the first chunk of data from file
        fileChannel.read(byteBuffer);
        // Get it ready for reading from internal buffer
        byteBuffer.flip();

        // Position buffer properly (past block header)
        prepareForBufferRead(byteBuffer);
    }


    /**
     * Sets the proper buffer position for first-time read AFTER the first header.
     * @param buffer buffer to prepare
     */
    protected void prepareForBufferRead(ByteBuffer buffer) {
        // Position after header
        int pos = 32;
        buffer.position(pos);

        // Deal with non-standard first header length.
        // No non-standard header lengths in evio version 2 & 3 files.
        if (evioVersion < 4) return;

        int headerLenDiff = blockHeader4.getHeaderLength() - BlockHeaderV4.HEADER_SIZE;
        // Hop over any extra header words
        if (headerLenDiff > 0) {
            for (int i=0; i < headerLenDiff; i++) {
//System.out.println("Skip extra header int");
                pos += 4;
                buffer.position(pos);
            }
        }

        // When reading files sequentially, the file is read into this buffer.
        // If there's a dictionary present, skip over it now.

    }


    /**
     * Reads the block (physical record) header.
     * Assumes mapped buffer or file is positioned at start of the next block header.
     * If a sequential file:
     *   version 4,   Read the entire next block into internal buffer.
     *   version 1-3, If unused data still exists in internal buffer, don't
     *                read anymore in right now as there is at least 1 block there
     *                (integral # of blocks read in).
     *                If no data in internal buffer read DEFAULT_READ_BYTES or the
     *                rest of the file, whichever is smaller, into the internal buffer.
     *
     * By the time this is called,
     * the version # and byte order have already been determined. Not necessary to do that
     * for each block header that's read.<p>
     *
     * A Bank header is 8, 32-bit ints. The first int is the size of the block in ints
     * (not counting the length itself, i.e., the number of ints to follow).
     *
     * Most users should have no need for this method, since most applications do not
     * care about the block (physical record) header.
     *
     * @return status of read attempt
     * @throws IOException if file access problems, evio format problems
     */
    protected ReadStatus processNextBlock() throws IOException {

        // We already read the last block header
        if (lastBlock && !doHeaderRecoveryCheck) {
            return ReadStatus.END_OF_FILE;
        }

        try {

            if (sequentialRead) {

                if (evioVersion < 4) {

                    int bytesInBuf = bufferBytesRemaining();
                    if (bytesInBuf == 0) {

                        // How much of the file is left to read?
                        long bytesLeftInFile = fileSize - fileChannel.position();
                        if (bytesLeftInFile < 32L) {
                            return ReadStatus.END_OF_FILE;
                        }

                        // The block size is 32kB which is on the small side.
                        // We want to read in 16MB (DEFAULT_READ_BYTES) or so
                        // at once for efficiency.
                        int bytesToRead = DEFAULT_READ_BYTES < bytesLeftInFile ?
                                          DEFAULT_READ_BYTES : (int) bytesLeftInFile;

                        // Reset buffer
                        byteBuffer.position(0).limit(bytesToRead);

                        // Read the entire chunk of data
                        int bytesActuallyRead = fileChannel.read(byteBuffer);
                        while (bytesActuallyRead < bytesToRead) {
                            bytesActuallyRead += fileChannel.read(byteBuffer);
                        }

                        byteBuffer.flip();
                        // Now keeping track of pos in this new blockBuffer
                        blockHeader.setBufferStartingPosition(0);
                    }
                    else if (bytesInBuf % 32768 == 0) {
                        // Next block header starts at this position in buffer
                        blockHeader.setBufferStartingPosition(byteBuffer.position());
                    }
                    else {
                        throw new IOException("file contains non-integral # of 32768 byte blocks");
                    }
                }
                else {
                    // Enough data left to read len?
                    if (fileSize - fileChannel.position() < 4L) {
                        return ReadStatus.END_OF_FILE;
                    }

                    long initPos = fileChannel.position();
                    
                    // Read len of block in 32 bit words
                    int blkSize = dataStream.readInt();
                    if (swap) blkSize = Integer.reverseBytes(blkSize);

                    // Check block size, attempt to recover if flag set
                    // (otherwise return exception with a hint to set flag)
                    // System.out.println("blkSize BEFORE = " + blkSize);
                    if(doHeaderRecoveryCheck && fileSize - fileChannel.position() >= 10*4) {

                        int expectedMagicPos = 27; // in words
                        int words_to_skip = 0; // words_to_skip = foundMagicPos - expectedMagicPos
                        
                        // peek at 40 words around current position (32-bit ints, 4 bytes)
                        //    WITHOUT changing current position
                        ByteBuffer peek = ByteBuffer.allocate(40*4).order(byteOrder);
                        // Perform positional read (does NOT move file position)
                        int bytesRead = fileChannel.read(peek, initPos - 20 * 4);
                        peek.flip(); // prepare for reading 
                        if (bytesRead != 40 * 4) {
                            throw new IOException("Failed to read sufficient data for header recovery.");
                        }

                        int foundMagicPos = -1000; // dummy val
                        int[] words = new int[40];
                        for (int i=0; i < 40; i++) {
                            words[i] = peek.getInt();
                            // System.out.println("            peeked word " + i + " = 0x" + Integer.toHexString(words[i]));
                            if(words[i] == IBlockHeader.MAGIC_NUMBER) {
                                foundMagicPos = i;
                                // System.out.println("Found magic # at pos " + foundMagicPos);
                            }
                        }
                        words_to_skip = foundMagicPos-expectedMagicPos;
                        
                        // It's a little more challenging to look backwards than forwards
                        // (since e rely on a relative read just below)
                        // Not sure why, but trying to reset position doesn't work
                        if (words_to_skip <= -1) {
                            System.out.println("Error: According to magic " +
                            "word, block header began earlier than the current file position");
                            // until implemented, return exception
                            return ReadStatus.EVIO_EXCEPTION;
                        }
                        if (words_to_skip > 0) {
                            // Skip over some this many words
                            for (int i=0; i < (words_to_skip); i++) {
                                blkSize = dataStream.readInt();
                                if (swap) blkSize = Integer.reverseBytes(blkSize);
                            }
                        }
                        blkSize+=words_to_skip; // Previous block pointed us to wrong place,  
                                                // but this will fix for future blocks 
                    }

                    // Change to bytes
                    int blkBytes = 4 * (blkSize); 
                    
                    // Enough data left to read rest of block?
                    if (fileSize - fileChannel.position() < blkBytes-4) {
                        return ReadStatus.END_OF_FILE;
                    }

                    // Create a buffer to hold the entire first block of data
                    if (byteBuffer.capacity() >= blkBytes) {
                        byteBuffer.clear();
                        byteBuffer.limit(blkBytes);
                    }
                    else {
                        // Make this bigger than necessary so we're not constantly reallocating
                        byteBuffer = ByteBuffer.allocate(blkBytes + 10000);
                        byteBuffer.limit(blkBytes);
                        byteBuffer.order(byteOrder);
                    }

                    // Read the entire block of data.
                    // First put in length we just read.
                    byteBuffer.putInt(blkSize);

                    // Now the rest of the block (already put int, 4 bytes, in)
                    int bytesActuallyRead = fileChannel.read(byteBuffer) + 4;
                    while (bytesActuallyRead < blkBytes) {
                        bytesActuallyRead += fileChannel.read(byteBuffer);
                    }

                    byteBuffer.flip();
                    // Now keeping track of pos in this new blockBuffer
                    blockHeader.setBufferStartingPosition(0);
                }
            }
            else {
                if (byteBuffer.remaining() < 32) {
                    byteBuffer.clear();
                    return ReadStatus.END_OF_FILE;
                }
                // Record starting position
                blockHeader.setBufferStartingPosition(byteBuffer.position());
            }

            if (evioVersion >= 4) {

                // Read the header data.
                blockHeader4.setSize(byteBuffer.getInt());
                blockHeader4.setNumber(byteBuffer.getInt());
                blockHeader4.setHeaderLength(byteBuffer.getInt());
                blockHeader4.setEventCount(byteBuffer.getInt());
                blockHeader4.setReserved1(byteBuffer.getInt());
                // Use 6th word to set bit info
                blockHeader4.parseToBitInfo(byteBuffer.getInt());
                blockHeader4.setVersion(evioVersion);
                lastBlock = blockHeader4.getBitInfo(1);
                blockHeader4.setReserved2(byteBuffer.getInt());
                blockHeader4.setMagicNumber(byteBuffer.getInt());
                blockHeader = blockHeader4;

                // System.out.println("BlockHeader v4:");
                // System.out.println("   block length  = " + blockHeader4.getSize());
                // System.out.println("   block number  = " + blockHeader4.getNumber());
                // System.out.println("   header length = " + blockHeader4.getHeaderLength());
                // System.out.println("   event count   = " + blockHeader4.getEventCount());
                // System.out.println("   version       = " + blockHeader4.getVersion());
                // System.out.println("   has Dict      = " + blockHeader4.getBitInfo(0));
                // System.out.println("   is End        = " + lastBlock);

                // Deal with non-standard header lengths here
                int headerLenDiff = blockHeader4.getHeaderLength() - BlockHeaderV4.HEADER_SIZE;
                // If too small quit with error since headers have a minimum size
                if (headerLenDiff < 0) {
                    return ReadStatus.EVIO_EXCEPTION;
                }
                // If bigger, read extra ints
                else if (headerLenDiff > 0) {
                    for (int i=0; i < headerLenDiff; i++) {
                        byteBuffer.getInt();
                    }
                }
                
            }
            else if (evioVersion < 4) {
                // read the header data
                blockHeader2.setSize(byteBuffer.getInt());
                blockHeader2.setNumber(byteBuffer.getInt());
                blockHeader2.setHeaderLength(byteBuffer.getInt());
                blockHeader2.setStart(byteBuffer.getInt());
                blockHeader2.setEnd(byteBuffer.getInt());
                // skip version
                byteBuffer.getInt();
                blockHeader2.setVersion(evioVersion);
                blockHeader2.setReserved1(byteBuffer.getInt());
                blockHeader2.setMagicNumber(byteBuffer.getInt());
                blockHeader = blockHeader2;
            }
            else {
                // bad version # - should never happen
                return ReadStatus.EVIO_EXCEPTION;
            }

            // check block number if so configured
            if (checkBlockNumberSequence) {
                if (blockHeader.getNumber() != blockNumberExpected) {

                System.out.println("block # out of sequence, got " + blockHeader.getNumber() +
                                   " expecting " + blockNumberExpected);
                    return ReadStatus.EVIO_EXCEPTION;
                }
                blockNumberExpected++;
            }
        }
        catch (EvioException e) {
            e.printStackTrace();
            return ReadStatus.EVIO_EXCEPTION;
        }
        catch (BufferUnderflowException a) {
            System.err.println("ERROR endOfBuffer " + a);
            byteBuffer.clear();
            return ReadStatus.UNKNOWN_ERROR;
        }

        return ReadStatus.SUCCESS;
    }


    /**
     * This method is only called once at the very beginning if buffer is known to have
     * a dictionary. It then reads that dictionary. Only called in format versions 4 and up.
     * Position buffer after dictionary.
     *
     * @since 4.0
     * @param buffer buffer to read to get dictionary
     * @throws EvioException if failed read due to bad buffer format;
     *                       if version 3 or earlier
     */
     protected void readDictionary(ByteBuffer buffer) throws EvioException {

         if (evioVersion < 4) {
             throw new EvioException("Unsupported version (" + evioVersion + ")");
         }

         // How many bytes remain in this buffer?
         int bytesRemaining = buffer.remaining();
         if (bytesRemaining < 12) {
             throw new EvioException("Not enough data in buffer");
         }

         // Once here, we are assured the entire next event is in this buffer.
         int length;
         length = buffer.getInt();

         if (length < 1) {
             throw new EvioException("Bad value for dictionary length");
         }
         bytesRemaining -= 4;

         // Since we're only interested in length, read but ignore rest of the header.
         buffer.getInt();
         bytesRemaining -= 4;

         // get the raw data
         int eventDataSizeBytes = 4*(length - 1);
         if (bytesRemaining < eventDataSizeBytes) {
             throw new EvioException("Not enough data in buffer");
         }

         byte bytes[] = new byte[eventDataSizeBytes];

         // Read in dictionary data
         try {
            buffer.get(bytes, 0, eventDataSizeBytes);
         }
         catch (Exception e) {
             throw new EvioException("Problems reading buffer");
         }

         // This is the very first event and must be a dictionary
         String[] strs = BaseStructure.unpackRawBytesToStrings(bytes, 0);
         if (strs == null) {
             throw new EvioException("Data in bad format");
         }
         dictionaryXML = strs[0];
     }

    /**
     * Get the event in the file/buffer at a given index (starting at 1).
     * As useful as this sounds, most applications will probably call
     * {@link #parseNextEvent()} or {@link #parseEvent(int)} instead,
     * since it combines combines getting an event with parsing it.<p>
     *
     * @param  index the event number in a 1,2,..N counting sense, from beginning of file/buffer.
     * @return the event in the file/buffer at the given index or null if none
     * @throws IOException   if failed file access
     * @throws EvioException if failed read due to bad file/buffer format;
     *                       if out of memory;
     *                       if index &lt; 1;
     *                       if object closed
     */
    @Override
    public EvioEvent getEvent(int index)
            throws IOException, EvioException {

        if (index < 1) {
            throw new EvioException("index arg starts at 1");
        }

        if (sequentialRead || evioVersion < 4) {
            // Do not fully parse events up to index_TH event
            return gotoEventNumber(index, false);
        }

        //  Version 4 and up && non-sequential
        return getEventV4(index);
    }


    /**
     * Get the event in the file/buffer at a given index (starting at 1).
     * It is only valid for evio versions 4+.
     * As useful as this sounds, most applications will probably call
     * {@link #parseNextEvent()} or {@link #parseEvent(int)} instead,
     * since it combines getting an event with parsing it.
     * Only called if not sequential reading.<p>
     *
     * @param  index the event number in a 1,2,...N counting sense, from beginning of file/buffer.
     * @return the event in the file/buffer at the given index or null if none
     * @throws EvioException if failed read due to bad file/buffer format;
     *                       if out of memory;
     *                       if object closed
     */
    protected EvioEvent getEventV4(int index) throws EvioException {

        if (index > mappedMemoryHandler.getEventCount()) {
            return null;
        }

        if (closed) {
            throw new EvioException("object closed");
        }

        index--;

        EvioEvent event = new EvioEvent();
        BaseStructureHeader header = event.getHeader();

        int length, eventDataSizeBytes = 0;

        ByteBuffer buf = mappedMemoryHandler.getByteBuffer(index);
        length = buf.getInt();

        if (length < 1) {
            throw new EvioException("Bad file/buffer format");
        }
        header.setLength(length);

        // Read and parse second header word
        int word = buf.getInt();
        header.setTag(word >>> 16);
        int dt = (word >> 8) & 0xff;
        // If only 7th bit set, it can be tag=0, num=0, type=0, padding=1.
        // This regularly happens with composite data.
        // However, it that MAY also be the legacy tagsegment type
        // with no padding information. Ignore this as having tag & num
        // in legacy code is probably rare.
        //if (dt == 0x40) {
        //    type = DataType.TAGSEGMENT.getValue();
        //    padding = 0;
        //}
        header.setDataType(dt & 0x3f);
        header.setPadding(dt >>> 6);
        header.setNumber(word & 0xff);

        // Once we know what the data type is, let the no-arg constructed
        // event know what type it is holding so xml names are set correctly.
        event.setXmlNames();

        try {
            // Read the raw data
            eventDataSizeBytes = 4*(length - 1);
            byte bytes[] = new byte[eventDataSizeBytes];
            buf.get(bytes, 0, eventDataSizeBytes);

            event.setRawBytes(bytes);
            event.setByteOrder(byteOrder);
            event.setEventNumber(++eventNumber);
        }
        catch (OutOfMemoryError e) {
            throw new EvioException("Out Of Memory: (event size = " + eventDataSizeBytes + ")", e);
        }
        catch (Exception e) {
            throw new EvioException("Error", e);
        }

        return event;
    }


	/**
	 * This is a workhorse method. It retrieves the desired event from the file/buffer,
     * and then parses it SAX-like. It will drill down and uncover all structures
     * (banks, segments, and tagsegments) and notify any interested listeners.
	 *
     * @param  index number of event desired, starting at 1, from beginning of file/buffer
	 * @return the parsed event at the given index or null if none
     * @throws IOException if failed file access
     * @throws EvioException if failed read due to bad file/buffer format;
     *                       if out of memory;
     *                       if index &lt; 1;
     *                       if object closed
	 */
	@Override
    public EvioEvent parseEvent(int index) throws IOException, EvioException {
		EvioEvent event = getEvent(index);
        if (event != null) parseEvent(event);
		return event;
	}


    /**
     * Get the next event in the file/buffer. As useful as this sounds, most
     * applications will probably call {@link #parseNextEvent()} instead, since
     * it combines getting the next event with parsing the next event.<p>
     *
     * Although this method can get events in versions 4+, it now delegates that
     * to another method. No changes were made to this method from versions 1-3 in order
     * to read the version 4 format as it is subset of versions 1-3 with variable block
     * length.
     *
     * @return the next event in the file.
     *         On error it throws an EvioException.
     *         On end of file, it returns <code>null</code>.
     * @throws IOException   if failed file access
     * @throws EvioException if failed read due to bad buffer format;
     *                       if object closed
     */
    @Override
    public EvioEvent nextEvent() throws IOException, EvioException {

        if (!sequentialRead && evioVersion > 3) {
            return getEvent(eventNumber+1);
        }

        if (closed) {
            throw new EvioException("object closed");
        }

        EvioEvent event = new EvioEvent();
        BaseStructureHeader header = event.getHeader();
        long currentPosition = byteBuffer.position();

        // How many bytes remain in this block until we reach the next block header?
        int blockBytesRemaining = blockBytesRemaining();

        if (blockBytesRemaining < 0) {
            throw new EvioException("Number of block bytes remaining is negative.");
        }

        // Are we at the block boundary? If so, read/skip-over next header.
        // Read in more blocks of data if necessary.
        //
        // version 1-3:
        // We now read in bigger chunks that are integral multiples of a single block
        // (32768 bytes). Must see if we have to deal with an event crossing physical
        // record boundaries. Previously, java evio only read 1 block at a time.
        if (blockBytesRemaining == 0) {
            ReadStatus status = processNextBlock();
            if (status == ReadStatus.SUCCESS) {
                return nextEvent();
            }
            else if (status == ReadStatus.END_OF_FILE) {
                return null;
            }
            else {
                throw new EvioException("Failed reading block header in nextEvent.");
            }
        }
        // Or have we already read in the last event?
        // If jevio versions 1-3, the last block may not be full.
        // Thus bytesRemaining may be > 0, but we may have read
        // in all the existing data. (This should never happen in version 4).
        else if (blockHeader.getBufferEndingPosition() == currentPosition) {
            return null;
        }

        // Version   4: Once here, we are assured the entire next event is in this block.
        //
        // Version 1-3: No matter what, we can get the length of the next event.
        //              This is because we read in multiples of blocks each with
        //              an integral number of 32 bit words.
        int length;
        length = byteBuffer.getInt();
        if (length < 1) {
            throw new EvioException("non-positive length (0x" + Integer.toHexString(length) + ")");
        }
        header.setLength(length);
        blockBytesRemaining -= 4; // just read in 4 bytes

        // Versions 1-3: if we were unlucky, after reading the length
        //               there are no bytes remaining in this block.
        // Don't really need the "if (version < 4)" here except for clarity.
        if (evioVersion < 4) {
            if (bufferBytesRemaining() == 0) {
                ReadStatus status = processNextBlock();
                if (status == ReadStatus.END_OF_FILE) {
                    return null;
                }
                else if (status != ReadStatus.SUCCESS) {
                    throw new EvioException("Failed reading block header in nextEvent.");
                }
                blockBytesRemaining = blockBytesRemaining();
            }
        }

        // Now we should be good to go, except data may cross block boundary.
        // In any case, should be able to read the rest of the header.

        // Read and parse second header word
        int word = byteBuffer.getInt();
        header.setTag(word >>> 16);
        int dt = (word >> 8) & 0xff;
        // If only 7th bit set, it can be tag=0, num=0, type=0, padding=1.
        // This regularly happens with composite data.
        // However, it that MAY also be the legacy tagsegment type
        // with no padding information. Ignore this as having tag & num
        // in legacy code is probably rare.
        //if (dt == 0x40) {
        //    type = DataType.TAGSEGMENT.getValue();
        //    padding = 0;
        //}
        header.setDataType(dt & 0x3f);
        header.setPadding(dt >>> 6);
        header.setNumber(word & 0xff);

        blockBytesRemaining -= 4; // just read in 4 bytes

        // get the raw data
        int eventDataSizeBytes = 4*(length - 1);

        try {
            byte bytes[] = new byte[eventDataSizeBytes];

            int bytesToGo = eventDataSizeBytes;
            int offset = 0;

            // Don't really need the "if (version < 4)" here except for clarity.
            if (evioVersion < 4) {

                // Be in while loop if have to cross block boundary[ies].
                while (bytesToGo > 0) {

                    // Don't read more than what is left in current block
                    int bytesToReadNow = bytesToGo > blockBytesRemaining ?
                                         blockBytesRemaining : bytesToGo;

                    // Read in bytes remaining in internal buffer
                    byteBuffer.get(bytes, offset, bytesToReadNow);
                    offset               += bytesToReadNow;
                    bytesToGo            -= bytesToReadNow;
                    blockBytesRemaining  -= bytesToReadNow;

                    if (blockBytesRemaining == 0) {
                        ReadStatus status = processNextBlock();
                        if (status == ReadStatus.END_OF_FILE) {
                            return null;
                        }
                        else if (status != ReadStatus.SUCCESS) {
                            throw new EvioException("Failed reading block header after crossing boundary in nextEvent.");
                        }

                        blockBytesRemaining  = blockBytesRemaining();
                    }
                }
            }

            // Last (perhaps only) read
            byteBuffer.get(bytes, offset, bytesToGo);
            event.setRawBytes(bytes);
            event.setByteOrder(byteOrder); // add this to track endianness, timmer
            // Don't worry about dictionaries here as version must be 1-3
            event.setEventNumber(++eventNumber);
            return event;
        }
        catch (OutOfMemoryError ome) {
            System.out.println("Out Of Memory\n" +
                                       "eventDataSizeBytes = " + eventDataSizeBytes + "\n" +
                                       "bytes Remaining = " + blockBytesRemaining + "\n" +
                                       "event Count: " + eventCount);
            return null;
        }
        catch (Exception e) {
            e.printStackTrace();
            return null;
        }
    }


	/**
	 * This is a workhorse method. It retrieves the next event from the file/buffer,
     * and then parses it SAX-like. It will drill down and uncover all structures
     * (banks, segments, and tagsegments) and notify any interested listeners.
	 *
	 * @return the event that was parsed.
     *         On error it throws an EvioException.
     *         On end of file, it returns <code>null</code>.
     * @throws IOException if failed file access
     * @throws EvioException if read failure or bad format
     *                       if object closed
     */
	@Override
    public EvioEvent parseNextEvent() throws IOException, EvioException {
		EvioEvent event = nextEvent();
		if (event != null) {
			parseEvent(event);
		}
		return event;
	}

	/**
	 * This will parse an event, SAX-like. It will drill down and uncover all structures
     * (banks, segments, and tagsegments) and notify any interested listeners.<p>
	 *
	 * As useful as this sounds, most applications will probably call {@link #parseNextEvent()}
     * instead, since it combines getting the next event with parsing the next event.<p>
     *
	 * @param evioEvent the event to parse.
	 * @throws EvioException if bad format
	 */
	@Override
    public void parseEvent(EvioEvent evioEvent) throws EvioException {
		parser.parseEvent(evioEvent, false);
	}


    /**
     * Parse the given byte array into an EvioEvent.
     * Byte array must not be in file format (have block headers),
     * but must consist of only the bytes comprising the evio event.
     *
     * @param array   data to parse.
     * @param offset  offset into array to begin parsing.
     * @param order   byte order of data.
     * @return the EvioEvent object parsed from the given bytes.
     * @throws EvioException if null arg, too little data, or data not in evio format.
     */
    public static EvioEvent parseEvent(byte[] array, int offset, ByteOrder order) throws EvioException {

        if (array == null || array.length - offset < 8) {
            throw new EvioException("arg null or too little data");
        }

        int byteLen = array.length - offset;
        EvioEvent event = new EvioEvent();
        BaseStructureHeader header = event.getHeader();
        ByteBuffer byteBuffer = ByteBuffer.wrap(array, offset, byteLen).order(order);

        // Read the first header word - the length in 32bit words
        int words = byteBuffer.getInt();
        if (words < 1) {
            throw new EvioException("non-positive length (0x" + Integer.toHexString(words) + ")");
        }
        else if (4*(words + 1) < byteLen) {
            throw new EvioException("too little data (needed " + (4*(words + 1)) +
                                    " but have " + byteLen + " bytes)");
        }
        header.setLength(words);

        // Read and parse second header word
        int word = byteBuffer.getInt();
        header.setTag(word >>> 16);
        int dt = (word >> 8) & 0xff;
        header.setDataType(dt & 0x3f);
        header.setPadding(dt >>> 6);
        header.setNumber(word & 0xff);

        // Set the raw data
        int dataBytes = 4*(words - 1);
        byte data[] = new byte[dataBytes];
        System.arraycopy(array, offset+8, data, 0, dataBytes);
        event.setRawBytes(data);
        event.setByteOrder(order);

        EventParser.eventParse(event);
        return event;
    }


    /**
     * Get an evio bank or event in byte array form.
     * @param eventNumber number of event of interest
     * @return array containing bank's/event's bytes.
     * @throws IOException if failed file access
     * @throws EvioException if eventNumber &lt; 1;
     *                       if the event number does not correspond to an existing event;
     *                       if object closed
     */
    @Override
    public byte[] getEventArray(int eventNumber)
            throws EvioException, IOException {

        EvioEvent ev = gotoEventNumber(eventNumber, false);
        if (ev == null) {
            throw new EvioException("event number must be > 0");
        }
        return ev.toArray();
    }

    /**
     * Get an evio bank or event in ByteBuffer form.
     * @param eventNumber number of event of interest
     * @return buffer containing bank's/event's bytes.
     * @throws IOException if failed file access
     * @throws EvioException if eventNumber &lt; 1;
     *                       if the event number does not correspond to an existing event;
     *                       if object closed
     */
    @Override
    public ByteBuffer getEventBuffer(int eventNumber)
            throws EvioException, IOException {
        return ByteBuffer.wrap(getEventArray(eventNumber)).order(byteOrder);
    }

    /**
   	 * Get the number of bytes remaining in the internal byte buffer.
     * Called only by {@link #nextEvent()}.
   	 *
   	 * @return the number of bytes remaining in the current block (physical record).
        */
   	protected int bufferBytesRemaining() {
        return byteBuffer.remaining();
   	}

    /**
   	 * Get the number of bytes remaining in the current block (physical record).
     * This is used for pathology checks like crossing the block boundary.
     * Called only by {@link #nextEvent()}.
   	 *
   	 * @return the number of bytes remaining in the current block (physical record).
        */
   	protected int blockBytesRemaining() {
   		try {
               return blockHeader.bytesRemaining(byteBuffer.position());
   		}
   		catch (EvioException e) {
   			e.printStackTrace();
   			return -1;
   		}
   	}

	/**
	 * The equivalent of rewinding the file. What it actually does
     * is set the position of the file/buffer back to where it was
     * after calling the constructor - after the first header.
     * This method, along with the two <code>position()</code> and the
     * <code>close()</code> method, allows applications to treat files
     * in a normal random access manner.
     *
     * @throws IOException   if failed file access or buffer/file read
     * @throws EvioException if object closed
	 */
    @Override
    public void rewind() throws IOException, EvioException {
        if (closed) {
            throw new EvioException("object closed");
        }

        if (sequentialRead) {
            fileChannel.position(initialPosition);
            prepareForSequentialRead();
        }
        else if (evioVersion < 4) {
            byteBuffer.position(initialPosition);
            prepareForBufferRead(byteBuffer);
        }

        lastBlock = false;
        eventNumber = 0;
        blockNumberExpected = 1;

        if (evioVersion < 4) {
            blockHeader = blockHeader2 = new BlockHeaderV2((BlockHeaderV2) firstBlockHeader);
        }
        else {
            blockHeader = blockHeader4 = new BlockHeaderV4((BlockHeaderV4) firstBlockHeader);
        }

        blockHeader.setBufferStartingPosition(initialPosition);

        if (sequentialRead && hasDictionaryXML()) {
            // Dictionary is always the first event so skip over it.
            // For sequential reads, do this after each rewind.
            nextEvent();
        }
	}

	/**
	 * This is equivalent to obtaining the current position in the file.
     * What it actually does is return the position of the buffer. This
     * method, along with the <code>rewind()</code>, <code>position(int)</code>
     * and the <code>close()</code> method, allows applications to treat files
     * in a normal random access manner. Only meaningful to evio versions 1-3
     * and for sequential reading.<p>
	 *
	 * @return the position of the buffer; -1 if version 4+
     * @throws IOException   if error accessing file
     * @throws EvioException if object closed
     */
	@Override
    public long position() throws IOException, EvioException {
        if (!sequentialRead && evioVersion > 3) return -1L;

        if (closed) {
            throw new EvioException("object closed");
        }

        if (sequentialRead) {
            return fileChannel.position();
        }
		return byteBuffer.position();
	}

//	/**
//	 * This method sets the current position in the file or buffer. This
//     * method, along with the <code>rewind()</code>, <code>position()</code>
//     * and the <code>close()</code> method, allows applications to treat files
//     * in a normal random access manner. Only meaningful to evio versions 1-3
//     * and for sequential reading.<p>
//     *
//     * <b>HOWEVER</b>, using this method is not necessary for random access of
//     * events and is no longer recommended because it interferes with the sequential
//     * reading of events. Therefore it is now deprecated.
//     *
//	 * @deprecated
//	 * @param position the new position of the buffer.
//     * @throws IOException   if error accessing file
//     * @throws EvioException if object closed
//     */
//	@Override
//    public void position(long position) throws IOException, EvioException  {
//        if (!sequentialRead && evioVersion > 3) return;
//
//        if (closed) {
//            throw new EvioException("object closed");
//        }
//
//        if (sequentialRead) {
//            fileChannel.position(position);
//        }
//        else {
//            byteBuffer.position((int)position);
//        }
//	}

	/**
	 * This is closes the file, but for buffers it only sets the position to 0.
     * This method, along with the <code>rewind()</code> and the two
     * <code>position()</code> methods, allows applications to treat files
     * in a normal random access manner.
     *
     * @throws IOException if error accessing file
	 */
    @Override
    public void close() throws IOException {
        if (closed) {
            return;
        }

        if (!sequentialRead && evioVersion > 3) {
            if (byteBuffer != null) byteBuffer.position(initialPosition);
            mappedMemoryHandler = null;

            if (fileChannel != null) {
                fileChannel.close();
                fileChannel = null;
            }

            closed = true;
            return;
        }

        if (sequentialRead) {
            fileChannel.close();
            dataStream.close();
        }
        else {
            byteBuffer.position(initialPosition);
        }

        closed = true;
    }

	/**
	 * This returns the current (active) block (physical record) header.
     * Since most users have no interest in physical records, this method
     * should not be used. Mostly it is used by the test programs in the
	 * <code>EvioReaderTest</code> class.
	 *
	 * @return the current block header.
	 */
	@Override
    public IBlockHeader getCurrentBlockHeader() {
		return blockHeader;
	}

    /**
     * Go to a specific event in the file. The events are numbered 1..N.
     * This number is transient--it is not part of the event as stored in the evio file.
     * In versions 4 and up this is just a wrapper on {@link #getEvent(int)}.
     *
     * @param evNumber the event number in a 1..N counting sense, from the start of the file.
     * @return the specified event in file or null if there's an error or nothing at that event #.
     * @throws IOException if failed file access
     * @throws EvioException if object closed
     */
    @Override
    public EvioEvent gotoEventNumber(int evNumber) throws IOException, EvioException {
        return gotoEventNumber(evNumber, true);
    }


    /**
     * Go to a specific event in the file. The events are numbered 1..N.
     * This number is transient--it is not part of the event as stored in the evio file.
     * Before version 4, this does the work for {@link #getEvent(int)}.
     *
     * @param  evNumber the event number in a 1,2,..N counting sense, from beginning of file/buffer.
     * @param  parse if {@code true}, parse the desired event
     * @return the specified event in file or null if there's an error or nothing at that event #.
     * @throws IOException if failed file access
     * @throws EvioException if object closed
     */
    protected EvioEvent gotoEventNumber(int evNumber, boolean parse)
            throws IOException, EvioException {

        if (evNumber < 1) {
			return null;
		}

        if (closed) {
            throw new EvioException("object closed");
        }

        if (!sequentialRead && evioVersion > 3) {
            try {
                if (parse) {
                    return parseEvent(evNumber);
                }
                else {
                    return getEvent(evNumber);
                }
            }
            catch (EvioException e) {
                return null;
            }
        }

		rewind();
		EvioEvent event;

		try {
			// get the first evNumber - 1 events without parsing
			for (int i = 1; i < evNumber; i++) {
				event = nextEvent();
				if (event == null) {
					throw new EvioException("Asked to go to event: " + evNumber + ", which is beyond the end of file");
				}
			}
			// get one more event, the evNumber'th event
            if (parse) {
			    return parseNextEvent();
            }
            else {
                return nextEvent();
            }
		}
		catch (EvioException e) {
			e.printStackTrace();
		}
		return null;
	}

    /**
     * Rewrite the file to XML (not including dictionary).
     *
     * @param path the path to the XML file.
     *
     * @return the status of the write.
     * @throws IOException   if failed file access
     * @throws EvioException if object closed
     */
    @Override
    public WriteStatus toXMLFile(String path) throws IOException, EvioException {
        return toXMLFile(path, false);
    }

    /**
     * Rewrite the file to XML (not including dictionary).
     *
     * @param path the path to the XML file.
     * @param hex if true, ints get displayed in hexadecimal
     *
     * @return the status of the write.
     * @throws IOException   if failed file access
     * @throws EvioException if object closed
     */
    @Override
    public WriteStatus toXMLFile(String path, boolean hex) throws IOException, EvioException {
        return toXMLFile(path, null, hex);
    }

    /**
     * Rewrite the file to XML (not including dictionary).
	 *
	 * @param path the path to the XML file.
	 * @param progressListener and optional progress listener, can be <code>null</code>.
	 * @return the status of the write.
     * @throws IOException   if failed file access
     * @throws EvioException if object closed
     * @see IEvioProgressListener
	 */
	@Override
    public WriteStatus toXMLFile(String path, IEvioProgressListener progressListener)
                throws IOException, EvioException {
        return toXMLFile(path, progressListener, false);
    }

    /**
     * Rewrite the file to XML (not including dictionary).
	 *
	 * @param path the path to the XML file.
	 * @param progressListener and optional progress listener, can be <code>null</code>.
     * @param hex if true, ints get displayed in hexadecimal
     *
	 * @return the status of the write.
     * @throws IOException   if failed file access
     * @throws EvioException if object closed
     * @see IEvioProgressListener
	 */
	@Override
    public WriteStatus toXMLFile(String path,
                                              IEvioProgressListener progressListener,
                                              boolean hex)
                throws IOException, EvioException {

        if (closed) {
            throw new EvioException("object closed");
        }

        FileOutputStream fos;

		try {
			fos = new FileOutputStream(path);
		}
		catch (FileNotFoundException e) {
			e.printStackTrace();
			return WriteStatus.CANNOT_OPEN_FILE;
		}

		try {
			XMLStreamWriter xmlWriter = XMLOutputFactory.newInstance().createXMLStreamWriter(fos);
			xmlWriter.writeStartDocument();
			xmlWriter.writeCharacters("\n");
			xmlWriter.writeComment("Event source file: " + path);

			// start the root element
			xmlWriter.writeCharacters("\n");
			xmlWriter.writeStartElement(ROOT_ELEMENT);
			xmlWriter.writeAttribute("numevents", "" + getEventCount());
            xmlWriter.writeCharacters("\n");

            // The difficulty is that this method can be called at
            // any time. So we need to save our state and then restore
            // it when we're done.
            ReaderState state = getState();

            // Go to the beginning
			rewind();

			// now loop through the events
			EvioEvent event;
			try {
				while ((event = parseNextEvent()) != null) {
					event.toXML(xmlWriter, hex);
					// anybody interested in progress?
					if (progressListener != null) {
						progressListener.completed(event.getEventNumber(), getEventCount());
					}
				}
			}
			catch (EvioException e) {
				e.printStackTrace();
				return WriteStatus.UNKNOWN_ERROR;
			}

			// done. Close root element, end the document, and flush.
			xmlWriter.writeEndElement();
			xmlWriter.writeEndDocument();
			xmlWriter.flush();
			xmlWriter.close();

			try {
				fos.close();
			}
			catch (IOException e) {
				e.printStackTrace();
			}

            // Restore our original settings
            restoreState(state);

		}
		catch (XMLStreamException e) {
			e.printStackTrace();
			return WriteStatus.UNKNOWN_ERROR;
		}
        catch (FactoryConfigurationError e) {
            return WriteStatus.UNKNOWN_ERROR;
        }
        catch (EvioException e) {
            return WriteStatus.EVIO_EXCEPTION;
        }

		return WriteStatus.SUCCESS;
	}


    /**
     * This is the number of events in the file. Any dictionary event is <b>not</b>
     * included in the count. In versions 3 and earlier, it is not computed unless
     * asked for, and if asked for it is computed and cached.
     *
     * @return the number of events in the file.
     * @throws IOException   if failed file access
     * @throws EvioException if read failure;
     *                       if object closed
     */
    @Override
    public int getEventCount() throws IOException, EvioException {

        if (closed) {
            throw new EvioException("object closed");
        }

        // sequentialRead is always false for reading buffer
        if (!sequentialRead && evioVersion > 3) {
            return mappedMemoryHandler.getEventCount();
        }

        if (eventCount < 0) {
            // The difficulty is that this method can be called at
            // any time. So we need to save our state and then restore
            // it when we're done.
            ReaderState state = getState();

            rewind();
            eventCount = 0;

            while (nextEvent() != null) {
                eventCount++;
            }

            // If sequential access to v2 file, then nextEvent() places
            // new data into byteBuffer. Restoring the original state
            // is useless without also restoring/re-reading the data.
            if (sequentialRead) {
                rewind();

                // Skip dictionary
                if (hasDictionaryXML()) {
                    nextEvent();
                }

                // Go back to original event # & therefore buffer data
                for (int i=1; i < state.eventNumber; i++) {
                    nextEvent();
                }
            }

            // Restore our original settings
            restoreState(state);
        }

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
     * @throws EvioException if object closed
     * @return the number of blocks in the file (estimate for version 3 files)
     */
    @Override
    public int getBlockCount() throws EvioException{

        if (closed) {
            throw new EvioException("object closed");
        }

        if (!sequentialRead && evioVersion > 3) {
            return mappedMemoryHandler.getBlockCount();
        }

        if (blockCount < 0) {
            // Although block size is theoretically adjustable, I believe
            // that everyone used 8192 words for the block size in version 3.
            blockCount = (int) (fileSize/firstBlockSize);
        }

        return blockCount;
    }


}