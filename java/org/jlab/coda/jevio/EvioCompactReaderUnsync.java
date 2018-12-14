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

    /** Evio version number (1-4, 6). Obtain this by reading first header. */
    private int evioVersion;

    /**
     * Endianness of the data being read, either
     * {@link ByteOrder#BIG_ENDIAN} or
     * {@link ByteOrder#LITTLE_ENDIAN}.
     */
    private ByteOrder byteOrder;

    /** The buffer being read. */
    private final ByteBuffer byteBuffer;

    /** Initial position of buffer (0 if file). */
    private final int initialPosition;

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
        findEvioVersion();

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
     * Reads a couple things in the first block (physical record) header
     * in order to determine the evio version of buffer.
     * @return evio version.
     * @throws BufferUnderflowException if not enough data in buffer.
     * @throws EvioException bad magic number in header.
     */
    private int findEvioVersion() throws BufferUnderflowException, EvioException {
        // Look at first block header
System.out.println("findEvioVersion: buf limit = " + byteBuffer.limit() + ", initialPos = " + initialPosition);
Utilities.printBuffer(byteBuffer, 0, 20, "buffer");
        // Have enough remaining bytes to read 8 words of header?
        if (byteBuffer.limit() - initialPosition < 32) {
            throw new BufferUnderflowException();
        }

        // Set the byte order to match the file's ordering.

        // Check the magic number for endianness (buffer defaults to big endian)
        byteOrder = byteBuffer.order();

        int magicNumber = byteBuffer.getInt(initialPosition + RecordHeader.MAGIC_OFFSET);
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
                throw new EvioException("magic number is bad, 0x" +  Integer.toHexString(magicNumber));
            }
        }

        // Find the version number
        int bitInfo = byteBuffer.getInt(initialPosition + RecordHeader.BIT_INFO_OFFSET);
        evioVersion = bitInfo & RecordHeader.VERSION_MASK;

        return evioVersion;
    }


    /** {@inheritDoc} */
    public boolean isFile() {return reader.isFile();}


    /** {@inheritDoc} */
    public void setBuffer(ByteBuffer buf) throws EvioException {reader.setBuffer(buf);}

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