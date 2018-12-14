/*
 *   Copyright (c) 2017.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.jevio;

import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.MappedByteBuffer;
import java.util.List;

/**
 * This is an interface for a compact reader of evio format files and buffers.
 * The word "compact" refers to using the EvioNode class as a compact or
 * lightweight means to refer to an evio event or structure in a buffer.
 * Compact readers do not use EvioEvent, EvioBank, EvioSegment or EvioTagSegment
 * classes which require a full parsing of each event and the creation of a large
 * number of objects.
 *
 * @author timmer
 * @since 11/1/17.
 */
public interface IEvioCompactReader {
    /**
	 * This <code>enum</code> denotes the status of a read. <br>
	 * SUCCESS indicates a successful read. <br>
	 * END_OF_FILE indicates that we cannot read because an END_OF_FILE has occurred. Technically this means that what
	 * ever we are trying to read is larger than the buffer's unread bytes.<br>
	 * EVIO_EXCEPTION indicates that an EvioException was thrown during a read, possibly due to out of range values,
	 * such as a negative start position.<br>
	 * UNKNOWN_ERROR indicates that an unrecoverable error has occurred.
	 */
	enum ReadStatus {
		SUCCESS, END_OF_FILE, EVIO_EXCEPTION, UNKNOWN_ERROR
	}

    /**  Offset to get magic number from start of file for every evio version. */
    int MAGIC_OFFSET = 28;

    /** Offset to get version number from start of file for every evio version. */
    int VERSION_OFFSET = 20;

    /** Mask to get version number from 6th int in block for every evio version. */
    int VERSION_MASK = 0xff;


    /**
     * Is this reader reading a file?
     * @return true if reading file, false if reading buffer
     */
    boolean isFile();

    /**
     * This method can be used to avoid creating additional EvioCompactReader
     * objects by reusing this one with another buffer. The method
     * {@link #close()} should be called before calling this.
     *
     * @param buf ByteBuffer to be read
     * @throws EvioException if arg is null;
     *                       if failure to read first block header
     */
    void setBuffer(ByteBuffer buf) throws EvioException;

    /**
     * Has {@link #close()} been called (without reopening by calling
     * {@link #setBuffer(ByteBuffer)})?
     *
     * @return {@code true} if this object closed, else {@code false}.
     */
    boolean isClosed();

    /**
     * Get the byte order of the file/buffer being read.
     * @return byte order of the file/buffer being read.
     */
    ByteOrder getByteOrder();

    /**
     * Get the evio version number.
     * @return evio version number.
     */
    int getEvioVersion();

    /**
      * Get the path to the file.
      * @return path to the file
      */
    String getPath();

    /**
     * When reading a file, this method's return value
     * is the byte order of the evio data in the file.
     * @return byte order of the evio data in the file.
     */
    ByteOrder getFileByteOrder();

    /**
     * Get the XML format dictionary is there is one.
     * @throws EvioException if object closed and dictionary still unread
     * @return XML format dictionary, else null.
     */
    String getDictionaryXML() throws EvioException;

    /**
     * Get the evio dictionary if is there is one.
     * @throws EvioException if object closed and dictionary still unread
     * @return evio dictionary if exists, else null.
     */
    EvioXMLDictionary getDictionary() throws EvioException;

    /**
     * Does this evio file have an associated XML dictionary?
     *
     * @return <code>true</code> if this evio file has an associated XML dictionary,
     *         else <code>false</code>
     */
    boolean hasDictionary();

    /**
     * Get the memory mapped buffer corresponding to the event file.
     * @return the memory mapped buffer corresponding to the event file.
     */
    MappedByteBuffer getMappedByteBuffer();

    /**
     * Get the byte buffer being read directly or corresponding to the event file.
     * @return the byte buffer being read directly or corresponding to the event file.
     */
    ByteBuffer getByteBuffer();

    /**
     * Get the size of the file being read, in bytes.
     * For small files, obtain the file size using the memory mapped buffer's capacity.
     * @return the file size in bytes
     */
    long fileSize();

    /**
     * Get the EvioNode object associated with a particular event number.
     * @param eventNumber number of event (place in file/buffer) starting at 1.
     * @return  EvioNode object associated with a particular event number,
     *          or null if there is none.
     */
    EvioNode getEvent(int eventNumber);

    /**
     * Get the EvioNode object associated with a particular event number
     * which has been scanned so all substructures are contained in the
     * node.allNodes list.
     * @param eventNumber number of event (place in file/buffer) starting at 1.
     * @return  EvioNode object associated with a particular event number,
     *          or null if there is none.
     */
    EvioNode getScannedEvent(int eventNumber);

    /**
     * Get the EvioNode object associated with a particular event number
     * which has been scanned so all substructures are contained in the
     * node.allNodes list.
     * @param eventNumber number of event (place in file/buffer) starting at 1.
     * @param nodeSource  source of EvioNode objects to use while parsing evio data.
     * @return  EvioNode object associated with a particular event number,
     *          or null if there is none.
     */
    EvioNode getScannedEvent(int eventNumber, EvioNodeSource nodeSource);

    /**
     * This returns the FIRST block (or record) header.
     * (Not the file header of evio version 6+ files).
     * @return the first block header.
     */
    IBlockHeader getFirstBlockHeader();

    /**
     * This method searches the specified event in a file/buffer and
     * returns a list of objects each of which contain information
     * about a single evio structure which matches the given tag and num.
     *
     * @param eventNumber place of event in buffer (starting with 1)
     * @param tag tag to match
     * @param num num to match
     * @return list of EvioNode objects corresponding to matching evio structures
     *         (empty if none found)
     * @throws EvioException if bad arg value(s);
     *                       if object closed
     */
    List<EvioNode> searchEvent(int eventNumber, int tag, int num)
                                 throws EvioException;

    /**
     * This method searches the specified event in a file/buffer and
     * returns a list of objects each of which contain information
     * about a single evio structure which matches the given dictionary
     * entry name.
     *
     * @param  eventNumber place of event in buffer (starting with 1)
     * @param  dictName name of dictionary entry to search for
     * @param  dictionary dictionary to use; if null, use dictionary with file/buffer
     *
     * @return list of EvioNode objects corresponding to matching evio structures
     *         (empty if none found)
     * @throws EvioException if dictName is null;
     *                       if dictName is an invalid dictionary entry;
     *                       if dictionary is null and none provided in file/buffer being read;
     *                       if object closed
     */
    List<EvioNode> searchEvent(int eventNumber, String dictName,
                               EvioXMLDictionary dictionary)
                                 throws EvioException;

    /**
     * This method removes the data of the given event from the buffer.
     * It also marks any existing EvioNodes representing the event and its
     * descendants as obsolete. They must not be used anymore.<p>
     *
     * If the constructor of this reader read in data from a file, it will now switch
     * to using a new, internal buffer which is returned by this method or can be
     * retrieved by calling {@link #getByteBuffer()}. It will <b>not</b> change the
     * file originally used. A new file can be created by calling either the
     * {@link #toFile(String)} or {@link #toFile(java.io.File)} methods.<p>
     *
     * @param eventNumber number of event to remove from buffer
     * @return new ByteBuffer created and updated to reflect the event removal
     * @throws EvioException if eventNumber &lt; 1;
     *                       if event number does not correspond to existing event;
     *                       if object closed;
     *                       if node was not found in any event;
     *                       if internal programming error
     */
    ByteBuffer removeEvent(int eventNumber) throws EvioException;

    /**
     * This method removes the data, represented by the given node, from the buffer.
     * It also marks the node and its descendants as obsolete. They must not be used
     * anymore.<p>
     *
     * If the constructor of this reader read in data from a file, it will now switch
     * to using a new, internal buffer which is returned by this method or can be
     * retrieved by calling {@link #getByteBuffer()}. It will <b>not</b> change the
     * file originally used. A new file can be created by calling either the
     * {@link #toFile(String)} or {@link #toFile(java.io.File)} methods.<p>
     *
     * @param removeNode  evio structure to remove from buffer
     * @return new ByteBuffer (perhaps created) and updated to reflect the node removal
     * @throws EvioException if object closed;
     *                       if node was not found in any event;
     *                       if internal programming error
     */
    ByteBuffer removeStructure(EvioNode removeNode) throws EvioException;

    /**
     * This method adds an evio container (bank, segment, or tag segment) as the last
     * structure contained in an event. It is the responsibility of the caller to make
     * sure that the buffer argument contains valid evio data (only data representing
     * the structure to be added - not in file format with block header and the like)
     * which is compatible with the type of data stored in the given event.<p>
     *
     * To produce such evio data use {@link EvioBank#write(ByteBuffer)},
     * {@link EvioSegment#write(ByteBuffer)} or
     * {@link EvioTagSegment#write(ByteBuffer)} depending on whether
     * a bank, seg, or tagseg is being added.<p>
     *
     * A note about files here. If the constructor of this reader read in data
     * from a file, it will now switch to using a new, internal buffer which
     * is returned by this method or can be retrieved by calling
     * {@link #getByteBuffer()}. It will <b>not</b> expand the file originally used.
     * A new file can be created by calling either the {@link #toFile(String)} or
     * {@link #toFile(java.io.File)} methods.<p>
     *
     * The given buffer argument must be ready to read with its position and limit
     * defining the limits of the data to copy.
     * This method is synchronized due to the bulk, relative puts.
     *
     * @param eventNumber number of event to which addBuffer is to be added
     * @param addBuffer buffer containing evio data to add (<b>not</b> evio file format,
     *                  i.e. no block headers)
     * @return a new ByteBuffer object which is created and filled with all the data
     *         including what was just added.
     * @throws EvioException if eventNumber &lt; 1;
     *                       if addBuffer is null;
     *                       if addBuffer arg is empty or has non-evio format;
     *                       if addBuffer is opposite endian to current event buffer;
     *                       if added data is not the proper length (i.e. multiple of 4 bytes);
     *                       if the event number does not correspond to an existing event;
     *                       if there is an internal programming error;
     *                       if object closed
     */
    ByteBuffer addStructure(int eventNumber, ByteBuffer addBuffer) throws EvioException;

    /**
     * Get the data associated with an evio structure in ByteBuffer form.
     * The returned buffer is a view into this reader's buffer (no copy done).
     * Changes in one will affect the other.
     *
     * @param node evio structure whose data is to be retrieved
     * @throws EvioException if object closed or node arg is null.
     * @return ByteBuffer object containing data. Position and limit are
     *         set for reading.
     */
    ByteBuffer getData(EvioNode node) throws EvioException;

    /**
     * Get the data associated with an evio structure in ByteBuffer form.
     * Depending on the copy argument, the returned buffer will either be
     * a copy of or a view into the data of this reader's buffer.<p>
     * This method is synchronized due to the bulk, relative gets &amp; puts.
     *
     * @param node evio structure whose data is to be retrieved
     * @param copy if <code>true</code>, then return a copy as opposed to a
     *             view into this reader object's buffer.
     * @throws EvioException if object closed
     * @return ByteBuffer object containing data. Position and limit are
     *         set for reading.
     */
    ByteBuffer getData(EvioNode node, boolean copy)
                            throws EvioException;

    /**
     * Get an evio bank or event in ByteBuffer form.
     * The returned buffer is a view into the data of this reader's buffer.<p>
     *
     * @param eventNumber number of event of interest
     * @return ByteBuffer object containing bank's/event's bytes. Position and limit are
     *         set for reading.
     * @throws EvioException if eventNumber &lt; 1;
     *                       if the event number does not correspond to an existing event;
     *                       if object closed
     */
    ByteBuffer getEventBuffer(int eventNumber) throws EvioException;

    /**
     * Get an evio bank or event in ByteBuffer form.
     * Depending on the copy argument, the returned buffer will either be
     * a copy of or a view into the data of this reader's buffer.<p>
     *
     * @param eventNumber number of event of interest
     * @param copy if <code>true</code>, then return a copy as opposed to a
     *        view into this reader object's buffer.
     * @return ByteBuffer object containing bank's/event's bytes. Position and limit are
     *         set for reading.
     * @throws EvioException if eventNumber &lt; 1;
     *                       if the event number does not correspond to an existing event;
     *                       if object closed
     */
    ByteBuffer getEventBuffer(int eventNumber, boolean copy)
            throws EvioException;

    /**
     * Get an evio structure (bank, seg, or tagseg) in ByteBuffer form.
     * The returned buffer is a view into the data of this reader's buffer.<p>
     *
     * @param node node object representing evio structure of interest
     * @return ByteBuffer object containing bank's/event's bytes. Position and limit are
     *         set for reading.
     * @throws EvioException if node is null;
     *                       if object closed
     */
    ByteBuffer getStructureBuffer(EvioNode node) throws EvioException;

    /**
     * Get an evio structure (bank, seg, or tagseg) in ByteBuffer form.
     * Depending on the copy argument, the returned buffer will either be
     * a copy of or a view into the data of this reader's buffer.<p>
     *
     * @param node node object representing evio structure of interest
     * @param copy if <code>true</code>, then return a copy as opposed to a
     *        view into this reader object's buffer.
     * @return ByteBuffer object containing structure's bytes. Position and limit are
     *         set for reading.
     * @throws EvioException if node is null;
     *                       if object closed
     */
    ByteBuffer getStructureBuffer(EvioNode node, boolean copy)
            throws EvioException;

    /** This sets the position to its initial value and marks reader as closed. */
    void close();

    /**
     * This is the number of events in the file/buffer. Any dictionary event is <b>not</b>
     * included in the count. In versions 3 and earlier, it is not computed unless
     * asked for, and if asked for it is computed and cached.
     *
     * @return the number of events in the file/buffer.
     */
    int getEventCount();

    /**
     * This is the number of blocks in the file/buffer including the empty
     * block at the end.
     *
     * @return the number of blocks in the file/buffer (estimate for version 3 files)
     */
    int getBlockCount();

    /**
     * Save the internal byte buffer to the given file
     * (overwrites existing file).
     *
     * @param fileName  name of file to write
     * @throws IOException if error writing to file
     * @throws EvioException if fileName arg is null;
     *                       if object closed
     */
    void toFile(String fileName) throws EvioException, IOException;

    /**
     * Save the internal byte buffer to the given file
     * (overwrites existing file).
     *
     * @param file  object of file to write
     * @throws EvioException if file arg is null;
     *                       if object closed
     * @throws IOException if error writing to file
     */
    void toFile(File file) throws EvioException, IOException;

}
