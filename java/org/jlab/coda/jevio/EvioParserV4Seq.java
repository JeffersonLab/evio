package org.jlab.coda.jevio;


import java.io.*;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;

/**
 * This is <b>not</b> a class that will be directly needed by the user.
 * It does the real work of reading an evio version 4 format
 * file in a sequential manner.
 * The <code>EvioReader</code> class will use this one underneath.
 *
 * @author timmer 6/29/2017
 */
class EvioParserV4Seq extends EvioParserAbstract {

     /**
      * Constructor for reading an version 4 evio file sequentially.
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
     EvioParserV4Seq(File ffile, FileChannel fileChannel, boolean checkBlkNumSeq,
                            BlockHeaderV4 blockHeader)
             throws EvioException, IOException {

         evioVersion = 4;
         checkBlockNumberSequence = checkBlkNumSeq;
         sequentialRead = true;
         initialPosition = 0;

         headerBuf = ByteBuffer.allocate(32);
         headerBuf.order(byteOrder);

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

         // TODO: THIS IS A NO-NO! This moves the disk head backwards, but only once per file
         fileChannel.position(0);
         eventParser = new EventParser();

         prepareForSequentialRead();

         if (blockHeader4.hasDictionary()) {
             // Dictionary is always the first event
             EvioEvent dict = parseNextEvent();
             if (dict != null) {
                 String[] strs = dict.getStringData();
                 dictionaryXML = strs[0];
             }
         }

     }


    /**
     * Reads file data into a buffer and gets the buffer ready for first-time read.
     * @throws IOException if file access problems
     */
    private void prepareForSequentialRead() throws IOException {
        // Create a buffer to hold a chunk of data.
        int bytesToRead;

        // Evio format version 4 or greater has a large enough default block size
        // so that reading a single block at a time is not inefficient.
        bytesToRead = 4*firstBlockHeader.getSize();

        if (byteBuffer == null || byteBuffer.capacity() < bytesToRead) {
            byteBuffer = ByteBuffer.allocate(bytesToRead);
            byteBuffer.order(byteOrder);
        }
        byteBuffer.clear().limit(bytesToRead);

        // Read the first chunk of data from file
        fileChannel.read(byteBuffer);
        // Get it ready for reading from internal buffer
        byteBuffer.flip();

        // Position buffer properly (past block header)
        prepareForBufferRead(byteBuffer);
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

            // Enough data left to read len?
            if (fileSize - fileChannel.position() < 4L) {
                return ReadStatus.END_OF_FILE;
            }

            // Read len of block in 32 bit words
            headerBuf.position(0).limit(4);
            int bytesActuallyRead = fileChannel.read(headerBuf);
            while (bytesActuallyRead < 4) {
                fileChannel.read(headerBuf);
            }
            int blkSize = headerBuf.getInt(0);

            if (swap) blkSize = Integer.reverseBytes(blkSize);
            // Change to bytes
            int blkBytes = 4 * blkSize;

            // Enough data left to read rest of block?
            if (fileSize - fileChannel.position() < blkBytes - 4) {
                return ReadStatus.END_OF_FILE;
            }

            // Make sure buffer can hold the entire block of data
            if (byteBuffer.capacity() >= blkBytes) {
                byteBuffer.clear();
                byteBuffer.limit(blkBytes);
            }
            else {
                // Make this bigger than necessary so we're not constantly reallocating
                byteBuffer = ByteBuffer.allocate(blkBytes + 10000);
                byteBuffer.limit(blkBytes);
                byteBuffer.order(byteOrder);
            }

            // Read the entire block of data.
            // First put in length we just read.
            byteBuffer.putInt(blkSize);

            // Now the rest of the block (already put int, 4 bytes, in)
            bytesActuallyRead = fileChannel.read(byteBuffer) + 4;
            while (bytesActuallyRead < blkBytes) {
                bytesActuallyRead += fileChannel.read(byteBuffer);
            }

            byteBuffer.flip();
            // Now keeping track of pos in this new blockBuffer
            blockHeader.setBufferStartingPosition(0);


            // Read the header data
            blockHeader4.setSize(byteBuffer.getInt());
            blockHeader4.setNumber(byteBuffer.getInt());
            blockHeader4.setHeaderLength(byteBuffer.getInt());
            blockHeader4.setEventCount(byteBuffer.getInt());
            blockHeader4.setCompressedLength(byteBuffer.getInt());

            // Use 6th word to set bit info
            blockHeader4.parseToBitInfo(byteBuffer.getInt());
            blockHeader4.setVersion(evioVersion);
            lastBlock = blockHeader4.getBitInfo(1);

            blockHeader4.setReserved2(byteBuffer.getInt());
            blockHeader4.setMagicNumber(byteBuffer.getInt());
            blockHeader = blockHeader4;

            // Deal with non-standard header lengths here
            int headerLenDiff = blockHeader4.getHeaderLength() - BlockHeaderV4.HEADER_SIZE;
            // If too small quit with error since headers have a minimum size
            if (headerLenDiff < 0) {
                return ReadStatus.EVIO_EXCEPTION;
            }
            // If bigger, read extra ints
            else if (headerLenDiff > 0) {
                for (int i = 0; i < headerLenDiff; i++) {
                    byteBuffer.getInt();
                }
            }

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

        if (index < 1) {
            throw new EvioException("index arg starts at 1");
        }

        // Do not fully parse events up to index_TH event
        return gotoEventNumber(index, false);
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
        // (This should never happen in version 4).
        else if (blockHeader.getBufferEndingPosition() == currentPosition) {
            return null;
        }

        // Version   4: Once here, we are assured the entire next event is in this block.
        int length;
        length = byteBuffer.getInt();
        if (length < 1) {
            throw new EvioException("non-positive length (0x" + Integer.toHexString(length) + ")");
        }
        header.setLength(length);
        blockBytesRemaining -= 4; // just read in 4 bytes

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

            // Last (perhaps only) read
            byteBuffer.get(bytes, 0, eventDataSizeBytes);
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
        if (closed) {
            throw new EvioException("object closed");
        }

        fileChannel.position(initialPosition);
        prepareForSequentialRead();

        eventNumber = 0;
        blockNumberExpected = 1;

        blockHeader = blockHeader4 = new BlockHeaderV4((BlockHeaderV4) firstBlockHeader);
        lastBlock = blockHeader.isLastBlock();

        blockHeader.setBufferStartingPosition(initialPosition);

        if (hasDictionaryXML()) {
            // Dictionary is always the first event so skip over it.
            // For sequential reads, do this after each rewind.
            nextEvent();
        }
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
        if (closed) {
            throw new EvioException("object closed");
        }

        return fileChannel.position();
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
        if (closed) {
            throw new EvioException("object closed");
        }

        fileChannel.position(position);
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

        fileChannel.close();

        closed = true;
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

            // If sequential access to v2 file, then nextEvent() places
            // new data into byteBuffer. Restoring the original state
            // is useless without also restoring/re-reading the data.
            rewind();

            // Skip dictionary
            if (hasDictionaryXML()) {
                nextEvent();
            }

            // Go back to original event # & therefore buffer data
            for (int i=1; i < state.eventNumber; i++) {
                nextEvent();
            }

            // Restore our original settings
            restoreState(state);
        }

        return eventCount;
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

        if (closed) {
            throw new EvioException("object closed");
        }

        if (blockCount < 0) {
            // Although block size is theoretically adjustable, I believe
            // that everyone used 8192 words for the block size in version 3.

            // TODO: This looks like an error!!!
            blockCount = (int) (fileSize/firstBlockSize);
        }

        return blockCount;
    }


}