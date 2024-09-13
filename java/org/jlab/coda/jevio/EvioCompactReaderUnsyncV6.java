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

import org.jlab.coda.hipo.HipoException;
import org.jlab.coda.hipo.Reader;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.util.ArrayList;
import java.util.List;

/**
 * <p>This class is used to read an evio format version 6 formatted file or buffer.
 * It's essentially a wrapper for the hipo.Reader class so the user can have
 * access to the EvioCompactReader methods. It is NOT thread-safe.</p>
 *
 * <p>Although this class can be used directly, it's generally used by
 * using {@link EvioCompactReader} which, in turn, uses this class.</p>
 *
 * @author timmer
 */
class EvioCompactReaderUnsyncV6 implements IEvioCompactReader {

    /** The reader object which does all the work. */
    protected Reader reader;

    /** Is this object currently closed? */
    protected boolean closed;

    /** Dictionary object created from dictionaryXML string. */
    protected EvioXMLDictionary dictionary;


    
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
    public EvioCompactReaderUnsyncV6(ByteBuffer byteBuffer) throws EvioException {
        this(byteBuffer, null);
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
    public EvioCompactReaderUnsyncV6(ByteBuffer byteBuffer, EvioNodeSource pool)
            throws EvioException {

        if (byteBuffer == null) {
            throw new EvioException("Buffer arg is null");
        }

        try {
            reader = new Reader(byteBuffer, pool);
            if (!reader.isEvioFormat()) {
                throw new EvioException("buffer not in evio format");
            }
        }
        catch (HipoException e) {
            throw new EvioException(e);
        }
    }

    /**
     * Is this reader reading a file? Always false for this class.
     * @return false.
     */
    public boolean isFile() {return false;}

    /** {@inheritDoc} */
    public boolean isCompressed() {return reader.isCompressed();}

    /** {@inheritDoc} */
    public void setBuffer(ByteBuffer buf) throws EvioException {
        setBuffer(buf, null);
    }

    /** {@inheritDoc} */
    public void setBuffer(ByteBuffer buf, EvioNodeSource pool) throws EvioException {
        try {
            reader.setBuffer(buf, pool);
            if (!reader.isEvioFormat()) {
                throw new EvioException("buffer not in evio format");
            }
        }
        catch (HipoException e) {
            throw new EvioException(e);
        }

        dictionary = null;
        closed = false;
    }

    /** {@inheritDoc} */
    public boolean isClosed() {return closed;}

    /** {@inheritDoc} */
    public ByteOrder getByteOrder() {return reader.getByteOrder();}

    /** {@inheritDoc} */
    public int getEvioVersion() {return reader.getVersion();}

    /**
      * Get the path to the file which is always null cause there is no file.
      * @return null
      */
     public String getPath() {return null;}

    /**
     * Get the byte order of the file being read.
     * Since we're not reading a file, always return null.
     * @return null.
     */
    public ByteOrder getFileByteOrder() {return null;}

    /** {@inheritDoc} */
    public String getDictionaryXML() {return reader.getDictionary();}

    /** {@inheritDoc} */
     public  EvioXMLDictionary getDictionary() throws EvioException {
         if (dictionary != null) return dictionary;

         if (closed) {
             throw new EvioException("object closed");
         }

         String dictXML = reader.getDictionary();
         if (dictXML != null) {
             dictionary = new EvioXMLDictionary(dictXML);
         }

         return dictionary;
     }

    /** {@inheritDoc} */
    public boolean hasDictionary() {return reader.hasDictionary();}

    /**
     * Get the byte buffer being read.
     * @return byte buffer being read.
     */
    public ByteBuffer getByteBuffer() {return reader.getBuffer();}

    /**
     * Get the memory mapped buffer corresponding to the event file.
     * Since we're not reading a file, always return null.
     * @return null.
     */
    public MappedByteBuffer getMappedByteBuffer() {return null;}

    /**
     * Get the size of the file being read, in bytes.
     * Since we're not reading a file, always return 0.
     * @return 0.
     */
    public long fileSize() {return 0L;}

    /**
     * Get the EvioNode object associated with a particular event number.
     * @param eventNumber number of event (place in buffer) starting at 1.
     * @return  EvioNode object associated with a particular event number,
     *          or null if eventNumber is out of bounds, reading a file or
     *          data is compressed.
     */
    public EvioNode getEvent(int eventNumber) {
        try {
            return reader.getEventNode(eventNumber - 1);
        }
        catch (IndexOutOfBoundsException e) { }
        return null;
    }


    /**
     * Get the EvioNode object associated with a particular event number
     * which has been scanned so all substructures are contained in the
     * node.allNodes list.
     * @param eventNumber number of event (place in file/buffer) starting at 1.
     * @return  EvioNode object associated with a particular event number,
     *          or null if eventNumber is out of bounds, reading a file or data is
     *          compressed.
     */
    public EvioNode getScannedEvent(int eventNumber) {
        try {
            return scanStructure(eventNumber);
        }
        catch (IndexOutOfBoundsException e) { }
        return null;
    }


    /**
     * Get the EvioNode object associated with a particular event number
     * which has been scanned so all substructures are contained in the
     * node.allNodes list.
     * @param eventNumber number of event (place in file/buffer) starting at 1.
     * @param nodeSource  source of EvioNode objects to use while parsing evio data.
     * @return  EvioNode object associated with a particular event number,
     *          or null if eventNumber is out of bounds, reading a file or data is
     *          compressed.
     */
    public EvioNode getScannedEvent(int eventNumber, EvioNodeSource nodeSource) {
        try {
            return scanStructure(eventNumber, nodeSource);
        }
        catch (IndexOutOfBoundsException e) {}
        return null;
    }


    /** {@inheritDoc} */
    public IBlockHeader getFirstBlockHeader() {return reader.getFirstRecordHeader();}


    /**
     * This method scans the given event number in the buffer.
     * It returns an EvioNode object representing the event.
     * All the event's substructures, as EvioNode objects, are
     * contained in the node.allNodes list (including the event itself).
     *
     * @param eventNumber number of the event to be scanned starting at 1
     * @return the EvioNode object corresponding to the given event number,
     *         or null if eventNumber is out of bounds, reading a file or data is
     *         compressed.
     */
    protected EvioNode scanStructure(int eventNumber) {

        // Node corresponding to event
        EvioNode node = reader.getEventNode(eventNumber - 1);
        if (node == null) return null;

        if (node.scanned) {
            node.clearLists();
        }

        // Do this before actual scan so clone() sets all "scanned" fields
        // of child nodes to "true" as well.
        node.scanned = true;

        EvioNode.scanStructure(node);

        return node;
    }


    /**
     * This method scans the given event number in the buffer.
     * It returns an EvioNode object representing the event.
     * All the event's substructures, as EvioNode objects, are
     * contained in the node.allNodes list (including the event itself).
     *
     * @param eventNumber number of the event to be scanned starting at 1
     * @param nodeSource  source of EvioNode objects to use while parsing evio data.
     * @return the EvioNode object corresponding to the given event number,
     *         or null if eventNumber is out of bounds, reading a file or data is
     *         compressed.
     */
    protected EvioNode scanStructure(int eventNumber, EvioNodeSource nodeSource) {

        // Node corresponding to event
        EvioNode node = reader.getEventNode(eventNumber - 1);
        if (node == null) return null;

        if (node.scanned) {
            node.clearLists();
        }

        // Do this before actual scan so clone() sets all "scanned" fields
        // of child nodes to "true" as well.
        node.scanned = true;

        EvioNode.scanStructure(node, nodeSource);

        return node;
    }


    /** {@inheritDoc} */
    public  List<EvioNode> searchEvent(int eventNumber, int tag, int num)
                                 throws EvioException {

        // check args
        if (tag < 0 || num < 0 || eventNumber < 1 || eventNumber > reader.getEventCount()) {
            throw new EvioException("bad arg value(s)");
        }

        if (closed) {
            throw new EvioException("object closed");
        }

        ArrayList<EvioNode> returnList = new ArrayList<>(100);

        // Scan the node
        ArrayList<EvioNode> list;
        EvioNode node = scanStructure(eventNumber);
        if (node == null) {
             return new ArrayList<>(0);
        }

        list = node.allNodes;
//System.out.println("searchEvent: ev# = " + eventNumber + ", list size = " + list.size() +
//" for tag/num = " + tag + "/" + num);

        // Now look for matches in this event
        for (EvioNode enode: list) {
//System.out.println("searchEvent: desired tag = " + tag + " found " + enode.tag);
//System.out.println("           : desired num = " + num + " found " + enode.num);
            if (enode.tag == tag && enode.num == num) {
//System.out.println("           : found node at pos = " + enode.pos + " len = " + enode.len);
                returnList.add(enode);
            }
        }

        return returnList;
    }


    /** {@inheritDoc} */
    public  List<EvioNode> searchEvent(int eventNumber, String dictName,
                                      EvioXMLDictionary dictionary)
                                 throws EvioException {

        if (dictName == null) {
            throw new EvioException("null dictionary entry name");
        }

        if (closed) {
            throw new EvioException("object closed");
        }

        // If no dictionary is specified, use the one provided with the
        // file/buffer. If that does not exist, throw an exception.
        int tag, num;

        if (dictionary == null && hasDictionary())  {
            dictionary = getDictionary();
        }

        if (dictionary != null) {
            tag = dictionary.getTag(dictName);
            num = dictionary.getNum(dictName);
            if (tag == -1 || num == -1) {
                throw new EvioException("no dictionary entry for " + dictName);
            }
        }
        else {
            throw new EvioException("no dictionary available");
        }

        return searchEvent(eventNumber, tag, num);
    }


    /**
     * This method removes the data of the given event from the buffer.
     * It also marks any existing EvioNodes representing the event and its
     * descendants as obsolete. They must not be used anymore.<p>
     *
     * @param eventNumber number of event to remove from buffer
     * @return new ByteBuffer created and updated to reflect the event removal,
     *         or null if reading a file or data is compressed.
     * @throws EvioException if eventNumber &lt; 1;
     *                       if event number does not correspond to existing event;
     *                       if object closed;
     *                       if node was not found in any event;
     *                       if internal programming error
     */
    public  ByteBuffer removeEvent(int eventNumber) throws EvioException {

        if (eventNumber < 1) {
            throw new EvioException("event number must be > 0");
        }

        if (closed) {
            throw new EvioException("object closed");
        }

        EvioNode eventNode;
        try {
            eventNode = reader.getEventNode(eventNumber - 1);
        }
        catch (IndexOutOfBoundsException e) {
            throw new EvioException("event " + eventNumber + " does not exist", e);
        }

        return removeStructure(eventNode);
    }


    /**
     * This method removes the data, represented by the given node, from the buffer.
     * It also marks all nodes taken from that buffer as obsolete.
     * They must not be used anymore.<p>
     *
     * @param removeNode  evio structure to remove from buffer
     * @return ByteBuffer updated to reflect the node removal
     * @throws EvioException if object closed;
     *                       if node was not found in any event;
     *                       if internal programming error;
     *                       if buffer has compressed data.
     */
    public  ByteBuffer removeStructure(EvioNode removeNode) throws EvioException {
        try {
            return reader.removeStructure(removeNode);
        }
        catch (HipoException e) {
            throw new EvioException(e);
        }
    }


    /**
     * This method adds an evio container (bank, segment, or tag segment) as the last
     * structure contained in an event. It is the responsibility of the caller to make
     * sure that the buffer argument contains valid evio data (only data representing
     * the structure to be added - not in file format with record header and the like)
     * which is compatible with the type of data stored in the given event.<p>
     *
     * To produce such evio data use {@link EvioBank#write(ByteBuffer)},
     * {@link EvioSegment#write(ByteBuffer)} or
     * {@link EvioTagSegment#write(ByteBuffer)} depending on whether
     * a bank, seg, or tagseg is being added.<p>
     *
     * The given buffer argument must be ready to read with its position and limit
     * defining the limits of the data to copy.
     *
     * @param eventNumber number of event to which addBuffer is to be added
     * @param addBuffer buffer containing evio data to add (<b>not</b> evio file format,
     *                  i.e. no record headers)
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
    public  ByteBuffer addStructure(int eventNumber, ByteBuffer addBuffer) throws EvioException {
        try {
            return reader.addStructure(eventNumber, addBuffer);
        }
        catch (HipoException e) {
            throw new EvioException(e);
        }
    }


    /** {@inheritDoc} */
    public ByteBuffer getData(EvioNode node) throws EvioException {
        return getData(node, false);
    }


    /** {@inheritDoc} */
    public  ByteBuffer getData(EvioNode node, boolean copy) throws EvioException {
        if (closed) {
            throw new EvioException("object closed");
        }
        else if (node == null) {
            throw new EvioException("node arg is null");
        }
        return node.getByteData(copy);
    }


    /** {@inheritDoc} */
    public ByteBuffer getEventBuffer(int eventNumber) throws EvioException {
        return getEventBuffer(eventNumber, false);
    }


    /** {@inheritDoc} */
    public  ByteBuffer getEventBuffer(int eventNumber, boolean copy)
            throws EvioException {

        if (eventNumber < 1) {
            throw new EvioException("event number must be > 0");
        }

        if (closed) {
            throw new EvioException("object closed");
        }

        EvioNode node;
        try {
            node = reader.getEventNode(eventNumber - 1);
        }
        catch (IndexOutOfBoundsException e) {
            throw new EvioException("event " + eventNumber + " does not exist");
        }

        return node.getStructureBuffer(copy);
    }


    /** {@inheritDoc} */
    public ByteBuffer getStructureBuffer(EvioNode node) throws EvioException {
        return getStructureBuffer(node, false);
    }


    /** {@inheritDoc} */
    public  ByteBuffer getStructureBuffer(EvioNode node, boolean copy)
            throws EvioException {

        if (node == null) {
            throw new EvioException("node arg is null");
        }

        if (closed) {
            throw new EvioException("object closed");
        }

        return node.getStructureBuffer(copy);
    }


    /** {@inheritDoc} */
    public  void close() {
        reader.getBuffer().position(reader.getBufferOffset());
        closed = true;
	}


    /** {@inheritDoc} */
    public int getEventCount() {return reader.getEventCount();}

    /** {@inheritDoc} */
    public int getBlockCount() { return reader.getRecordCount();}

    /** {@inheritDoc} */
    public void toFile(String fileName) throws EvioException, IOException {
        if (fileName == null) {
            throw new EvioException("null fileName arg");
        }
        File f = new File(fileName);
        toFile(f);
    }

    /** {@inheritDoc} */
    public  void toFile(File file) throws EvioException, IOException {
        if (file == null) {
            throw new EvioException("null file arg");
        }

        if (closed) {
            throw new EvioException("object closed");
        }

        // Remember where we were
        ByteBuffer buf = reader.getBuffer();
        int pos = buf.position();

        // Write the file
        FileOutputStream fos = new FileOutputStream(file);
        FileChannel channel = fos.getChannel();
        channel.write(buf);
        channel.close();

        // Go back to where we were
        buf.position(pos);
    }

}