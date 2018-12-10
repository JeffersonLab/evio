package org.jlab.coda.jevio;

import org.jlab.coda.hipo.FileHeader;
import org.jlab.coda.hipo.RecordHeader;

import javax.xml.stream.FactoryConfigurationError;
import javax.xml.stream.XMLOutputFactory;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamWriter;
import java.io.*;
import java.nio.*;
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
 * @author heddle
 * @author timmer
 *
 */
public class EvioReader implements IEvioReader {

    /** Evio version number (1-4, 6). Obtain this by reading first header. */
    private int evioVersion;

    /**
     * Endianness of the data being read, either
     * {@link java.nio.ByteOrder#BIG_ENDIAN} or
     * {@link java.nio.ByteOrder#LITTLE_ENDIAN}.
     */
    private ByteOrder byteOrder;

    /** The buffer being read. */
    private final ByteBuffer byteBuffer;

    /** Initial position of buffer or mappedByteBuffer when reading a file. */
    private final int initialPosition;

    //------------------------
    // Object to delegate to
    //------------------------
    private final IEvioReader reader;




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
    public EvioReader(String path) throws EvioException, IOException {
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
    public EvioReader(String path, boolean checkBlkNumSeq) throws EvioException, IOException {
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
    public EvioReader(File file) throws EvioException, IOException {
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
    public EvioReader(File file, boolean checkBlkNumSeq)
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
    public EvioReader(String path, boolean checkBlkNumSeq, boolean sequential)
            throws EvioException, IOException {
        this(new File(path), checkBlkNumSeq, sequential);
    }


    /**
     * Constructor for reading an event file.
     * Do <b>not</b> set sequential to false for remote files.
     *
     * @param file the file that contains events.
     * @param checkRecNumSeq if <code>true</code> check the block number sequence
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
    public EvioReader(File file, boolean checkRecNumSeq, boolean sequential)
                                        throws EvioException, IOException {
        if (file == null) {
            throw new EvioException("File arg is null");
        }

        initialPosition = 0;

        // Read first 32 bytes of file header
        RandomAccessFile rFile = new RandomAccessFile(file, "r");
        byteBuffer = ByteBuffer.wrap(new byte[32]);
        rFile.read(byteBuffer.array());

        // Parse file header to find the file's endianness & evio version #
        if (findEvioVersion() != ReadStatus.SUCCESS) {
            throw new EvioException("Failed reading first block header");
        }

        // This object is no longer needed
        rFile.close();

        if (evioVersion < 5)  {
            reader = new EvioReaderV4(file, checkRecNumSeq, sequential);
        }
        else if (evioVersion == 6) {
            reader = new EvioReaderV6(file, checkRecNumSeq, sequential);
        }
        else {
            throw new EvioException("unsupported evio version (" + evioVersion + ")");
        }
    }

    //------------------------------------------
    //   BUFFER
    //------------------------------------------

    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     * @see EventWriter
     * @throws EvioException if buffer arg is null
     */
    public EvioReader(ByteBuffer byteBuffer) throws EvioException {
        this(byteBuffer, false, false);
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
    public EvioReader(ByteBuffer byteBuffer, boolean checkBlkNumSeq) throws EvioException {
        this(byteBuffer, checkBlkNumSeq, false);
     }

    /**
     * Constructor for reading a buffer with option of removing synchronization
     * for much greater speed.
     * @param byteBuffer     the buffer that contains events.
     * @param checkRecNumSeq if <code>true</code> check the record number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1
     * @param synced         if true, methods are synchronized for thread safety, else false.
     * @see EventWriter
     * @throws EvioException if buffer arg is null;
     *                       failure to read first block header
     */
    public EvioReader(ByteBuffer byteBuffer, boolean checkRecNumSeq, boolean synced)
            throws EvioException {

        if (byteBuffer == null) {
            throw new EvioException("Buffer arg is null");
        }

        this.byteBuffer = byteBuffer.slice(); // remove necessity to track initial position
        initialPosition = byteBuffer.position();

        // Read first block header and find the file's endianness & evio version #.
        if (findEvioVersion() != ReadStatus.SUCCESS) {
            throw new EvioException("Failed reading first record header");
        }

        if (evioVersion < 4)  {
            throw new EvioException("unsupported evio version (" + evioVersion + "), only 4+");
        }

        if (evioVersion == 4) {
            if (synced) {
                reader = new EvioReaderV4(byteBuffer, checkRecNumSeq);
            }
            else {
                reader = new EvioReaderUnsyncV4(byteBuffer, checkRecNumSeq);
            }
        }
        else if (evioVersion == 6) {
            if (synced) {
                reader = new EvioReaderV6(byteBuffer, checkRecNumSeq);
            }
            else {
                //TODO: need an unsync version!!
                reader = new EvioReaderV6(byteBuffer, checkRecNumSeq);
            }
        }
        else {
            throw new EvioException("unsupported evio version (" + evioVersion + ")");
        }
    }


    /** {@inheritDoc} */
    public void setBuffer(ByteBuffer buf) throws EvioException, IOException {
        reader.setBuffer(buf);
    }

    /** {@inheritDoc} */
    public synchronized boolean isClosed() {return reader.isClosed();}

    /** {@inheritDoc} */
    public boolean checkBlockNumberSequence() {return reader.checkBlockNumberSequence();}

    /** {@inheritDoc} */
    public ByteOrder getByteOrder() {return reader.getByteOrder();}

    /** {@inheritDoc} */
    public int getEvioVersion() {return evioVersion;}

    /** {@inheritDoc} */
     public String getPath() {return reader.getPath();}

    /** {@inheritDoc} */
    public EventParser getParser() {return reader.getParser();}

    /** {@inheritDoc} */
    public void setParser(EventParser parser) {reader.setParser(parser);}

    /** {@inheritDoc} */
    public String getDictionaryXML() {return reader.getDictionaryXML();}

    /** {@inheritDoc} */
    public boolean hasDictionaryXML() {return reader.hasDictionaryXML();}

    /** {@inheritDoc} */
    public int getNumEventsRemaining() throws IOException, EvioException {
        return reader.getNumEventsRemaining();
    }

    /** {@inheritDoc} */
    public ByteBuffer getByteBuffer() {return reader.getByteBuffer();}

    /** {@inheritDoc} */
    public long fileSize() {return reader.fileSize();}

    /** {@inheritDoc} */
    public IBlockHeader getFirstBlockHeader() {return reader.getFirstBlockHeader();}

    /**
     * Reads a couple things in the first block (physical record) header
     * in order to determine the evio version of buffer.
     * @return status of read attempt
     */
    private ReadStatus findEvioVersion() {
        // Look at first record header
        System.out.println("findEvioVersion: IN");

        // Have enough remaining bytes to read 8 words of header?
        if (byteBuffer.limit() - initialPosition < 32) {
            return ReadStatus.END_OF_FILE;
        }

        try {
            // Set the byte order to match the file's ordering.

            // Check the magic number for endianness (buffer defaults to big endian)
            byteOrder = byteBuffer.order();

            int magicNumber = byteBuffer.getInt(initialPosition + RecordHeader.MAGIC_OFFSET);
System.out.println("findEvioVersion: magic # = 0x" + Integer.toHexString(magicNumber));
            if (magicNumber != IBlockHeader.MAGIC_NUMBER) {
                if (byteOrder == ByteOrder.BIG_ENDIAN) {
                    byteOrder = ByteOrder.LITTLE_ENDIAN;
                }
                else {
                    byteOrder = ByteOrder.BIG_ENDIAN;
                }
                byteBuffer.order(byteOrder);

                // Reread magic number to make sure things are OK
                magicNumber = byteBuffer.getInt(initialPosition + RecordHeader.MAGIC_OFFSET);
                if (magicNumber != IBlockHeader.MAGIC_NUMBER) {
                    return ReadStatus.EVIO_EXCEPTION;
                }
            }

            // Find the version number
            int bitInfo = byteBuffer.getInt(initialPosition + RecordHeader.BIT_INFO_OFFSET);
            evioVersion = bitInfo & RecordHeader.VERSION_MASK;
        }
        catch (BufferUnderflowException a) {/* never happen */}

        return ReadStatus.SUCCESS;
    }


    /** {@inheritDoc} */
    public EvioEvent getEvent(int index) throws IOException, EvioException {
        return reader.getEvent(index);
    }


    /** {@inheritDoc} */
    public synchronized EvioEvent parseEvent(int index) throws IOException, EvioException {
		return reader.parseEvent(index);
	}


    /** {@inheritDoc} */
    public synchronized EvioEvent nextEvent() throws IOException, EvioException {
        return reader.nextEvent();
    }


    /** {@inheritDoc} */
    public synchronized EvioEvent parseNextEvent() throws IOException, EvioException {
		return reader.parseNextEvent();
	}


    /** {@inheritDoc} */
    public void parseEvent(EvioEvent evioEvent) throws EvioException {
		reader.parseEvent(evioEvent);
	}


    /** {@inheritDoc} */
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


    /** {@inheritDoc} */
    public byte[] getEventArray(int eventNumber) throws EvioException, IOException {
        return reader.getEventArray(eventNumber);
    }


    /** {@inheritDoc} */
    public ByteBuffer getEventBuffer(int eventNumber) throws EvioException, IOException {
        return reader.getEventBuffer(eventNumber);
    }


    /** {@inheritDoc} */
    public synchronized void rewind() throws IOException, EvioException {
        reader.rewind();
	}


    /** {@inheritDoc} */
    public synchronized long position() throws IOException, EvioException {
		return reader.position();
	}


    /** {@inheritDoc} */
    public synchronized void close() throws IOException {reader.close();}


    /** {@inheritDoc} */
    public IBlockHeader getCurrentBlockHeader() {return reader.getCurrentBlockHeader();}


    /** {@inheritDoc} */
    public EvioEvent gotoEventNumber(int evNumber) throws IOException, EvioException {
        return reader.gotoEventNumber(evNumber);
    }


    /** {@inheritDoc} */
    public WriteStatus toXMLFile(String path) throws IOException, EvioException {
        return reader.toXMLFile(path);
    }


    /** {@inheritDoc} */
    public WriteStatus toXMLFile(String path, boolean hex) throws IOException, EvioException {
        return reader.toXMLFile(path, hex);
    }


    /** {@inheritDoc} */
    public WriteStatus toXMLFile(String path, IEvioProgressListener progressListener)
                throws IOException, EvioException {
        return reader.toXMLFile(path, progressListener);
    }


    /** {@inheritDoc} */
    public synchronized WriteStatus toXMLFile(String path,
                                              IEvioProgressListener progressListener,
                                              boolean hex)
                throws IOException, EvioException {
        return reader.toXMLFile(path, progressListener, hex);
	}


    /** {@inheritDoc} */
    public synchronized int getEventCount() throws IOException, EvioException {
        return reader.getEventCount();
    }


    /** {@inheritDoc} */
    public synchronized int getBlockCount() throws EvioException{
        return reader.getBlockCount();
    }


}