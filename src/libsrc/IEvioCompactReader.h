//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_IEVIOCOMPACTREADER_H
#define EVIO_6_0_IEVIOCOMPACTREADER_H


#include <memory>
#include <vector>


#include "ByteBuffer.h"
#include "ByteOrder.h"
#include "EvioException.h"
#include "EvioNode.h"
#include "EvioXMLDictionary.h"
#include "IBlockHeader.h"


namespace evio {


    /**
     * This is an interface for a compact reader of evio format files and buffers.
     * The word "compact" refers to using the EvioNode class as a compact or
     * lightweight means to refer to an evio event or structure in a buffer.
     * Compact readers do not use EvioEvent, EvioBank, EvioSegment or EvioTagSegment
     * classes which require a full parsing of each event and the creation of a large
     * number of objects.
     *
     * @author timmer
     * @date 6/18/2020.
     */
    class IEvioCompactReader {

    public:

        /**
         * Is this reader reading a file?
         * @return true if reading file, false if reading buffer
         */
        virtual bool isFile() = 0;

        /**
         * Is the data in the file/buffer compressed?
         * @return true if data is compressed.
         */
        virtual bool isCompressed() = 0;

        /**
         * This method can be used to avoid creating additional EvioCompactReader
         * objects by reusing this one with another buffer. The method
         * {@link #close()} should be called before calling this.
         *
         * @param buf ByteBuffer to be read
         * @throws EvioException if arg is null;
         *                       if failure to read first block header;
         *                       if buffer not in evio format.
         */
        virtual void setBuffer(std::shared_ptr<ByteBuffer> & buf) = 0;

        /**
         * Has {@link #close()} been called (without reopening by calling
         * {@link #setBuffer(std::shared_ptr<ByteBuffer> &)})?
         *
         * @return {@code true} if this object closed, else {@code false}.
         */
        virtual bool isClosed() = 0;

        /**
         * Get the byte order of the file/buffer being read.
         * @return byte order of the file/buffer being read.
         */
        virtual ByteOrder getByteOrder() = 0;

        /**
         * Get the evio version number.
         * @return evio version number.
         */
        virtual uint32_t getEvioVersion() = 0;

        /**
          * Get the path to the file.
          * @return path to the file
          */
        virtual std::string getPath() = 0;

        /**
         * When reading a file, this method's return value
         * is the byte order of the evio data in the file.
         * @return byte order of the evio data in the file.
         */
        virtual ByteOrder getFileByteOrder() = 0;

        /**
         * Get the XML format dictionary is there is one.
         * @throws EvioException if object closed and dictionary still unread
         * @return XML format dictionary, else null.
         */
        virtual std::string getDictionaryXML() = 0;

        /**
         * Get the evio dictionary if is there is one.
         * @throws EvioException if object closed and dictionary still unread
         * @return evio dictionary if exists, else null.
         */
        virtual std::shared_ptr<EvioXMLDictionary> getDictionary() = 0;

        /**
         * Does this evio file have an associated XML dictionary?
         *
         * @return <code>true</code> if this evio file has an associated XML dictionary,
         *         else <code>false</code>
         */
        virtual bool hasDictionary() = 0;

        /**
         * Get the byte buffer being read directly or corresponding to the event file.
         * @return the byte buffer being read directly or corresponding to the event file.
         */
        virtual std::shared_ptr<ByteBuffer> getByteBuffer() = 0;

        /**
         * Get the size of the file being read, in bytes.
         * For small files, obtain the file size using the memory mapped buffer's capacity.
         * @return the file size in bytes
         */
        virtual size_t fileSize() = 0;

        /**
         * Get the EvioNode object associated with a particular event number.
         * @param eventNumber number of event (place in file/buffer) starting at 1.
         * @return  EvioNode object associated with a particular event number,
         *          or null if there is none.
         */
        virtual std::shared_ptr<EvioNode> getEvent(size_t eventNumber) = 0;

        /**
         * Get the EvioNode object associated with a particular event number
         * which has been scanned so all substructures are contained in the
         * node.allNodes list.
         * @param eventNumber number of event (place in file/buffer) starting at 1.
         * @return  EvioNode object associated with a particular event number,
         *          or null if there is none.
         */
        virtual std::shared_ptr<EvioNode> getScannedEvent(size_t eventNumber) = 0;

        /**
         * This returns the FIRST block (or record) header.
         * (Not the file header of evio version 6+ files).
         * @return the first block header.
         */
        virtual std::shared_ptr<IBlockHeader> getFirstBlockHeader() = 0;

        /**
         * This method searches the specified event in a file/buffer and
         * returns a vector of objects each of which contain information
         * about a single evio structure which matches the given tag and num.
         *
         * @param eventNumber place of event in buffer (starting with 1)
         * @param tag tag to match.
         * @param num num to match.
         * @param vec vector to be filled with matching evio structures (empty if none found).
         * @throws EvioException if bad arg value(s);
         *                       if object closed
         */
        virtual void searchEvent(size_t eventNumber, uint16_t tag, uint8_t num,
                                 std::vector<std::shared_ptr<EvioNode>> & vec) = 0;

        /**
         * This method searches the specified event in a file/buffer and
         * returns a vector of objects each of which contain information
         * about a single evio structure which matches the given dictionary
         * entry name.
         *
         * @param  eventNumber place of event in buffer (starting with 1)
         * @param  dictName name of dictionary entry to search for
         * @param  dictionary dictionary to use; if null, use dictionary with file/buffer
         * @param  vec vector to be filled with matching evio structures (empty if none found).
         * @throws EvioException if dictName is null;
         *                       if dictName is an invalid dictionary entry;
         *                       if dictionary is null and none provided in file/buffer being read;
         *                       if object closed
         */
        virtual void searchEvent(size_t eventNumber, std::string const & dictName,
                                 std::shared_ptr<EvioXMLDictionary> & dictionary,
                                 std::vector<std::shared_ptr<EvioNode>> & vec) = 0;

        /**
         * This method removes the data of the given event from the buffer.
         * It also marks any existing EvioNodes representing the event and its
         * descendants as obsolete. They must not be used anymore.<p>
         *
         * If the constructor of this reader read in data from a file, it will now switch
         * to using a new, internal buffer which is returned by this method or can be
         * retrieved by calling {@link #getByteBuffer()}. It will <b>not</b> change the
         * file originally used. A new file can be created by calling either the
         * {@link #toFile(std::string const &)} method.<p>
         *
         * @param eventNumber number of event to remove from buffer
         * @return new ByteBuffer created and updated to reflect the event removal
         * @throws EvioException if evNumber &lt; 1;
         *                       if event number does not correspond to existing event;
         *                       if object closed;
         *                       if node was not found in any event;
         *                       if internal programming error
         */
        virtual std::shared_ptr<ByteBuffer> removeEvent(size_t eventNumber) = 0;

        /**
         * This method removes the data, represented by the given node, from the buffer.
         * It also marks the node and its descendants as obsolete. They must not be used
         * anymore.<p>
         *
         * If the constructor of this reader read in data from a file, it will now switch
         * to using a new, internal buffer which is returned by this method or can be
         * retrieved by calling {@link #getByteBuffer()}. It will <b>not</b> change the
         * file originally used. A new file can be created by calling either the
         * {@link #toFile(std::string const &)} method.<p>
         *
         * @param removeNode  evio structure to remove from buffer
         * @return new ByteBuffer (perhaps created) and updated to reflect the node removal
         * @throws EvioException if object closed;
         *                       if node was not found in any event;
         *                       if internal programming error
         */
        virtual std::shared_ptr<ByteBuffer> removeStructure(std::shared_ptr<EvioNode> & removeNode) = 0;

        /**
         * This method adds an evio container (bank, segment, or tag segment) as the last
         * structure contained in an event. It is the responsibility of the caller to make
         * sure that the buffer argument contains valid evio data (only data representing
         * the structure to be added - not in file format with block header and the like)
         * which is compatible with the type of data stored in the given event.<p>
         *
         * To produce such evio data use {@link EvioBank#write(ByteBuffer &)},
         * {@link EvioSegment#write(ByteBuffer &)} or
         * {@link EvioTagSegment#write(ByteBuffer &)} depending on whether
         * a bank, seg, or tagseg is being added.<p>
         *
         * A note about files here. If the constructor of this reader read in data
         * from a file, it will now switch to using a new, internal buffer which
         * is returned by this method or can be retrieved by calling
         * {@link #getByteBuffer()}. It will <b>not</b> expand the file originally used.
         * A new file can be created by calling either the {@link #toFile(std::string const &)} method.<p>
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
         * @throws EvioException if evNumber &lt; 1;
         *                       if addBuffer is null;
         *                       if addBuffer arg is empty or has non-evio format;
         *                       if addBuffer is opposite endian to current event buffer;
         *                       if added data is not the proper length (i.e. multiple of 4 bytes);
         *                       if the event number does not correspond to an existing event;
         *                       if there is an internal programming error;
         *                       if object closed
         */
        virtual std::shared_ptr<ByteBuffer> addStructure(size_t eventNumber, ByteBuffer & addBuffer) = 0;

        /**
         * Get the data associated with an evio structure in ByteBuffer form.
         * The returned buffer is a view into this reader's buffer (no copy done).
         * Changes in one will affect the other.
         *
         * @param node evio structure whose data is to be retrieved
         * @throws EvioException if object closed or node arg is null.
         * @return ByteBuffer filled with data.
         */
        virtual std::shared_ptr<ByteBuffer> getData(std::shared_ptr<EvioNode> & node) = 0;

        /**
         * Get the data associated with an evio structure in ByteBuffer form.
         * Depending on the copy argument, the given/returned buffer will either be
         * a copy of or a view into the data of this reader's buffer.<p>
         * This method is synchronized due to the bulk, relative gets &amp; puts.
         *
         * @param node evio structure whose data is to be retrieved
         * @param copy if <code>true</code>, then return a copy as opposed to a
         *             view into this reader object's buffer.
         * @throws EvioException if object closed
         * @return ByteBuffer filled with data.
         */
        virtual std::shared_ptr<ByteBuffer> getData(std::shared_ptr<EvioNode> & node, bool copy) = 0;

        /**
         * Get the data associated with an evio structure in ByteBuffer form.
         * The returned buffer is a view into this reader's buffer (no copy done).
         * Changes in one will affect the other.
         *
         * @param node evio structure whose data is to be retrieved
         * @param buf  user-supplied ByteBuffer to be filled with data.
         * @throws EvioException if object closed or node arg is null.
         * @return buf arg.
         */
        virtual std::shared_ptr<ByteBuffer> getData(std::shared_ptr<EvioNode> & node,
                                                    std::shared_ptr<ByteBuffer> & buf) = 0;

        /**
         * Get the data associated with an evio structure in ByteBuffer form.
         * Depending on the copy argument, the given/returned buffer will either be
         * a copy of or a view into the data of this reader's buffer.<p>
         * This method is synchronized due to the bulk, relative gets &amp; puts.
         *
         * @param node evio structure whose data is to be retrieved
         * @param buf  user-supplied ByteBuffer to be filled with data.
         * @param copy if <code>true</code>, then return a copy as opposed to a
         *             view into this reader object's buffer.
         * @throws EvioException if object closed
         * @return buf arg.
         */
        virtual std::shared_ptr<ByteBuffer> getData(std::shared_ptr<EvioNode> & node,
                                                    std::shared_ptr<ByteBuffer> & buf, bool copy) = 0;

        /**
         * Get an evio bank or event in ByteBuffer form.
         * The returned buffer is a view into the data of this reader's buffer.<p>
         *
         * @param eventNumber number of event of interest
         * @return ByteBuffer object containing bank's/event's bytes. Position and limit are
         *         set for reading.
         * @throws EvioException if evNumber &lt; 1;
         *                       if the event number does not correspond to an existing event;
         *                       if object closed
         */
        virtual std::shared_ptr<ByteBuffer> getEventBuffer(size_t eventNumber) = 0;

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
         * @throws EvioException if evNumber &lt; 1;
         *                       if the event number does not correspond to an existing event;
         *                       if object closed
         */
        virtual std::shared_ptr<ByteBuffer> getEventBuffer(size_t eventNumber, bool copy) = 0;

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
        virtual std::shared_ptr<ByteBuffer> getStructureBuffer(std::shared_ptr<EvioNode> & node) = 0;

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
        virtual std::shared_ptr<ByteBuffer> getStructureBuffer(std::shared_ptr<EvioNode> & node, bool copy) = 0;

        /** This sets the position to its initial value and marks reader as closed. */
        virtual void close() = 0;

        /**
         * This is the number of events in the file/buffer. Any dictionary event is <b>not</b>
         * included in the count. In versions 3 and earlier, it is not computed unless
         * asked for, and if asked for it is computed and cached.
         *
         * @return the number of events in the file/buffer.
         */
        virtual uint32_t getEventCount() = 0;

        /**
         * This is the number of blocks in the file/buffer including the empty
         * block at the end.
         *
         * @return the number of blocks in the file/buffer (estimate for version 3 files)
         */
        virtual uint32_t getBlockCount() = 0;

        /**
         * Save the internal byte buffer to the given file
         * (overwrites existing file).
         *
         * @param fileName  name of file to write
         * @throws IOException if error writing to file
         * @throws EvioException if fileName arg is null;
         *                       if object closed
         */
        virtual void toFile(std::string const & fileName) = 0;
    };


}

#endif //EVIO_6_0_IEVIOCOMPACTREADER_H
