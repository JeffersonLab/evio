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

import org.jlab.coda.hipo.*;
import org.jlab.coda.hipo.Reader;

import javax.xml.stream.FactoryConfigurationError;
import javax.xml.stream.XMLOutputFactory;
import javax.xml.stream.XMLStreamException;
import javax.xml.stream.XMLStreamWriter;
import java.io.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * This class is used to read an evio version 6 format file or buffer.
 * It is called by an <code>EvioReader</code> object. This class is mostly
 * a wrapper to the new hipo library.<p>
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
 * @since version 6
 * @author timmer
 */
public class EvioReaderV6 implements IEvioReader {

    /** The reader object which does all the work. */
    private org.jlab.coda.hipo.Reader reader;

    /** Is this object currently closed? */
    private boolean closed;

    /** Root element tag for XML file */
    private static final String ROOT_ELEMENT = "evio-data";

    /** Parser object for file/buffer. */
    private EventParser parser;


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
    public EvioReaderV6(String path) throws EvioException, IOException {
        this(new File(path));
    }

    /**
     * Constructor for reading an event file.
     *
     * @param path the full path to the file that contains events.
     *             For writing event files, use an <code>EventWriter</code> object.
     * @param checkRecNumSeq if <code>true</code> check the record number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null;
     *                       if first record number != 1 when checkRecNumSeq arg is true
     */
    public EvioReaderV6(String path, boolean checkRecNumSeq) throws EvioException, IOException {
        this(new File(path), checkRecNumSeq);
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
    public EvioReaderV6(File file) throws EvioException, IOException {
        this(file, false);
    }


    /**
     * Constructor for reading an event file.
     * Sequential reading and not memory-mapped buffer.
     *
     * @param file the file that contains events.
     * @param checkRecNumSeq if <code>true</code> check the record number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null;
     *                       if first record number != 1 when checkRecNumSeq arg is true
     */
    public EvioReaderV6(File file, boolean checkRecNumSeq)
                                        throws EvioException, IOException {
        this(file, checkRecNumSeq, true);
    }


    /**
     * Constructor for reading an event file.
     * Do <b>not</b> set sequential to false for remote files.
     *
     * @param path the full path to the file that contains events.
     *             For writing event files, use an <code>EventWriter</code> object.
     * @param checkRecNumSeq if <code>true</code> check the record number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1
     * @param sequential     if <code>true</code> read the file sequentially,
     *                       else use memory mapped buffers. If file &gt; 2.1 GB,
     *                       reads are always sequential for the older evio format.
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null;
     *                       if first record number != 1 when checkRecNumSeq arg is true
     */
    public EvioReaderV6(String path, boolean checkRecNumSeq, boolean sequential)
            throws EvioException, IOException {
        this(new File(path), checkRecNumSeq, sequential);
    }


    /**
     * Constructor for reading an event file.
     * Do <b>not</b> set sequential to false for remote files.
     *
     * @param file the file that contains events.
     * @param checkRecNumSeq if <code>true</code> check the record number sequence
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
     *                       if first record number != 1 when checkRecNumSeq arg is true
     */
    EvioReaderV6(File file, boolean checkRecNumSeq, boolean sequential)
                                        throws EvioException, IOException {

        try {
            reader = new Reader(file.getPath(), true, checkRecNumSeq);
            parser = new EventParser();
        }
        catch (HipoException e) {
            throw new EvioException(e);
        }
    }


    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     * @see EventWriter
     * @throws EvioException if buffer arg is null
     */
    public EvioReaderV6(ByteBuffer byteBuffer) throws EvioException {
        this(byteBuffer, false);
    }

    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     * @param checkRecNumSeq if <code>true</code> check the record number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1
     * @see EventWriter
     * @throws EvioException if buffer arg is null;
     *                       if first record number != 1 when checkRecNumSeq arg is true
     */
    public EvioReaderV6(ByteBuffer byteBuffer, boolean checkRecNumSeq) throws EvioException {

         try {
             reader = new Reader(byteBuffer);
             reader.scanBuffer();
             parser = new EventParser();
         }
         catch (HipoException e) {
             throw new EvioException(e);
         }
    }

    /**
     * This method can be used to avoid creating additional EvioReader
     * objects by reusing this one with another buffer. The method
     * {@link #close()} is called before anything else.
     *
     * @param buf ByteBuffer to be read
     * @throws IOException   if read failure
     * @throws EvioException if buf is null;
     *                       if first record number != 1 when checkRecNumSeq arg is true
     */
    @Override
    public synchronized void setBuffer(ByteBuffer buf) throws EvioException, IOException {
        try {
            reader.setBuffer(buf);
        }
        catch (HipoException e) {/* never thrown cause buf never null */}
    }

    /** {@inheritDoc} */
    @Override
    public synchronized boolean isClosed() {return reader.isClosed();}

    /** {@inheritDoc} */
    @Override
    public boolean checkBlockNumberSequence() {return reader.getCheckRecordNumberSequence();}

    /** {@inheritDoc} */
    @Override
    public ByteOrder getByteOrder() {return reader.getByteOrder();}

    /** {@inheritDoc} */
    @Override
    public int getEvioVersion() {return reader.getVersion();}

    /** {@inheritDoc} */
    @Override
    public String getPath() {return reader.getFileName();}

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

    /** {@inheritDoc} */
    @Override
    public String getDictionaryXML() {return reader.getDictionary();}

    /** {@inheritDoc} */
    @Override
    public boolean hasDictionaryXML() {return reader.hasDictionary();}

    /**
     * Get the number of events remaining in the file.
     * Useful only if doing a sequential read.
     *
     * @return number of events remaining in the file
     */
    @Override
    public int getNumEventsRemaining() throws EvioException {return  reader.getNumEventsRemaining();}

    /** {@inheritDoc} */
    @Override
    public ByteBuffer getByteBuffer() {return reader.getBuffer();}

    /** {@inheritDoc} */
    @Override
    public long fileSize() {return reader.getFileSize();}


    /** {@inheritDoc} */
    @Override
    public IBlockHeader getFirstBlockHeader() {return reader.getFirstRecordHeader();}


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
     *                       if index out of bounds;
     *                       if object closed
     */
    @Override
    public synchronized EvioEvent getEvent(int index) throws EvioException {

        return getEvent(getEventArray(index), 0, reader.getByteOrder());
    }


    /** {@inheritDoc} */
    @Override
    public synchronized EvioEvent parseEvent(int index) throws EvioException {
		EvioEvent event = getEvent(index);
        if (event != null) parseEvent(event);
		return event;
	}


    /** {@inheritDoc} */
    @Override
    public synchronized EvioEvent nextEvent() throws EvioException {

        if (closed) {
            throw new EvioException("object closed");
        }

        byte[] bytes;
        try {
            bytes = reader.getNextEvent();
        }
        catch (HipoException e) {
            throw new EvioException(e);
        }

        return getEvent(bytes, 0, reader.getByteOrder());
    }


    /** {@inheritDoc} */
    @Override
    public synchronized EvioEvent parseNextEvent() throws IOException, EvioException {
		EvioEvent event = nextEvent();
		if (event != null) {
			parseEvent(event);
		}
		return event;
	}

	
    /** {@inheritDoc} */
    @Override
    public void parseEvent(EvioEvent evioEvent) throws EvioException {
        // This method is synchronized too
		parser.parseEvent(evioEvent);
	}


    /**
     * Transform an event in the form of a byte array into an EvioEvent object.
     * However, only top level header is parsed.
     * Byte array must not be in file format (have record headers),
     * but must consist of only the bytes comprising the evio event.
     *
     * @param array   array to parse into EvioEvent object.
     * @param offset  offset into array.
     * @param order   byte order to use.
     * @return an EvioEvent object parsed from the given array.
     * @throws EvioException if null arg, too little data, length too large, or data not in evio format.
     */
    static EvioEvent getEvent(byte[] array, int offset, ByteOrder order) throws EvioException {

        if (array == null || array.length - offset < 8) {
            throw new EvioException("arg null or too little data");
        }

        int byteLen = array.length - offset;
        EvioEvent event = new EvioEvent();
        BaseStructureHeader header = event.getHeader();

        // Read the first header word - the length in 32bit words
        int wordLen   = ByteDataTransformer.toInt(array, order, offset);
        int dataBytes = 4*(wordLen - 1);
        if (wordLen < 1) {
            throw new EvioException("non-positive length (0x" + Integer.toHexString(wordLen) + ")");
        }
        else if (dataBytes > byteLen) {
            // Protect against too large length
            throw new EvioException("bank length too large (needed " + dataBytes +
                                            " but have " + byteLen + " bytes)");
        }
        header.setLength(wordLen);

        // Read and parse second header word
        int word = ByteDataTransformer.toInt(array, order, offset+4);
        header.setTag(word >>> 16);
        int dt = (word >> 8) & 0xff;
        header.setDataType(dt & 0x3f);
        header.setPadding(dt >>> 6);
        header.setNumber(word & 0xff);

        // Set the raw data
        byte data[] = new byte[dataBytes];
        System.arraycopy(array, offset+8, data, 0, dataBytes);
        event.setRawBytes(data);
        event.setByteOrder(order);

        return event;
    }


    /**
     * Completely parse the given byte array into an EvioEvent.
     * Byte array must not be in file format (have record headers),
     * but must consist of only the bytes comprising the evio event.
     *
     * @param array   array to parse into EvioEvent object.
     * @param offset  offset into array.
     * @param order   byte order to use.
     * @return the EvioEvent object parsed from the given bytes.
     * @throws EvioException if null arg, too little data, length too large, or data not in evio format.
     */
    public static EvioEvent parseEvent(byte[] array, int offset, ByteOrder order) throws EvioException {
        EvioEvent event = getEvent(array, offset, order);
        EventParser.eventParse(event);
        return event;
    }


    /** {@inheritDoc} */
    @Override
    public byte[] getEventArray(int eventNumber) throws EvioException {

        if (closed) {
            throw new EvioException("object closed");
        }

        try {
            byte[] evBytes = reader.getEvent(eventNumber - 1);
            if (evBytes == null) {
                throw new EvioException("eventNumber (" + eventNumber + ") is out of bounds");
            }
            return evBytes;
        }
        catch (HipoException e) {
            throw new EvioException(e);
        }
    }

    
    /** {@inheritDoc} */
    @Override
    public ByteBuffer getEventBuffer(int eventNumber)
            throws EvioException, IOException {
        return ByteBuffer.wrap(getEventArray(eventNumber)).order(reader.getByteOrder());
    }


	/** This method is not relevant in evio 6 and does nothing. */
    @Override
    public void rewind() {}


    /** This method is not relevant in evio 6, does nothing, and returns 0. */
	@Override
    public synchronized long position() {return 0L;}


	/**
	 * This is closes the file, but for buffers it only sets the position to 0.
     * @throws IOException if error accessing file
	 */
    @Override
    public synchronized void close() throws IOException {
        if (closed) {
            return;
        }

        reader.close();
        closed = true;
    }

    
    /** {@inheritDoc} */
	@Override
    public IBlockHeader getCurrentBlockHeader() {return reader.getCurrentRecordStream().getHeader();}


    /**
     * In this version, this method is a wrapper on {@link #parseEvent(int)}.
     *
     * @deprecated use {@link #parseEvent(int)}.
     * @param evNumber the event number from the start of the file starting at 1.
     * @return the specified event in file or null if there's an error or nothing at that event #.
     * @throws EvioException if object closed
     */
    @Override
    public synchronized EvioEvent gotoEventNumber(int evNumber) throws EvioException {

        if (closed) {
            throw new EvioException("object closed");
        }

        try {
            return parseEvent(evNumber);
        }
        catch (EvioException e) {
            return null;
        }
    }


    /** {@inheritDoc} */
    @Override
    public WriteStatus toXMLFile(String path) throws EvioException {
        return toXMLFile(path, false);
    }


    /** {@inheritDoc} */
    @Override
    public WriteStatus toXMLFile(String path, boolean hex) throws EvioException {
        return toXMLFile(path, null, hex);
    }

    /** {@inheritDoc} */
	@Override
    public WriteStatus toXMLFile(String path, IEvioProgressListener progressListener)
                throws EvioException {
        return toXMLFile(path, progressListener, false);
    }


    /** {@inheritDoc} */
	@Override
    public synchronized WriteStatus toXMLFile(String path,
                                              IEvioProgressListener progressListener,
                                              boolean hex)
                throws EvioException {

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

			int eventCount = getEventCount();

			// Loop through the events
			EvioEvent event;
			try {
				for (int i=1; i <= eventCount; i++) {
				    event = parseEvent(i);
					event.toXML(xmlWriter, hex);
					// Anybody interested in progress?
					if (progressListener != null) {
						progressListener.completed(event.getEventNumber(), eventCount);
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


    /** {@inheritDoc} */
    @Override
    public synchronized int getEventCount() throws EvioException {

        if (closed) {
            throw new EvioException("object closed");
        }

        return reader.getEventCount();
    }


    /** {@inheritDoc} */
    @Override
    public synchronized int getBlockCount() throws EvioException {

        if (closed) {
            throw new EvioException("object closed");
        }

        return reader.getRecordCount();
    }


}