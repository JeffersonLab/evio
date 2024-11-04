//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "EvioCompactStructureHandler.h"


namespace evio {



    /**
     * Constructor for reading an EvioNode object.
     * The data represented by the given EvioNode object will be copied to a new
     * buffer (obtainable by calling {@link #getByteBuffer()}) and the node and
     * all of its descendants will switch to that new buffer.
     *
     * @param node the node to be analyzed.
     * @throws EvioException if node arg is null.
     */
    EvioCompactStructureHandler::EvioCompactStructureHandler(std::shared_ptr<EvioNode> node) {
        if (node == nullptr) {
            throw EvioException("node arg is null");
        }

        // Node's backing buffer
        std::shared_ptr<ByteBuffer> bb = node->getBuffer();

        // Duplicate backing buf because we'll be changing pos & lim
        // and don't want to mess it up for someone else.
        byteOrder = bb->order();
        byteBuffer = bb->duplicate();
        byteBuffer->order(byteOrder);

        // Copy data into a new internal buffer to allow modifications
        // to it without affecting the original buffer.
        bufferInit(node);
    }

    /**
     * Constructor for reading a buffer that contains 1 structure only (no record headers).
     * The data in the given ByteBuffer object will be copied to a new
     * buffer (obtainable by calling {@link #getByteBuffer()}).
     *
     * @param buf  the buffer to be read that contains 1 structure only (no block headers).
     * @param type the type of outermost structure contained in buffer, may be {@link DataType#BANK},
     *             {@link DataType#SEGMENT}, {@link DataType#TAGSEGMENT} or equivalent.
     * @throws EvioException if byteBuffer arg is null;
     *                       if type arg is null or is not an evio structure;
     *                       if byteBuffer not in proper format;
     */
    EvioCompactStructureHandler::EvioCompactStructureHandler(std::shared_ptr<ByteBuffer> buf, DataType & type) {
            setBuffer(buf, type);
    }


    // TODO: Needs to be synchronized !!

    /**
     * This method can be used to avoid creating additional EvioCompactStructureHandler
     * objects by reusing this one with another buffer.
     *
     * @param buf the buffer to be read that contains 1 structure only (no block headers).
     * @param type the type of outermost structure contained in buffer, may be {@link DataType#BANK},
     *             {@link DataType#SEGMENT}, {@link DataType#TAGSEGMENT} or equivalent.
     * @throws EvioException if buf arg is null;
     *                       if type is not an evio structure;
     *                       if buf not in proper format;
     */
    /*synchronized*/
    void EvioCompactStructureHandler::setBuffer(std::shared_ptr<ByteBuffer> buf, DataType & type) {

        if (buf == nullptr) {
            throw EvioException("buffer arg is null");
        }

        if (!type.isStructure()) {
            throw EvioException("type arg is not an evio structure");
        }

        if (buf->remaining() < 1) {
            throw EvioException("buffer has too little data");
        }
        else if (type.isBank() && buf->remaining() < 2 ) {
            throw EvioException("buffer has too little data");
        }

        closed = true;

        // Duplicate buf because we'll be changing pos & lim and
        // don't want to mess up the original for someone else.
        byteOrder  = buf->order();
        byteBuffer = buf->duplicate();
        byteBuffer->order(byteOrder);

        // Pull out all header info & place into EvioNode object
        node = extractNode(byteBuffer, nullptr, type, byteBuffer->position(), 0, true);

        // See if the length given in header is consistent with buffer size
        if (node->len + 1 > byteBuffer->remaining()/4) {
            throw EvioException("buffer has too little data");
        }

        // Copy data into a new internal buffer to allow modifications
        // to it without affecting the original buffer.
        bufferInit(node);

        closed = false;
    }


    /**
     * This method takes the EvioNode object representing the evio data of interest
     * and uses it to copy that data into a new local buffer. This allows changes to
     * be made to the data without affecting the original buffer. The new buffer is
     * 25% larger to accommodate any future additions to the data.
     *
     * @param node the node representing evio data.
     */
    void EvioCompactStructureHandler::bufferInit(std::shared_ptr<EvioNode> node) {

        // Position of evio structure in byteBuffer
        size_t endPos   = node->dataPos + 4*node->dataLen;
        size_t startPos = node->pos;

        // Copy data so we're not tied to byteBuffer arg.
        // Since this handler is often used to add banks to the node structure,
        // make the new buffer 25% bigger in anticipation.
        auto newBuffer = std::make_shared<ByteBuffer>(5*(endPos - startPos)/4);
        newBuffer->order(byteOrder);

        // Copy evio data into new buffer
        byteBuffer->limit(endPos).position(startPos);
        newBuffer->put(byteBuffer);
        newBuffer->position(0).limit((endPos - startPos));

        // Update node & descendants

        // Can call the top node an "event", even if it isn't a bank,
        // and things should still work fine.
        node->izEvent    = true;
        node->allNodes.clear();
        node->allNodes.push_back(node);
        node->parentNode = nullptr;
        node->dataPos   -= startPos;
        node->pos       -= startPos;
        node->buffer     = newBuffer;

        // Doing scan after above adjustments
        // will make other nodes come out right.
        EvioNode::scanStructure(node);
        node->scanned = true;

        byteBuffer = newBuffer;
        this->node = node;
    }


    /**
     * This method expands the data buffer, copies the data into it,
     * and updates all associated EvioNode objects.
     *
     * @param byteSize the number of bytes needed in new buffer.
     */
    void EvioCompactStructureHandler::expandBuffer(size_t byteSize) {
        // Since private method, forget about bounds checking of arg

        // Since this handler is often used to add banks to the node structure,
        // make the new buffer 25% bigger in anticipation.
        auto newBuffer = std::make_shared<ByteBuffer>(5*byteSize/4);
        newBuffer->order(byteOrder);

        // Ending position of data in byteBuffer
        size_t endPos = node->dataPos + 4*node->dataLen;

        // Copy existing data into new buffer
        byteBuffer->position(0).limit(endPos);
        newBuffer->put(byteBuffer);
        newBuffer->position(0).limit(endPos);

        // Update node & descendants
        for (std::shared_ptr<EvioNode> n : node->allNodes) {
            // Using a new buffer now
            n->buffer = newBuffer;
        }

        byteBuffer = newBuffer;
    }


    /**
     * Has {@link #close()} been called (without reopening by calling
     * {@link #setBuffer})?
     *
     * @return {@code true} if this object closed, else {@code false}.
     */
    /*synchronized*/
    bool EvioCompactStructureHandler::isClosed() {return closed;}


    /**
     * Get the byte order of buffer being read.
     * @return the byte order of buffer being read.
     */
    ByteOrder EvioCompactStructureHandler::getByteOrder() {return byteOrder;}


    /**
     * Get the byte buffer being read.
     * @return the byte buffer being read.
     */
    std::shared_ptr<ByteBuffer> EvioCompactStructureHandler::getByteBuffer() {return byteBuffer;}


    /**
     * Get the EvioNode object associated with the structure.
     * @return EvioNode object associated with the structure.
     */
    std::shared_ptr<EvioNode> EvioCompactStructureHandler::getStructure() {return node;}


    /**
      * Get the EvioNode object associated with the structure
      * which has been scanned so all substructures are contained in the
      * node.allNodes vector.
      * @return  EvioNode object associated with the structure,
      *          or null if there is none.
      */
    std::shared_ptr<EvioNode> EvioCompactStructureHandler::getScannedStructure() {
        EvioNode::scanStructure(node);
        return node;
    }




    /**
     * This method extracts an EvioNode object from a given buffer, a
     * location in the buffer, and a few other things. An EvioNode
     * object represents an evio container - either a bank, segment,
     * or tag segment.
     *
     * @param buffer    buffer to examine
     * @param eventNode object holding data about containing event (null if isEvent = true)
     * @param type      type of evio structure to extract
     * @param position  position in buffer
     * @param place     place of event in buffer (starting at 0)
     * @param isEvent   is the node being extracted an event (top-level bank)?
     *
     * @return          EvioNode object containing evio structure information
     * @throws          EvioException if file/buffer not in evio format
     */
    std::shared_ptr<EvioNode> EvioCompactStructureHandler::extractNode(std::shared_ptr<ByteBuffer> buffer,
                                                                       std::shared_ptr<EvioNode> eventNode, DataType & type,
                                                                       size_t position, int place, bool isEvent) {

        // Store current evio info without de-serializing
        auto node = std::make_shared<EvioNode>();
        node->pos        = position;
        node->place      = place;      // Which # event from beginning am I?
        node->eventNode  = eventNode;
        node->izEvent    = isEvent;
        node->type       = type.getValue();
        node->buffer     = buffer;
        if (eventNode != nullptr) node->allNodes = eventNode->allNodes;

        try {

            if  (type.isBank()) {

                // Get length of current event
                node->len = buffer->getInt(position);
                // Position of data for a bank
                node->dataPos = position + 8;
                // Len of data for a bank
                node->dataLen = node->len - 1;

                // Hop over length word
                position += 4;

                // Read and parse second header word
                int32_t word = buffer->getInt(position);
                node->tag = (word >> 16) & 0xffff;
                int dt = (word >> 8) & 0xff;
                node->dataType = dt & 0x3f;
                node->pad = dt >> 6;
                // If only 7th bit set, it can be tag=0, num=0, type=0, padding=1.
                // This regularly happens with composite data.
                // However, it that MAY also be the legacy tagsegment type
                // with no padding information. Ignore this as having tag & num
                // in legacy code is probably rare.
                //if (dt == 0x40) {
                //    node->dataType = DataType::TAGSEGMENT.getValue();
                //    node->pad = 0;
                //}
                node->num = word & 0xff;
            }
            else if (type.isSegment()) {

                node->dataPos = position + 4;

                int32_t word = buffer->getInt(position);
                node->tag = (word >> 24) & 0xff;
                int dt = (word >> 16) & 0xff;
                node->dataType = dt & 0x3f;
                node->pad = dt >> 6;
                node->len = word & 0xffff;
                node->dataLen = node->len;
            }
            else if (type.isTagSegment()) {

                node->dataPos = position + 4;

                int32_t word = buffer->getInt(position);
                node->tag = (word >> 20) & 0xfff;
                node->dataType = (word >> 16) & 0xf;
                node->len = word & 0xffff;
                node->dataLen = node->len;
            }
            else {
                throw EvioException("Buffer bad format");
            }
        }
        catch (std::runtime_error & a) {
            throw EvioException("Buffer bad format");
        }

        return node;
    }



    /**
     * This method scans the event in the buffer.
     * The results are stored for future reference so the
     * event is only scanned once. It returns a vector of EvioNode
     * objects representing evio structures (banks, segs, tagsegs).
     *
     * @return vector of objects (evio structures containing data)
     *         obtained from the scan
     */
    std::vector<std::shared_ptr<EvioNode>> EvioCompactStructureHandler::scanStructure() {

        if (!node->scanned) {
            // Do this before actual scan so clone() sets all "scanned" fields
            // of child nodes to "true" as well.
            node->scanned = true;

            EvioNode::scanStructure(node);
        }

        // Return results of this or a previous scan
        return node->allNodes;
    }


    // TODO: deal with synchronized !!!

    /**
     * This method searches the event and
     * returns a vector of objects each of which contain information
     * about a single evio structure which matches the given tag and num.
     *
     * @param tag tag to match
     * @param num num to match
     * @return vector of EvioNode objects corresponding to matching evio structures
     *         (empty if none found)
     * @throws EvioException if object closed.
     */
    /*synchronized*/
    std::vector<std::shared_ptr<EvioNode>> EvioCompactStructureHandler::searchStructure(uint16_t tag, uint8_t num) {

        if (closed) {
            throw EvioException("object closed");
        }

        std::vector<std::shared_ptr<EvioNode>> returnList;

        // If event has been previously scanned we get that list,
        // otherwise we scan the node and store the results for future use.
        std::vector<std::shared_ptr<EvioNode>> list = scanStructure();
        //std::cout << "searchEvent: list size = " << list.size() << " for tag/num = " << tag << "/" << num << std::endl;

        // Now look for matches in this event
        for (auto enode: list) {
            //std::cout << "searchEvent: desired tag = " << tag << " found " << enode->tag << std::endl;
            //std::cout << "           : desired num = " << num << " found " << enode->num << std::endl;
            if (enode->tag == tag && enode->num == num) {
                //std::cout << "           : found node at pos = " << enode->pos << " len = " << enode->len << std::endl;
                returnList.push_back(enode);
            }
        }

        return returnList;
    }


    /**
     * This method searches the event and
     * returns a vector of objects each of which contain information
     * about a single evio structure which matches the given dictionary
     * entry name.
     *
     * @param  dictName name of dictionary entry to search for
     * @param  dictionary dictionary to use
     * @return vector of EvioNode objects corresponding to matching evio structures
     *         (empty if none found)
     * @throws EvioException if either dictName or dictionary arg is empty/null;
     *                       if dictName is an invalid dictionary entry;
     *                       if object closed.
     */
    std::vector<std::shared_ptr<EvioNode>> EvioCompactStructureHandler::searchStructure(std::string const & dictName,
                                                                                        std::shared_ptr<EvioXMLDictionary> dictionary) {
        if (dictName.empty() || dictionary == nullptr) {
            throw EvioException("null dictionary and/or entry name");
        }

        uint16_t tag;
        uint8_t  num;

        bool found = dictionary->getTag(dictName, &tag);
        if (!found) {
            throw EvioException("no dictionary entry available");
        }
        dictionary->getNum(dictName, &num);

        return searchStructure(tag, num);
    }


    /**
     * This method adds a bank, segment, or tagsegment onto the end of a
     * structure which contains banks, segments, or tagsegments respectively.
     * It is the responsibility of the caller to make sure that
     * the buffer argument contains valid evio data (only data representing
     * the bank or structure to be added - not in file format with bank
     * header and the like) which is compatible with the type of data
     * structure stored in the parent structure. This means it is not
     * possible to add to structures which contain only a primitive data type.<p>
     *
     * To produce properly formatted evio data, use
     * {@link EvioBank#write(ByteBuffer)},
     * {@link EvioSegment#write(ByteBuffer)} or
     * {@link EvioTagSegment#write(ByteBuffer)} depending on whether
     * a bank, seg, or tagseg is being added.<p>
     *
     * The given buffer argument must be ready to read with its position and limit
     * defining the limits of the data to copy.
     * This method is synchronized due to the bulk, relative puts.
     *
     * @param addBuffer buffer containing evio data to add (<b>not</b> evio file format,
     *                  i.e. no bank headers)
     * @return a new ByteBuffer object which is created and filled with all the data
     *         including what was just added.
     * @throws EvioException if addBuffer is null;
     *                       if addBuffer arg is empty or has non-evio format;
     *                       if addBuffer is opposite endian to current event buffer;
     *                       if added data is not the proper length (i.e. multiple of 4 bytes);
     *                       if there is an internal programming error;
     *                       if object closed.
     */
    /*synchronized*/
    std::shared_ptr<ByteBuffer> EvioCompactStructureHandler::addStructure(std::shared_ptr<ByteBuffer> addBuffer) {

            if (!node->getDataTypeObj().isStructure()) {
                throw EvioException("cannot add structure to bank of primitive type");
            }

            if (addBuffer == nullptr || addBuffer->remaining() < 4) {
                throw EvioException("null, empty, or non-evio format buffer arg");
            }

            if (addBuffer->order() != byteOrder) {
                throw EvioException("trying to add wrong endian buffer");
            }

            if (closed) {
                throw EvioException("object closed");
            }

            // Position in byteBuffer just past end of event
            size_t endPos = node->dataPos + 4*node->dataLen;

            // Original position of buffer being added
            size_t origAddBufPos = addBuffer->position();

            // How many bytes are we adding?
            size_t appendDataLen = addBuffer->remaining();

            // Make sure it's a multiple of 4
            if (appendDataLen % 4 != 0) {
                throw EvioException("data added is not in evio format");
            }

            // Since we're changing node's data, get rid of stored data in int[] format
            node->clearIntArray();

            // Data length in 32-bit words
            size_t appendDataWordLen = appendDataLen / 4;

            // Event contains structures of this type
            DataType eventDataType = node->getDataTypeObj();

            // Is there enough space to fit it?
            if ((byteBuffer->capacity() - byteBuffer->limit()) < appendDataLen) {
                // Not enough room so expand the buffer to fit + 25%.
                // Data copy and node adjustments too.
                expandBuffer(byteBuffer->limit() + appendDataLen);
            }

            //--------------------------------------------------------
            // Add new structure to end of event (nothing comes after)
            //--------------------------------------------------------

            // Position existing buffer at very end
            byteBuffer->limit(byteBuffer->capacity()).position(endPos);
            // Copy new data into buffer
            byteBuffer->put(addBuffer);
            // Get buffer ready for reading
            byteBuffer->flip();
            // Restore original position of add buffer
            addBuffer->position(origAddBufPos);

            //--------------------------------------------------------
            // Adjust event sizes in both node object and buffer.
            //--------------------------------------------------------

            // Position in byteBuffer of top level evio structure (event)
            size_t eventLenPos = 0;

            // Increase event size
            node->len     += appendDataWordLen;
            node->dataLen += appendDataWordLen;

            if (eventDataType.isBank()) {
                //std::cout << "event pos = " << eventLenPos << ", len = " << (node->len - appendDataWordLen) << ", set to " << node->len << std::endl;
                byteBuffer->putInt(eventLenPos, node->len);
            }
            else if (eventDataType.isStructure()) {
                // segment or tagsegment
                //std::cout << "event SEG/TAGSEG pos = " << eventLenPos << ", len = " << (node->len - appendDataWordLen) << ", set to " << node->len << std::endl;
                if (byteBuffer->order() == ByteOrder::ENDIAN_BIG) {
                    byteBuffer->putShort(eventLenPos + 2, (short) (node->len));
                }
                else {
                    byteBuffer->putShort(eventLenPos, (short) (node->len));
                }
            }
            else {
                throw EvioException("internal programming error");
            }

            // Create a node object from the data we just added to the byteBuffer
            std::shared_ptr<EvioNode> newNode = extractNode(byteBuffer, node, eventDataType, endPos, 0, false);

            // Add this new node to child and allNodes lists
            node->addChild(newNode);

            // Add its children into those lists too
            EvioNode::scanStructure(newNode);

            return byteBuffer;
    }


    /**
     * This method removes the data, represented by the given node, from the buffer.
     * It also marks the node and its descendants as obsolete. They must not be used
     * anymore.<p>
     *
     * @param removeNode  evio structure to remove from buffer
     * @return backing buffer updated to reflect the node removal
     * @throws EvioException if object closed;
     *                       if node was not found in any event;
     *                       if internal programming error
     */
    /*synchronized*/
    std::shared_ptr<ByteBuffer> EvioCompactStructureHandler::removeStructure(std::shared_ptr<EvioNode> removeNode) {

        // If we're removing nothing, then DO nothing
        if (removeNode == nullptr) {
            return byteBuffer;
        }

        if (closed) {
            throw EvioException("object closed");
        }
        else if (removeNode->isObsolete()) {
            return byteBuffer;
        }

        bool foundNode = false;
        // Place of the removed node in allNodes list. 0 means top level.
        int removeNodePlace = 0;

        // Locate the node to be removed ...
        for (auto n : node->allNodes) {
            // The first node in allNodes is the event node,
            // so do not increment removeNodePlace now.

            if (removeNode == n) {
                //System.out.println("removeStruct: found node to remove in struct, " + removeNodePlace);
                foundNode = true;
                break;
            }

            // Keep track of where inside the event it is
            removeNodePlace++;
        }

        if (!foundNode) {
            throw EvioException("removeNode not found");
        }

        // The data these nodes represent will be removed from the buffer,
        // so the node will be obsolete along with all its descendants.
        removeNode->setObsolete(true);

        //---------------------------------------------------
        // Move all data that came after removed
        // node to where removed node used to be.
        //---------------------------------------------------

        // Amount of data being removed
        size_t removeDataLen = removeNode->getTotalBytes();
        size_t removeWordLen = removeDataLen / 4;

        // Copy existing data from after the node being removed,
        size_t copyFromPos = removeNode->pos + removeDataLen;
        // To the beginning of the node to be removed
        size_t copyToPos = removeNode->pos;

        // If it's the last structure, things are real easy
        if (copyFromPos == byteBuffer->limit()) {
            // Just reset the limit
            byteBuffer->limit(copyToPos);
        }
        else {
            // Can't use duplicate(), must copy the backing buffer
            // (limit is already set to end of valid data).
            size_t bufferLim = byteBuffer->limit();
            ByteBuffer moveBuffer(bufferLim - copyFromPos);
            moveBuffer.order(byteBuffer->order());
            byteBuffer->limit(bufferLim).position(copyFromPos);
            moveBuffer.put(byteBuffer);

            // Prepare to move data currently sitting past the removed node
            moveBuffer.clear();

            // Get buffer ready to receiving data
            byteBuffer->limit(byteBuffer->capacity()).position(copyToPos);

            // Copy
            byteBuffer->put(moveBuffer);
            byteBuffer->flip();
        }

        // Reset some buffer position & limit values

        //-------------------------------------
        // By removing a structure, we need to shift the POSITIONS of all
        // structures that follow by the size of the deleted chunk.
        //-------------------------------------
        int i=0;

        for (auto n : node->allNodes) {
            // Removing one node may remove others, skip them all
            if (n->obsolete) {
                i++;
                continue;
            }

            // For events that come after, move all contained nodes
            if (i > removeNodePlace) {
                n->pos     -= removeDataLen;
                n->dataPos -= removeDataLen;
            }
            i++;
        }

        //--------------------------------------------
        // We need to update the lengths of all the
        // removed node's parent structures as well.
        //--------------------------------------------

        // Position of parent in new byteBuffer of event
        size_t parentPos;
        std::shared_ptr<EvioNode> removeParent, parent;
        removeParent = parent = removeNode->parentNode;

        while (parent != nullptr) {
            // Update event size
            parent->len -= removeWordLen;
            parent->dataLen -= removeWordLen;
            parentPos = parent->pos;
            // Since we're changing parent's data, get rid of stored data in int[] format
            parent->clearIntArray();

            // Parent contains data of this type
            if (parent->getDataTypeObj().isBank()) {
                //std::cout << "parent bank pos = " << parentPos << ", len was = " << (parent->len + removeWordLen) << ", now set to " << parent->len << std::endl;
                byteBuffer->putInt(parentPos, parent->len);
            }
            else if (parent->getDataTypeObj().isStructure()) {
                //std::cout << "parent seg/tagseg pos pos = " << parentPos << ", len was = " << (parent->len + removeWordLen) << ", now set to " << parent->len << std::endl;
                if (byteOrder == ByteOrder::ENDIAN_BIG) {
                    byteBuffer->putShort(parentPos + 2, (short) (parent->len));
                }
                else {
                    byteBuffer->putShort(parentPos, (short) (parent->len));
                }
            }
            else {
                throw EvioException("internal programming error");
            }

            parent = parent->parentNode;
        }

        // Remove node and node's children from lists
        if (removeParent != nullptr) {
            removeParent->removeChild(removeNode);
        }

        return byteBuffer;
    }


    /**
     * Get the data associated with an evio structure in ByteBuffer form.
     * The returned buffer is a view into this reader's buffer (no copy done).
     * Changes in one will affect the other.
     *
     * @param node evio structure whose data is to be retrieved
     * @throws EvioException if object closed
     * @return ByteBuffer object containing data. Position and limit are
     *         set for reading.
     */
    std::shared_ptr<ByteBuffer> EvioCompactStructureHandler::getData(std::shared_ptr<EvioNode> node) {
        return getData(node, false);
    }


    /**
     * Get the data associated with an evio structure in ByteBuffer form.
     * Depending on the copy argument, the returned buffer will either be
     * a copy of or a view into the data of this reader's buffer.<p>
     * This method is synchronized due to the bulk, relative gets &amp; puts.
     *
     * @param node evio structure whose data is to be retrieved
     * @param copy if <code>true</code>, then return a copy as opposed to a
     *        view into this reader object's buffer.
     * @throws EvioException if object closed
     * @return ByteBuffer object containing data. Position and limit are
     *         set for reading.
     */
    /*synchronized*/
    std::shared_ptr<ByteBuffer> EvioCompactStructureHandler::getData(std::shared_ptr<EvioNode> node, bool copy) {
        if (closed) {
            throw EvioException("object closed");
        }

        size_t lim = byteBuffer->limit();
        byteBuffer->limit(node->dataPos + 4*node->dataLen - node->pad).position(node->dataPos);

        if (copy) {
            auto newBuf = std::make_shared<ByteBuffer>(4*node->dataLen - node->pad);
            newBuf->order(byteOrder);

            // Relative get and put changes position in both buffers
            newBuf->put(byteBuffer);
            newBuf->flip();
            byteBuffer->limit(lim).position(0);
            return newBuf;
        }

        std::shared_ptr<ByteBuffer> buf = std::make_shared<ByteBuffer>(1);
        byteBuffer->slice(buf)->order(byteOrder);
        byteBuffer->limit(lim).position(0);
        return buf;
    }


    /**
     * Get an evio structure (bank, seg, or tagseg) in ByteBuffer form.
     * The returned buffer is a view into the data of this reader's buffer.<p>
     * This method is synchronized due to the bulk, relative gets &amp; puts.
     *
     * @param node node object representing evio structure of interest
     * @return ByteBuffer object containing bank's/event's bytes. Position and limit are
     *         set for reading.
     * @throws EvioException if node is null;
     */
    std::shared_ptr<ByteBuffer> EvioCompactStructureHandler::getStructureBuffer(std::shared_ptr<EvioNode> node) {
            return getStructureBuffer(node, false);
    }


    /**
     * Get an evio structure (bank, seg, or tagseg) in ByteBuffer form.
     * Depending on the copy argument, the returned buffer will either be
     * a copy of or a view into the data of this reader's buffer.<p>
     * This method is synchronized due to the bulk, relative gets &amp; puts.
     *
     * @param node node object representing evio structure of interest
     * @param copy if <code>true</code>, then return a copy as opposed to a
     *        view into this reader object's buffer.
     * @return ByteBuffer object containing structure's bytes. Position and limit are
     *         set for reading.
     * @throws EvioException if node is null;
     *                       if object closed
     */
    /*synchronized*/
    std::shared_ptr<ByteBuffer> EvioCompactStructureHandler::getStructureBuffer(std::shared_ptr<EvioNode> node, bool copy) {

        if (node == nullptr) {
            throw EvioException("node arg is null");
        }

        if (closed) {
            throw EvioException("object closed");
        }

        size_t lim = byteBuffer->limit();
        byteBuffer->limit(node->dataPos + 4*node->dataLen).position(node->pos);

        if (copy) {
            auto newBuf = std::make_shared<ByteBuffer>(node->getTotalBytes());
            newBuf->order(byteOrder);

            // Relative get and put changes position in both buffers
            newBuf->put(byteBuffer);
            newBuf->flip();
            byteBuffer->limit(lim).position(0);
            return newBuf;
        }

        std::shared_ptr<ByteBuffer> buf = std::make_shared<ByteBuffer>(1);
        byteBuffer->slice(buf)->order(byteOrder);
        byteBuffer->limit(lim).position(0);
        return buf;
    }


    /**
     * This method returns a vector of all
     * evio structures in buffer as EvioNode objects.
     *
     * @throws EvioException if object closed
     * @return vector of all evio structures in buffer as EvioNode objects.
     */
    /*synchronized*/
    std::vector<std::shared_ptr<EvioNode>> EvioCompactStructureHandler::getNodes() {
        if (closed) {
            throw EvioException("object closed");
        }
        return scanStructure();
    }


    /**
     * This method returns a vector of all child
     * evio structures in buffer as EvioNode objects.
     *
     * @throws EvioException if object closed
     * @return vector of all child evio structures in buffer as EvioNode objects.
     */
    /*synchronized*/
    std::vector<std::shared_ptr<EvioNode>> EvioCompactStructureHandler::getChildNodes() {
        if (closed) {
            throw EvioException("object closed");
        }
        scanStructure();
        return node->childNodes;
    }


    /**
     * This only sets the position to its initial value.
     */
    /*synchronized*/
    void EvioCompactStructureHandler::close() {
        byteBuffer->position(0);
        closed = true;
    }


}