package org.jlab.coda.jevio;

import net.jpountz.lz4.LZ4Factory;
import net.jpountz.lz4.LZ4FastDecompressor;


import java.io.*;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;

/**
 * This is <b>not</b> a class that will be directly needed by the user.
 * It does the real work of sequentially reading an evio version 4 format
 * file which has been compressed using an LZ4 library.
 * The <code>EvioReader</code> class will use this one underneath.
 *
 * @author timmer 6/29/2017
 */
class EvioParserV4Lz4 extends EvioParserAbstract {

    /** Object used to decompress data. */
    private LZ4FastDecompressor lz4Decomp;


    /**
     * Constructor for sequentially reading an version 4 evio file with compressed data.
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
    EvioParserV4Lz4(File ffile, FileChannel fileChannel, boolean checkBlkNumSeq,
                           BlockHeaderV4 blockHeader)
            throws EvioException, IOException {

        evioVersion = 4;
        checkBlockNumberSequence = checkBlkNumSeq;
        // It doesn't make any sense to memory map a compressed file
        sequentialRead = true;
        initialPosition = 0;

        // If we're here, the EvioReader has already read the first 32 bytes of the first
        // block header. Prepare to read following block headers into this buffer.
        headerBuf = ByteBuffer.allocate(32);
        headerBuf.order(byteOrder);

        // The EvioCompressedReader object that calls this method,
        // parses the first header and stores that info in the
        // blockHeader object.
        this.blockHeader = blockHeader4 = blockHeader;
        firstBlockHeader = new BlockHeaderV4(blockHeader);

        System.out.println("EvioParserV4Lz4: FIRST block header = \n" + firstBlockHeader);
        // Store this for later regurgitation of blockCount
        firstBlockSize = 4*blockHeader.getSize();
        // Extract a couple things for our use
        byteOrder = blockHeader.getByteOrder();
        swap = blockHeader.isSwapped();
        hasDictionary = blockHeader.hasDictionary();

        // The file channel has already had 8 words of the header read from it
        // by EvioCompressedReader class. Thus file position is at 32.
        this.fileChannel = fileChannel;
        fileSize = fileChannel.size();
        path = ffile.getAbsolutePath();

        eventParser = new EventParser();

        // This is a compressed file in lz4 format
        compressedInput = true;
        lz4Decomp = LZ4Factory.fastestInstance().fastDecompressor();

        // Create this for reading large block headers
        // containing event indexes in the future.
        indexBuf = ByteBuffer.allocate(4096);

        prepareForCompressedRead();

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
        // Evio format version 4 or greater has a large enough default block size
        // so that reading a single block at a time is not inefficient.

        // For compressed data, don't read the header, just the data
        int bytesToRead = 4*(firstBlockHeader.getSize() - firstBlockHeader.getHeaderLength());

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
        //prepareForBufferRead(byteBuffer);
    }


    /**
     * Reads compressed block of data into a buffer and
     * gets the buffer ready for first-time read.
     * 
     * @throws IOException if file access problems
     */
    private void prepareForCompressedRead() throws IOException {

        // When decompressing (only version 4 & later), read in a single block's
        // data since headers are not compressed.

        // Number of compressed bytes to read
        int compressedBytes = blockHeader4.getCompressedLength();

        // Prepare the ByteBuffer in which to read compressed data
        int compBufSize = (4*EventWriter.DEFAULT_BLOCK_SIZE > compressedBytes + 3) ?
                          (4*EventWriter.DEFAULT_BLOCK_SIZE) : compressedBytes + 3;
        if (compressedBuffer == null || compressedBuffer.capacity() < compBufSize) {
            compressedBuffer = ByteBuffer.allocate(compBufSize);
        }

        // We round the compressed data up to 4 byte boundaries. So be
        // sure to read it all.
        int allBytes = (((compressedBytes + 3) >>> 2) << 2);

 System.out.println("prepareForCompressedRead: READING EXTRA bytes = " + (allBytes - compressedBytes));
        compressedBuffer.clear().limit(allBytes);
//System.out.println("prepareForCompressedRead: compBuf = " + compBufSize + " bytes, " +
//                   ", lim (comp bytes) = " + compressedBytes);

        // The EvioReader object has already read in the first 32 bytes
        // so we are in the correct position to read any index array
        // that's part of the first block header.
        if (blockHeader4.hasEventLengths()) {
            // TODO: something here
        }

        // Position channel after first header
        fileChannel.position(4*blockHeader4.getHeaderLength());

        // Read the first chunk of compressed data from file
        // Read the entire block of data
        int bytesActuallyRead = fileChannel.read(compressedBuffer);
        while (bytesActuallyRead < compressedBytes) {
            bytesActuallyRead += fileChannel.read(compressedBuffer);
        }

        // Get it ready to read for decompressing
        compressedBuffer.flip();
        // Be sure not to read the possible few extra bytes at the end
        compressedBuffer.limit(compressedBytes);

//System.out.println("prepareForCompressedRead: read in compressed block, header len = " +
//                   (4*blockHeader4.getHeaderLength()));

        // Get a buffer ready to receive uncompressed data
        int uncompressedBytes = 4*(blockHeader4.getSize() - blockHeader4.getHeaderLength());

        if (byteBuffer == null || byteBuffer.capacity() < uncompressedBytes) {
            int bufSize  = (4*EventWriter.DEFAULT_BLOCK_SIZE > uncompressedBytes) ?
                           (4*EventWriter.DEFAULT_BLOCK_SIZE) : uncompressedBytes;

            byteBuffer = ByteBuffer.allocate(bufSize);
            byteBuffer.order(byteOrder);
        }
        byteBuffer.clear().limit(uncompressedBytes);

        // Do the actual decompression
        lz4Decomp.decompress(compressedBuffer, byteBuffer);

//System.out.println("prepareForCompressedRead: decompressed block into " +
//                                   uncompressedBytes + " bytes");
        // Position buffer properly for reading
        byteBuffer.flip();
    }



    /**
     * Reads next compressed block of data into a buffer and
     * gets the buffer ready for more reading.
     *
     * @return status of read attempt, SUCCESS or EOF
     * @throws IOException if file read problems, too little data in file
     * @throws EvioException if header data is not in proper evio format
     */
    private ReadStatus getNextCompressedBlock() throws EvioException, IOException {
//System.out.println("getNextCompressedBlock: in");
        // Enough data left to read 8 header words?
        long bytesLeft = fileSize - fileChannel.position();
        if (bytesLeft <= 32L) {
            // If no data, there's no ending header.
            // If 32 bytes left, only a single header is left and no data.
            if (bytesLeft == 0L || bytesLeft == 32L) {
                return ReadStatus.END_OF_FILE;
            }
            throw new IOException("not enough data to read block header");
        }

        // Read first 8 words of header
        headerBuf.clear();
        int bytesActuallyRead = fileChannel.read(headerBuf);
        while (bytesActuallyRead < 32) {
            bytesActuallyRead += fileChannel.read(headerBuf);
        }
        bytesLeft -= bytesActuallyRead;

        Utilities.printBuffer(headerBuf, 0, 8, "HEADER");

        // Parse the header data. Don't bother setting version or reserved2
        blockHeader4.setSize(headerBuf.getInt(0));
        blockHeader4.setNumber(headerBuf.getInt(4));
        blockHeader4.setHeaderLength(headerBuf.getInt(8));
        blockHeader4.setEventCount(headerBuf.getInt(12));
        blockHeader4.setCompressedLength(headerBuf.getInt(16));
        blockHeader4.parseToBitInfo(headerBuf.getInt(20));
        blockHeader4.setMagicNumber(headerBuf.getInt(28));
        lastBlock = blockHeader4.getBitInfo(1);

        int headerWords = blockHeader4.getHeaderLength();
        int compressedBytes = blockHeader4.getCompressedLength();
        int uncompressedBytes = 4*(blockHeader4.getSize() - headerWords);

        // If header is over-sized, read the rest of it (indexes of events in block)
        if (headerWords > BlockHeaderV4.HEADER_SIZE) {
            // Bytes in header after initial 8 words
            int restOfHeaderBytes = 4*headerWords - 32;

            // Check to see if we have enough data
            if (bytesLeft <= restOfHeaderBytes) {
                lastBlock = true;
                // If header is there, but no more data beyond, EOF
                if (bytesLeft == restOfHeaderBytes) {
                    return ReadStatus.END_OF_FILE;
                }
                throw new IOException("not enough data to read block header");
            }

            // Make sure we have room in buffer
            if (restOfHeaderBytes > indexBuf.capacity()) {
                // Give it a little extra
                indexBuf = ByteBuffer.allocate(restOfHeaderBytes + 1000);
            }
            indexBuf.clear().limit(restOfHeaderBytes);

            // Read in rest of header
            bytesActuallyRead = fileChannel.read(indexBuf);
            while (bytesActuallyRead < restOfHeaderBytes) {
                bytesActuallyRead += fileChannel.read(indexBuf);
            }

            bytesLeft -= bytesActuallyRead;
        }

//System.out.println("getNextCompressedBlock: block has " + compressedBytes + " compr bytes, " +
//                   uncompressedBytes + " uncompr data bytes");

        // Is there enough data left to read data in block?
        if (bytesLeft < compressedBytes) {
            throw new IOException("not enough data to read block");
        }

        // We round the compressed data up to 4 byte boundaries. So be
        // sure to read it all.
        int allBytes = (((compressedBytes + 3) >>> 2) << 2);

 System.out.println("READING EXTRA bytes = " + (allBytes - compressedBytes));

        // Make sure buffer can hold the entire block of compressed data.
        if (compressedBuffer.capacity() < allBytes) {
            // Make this bigger than necessary so we're not constantly reallocating
            compressedBuffer = ByteBuffer.allocate(allBytes + 10000);
        }
        compressedBuffer.clear().limit(allBytes);

        // Read the entire block of data
        bytesActuallyRead = fileChannel.read(compressedBuffer);
        while (bytesActuallyRead < compressedBytes) {
            bytesActuallyRead += fileChannel.read(compressedBuffer);
        }

        compressedBuffer.flip();
        // Be sure not to read the possible few extra bytes at the end
        compressedBuffer.limit(compressedBytes);

        // Now for the uncompressed data
        if (byteBuffer.capacity() < uncompressedBytes) {
            byteBuffer = ByteBuffer.allocate(uncompressedBytes + 10000);
            byteBuffer.order(byteOrder);
        }
        byteBuffer.clear().limit(uncompressedBytes);

        // Do the actual decompression
        lz4Decomp.decompress(compressedBuffer, byteBuffer);

        // Position buffer properly for reading
        byteBuffer.flip();

        // Now keeping track of pos in this new blockBuffer
        blockHeader.setBufferStartingPosition(0);

        return ReadStatus.SUCCESS;
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
     * @throws EvioException if header data is not in proper evio format
     */
    private ReadStatus processNextBlock() throws IOException, EvioException {

System.out.println("     PROCESS NEXT BLOCK");
        // We already read the last block header
        if (lastBlock) {
            return ReadStatus.END_OF_FILE;
        }

        try {
            // Handle decompressed data here
            if (getNextCompressedBlock() == ReadStatus.END_OF_FILE) {
                return ReadStatus.END_OF_FILE;
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

        // How many data bytes remain in this block until we reach the next block header?
        // Remember, for compressed data, the header is not included in byteBuffer.
        int blockBytesRemaining = blockDataBytesRemaining();
//System.out.println("nextEvent: block bytes remaining = " + blockBytesRemaining +
//                   " cur buf pos = " + currentPosition);

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

        // Once here, we are assured the entire next event is in this block
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

            int bytesToGo = eventDataSizeBytes;
            int offset = 0;

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
        System.out.println("REWIND");
        lastBlock = false;
        eventNumber = 0;
        blockNumberExpected = 1;
        blockHeader = blockHeader4 = new BlockHeaderV4((BlockHeaderV4) firstBlockHeader);

        prepareForCompressedRead();

        if (hasDictionaryXML()) {
            // Dictionary is always the first event so skip over it.
            // For sequential reads, do this after each rewind.
            nextEvent();
        }
    }

    /** {@inheritDoc} */
	public synchronized long position() throws IOException, EvioException {
        if (closed) {
            throw new EvioException("object closed");
        }

        return fileChannel.position();
	}

    /** {@inheritDoc} */
	public synchronized void position(long position) throws IOException, EvioException  {
        if (closed) {
            throw new EvioException("object closed");
        }

        fileChannel.position(position);
 	}

    /** {@inheritDoc} */
    public synchronized void close() throws IOException {
        if (closed) {
            return;
        }

        fileChannel.close();
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
//System.out.println("ev count = " + eventCount);
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

    /** {@inheritDoc} */
    public synchronized int getBlockCount() throws EvioException{

        if (closed) {
            throw new EvioException("object closed");
        }

        if (blockCount < 0) {
            // Although block size is theoretically adjustable, I believe
            // that everyone used 8192 words for the block size in version 3.
            // TODO: This looks like an error !!!
            blockCount = (int) (fileSize/firstBlockSize);
        }

        return blockCount;
    }


}