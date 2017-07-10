package org.jlab.coda.jevio;

import javax.xml.stream.XMLOutputFactory;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamWriter;
import java.io.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;

/**
 * This is <b>not</b> an interface that will be directly needed by the user.
 * It allows the <code>EvioReader</code> class to hand-off parsing evio
 * files and buffers to subclasses specialized for various versions and
 * manners of access (random, sequential). This abstract class contains
 * the common code for these subclasses.
 *
 * @author timmer 6/29/2017
 */
abstract class EvioParserAbstract implements IEvioParser {

    /**
	 * This <code>enum</code> denotes the status of a read. <br>
	 * SUCCESS indicates a successful read. <br>
	 * END_OF_FILE indicates that we cannot read because an END_OF_FILE has occurred. Technically this means that what
	 * ever we are trying to read is larger than the buffer's unread bytes.<br>
	 * EVIO_EXCEPTION indicates that an EvioException was thrown during a read, possibly due to out of range values,
	 * such as a negative start position.<br>
	 * UNKNOWN_ERROR indicates that an unrecoverable error has occurred.
	 */
	enum ReadStatus {
		SUCCESS, END_OF_FILE, EVIO_EXCEPTION, UNKNOWN_ERROR
	}

    /** Root element tag for XML file */
    private static final String ROOT_ELEMENT = "evio-data";

    /** Default size for a single file read in bytes when reading
     *  evio format 1-3. Equivalent to 500, 32,768 byte blocks.
     *  This constant <b>MUST BE</b> an integer multiple of 32768.*/
    static final int DEFAULT_READ_BYTES = 32768 * 500; // 16384000 bytes



    /** When doing a sequential read, used to assign a transient
     * number [1..n] to events as they are being read. */
    int eventNumber = 0;

    /**
     * This is the number of events in the file. It is not computed unless asked for,
     * and if asked for it is computed and cached in this variable.
     */
    int eventCount = -1;

    /** Evio version number (1-4). Obtain this by reading first block header. */
    int evioVersion;

    /**
     * Endianness of the data being read, either
     * {@link ByteOrder#BIG_ENDIAN} or
     * {@link ByteOrder#LITTLE_ENDIAN}.
     */
    ByteOrder byteOrder;

    /** Size of the first block in bytes. */
    int firstBlockSize;

    /**
     * This is the number of blocks in the file including the empty block at the
     * end of the version 4 files. It is not computed unless asked for,
     * and if asked for it is computed and cached in this variable.
     */
    int blockCount = -1;

    ByteBuffer headerBuf;
    ByteBuffer indexBuf;

	/** The current block header for evio versions 1-3. */
    BlockHeaderV2 blockHeader2 = new BlockHeaderV2();

    /** The current block header for evio version 4. */
    BlockHeaderV4 blockHeader4 = new BlockHeaderV4();

    /** Reference to current block header, any version, through interface.
     *  This must be the same object as either blockHeader2 or blockHeader4
     *  depending on which evio format version the data is in. */
    IBlockHeader blockHeader;

    /** Reference to first block header. */
    IBlockHeader firstBlockHeader;

    /** Block number expected when reading. Used to check sequence of blocks. */
    int blockNumberExpected = 1;

    /** If true, throw an exception if block numbers are out of sequence. */
    boolean checkBlockNumberSequence;

    /** Is this the last block in the file or buffer? */
    boolean lastBlock;

    /** Does the current block have a dictionary? */
    boolean hasDictionary;

   /**
     * Version 4 files may have an xml format dictionary in the
     * first event of the first block.
     */
    String dictionaryXML;

    /** The buffer being read. */
    ByteBuffer byteBuffer;

    /** The buffer with decompressed data. */
    ByteBuffer compressedBuffer;

    /** Parser object for this file/buffer. */
    EventParser eventParser;

    /** Initial position of buffer or mappedByteBuffer when reading a file. */
    int initialPosition;

    //------------------------
    // File specific members
    //------------------------

    /** Use this object to handle files &gt; 2.1 GBytes but still use memory mapping. */
    MappedMemoryHandler mappedMemoryHandler;

    /** Absolute path of the underlying file. */
    String path;

    /** File object. */
    RandomAccessFile file;

    /** File size in bytes. */
    long fileSize;

    /** File channel used to read data and access file position. */
    FileChannel fileChannel;

    /** Do we need to swap data from file? */
    boolean swap;

    /**
     * Read this file sequentially and not using a memory mapped buffer.
     * If the file being read &gt; 2.1 GBytes, then this is always true.
     */
    boolean sequentialRead;

    //-------------------------------------
    // LZ4 Compression related members
    //-------------------------------------

    /** If true, write files as compressed evio output. */
    boolean compressedInput = false;

    //------------------------
    // EvioReader's state
    //------------------------

    /** Is this object currently closed? */
    boolean closed;

    //------------------------
    // Abstract Methods
    //------------------------

    /** {@inheritDoc} */
    abstract public EvioEvent getEvent(int index) throws IOException, EvioException;

    /** {@inheritDoc} */
    abstract public EvioEvent nextEvent() throws IOException, EvioException;

    /** {@inheritDoc} */
    abstract public void rewind() throws IOException, EvioException;

    /** {@inheritDoc} */
    abstract public int getBlockCount() throws EvioException;

    /** {@inheritDoc} */
    abstract public int getEventCount() throws IOException, EvioException;

    /** {@inheritDoc} */
    abstract public void close() throws IOException;

    /** {@inheritDoc} */
    abstract public void position(long position) throws IOException, EvioException;

    /** {@inheritDoc} */
    abstract public long position() throws IOException, EvioException;
    
    //------------------------

    /**
     * This class stores the state of this reader so it can be recovered
     * after a state-changing method has been called -- like {@link #rewind()}.
     */
    static final class ReaderState {
        boolean lastBlock;
        int eventNumber;
        long filePosition;
        int byteBufferLimit;
        int byteBufferPosition;
        int blockNumberExpected;
        BlockHeaderV2 blockHeader2;
        BlockHeaderV4 blockHeader4;
    }


    /**
     * This method saves the current state of this EvioReader object.
     * @return the current state of this EvioReader object.
     */
    ReaderState getState() {
        ReaderState currentState = new ReaderState();
        currentState.lastBlock   = lastBlock;
        currentState.eventNumber = eventNumber;
        currentState.blockNumberExpected = blockNumberExpected;

        if (sequentialRead) {
            try {
                currentState.filePosition = fileChannel.position();
            }
            catch (IOException e) {/* should be OK */}
        }

        if (byteBuffer != null) {
            currentState.byteBufferLimit = byteBuffer.limit();
            currentState.byteBufferPosition = byteBuffer.position();
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
    void restoreState(ReaderState state) {
        lastBlock   = state.lastBlock;
        eventNumber = state.eventNumber;
        blockNumberExpected = state.blockNumberExpected;

        if (sequentialRead) {
            try {
                fileChannel.position(state.filePosition);
            }
            catch (IOException e) {/* should be OK */}
        }
        
        if (byteBuffer != null) {
            byteBuffer.limit(state.byteBufferLimit);
            byteBuffer.position(state.byteBufferPosition);
        }

        if (evioVersion > 3) {
            blockHeader = blockHeader4 = state.blockHeader4;
        }
        else {
            blockHeader = blockHeader2 = state.blockHeader2;
        }
    }


    //------------------------


    /** {@inheritDoc} */
    public void setBuffer(ByteBuffer buf) throws EvioException, IOException {}

    /** {@inheritDoc} */
   public synchronized boolean isClosed() { return closed; }

    /** {@inheritDoc} */
    public boolean checkBlockNumberSequence() { return checkBlockNumberSequence; }

    /** {@inheritDoc} */
    public ByteOrder getByteOrder() { return byteOrder; }


    /** {@inheritDoc} */
     public String getPath() {
         return path;
     }

    /** {@inheritDoc} */
    public EventParser getParser() {
        return eventParser;
    }

    /** {@inheritDoc} */
    public void setParser(EventParser parser) {
        if (parser != null) {
            this.eventParser = parser;
        }
    }

    /** {@inheritDoc} */
    public String getDictionaryXML() {
        return dictionaryXML;
    }

    /** {@inheritDoc} */
    public boolean hasDictionaryXML() {
        return dictionaryXML != null;
    }

    /** {@inheritDoc} */
    public int getNumEventsRemaining() throws IOException, EvioException {
        return getEventCount() - eventNumber;
    }

    /** {@inheritDoc} */
    public ByteBuffer getByteBuffer() { return byteBuffer; }

    /** {@inheritDoc} */
    public long fileSize() {
        return fileSize;
    }

    /** {@inheritDoc} */
    public IBlockHeader getFirstBlockHeader() { return firstBlockHeader; }

    /**
     * Sets the proper buffer position for first-time read AFTER the first header.
     * @param buffer buffer to prepare
     */
    void prepareForBufferRead(ByteBuffer buffer) {
        buffer.position(4*blockHeader.getHeaderLength());
    }


    /** {@inheritDoc} */
	public synchronized EvioEvent parseEvent(int index) throws IOException, EvioException {
		EvioEvent event = getEvent(index);
        if (event != null) parseEvent(event);
		return event;
	}


    /** {@inheritDoc} */
	public synchronized EvioEvent parseNextEvent() throws IOException, EvioException {
		EvioEvent event = nextEvent();
		if (event != null) {
			parseEvent(event);
		}
		return event;
	}


    /** {@inheritDoc} */
	public void parseEvent(EvioEvent evioEvent) throws EvioException {
        // This method is synchronized too
		eventParser.parseEvent(evioEvent);
	}

    /**
   	 * Get the number of bytes remaining in the internal byte buffer.
     * Called only by {@link #nextEvent()}.
   	 *
   	 * @return the number of bytes remaining in the current block (physical record).
        */
   	int bufferBytesRemaining() {
        return byteBuffer.remaining();
   	}

    /**
   	 * Get the number of bytes remaining in the current block (physical record).
     * This is used for pathology checks like crossing the block boundary.
     * Called only by {@link #nextEvent()}.
   	 *
   	 * @return the number of bytes remaining in the current block (physical record).
        */
   	int blockBytesRemaining() {
   		try {
               return blockHeader.bytesRemaining(byteBuffer.position());
   		}
   		catch (EvioException e) {
   			e.printStackTrace();
   			return -1;
   		}
   	}

    /**
   	 * Get the number of bytes remaining in the current block (physical record).
     * This is used for pathology checks like crossing the block boundary.
     * Called only by {@link #nextEvent()}.
   	 *
   	 * @return the number of bytes remaining in the current block (physical record).
        */
   	int blockDataBytesRemaining() {
   		try {
            return blockHeader.dataBytesRemaining(byteBuffer.position());
   		}
   		catch (EvioException e) {
   			e.printStackTrace();
   			return -1;
   		}
   	}

    /** {@inheritDoc} */
	public IBlockHeader getCurrentBlockHeader() {
		return blockHeader;
	}

    /** {@inheritDoc} */
    public EvioEvent gotoEventNumber(int evNumber) throws IOException, EvioException {
        return gotoEventNumber(evNumber, true);
    }

    /** {@inheritDoc} */
    public synchronized EvioEvent gotoEventNumber(int evNumber, boolean parse)
            throws IOException, EvioException {

        if (evNumber < 1) {
			return null;
		}

        if (closed) {
            throw new EvioException("object closed");
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


    /** {@inheritDoc} */
    public void toXMLFile(String path) throws IOException, EvioException, XMLStreamException {
        toXMLFile(path, false);
    }

    /** {@inheritDoc} */
    public void toXMLFile(String path, boolean hex) throws IOException, EvioException, XMLStreamException {
        toXMLFile(path, null, hex);
    }

    /** {@inheritDoc} */
	public void toXMLFile(String path, IEvioProgressListener progressListener)
                throws IOException, EvioException, XMLStreamException {
        toXMLFile(path, progressListener, false);
    }

    /** {@inheritDoc} */
    public synchronized void toXMLFile(String path,
                                       IEvioProgressListener progressListener,
                                       boolean hex)
            throws IOException, EvioException, XMLStreamException {

        if (closed) {
            throw new EvioException("object closed");
        }

        FileOutputStream fos = new FileOutputStream(path);   // FileNotFoundException

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
        while ((event = parseNextEvent()) != null) {
            event.toXML(xmlWriter, hex);
            // anybody interested in progress?
            if (progressListener != null) {
                progressListener.completed(event.getEventNumber(), getEventCount());
            }
        }

        // done. Close root element, end the document, and flush.
        xmlWriter.writeEndElement();
        xmlWriter.writeEndDocument();
        xmlWriter.flush();
        xmlWriter.close();

        fos.close();

        // Restore our original settings
        restoreState(state);
    }

}