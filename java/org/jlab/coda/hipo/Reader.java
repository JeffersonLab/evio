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
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.util.ArrayList;
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

    /** Buffer being read. */
    protected ByteBuffer buffer;

    /** Initial position of buffer. */
    protected int bufferOffset;

    /** Limit of buffer. */
    protected int bufferLimit;

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

    /** Stores info of all the (top-level) events. */
    protected final ArrayList<EvioNode> eventNodes = new ArrayList<>(1000);

    /** Is this object currently closed? */
    protected boolean closed;

    /** Is this data in file/buffer compressed? */
    protected boolean compressed;

    /** Byte order of file/buffer being read. */
    protected ByteOrder byteOrder;

    /** Keep track of next EvioNode when calling {@link #getNextEventNode()}. */
    protected int sequentialIndex = -1;

    /** If true, the last sequential call was to getNextEvent or getNextEventNode.
     *  If false, the last sequential call was to getPrevEvent. Used to determine
     *  which event is prev or next. */
    protected boolean lastCalledSeqNext;

    /** Evio version of file/buffer being read. */
    protected int evioVersion;


    
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
        open(filename);
        scanFile(false);
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
     * @throws HipoException if buffer too small, not in the proper format, or earlier than version 6
     */
    public Reader(ByteBuffer buffer) throws HipoException {
        this.buffer = buffer;
        bufferOffset = buffer.position();
        bufferLimit  = buffer.limit();
        fromFile = false;
        scanBuffer();
    }

    /**
     * Opens an input stream in binary mode. Scans for
     * records in the file and stores record information
     * in internal array. Each record can be read from the file.
     * @param filename input file name
     * @throws IOException if file not found or error opening file
     */
    public final void open(String filename) throws IOException {
        if (inStreamRandom != null && inStreamRandom.getChannel().isOpen()) {
            try {
                System.out.println("[READER] ---> closing current file : " + inStreamRandom.getFilePointer());
                inStreamRandom.close();
            }
            catch (IOException ex) {}
        }

        System.out.println("[READER] ----> opening current file : " + filename);
        inStreamRandom = new RandomAccessFile(filename,"r");
        System.out.println("[READER] ---> open successful, size : " + inStreamRandom.length());
    }

    /**
     * This is closes the file, but for buffers it only sets the position to 0.
     * This method, along with the <code>rewind()</code> and the two
     * <code>position()</code> methods, allows applications to treat files
     * in a normal random access manner.
     *
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
     * This method can be used to avoid creating additional Reader
     * objects by reusing this one with another buffer.
     * @param buf ByteBuffer to be read
     * @throws HipoException if buf arg is null, buffer too small,
     *                       not in the proper format, or earlier than version 6
     */
    public void setBuffer(ByteBuffer buf) throws HipoException {
        if (buf == null) {
            throw new HipoException("buf arg is null");
        }

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
        firstRecordHeader = null;
        currentRecordLoaded = 0;

        scanBuffer();
        
        closed = false;
    }

    /**
     * Get the file header from reading a file.
     * Will return null if reading a buffer.
     * @return file header from reading a file.
     */
    public FileHeader getFileHeader() {return fileHeader;}

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
     * @return 
     */
    public List<RecordPosition> getRecordPositions(){ return this.recordPositions;}
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
     * previous to what that returned. If this is the called before getNextEvent,
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
        }
        else {
            int userLen = firstRecordHeader.getUserHeaderLength();
            //System.out.println("  " + firstRecordHeader.getUserHeaderLength()
            //                           + "  " + firstRecordHeader.getHeaderLength() + "  " + firstRecordHeader.getIndexLength());
            userBytes = new byte[userLen];

            buffer.position(firstRecordHeader.getHeaderLength() + firstRecordHeader.getIndexLength());
            buffer.get(userBytes, 0, userLen);
        }

        return ByteBuffer.wrap(userBytes);
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
        if (inputRecordStream.getEntries() == 0) {
            System.out.println("[READER] first time reading buffer");
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
        System.out.println("index = " + index + " >? " + eventIndex.getMaxEvents() +
        ", fromFile = " + fromFile + ", compressed = " + compressed);
        if (index < 0 || index >= eventIndex.getMaxEvents() || fromFile || compressed) {
            return null;
        }
        System.out.println("Getting node at index = " + index);
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
        System.out.println("extractDictionaryFromFile: IN, hasFirst = " + fileHeader.hasFirstEvent());
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
            System.out.println("extractDictionaryFromFile: Read");
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
     * Scan buffer to find all records and store their position, length, and event count.
     * Also find all events and create & store their associated EvioNode objects.
     * @throws HipoException if buffer too small, not in the proper format, or earlier than version 6
     */
    public void scanBuffer() throws HipoException {

        byte[] headerBytes = new byte[RecordHeader.HEADER_SIZE_BYTES];
        ByteBuffer headerBuffer = ByteBuffer.wrap(headerBytes);

        RecordHeader recordHeader = new RecordHeader();
        boolean haveFirstRecordHeader = false;

        BufferNode bufferNode = new BufferNode(buffer);

        // Start at the buffer's initial position
        int position  = bufferOffset;
        int bytesLeft = bufferLimit - bufferOffset;

System.out.println("  scanBuffer: buffer pos = " + bufferOffset + ", bytesLeft = " + bytesLeft);
        // Keep track of the # of records, events, and valid words in file/buffer
        int eventCount = 0, byteLen, recordBytes, eventsInRecord;
        recordPositions.clear();

        while (bytesLeft >= RecordHeader.HEADER_SIZE_BYTES) {
            // Read record header
            buffer.position(position);
            // This moves the buffer's position
            buffer.get(headerBytes, 0, RecordHeader.HEADER_SIZE_BYTES);
            recordHeader.readHeader(headerBuffer);
            System.out.println("read header ->\n" + recordHeader);

            // Save the first record header
            if (!haveFirstRecordHeader) {
                // First time through, save byte order and version
                byteOrder = recordHeader.getByteOrder();
                evioVersion = recordHeader.getVersion();
                firstRecordHeader = new RecordHeader(recordHeader);
                compressed = recordHeader.getCompressionType() != 0;
                haveFirstRecordHeader = true;
            }

            // Check to see if the whole record is there
            if (recordHeader.getLength() > bytesLeft) {
System.out.println("    record size = " + recordHeader.getLength() + " >? bytesLeft = " + bytesLeft +
                   ", pos = " + buffer.position());
                throw new HipoException("Bad hipo format: not enough data to read record");
            }

            //System.out.println(">>>>>==============================================");
            //System.out.println(recordHeader.toString());
            recordBytes = recordHeader.getLength();
            eventsInRecord = recordHeader.getEntries();
            RecordPosition rp = new RecordPosition(position, recordBytes, eventsInRecord);
//System.out.println(" RECORD HEADER ENTRIES = " + eventsInRecord);
            recordPositions.add(rp);
            // Track # of events in this record for event index handling
            eventIndex.addEventSize(eventsInRecord);

            // Hop over record header, user header, and index to events
            byteLen = recordHeader.getHeaderLength() +
                      recordHeader.getUserHeaderLength() +
                      recordHeader.getIndexLength();
            position  += byteLen;
            bytesLeft -= byteLen;

            // Do this because extractEventNode uses the buffer position
            buffer.position(position);
System.out.println("    hopped to data, pos = " + position);

            // For each event in record, store its location
            for (int i=0; i < eventsInRecord; i++) {
                EvioNode node;
                try {
                    node = EvioNode.extractEventNode(bufferNode, null, position, eventCount + i);
                }
                catch (EvioException e) {
                    throw new HipoException("Bad evio format: not enough data to read event (bad bank len?)");
                }
System.out.println("      event "+i+" in record: pos = " + node.getPosition() +
                           ", dataPos = " + node.getDataPosition() + ", ev # = " + (eventCount + i + 1));
                eventNodes.add(node);

                // Hop over event
                byteLen    = node.getTotalBytes();
                position  += byteLen;
                bytesLeft -= byteLen;

                if (byteLen < 8 || bytesLeft < 0) {
                    throw new HipoException("Bad evio format: bad bank length");
                }

//System.out.println("        hopped event " + i + ", bytesLeft = " + bytesLeft + ", pos = " + position + "\n");
            }

            eventCount += eventsInRecord;
        }

        buffer.position(bufferOffset);

        //eventIndex.show();
        //System.out.println("NUMBER OF RECORDS " + recordPositions.size());
    }


    /**
     * Scan file to find all records and store their position, length, and event count.
     * @throws IOException   if error reading file
     * @throws HipoException if file is not in the proper format or earlier than version 6
     */
    public void forceScanFile() throws IOException, HipoException {
        
        FileChannel channel;
        byte[] headerBytes = new byte[RecordHeader.HEADER_SIZE_BYTES];
        ByteBuffer headerBuffer = ByteBuffer.wrap(headerBytes);

        // Read and parse file header if we have't already done so in scanFile()
        if (fileHeader == null) {
            fileHeader = new FileHeader();
            channel = inStreamRandom.getChannel().position(0L);
            inStreamRandom.read(headerBytes);
            fileHeader.readHeader(headerBuffer);
            byteOrder = fileHeader.getByteOrder();
            evioVersion = fileHeader.getVersion();
        }
        else {
            // If we've already read in the header, position file to read what follows
            channel = inStreamRandom.getChannel().position(RecordHeader.HEADER_SIZE_BYTES);
        }

        int offset;
        recordPositions.clear();
        RecordHeader recordHeader = new RecordHeader();
        boolean haveFirstRecordHeader = false;

        // Scan file by reading each record header and
        // storing its position, length, and event count.
        long fileSize = inStreamRandom.length();

        // Don't go beyond 1 header length before EOF since we'll be reading in 1 header
        long maximumSize = fileSize - RecordHeader.HEADER_SIZE_BYTES;

        // First record position (past file's header + index + user header)
        long recordPosition = fileHeader.getHeaderLength() +
                              fileHeader.getUserHeaderLength() +
                              fileHeader.getIndexLength();

        while (recordPosition < maximumSize) {
            channel.position(recordPosition);
            inStreamRandom.read(headerBytes);
            recordHeader.readHeader(headerBuffer);
            // Save the first record header
            if (!haveFirstRecordHeader) {
                firstRecordHeader = new RecordHeader(recordHeader);
                compressed = firstRecordHeader.getCompressionType() != 0;
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
    }

    /**
     * Scans the file to index all the record positions.
     * It takes advantage of any existing indexes in file.
     * @param force if true, force a file scan even if header
     *              or trailer have index info.
     * @throws IOException   if error reading file
     * @throws HipoException if file is not in the proper format or earlier than version 6
     */
    protected void scanFile(boolean force) throws IOException, HipoException {

        if (force) {
            forceScanFile();
            return;
        }

        //System.out.println("[READER] ---> scanning the file");
        byte[] headerBytes = new byte[RecordHeader.HEADER_SIZE_BYTES];
        ByteBuffer headerBuffer = ByteBuffer.wrap(headerBytes);

        fileHeader = new FileHeader();
        RecordHeader recordHeader = new RecordHeader();

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
            System.out.println(" file has index = " + fileHasIndex
                    + "  " + fileHeader.hasTrailerWithIndex()
                    + "  " + fileHeader.hasIndex());

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
        //System.out.println("record position = " + recordPosition);

        // Move to first record and save the header
        channel.position(recordPosition);
        inStreamRandom.read(headerBytes);
        firstRecordHeader = new RecordHeader(recordHeader);
        firstRecordHeader.readHeader(headerBuffer);
        compressed = firstRecordHeader.getCompressionType() != 0;

        int indexLength;

        // If we have a trailer with indexes ...
        if (useTrailer) {
            // Position read right before trailing header
            channel.position(fileHeader.getTrailerPosition());
System.out.println("position file to trailer = " + fileHeader.getTrailerPosition());
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
            int[] intData = ByteDataTransformer.toIntArray(index, fileHeader.getByteOrder());
            // Turn record lengths into file positions and store in list
            recordPositions.clear();
            for (int i=0; i < intData.length; i += 2) {
                len = intData[i];
                count = intData[i+1];
                System.out.println("record pos = " + recordPosition + ", len = " + len + ", count = " + count);
                RecordPosition pos = new RecordPosition(recordPosition, len, count);
                recordPositions.add(pos);
                // Track # of events in this record for event index handling
                eventIndex.addEventSize(count);
                recordPosition += len;
            }
        }
        catch (EvioException e) {/* never happen */}
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
        try {
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
        catch (Exception e) {
            e.printStackTrace();
        }
    }
}
