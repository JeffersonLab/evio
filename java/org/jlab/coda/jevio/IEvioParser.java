package org.jlab.coda.jevio;


import javax.xml.stream.XMLStreamException;
import java.io.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * This is <b>not</b> an interface that will be directly needed by the user.
 * It allows the <code>EvioReader</code> class to hand-off parsing evio
 * files and buffers to subclasses specialized for various versions and
 * manners of access (random, sequential).
 *
 * @author timmer 6/29/2017
 */
public interface IEvioParser {


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
    void setBuffer(ByteBuffer buf) throws EvioException, IOException;
    
    /**
     * Has {@link #close()} been called?
     *
     * @return {@code true} if this object closed, else {@code false}.
     */
    boolean isClosed();

    /**
     * Is this reader checking the block number sequence and
     * throwing an exception is it's not sequential and starting with 1?
     * @return <code>true</code> if checking block number sequence, else <code>false</code>
     */
    boolean checkBlockNumberSequence();

    /**
     * Get the byte order of the file/buffer being read.
     * @return byte order of the file/buffer being read.
     */
    ByteOrder getByteOrder();


    /**
     * Get the path to the file.
     * @return path to the file
     */
    String getPath();

    /**
     * Get the file/buffer parser.
     * @return file/buffer parser.
     */
    EventParser getParser();

    /**
     * Set the file/buffer parser.
     * @param parser file/buffer parser.
     */
    void setParser(EventParser parser);

    /**
     * Get the XML format dictionary is there is one.
     *
     * @return XML format dictionary, else null.
     */
    String getDictionaryXML();

    // PUT in EvioReader
    /**
     * Does this evio file have an associated XML dictionary?
     *
     * @return <code>true</code> if this evio file has an associated XML dictionary,
     *         else <code>false</code>
     */
    boolean hasDictionaryXML();

    /**
     * Get the number of events remaining in the file.
     *
     * @return number of events remaining in the file
     * @throws IOException if failed file access
     * @throws EvioException if failed reading from coda v3 file
     */
    int getNumEventsRemaining() throws IOException, EvioException;

    /**
     * Get the byte buffer being read directly or corresponding to the event file.
     * Not a very useful method. For files, it works only for evio format versions 2,3 and
     * returns the internal buffer containing an evio block if using sequential access
     * (for example files &gt; 2.1 GB). It returns the memory mapped buffer otherwise.
     * For reading buffers it returns the buffer being read.
     * @return the byte buffer being read (in certain cases).
     */
    ByteBuffer getByteBuffer();

    /**
     * Get the size of the file being read, in bytes.
     * For small files, obtain the file size using the memory mapped buffer's capacity.
     * @return the file size in bytes
     */
    long fileSize();


    // PUT in EvioReader
    /**
     * This returns the FIRST block (physical record) header.
     *
     * @return the first block header.
     */
    IBlockHeader getFirstBlockHeader();


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
    EvioEvent getEvent(int index) throws IOException, EvioException;


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
    EvioEvent parseEvent(int index) throws IOException, EvioException;


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
    EvioEvent nextEvent() throws IOException, EvioException;


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
    EvioEvent parseNextEvent() throws IOException, EvioException;

	/**
	 * This will parse an event, SAX-like. It will drill down and uncover all structures
     * (banks, segments, and tagsegments) and notify any interested listeners.<p>
	 *
	 * As useful as this sounds, most applications will probably call {@link #parseNextEvent()}
     * instead, since it combines combines getting the next event with parsing the next event.<p>
     *
     * This method is only called by synchronized methods and therefore is not synchronized.
	 *
	 * @param evioEvent the event to parse.
	 * @throws EvioException if bad format
	 */
    void parseEvent(EvioEvent evioEvent) throws EvioException;


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
    void rewind() throws IOException, EvioException;

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
    long position() throws IOException, EvioException;

	/**
	 * This method sets the current position in the file or buffer. This
     * method, along with the <code>rewind()</code>, <code>position()</code>
     * and the <code>close()</code> method, allows applications to treat files
     * in a normal random access manner. Only meaningful to evio versions 1-3
     * and for sequential reading.<p>
     *
     * <b>HOWEVER</b>, using this method is not necessary for random access of
     * events and is no longer recommended because it interferes with the sequential
     * reading of events. Therefore it is now deprecated.
     *
	 * @deprecated
	 * @param position the new position of the buffer.
     * @throws IOException   if error accessing file
     * @throws EvioException if object closed
     */
    void position(long position) throws IOException, EvioException;

	/**
	 * This closes the file, but for buffers it only sets the position to 0.
     * This method, along with the <code>rewind()</code> and the two
     * <code>position()</code> methods, allows applications to treat files
     * in a normal random access manner.
     *
     * @throws IOException if error accessing file
	 */
    void close() throws IOException;

	/**
	 * This returns the current (active) block (physical record) header.
     * Since most users have no interest in physical records, this method
     * should not be used. Mostly it is used by the test programs in the
	 * <code>EvioReaderTest</code> class.
	 *
	 * @return the current block header.
	 */
	IBlockHeader getCurrentBlockHeader();

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
    EvioEvent gotoEventNumber(int evNumber) throws IOException, EvioException;

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
    EvioEvent gotoEventNumber(int evNumber, boolean parse)
            throws IOException, EvioException;

    /**
     * Rewrite the file to XML (not including dictionary).
     *
     * @param path the path to the XML file.
     *
     * @throws IOException   if failed file access
     * @throws EvioException if object closed, parse error
     * @throws XMLStreamException if XML issues
     */
    void toXMLFile(String path) throws IOException, EvioException, XMLStreamException;

    /**
     * Rewrite the file to XML (not including dictionary).
     *
     * @param path the path to the XML file.
     * @param hex if true, ints get displayed in hexadecimal
     *
     * @throws IOException   if failed file access
     * @throws EvioException if object closed, parse error
     * @throws XMLStreamException if XML issues
     */
    void toXMLFile(String path, boolean hex) throws IOException, EvioException, XMLStreamException;

    /**
     * Rewrite the file to XML (not including dictionary).
	 *
	 * @param path the path to the XML file.
	 * @param progressListener and optional progress listener, can be <code>null</code>.
     *
     * @throws IOException   if failed file access
     * @throws EvioException if object closed, parse error
     * @throws XMLStreamException if XML issues
     * @see IEvioProgressListener
	 */
	void toXMLFile(String path, IEvioProgressListener progressListener)
                throws IOException, EvioException, XMLStreamException;

    /**
     * Rewrite the file to XML (not including dictionary).
	 *
	 * @param path the path to the XML file.
	 * @param progressListener and optional progress listener, can be <code>null</code>.
     * @param hex if true, ints get displayed in hexadecimal
     *
     * @throws IOException   if failed file access
     * @throws EvioException if object closed, parse error
     * @throws XMLStreamException if XML issues
     * @see IEvioProgressListener
	 */
	void toXMLFile(String path, IEvioProgressListener progressListener,
                                              boolean hex)
            throws IOException, EvioException, XMLStreamException;


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
    int getEventCount() throws IOException, EvioException;

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
    int getBlockCount() throws EvioException;

}