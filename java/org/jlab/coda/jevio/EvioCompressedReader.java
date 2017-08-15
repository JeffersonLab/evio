package org.jlab.coda.jevio;

import javax.xml.stream.XMLStreamException;
import java.io.*;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
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
public class EvioCompressedReader {

    /** Evio version number (1-4). Obtain this by reading first block header. */
    private int evioVersion;

    /** The current block header for evio versions 1-3. */
    private BlockHeaderV2 blockHeader2 = new BlockHeaderV2();

    /** The current block header for evio version 4. */
    private BlockHeaderV4 blockHeader4 = new BlockHeaderV4();

    /** Block number expected when reading. Used to check sequence of blocks. */
    private int blockNumberExpected = 1;

    /** If true, throw an exception if block numbers are out of sequence. */
    private boolean checkBlockNumberSequence;


    //-----------------------------
    // Specific parsing object
    //-----------------------------

    /** Object used to actually parse the file or buffer. */
    private IEvioParser parser;


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
    public EvioCompressedReader(String path) throws EvioException, IOException {
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
    public EvioCompressedReader(String path, boolean checkBlkNumSeq) throws EvioException, IOException {
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
    public EvioCompressedReader(File file) throws EvioException, IOException {
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
    public EvioCompressedReader(File file, boolean checkBlkNumSeq)
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
    public EvioCompressedReader(String path, boolean checkBlkNumSeq, boolean sequential)
            throws EvioException, IOException {
        this(new File(path), checkBlkNumSeq, sequential);
    }


    /**
     * Constructor for reading an event file.
     * Do <b>not</b> set sequential to false for remote files.
     *
     * @param ffile the file that contains events.
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
    public EvioCompressedReader(File ffile, boolean checkBlkNumSeq, boolean sequential)
                                        throws EvioException, IOException {
        if (ffile == null) {
            throw new EvioException("File arg is null");
        }

        RandomAccessFile file = new RandomAccessFile(ffile, "r");
        FileChannel fileChannel = file.getChannel();
        long fileSize = fileChannel.size();

        if (fileSize < 40) {
            throw new EvioException("File too small to have valid evio data");
        }

        // Look at the first 32 bytes (8words) of the block
        // header to get info like endianness and version.
        // Store it for later reference in blockHeader2,4 and in other variables.
        ByteBuffer headerBuf = ByteBuffer.allocate(32);
        int bytesRead = 0;
        while (bytesRead < 32) {
            bytesRead += fileChannel.read(headerBuf);
        }
        headerBuf.flip();
        parseFirstHeader(headerBuf);

        // What we do from here depends on the evio format version.
        // If we've got the old version, don't memory map big (> 2.1 GB) files,
        // use sequential reading instead.
        if (evioVersion < 4) {
            // Got a big file? If so we cannot use a memory mapped file.
            if (fileSize > Integer.MAX_VALUE) {
                sequential = true;
            }

            if (sequential) {
System.out.println("EvioCompressedReader: version 2, sequential");
                parser = new EvioParserV2Seq(ffile, fileChannel, checkBlkNumSeq, blockHeader2);
            }
            else {
System.out.println("EvioCompressedReader: version 2, random access");
                parser = new EvioParserV2Ra(ffile, fileChannel, checkBlkNumSeq, blockHeader2);
            }
        }
        // For version 4 & UNcompressed data ...
        else if (!blockHeader4.isCompressed()) {
            if (sequential) {
System.out.println("EvioCompressedReader: version 4, sequential");
                parser = new EvioParserV4Seq(ffile, fileChannel, checkBlkNumSeq, blockHeader4);
            }
            else {
System.out.println("EvioCompressedReader: version 4, random access");
                parser = new EvioParserV4Ra(ffile, fileChannel, checkBlkNumSeq, blockHeader4);
            }
        }
        // For version 4 & compressed data ...
        else {
System.out.println("EvioCompressedReader: LZ4 compressed");
            parser = new EvioParserV4Lz4(ffile, fileChannel, checkBlkNumSeq, blockHeader4);
        }
    }


    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if buffer arg is null
     */
    public EvioCompressedReader(ByteBuffer byteBuffer) throws EvioException, IOException {
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
     * @throws IOException   if read failure
     * @throws EvioException if buffer arg is null;
     *                       if first block number != 1 when checkBlkNumSeq arg is true
     */
    public EvioCompressedReader(ByteBuffer byteBuffer, boolean checkBlkNumSeq)
                                                    throws EvioException, IOException {

        if (byteBuffer == null) {
            throw new EvioException("Buffer arg is null");
        }

        checkBlockNumberSequence = checkBlkNumSeq;

        // Look at the first block header to get various info like endianness and version.
        // Store it for later reference in blockHeader2,4 and in variables.
        // Position does not change.
        parseFirstHeader(byteBuffer);

        // For the latest evio format, generate a table
        // of all event positions in buffer for random access.
        if (evioVersion > 3) {
System.out.println("EvioCompressedReader: version 4, random access (buffer)");
            parser = new EvioParserV4Ra(byteBuffer, checkBlkNumSeq, blockHeader4);
        }
        else {
System.out.println("EvioCompressedReader: version 2, random access (buffer)");
            parser = new EvioParserV2Ra(byteBuffer, checkBlkNumSeq, blockHeader2);
        }
    }


    /**
     * Reads 8 words of the first block (physical record) header in order to determine
     * the evio version # and endianness of the file or buffer in question. These things
     * do <b>not</b> need to be examined in subsequent block headers. Called only by
     * synchronized methods or constructors. Since absolute "gets" are used, the position
     * of the buffer is not changed by this method.
     *
     * @throws EvioException if buffer contains too little data, invalid data,
     *                       or bad block # sequence
     */
    private void parseFirstHeader(ByteBuffer headerBuf)
            throws BufferUnderflowException, EvioException {

        // Check buffer length
        int bufPos = headerBuf.position();

        if (headerBuf.remaining() < 32) {
            throw new EvioException("too little data in buffer");
        }

        // Get the file's version # and byte order
        boolean swap = false;
        ByteOrder byteOrder = headerBuf.order();

        int magicNumber = headerBuf.getInt(bufPos + IBlockHeader.MAGIC_OFFSET);

        if (magicNumber != IBlockHeader.MAGIC_NUMBER) {
            swap = true;

            if (byteOrder == ByteOrder.BIG_ENDIAN) {
                byteOrder = ByteOrder.LITTLE_ENDIAN;
            }
            else {
                byteOrder = ByteOrder.BIG_ENDIAN;
            }
            headerBuf.order(byteOrder);
System.out.println("Byte order = " + byteOrder);

            // Reread magic number to make sure things are OK
            magicNumber = headerBuf.getInt(bufPos + IBlockHeader.MAGIC_OFFSET);
            if (magicNumber != IBlockHeader.MAGIC_NUMBER) {
System.out.println("ERROR reread magic # (" + magicNumber + ") & still not right");
                throw new EvioException("bad magic #");
            }
        }

        // Check the version number. This requires peeking ahead 5 ints or 20 bytes.
        evioVersion = headerBuf.getInt(bufPos + IBlockHeader.VERSION_OFFSET) & IBlockHeader.VERSION_MASK;
        if (evioVersion < 1 || evioVersion > 4)  {
            throw new EvioException("bad version # (" + evioVersion + ")");
        }

        // Reference to current block header, any version, through interface.
        // This must be the same object as either blockHeader2 or blockHeader4
        // depending on which evio format version the data is in.
        IBlockHeader blockHeader;

        if (evioVersion >= 4) {
            blockHeader4.setBufferStartingPosition(bufPos);

            // Read the header data
            blockHeader4.setSize            (headerBuf.getInt(bufPos + 4*BlockHeaderV4.EV_BLOCKSIZE));
            blockHeader4.setNumber          (headerBuf.getInt(bufPos + 4*BlockHeaderV4.EV_BLOCKNUM));
            blockHeader4.setHeaderLength    (headerBuf.getInt(bufPos + 4*BlockHeaderV4.EV_HEADERSIZE));
            blockHeader4.setEventCount      (headerBuf.getInt(bufPos + 4*BlockHeaderV4.EV_COUNT));
            blockHeader4.setCompressedLength(headerBuf.getInt(bufPos + 4*BlockHeaderV4.EV_COMPRESSEDLENGTH));
            blockHeader4.parseToBitInfo     (headerBuf.getInt(bufPos + 4*BlockHeaderV4.EV_VERSION));
            blockHeader4.setReserved2       (headerBuf.getInt(bufPos + 4*BlockHeaderV4.EV_RESERVED2));
            blockHeader4.setMagicNumber     (headerBuf.getInt(bufPos + 4*BlockHeaderV4.EV_MAGIC));

            // Store some of the parsed info
            blockHeader4.setVersion(evioVersion);
            blockHeader4.setByteOrder(byteOrder);
            blockHeader4.swapped(swap);

            blockHeader = blockHeader4;

            // Deal with non-standard header lengths here
            int headerLenDiff = blockHeader4.getHeaderLength() - BlockHeaderV4.HEADER_SIZE;
            // If too small, quit with error since headers have a minimum size
            if (headerLenDiff < 0) {
                throw new EvioException("too little data in buffer");
            }

System.out.println("BlockHeader v4:\n" + blockHeader4);
System.out.println();
        }
        else {
            // Cache the starting position
            blockHeader2.setBufferStartingPosition(bufPos);

            // Read the header data
            blockHeader2.setSize            (headerBuf.getInt(bufPos + 4*BlockHeaderV2.EV_BLOCKSIZE));
            blockHeader2.setNumber          (headerBuf.getInt(bufPos + 4*BlockHeaderV2.EV_BLOCKNUM));
            blockHeader2.setHeaderLength    (headerBuf.getInt(bufPos + 4*BlockHeaderV2.EV_HEADERSIZE));
            blockHeader2.setStart           (headerBuf.getInt(bufPos + 4*BlockHeaderV2.EV_START));
            blockHeader2.setEnd             (headerBuf.getInt(bufPos + 4*BlockHeaderV2.EV_END));
            blockHeader2.setVersion         (evioVersion);
            blockHeader2.setReserved1       (headerBuf.getInt(bufPos + 4*BlockHeaderV2.EV_RESERVED1));
            blockHeader2.setMagicNumber     (headerBuf.getInt(bufPos + 4*BlockHeaderV2.EV_MAGIC));

            // set other info
            blockHeader2.setByteOrder(byteOrder);
            blockHeader2.swapped(swap);
            blockHeader = blockHeader2;

System.out.println("BlockHeader v2:\n" + blockHeader2);
        }


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
     * This method can be used to avoid creating additional EvioReader
     * objects by reusing this one with another buffer. The method
     * {@link #close()} is called before anything else.
     *
     * @param buf ByteBuffer to be read
     * @throws IOException   if read failure
     * @throws EvioException if buf is null;
     *                       if first block number != 1 when checkBlkNumSeq arg is true
     */
    public synchronized void setBuffer(ByteBuffer buf) throws EvioException, IOException {
        if (buf == null) {
            throw new EvioException("arg is null");
        }

        parser.close();
        parseFirstHeader(buf);
        parser.setBuffer(buf);
    }

    /**
     * Has {@link #close()} been called (without reopening by calling
     * {@link #setBuffer(ByteBuffer)})?
     *
     * @return {@code true} if this object closed, else {@code false}.
     */
    public synchronized boolean isClosed() { return parser.isClosed(); }

    /**
     * Is this reader checking the block number sequence and
     * throwing an exception is it's not sequential and starting with 1?
     * @return <code>true</code> if checking block number sequence, else <code>false</code>
     */
    public boolean checkBlockNumberSequence() { return parser.checkBlockNumberSequence(); }

    /**
     * Get the byte order of the file/buffer being read.
     * @return byte order of the file/buffer being read.
     */
    public ByteOrder getByteOrder() { return parser.getByteOrder(); }

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
     public String getPath() {
         return parser.getPath();
     }

    /**
     * Get the file/buffer parser.
     * @return file/buffer parser.
     */
    public EventParser getParser() {
        return parser.getParser();
    }

    /**
     * Set the file/buffer parser.
     * @param eventParser file/buffer parser.
     */
    public void setParser(EventParser eventParser) {
        parser.setParser(eventParser);
    }

     /**
     * Get the XML format dictionary is there is one.
     *
     * @return XML format dictionary, else null.
     */
    public String getDictionaryXML() {
        return parser.getDictionaryXML();
    }

    /**
     * Does this evio file have an associated XML dictionary?
     *
     * @return <code>true</code> if this evio file has an associated XML dictionary,
     *         else <code>false</code>
     */
    public boolean hasDictionaryXML() { return parser.hasDictionaryXML(); }

    /**
     * Get the number of events remaining in the file.
     *
     * @return number of events remaining in the file
     * @throws IOException if failed file access
     * @throws EvioException if failed reading from coda v3 file
     */
    public int getNumEventsRemaining() throws IOException, EvioException {
        return parser.getNumEventsRemaining();
    }

    /**
     * Get the byte buffer being read directly or corresponding to the event file.
     * Not a very useful method. For files, it works only for evio format versions 2,3 and
     * returns the internal buffer containing an evio block if using sequential access
     * (for example files &gt; 2.1 GB). It returns the memory mapped buffer otherwise.
     * For reading buffers it returns the buffer being read.
     * @return the byte buffer being read (in certain cases).
     */
    public ByteBuffer getByteBuffer() {
        return parser.getByteBuffer();
    }

    /**
     * Get the size of the file being read, in bytes.
     * For small files, obtain the file size using the memory mapped buffer's capacity.
     * @return the file size in bytes
     */
    public long fileSize() {
        return parser.fileSize();
    }


    /**
     * This returns the FIRST block (physical record) header.
     *
     * @return the first block header.
     */
    public IBlockHeader getFirstBlockHeader() {
        return parser.getFirstBlockHeader();
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
    public EvioEvent getEvent(int index)
            throws IOException, EvioException {

       return parser.getEvent(index);
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
	public synchronized EvioEvent parseEvent(int index) throws IOException, EvioException {
		return parser.parseEvent(index);
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
    public synchronized EvioEvent nextEvent() throws IOException, EvioException {
        return parser.nextEvent();
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
	public synchronized EvioEvent parseNextEvent() throws IOException, EvioException {
		return parser.parseNextEvent();
	}

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
	public void parseEvent(EvioEvent evioEvent) throws EvioException {
        // This method is synchronized too
		parser.getParser().parseEvent(evioEvent);
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
    public synchronized void rewind() throws IOException, EvioException {
        parser.rewind();
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
	public synchronized long position() throws IOException, EvioException {
        return parser.position();
	}

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
	public synchronized void position(long position) throws IOException, EvioException  {
        parser.position(position);
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
        parser.close();
    }

	/**
	 * This returns the current (active) block (physical record) header.
     * Since most users have no interest in physical records, this method
     * should not be used. Mostly it is used by the test programs in the
	 * <code>EvioReaderTest</code> class.
	 *
	 * @return the current block header.
	 */
	public IBlockHeader getCurrentBlockHeader() {
		return parser.getCurrentBlockHeader();
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
    public EvioEvent gotoEventNumber(int evNumber) throws IOException, EvioException {
        return parser.gotoEventNumber(evNumber);
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
    private synchronized EvioEvent gotoEventNumber(int evNumber, boolean parse)
            throws IOException, EvioException {
        return parser.gotoEventNumber(evNumber, parse);
	}

    /**
     * Rewrite the file to XML (not including dictionary).
     *
     * @param path the path to the XML file.
     * @throws IOException   if failed file access
     * @throws EvioException if object closed
     * @throws XMLStreamException if XML issues
     */
    public void toXMLFile(String path)
            throws IOException, EvioException, XMLStreamException {
        parser.toXMLFile(path);
    }

    /**
     * Rewrite the file to XML (not including dictionary).
     *
     * @param path the path to the XML file.
     * @param hex if true, ints get displayed in hexadecimal
     *
     * @throws IOException   if failed file access
     * @throws EvioException if object closed
     * @throws XMLStreamException if XML issues
     */
    public void toXMLFile(String path, boolean hex)
            throws IOException, EvioException, XMLStreamException {
        parser.toXMLFile(path, hex);
    }

    /**
     * Rewrite the file to XML (not including dictionary).
	 *
	 * @param path the path to the XML file.
	 * @param progressListener and optional progress listener, can be <code>null</code>.
     *
     * @throws IOException   if failed file access
     * @throws EvioException if object closed
     * @throws XMLStreamException if XML issues
     * @see IEvioProgressListener
	 */
	public void toXMLFile(String path, IEvioProgressListener progressListener)
                throws IOException, EvioException, XMLStreamException {
        parser.toXMLFile(path, progressListener);
    }

    /**
     * Rewrite the file to XML (not including dictionary).
	 *
	 * @param path the path to the XML file.
	 * @param progressListener and optional progress listener, can be <code>null</code>.
     * @param hex if true, ints get displayed in hexadecimal
     *
     * @throws IOException   if failed file access
     * @throws EvioException if object closed
     * @throws XMLStreamException if XML issues
     * @see IEvioProgressListener
	 */
	public synchronized void toXMLFile(String path,
                                        IEvioProgressListener progressListener,
                                        boolean hex)
                throws IOException, EvioException, XMLStreamException {

        parser.toXMLFile(path, progressListener, hex);
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
    public synchronized int getEventCount() throws IOException, EvioException {
        return  parser.getEventCount();
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
    public synchronized int getBlockCount() throws EvioException{
        return parser.getBlockCount();
    }


}