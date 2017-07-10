package org.jlab.coda.jevio;

import java.io.*;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;

/**
 * This is <b>not</b> a class that will be directly needed by the user.
 * It does the real work of reading an evio version 2 or 3 format
 * file or buffer in a random access manner.
 * The <code>EvioReader</code> class will use this one underneath.
 *
 * @author timmer 6/29/2017
 */
class EvioParserV2Ra extends EvioParserAbstract {

     /**
      * Constructor for reading an version 2 or 3 evio file using a memory-mapped buffer.
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
     EvioParserV2Ra(File ffile, FileChannel fileChannel, boolean checkBlkNumSeq,
                           BlockHeaderV2 blockHeader)
             throws EvioException, IOException {

         evioVersion = 2;
         checkBlockNumberSequence = checkBlkNumSeq;
         sequentialRead = false;
         initialPosition = 0;

         // The EvioCompressedReader object that calls this method,
         // parses the first header and stores that info in the
         // blockHeader object.
         this.blockHeader = blockHeader2 = blockHeader;
         firstBlockHeader = new BlockHeaderV2(blockHeader);
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

         byteBuffer = fileChannel.map(FileChannel.MapMode.READ_ONLY, 0L, fileSize);
         byteBuffer.order(byteOrder);
         prepareForBufferRead(byteBuffer);
//System.out.println("Memory Map versions 2,3");
     }


    /**
     * Constructor for reading an version 2 or 3 evio buffer.
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
    EvioParserV2Ra(ByteBuffer buffer, boolean checkBlkNumSeq,
                          BlockHeaderV2 blockHeader)
            throws EvioException, IOException {

        if (buffer == null) {
            throw new EvioException("Buffer arg is null");
        }

        evioVersion = 2;
        sequentialRead = false;
        checkBlockNumberSequence = checkBlkNumSeq;

        // Remove necessity to track initial position
        byteBuffer = buffer.slice();
        this.blockHeader = blockHeader2 = blockHeader;
        firstBlockHeader = new BlockHeaderV2(blockHeader);
        // Store this for later regurgitation of blockCount
        firstBlockSize = 4*blockHeader.getSize();

        // Setting the byte order is only necessary if someone hands
        // this method a buffer in which the byte order is improperly set.
        byteBuffer.order(byteOrder);
        prepareForBufferRead(byteBuffer);

        eventParser = new EventParser();
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

        lastBlock           =  false;
        eventNumber         =  0;
        blockCount          = -1;
        eventCount          = -1;
        blockNumberExpected =  1;
        dictionaryXML       =  null;
        initialPosition    =  buf.position();
        byteBuffer          =  buf.slice();
        sequentialRead      = false;

        byteBuffer.order(byteOrder);
        prepareForBufferRead(byteBuffer);

        closed = false;
    }


     /** {@inheritDoc} */
    public String getDictionaryXML() {
        return null;
    }

    /** {@inheritDoc} */
    public boolean hasDictionaryXML() {
        return false;
    }


    /**
     * Reads the block (physical record).
     * Assumes mapped buffer or file is positioned at start of the next block header.
     * If a sequential file:
     *   version 4,   Read the entire next block into internal buffer.
     *   version 1-3, If unused data still exists in internal buffer, don't
     *                read anymore in right now as there is at least 1 block there
     *                (integral # of blocks read in).
     *                If no data in internal buffer read DEFAULT_READ_BYTES or the
     *                rest of the file, whichever is smaller, into the internal buffer.
     *
     * By the time this is called, the version #, byte order, and if data is compressed
     * have already been determined. Not necessary to do that
     * for each block header that's read. Called from synchronized method.<p>
     *
     * A Bank header is usually 8, 32-bit ints. The first int is the size of the block in ints
     * (not counting the length itself, i.e., the number of ints to follow).
     *
     * Most users should have no need for this method, since most applications do not
     * care about the block (physical record) structure.
     *
     * @return status of read attempt
     * @throws IOException if file access problems, not enough data in file,
     *                     evio format problems
     */
    private ReadStatus processNextBlock() throws IOException {

        // We already read the last block header
        if (lastBlock) {
            return ReadStatus.END_OF_FILE;
        }

        try {
            if (byteBuffer.remaining() < 32) {
                byteBuffer.clear();
                return ReadStatus.END_OF_FILE;
            }

            // Record starting position
            blockHeader.setBufferStartingPosition(byteBuffer.position());

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


    /** {@inheritDoc} */
    public EvioEvent getEvent(int index)
            throws IOException, EvioException {

        if (index < 1) {
            throw new EvioException("index arg starts at 1");
        }

        // Do not fully parse events up to index_TH event
        return gotoEventNumber(index, false);
    }


    /** {@inheritDoc} */
    public synchronized EvioEvent nextEvent() throws IOException, EvioException {

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

        // Now we should be good to go, except data may cross block boundary.
        // In any case, should be able to read the rest of the header.

        // Read and parse second header word
        int word = byteBuffer.getInt();
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

        blockBytesRemaining -= 4; // just read in 4 bytes

        // get the raw data
        int eventDataSizeBytes = 4*(length - 1);

        try {
            byte bytes[] = new byte[eventDataSizeBytes];

            int bytesToGo = eventDataSizeBytes;
            int offset = 0;

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

    /** {@inheritDoc} */
    public synchronized void rewind() throws IOException, EvioException {
        if (closed) {
            throw new EvioException("object closed");
        }

        byteBuffer.position(initialPosition);
        prepareForBufferRead(byteBuffer);

        lastBlock = false;
        eventNumber = 0;
        blockNumberExpected = 1;

        blockHeader = blockHeader2 = new BlockHeaderV2((BlockHeaderV2) firstBlockHeader);

        blockHeader.setBufferStartingPosition(initialPosition);
	}

    /** {@inheritDoc} */
	public synchronized long position() throws IOException, EvioException {

        if (closed) {
            throw new EvioException("object closed");
        }

		return byteBuffer.position();
	}

    /** {@inheritDoc} */
	public synchronized void position(long position) throws IOException, EvioException  {

        if (closed) {
            throw new EvioException("object closed");
        }

        byteBuffer.position((int)position);
	}

    /** {@inheritDoc} */
    public synchronized void close() throws IOException {
        if (closed) {
            return;
        }

        byteBuffer.position(initialPosition);

        closed = true;
    }


    /** {@inheritDoc} */
    public synchronized int getEventCount() throws IOException, EvioException {

        if (closed) {
            throw new EvioException("object closed");
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

            // Restore our original settings
            restoreState(state);
        }

        return eventCount;
    }

    /** {@inheritDoc} */
    public synchronized int getBlockCount() throws EvioException{

        if (closed) {
            throw new EvioException("object closed");
        }

        if (blockCount < 0) {
            // Although block size is theoretically adjustable, I believe
            // that everyone used 8192 words for the block size in version 3.
            blockCount = (int) (fileSize/firstBlockSize);
        }

        return blockCount;
    }


}