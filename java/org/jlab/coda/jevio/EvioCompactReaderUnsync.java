/*
 *   Copyright (c) 2017.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.jevio;

import org.jlab.coda.hipo.RecordHeader;

import java.io.File;
import java.io.IOException;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.MappedByteBuffer;
import java.util.List;

/**
 * This class is used to read an evio version 4 formatted file or buffer
 * and extract specific evio containers (bank, seg, or tagseg)
 * with actual data in them given a tag/num pair. It is not thread-safe.
 * It is designed to be fast and does <b>NOT</b> do a full deserialization
 * on each event examined.<p>
 *
 * @deprecated use EvioCompactReader with constructor's sync arg set to false.
 * @author timmer
 */
public class EvioCompactReaderUnsync implements IEvioCompactReader {

    /** Evio version number (1-4, 6) of data in buffer currently being read. */
    private int evioVersion;

    /** For evio version number 6 and above,
     * is the data compressed in buffer currently being read. */
    private boolean isCompressed;

    /**
     * Endianness of the data currently being read, either
     * {@link ByteOrder#BIG_ENDIAN} or
     * {@link ByteOrder#LITTLE_ENDIAN}.
     */
    private ByteOrder byteOrder;

    /** The buffer being read. */
    private final ByteBuffer byteBuffer;

    /** Initial position of buffer (0 if file). */
    private final int initialPosition;

    //----------------------------------------
    // Results of last call to findEvioInfo()
    //----------------------------------------

    /** Evio version number (1-4, 6) found by last call to
     * {@link #findRecordInfo(ByteBuffer)} reading first header. */
    static private int version;

    /** For evio version number 6 and above,
     * is the data compressed, found by last call to
     * {@link #findRecordInfo(ByteBuffer)} reading first header. */
    static private boolean compressed;

    /**
     * Endianness found by last call to
     * {@link #findRecordInfo(ByteBuffer)} reading first header.
     */
    static private ByteOrder order;

    /** Is this the last record in buffer for evio V6? */
    static private boolean isLastRecord;

    /** Length of entire (possibly compressed) record in bytes for evio V6. */
    static private int recordLen;

    /** Length of general record header itself in bytes for evio V6. */
    static private int headerLen;

    /** Length of uncompressed event index array in bytes for evio V6. */
    static private int indexLen;

    /** Length of uncompressed user header in bytes for evio V6. */
    static private int userHdrLen;

    /** Length of uncompressed data in bytes for evio V6. */
    static private int uncompDataLen;

    //------------------------
    // Object to delegate to
    //------------------------
    private final IEvioCompactReader reader;


    /**
     * Constructor for reading a buffer.
     * @param byteBuffer the buffer that contains events.
     * @see EventWriter
     * @throws EvioException if buffer arg is null;
     *                       failure to read first block header
     */
    public EvioCompactReaderUnsync(ByteBuffer byteBuffer)  throws EvioException {
        this(byteBuffer, null);
    }


    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     * @param pool pool of EvioNode objects.
     *
     * @see EventWriter
     * @throws BufferUnderflowException if not enough buffer data;
     * @throws EvioException if buffer arg is null;
     *                       failure to parse first block header;
     *                       unsupported evio version.
     */
    public EvioCompactReaderUnsync(ByteBuffer byteBuffer, EvioNodeSource pool) throws EvioException {

        if (byteBuffer == null) {
            throw new EvioException("Buffer arg is null");
        }

        initialPosition = byteBuffer.position();
        this.byteBuffer = byteBuffer;

        // Read first block header and find the file's endianness & evio version #
        findRecordInfo(byteBuffer);

        // Take results of above method call and associate it with buffer being read
        byteOrder = order;
        evioVersion = version;
        isCompressed = compressed;

        if (evioVersion < 4)  {
            throw new EvioException("unsupported evio version (" + evioVersion + "), only 4+");
        }

        if (evioVersion == 4) {
            reader = new EvioCompactReaderUnsyncV4(byteBuffer, pool);
        }
        else if (evioVersion == 6) {
            reader = new EvioCompactReaderUnsyncV6(byteBuffer, pool);
        }
        else {
            throw new EvioException("unsupported evio version (" + evioVersion + ")");
        }
    }


    /**
     * Reads some data in a record header (located at buffer position)
     * in order to determine things like the evio version of buffer.
     * Does <b>not</b> change the position or limit of buffer.
     * 
     * @param  buf buffer containing evio header to parse.
     * @throws BufferUnderflowException if not enough data in buffer.
     * @throws EvioException null argument or bad magic number in header.
     */
    static private void findRecordInfo(ByteBuffer buf) throws BufferUnderflowException, EvioException {
//System.out.println("findEvioVersion: buf limit = " + buf.limit() +
//                   ", cap = " + buf.capacity() + ", initialPos = " + buf.position());
//Utilities.printBuffer(buf, 0, 40, "buffer");
        if (buf == null) {
            throw new EvioException("null argument");
        }

        // Look at first block header.
        // Have enough remaining bytes to read 8 words of header?
        if (buf.remaining() < 32) {
            throw new BufferUnderflowException();
        }

        // Set the byte order to match the file's ordering.
        int initialPos = buf.position();

        // Check the magic number for endianness (buffer defaults to big endian)
        order = buf.order();

        int magicNumber = buf.getInt(initialPos + RecordHeader.MAGIC_OFFSET);
        if (magicNumber != IBlockHeader.MAGIC_NUMBER) {
            if (order == ByteOrder.BIG_ENDIAN) {
                order = ByteOrder.LITTLE_ENDIAN;
            }
            else {
                order = ByteOrder.BIG_ENDIAN;
            }
            buf.order(order);

            // Reread magic number to make sure things are OK
            magicNumber = buf.getInt(initialPos + RecordHeader.MAGIC_OFFSET);
            if (magicNumber != IBlockHeader.MAGIC_NUMBER) {
                throw new EvioException("magic number is bad, 0x" +  Integer.toHexString(magicNumber));
            }
//Utilities.printBuffer(byteBuffer, 0, 40, "Switch endian ....");
        }

        // Find the version number
        int bitInfo = buf.getInt(initialPos + RecordHeader.BIT_INFO_OFFSET);
        version = bitInfo & RecordHeader.VERSION_MASK;

        // If it's version 6, read whether data is compressed or not
        // and assorted uncompressed lengths.
        if (version > 5) {
            if (buf.remaining() < 40) {
                throw new BufferUnderflowException();
            }

            int compressionType = buf.getInt(initialPos + RecordHeader.COMPRESSION_TYPE_OFFSET) >>> 28;
            compressed    = (compressionType == 0);

            recordLen     = buf.getInt(initialPos + RecordHeader.RECORD_LENGTH_OFFSET) * 4;
            headerLen     = buf.getInt(initialPos + RecordHeader.HEADER_LENGTH_OFFSET) * 4;
            indexLen      = buf.getInt(initialPos + RecordHeader.INDEX_ARRAY_OFFSET);
            userHdrLen    = buf.getInt(initialPos + RecordHeader.USER_LENGTH_OFFSET);
            uncompDataLen = buf.getInt(initialPos + RecordHeader.UNCOMPRESSED_LENGTH_OFFSET);
            isLastRecord  = RecordHeader.isLastRecord(bitInfo);
        }
        else {
            compressed = isLastRecord = false;
            recordLen = headerLen = indexLen = userHdrLen = uncompDataLen = 0;
        }
    }

    /**
     * This method finds out whether the given buffer contains compressed data or not.
     *
     * @param src  ByteBuffer containing evio/hipo data
     * @return {@code true} if the data in the source buffer was compressed,
     *         else {@code false}.
     * @throws EvioException if arg is null;
     *                       if failure to read first block header
     */
    public boolean isCompressed(ByteBuffer src) throws EvioException {
        findRecordInfo(src);
        return compressed;
    }

    /**
     * This method ensures that the given buffer has sufficient capacity to
     * hold its current contents while uncompressed. If the first record header
     * indicates that there is no compressed data, the given buffer is returned as is.
     * If there is insufficient memory to hold the uncompressed data, a bigger buffer
     * is allocated, current contents are copied into it, and the new buffer is the
     * return value of this method.<p>
     *
     * @param buf  ByteBuffer containing evio/hipo data
     * @return if buf is not compressed, buf is returned. If buf is too small to hold
     *         uncompressed data, a bigger buffer is created, data is copied into the
     *         new buffer, and the new buffer is returned.
     * @throws BufferUnderflowException if not enough data in buffer.
     * @throws EvioException if arg is null; if failure to read record header.
     */
    public static ByteBuffer ensureUncompressedCapacity(ByteBuffer buf)
                            throws EvioException, BufferUnderflowException {

        if (buf == null) throw new EvioException("null arg");
        int srcPos = buf.position();
        int srcPosOrig = srcPos;
        int recordBytes, totalBytes = 0, totalCompressed = 0;

        ByteBuffer biggerBuf;

        do {
            // Look at the record
            findRecordInfo(buf);

            // If there's no data compression, we don't need to expand buffer.
            // This assumes all records have the same compression.
            if (!compressed) return buf;

            // Total uncompressed length of record
            recordBytes = headerLen + indexLen + userHdrLen + uncompDataLen;

           // Track total uncompressed & compressed sizes
            totalBytes += recordBytes;
            totalCompressed += recordLen;

            // Hop over record
            srcPos += recordLen;
            buf.position(srcPos);

        } while (!isLastRecord); // Go to the next record if any

        buf.position(srcPosOrig);

        // If we've got enough room to handle all the uncompressed data ...
        if (totalBytes <= (buf.capacity() - srcPos)) {
            // TODO: should we do this?
            //buf.limit(totalCompressed + srcPosOrig);
            return buf;
        }

        // Give buffer an extra 2KB
        if (buf.isDirect()) {
            biggerBuf = ByteBuffer.allocateDirect(totalBytes + srcPos + 2048);
            biggerBuf.order(order).limit(srcPosOrig + totalCompressed);
            // Copy over all data in buf
            biggerBuf.put(buf);
            // Set back to original pos
            buf.position(srcPosOrig);
            // Prepare to be able to write into
            biggerBuf.clear();
        }
        else {
            // Backed by array
            biggerBuf = ByteBuffer.allocate(totalBytes + srcPos + 2048);
            biggerBuf.order(order);
            // Copy over all data in buf
            System.arraycopy(buf, buf.position(), biggerBuf, 0, totalCompressed);
        }

        return biggerBuf;
    }

    /** {@inheritDoc} */
    public boolean isFile() {return reader.isFile();}

    /** {@inheritDoc} */
    public boolean isCompressed() {return reader.isCompressed();}

    /** {@inheritDoc} */
    public void setBuffer(ByteBuffer buf) throws EvioException {reader.setBuffer(buf);}

    /** {@inheritDoc} */
    public void setBuffer(ByteBuffer buf, EvioNodeSource pool) throws EvioException {
        reader.setBuffer(buf, pool);
    }

    /** {@inheritDoc} */
    public ByteBuffer setCompressedBuffer(ByteBuffer buf, EvioNodeSource pool) throws EvioException {
        return reader.setCompressedBuffer(buf, pool);
    }

    /** {@inheritDoc} */
    public synchronized boolean isClosed() {return reader.isClosed();}

    /** {@inheritDoc} */
    public ByteOrder getByteOrder() {return reader.getByteOrder();}

    /** {@inheritDoc} */
    public int getEvioVersion() {return evioVersion;}

    /** {@inheritDoc} */
     public String getPath() {return reader.getPath();}

    /** {@inheritDoc} */
    public ByteOrder getFileByteOrder() {return reader.getFileByteOrder();}

    /** {@inheritDoc} */
    public synchronized String getDictionaryXML() throws EvioException {
        return reader.getDictionaryXML();
    }

    /** {@inheritDoc} */
    public synchronized EvioXMLDictionary getDictionary() throws EvioException {
        return reader.getDictionary();
    }

    /** {@inheritDoc} */
    public boolean hasDictionary() {return reader.hasDictionary();}

    /** {@inheritDoc} */
    public MappedByteBuffer getMappedByteBuffer() {return reader.getMappedByteBuffer();}

    /** {@inheritDoc} */
    public ByteBuffer getByteBuffer() {return reader.getByteBuffer();}

    /** {@inheritDoc} */
    public long fileSize() {return reader.fileSize();}


    /** {@inheritDoc} */
    public EvioNode getEvent(int eventNumber) {return reader.getEvent(eventNumber);}

    /** {@inheritDoc} */
    public EvioNode getScannedEvent(int eventNumber) {return reader.getScannedEvent(eventNumber);}

    /** {@inheritDoc} */
    public EvioNode getScannedEvent(int eventNumber, EvioNodeSource nodeSource) {
        return reader.getScannedEvent(eventNumber, nodeSource);
    }

    /** {@inheritDoc} */
    public IBlockHeader getFirstBlockHeader() {return reader.getFirstBlockHeader();}

    /** {@inheritDoc} */
    public synchronized List<EvioNode> searchEvent(int eventNumber, int tag, int num)
                                 throws EvioException {
        return reader.searchEvent(eventNumber, tag, num);
    }

    /** {@inheritDoc} */
    public synchronized List<EvioNode> searchEvent(int eventNumber, String dictName,
                                                   EvioXMLDictionary dictionary)
                                 throws EvioException {

        return reader.searchEvent(eventNumber, dictName, dictionary);
    }

    /** {@inheritDoc} */
    public synchronized ByteBuffer removeEvent(int eventNumber) throws EvioException {
        return reader.removeEvent(eventNumber);
    }

    /** {@inheritDoc} */
    public synchronized ByteBuffer removeStructure(EvioNode removeNode) throws EvioException {
        return reader.removeStructure(removeNode);
    }

    /** {@inheritDoc} */
    public synchronized ByteBuffer addStructure(int eventNumber, ByteBuffer addBuffer) throws EvioException {
        return reader.addStructure(eventNumber, addBuffer);
    }

    /** {@inheritDoc} */
    public ByteBuffer getData(EvioNode node) throws EvioException {
        return reader.getData(node);
    }

    /** {@inheritDoc} */
    public synchronized ByteBuffer getData(EvioNode node, boolean copy)
                            throws EvioException {
        return reader.getData(node, copy);
    }

    /** {@inheritDoc} */
    public ByteBuffer getEventBuffer(int eventNumber) throws EvioException {
        return reader.getEventBuffer(eventNumber);
    }

    /** {@inheritDoc} */
    public synchronized ByteBuffer getEventBuffer(int eventNumber, boolean copy)
                            throws EvioException {
        return reader.getEventBuffer(eventNumber, copy);
    }

    /** {@inheritDoc} */
    public ByteBuffer getStructureBuffer(EvioNode node) throws EvioException {
        return reader.getStructureBuffer(node);
    }

    /** {@inheritDoc} */
    public synchronized ByteBuffer getStructureBuffer(EvioNode node, boolean copy)
                            throws EvioException {
        return reader.getStructureBuffer(node, copy);
    }


    /** {@inheritDoc} */
    public synchronized void close() {reader.close();}

    /** {@inheritDoc} */
    public int getEventCount() {return reader.getEventCount();}

    /** {@inheritDoc} */
    public int getBlockCount() {return reader.getBlockCount();}

    /** {@inheritDoc} */
    public void toFile(String fileName) throws EvioException, IOException {
        reader.toFile(fileName);
    }

    /** {@inheritDoc} */
    public synchronized void toFile(File file) throws EvioException, IOException {
        reader.toFile(file);
    }

}