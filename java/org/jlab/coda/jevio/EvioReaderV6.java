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

/**
 * This class is used to read an evio version 6 format file or buffer.
 * It is called by an <code>EvioReader</code> object. This class is mostly
 * a wrapper to the new hipo library.<p>
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
 * This class is a thread safe version of EvioReaderUnsyncV6.<p>
 *
 * Although this class can be used directly, it's generally used by
 * using {@link EvioCompactReader} which, in turn, uses this class.<p>
 *
 * @since version 6
 * @author timmer
 */
public class EvioReaderV6 extends EvioReaderUnsyncV6 {


    /**
     * Constructor for reading an event file.
     *
     * @param path the full path to the file that contains events.
     *             For writing event files, use an <code>EventWriter</code> object.
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null
     */
    public EvioReaderV6(String path) throws EvioException, IOException {
        super(path);
    }

    /**
     * Constructor for reading an event file.
     *
     * @param path the full path to the file that contains events.
     *             For writing event files, use an <code>EventWriter</code> object.
     * @param checkRecNumSeq if <code>true</code> check the record number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null;
     *                       if first record number != 1 when checkRecNumSeq arg is true
     */
    public EvioReaderV6(String path, boolean checkRecNumSeq) throws EvioException, IOException {
        super(path, checkRecNumSeq);
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
    public EvioReaderV6(File file) throws EvioException, IOException {
        super(file);
    }

    /**
     * Constructor for reading an event file.
     * Do <b>not</b> set sequential to false for remote files.
     *
     * @param file the file that contains events.
     * @param checkRecNumSeq if <code>true</code> check the record number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1
     *
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null;
     *                       if file is too small to have valid evio format data
     *                       if first record number != 1 when checkRecNumSeq arg is true
     */
    EvioReaderV6(File file, boolean checkRecNumSeq)
                                        throws EvioException, IOException {
        super(file, checkRecNumSeq);
    }


    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     * @see EventWriter
     * @throws EvioException if buffer arg is null
     */
    public EvioReaderV6(ByteBuffer byteBuffer) throws EvioException {
        super(byteBuffer);
    }

    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     * @param checkRecNumSeq if <code>true</code> check the record number sequence
     *                       and throw an exception if it is not sequential starting with 1;
     * @see EventWriter
     * @throws EvioException if buffer arg is null;
     *                       if first record number != 1 when checkRecNumSeq arg is true;
     *                       buf not in evio format.
     */
    public EvioReaderV6(ByteBuffer byteBuffer, boolean checkRecNumSeq) throws EvioException {
        super(byteBuffer, checkRecNumSeq);
    }


    //--------------------------------------------------------------
    // The following methods overwrite the equivalent methods in
    // EventReaderUnsyncV6. They are mutex protected for thread
    // safety.
    //--------------------------------------------------------------


    /**
     * This method can be used to avoid creating additional EvioReader
     * objects by reusing this one with another buffer. The method
     * {@link #close()} is called before anything else.
     *
     * @param buf ByteBuffer to be read
     * @throws IOException   if read failure
     * @throws EvioException if buf is null;
     *                       if first record number != 1 when checkRecNumSeq arg is true;
     *                       buf not in evio format.
     */
    @Override
    public synchronized void setBuffer(ByteBuffer buf) throws EvioException, IOException {
        super.setBuffer(buf);
    }

    /** {@inheritDoc} */
    @Override
    public synchronized boolean isClosed() {return super.isClosed();}

    /** {@inheritDoc} */
    @Override
    public synchronized EvioEvent getEvent(int index) throws EvioException {
        return super.getEvent(index);
    }

    /** {@inheritDoc} */
    @Override
    public synchronized EvioEvent parseEvent(int index) throws EvioException {
        return super.parseEvent(index);
	}

    /** {@inheritDoc} */
    @Override
    public synchronized EvioEvent nextEvent() throws EvioException {
        return super.nextEvent();
    }

    /** {@inheritDoc} */
    @Override
    public synchronized EvioEvent parseNextEvent() throws IOException, EvioException {
        return super.parseNextEvent();
	}

    /** {@inheritDoc} */
	@Override
    public synchronized long position() {return super.position();}

    /** {@inheritDoc} */
    @Override
    public synchronized void close() throws IOException {
        super.close();
    }

    /** {@inheritDoc} */
    @Override
    @Deprecated
    public synchronized EvioEvent gotoEventNumber(int evNumber) throws EvioException {
        return super.gotoEventNumber(evNumber);
    }

    /** {@inheritDoc} */
	@Override
    public synchronized WriteStatus toXMLFile(String path,
                                              IEvioProgressListener progressListener,
                                              boolean hex)
                throws EvioException {

	    return super.toXMLFile(path, progressListener, hex);
	}

    /** {@inheritDoc} */
    @Override
    public synchronized int getEventCount() throws EvioException {
        return super.getEventCount();
    }

    /** {@inheritDoc} */
    @Override
    public synchronized int getBlockCount() throws EvioException {
        return super.getBlockCount();
    }

}