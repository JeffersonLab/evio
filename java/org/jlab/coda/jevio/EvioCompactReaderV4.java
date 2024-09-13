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

import java.io.*;
import java.nio.ByteBuffer;
import java.util.List;

/**
 * This class is used to read an evio format version 4 formatted file or buffer
 * and extract specific evio containers (bank, seg, or tagseg)
 * with actual data in them given a tag/num pair.<p>
 *
 * @author timmer
 */
class EvioCompactReaderV4 extends EvioCompactReaderUnsyncV4 {


    /**
     * Constructor for reading an event file.
     *
     * @param path the full path to the file that contains events.
     *             For writing event files, use an <code>EventWriter</code> object.
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null
     */
    public EvioCompactReaderV4(String path) throws EvioException, IOException {
        super(path);
    }


    /**
     * Constructor for reading an event file.
     *
     * @param file the file that contains events.
     *
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null; file is too large;
     */
    public EvioCompactReaderV4(File file) throws EvioException, IOException {
        super(file);
    }


    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     *
     * @see EventWriter
     * @throws EvioException if buffer arg is null;
     *                       failure to read first block header
     */
    public EvioCompactReaderV4(ByteBuffer byteBuffer) throws EvioException {
        super(byteBuffer);
    }


    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     * @param pool pool of EvioNode objects to use when parsing buf.
     *
     * @see EventWriter
     * @throws EvioException if buffer arg is null;
     *                       failure to read first block header
     */
    public EvioCompactReaderV4(ByteBuffer byteBuffer, EvioNodeSource pool) throws EvioException {
        super(byteBuffer, pool);
    }


    /**
     * Constructor for reading a buffer.
     * Turning off the use of BlockNode objects results in NOT being able to call the
     * {@link #addStructure(int, ByteBuffer)} {@link #removeEvent(int)}, or
     * {@link #removeEvent(int)} methods. It does reduce garbage generation for
     * applications that do not need to call these (e.g streaming data in CODA).
     *
     * @param byteBuffer the buffer that contains events.
     * @param pool pool of EvioNode objects to use when parsing buf.
     * @param useBlockHeaders if false, do not create and store BlockNode objects.
     *
     * @see EventWriter
     * @throws EvioException if buffer arg is null;
     *                       failure to read first block header
     */
    public EvioCompactReaderV4(ByteBuffer byteBuffer, EvioNodeSource pool, boolean useBlockHeaders)
            throws EvioException {

        super(byteBuffer, pool, useBlockHeaders);
    }


    //--------------------------------------------------------------
    // The following methods overwrite the equivalent methods in
    // EvioCompactReaderUnsyncV4. They are mutex protected for thread
    // safety.
    //--------------------------------------------------------------


    /** {@inheritDoc} */
    public synchronized boolean isClosed() {return super.isClosed();}

    /** {@inheritDoc} */
    public synchronized String getDictionaryXML() throws EvioException {
        return super.getDictionaryXML();
    }

    /** {@inheritDoc} */
     public synchronized EvioXMLDictionary getDictionary() throws EvioException {
         return super.getDictionary();
     }

    /** {@inheritDoc} */
    public synchronized List<EvioNode> searchEvent(int eventNumber, int tag, int num)
                                 throws EvioException {
        return super.searchEvent(eventNumber, tag, num);
    }

    /** {@inheritDoc} */
    public synchronized List<EvioNode> searchEvent(int eventNumber, String dictName,
                                                   EvioXMLDictionary dictionary)
                                 throws EvioException {
        return super.searchEvent(eventNumber, dictName, dictionary);
    }

    /** {@inheritDoc} */
    public synchronized ByteBuffer removeEvent(int eventNumber) throws EvioException {
        return super.removeEvent(eventNumber);
    }

    /** {@inheritDoc} */
    public synchronized ByteBuffer removeStructure(EvioNode removeNode) throws EvioException {
        return super.removeStructure(removeNode);
    }

    /** {@inheritDoc} */
    public synchronized ByteBuffer addStructure(int eventNumber, ByteBuffer addBuffer) throws EvioException {
        return super.addStructure(eventNumber, addBuffer);
    }

    /** {@inheritDoc} */
    public synchronized ByteBuffer getData(EvioNode node, boolean copy)
                            throws EvioException {
        return super.getData(node, copy);
    }

    /** {@inheritDoc} */
    public synchronized ByteBuffer getEventBuffer(int eventNumber, boolean copy)
                            throws EvioException {
        return super.getEventBuffer(eventNumber, copy);
    }

    /** {@inheritDoc} */
    public synchronized ByteBuffer getStructureBuffer(EvioNode node, boolean copy)
            throws EvioException {
        return super.getStructureBuffer(node, copy);
    }

    /** {@inheritDoc} */
    public synchronized void close() {
        super.close();
	}

    /** {@inheritDoc} */
    public synchronized void toFile(File file) throws EvioException, IOException {
        super.toFile(file);
    }

}