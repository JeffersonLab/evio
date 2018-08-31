/*
 *   Copyright (c) 2016.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.hipo;

import org.jlab.coda.jevio.*;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * Reader class that reads files stored in the HIPO format.<p>
 *
 * <pre>
 * File has this structure:
 *
 *    +----------------------------------+
 *    |      General File Header         |
 *    +----------------------------------+
 *    +----------------------------------+
 *    |         Index (optional)         |
 *    +----------------------------------+
 *    +----------------------------------+
 *    |     User Header (optional)       |
 *    +----------------------------------+
 *    +----------------------------------+
 *    |                                  |
 *    |            Record 1              |
 *    |                                  |
 *    |                                  |
 *    |                                  |
 *    +----------------------------------+
 *                   ...
 *    +----------------------------------+
 *    |                                  |
 *    |            Record N              |
 *    |                                  |
 *    |                                  |
 *    |                                  |
 *    +----------------------------------+
 *    +----------------------------------+
 *    |       Trailer (optional)         |
 *    +----------------------------------+
 *    +----------------------------------+
 *    |    Trailer's Index (optional)    |
 *    +----------------------------------+
 *
 *
 *
 * Buffer or streamed data has this structure:
 *
 *    +----------------------------------+
 *    |                                  |
 *    |            Record 1              |
 *    |                                  |
 *    |                                  |
 *    |                                  |
 *    +----------------------------------+
 *                   ...
 *    +----------------------------------+
 *    |                                  |
 *    |            Record N              |
 *    |                                  |
 *    |                                  |
 *    |                                  |
 *    +----------------------------------+
 *    +----------------------------------+
 *    |       Trailer (optional)         |
 *    +----------------------------------+
 *
 * The important thing with a buffer or streaming is for the last header or
 * trailer to set the "last record" bit.
 *
 * </pre>
 *
 * @version 6.0
 * @since 6.0 08/10/2017
 * @author gavalian
 * @author timmer
 * @see BufferReader
 * @see FileHeader
 * @see RecordInputStream
 */
public class Reader {
    /**
     * List of records in the file. The array is initialized
     * when the entire file is scanned to read out positions
     * of each record in the file (in constructor).
     */
    protected final List<RecordPosition> recordPositions = new ArrayList<RecordPosition>();

    /** Fastest way to read/write files. */
    protected RandomAccessFile inStreamRandom;

    protected ByteBuffer buffer;

    protected int initialBufferPosition;

    /** Keep one record for reading in data record-by-record. */
    protected final RecordInputStream inputRecordStream = new RecordInputStream();
    
    /** Number or position of last record to be read. */
    protected int currentRecordLoaded;

    /** File header. */
    protected FileHeader fileHeader;

    /** First record's header. */
    protected RecordHeader firstRecordHeader;

    /** Files may have an xml format dictionary in the user header of the file header. */
    protected String dictionaryXML;

    /** Each file of a set of split CODA files may have a "first" event common to all. */
    protected byte[] firstEvent;

    /** Object to handle event indexes in context of file and having to change records. */
    protected FileEventIndex eventIndex = new FileEventIndex();

    /** Are we reading from file (true) or buffer? */
    protected boolean fromFile = true;


    
    /** Is this object currently closed? */
    private boolean closed;
    /** Byte order of file/buffer being read. */
    private ByteOrder byteOrder;
    /** Evio version of file/buffer being read. */
    private int evioVersion;

    /**
     * Default constructor. Does nothing.
     * The {@link #open(String)} method has to be called to open the input stream.
     * Also {@link #forceScanFile()} needs to be called to find records.
     */
    public Reader() {}

    /**
     * Constructor with file already opened.
     * The method {@link #forceScanFile()} needs to be called to find records.
     * @param file open file
     */
    public Reader(RandomAccessFile file) {
        inStreamRandom = file;
    }

    /**
     * Constructor with filename. Creates instance and opens
     * the input stream with given name. Uses existing indexes
     * in file before scanning.
     * @param filename input file name.
     */
    public Reader(String filename){
        open(filename);
        scanFile(false);
    }

    /**
     * Constructor with filename. Creates instance and opens
     * the input stream with given name.
     * @param filename input file name.
     * @param forceScan if true, force a scan of file, else use existing indexes first.
     */
    public Reader(String filename, boolean forceScan) {
        open(filename);
        if (forceScan){
           forceScanFile();
        } else {
            scanFile(forceScan);
        }
    }

    /**
     * Constructor for reading buffer with evio data.
     * Buffer must be ready to read with position and limit set properly.
     * @param buffer buffer with evio data.
     */
    public Reader(ByteBuffer buffer) {
        this.buffer = buffer;
        initialBufferPosition = buffer.position();
        scanBuffer();
    }

    /**
     * Opens an input stream in binary mode. Scans for
     * records in the file and stores record information
     * in internal array. Each record can be read from the file.
     * @param filename input file name
     */
    public final void open(String filename){
        if(inStreamRandom != null && inStreamRandom.getChannel().isOpen()) {
            try {
                System.out.println("[READER] ---> closing current file : " + inStreamRandom.getFilePointer());
                inStreamRandom.close();
            } catch (IOException ex) {
                Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
            }
        }

        try {
            System.out.println("[READER] ----> opening current file : " + filename);
            inStreamRandom = new RandomAccessFile(filename,"r");
            System.out.println("[READER] ---> open successful, size : " + inStreamRandom.length());
        } catch (FileNotFoundException ex) {
            Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
        } catch (IOException ex) {
            Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
        }
    }

    /**
     * This is closes the file, but for buffers it only sets the position to 0.
     * This method, along with the <code>rewind()</code> and the two
     * <code>position()</code> methods, allows applications to treat files
     * in a normal random access manner.
     *
     * @throws IOException if error accessing file
     */
    public synchronized void close() throws IOException {
        if (closed) {
            return;
        }

        if (fromFile) {
            inStreamRandom.close();
        }

        closed = true;
    }

    /**
     * This method can be used to avoid creating additional Reader
     * objects by reusing this one with another buffer.
     * @param buf ByteBuffer to be read
     */
    public void setBuffer(ByteBuffer buf) {
        if (buf == null) {
            return;
        }

        buffer = buf;
        initialBufferPosition = buffer.position();
        scanBuffer();
        closed = false;
    }

    /**
     * Get the byte order of the file/buffer being read.
     * @return byte order of the file/buffer being read.
     */
    public ByteOrder getByteOrder() {return byteOrder;}

    /**
     * Set the byte order of the file/buffer being read.
     * @param order byte order of the file/buffer being read.
     */
    private void setByteOrder(ByteOrder order) {byteOrder = order;}

    /**
     * Get the Evio format version number of the file/buffer being read.
     * @return Evio format version number of the file/buffer being read.
     */
    public int  getVersion() {return evioVersion;}

    /**
     * Get the XML format dictionary if there is one.
     * @return XML format dictionary, else null.
     */
    public String getDictionary() {
        // Read in dictionary if necessary
        extractDictionary();
        return dictionaryXML;
    }

    /**
     * Does this evio file/buffer have an associated XML dictionary?
     * @return <code>true</code> if this evio file/buffer has an associated XML dictionary,
     *         else <code>false</code>.
     */
    public boolean hasDictionary() {
        if (fromFile) {
            return fileHeader.hasDictionary();
        }
        return firstRecordHeader.hasDictionary();
    }

    /**
     * Get a byte array representing the first event.
     * @return byte array representing the first event. Null if none.
     */
    public byte[] getFirstEvent() {
        // Read in first event if necessary
        extractDictionary();
        return firstEvent;
    }

    /**
     * Does this evio file/buffer have an associated first event?
     * @return <code>true</code> if this evio file/buffer has an associated first event,
     *         else <code>false</code>.
     */
    public boolean hasFirstEvent() {
        if (fromFile) {
            return fileHeader.hasFirstEvent();
        }
        return firstRecordHeader.hasFirstEvent();
    }

    /**
     * Get the number of events in file/buffer.
     * @return number of events in file/buffer.
     */
    public int getEventCount() {return eventIndex.getMaxEvents();}

    /**
     * Get the number of records read from the file/buffer.
     * @return number of records read from the file/buffer.
     */
    public int getRecordCount() {return recordPositions.size();}

    /**
     * Returns the list of record positions in the file.
     * @return 
     */
    public List<RecordPosition> getRecordPositions(){ return this.recordPositions;}
    // Methods for current record

    /**
     * Get a byte array representing the next event from the file/buffer while sequentially reading.
     * @return byte array representing the next event or null if there is none.
     * @throws HipoException if file/buffer not in hipo format
     */
    public byte[] getNextEvent() throws HipoException {
        // If reached last event ...
        if (!eventIndex.canAdvance()) return null;
        if (eventIndex.advance()) {
            // If here, the next event is in the next record
            readRecord(eventIndex.getRecordNumber());
        }
        if(inputRecordStream.getEntries()==0){
            System.out.println("[READER] first time reading");
            readRecord(eventIndex.getRecordNumber());
        }
        return inputRecordStream.getEvent(eventIndex.getRecordEventNumber());
    }

    /**
     * Get a byte array representing the previous event from the sequential queue.
     * @return byte array representing the previous event or null if there is none.
     * @throws HipoException if the file/buffer is not in HIPO format
     */
    public byte[] getPrevEvent() throws HipoException {
        if(!eventIndex.canRetreat()) return null;
        if(eventIndex.retreat()){
            readRecord(eventIndex.getRecordNumber());
        }
        return inputRecordStream.getEvent(eventIndex.getRecordEventNumber());
    }

    /**
     * Reads user header of the file header/first record header of buffer.
     * The returned ByteBuffer also contains endianness of the file/buffer.
     * @return ByteBuffer containing the user header of the file/buffer.
     */
    public ByteBuffer readUserHeader() {
        int userLen = fileHeader.getUserHeaderLength();
        System.out.println("  " + fileHeader.getUserHeaderLength()
         + "  " + fileHeader.getHeaderLength() + "  " + fileHeader.getIndexLength());
        try {
            byte[] userBytes = new byte[userLen];

            if (fromFile) {
                inStreamRandom.getChannel().position(fileHeader.getHeaderLength() + fileHeader.getIndexLength());
                inStreamRandom.read(userBytes);
            }
            else {
                buffer.position(firstRecordHeader.getHeaderLength() + firstRecordHeader.getIndexLength());
                buffer.get(userBytes, 0, userLen);
            }

            return ByteBuffer.wrap(userBytes);

        } catch (IOException ex) {
            Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
        }
        return null;
    }

    /**
     * Get a byte array representing the specified event from the file/buffer.
     * If index is out of bounds, null is returned.
     * @param index index of specified event within the entire file/buffer,
     *              contiguous starting at 0.
     * @return byte array representing the specified event or null if
     *         index is out of bounds.
     * @throws HipoException if file/buffer not in hipo format
     */
    public byte[] getEvent(int index) throws HipoException {
        
        if (index < 0 || index >= eventIndex.getMaxEvents()) {
            return null;
        }
        
        if (eventIndex.setEvent(index)) {
            // If here, the event is in the next record
            readRecord(eventIndex.getRecordNumber());
        }
        if(inputRecordStream.getEntries()==0){
            System.out.println("[READER] first time reading");
            readRecord(eventIndex.getRecordNumber());
        }
        return inputRecordStream.getEvent(eventIndex.getRecordEventNumber());
    }

    /**
     * Get a byte array representing the specified event from the file/buffer
     * and place it in the given buf.
     * If no buf is given (arg is null), create a buffer internally and return it.
     * If index is out of bounds, null is returned.
     * @param buf buffer in which to place event data.
     * @param index index of specified event within the entire file/buffer,
     *              contiguous starting at 0.
     * @return buf or null if buf is null or index out of bounds.
     * @throws HipoException if file/buffer not in hipo format, or
     *                       if buf has insufficient space to contain event
     *                       (buf.capacity() < event size).
     */
    public ByteBuffer getEvent(ByteBuffer buf, int index) throws HipoException {
        if (index < 0 || index >= eventIndex.getMaxEvents()) {
            return null;
        }

        if (eventIndex.setEvent(index)) {
            // If here, the event is in the next record
            readRecord(eventIndex.getRecordNumber());
        }
        return inputRecordStream.getEvent(buf, eventIndex.getRecordEventNumber());
    }

    /**
     * Checks if the file has an event to read next.
     * @return true if the next event is available, false otherwise
     */
    public boolean hasNext(){
       return eventIndex.canAdvance();
    }
    /**
     * Checks if the stream has previous event to be accessed through, getPrevEvent()
     * @return true if previous event is accessible, false otherwise
     */
    public boolean hasPrev(){
       return eventIndex.canRetreat();
    }
    /**
     * Get the number of events in current record.
     * @return number of events in current record.
     */
    public int getRecordEventCount() {return inputRecordStream.getEntries();}

    /**
     * Get the index of the current record.
     * @return index of the current record.
     */
    public int getCurrentRecord() {return currentRecordLoaded;}

    /**
     * Get the current record stream.
     * @return current record stream.
     */
    public RecordInputStream getCurrentRecordStream() {return inputRecordStream;}

    /**
     * Reads record from the file/buffer at the given record index.
     * @param index record index  (starting at 0).
     * @return true if valid index and successful reading record, else false.
     * @throws HipoException if file/buffer not in hipo format
     */
    public boolean readRecord(int index) throws HipoException {
        if (index >= 0 && index < recordPositions.size()) {
            RecordPosition pos = recordPositions.get(index);
            if (fromFile) {
                inputRecordStream.readRecord(inStreamRandom, pos.getPosition());
            }
            else {
                inputRecordStream.readRecord(buffer, (int)pos.getPosition());
            }
            currentRecordLoaded = index;
            return true;
        }
        return false;
    }

    /** Extract dictionary and first event from file/buffer if possible, else do nothing. */
    protected void extractDictionary() {
        // If already read & parsed ...
        if (dictionaryXML != null || firstEvent != null) {
            return;
        }

        if (fromFile) {
            extractDictionaryFromFile();
            return;
        }
        extractDictionaryFromBuffer();
    }

    /** Extract dictionary and first event from buffer if possible, else do nothing. */
    protected void extractDictionaryFromBuffer() {

        // If no dictionary or first event ...
        if (!firstRecordHeader.hasDictionary() && !firstRecordHeader.hasFirstEvent()) {
            return;
        }

        int userLen = firstRecordHeader.getUserHeaderLength();
        // 8 byte min for evio event, more for xml dictionary
        if (userLen < 8) {
            return;
        }

        RecordInputStream record;

        try {
            // Position right before record header's user header
            buffer.position(initialBufferPosition +
                            firstRecordHeader.getHeaderLength() +
                            firstRecordHeader.getIndexLength());
            // Read user header
            byte[] userBytes = new byte[userLen];
            buffer.get(userBytes, 0, userLen);
            ByteBuffer userBuffer = ByteBuffer.wrap(userBytes);
            
            // Parse user header as record
            record = new RecordInputStream(firstRecordHeader.getByteOrder());
            record.readRecord(userBuffer, 0);
        }
        catch (HipoException e) {
            // Not in proper format
            return;
        }

        int eventIndex = 0;

        // Dictionary always comes first in record
        if (firstRecordHeader.hasDictionary()) {
            // Just plain ascii, not evio format
            byte[] dict = record.getEvent(eventIndex++);
            try {dictionaryXML = new String(dict, "US-ASCII");}
            catch (UnsupportedEncodingException e) {/* never happen */}
        }

        // First event comes next
        if (firstRecordHeader.hasFirstEvent()) {
            firstEvent = record.getEvent(eventIndex);
        }
    }

    /** Extract dictionary and first event from file if possible, else do nothing. */
    protected void extractDictionaryFromFile() {

        // If no dictionary or first event ...
        if (!fileHeader.hasDictionary() && !fileHeader.hasFirstEvent()) {
            return;
        }

        int userLen = fileHeader.getUserHeaderLength();
        // 8 byte min for evio event, more for xml dictionary
        if (userLen < 8) {
            return;
        }
        
        RecordInputStream record;

        try {
            // Position right before file header's user header
            inStreamRandom.getChannel().position(fileHeader.getHeaderLength() + fileHeader.getIndexLength());
            // Read user header
            byte[] userBytes = new byte[userLen];
            inStreamRandom.read(userBytes);
            ByteBuffer userBuffer = ByteBuffer.wrap(userBytes);
            // Parse user header as record
            record = new RecordInputStream(fileHeader.getByteOrder());
            record.readRecord(userBuffer, 0);
        }
        catch (IOException e) {
            // Can't read file
            return;
        }
        catch (HipoException e) {
            // Not in proper format
            return;
        }

        int eventIndex = 0;

        // Dictionary always comes first in record
        if (fileHeader.hasDictionary()) {
            // Just plain ascii, not evio format
            byte[] dict = record.getEvent(eventIndex++);
            try {dictionaryXML = new String(dict, "US-ASCII");}
            catch (UnsupportedEncodingException e) {/* never happen */}
        }

        // First event comes next
        if (fileHeader.hasFirstEvent()) {
            firstEvent = record.getEvent(eventIndex);
        }
    }

    //-----------------------------------------------------------------

    /** Scan buffer to find all records and store their position, length, and event count. */
    public void scanBuffer() {

        byte[] headerBytes = new byte[RecordHeader.HEADER_SIZE_BYTES];
        ByteBuffer headerBuffer = ByteBuffer.wrap(headerBytes);

        RecordHeader recordHeader = new RecordHeader();
        boolean haveFirstRecordHeader = false;

        try {
            // Scan buffer by reading each record header and
            // storing its position, length, and event count.
            int bufSize = buffer.remaining();

            // Don't go beyond 1 header length before EOB since we'll be reading in 1 header
            int maximumSize = bufSize - RecordHeader.HEADER_SIZE_BYTES;
            recordPositions.clear();

            // First record position is at beginning
            int recordPosition = initialBufferPosition;
            int offset;
                        
            while (recordPosition < maximumSize) {
                buffer.position(recordPosition);
                buffer.get(headerBytes, 0, RecordHeader.HEADER_SIZE_BYTES);
                recordHeader.readHeader(headerBuffer);
                byteOrder = recordHeader.getByteOrder();
                evioVersion = recordHeader.getVersion();

                // Save the first record header
                if (!haveFirstRecordHeader) {
                    firstRecordHeader = new RecordHeader(recordHeader);
                    haveFirstRecordHeader = true;
                }
                //System.out.println(">>>>>==============================================");
                //System.out.println(recordHeader.toString());
                offset = recordHeader.getLength();
                RecordPosition pos = new RecordPosition(recordPosition, offset,
                                                        recordHeader.getEntries());
                //System.out.println(" RECORD HEADER ENTRIES = " + recordHeader.getEntries());
                recordPositions.add(pos);
                // Track # of events in this record for event index handling
                eventIndex.addEventSize(recordHeader.getEntries());
                recordPosition += offset;
            }
            //eventIndex.show();
            //System.out.println("NUMBER OF RECORDS " + recordPositions.size());
        } catch (HipoException ex) {
            Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
        }
    }


    /** Stores info of all the (top-level) events. */
    private final ArrayList<EvioNode> eventNodes = new ArrayList<>(1000);

    /** Store info of all block headers. */
    private final HashMap<Integer, BlockNode> blockNodes = new HashMap<>(20);


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
        boolean  firstBlock=true, hasDictionary=false;
        //boolean  curLastBlock=false;

//        long t2, t1 = System.currentTimeMillis();

        BufferNode bufferNode = new BufferNode(buffer);

        // Start at the beginning of buffer without changing
        // its current position. Do this with absolute gets.
        int position  = initialBufferPosition;
        int bytesLeft = buffer.limit() - position;

        // Keep track of the # of blocks, events, and valid words in file/buffer
        int blockCount = 0;
        int eventCount = 0;
        int validDataWords = 0;
        BlockNode blockNode;

        try {

            while (bytesLeft > 0) {
                // Need enough data to at least read 1 block header (32 bytes)
                if (bytesLeft < 32) {
                    throw new EvioException("Bad evio format: extra " + bytesLeft +
                                                    " bytes at file end");
                }

                // Swapping is taken care of
                blockSize       = buffer.getInt(position);
                byteInfo        = buffer.getInt(position + 4* BlockHeaderV4.EV_VERSION);
                blockHdrSize    = buffer.getInt(position + 4*BlockHeaderV4.EV_HEADERSIZE);
                blockEventCount = buffer.getInt(position + 4*BlockHeaderV4.EV_COUNT);
                magicNum        = buffer.getInt(position + 4*BlockHeaderV4.EV_MAGIC);

//                System.out.println("    genEvTablePos: blk ev count = " + blockEventCount +
//                                           ", blockSize = " + blockSize +
//                                           ", blockHdrSize = " + blockHdrSize +
//                                           ", byteInfo  = " + byteInfo);

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

                // File is now positioned before block header.
                // Look at block header to get info.
                blockNode = new BlockNode();

                blockNode.setPos(position);
                blockNode.setLen(blockSize);
                blockNode.setCount(blockEventCount);

                blockNodes.put(blockCount, blockNode);
                bufferNode.getBlockNodes().add(blockNode);

                blockNode.setPlace(blockCount++);
                validDataWords += blockSize;
//                curLastBlock    = BlockHeaderV4.isLastBlock(byteInfo);
                if (firstBlock) hasDictionary = BlockHeaderV4.hasDictionary(byteInfo);

                // Hop over block header to events
                position  += 4*blockHdrSize;
                bytesLeft -= 4*blockHdrSize;

//System.out.println("    hopped blk hdr, pos = " + position + ", is last blk = " + curLastBlock);

                // Check for a dictionary - the first event in the first block.
                // It's not included in the header block count, but we must take
                // it into account by skipping over it.
                if (firstBlock && hasDictionary) {
                    firstBlock = false;

                    // Get its length - bank's len does not include itself
                    byteLen = 4*(buffer.getInt(position) + 1);

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

                    EvioNode node = EvioNode.extractEventNode(bufferNode, blockNode,
                                                     position, eventCount + i);
//System.out.println("      event "+i+" in block: pos = " + node.pos +
//                           ", dataPos = " + node.dataPos + ", ev # = " + (eventCount + i + 1));
                    eventNodes.add(node);
                    //blockNode.allEventNodes.add(node);

                    // Hop over header + data
                    byteLen = 8 + 4*node.getDataLength();
                    position  += byteLen;
                    bytesLeft -= byteLen;

                    if (byteLen < 8 || bytesLeft < 0) {
                        throw new EvioException("Bad evio format: bad bank length");
                    }

//System.out.println("    hopped event " + (i+1) + ", bytesLeft = " + bytesLeft + ", pos = " + position + "\n");
                }

                eventCount += blockEventCount;
            }
        }
        catch (IndexOutOfBoundsException e) {
            throw new EvioException("Bad evio format", e);
        }
    }

    /** Scan file to find all records and store their position, length, and event count. */
    public void forceScanFile() {
        
        byte[] headerBytes = new byte[RecordHeader.HEADER_SIZE_BYTES];
        ByteBuffer headerBuffer = ByteBuffer.wrap(headerBytes);
        
        fileHeader = new FileHeader();
        RecordHeader recordHeader = new RecordHeader();

        boolean haveFirstRecordHeader = false;
        
        try {
            // Scan file by reading each record header and
            // storing its position, length, and event count.
            long fileSize = inStreamRandom.length();
            // Don't go beyond 1 header length before EOF since we'll be reading in 1 header
            long maximumSize = fileSize - RecordHeader.HEADER_SIZE_BYTES;
            recordPositions.clear();
            int  offset;
            FileChannel channel = inStreamRandom.getChannel().position(0L);

            // Read and parse file header
            inStreamRandom.read(headerBytes);
            fileHeader.readHeader(headerBuffer);
            byteOrder = fileHeader.getByteOrder();
            evioVersion = fileHeader.getVersion();

            // First record position (past file's header + index + user header)
            //long recordPosition = fileHeader.getLength();
            long recordPosition = fileHeader.getHeaderLength() + 
                    fileHeader.getUserHeaderLength() + fileHeader.getIndexLength();
            /*System.out.println(" READER FORCE SCAN : HEADER LENGTH = " 
                    + fileHeader.getLength() + " USER HEADER LENGTH = " + fileHeader.getUserHeaderLength()
            + " INDEX LENGTH = " + fileHeader.getIndexLength() + "  HEADER LENGTH = " + fileHeader.getHeaderLength()
            + "  INDEX LENGTH = " + fileHeader.getIndexLength());*/
            while (recordPosition < maximumSize) {
                channel.position(recordPosition);
                inStreamRandom.read(headerBytes); 
                recordHeader.readHeader(headerBuffer);
                // Save the first record header
                if (!haveFirstRecordHeader) {
                    firstRecordHeader = new RecordHeader(recordHeader);
                    haveFirstRecordHeader = true;
                }
                //System.out.println(">>>>>==============================================");
                //System.out.println(recordHeader.toString());
                offset = recordHeader.getLength();
                RecordPosition pos = new RecordPosition(recordPosition, offset,
                                                        recordHeader.getEntries());
                //System.out.println(" RECORD HEADER ENTRIES = " + recordHeader.getEntries());
                recordPositions.add(pos);
                // Track # of events in this record for event index handling
                eventIndex.addEventSize(recordHeader.getEntries());
                recordPosition += offset;
            }
            //eventIndex.show();
            //System.out.println("NUMBER OF RECORDS " + recordPositions.size());
        } catch (IOException ex) {
            Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
        } catch (HipoException ex) {
            Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
        }
    }

    /**
     * Scans the file to index all the record positions.
     * It takes advantage of any existing indexes in file.
     * @param force if true, force a file scan even if header
     *              or trailer have index info.
     */
    protected void scanFile(boolean force) {

        if (force) {
            forceScanFile();
            return;
        }

        //System.out.println("[READER] ---> scanning the file");
        byte[] headerBytes = new byte[RecordHeader.HEADER_SIZE_BYTES];
        ByteBuffer headerBuffer = ByteBuffer.wrap(headerBytes);

        fileHeader = new FileHeader();
        RecordHeader recordHeader = new RecordHeader();

        try {
            FileChannel channel = inStreamRandom.getChannel().position(0L);

            // Read and parse file header
            inStreamRandom.read(headerBytes);
            fileHeader.readHeader(headerBuffer);
            byteOrder = fileHeader.getByteOrder();
            evioVersion = fileHeader.getVersion();

            // Is there an existing record length index?
            // Index in trailer gets first priority.
            // Index in file header gets next priority.
            boolean fileHasIndex = fileHeader.hasTrailerWithIndex() || (fileHeader.hasIndex());
            /*System.out.println(" file has index = " + fileHasIndex
                    + "  " + fileHeader.hasTrailerWithIndex()
                    + "  " + fileHeader.hasIndex()); */

            // If there is no index, scan file
            if (!fileHasIndex) {
                forceScanFile();
                return;
            }

            // Take care of non-standard header size
            channel.position(fileHeader.getHeaderLength());
            //System.out.println(header.toString());

            // First record position (past file's header + index + user header)
            long recordPosition = fileHeader.getLength();
            //System.out.println("record position = " + recordPosition);

            int indexLength;
            // If we have a trailer with indexes ...
            if (fileHeader.hasTrailerWithIndex()) {
                // Position read right before trailing header
                channel.position(fileHeader.getTrailerPosition());
                // Read trailer
                inStreamRandom.read(headerBytes);
                recordHeader.readHeader(headerBuffer);
                indexLength = recordHeader.getIndexLength();
            }
            else {
                // If index immediately follows file header,
                // we're already in position to read it.
                indexLength = fileHeader.getIndexLength();
            }

            // Read indexes
            byte[] index = new byte[indexLength];
            inStreamRandom.read(index);
            int len, count;
            try {
                // Turn bytes into record lengths & event counts
                int[] intData = ByteDataTransformer.toIntArray(index, fileHeader.getByteOrder());
                // Turn record lengths into file positions and store in list
                recordPositions.clear();
                for (int i=0; i < intData.length; i += 2) {
                    len = intData[i];
                    count = intData[i+1];
                    RecordPosition pos = new RecordPosition(recordPosition, len, count);
                    recordPositions.add(pos);
                    // Track # of events in this record for event index handling
                    eventIndex.addEventSize(count);
                    recordPosition += len;
                }
            }
            catch (EvioException e) {/* never happen */}

        } catch (IOException ex) {
            Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
        } catch (HipoException ex) {
            Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
        }
    }
    
    public void show(){
        System.out.println(" ***** FILE: (info), RECORDS = "
                + recordPositions.size() + " *****");
        for(RecordPosition entry : this.recordPositions){
            System.out.println(entry);
        }
    }
    
    /**
     * Internal class to keep track of the records in the file/buffer.
     * Each entry keeps record position in the file/buffer, length of
     * the record and number of entries contained.
     */
    public static class RecordPosition {
        /** Position in file/buffer. */
        private long position;
        /** Length in bytes. */
        private int  length;
        /** Number of entries in record. */
        private int  count;

        RecordPosition(long pos) {position = pos;}
        RecordPosition(long pos, int len, int cnt) {
            position = pos; length = len; count = cnt;
        }

        public RecordPosition setPosition(long _pos){ position = _pos; return this; }
        public RecordPosition setLength(int _len)   { length = _len;   return this; }
        public RecordPosition setCount( int _cnt)   { count = _cnt;    return this; }
        
        public long getPosition(){ return position;}
        public int  getLength(){   return   length;}
        public int  getCount(){    return    count;}
        
        @Override
        public String toString(){
            return String.format(" POSITION = %16d, LENGTH = %12d, COUNT = %8d", position, length, count);
        }
    }
    
    public static void main(String[] args){
        Reader reader = new Reader("/Users/gavalian/Work/Software/project-3a.0.0/Distribution/clas12-offline-software/coatjava/clas_000810_324.hipo",true);
        
        int icounter = 0;
        //reader.show();
        while(reader.hasNext()==true){
            System.out.println(" reading event # " + icounter);
            try {
                byte[] event = reader.getNextEvent();
            } catch (HipoException ex) {
                Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
            }

            icounter++;
        }
        
        //reader.open("test.evio");
        /*reader.readRecord(0);
        int nevents = reader.getRecordEventCount();
        System.out.println("-----> events = " + nevents);
        for(int i = 0; i < 10 ; i++){
            byte[] event = reader.getEvent(i);
            System.out.println("---> events length = " + event.length);
            ByteBuffer buffer = ByteBuffer.wrap(event);
            buffer.order(ByteOrder.LITTLE_ENDIAN);
            String data = DataUtils.getStringArray(buffer, 10,30);
            System.out.println(data);
        }*/
    }
}
