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

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.List;

/**
 * <p>This class is used to read an evio format version 6 formatted file or buffer.
 * It's essentially a wrapper for the hipo.Reader class so the user can have
 * access to the EvioCompactReader methods. It <b>IS</b> thread-safe.</p>
 *
 * <p>Although this class can be used directly, it's generally used by
 * using {@link EvioCompactReader} which, in turn, uses this class.</p>
 *
 * @author timmer
 */
class EvioCompactReaderV6 extends EvioCompactReaderUnsyncV6 {


    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     *
     * @see EventWriter
     * @throws EvioException if buffer arg is null,
     *                       buffer too small,
     *                       buffer not in the proper format,
     *                       or earlier than version 6.
     */
    public EvioCompactReaderV6(ByteBuffer byteBuffer) throws EvioException {
        super(byteBuffer);
    }

    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     * @param pool pool of EvioNode objects to use when parsing buf to avoid garbage collection.
     *
     * @see EventWriter
     * @throws EvioException if buffer arg is null,
     *                       buffer too small,
     *                       buffer not in the proper format,
     *                       or earlier than version 6.
     */
    public EvioCompactReaderV6(ByteBuffer byteBuffer, EvioNodeSource pool) throws EvioException {
        super(byteBuffer, pool);
    }


    //--------------------------------------------------------------
    // The following methods overwrite the equivalent methods in
    // EvioCompactReaderUnsyncV6. They are mutex protected for thread
    // safety.
    //--------------------------------------------------------------

    /** {@inheritDoc} */
    public synchronized boolean isClosed() {return super.isClosed();}

    /** {@inheritDoc} */
    public synchronized String getDictionaryXML() {return super.getDictionaryXML();}

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
    public synchronized ByteBuffer getData(EvioNode node, boolean copy) throws EvioException {
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