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

/**
 * This is a class of interest to the user. It is used to read an evio version 4 or earlier
 * format file or buffer. Create an object corresponding to an event
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
 * This class is a thread safe version of EvioReaderUnsync4.
 *
 * @author heddle
 * @author timmer
 *
 */
public class EvioReaderV4 extends EvioReaderUnsyncV4 {

    /**
     * Constructor for reading an event file.
     *
     * @param path the full path to the file that contains events.
     *             For writing event files, use an <code>EventWriter</code> object.
     * @see EventWriter
     * @throws IOException   if read failure
     * @throws EvioException if file arg is null
     */
    public EvioReaderV4(String path) throws EvioException, IOException {
        super(path);
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
    public EvioReaderV4(String path, boolean checkBlkNumSeq) throws EvioException, IOException {
        super(path, checkBlkNumSeq);
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
    public EvioReaderV4(File file) throws EvioException, IOException {
        super(file);
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
    public EvioReaderV4(File file, boolean checkBlkNumSeq)
                                        throws EvioException, IOException {
        super(file, checkBlkNumSeq);
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
    public EvioReaderV4(String path, boolean checkBlkNumSeq, boolean sequential)
            throws EvioException, IOException {
        super(path, checkBlkNumSeq, sequential);
    }


    /**
     * Constructor for reading an event file.
     * Do <b>not</b> set sequential to false for remote files.
     *
     * @param file the file that contains events.
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
    public EvioReaderV4(File file, boolean checkBlkNumSeq, boolean sequential)
                                        throws EvioException, IOException {

        super(file, checkBlkNumSeq, sequential);
    }


    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     * @see EventWriter
     * @throws EvioException if buffer arg is null
     */
    public EvioReaderV4(ByteBuffer byteBuffer) throws EvioException {
        super(byteBuffer);
    }

    /**
     * Constructor for reading a buffer.
     *
     * @param byteBuffer the buffer that contains events.
     * @param checkBlkNumSeq if <code>true</code> check the block number sequence
     *                       and throw an exception if it is not sequential starting
     *                       with 1
     * @see EventWriter
     * @throws EvioException if buffer arg is null;
     *                       if first block number != 1 when checkBlkNumSeq arg is true
     */
    public EvioReaderV4(ByteBuffer byteBuffer, boolean checkBlkNumSeq) throws EvioException {

        super(byteBuffer, checkBlkNumSeq);
    }


    //--------------------------------------------------------------
    // The following methods overwrite the equivalent methods in
    // EventReaderUnsyncV4. They are mutex protected for thread
    // safety.
    //--------------------------------------------------------------


    /** {@inheritDoc} */
    @Override
    public synchronized void setBuffer(ByteBuffer buf) throws EvioException, IOException {
        super.setBuffer(buf);
    }

    /** {@inheritDoc} */
    @Override
    public synchronized boolean isClosed() { return super.isClosed(); }

    /** {@inheritDoc} */
    protected synchronized EvioEvent getEventV4(int index) throws EvioException {
        return super.getEventV4(index);
    }

    /** {@inheritDoc} */
	@Override
    public synchronized EvioEvent parseEvent(int index) throws IOException, EvioException {
	    return super.parseEvent(index);
	}

    /** {@inheritDoc} */
    @Override
    public synchronized EvioEvent nextEvent() throws IOException, EvioException {
        return super.nextEvent();
    }

    /** {@inheritDoc} */
	@Override
    public synchronized EvioEvent parseNextEvent() throws IOException, EvioException {
	    return super.parseNextEvent();
	}

    /** {@inheritDoc} */
    @Override
    public synchronized void rewind() throws IOException, EvioException {
        super.rewind();
	}

    /** {@inheritDoc} */
	@Override
    public synchronized long position() throws IOException, EvioException {
	    return super.position();
	}

    /** {@inheritDoc} */
    @Override
    public synchronized void close() throws IOException {
        super.close();
    }

    /** {@inheritDoc} */
    protected synchronized EvioEvent gotoEventNumber(int evNumber, boolean parse)
            throws IOException, EvioException {

        return super.gotoEventNumber(evNumber, parse);
	}

    /** {@inheritDoc} */
	@Override
    public synchronized WriteStatus toXMLFile(String path,
                                              IEvioProgressListener progressListener,
                                              boolean hex)
                throws IOException, EvioException {

	    return super.toXMLFile(path, progressListener, hex);
	}

    /** {@inheritDoc} */
    @Override
    public synchronized int getEventCount() throws IOException, EvioException {
        return super.getEventCount();
    }

    /** {@inheritDoc} */
    @Override
    public synchronized int getBlockCount() throws EvioException {
        return super.getBlockCount();
    }


}