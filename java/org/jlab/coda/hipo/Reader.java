/*
 *   Copyright (c) 2016.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.hipo;

import org.jlab.coda.jevio.*;

import java.io.IOException;
import java.io.RandomAccessFile;
import java.io.UnsupportedEncodingException;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.util.ArrayList;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * Reader class that reads files stored in the HIPO format.
 *
 * <pre><code>
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
 *    |                                  |              RecordPosition
 *    |                                  |
 *    +----------------------------------+
 *    +----------------------------------+
 *    |       Trailer (optional)         |
 *    +----------------------------------+
 *
 * The important thing with a buffer or streaming is for the last header or
 * trailer to set the "last record" bit.
 *
 * </code></pre>
 *
 * Something to keep in mind is one can intersperse sequential calls
 * (getNextEvent, getPrevEvent, or getNextEventNode) with random access
 * calls (getEvent or getEventNode), and the sequence remains unchanged
 * after the random access.
 *
 * @version 6.0
 * @since 6.0 08/10/2017
 * @author gavalian
 * @author timmer
 * @see FileHeader
 * @see RecordInputStream
 */
public class Reader {

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


    /**
     * List of records in the file. The array is initialized
     * when the entire file is scanned to read out positions
     * of each record in the file (in constructor).
     */
    protected final List<RecordPosition> recordPositions = new ArrayList<RecordPosition>();
    /** Fastest way to read/write files. */
    protected RandomAccessFile inStreamRandom;
    /** File being read. */
    protected String fileName;
    /** File size in bytes. */
    protected long fileSize;
    /** File header. */
    protected FileHeader fileHeader;
    /** Are we reading from file (true) or buffer? */
    protected boolean fromFile = true;


    /** Buffer being read. */
    protected ByteBuffer buffer;
    /** Buffer used to temporarily hold data while decompressing. */
    protected ByteBuffer tempBuffer;
    /** Initial position of buffer. */
    protected int bufferOffset;
    /** Limit of buffer. */
    protected int bufferLimit;


    /** Keep one record for reading in data record-by-record. */
    protected final RecordInputStream inputRecordStream = new RecordInputStream();
    /** Number or position of last record to be read. */
    protected int currentRecordLoaded;
    /** First record's header. */
    protected RecordHeader firstRecordHeader;
    /** Record number expected when reading. Used to check sequence of records. */
    protected int recordNumberExpected = 1;
    /** If true, throw an exception if record numbers are out of sequence. */
    protected boolean checkRecordNumberSequence;
    /** Object to handle event indexes in context of file and having to change records. */
    protected FileEventIndex eventIndex = new FileEventIndex();

    // For garbage-free parsing
    protected byte[] headerBytes;
    protected ByteBuffer headerBuffer;
    protected RecordHeader recordHeader;

    /** Files may have an xml format dictionary in the user header of the file header. */
    protected String dictionaryXML;
    /** Each file of a set of split CODA files may have a "first" event common to all. */
    protected byte[] firstEvent;
    /** Stores info of all the (top-level) events in a scanned buffer. */
    protected final ArrayList<EvioNode> eventNodes = new ArrayList<>(1000);

    /** Is this object currently closed? */
    protected boolean closed;
    /** Is this data in file/buffer compressed? */
    protected boolean compressed;
    /** Byte order of file/buffer being read. */
    protected ByteOrder byteOrder;
    /** Keep track of next EvioNode when calling {@link #getNextEventNode()},
     * {@link #getEvent(int)}, or {@link #getPrevEvent()}. */
    protected int sequentialIndex = -1;

    /**
     * If this buf/file contains non-evio events (permissible to read in this class),
     * set this flag to false, which helps EvioCompactReader and EvioReader to avoid
     * choking while trying to parse them.
     */
    protected boolean evioFormat = true;

    /** If true, the last sequential call was to getNextEvent or getNextEventNode.
     *  If false, the last sequential call was to getPrevEvent. Used to determine
     *  which event is prev or next. */
    protected boolean lastCalledSeqNext;
    /** Evio version of file/buffer being read. */
    protected int evioVersion = 6;
    /** Source (pool) of EvioNode objects used for parsing Evio data in buffer. */
    protected EvioNodeSource nodePool;

    /** Place to store data read in from record header. */
    private int[] headerInfo = new int[7];

    
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
    public Reader(RandomAccessFile file) {inStreamRandom = file;}

    /**
     * Constructor with filename. Creates instance and opens
     * the input stream with given name. Uses existing indexes
     * in file before scanning.
     * @param filename input file name.
     * @throws IOException   if error reading file
     * @throws HipoException if file is not in the proper format or earlier than version 6
     */
    public Reader(String filename) throws IOException, HipoException {
        open(filename, true);
    }

    /**
     * Constructor with filename. Creates instance and opens
     * the input stream with given name.
     * @param filename input file name.
     * @param forceScan if true, force a scan of file, else use existing indexes first.
     * @throws IOException   if error reading file
     * @throws HipoException if file is not in the proper format or earlier than version 6
     */
    public Reader(String filename, boolean forceScan) throws IOException, HipoException {
        open(filename, false);
        scanFile(forceScan);
    }

    /**
     * Constructor for reading buffer with evio data.
     * Buffer must be ready to read with position and limit set properly.
     * If the given buffer contains compressed data, it is uncompressed into another buffer.
     * The buffer containing the newly uncompressed data then becomes the internal buffer of
     * this object. It can be obtained by calling {@link #getBuffer}.
     *
     * @param buffer buffer with evio data.
     * @throws HipoException if buffer too small, not in the proper format, or earlier than version 6
     */
    public Reader(ByteBuffer buffer) throws HipoException {
        this(buffer, null);
    }

    /**
     * Constructor for reading buffer with evio data.
     * Buffer must be ready to read with position and limit set properly.
     * If the given buffer contains compressed data, it is uncompressed into another buffer.
     * The buffer containing the newly uncompressed data then becomes the internal buffer of
     * this object. It can be obtained by calling {@link #getBuffer}.
     *
     * @param buffer buffer with evio data.
     * @param pool pool of EvioNode objects for garbage-free operation.
     * @throws HipoException if buffer too small, not in the proper format, or earlier than version 6
     */
    public Reader(ByteBuffer buffer, EvioNodeSource pool) throws HipoException {
        this(buffer, pool, false);
    }

    /**
     * Constructor for reading buffer with evio data.
     * Buffer must be ready to read with position and limit set properly.
     * If the given buffer contains compressed data, it is uncompressed into another buffer.
     * The buffer containing the newly uncompressed data then becomes the internal buffer of
     * this object. It can be obtained by calling {@link #getBuffer}.
     *
     * @param buffer buffer with evio data.
     * @param pool pool of EvioNode objects for garbage-free operation.
     * @param checkRecordNumSeq if true, check to see if all record numbers are in order,
     *                          if not throw exception.
     * @throws HipoException if buffer too small, not in the proper format, or earlier than version 6;
     *                       if checkRecordNumSeq is true and records are out of sequence.
     */
    public Reader(ByteBuffer buffer, EvioNodeSource pool, boolean checkRecordNumSeq) throws HipoException {
        this.buffer = buffer;
        bufferOffset = buffer.position();
        bufferLimit  = buffer.limit();
        nodePool = pool;
        fromFile = false;
        checkRecordNumberSequence = checkRecordNumSeq;

        // For garbage-free parsing
        headerBytes = new byte[RecordHeader.HEADER_SIZE_BYTES];
        headerBuffer = ByteBuffer.wrap(headerBytes);
        recordHeader = new RecordHeader();
        firstRecordHeader = new RecordHeader();

        ByteBuffer bb = scanBuffer();
        if (compressed) {
            // scanBuffer() will uncompress all data in buffer
            // and store it in the returned buffer (bb).
            // Make that our internal buffer so we don't have to do any more uncompression.
            this.buffer = bb;
            compressed = false;
        }
    }

    /**
     * Opens an input stream in binary mode. Scans for
     * records in the file and stores record information
     * in internal array. Each record can be read from the file.
     * @param filename input file name
     * @throws IOException if file not found or error opening file
     * @throws HipoException if file is not in the proper format or earlier than version 6
     */
    public final void open(String filename) throws IOException, HipoException {
        open(filename, true);
    }

    /**
     * Opens an input stream in binary mode. Scans for
     * records in the file and stores record information
     * in internal array. Each record can be read from the file.
     * @param filename input file name
     * @param scan if true, call scanFile(false).
     * @throws IOException if file not found or error opening file
     * @throws HipoException if file is not in the proper format or earlier than version 6
     */
    public final void open(String filename, boolean scan) throws IOException, HipoException {
        if (inStreamRandom != null && inStreamRandom.getChannel().isOpen()) {
            try {
                //System.out.println("[READER] ---> closing current file : " + inStreamRandom.getFilePointer());
                inStreamRandom.close();
            }
            catch (IOException ex) {}
        }

        // This may be called after using a buffer as input, so zero some things out
        buffer = null;
        bufferOffset = 0;
        bufferLimit  = 0;
        fromFile = true;

        fileName = filename;

        //System.out.println("[READER] ----> opening file : " + filename);
        inStreamRandom = new RandomAccessFile(filename,"r");
        fileSize = inStreamRandom.length();
        if (scan) {
            scanFile(false);
        }

        //System.out.println("[READER] ---> open successful, size : " + inStreamRandom.length());
    }

    /**
     * This closes the file.
     * @throws IOException if error accessing file
     */
    public void close() throws IOException {
        if (closed) {
            return;
        }

        if (fromFile) {
            inStreamRandom.close();
        }

        closed = true;
    }

    /**
     * Has {@link #close()} been called (without reopening by calling
     * {@link #setBuffer(ByteBuffer)})?
     *
     * @return {@code true} if this object closed, else {@code false}.
     */
    public boolean isClosed() {return closed;}

    /**
     * Is a file being read?
     * @return {@code true} if a file is being read, {@code false} if it's a buffer.
     */
    public boolean isFile() {return fromFile;}

    /**
     * This method can be used to avoid creating additional Reader
     * objects by reusing this one with another buffer.
     * If the given buffer contains compressed data, it is uncompressed into another buffer.
     * The buffer containing the newly uncompressed data then becomes the internal buffer of
     * this object. It can be obtained by calling {@link #getBuffer}.
     *
     * @param buf ByteBuffer to be read
     * @throws HipoException if buf arg is null, buffer too small,
     *                       not in the proper format, or earlier than version 6
     */
    public void setBuffer(ByteBuffer buf) throws HipoException {
        setBuffer(buf, null);
    }

    /**
     * This method can be used to avoid creating additional Reader
     * objects by reusing this one with another buffer. The pool is <b>not</b>
     * reset in this method. Caller may do that prior to calling this method.
     * If the given buffer contains compressed data, it is uncompressed into another buffer.
     * The buffer containing the newly uncompressed data then becomes the internal buffer of
     * this object. It can be obtained by calling {@link #getBuffer}.
     *
     * @param buf ByteBuffer to be read
     * @param pool pool of EvioNode objects to use when parsing buf.
     * @throws HipoException if buf arg is null, buffer too small,
     *                       not in the proper format, or earlier than version 6
     */
    public void setBuffer(ByteBuffer buf, EvioNodeSource pool) throws HipoException {
        if (buf == null) {
            throw new HipoException("buf arg is null");
        }

        nodePool = pool;
        buffer = buf;
        bufferLimit = buffer.limit();
        bufferOffset = buffer.position();
        eventIndex = new FileEventIndex();

        eventNodes.clear();
        recordPositions.clear();
        
        fromFile = false;
        compressed = false;
        firstEvent = null;
        dictionaryXML = null;
        sequentialIndex = 0;
        currentRecordLoaded = 0;

        ByteBuffer bb = scanBuffer();
        if (compressed) {
            // scanBuffer() will uncompress all data in buffer
            // and store it in the returned buffer (bb).
            // Make that our internal buffer so we don't have to do any more uncompression.
            this.buffer = bb;
            compressed = false;
        }

        closed = false;
    }

    /**
     * Get the name of the file being read.
     * @return name of the file being read or null if none.
     */
    public String getFileName() {return fileName;}

    /**
     * Get the size of the file being read, in bytes.
     * @return size of the file being read, in bytes, or 0 if none.
     */
    public long getFileSize() {return fileSize;}

    /**
     * Get the buffer being read, if any.
     * @return buffer being read, if any.
     */
    public ByteBuffer getBuffer() {return buffer;}

    /**
     * Get the beginning position of the buffer being read.
     * @return the beginning position of the buffer being read.
     */
    public int getBufferOffset() {return bufferOffset;}

    /**
     * Get the file header from reading a file.
     * Will return null if reading a buffer.
     * @return file header from reading a file.
     */
    public FileHeader getFileHeader() {return fileHeader;}

    /**
     * Get the first record header from reading a file/buffer.
     * @return first record header from reading a file/buffer.
     */
    public RecordHeader getFirstRecordHeader() {return firstRecordHeader;}

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
    public int getVersion() {return evioVersion;}

    /**
     * Is the data in the file/buffer compressed?
     * @return true if data is compressed.
     */
    public boolean isCompressed() {return compressed;}

    /**
     * Does this file/buffer contain non-evio format events?
     * @return true if all events are in evio format, else false.
     */
    public boolean isEvioFormat() {return evioFormat;}

    /**
     * Get the XML format dictionary if there is one.
     * @return XML format dictionary, else null.
     */
    public String getDictionary() {
        // Read in dictionary if necessary
        extractDictionaryAndFirstEvent();
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
        extractDictionaryAndFirstEvent();
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
     * @return the list of record positions in the file.
     */
    public List<RecordPosition> getRecordPositions() {return recordPositions;}

    /**
     * Get the list of EvioNode objects contained in the buffer being read.
     * To be used internally to evio.
     * @return list of EvioNode objects contained in the buffer being read.
     */
    public ArrayList<EvioNode> getEventNodes() {return eventNodes;}

    /**
     * Get whether or not record numbers are enforced to be sequential.
     * @return {@code true} if record numbers are enforced to be sequential.
     */
    public boolean getCheckRecordNumberSequence() {return checkRecordNumberSequence;}

    /**
     * Get the number of events remaining in the file/buffer.
     * Useful only if doing a sequential read.
     *
     * @return number of events remaining in the file/buffer
     */
    public int getNumEventsRemaining() {return (eventIndex.getMaxEvents() - sequentialIndex);}

    // Methods for current record

    /**
     * Get a byte array representing the next event from the file/buffer while sequentially reading.
     * If the previous call was to {@link #getPrevEvent}, this will get the event
     * past what that returned. Once the last event is returned, this will return null.
     * @return byte array representing the next event or null if there is none.
     * @throws HipoException if file/buffer not in hipo format
     */
    public byte[] getNextEvent() throws HipoException {
        boolean debug = false;

        // If the last method called was getPrev, not getNext,
        // we don't want to get the same event twice in a row, so
        // increment index. Take into account if this is the
        // first time getNextEvent or getPrevEvent called.
        if (sequentialIndex < 0) {
            sequentialIndex = 0;
            if (debug) System.out.println("getNextEvent first time index set to " + sequentialIndex);
        }
        // else if last call was to getPrevEvent ...
        else if (!lastCalledSeqNext) {
            sequentialIndex++;
            if (debug) System.out.println("getNextEvent extra increment to " + sequentialIndex);
        }

        byte[] array = getEvent(sequentialIndex++);
        lastCalledSeqNext = true;

        if (array == null) {
            if (debug) System.out.println("getNextEvent hit limit at index " + (sequentialIndex - 1) +
                                          ", set to " + (sequentialIndex - 1) + "\n");
            sequentialIndex--;
        }
        else {
            if (debug) System.out.println("getNextEvent got event " + (sequentialIndex - 1) + "\n");
        }

        return array;
    }

    /**
     * Get a byte array representing the previous event from the sequential queue.
     * If the previous call was to {@link #getNextEvent}, this will get the event
     * previous to what that returned. If this is called before getNextEvent,
     * it will always return null. Once the first event is returned, this will
     * return null.
     * @return byte array representing the previous event or null if there is none.
     * @throws HipoException if the file/buffer is not in HIPO format
     */
    public byte[] getPrevEvent() throws HipoException {
        boolean debug = false;

        // If the last method called was getNext, not getPrev,
        // we don't want to get the same event twice in a row, so
        // decrement index. Take into account if this is the
        // first time getNextEvent or getPrevEvent called.
        if (sequentialIndex < 0) {
            if (debug) System.out.println("getPrevEvent first time index = " + sequentialIndex);
        }
        // else if last call was to getNextEvent ...
        else if (lastCalledSeqNext) {
            sequentialIndex--;
            if (debug) System.out.println("getPrevEvent extra decrement to " + sequentialIndex);
        }

        byte[] array = getEvent(--sequentialIndex);
        lastCalledSeqNext = false;

        if (array == null) {
            if (debug) System.out.println("getPrevEvent hit limit at index " + sequentialIndex +
                                          ", set to " + (sequentialIndex + 1) + "\n");
            sequentialIndex++;
        }
        else {
            if (debug) System.out.println("getPrevEvent got event " + (sequentialIndex) + "\n");
        }

        return array;
    }

    /**
     * Get an EvioNode representing the next event from the buffer while sequentially reading.
     * Calling this and calling {@link #getNextEvent()} have the same effect in terms of
     * advancing the same internal counter.
     * If the previous call was to {@link #getPrevEvent}, this will get the event
     * past what that returned. Once the last event is returned, this will return null.
     * 
     * @return EvioNode representing the next event or null if no more events,
     *         reading a file or data is compressed.
     */
    public EvioNode getNextEventNode() {
        if (sequentialIndex >= eventIndex.getMaxEvents() || fromFile || compressed) {
            return null;
        }

        if (sequentialIndex < 0) {
            sequentialIndex = 0;
        }
        // else if last call was to getPrevEvent ...
        else if (!lastCalledSeqNext) {
            sequentialIndex++;
        }

        lastCalledSeqNext = true;
        return eventNodes.get(sequentialIndex++);
    }

    /**
     * Reads user header of the file header/first record header of buffer.
     * The returned ByteBuffer also contains endianness of the file/buffer.
     * @return ByteBuffer containing the user header of the file/buffer.
     * @throws IOException if error reading file
     */
    public ByteBuffer readUserHeader() throws IOException {
        byte[] userBytes;

        if (fromFile) {
            int userLen = fileHeader.getUserHeaderLength();
            //System.out.println("  " + fileHeader.getUserHeaderLength()
            //                           + "  " + fileHeader.getHeaderLength() + "  " + fileHeader.getIndexLength());
            userBytes = new byte[userLen];

            inStreamRandom.getChannel().position(fileHeader.getHeaderLength() + fileHeader.getIndexLength());
            inStreamRandom.read(userBytes);
            return ByteBuffer.wrap(userBytes).order(fileHeader.getByteOrder());
        }
        else {
            int userLen = firstRecordHeader.getUserHeaderLength();
            //System.out.println("  " + firstRecordHeader.getUserHeaderLength()
            //                           + "  " + firstRecordHeader.getHeaderLength() + "  " + firstRecordHeader.getIndexLength());
            userBytes = new byte[userLen];

            buffer.position(firstRecordHeader.getHeaderLength() + firstRecordHeader.getIndexLength());
            buffer.get(userBytes, 0, userLen);
            return ByteBuffer.wrap(userBytes).order(firstRecordHeader.getByteOrder());
        }
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
            //System.out.println("[READER] getEvent: index = " + index + ", max events = " +
            //               eventIndex.getMaxEvents());
            return null;
        }

        if (eventIndex.setEvent(index)) {
            // If here, the event is in the next record
//System.out.println("[READER] getEvent: read record at index =" + eventIndex.getRecordNumber());
            readRecord(eventIndex.getRecordNumber());
        }

        if (inputRecordStream.getEntries() == 0) {
//System.out.println("[READER] getEvent: first time reading record at index = " + eventIndex.getRecordNumber());
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
     *                       (buf.capacity() &lt; event size).
     */
    public ByteBuffer getEvent(ByteBuffer buf, int index) throws HipoException {

        if (index < 0 || index >= eventIndex.getMaxEvents()) {
            return null;
        }

        if (eventIndex.setEvent(index)) {
            // If here, the event is in the next record
            readRecord(eventIndex.getRecordNumber());
        }
        if (inputRecordStream.getEntries() == 0) {
            //System.out.println("[READER] first time reading buffer");
            readRecord(eventIndex.getRecordNumber());
        }
        return inputRecordStream.getEvent(buf, eventIndex.getRecordEventNumber());
    }

    /**
     * Get an EvioNode representing the specified event from the buffer.
     * If index is out of bounds, null is returned.
     * @param index index of specified event within the entire buffer,
     *              starting at 0.
     * @return EvioNode representing the specified event or null if
     *         index is out of bounds, reading a file or data is compressed.
     */
    public EvioNode getEventNode(int index) {
//System.out.println("getEventNode: index = " + index + " >? " + eventIndex.getMaxEvents() +
//                   ", fromFile = " + fromFile + ", compressed = " + compressed);
        if (index < 0 || index >= eventIndex.getMaxEvents() || fromFile) {
//System.out.println("getEventNode: index out of range, from file or compressed so node = NULL");
            return null;
        }
//System.out.println("getEventNode: Getting node at index = " + index);
        return eventNodes.get(index);
    }

    /**
     * Checks if the file has an event to read next.
     * @return true if the next event is available, false otherwise
     */
    public boolean hasNext() {return eventIndex.canAdvance();}

    /**
     * Checks if the stream has previous event to be accessed through, getPrevEvent()
     * @return true if previous event is accessible, false otherwise
     */
    public boolean hasPrev() {return eventIndex.canRetreat();}

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
    protected void extractDictionaryAndFirstEvent() {
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
            buffer.position(bufferOffset +
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
//        System.out.println("extractDictionaryFromFile: IN, hasFirst = " + fileHeader.hasFirstEvent());
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
//            System.out.println("extractDictionaryFromFile: Read");
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
    
    /**
     * Reads data from a record header in order to determine things
     * like the bitInfo word, various lengths, etc.
     * Does <b>not</b> change the position or limit of buffer.
     *
     * @param  buf     buffer containing evio header.
     * @param  offset  byte offset into buffer.
     * @param  info    array in which to store header info. Elements are:
     *  <ol start="0">
     *      <li>bit info word</li>
     *      <li>record length in bytes (inclusive)</li>
     *      <li>compression type</li>
     *      <li>header length in bytes</li>
     *      <li>index array length in bytes (not padded) </li>
     *      <li>user header length in bytes</li>
     *      <li>uncompressed data length in bytes (w/o record header)</li>
     *  </ol>
     * @throws BufferUnderflowException if not enough data in buffer.
     * @throws HipoException null arg(s), negative offset, or info.length &lt; 7.
     */
    static public void findRecordInfo(ByteBuffer buf, int offset, int[] info)
            throws HipoException, BufferUnderflowException {

        if (buf == null || offset < 0 || info == null || info.length < 7) {
            throw new HipoException("bad arg or info.length < 7");
        }
//        if (buf.capacity() - offset < 1000) {
//            System.out.println("findRecInfo: buf cap = " + buf.capacity() + ", offset = " + offset +
//                                       ", lim = " + buf.limit());
//        }
        // Have enough bytes to read 10 words of header?
        if ((buf.capacity() - offset) < 40) {
System.out.println("findRecInfo: buf cap = " + buf.capacity() + ", offset = " + offset +
        ", lim = " + buf.limit());
            throw new BufferUnderflowException();
        }
        
        info[0] = buf.getInt(offset + RecordHeader.BIT_INFO_OFFSET);
        info[1] = buf.getInt(offset + RecordHeader.RECORD_LENGTH_OFFSET) * 4;
        info[2] = buf.getInt(offset + RecordHeader.COMPRESSION_TYPE_OFFSET) >>> 28;
        info[3] = buf.getInt(offset + RecordHeader.HEADER_LENGTH_OFFSET) * 4;
        info[4] = buf.getInt(offset + RecordHeader.INDEX_ARRAY_OFFSET);
        info[5] = buf.getInt(offset + RecordHeader.USER_LENGTH_OFFSET);
        info[6] = buf.getInt(offset + RecordHeader.UNCOMPRESSED_LENGTH_OFFSET);
    }

    /**
     * This method gets the total number of evio/hipo format bytes in
     * the given buffer, both compressed and uncompressed. Results are
     * stored in the given int array. First element is compressed length,
     * second is uncompressed length.
     *
     * @param buf   ByteBuffer containing evio/hipo data
     * @param info  integer array containing evio/hipo data. Elements are:
     *  <ol start="0">
     *      <li>compressed length in bytes (padded) </li>
     *      <li>uncompressed length in bytes (padded) </li>
      *  </ol>
     * @return total uncompressed hipo/evio data in bytes (padded).
     * @throws BufferUnderflowException if not enough data in buffer.
     * @throws HipoException null arg(s), negative offset, or info.length &lt; 7.
     */
    static private int getTotalByteCounts(ByteBuffer buf, int[] info)
            throws HipoException, BufferUnderflowException {

        if (buf == null || info == null || info.length < 7) {
            throw new HipoException("bad arg or info.length < 7");
        }

        int offset = buf.position();
        int totalCompressed = 0;
        int totalBytes = 0;

        while (true) {
            // Look at the record
            findRecordInfo(buf, offset, info);

            // Total uncompressed length of record
            totalBytes += info[3] + info[4] +
                          4*Utilities.getWords(info[5]) + // user array + padding
                          4*Utilities.getWords(info[6]);  // uncompressed data + padding

            // Track total uncompressed & compressed sizes
            totalCompressed += info[1];

            // Hop over record
            offset += info[1];

            // Quit after last record
            if (RecordHeader.isLastRecord(info[0])) break;
        }

        // No longer input, we now use array for output
        info[0] = totalCompressed;
        info[1] = totalBytes;

        return totalBytes;
    }


    /**
     * This method scans a buffer to find all records and store their position, length,
     * and event count. It also finds all events and then creates and stores their associated
     * EvioNode objects.<p>
     *
     * The difficulty with doing this is that the buffer may contain compressed data.
     * It must then be uncompressed into a different buffer. There are 2 possibilities.
     * First, if the buffer being parsed is too small to hold its uncompressed form,
     * then a new, larger buffer is created, filled with the uncompressed data and then
     * given as the return value of this method. Second, if the buffer being parsed is
     * large enough to hold its uncompressed form, the data is uncompressed into a
     * temporary holding buffer. When all decompresssion and parsing is finished, the
     * contents of the temporary buffer are copied back into the original buffer which
     * then becomes the return value.
     *
     * @return buffer containing uncompressed data. This buffer may be different than the
     *         one originally scanned if the data was compressed and the uncompressed length
     *         is greater than the original buffer could hold.
     * @throws HipoException if buffer not in the proper format or earlier than version 6;
     *                       if checkRecordNumberSequence is true and records are out of sequence.
     * @throws BufferUnderflowException if not enough data in buffer.
     */
    public ByteBuffer scanBuffer() throws HipoException, BufferUnderflowException {

        if (!RecordHeader.isCompressed(buffer, bufferOffset)) {
            // Since data is not compressed ...
            scanUncompressedBuffer();
            return buffer;
        }

        compressed = true;

        // The previous method call will set the endianness of the buffer properly.
        // Hop through ALL RECORDS to find their total lengths. This does NOT
        // change pos/limit of buffer.
        int totalUncompressedBytes = getTotalByteCounts(buffer, headerInfo);

//System.out.println("scanBuffer: total Uncomp bytes = " + totalUncompressedBytes +
//                " >? cap - off = " + (buffer.capacity() - bufferOffset));

        // Create buffer big enough to hold everything
        ByteBuffer bigEnoughBuf = ByteBuffer.allocate(totalUncompressedBytes + bufferOffset + 1024);
        bigEnoughBuf.order(buffer.order()).position(bufferOffset);
        // Copy over everything up to the current offset
        System.arraycopy(buffer.array(), buffer.arrayOffset(),
                         bigEnoughBuf.array(), 0, bufferOffset);

        boolean isArrayBacked = (bigEnoughBuf.hasArray() && buffer.hasArray());
        boolean haveFirstRecordHeader = false;

        recordHeader.setHeaderType(HeaderType.EVIO_RECORD);
        // Start at the buffer's initial position
        int position  = bufferOffset;
        int recordPos = bufferOffset;
        int bytesLeft = totalUncompressedBytes;

//System.out.println("  scanBuffer: buffer pos = " + bufferOffset + ", bytesLeft = " + bytesLeft);
        // Keep track of the # of records, events, and valid words in file/buffer.
        // eventPlace is the place of each event (evio or not) with repect to each other (0, 1, 2 ...)
        int eventPlace = 0, byteLen;
        eventNodes.clear();
        recordPositions.clear();
        eventIndex.clear();
        // TODO: this should NOT change in records in 1 buffer, only BETWEEN buffers!!!!!!!!!!!!
        recordNumberExpected = 1;

        // Try to allocate this only once
        int[] eventLengths = new int[2000];

        // Go through data record-by-record
        do {
            // If this is not the first record anymore, then the limit of bigEnoughBuf,
            // set above, may not be big enough.

            // Uncompress record in buffer and place into bigEnoughBuf
            int origRecordBytes = inputRecordStream.uncompressRecord(
                    buffer, recordPos, bigEnoughBuf,
                    isArrayBacked, recordHeader);

            // The only certainty at this point about pos/limit is that
            // bigEnoughBuf.position = after header/index/user, just before data.
            // What exactly the decompression methods do is unknown.

            // recordHeader is now describing the uncompressed buffer, bigEnoughBuf
            int eventCount = recordHeader.getEntries();
            int recordHeaderLen = recordHeader.getHeaderLength();
            int recordBytes = recordHeader.getLength();

            // uncompressRecord(), above, read the header. Save the first header.
            if (!haveFirstRecordHeader) {
                // First time through, save byte order and version
                byteOrder = recordHeader.getByteOrder();
                buffer.order(byteOrder);
                bigEnoughBuf.order(byteOrder);
                evioVersion = recordHeader.getVersion();
                firstRecordHeader.copy(recordHeader);
                haveFirstRecordHeader = true;
            }

            //System.out.println("read header ->\n" + recordHeader);

            if (checkRecordNumberSequence) {
                if (recordHeader.getRecordNumber() != recordNumberExpected) {
                    //System.out.println("  scanBuffer: record # out of sequence, got " + recordHeader.getRecordNumber() +
                    //                   " expecting " + recordNumberExpected);
                    throw new HipoException("bad record # sequence");
                }
                recordNumberExpected++;
            }

            // Check to see if the whole record is there
            if (recordHeader.getLength() > bytesLeft) {
                System.out.println("    record size = " + recordHeader.getLength() + " >? bytesLeft = " + bytesLeft +
                        ", pos = " + buffer.position());
                throw new HipoException("Bad evio format: not enough data to read record");
            }

            // Header is now describing the uncompressed buffer, bigEnoughBuf
            RecordPosition rp = new RecordPosition(position, recordBytes, eventCount);
            recordPositions.add(rp);
            // Track # of events in this record for event index handling
            eventIndex.addEventSize(eventCount);

            // Find & store the index of event sizes (words)
            if (eventCount > 0) {
                // allocate more memory if we need it
                if (eventCount > eventLengths.length) {
                    eventLengths = new int[2*eventCount];
                }

                // Place in buffer to start reading event lengths (
                int lenIndex = position + recordHeaderLen;

                for (int i=0; i < eventCount; i++) {
                    eventLengths[i] = bigEnoughBuf.getInt(lenIndex);
                    lenIndex += 4;
                }
            }

            // How many bytes left in the newly expanded buffer
            bytesLeft -= recordHeader.getUncompressedRecordLength();

            // After calling uncompressRecord(), bigEnoughBuf will be positioned
            // right before where the events start.
            position = bigEnoughBuf.position();

            //-----------------------------------------------------
            // For each event in record, store its location.
            // THIS ONLY WORKS AND IS ONLY NEEDED FOR EVIO DATA!
            // So the question is, how do we know when we've got
            // evio data and when we don't?
            //-----------------------------------------------------
            for (int i=0; i < eventCount; i++) {
                // Is the length we get from the first word of an evio bank/event (bytes)
                // the same as the length we got from the record header? If not, it's not evio.
                boolean isEvio = 4*(bigEnoughBuf.getInt(position) + 1) == eventLengths[i];

                if (isEvio) {
                    try {
                        // If the event is in evio format, parse it a bit
                        EvioNode node = EvioNode.extractEventNode(bigEnoughBuf, nodePool, recordPos,
                                                                  position, eventPlace + i);
                        byteLen = node.getTotalBytes();
                        eventNodes.add(node);
//System.out.println("\n      scanUncompressedBuffer: extracted node : " + node.toString());
//System.out.println("      event (evio)" + i + ", pos = " + node.getPosition() +
//                   ", dataPos = " + node.getDataPosition() + ", ev # = " + (eventPlace + i + 1) +
//                   ", bytes = " + byteLen);
                    }
                    catch (EvioException e) {
                        // If we're here, the event is not in evio format

                        // The problem with doing things in the following way
                        // (throwing an exception if the event is NOT is evio format)
                        // is that the exception throwing mechanism is not a good way to
                        // handle normal logic flow. But not sure what else can be done.
                        // This should only happen very, very seldom.

                        byteLen = eventLengths[i];
                        evioFormat = false;
//System.out.println("      event (binary, exception)" + i + ", bytes = " + byteLen);
                    }
                }
                else {
                    // If we're here, the event is not in evio format, so just use the length we got
                    // previously from the record index.
                    byteLen = eventLengths[i];
                    evioFormat = false;
//System.out.println( "      event (binary, regular logic)" + i + ", bytes = " + byteLen);
                }

                // Hop over event
                position += byteLen;

                if (byteLen < 8) {
                    throw new HipoException("Bad evio format: bad bank length");
                }
//System.out.println("        hopped event " + i + ", bytesLeft = " + bytesLeft + ", pos = " + position + "\n");
            }

            bigEnoughBuf.position(position);
            eventPlace += eventCount;

            // Next record position
            recordPos += origRecordBytes;

            // If the evio buffer was written with the DAQ Java evio library,
            // there is only 1 record with event per buffer -- the first record.
            // It's followed by a trailer.

            // Read the next record if this is not the last one and there's enough data to
            // read a header.

        } while (!recordHeader.isLastRecord() && bytesLeft >= RecordHeader.HEADER_SIZE_BYTES);

        bigEnoughBuf.flip();
        return bigEnoughBuf;
    }


    /**
     * Scan buffer to find all records and store their position, length, and event count.
     * Also finds all events and then creates and stores their associated EvioNode objects.
     * Be warned that this method generates garbage.
     * @throws HipoException if buffer too small, not in the proper format, or earlier than version 6;
     *                       if checkRecordNumberSequence is true and records are out of sequence.
     */
    public void scanUncompressedBuffer() throws HipoException {

        boolean haveFirstRecordHeader = false;

        // Start at the buffer's initial position
        int position  = bufferOffset;
        int recordPos = bufferOffset;
        int bytesLeft = bufferLimit - bufferOffset;

//System.out.println("  scanUncompressedBuffer: buffer pos = " + bufferOffset + ", bytesLeft = " + bytesLeft);
        // Keep track of the # of records, events, and valid words in file/buffer
        int eventPlace = 0;
        eventNodes.clear();
        recordPositions.clear();
        eventIndex.clear();
// TODO: this should NOT change in records in 1 buffer, only BETWEEN buffers!!!!!!!!!!!!
        recordNumberExpected = 1;

        while (bytesLeft >= RecordHeader.HEADER_SIZE_BYTES) {
            // Read record header
            buffer.position(position);
            // This moves the buffer's position
            buffer.get(headerBytes, 0, RecordHeader.HEADER_SIZE_BYTES);
            // Only sets the byte order of headerBuffer
            recordHeader.readHeader(headerBuffer);
            int eventCount = recordHeader.getEntries();
            int recordHeaderLen = recordHeader.getHeaderLength();
            int recordBytes = recordHeader.getLength();

            //System.out.println("read header ->\n" + recordHeader);

            // Save the first record header
            if (!haveFirstRecordHeader) {
                // First time through, save byte order and version
                byteOrder = recordHeader.getByteOrder();
                buffer.order(byteOrder);
                evioVersion = recordHeader.getVersion();
                firstRecordHeader.copy(recordHeader);
                compressed = recordHeader.getCompressionType() != CompressionType.RECORD_UNCOMPRESSED;
                haveFirstRecordHeader = true;
            }

            if (checkRecordNumberSequence) {
                if (recordHeader.getRecordNumber() != recordNumberExpected) {
                    //System.out.println("  scanBuffer: record # out of sequence, got " + recordHeader.getRecordNumber() +
                    //                   " expecting " + recordNumberExpected);
                    throw new HipoException("bad record # sequence");
                }
                recordNumberExpected++;
            }

            // Check to see if the whole record is there
            if (recordHeader.getLength() > bytesLeft) {
                System.out.println("    record size = " + recordHeader.getLength() + " >? bytesLeft = " + bytesLeft +
                        ", pos = " + buffer.position());
                throw new HipoException("Bad hipo format: not enough data to read record");
            }

            //System.out.println(">>>>>==============================================");
            //System.out.println(recordHeader.toString());

            RecordPosition rp = new RecordPosition(position, recordBytes, eventCount);
            //System.out.println(" RECORD HEADER ENTRIES = " + eventsInRecord);
            recordPositions.add(rp);
            // Track # of events in this record for event index handling
            eventIndex.addEventSize(eventCount);

            int eventLength;
            // Place in buffer to start reading event lengths
            int lenIndex = position + recordHeaderLen;

            // Hop over record header, user header, and index to events
            int byteLen = recordHeader.getHeaderLength() +
                    4*recordHeader.getUserHeaderLengthWords() + // takes account of padding
                    recordHeader.getIndexLength();
            position  += byteLen;
            bytesLeft -= byteLen;

            // Do this because extractEventNode uses the buffer position
            buffer.position(position);
            //System.out.println("    hopped to data, pos = " + position);


            // For each event in record, store its location
            for (int i=0; i < eventCount; i++) {
                eventLength = buffer.getInt(lenIndex);
                lenIndex += 4;

                // Is the length we get from the first word of an evio bank/event (bytes)
                // the same as the length we got from the record header? If not, it's not evio.
                boolean isEvio = 4*(buffer.getInt(position) + 1) == eventLength;
                //EvioNode node;


                if (isEvio) {
                    try {
                        // If the event is in evio format, parse it a bit
                        EvioNode node = EvioNode.extractEventNode(buffer, nodePool, recordPos,
                                position, eventPlace + i);
                        byteLen = node.getTotalBytes();
                        eventNodes.add(node);
//System.out.println("\n      scanUncompressedBuffer: extracted node : " + node.toString());
//System.out.println("      event (evio)" + i + ", pos = " + node.getPosition() +
//                   ", dataPos = " + node.getDataPosition() + ", ev # = " + (eventPlace + i + 1) +
//                   ", bytes = " + byteLen);
                    }
                    catch (EvioException e) {
                        // If we're here, the event is not in evio format

                        // The problem with doing things in the following way
                        // (throwing an exception if the event is NOT is evio format)
                        // is that the exception throwing mechanism is not a good way to
                        // handle normal logic flow. But not sure what else can be done.
                        // This should only happen very, very seldom.

                        byteLen = eventLength;
                        evioFormat = false;
//System.out.println("      event (binary, exception)" + i + ", bytes = " + byteLen);
                    }
                }
                else {
                    // If we're here, the event is not in evio format, so just use the length we got
                    // previously from the record index.
                    byteLen = eventLength;
                    evioFormat = false;
//System.out.println( "      event (binary, regular logic)" + i + ", bytes = " + byteLen);
                }


                // Hop over event
                position  += byteLen;
                bytesLeft -= byteLen;

                if (byteLen < 8 || bytesLeft < 0) {
                    throw new HipoException("Bad evio format: bad bank length");
                }

                //System.out.println("        hopped event " + i + ", bytesLeft = " + bytesLeft + ", pos = " + position + "\n");
            }

            eventPlace += eventCount;
        }

        buffer.position(bufferOffset);
    }


    /**
     * Scan file to find all records and store their position, length, and event count.
     * Safe to call this method successively.
     * @throws IOException   if error reading file
     * @throws HipoException if file is not in the proper format or earlier than version 6;
     *                       if checkRecordNumberSequence is true and records are out of sequence.
     */
    public void forceScanFile() throws IOException, HipoException {
        
        FileChannel channel;

        // Read and parse file header if we have't already done so in scanFile()
        if (fileHeader == null) {
            fileHeader = new FileHeader();
            // Go to file beginning
            channel = inStreamRandom.getChannel().position(0L);
            inStreamRandom.read(headerBytes);
            fileHeader.readHeader(headerBuffer);
            byteOrder = fileHeader.getByteOrder();
            evioVersion = fileHeader.getVersion();
//System.out.println("forceScanFile: file header -->" + "\n" + fileHeader);
        }
        else {
//System.out.println("forceScanFile: already read file header, so set position to read");
            // If we've already read in the header, position file to read what follows
            channel = inStreamRandom.getChannel().position(RecordHeader.HEADER_SIZE_BYTES);
        }

        int recordLen;
        eventIndex.clear();
        recordPositions.clear();
        recordNumberExpected = 1;
        boolean haveFirstRecordHeader = false;

        // Scan file by reading each record header and
        // storing its position, length, and event count.
        long fileSize = inStreamRandom.length();

        // Don't go beyond 1 header length before EOF since we'll be reading in 1 header
        long maximumSize = fileSize - RecordHeader.HEADER_SIZE_BYTES;

        // First record position (past file's header + index + user header)
        long recordPosition = fileHeader.getHeaderLength() +
                              fileHeader.getUserHeaderLength() +
                              fileHeader.getIndexLength() +
                              fileHeader.getUserHeaderLengthPadding();

//System.out.println("forceScanFile: record 1 pos = 0");
        int recordCount = 0;
        while (recordPosition < maximumSize) {
            channel.position(recordPosition);
//System.out.println("forceScanFile: record " + recordCount +  " @ pos = " + recordPosition +
//                   ", maxSize = " + maximumSize);
            inStreamRandom.read(headerBytes);
            recordHeader.readHeader(headerBuffer);
//System.out.println("forceScanFile: record header " + recordCount + " -->" + "\n" + recordHeader);
            recordCount++;


            // Checking record # sequence does NOT make sense when reading a file.
            // It only makes sense when reading from a stream and checking to see
            // if the record id, set by the sender, is sequential.
            // So feature turned off if reading from file.
            if (checkRecordNumberSequence) {
                if (recordHeader.getRecordNumber() != recordNumberExpected) {
System.out.println("forceScanFile: record # out of sequence, got " + recordHeader.getRecordNumber() +
                   " expecting " + recordNumberExpected);

                    throw new HipoException("bad record # sequence");
                }
                recordNumberExpected++;
            }

            // Save the first record header
            if (!haveFirstRecordHeader) {
                firstRecordHeader.copy(recordHeader);
                compressed = firstRecordHeader.getCompressionType().isCompressed();
                haveFirstRecordHeader = true;
            }

            recordLen = recordHeader.getLength();
            RecordPosition pos = new RecordPosition(recordPosition, recordLen,
                                                    recordHeader.getEntries());
            recordPositions.add(pos);
            // Track # of events in this record for event index handling
            eventIndex.addEventSize(recordHeader.getEntries());
            recordPosition += recordLen;
        }
//eventIndex.show();
//System.out.println("NUMBER OF RECORDS " + recordPositions.size());
    }

    /**
     * Scans the file to index all the record positions.
     * It takes advantage of any existing indexes in file.
     * @param force if true, force a file scan even if header
     *              or trailer have index info.
     * @throws IOException   if error reading file
     * @throws HipoException if file is not in the proper format or earlier than version 6
     */
    public void scanFile(boolean force) throws IOException, HipoException {

        // For garbage-free parsing
        headerBytes  = new byte[RecordHeader.HEADER_SIZE_BYTES];
        headerBuffer = ByteBuffer.wrap(headerBytes);
        recordHeader = new RecordHeader();
        firstRecordHeader = new RecordHeader();

        if (force) {
            forceScanFile();
            return;
        }

        eventIndex.clear();
        recordPositions.clear();
//        recordNumberExpected = 1;

        //System.out.println("[READER] ---> scanning the file");

        fileHeader = new FileHeader();

        // Go to file beginning
        FileChannel channel = inStreamRandom.getChannel().position(0L);

        // Read and parse file header
        inStreamRandom.read(headerBytes);
        fileHeader.readHeader(headerBuffer);
        byteOrder = fileHeader.getByteOrder();
        evioVersion = fileHeader.getVersion();
//System.out.println("scanFile: file header: \n" + fileHeader.toString());

        // Is there an existing record length index?
        // Index in trailer gets first priority.
        // Index in file header gets next priority.
        boolean fileHasIndex = fileHeader.hasTrailerWithIndex() || (fileHeader.hasIndex());
//System.out.println(" file has index = " + fileHasIndex
//                    + ", has trailer w/ index = " + fileHeader.hasTrailerWithIndex()
//                    + ", file hdr has index = " + fileHeader.hasIndex());

        // If there is no index, scan file
        if (!fileHasIndex) {
            forceScanFile();
            return;
        }

        // If we're using the trailer, make sure it's position is valid,
        // (ie 0 is NOT valid).
        boolean useTrailer = fileHeader.hasTrailerWithIndex();
        if (useTrailer) {
            // If trailer position is NOT valid ...
            if (fileHeader.getTrailerPosition() < 1) {
System.out.println("scanFile: bad trailer position, " + fileHeader.getTrailerPosition());
                if (fileHeader.hasIndex()) {
                    // Use file header index if there is one
                    useTrailer = false;
                }
                else {
                    // Scan if no viable index exists
                    forceScanFile();
                    return;
                }
            }
        }

        // First record position (past file's header + index + user header)
        long recordPosition = fileHeader.getLength();
//System.out.println("scanFile: first record position = " + recordPosition);

        // Move to first record and save the header
        channel.position(recordPosition);
        inStreamRandom.read(headerBytes);
        firstRecordHeader.copy(recordHeader);
        firstRecordHeader.readHeader(headerBuffer);
        compressed = firstRecordHeader.getCompressionType().isCompressed();

        int indexLength;

        // If we have a trailer with indexes ...
        if (useTrailer) {
            // Position read right before trailing header
            channel.position(fileHeader.getTrailerPosition());
//System.out.println("scanFile: position file to trailer = " + fileHeader.getTrailerPosition());
            // Read trailer
            inStreamRandom.read(headerBytes);
            recordHeader.readHeader(headerBuffer);
            indexLength = recordHeader.getIndexLength();
        }
        else {
            // Move back to immediately past file header
            // while taking care of non-standard size
            channel.position(fileHeader.getHeaderLength());
            // Index immediately follows file header in this case
            indexLength = fileHeader.getIndexLength();
        }

        // Read indexes
        byte[] index = new byte[indexLength];
        inStreamRandom.read(index);
        int len, count;
        try {
            // Turn bytes into record lengths & event counts
//System.out.println("scanFile: transform int array from " + fileHeader.getByteOrder());
            int[] intData = ByteDataTransformer.toIntArray(index, fileHeader.getByteOrder());
            // Turn record lengths into file positions and store in list
            recordPositions.clear();
            for (int i=0; i < intData.length; i += 2) {
                len = intData[i];
                count = intData[i+1];
//System.out.println("scanFile: record pos = " + recordPosition + ", len = " + len + ", count = " + count);
                RecordPosition pos = new RecordPosition(recordPosition, len, count);
                recordPositions.add(pos);
                // Track # of events in this record for event index handling
//System.out.println("scanFile: add record's event count (" + count + ") to eventIndex");
//Utilities.printStackTrace();
                eventIndex.addEventSize(count);
                recordPosition += len;
//System.out.println("scanFile: next record position = " + recordPosition);
            }
        }
        catch (EvioException e) {/* never happen */}
    }



    /**
     * This method removes the data, represented by the given node, from the buffer.
     * It also marks all nodes taken from that buffer as obsolete.
     * They must not be used anymore.<p>
     *
     * @param  removeNode  evio structure to remove from buffer
     * @return ByteBuffer updated to reflect the node removal
     * @throws HipoException if object closed;
     *                       if node was not found in any event;
     *                       if internal programming error;
     *                       if buffer has compressed data;
     */
    public  ByteBuffer removeStructure(EvioNode removeNode) throws HipoException {

        // If we're removing nothing, then DO nothing
        if (removeNode == null) {
            return buffer;
        }

        if (closed) {
            throw new HipoException("object closed");
        }
        else if (removeNode.isObsolete()) {
            //System.out.println("removeStructure: node has already been removed");
            return buffer;
        }

        if (firstRecordHeader.getCompressionType().isCompressed()) {
            throw new HipoException("cannot remove node from buffer of compressed data");
        }

        boolean foundNode = false;

        // Locate the node to be removed ...
        outer:
        for (EvioNode ev : eventNodes) {
            // See if it's an event ...
            if (removeNode == ev) {
                foundNode = true;
                break;
            }

            for (EvioNode n : ev.getAllNodes()) {
                // The first node in allNodes is the event node,
                if (removeNode == n) {
                    foundNode = true;
                    break outer;
                }
            }
        }

        if (!foundNode) {
            throw new HipoException("removeNode not found in any event");
        }

        // The data these nodes represent will be removed from the buffer,
        // so the node will be obsolete along with all its descendants.
        removeNode.setObsolete(true);

        //---------------------------------------------------
        // Remove structure. Keep using current buffer.
        // We'll move all data that came after removed node
        // to where removed node used to be.
        //---------------------------------------------------

        // Amount of data being removed
        int removeDataLen = removeNode.getTotalBytes();

        // Just after removed node (start pos of data being moved)
        int startPos = removeNode.getPosition() + removeDataLen;

        // Can't use duplicate(), must copy the backing buffer
        ByteBuffer moveBuffer = ByteBuffer.allocate(bufferLimit-startPos).order(buffer.order());
        int bufferLim = buffer.limit();
        buffer.limit(bufferLimit).position(startPos);
        moveBuffer.put(buffer);
        buffer.limit(bufferLim);

        // Prepare to move data currently sitting past the removed node
        moveBuffer.clear();

        // Set place to put the data being moved - where removed node starts
        buffer.position(removeNode.getPosition());
        // Copy it over
        buffer.put(moveBuffer);

        // Reset some buffer values
        buffer.position(bufferOffset);
        bufferLimit -= removeDataLen;
        buffer.limit(bufferLimit);

        // Reduce lengths of parent nodes
        EvioNode parent = removeNode.getParentNode();
        parent.updateLengths(-removeDataLen);

        // Reduce containing record's length
        int pos = removeNode.getRecordPosition();
        // Header length in words
        int oldLen = 4*buffer.getInt(pos);
        buffer.putInt(pos, (oldLen - removeDataLen)/4);
        // Uncompressed data length in bytes
        oldLen = buffer.getInt(pos + RecordHeader.UNCOMPRESSED_LENGTH_OFFSET);
        buffer.putInt(pos + RecordHeader.UNCOMPRESSED_LENGTH_OFFSET, oldLen - removeDataLen);

        // Invalidate all nodes obtained from the last buffer scan
        for (EvioNode ev : eventNodes) {
            ev.setObsolete(true);
        }

        // Now the evio data in buffer is in a valid state so rescan buffer to update everything
        scanBuffer();

        return buffer;
    }


    /**
     * This method adds an evio container (bank, segment, or tag segment) as the last
     * structure contained in an event. It is the responsibility of the caller to make
     * sure that the buffer argument contains valid evio data (only data representing
     * the structure to be added - not in file format with record header and the like)
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
     *                  i.e. no record headers)
     * @return a new ByteBuffer object which is created and filled with all the data
     *         including what was just added.
     * @throws HipoException if eventNumber out of bounds;
     *                       if addBuffer is null;
     *                       if addBuffer arg is empty or has non-evio format;
     *                       if addBuffer is opposite endian to current event buffer;
     *                       if added data is not the proper length (i.e. multiple of 4 bytes);
     *                       if the event number does not correspond to an existing event;
     *                       if there is an internal programming error;
     *                       if object closed
     */
    public  ByteBuffer addStructure(int eventNumber, ByteBuffer addBuffer) throws HipoException {

        if (addBuffer == null || addBuffer.remaining() < 8) {
            throw new HipoException("null, empty, or non-evio format buffer arg");
        }

        if (addBuffer.order() != byteOrder) {
            throw new HipoException("trying to add wrong endian buffer");
        }

        if (eventNumber < 1 || eventNumber > eventNodes.size()) {
            throw new HipoException("event number out of bounds");
        }

        if (closed) {
            throw new HipoException("object closed");
        }

        EvioNode eventNode;
        try {
            eventNode = eventNodes.get(eventNumber - 1);
        }
        catch (IndexOutOfBoundsException e) {
            throw new HipoException("event " + eventNumber + " does not exist", e);
        }

        // Position in byteBuffer just past end of event
        int endPos = eventNode.getDataPosition() + 4*eventNode.getDataLength();

        // How many bytes are we adding?
        int appendDataLen = addBuffer.remaining();

        // Make sure it's a multiple of 4
        if (appendDataLen % 4 != 0) {
            throw new HipoException("data added is not in evio format");
        }

        //--------------------------------------------
        // Add new structure to end of specified event
        //--------------------------------------------

        // Create a new buffer
        ByteBuffer newBuffer = ByteBuffer.allocate(bufferLimit - bufferOffset + appendDataLen);
        newBuffer.order(byteOrder);

        // Copy beginning part of existing buffer into new buffer
        buffer.limit(endPos).position(bufferOffset);
        newBuffer.put(buffer);

        // Copy new structure into new buffer
        newBuffer.put(addBuffer);

        // Copy ending part of existing buffer into new buffer
        buffer.limit(bufferLimit).position(endPos);
        newBuffer.put(buffer);

        // Get new buffer ready for reading
        newBuffer.flip();
        buffer = newBuffer;
        bufferOffset = 0;
        bufferLimit  = newBuffer.limit();

        // Increase lengths of parent nodes
        EvioNode addToNode = eventNodes.get(eventNumber);
        EvioNode parent = addToNode.getParentNode();
        parent.updateLengths(appendDataLen);

        // Increase containing record's length
        int pos = addToNode.getRecordPosition();
        // Header length in words
        int oldLen = 4*buffer.getInt(pos);
        buffer.putInt(pos, (oldLen + appendDataLen)/4);
        // Uncompressed data length in bytes
        oldLen = buffer.getInt(pos + RecordHeader.UNCOMPRESSED_LENGTH_OFFSET);
        buffer.putInt(pos + RecordHeader.UNCOMPRESSED_LENGTH_OFFSET, oldLen + appendDataLen);

        // Invalidate all nodes obtained from the last buffer scan
        for (EvioNode ev : eventNodes) {
            ev.setObsolete(true);
        }

        // Now the evio data in buffer is in a valid state so rescan buffer to update everything
        scanBuffer();

        return buffer;
    }


    public void show() {
        System.out.println(" ***** FILE: (info), RECORDS = "
                + recordPositions.size() + " *****");
        for(RecordPosition entry : this.recordPositions){
            System.out.println(entry);
        }
    }
    
    public static void main(String[] args){
        try {
            Reader reader = new Reader("/Users/gavalian/Work/Software/project-3a.0.0/Distribution/clas12-offline-software/coatjava/clas_000810_324.hipo",true);

            int icounter = 0;
            //reader.show();
            while (reader.hasNext()) {
                System.out.println(" reading event # " + icounter);
                try {
                    byte[] event = reader.getNextEvent();
                } catch (HipoException ex) {
                    Logger.getLogger(Reader.class.getName()).log(Level.SEVERE, null, ex);
                }

                icounter++;
            }

            //reader.open("test.evio");
//            reader.readRecord(0);
//            int nevents = reader.getRecordEventCount();
//            System.out.println("-----> events = " + nevents);
//            for(int i = 0; i < 10 ; i++){
//                byte[] event = reader.getEvent(i);
//                System.out.println("---> events length = " + event.length);
//                ByteBuffer buffer = ByteBuffer.wrap(event);
//                buffer.order(ByteOrder.LITTLE_ENDIAN);
//                String data = DataUtils.getStringArray(buffer, 10,30);
//                System.out.println(data);
//            }
        }
        catch (Exception e) {
            e.printStackTrace();
        }
    }
}
