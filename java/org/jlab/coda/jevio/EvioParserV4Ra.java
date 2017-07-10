package org.jlab.coda.jevio;

import java.io.*;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;

/**
 * This is <b>not</b> a class that will be directly needed by the user.
 * It does the real work of reading an evio version 4 format
 * file or buffer in a random access manner.
 * The <code>EvioReader</code> class will use this one underneath.
 *
 * @author timmer 6/29/2017
 */
class EvioParserV4Ra extends EvioParserAbstract {

     /**
      * Constructor for reading an version 4 evio file using a memory-mapped buffer.
      * @see EventWriter
      *
      * @param ffile the file that contains events.
      * @param fileChannel FileChannel object associated with file being read.
      * @param checkBlkNumSeq if <code>true</code> check the block number sequence
      *                       and throw an exception if it is not sequential starting
      *                       with 1.
      * @param blockHeader   the first block header.
      *
      * @throws IOException   if read failure
      * @throws EvioException if file arg is null;
      *                       if first block number != 1 when checkBlkNumSeq arg is true
      */
     EvioParserV4Ra(File ffile, FileChannel fileChannel, boolean checkBlkNumSeq,
                           BlockHeaderV4 blockHeader)
             throws EvioException, IOException {

         evioVersion = 4;
         checkBlockNumberSequence = checkBlkNumSeq;
         sequentialRead = false;
         initialPosition = 0;

         // The EvioCompressedReader object that calls this method,
         // parses the first header and stores that info in the
         // blockHeader object.
         this.blockHeader = blockHeader4 = blockHeader;
         firstBlockHeader = new BlockHeaderV4(blockHeader);
         // Store this for later regurgitation of blockCount
         firstBlockSize = 4*blockHeader.getSize();
         // Extract a couple things for our use
         byteOrder = blockHeader.getByteOrder();
         swap = blockHeader.isSwapped();
         
         path = ffile.getAbsolutePath();
         this.fileChannel = fileChannel;
         fileSize = fileChannel.size();

         // TODO: THIS IS A NO-NO !!!! This moves the disk head backwards.
         fileChannel.position(0);
         eventParser = new EventParser();

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


    /**
     * Constructor for reading an version 4 evio buffer.
     * @see EventWriter
     *
     * @param buffer the buffer that contains events.
     * @param checkBlkNumSeq if <code>true</code> check the block number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1
     * @param blockHeader   the first block header.
     *
     * @throws IOException   if read failure
     * @throws EvioException if buffer arg is null;
     *                       if first block number != 1 when checkBlkNumSeq arg is true
     */
    EvioParserV4Ra(ByteBuffer buffer, boolean checkBlkNumSeq,
                          BlockHeaderV4 blockHeader)
            throws EvioException, IOException {

        if (buffer == null) {
            throw new EvioException("Buffer arg is null");
        }

        evioVersion = 4;
        checkBlockNumberSequence = checkBlkNumSeq;
        sequentialRead = false;
        // Remove necessity to track initial position
        byteBuffer = buffer.slice();
        this.blockHeader = blockHeader4 = blockHeader;
        firstBlockHeader = new BlockHeaderV4(blockHeader);
        // Store this for later regurgitation of blockCount
        firstBlockSize = 4*blockHeader.getSize();

        // For the latest evio format, generate a table
        // of all event positions in buffer for random access.

// System.out.println("EvioReader const: evioVersion = " + evioVersion + ", create mem handler");
        mappedMemoryHandler = new MappedMemoryHandler(byteBuffer);
        if (blockHeader4.hasDictionary()) {
            ByteBuffer buf = mappedMemoryHandler.getFirstMap();
            // Jump to the first event
            prepareForBufferRead(buf);
            // Dictionary is the first event
            readDictionary(buf);
        }

        eventParser = new EventParser();
    }

    /** {@inheritDoc} */
    public synchronized void setBuffer(ByteBuffer buf) throws EvioException, IOException {

        lastBlock           =  false;
        eventNumber         =  0;
        blockCount          = -1;
        eventCount          = -1;
        blockNumberExpected =  1;
        dictionaryXML       =  null;
        initialPosition    =  buf.position();
        byteBuffer          =  buf.slice();
        sequentialRead      = false;

        mappedMemoryHandler = new MappedMemoryHandler(byteBuffer);
        if (blockHeader4.hasDictionary()) {
            ByteBuffer bb = mappedMemoryHandler.getFirstMap();
            // Jump to the first event
            prepareForBufferRead(bb);
            // Dictionary is the first event
            readDictionary(bb);
        }

        closed = false;
    }


    /**
     * This method is only called once at the very beginning if buffer is known to have
     * a dictionary. It then reads that dictionary. Only called in format versions 4 & up.
     * Position buffer after dictionary. Called from synchronized method or constructor.
     *
     * @since 4.0
     * @param buffer buffer to read to get dictionary
     * @throws EvioException if failed read due to bad buffer format;
     *                       if version 3 or earlier
     */
     private void readDictionary(ByteBuffer buffer) throws EvioException {

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

    /** {@inheritDoc} */
    public EvioEvent getEvent(int index)
            throws IOException, EvioException {

        if (index < 1) {
            throw new EvioException("index arg starts at 1");
        }

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
        int type = dt & 0x3f;
        int padding = dt >>> 6;
        // If only 7th bit set, that can only be the legacy tagsegment type
        // with no padding information - convert it properly.
        if (dt == 0x40) {
            type = DataType.TAGSEGMENT.getValue();
            padding = 0;
        }
        header.setDataType(type);
        header.setPadding(padding);
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

    /** {@inheritDoc} */
    public synchronized EvioEvent nextEvent() throws IOException, EvioException {
        return getEvent(eventNumber+1);
    }

    /** {@inheritDoc} */
	public void parseEvent(EvioEvent evioEvent) throws EvioException {
        // This method is synchronized too
		eventParser.parseEvent(evioEvent);
	}

    /** {@inheritDoc} */
    public synchronized void rewind() throws IOException, EvioException {
        if (closed) {
            throw new EvioException("object closed");
        }

        lastBlock = false;
        eventNumber = 0;
        blockNumberExpected = 1;

        blockHeader = blockHeader4 = new BlockHeaderV4((BlockHeaderV4) firstBlockHeader);

        blockHeader.setBufferStartingPosition(initialPosition);
	}

    /** {@inheritDoc} */
	public synchronized long position() throws IOException, EvioException {
        return -1L;
	}

    /** {@inheritDoc} */
	public synchronized void position(long position) throws IOException, EvioException  {
        return;
	}

    /** {@inheritDoc} */
    public synchronized void close() throws IOException {
        if (closed) {
            return;
        }

        if (byteBuffer != null) byteBuffer.position(initialPosition);
        mappedMemoryHandler = null;

        if (fileChannel != null) {
            fileChannel.close();
            fileChannel = null;
        }

        closed = true;
        return;
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

        try {
            if (parse) {
                return parseEvent(evNumber);
            }
            else {
                return getEvent(evNumber);
            }
        }
        catch (EvioException e) {
        }
        
        return null;
    }


    /** {@inheritDoc} */
    public synchronized int getEventCount() throws IOException, EvioException {

        if (closed) {
            throw new EvioException("object closed");
        }

        return mappedMemoryHandler.getEventCount();
    }

    /** {@inheritDoc} */
    public synchronized int getBlockCount() throws EvioException{

        if (closed) {
            throw new EvioException("object closed");
        }

        return mappedMemoryHandler.getBlockCount();
    }


}