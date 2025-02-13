//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "EvioNode.h"


namespace evio {


    // Initialize for debugging
    uint32_t EvioNode::staticId = 0;

    // Private methods to construct an EvioNode object

    /** Constructor when fancy features not needed. */
    EvioNode::EvioNode() {
        // Used for debugging only
        // id = staticId++;
        // std::cout << "creating node with id = " << id << "\n";

        // NOTE: the following line is only valid if this EvioNode object is created as a shared_ptr,
        // else it will throw an exception. This line was moved into the variadic template in EvioNode.h
        // to avoid a bad_weak_ptr exception.
        //        allNodes.push_back(shared_from_this());
    }


    /**
     * Constructor used when swapping data.
     * @param containingEvent event containing this node.
     * @param dummy this arg is only here to differentiate it from the other constructor
     *              taking a shared pointer of EvioNode. Use any value.
     */
    EvioNode::EvioNode(std::shared_ptr<EvioNode> & containingEvent, int dummy) : EvioNode() {
        scanned   = true;
        eventNode = containingEvent;
    }


//    /** Copy constructor. */
//    EvioNode::EvioNode(const EvioNode & src) {
//        copy(src);
//        //id = staticId++;
//    }


    /** Copy constructor. */
    EvioNode::EvioNode(std::shared_ptr<EvioNode> & src) {
        copy(*src);
        //id = staticId++;
    }


    /** Move constructor. */
    EvioNode::EvioNode(EvioNode && src) noexcept {

        if (this != &src) {
            len = src.len;
            tag = src.tag;
            num = src.num;
            pad = src.pad;
            pos = src.pos;
            type = src.type;
            dataLen = src.dataLen;
            dataPos = src.dataPos;
            dataType = src.dataType;
            recordPos = src.recordPos;
            place = src.place;

            buffer = src.buffer;

            izEvent = src.izEvent;
            obsolete = src.obsolete;
            scanned = src.scanned;

            data = std::move(src.data);
            eventNode = std::move(src.eventNode);
            parentNode = std::move(src.parentNode);

            allNodes = std::move(src.allNodes);
            childNodes = std::move(src.childNodes);

            recordNode = src.recordNode;
        }
    }


    /**
     * Constructor which creates an EvioNode associated with
     * an event (top level) evio container when parsing buffers
     * for evio data.
     *
     * @param pos         position of event in buffer (number of bytes)
     * @param place       containing event's place in buffer (starting at 0)
     * @param buffer      buffer containing this event
     * @param recordNode  block containing this event
     */
    EvioNode::EvioNode(size_t pos, uint32_t place, std::shared_ptr<ByteBuffer> & buffer, RecordNode & recordNode) : EvioNode() {
        this->pos = pos;
        this->place = place;
        this->recordNode = recordNode;
        this->buffer = buffer;
        // This is an event by definition
        this->izEvent = true;
        // Event is a Bank by definition
        this->type = DataType::BANK.getValue();
    }


    /**
     * Constructor which creates an EvioNode associated with
     * an event (top level) evio container when parsing buffers
     * for evio data.
     *
     * @param pos        position of event in buffer (number of bytes).
     * @param place      containing event's place in buffer (starting at 0).
     * @param recordPos  position of record containing this node.
     * @param buffer     buffer containing this event.
     */
    EvioNode::EvioNode(size_t pos, uint32_t place, size_t recordPos, std::shared_ptr<ByteBuffer> & buffer) : EvioNode() {
        this->pos = pos;
        this->place = place;
        this->recordPos = recordPos;
        this->buffer = buffer;
        this->izEvent = true;
        this->type = DataType::BANK.getValue();
    }


    /**
     * Constructor which creates an EvioNode in the CompactEventBuilder.
     *
     * @param tag        the tag for the event (or bank) header.
     * @param num        the num for the event (or bank) header.
     * @param pos        position of event in buffer (bytes).
     * @param dataPos    position of event's data in buffer (bytes.)
     * @param type       the type of this evio structure.
     * @param dataType   the data type contained in this evio event.
     * @param buffer     buffer containing this event.
     */
    EvioNode::EvioNode(uint16_t tag, uint8_t num, size_t pos, size_t dataPos,
                       DataType const & type, DataType const & dataType,
                       std::shared_ptr<ByteBuffer> & buffer) : EvioNode() {
        this->tag = tag;
        this->num = num;
        this->pos = pos;
        this->dataPos = dataPos;

        this->type = type.getValue();
        this->dataType = dataType.getValue();
        this->buffer = buffer;
    }


    /** Copy method. */
    void EvioNode::copy(const EvioNode & src) {

        if (this != &src) {
            len = src.len;
            tag = src.tag;
            num = src.num;
            pad = src.pad;
            pos = src.pos;
            type = src.type;
            dataLen = src.dataLen;
            dataPos = src.dataPos;
            dataType = src.dataType;
            recordPos = src.recordPos;
            place = src.place;

            buffer = src.buffer;

            izEvent  = src.izEvent;
            obsolete = src.obsolete;
            scanned  = src.scanned;

            data       = src.data;
            eventNode  = src.eventNode;
            parentNode = src.parentNode;

            // Replace elements from this with src's
            allNodes   = src.allNodes;
            childNodes = src.childNodes;

            recordNode = src.recordNode;
        }
    }


//    /**
//     * Assignment operator.
//     * @param src right side object.
//     * @return left side object.
//     */
//    EvioNode & EvioNode::operator=(const EvioNode& src) {
//        if (this != &src) {
//            copy(src);
//        }
//        return *this;
//    }


    /**
     * Comparison operator.
     * @param src right side object.
     * @return left side object.
     */
    bool EvioNode::operator==(const EvioNode & src) const {
        // Same object, must be equal
        return this == &src;
    }

    /**
     * Comparison operator.
     * @param src right side object.
     * @return left side object.
     */
    bool EvioNode::operator==(std::shared_ptr<const EvioNode> & src) const {
        // Same object
        return shared_from_this() == src;
    }


    //-------------------------------
    // Methods
    //-------------------------------


    /**
     * Shift the positions (pos, dataPos, and recordPos) of this node and its
     * children by a fixed amount.
     * Useful, for example, when the contents of one buffer is copied into another.
     *
     * @param deltaPos number of bytes to add to existing positions.
     * @return reference to this object.
     */
    void EvioNode::shift(int deltaPos) {
        pos       += deltaPos;
        dataPos   += deltaPos;
        recordPos += deltaPos;

        for (std::shared_ptr<EvioNode> & kid : childNodes) {
            kid->shift(deltaPos);
        }
    }


    /**
     * Get a string representation of this object.
     * @return a string representation of this object.
     */
    std::string EvioNode::toString() const {
        std::stringstream ss;

        ss << "tag = "          << tag;
        ss << ", num = "        << +num;
        ss << ", type = "       << getTypeObj().toString();
        ss << ", dataType = "   << getDataTypeObj().toString();
        ss << ", pos = "        << pos;
        ss << ", dataPos = "    << dataPos;
        ss << ", len = "        << len;
        ss << ", dataLen = "    << dataLen;
        ss << ", recordPos = "  << recordPos;

        return ss.str();
    }


    /**
     * Copy parameters from a parent node when scanning evio data and
     * placing into EvioNode obtained from an EvioNodeSource.
     * @param parent parent of the object.
     */
    void EvioNode::copyParentForScan(std::shared_ptr<EvioNode> & parent) {
        recordNode = parent->recordNode;
        buffer     = parent->buffer;
        allNodes   = parent->allNodes;
        eventNode  = parent->eventNode;
        place      = parent->place;
        scanned    = parent->scanned;
        recordPos  = parent->recordPos;
        parentNode = parent;
    }


    //TODO: figure out which clears are needed
    /**
     * Clear childNodes.
     * Place only this or eventNode object into the allNodes.
     */
    void EvioNode::clearLists() {
        childNodes.clear();

        allNodes.clear();
        // Remember to add event's node into list
        if (eventNode == nullptr) {
            allNodes.push_back(getThis());
        }
        else {
            allNodes.push_back(eventNode);
        }
    }


    /** Clear all data in this object. */
    void EvioNode::clear() {
        allNodes.clear();
        len = tag = num = pad = pos = type = dataLen = dataPos = dataType = place = recordPos = 0;
        clearObjects();
    }


    /** Empty all lists and remove all other objects from this object. */
    void EvioNode::clearObjects() {
        childNodes.clear();

        izEvent = obsolete = scanned = false;
        data.clear();
        recordNode.clear();
        buffer->clear();
        eventNode  = nullptr;
        parentNode = nullptr;
    }


    /** Only clear the data vector. */
    void EvioNode::clearIntArray() {data.clear();}


    //-------------------------------
    // Setters & Getters
    //-------------------------------


    /**
     * Set the buffer.
     * @param buf buffer associated with this object.
     */
    void EvioNode::setBuffer(std::shared_ptr<ByteBuffer> & buf) {buffer = buf;}


    /**
     * Once this node is cleared, it may be reused and then re-initialized
     * with this method.
     *
     * @param position    position in buffer
     * @param plc         place of event in buffer (starting at 0)
     * @param buf         buffer to examine
     * @param recNode     object holding data about header of block containing event
     */
    void EvioNode::setData(size_t position, uint32_t plc,
                           std::shared_ptr<ByteBuffer> & buf, RecordNode & recNode) {
        buffer     = buf;
        recordNode = recNode;
        pos        = position;
        place      = plc;
        izEvent    = true;
        type       = DataType::BANK.getValue();
        allNodes.push_back(getThis());
    }


    /**
     * Once this node is cleared, it may be reused and then re-initialized
     * with this method.
     *
     * @param position   position in buffer
     * @param plc        place of event in buffer (starting at 0)
     * @param recPos     place of event in containing record (bytes)
     * @param buf        buffer to examine
     */
    void EvioNode::setData(size_t position, uint32_t plc, size_t recPos, std::shared_ptr<ByteBuffer> & buf) {
        buffer     = buf;
        recordPos  = recPos;
        pos        = position;
        place      = plc;
        izEvent    = true;
        type       = DataType::BANK.getValue();
        allNodes.push_back(getThis());
    }


    //-----------------------------------------------
    // Static methods
    //-----------------------------------------------


    /**
     * This method extracts an EvioNode object representing an
     * evio event (top level evio bank) from a given buffer, a
     * location in the buffer, and a few other things. An EvioNode
     * object represents an evio container - either a bank, segment,
     * or tag segment.
     *
     * @param buffer     buffer to examine
     * @param recNode object holding data about block header
     * @param position   position in buffer
     * @param place      place of event in buffer (starting at 0)
     *
     * @return EvioNode object containing evio event information
     * @throws EvioException if not enough data in buffer to read evio bank header (8 bytes).
     */
    std::shared_ptr<EvioNode> & EvioNode::extractEventNode(std::shared_ptr<ByteBuffer> & buffer,
                                                         RecordNode & recNode,
                                                         size_t position, uint32_t place) {

        // Make sure there is enough data to at least read evio header
        if (buffer->remaining() < 8) {
            throw EvioException("buffer underflow");
        }

        // Store evio event info, without de-serializing, into EvioNode object
        // Create node here and pass reference back
        auto node = createEvioNode(position, place, buffer, recNode);
        return extractNode(node, position);
    }


    /**
     * This method extracts an EvioNode object representing an
     * evio event (top level evio bank) from a given buffer, a
     * location in the buffer, and a few other things. An EvioNode
     * object represents an evio container - either a bank, segment,
     * or tag segment.
     *
     * @param buffer       buffer to examine
     * @param recPosition  position of containing record
     * @param position     position in buffer
     * @param place        place of event in buffer (starting at 0)
     *
     * @return EvioNode object containing evio event information
     * @throws EvioException if not enough data in buffer to read evio bank header (8 bytes).
     */
    std::shared_ptr<EvioNode> & EvioNode::extractEventNode(std::shared_ptr<ByteBuffer> & buffer,
                                                         size_t recPosition,
                                                         size_t position, uint32_t place) {

        // Make sure there is enough data to at least read evio header
        if (buffer->remaining() < 8) {
            throw EvioException("buffer underflow");
        }

        // Store evio event info, without de-serializing, into EvioNode object
        // Create node here and pass reference back
        auto node = createEvioNode(position, place, recPosition, buffer);
        return extractNode(node, position);
    }


    /**
     * This method populates an EvioNode object that will represent an
     * evio bank from that same node containing a reference to the
     * backing buffer and given a position in that buffer.
     *
     * @param bankNode   EvioNode to represent a bank and containing,
     *                   at least, a reference to backing buffer.
     * @param position   position in backing buffer
     *
     * @return EvioNode bankNode arg filled with appropriate data.
     * @throws EvioException if not enough data in buffer to read evio bank header (8 bytes).
     */
    std::shared_ptr<EvioNode> & EvioNode::extractNode(std::shared_ptr<EvioNode> & bankNode, size_t position) {

        // Make sure there is enough data to at least read evio header
        ByteBuffer* buffer = bankNode->buffer.get();
        if (buffer->remaining() < 8) {
            throw EvioException("buffer underflow");
        }

        // Get length of current bank
        uint32_t length = buffer->getUInt(position);
        bankNode->len = length;
        bankNode->pos = position;
        bankNode->type = DataType::BANK.getValue();

        // Position of data for a bank
        bankNode->dataPos = position + 8;
        // Len of data for a bank
        bankNode->dataLen = length - 1;
//std::cout << "          extractNode: len = " << length << ", pos = " << position << ", type = " << DataType::BANK.toString() <<
//             ", dataPos = " << bankNode->dataPos << ", dataLen = " << bankNode->dataLen << std::endl;

        // Make sure there is enough data to read full bank
        // even though it is NOT completely read at this time.
        if (buffer->remaining() < 4*(length + 1)) {
//std::cout << "ERROR: remaining = " << buffer->remaining() <<
//             ", node len bytes = " << ( 4*(length + 1)) << ", len = " << length << std::endl;
            throw EvioException("buffer underflow");
        }

        // Hop over length word
        position += 4;

        // Read and parse second header word
        uint32_t word = buffer->getInt(position);
        bankNode->tag = (word >> 16) & 0xffff;
        uint32_t dt  = (word >> 8) & 0xff;
        bankNode->dataType = dt & 0x3f;
        bankNode->pad = dt >> 6;
        bankNode->num = word & 0xff;

        return bankNode;
    }


    /**
     * This recursive method stores, in the given EvioNode, all the information
     * about an evio structure and its children found in that node
     * (representing all or part of an underlying ByteBuffer).
     * It uses absolute gets so the underlying buffer's position does <b>not</b> change.
     * In the vector of all nodes contained in each EvioNode object (including the top-level object),
     * the ordering is according to their placement in the buffer (which happens to be depth-first).
     * This method does a depth-first search (DFS).
     *
     * @param node node being scanned
     */
    void EvioNode::scanStructure(std::shared_ptr<EvioNode> & node) {

        uint32_t dType = node->dataType;

        // If node does not contain containers, return since we can't drill any further down
        if (!DataType::isStructure(dType)) {
            return;
        }

        // Start at beginning position of evio structure being scanned
        size_t position = node->dataPos;
        // Don't go past the data's end which is (position + length)
        // of evio structure being scanned in bytes.
        size_t endingPos = position + 4*node->dataLen;
        // Buffer we're using
        auto buffer = node->buffer;

        uint32_t dt, dataType, dataLen, len, word;

        // Do something different depending on what node contains
        if (DataType::isBank(dType)) {
            // Extract all the banks from this bank of banks.
            // Make allowance for reading header (2 ints).
            endingPos -= 8;
            while (position <= endingPos) {

                // Copy node for setting stuff that's the same as the parent
                auto kidNode = createEvioNode(node);
                // Clear children & data
                kidNode->childNodes.clear();
                kidNode->data.clear();

                // Read first header word
                len = buffer->getInt(position);
                kidNode->pos = position;

                // Len of data (no header) for a bank
                dataLen = len - 1;
                position += 4;

                // Read and parse second header word
                word = buffer->getInt(position);
                position += 4;
                kidNode->tag = (word >> 16) & 0xffff;
                dt = (word >> 8) & 0xff;
                dataType = dt & 0x3f;
                kidNode->pad = dt >> 6;
                kidNode->num = word & 0xff;


                kidNode->len = len;
                kidNode->type = DataType::BANK.getValue();  // This is a bank
                kidNode->dataLen = dataLen;
                kidNode->dataPos = position;
                kidNode->dataType = dataType;
                kidNode->izEvent = false;

                // Create the tree structure
                kidNode->parentNode = node;
                // Add this to list of children and to list of all nodes in the event
                node->addChild(kidNode);

                // Only scan through this child if it's a container
                if (DataType::isStructure(dataType)) {
                    scanStructure(kidNode);
                }

                // Set position to start of next header (hop over kid's data)
                position += 4 * dataLen;
            }
        }
        else if (DataType::isSegment(dType)) {

            // Extract all the segments from this bank of segments.
            // Make allowance for reading header (1 int).
            endingPos -= 4;
            while (position <= endingPos) {
                // Copy node for setting stuff that's the same as the parent
                auto kidNode = createEvioNode(node);
                // Clear children & data
                kidNode->childNodes.clear();
                kidNode->data.clear();

                kidNode->pos = position;

                word = buffer->getInt(position);
                position += 4;
                kidNode->tag = (word >> 24) & 0xff;
                dt = (word >> 16) & 0xff;
                dataType = dt & 0x3f;
                kidNode->pad = dt >> 6;
                len = word & 0xffff;


                kidNode->num      = 0;
                kidNode->len      = len;
                kidNode->type     = DataType::SEGMENT.getValue();  // This is a segment
                kidNode->dataLen  = len;
                kidNode->dataPos  = position;
                kidNode->dataType = dataType;
                kidNode->izEvent  = false;

                kidNode->parentNode = node;
                node->addChild(kidNode);

                if (DataType::isStructure(dataType)) {
                    scanStructure(kidNode);
                }

                position += 4*len;
            }
        }
        // Only one type of structure left - tagsegment
        else {

            // Extract all the tag segments from this bank of tag segments.
            // Make allowance for reading header (1 int).
            endingPos -= 4;
            while (position <= endingPos) {
                // Copy node for setting stuff that's the same as the parent
                auto kidNode = createEvioNode(node);
                // Clear children & data
                kidNode->childNodes.clear();
                kidNode->data.clear();

                kidNode->pos = position;

                word = buffer->getInt(position);
                position += 4;
                kidNode->tag = (word >> 20) & 0xfff;
                dataType    = (word >> 16) & 0xf;
                len         =  word & 0xffff;

                kidNode->pad      = 0;
                kidNode->num      = 0;
                kidNode->len      = len;
                kidNode->type     = DataType::TAGSEGMENT.getValue();  // This is a tag segment
                kidNode->dataLen  = len;
                kidNode->dataPos  = position;
                kidNode->dataType = dataType;
                kidNode->izEvent  = false;

                kidNode->parentNode = node;
                node->addChild(kidNode);

                if (DataType::isStructure(dataType)) {
                    scanStructure(kidNode);
                }

                position += 4*len;
            }
        }
    }


    //----------------------------------
    // End of static methods
    //----------------------------------


    /**
     * Add a node to the end of the list of all nodes contained in event.
     * @param node child node to add to the list of all nodes
     */
    void EvioNode::addToAllNodes(std::shared_ptr<EvioNode>& node) {
        auto & allNodeVector = getAllNodes();
        allNodeVector.push_back(node);
        //allNodes.push_back(node);
    }


    /**
     * Remove a node & all of its descendants from the list of all nodes
     * contained in event.
     * @param node node & descendants to remove from the list of all nodes
     */
    void EvioNode::removeFromAllNodes(std::shared_ptr<EvioNode> & node) {

        // Remove from allNodes (very strange in C++ !)
        allNodes.erase(std::remove(allNodes.begin(), allNodes.end(), node), allNodes.end());

        // Remove descendants also
        for (std::shared_ptr<EvioNode> & n : node->childNodes) {
            removeFromAllNodes(n);
        }

        // NOTE: only one "allNodes" exists - at event/top level
    }


    /**
     * Add a child node to the end of the child list and
     * to the list of all nodes contained in event.
     * This is called internally in sequence so every node ends up in the right
     * place in allNodes. When the user adds a structure by calling
     * {@link EvioCompactReader#addStructure(size_t, ByteBuffer &)}, the structure or node
     * gets added at the very end - as the last child of the event.
     *
     * @param node child node to add to the end of the child list.
     */
    void EvioNode::addChild(std::shared_ptr<EvioNode> & node) {

        // Make sure we have each member of the tree setting the proper top level event
        if (eventNode == nullptr) {
            // If this IS the top level event ...
            node->eventNode = getThis();
        }
        else {
            node->eventNode = eventNode;
        }

        childNodes.push_back(node);
        addToAllNodes(node);
    }


    /**
     * Remove a node from this child list and, along with its descendants,
     * from the list of all nodes contained in event.
     * If not a child, do nothing.
     * @param node node to remove from child & allNodes lists.
     */
    void EvioNode::removeChild(std::shared_ptr<EvioNode> & node) {

        uint64_t sizeBefore = childNodes.size();
        childNodes.erase(std::remove(childNodes.begin(), childNodes.end(), node), childNodes.end());
        uint64_t sizeAfter = childNodes.size();

        // Remove from allNodes too since it was contained in childNodes
        if (sizeBefore > sizeAfter) {
            removeFromAllNodes(node);
        }
    }


    /**
     * Get the object representing the record.
     * @return object representing the record.
     */
    RecordNode & EvioNode::getRecordNode() {return recordNode;}


    /**
     * Has the data this node represents in the buffer been removed?
     * @return true if node no longer represents valid buffer data, else false.
     */
    bool EvioNode::isObsolete() const {return obsolete;}


    /**
     * Set whether this node & descendants are now obsolete because the
     * data they represent in the buffer has been removed.
     * Only for internal use.
     * @param ob true if node & descendants no longer represent valid
     *                 buffer data, else false.
     */
    void EvioNode::setObsolete(bool ob) {
        obsolete = ob;

        // Set for all descendants.
        for (std::shared_ptr<EvioNode> & n : childNodes) {
            n->setObsolete(ob);
        }
    }


    /**
     * Get the vector of all nodes that this node contains,
     * always including itself. This is meaningful only if this
     * node has been scanned, otherwise it contains only itself.
     *
     * @return list of all nodes that this node contains.
     */
    std::vector<std::shared_ptr<EvioNode>> & EvioNode::getAllNodes() {
        if (eventNode == nullptr) {
//std::cout << "\ngetAllNodes: WARNING: get node #" << id << " vector\n\n";
            return allNodes;
        }
//std::cout << "getAllNodes: get EVENT node #" << eventNode->id << " vector\n";
        return eventNode->allNodes;
    }


    /**
     * Get the vector of all child nodes that this node contains.
     * This is meaningful only if this node has been scanned,
     * otherwise it is null.
     *
     * @return list of all child nodes that this node contains;
     *         empty if not scanned or no children
     */
    std::vector<std::shared_ptr<EvioNode>> & EvioNode::getChildNodes() {return childNodes;}


    /**
     * Get the list of all descendant nodes that this node contains -
     * not only the immediate children.
     * This is meaningful only if this node has been scanned,
     * otherwise nothing is added to the given list.
     *
     * @param descendants list to be filled with EvioNodes of all descendants
     */
    void EvioNode::getAllDescendants(std::vector<std::shared_ptr<EvioNode>> & descendants) {
        // Add children recursively
        for (std::shared_ptr<EvioNode> & n : childNodes) {
            descendants.push_back(n);
            n->getAllDescendants(descendants);
        }
    }


    /**
     * Get the child node at the given index (starts at 0).
     * This is meaningful only if this node has been scanned,
     * otherwise it is null.
     *
     * @param index index of child node
     * @return child node at the given index;
     *         null if not scanned or no child at that index
     */
    std::shared_ptr<EvioNode> EvioNode::getChildAt(uint32_t index) {
        if (childNodes.size() < index+1) return nullptr;
        return childNodes[index];
    }


    /**
     * Get the number all children that this node contains.
     * This is meaningful only if this node has been scanned,
     * otherwise it returns 0.
     *
     * @return number of children that this node contains;
     *         0 if not scanned
     */
    uint32_t EvioNode::getChildCount() const {return childNodes.size();}


    /**
    * Get the number of children that this node contains at a single
    * level of the evio tree.
    * This is meaningful only if this node has been scanned,
    * otherwise it returns 0.
    *
    * @param level go down this many levels in evio structure to count children.
    *              A level of 0 means immediate children, 1 means
    *              grandchildren, etc.
    * @return number of children that this node contains at the given level;
    *         0 if not scanned or level &lt; 0.
    */
    uint32_t EvioNode::getChildCount(int level) {
        if (childNodes.empty() || level < 0) {
            return 0;
        }

        if (level == 0) {
            return childNodes.size();
        }

        uint32_t kidCount = 0;
        for (std::shared_ptr<EvioNode> & n : childNodes) {
            kidCount += n->getChildCount(level - 1);
        }

        return kidCount;
    }


    /**
     * Get the buffer containing this node.
     * Note, buffer's position and limit may not be set according to this node's position and limit.
     * @return buffer containing this node.
     */
    std::shared_ptr<ByteBuffer> & EvioNode::getBuffer() {return buffer;}


    /**
     * Get the length of this evio structure (not including length word itself)
     * in 32-bit words.
     * @return length of this evio structure (not including length word itself)
     *         in 32-bit words
     */
    uint32_t EvioNode::getLength() const {return len;}


    /**
     * Get the length of this evio structure including entire header in bytes.
     * @return length of this evio structure including entire header in bytes.
     */
    uint32_t EvioNode::getTotalBytes() const {return 4*dataLen + dataPos - pos;}

    /**
     * Get the tag of this evio structure.
     * @return tag of this evio structure
     */
    uint16_t EvioNode::getTag() const {return tag;}

    /**
     * Get the num of this evio structure.
     * Will be zero for tagsegments.
     * @return num of this evio structure
     */
    uint8_t  EvioNode::getNum() const {return num;}

    /**
     * Get the padding of this evio structure.
     * Will be zero for segments and tagsegments.
     * @return padding of this evio structure
     */
    uint32_t EvioNode::getPad() const {return pad;}

    /**
     * Get the file/buffer byte position of this evio structure.
     * @return file/buffer byte position of this evio structure
     */
    size_t EvioNode::getPosition() const {return pos;}

    /**
     * Get the evio type of this evio structure, not what it contains.
     * Call {@link DataType#getDataType(uint32_t)} on the
     * returned value to get the object representation.
     * @return evio type of this evio structure, not what it contains
     */
    uint32_t EvioNode::getType() const {return type;}

    /**
     * Get the evio type of this evio structure as an object.
     * @return evio type of this evio structure as an object.
     */
    DataType EvioNode::getTypeObj() const {return DataType::getDataType(type);}

    /**
     * Get the length of this evio structure's data only (no header words)
     * in 32-bit words.
     * @return length of this evio structure's data only (no header words)
     *         in 32-bit words.
     */
    uint32_t EvioNode::getDataLength() const {return dataLen;}

    /**
     * Get the file/buffer byte position of this evio structure's data.
     * @return file/buffer byte position of this evio structure's data
     */
    size_t EvioNode::getDataPosition() const {return dataPos;}

    /**
     * Get the evio type of the data this evio structure contains.
     * Call {@link DataType#getDataType(uint32_t)} on the
     * returned value to get the object representation.
     * @return evio type of the data this evio structure contains
     */
    uint32_t EvioNode::getDataType() const {return dataType;}

    /**
     * Get the evio type of the data this evio structure contains as an object.
     * @return evio type of the data this evio structure contains as an object.
     */
    DataType EvioNode::getDataTypeObj() const {return DataType::getDataType(dataType);}

    /**
     * Get the file/buffer byte position of the record containing this node.
     * @since version 6.
     * @return file/buffer byte position of the record containing this node.
     */
    size_t EvioNode::getRecordPosition() const {return recordPos;}

    /**
     * Get the place of containing event in file/buffer. First event = 0, second = 1, etc.
     * Only for internal use.
     * @return place of containing event in file/buffer.
     */
    uint32_t EvioNode::getPlace() const {return place;}

    /**
     * Get this node's parent node.
     * @return this node's parent node or null if none.
     */
    std::shared_ptr<EvioNode> EvioNode::getParentNode() {return parentNode;}

    /**
     * If this object represents an event (top-level, evio bank),
     * then returns its number (place in file or buffer) starting
     * with 1. If not, return -1.
     * @return event number if representing an event, else -1
     */
    uint32_t EvioNode::getEventNumber() const {return (place + 1);}


    /**
     * Does this object represent an event?
     * @return <code>true</code> if this object represents an event,
     *         else <code>false</code>
     */
    bool EvioNode::isEvent() const {return izEvent;}


    /**
     * Has this object been scanned (i.e. has all the information
     * about this node's children been parsed and stored) ?
     * @return <code>true</code> if this object has been scanned,
     *         else <code>false</code>
     */
    bool EvioNode::getScanned() const {return scanned;}


    /**
     * Update the length of this node in the buffer and all its parent nodes as well.
     * For internal use only.
     * @param deltaLen change in length (words). Negative value reduces lengths.
     */
    void EvioNode::updateLengths(int32_t deltaLen) {

        EvioNode* node = this;
        uint32_t typ = getType();
        uint32_t length;

        if ((typ == DataType::BANK.getValue()) || (typ == DataType::ALSOBANK.getValue())) {
            length = buffer->getUInt(node->pos) + deltaLen;
            buffer->putInt(node->pos, length);
        }
        else if ((typ == DataType::SEGMENT.getValue())     ||
                 (typ == DataType::ALSOSEGMENT.getValue()) ||
                 (typ == DataType::TAGSEGMENT.getValue()))   {

            if (buffer->order() == ByteOrder::ENDIAN_BIG) {
                length = (buffer->getShort(node->pos+2) & 0xffff) + deltaLen;
                buffer->putShort(node->pos+2, (short)length);
            }
            else {
                length = (buffer->getShort(node->pos) & 0xffff) + deltaLen;
                buffer->putShort(node->pos, (short)length);
            }
        }
    }


    /**
     * Update, in the buffer, the tag of the structure header this object represents.
     * Sometimes it's necessary to go back and change the tag of an evio
     * structure that's already been written. This will do that.
     *
     * @param newTag new tag value
     */
    void EvioNode::updateTag(uint16_t newTag) {

        if ((type == DataType::BANK.getValue()) ||
            (type == DataType::ALSOBANK.getValue())) {
            if (buffer->order() == ByteOrder::ENDIAN_BIG) {
                buffer->putShort(pos + 4, (short) newTag);
            } else {
                buffer->putShort(pos + 6, (short) newTag);
            }
            return;
        }
        else  if ((type == DataType::SEGMENT.getValue()) ||
                  (type == DataType::ALSOSEGMENT.getValue())) {
            if (buffer->order() == ByteOrder::ENDIAN_BIG) {
                buffer->put(pos, (uint8_t) newTag);
            } else {
                buffer->put(pos + 3, (uint8_t) newTag);
            }
            return;
        }
        else if (type == DataType::TAGSEGMENT.getValue()) {
            auto compositeWord = (short) ((tag << 4) | (dataType & 0xf));
            if (buffer->order() == ByteOrder::ENDIAN_BIG) {
                buffer->putShort(pos, compositeWord);
            }
            else {
                buffer->putShort(pos+2, compositeWord);
            }
            return;
        }
    }


    /**
     * Update, in the buffer, the num of the bank header this object represents.
     * Sometimes it's necessary to go back and change the num of an evio
     * structure that's already been written. This will do that.
     * @param newNum new num value
     */
    void EvioNode::updateNum(uint8_t newNum) {

        if ((type == DataType::BANK.getValue()) ||
            (type == DataType::ALSOBANK.getValue())) {

            if (buffer->order() == ByteOrder::ENDIAN_BIG) {
                buffer->put(pos+7, newNum);
            }
            else {
                buffer->put(pos+4, newNum);
            }
            return;
        }
    }


    /**
     * Get the data associated with this node in ByteBuffer form.
     * Depending on the copy argument, the given buffer will be filled with either
     * a copy of or a view into this node's buffer.
     * Position and limit are set for reading.<p>
     *
     * @param dest buffer in which to place data.
     * @param copy if <code>true</code>, then return a copy as opposed to a
     *             view into this node's buffer.
     * @return dest arg ByteBuffer containing data.
     *         Position and limit are set for reading.
     */
    ByteBuffer & EvioNode::getByteData(ByteBuffer & dest, bool copy) {
        if (copy) {
            // copy data & everything else
            dest.copyData(buffer, dataPos, (dataPos + 4*dataLen - pad));
        } else {
            // dest now has shared pointer to buffer's data
            buffer->duplicate(dest);
            dest.limit(dataPos + 4*dataLen - pad).position(dataPos);
        }
        return dest;
    }


    /**
     * Get the data associated with this node in ByteBuffer form.
     * Depending on the copy argument, the given buffer will be filled with either
     * a copy of or a view into this node's buffer.
     * Position and limit are set for reading.<p>
     *
     * @param dest buffer in which to place data.
     * @param copy if <code>true</code>, then return a copy as opposed to a
     *             view into this node's buffer.
     * @return dest arg ByteBuffer containing data.
     *         Position and limit are set for reading.
     */
    std::shared_ptr<ByteBuffer> & EvioNode::getByteData(std::shared_ptr<ByteBuffer> & dest, bool copy) {
        auto & buff = *dest;
        getByteData(buff, copy);
        return dest;
    }


    /**
     * Get the data associated with this node in ByteBuffer form.
     * Depending on the copy argument, the given buffer will be filled with either
     * a copy of or a view into this node's buffer.
     * Position and limit are set for reading.<p>
     *
     * @param copy if <code>true</code>, then return a copy as opposed to a
     *             view into this node's buffer.
     * @return newly created ByteBuffer containing data.
     *         Position and limit are set for reading.
     */
    std::shared_ptr<ByteBuffer> EvioNode::getByteData(bool copy) {
        // The tricky thing to keep in mind is that the buffer
        // which this node uses may also be used by other nodes.
        // That means setting its limit and position may interfere
        // with other operations being done to it.
        // So even though it is less efficient, use a duplicate of the
        // buffer which gives us our own limit and position.
        ByteOrder order = buffer->order();
        auto buffer2 = buffer->duplicate();
        buffer2->order(order);
        buffer2->limit(dataPos + 4*dataLen - pad).position(dataPos);
//std::cout << "getByteData: dataPos (pos) = " << dataPos << ", lim = " << (dataPos + 4*dataLen - pad) << std::endl;
        if (copy) {
            auto newBuf = std::make_shared<ByteBuffer>(4*dataLen - pad);
            newBuf->order(order);
            newBuf->put(buffer2);
            newBuf->flip();
            return newBuf;
        }

        return buffer2;
    }


    /**
     * Get the data associated with this node as an 32-bit integer vector.
     * Store it and return it in future calls (like in event builder).
     * If data is of a type less than 32 bits, the last int will be junk.
     *
     * @return integer array containing data.
     */
    std::vector<uint32_t> & EvioNode::getIntData() {
        if (data.empty()) {
            for (size_t i = dataPos; i < dataPos + 4 * dataLen; i += 4) {
                data[(i - dataPos) / 4] = buffer->getInt(i);
            }
        }
        return data;
    }


    /**
     * Get the data associated with this node as an 32-bit integer vector.
     * Place data in the given vector.
     * If data is of a type less than 32 bits, the last int will be junk.
     *
     * @param intData vector in which to store data.
     */
    void EvioNode::getIntData(std::vector<uint32_t> & intData) {
        intData.clear();

        for (size_t i = dataPos; i < dataPos + 4*dataLen; i+= 4) {
            intData[(i-dataPos)/4] = buffer->getInt(i);
        }

        // added dataLen number of elements
    }


    /**
     * Get the data associated with this node as an 64-bit integer vector.
     * Place data in the given vector.
     * If data is of a type less than 64 bits, the last element may be junk.
     *
     * @param longData vector in which to store data.
     */
    void EvioNode::getLongData(std::vector<uint64_t> & longData) {
        longData.clear();

        for (size_t i = dataPos; i < dataPos + 4*dataLen; i+= 8) {
            longData[(i-dataPos)/8] = buffer->getLong(i);
        }
    }


    /**
     * Get the data associated with this node as an 16-bit integer vector.
     * Place data in the given vector.
     * If data is of a type less than 16 bits, the last element may be junk.
     *
     * @param shortData vectro in which to store data.
     */
    void EvioNode::getShortData(std::vector<uint16_t> & shortData) {
        shortData.clear();

        for (size_t i = dataPos; i < dataPos + 4*dataLen; i+= 2) {
            shortData[(i-dataPos)/2] = buffer->getShort(i);
        }
    }


    /**
     * Get this node's entire evio structure in ByteBuffer form.
     * Depending on the copy argument, the returned buffer will either have
     * a copy of or a view into the data of this node's buffer.
     * Position and limit are set for reading.
     *
     * @param dest ByteBuffer provided to hold the retrieved evio structure.
     * @param copy if <code>true</code>, then use a copy of as opposed to a
     *        view into this node's buffer.
     * @return dest arg ByteBuffer containing evio structure's bytes.
     *         Position and limit are set for reading.
     */
    ByteBuffer & EvioNode::getStructureBuffer(ByteBuffer & dest, bool copy) {
        if (copy) {
            // copy data & everything else
            dest.copyData(buffer, pos, (dataPos + 4 * dataLen));
        } else {
            // dest now has shared pointer to buffer's data
            buffer->duplicate(dest);
            dest.limit(dataPos + 4 * dataLen).position(pos);
        }
        return dest;
    }


    /**
     * Get this node's entire evio structure in ByteBuffer form.
     * Depending on the copy argument, the returned buffer will either have
     * a copy of or a view into the data of this node's buffer.
     * Position and limit are set for reading.
     *
     * @param dest ByteBuffer provided to hold the retrieved evio structure.
     * @param copy if <code>true</code>, then use a copy of as opposed to a
     *        view into this node's buffer.
     * @return dest arg ByteBuffer containing evio structure's bytes.
     *         Position and limit are set for reading.
     */
    std::shared_ptr<ByteBuffer> & EvioNode::getStructureBuffer(std::shared_ptr<ByteBuffer> & dest, bool copy) {
        auto & buff = *dest;
        getStructureBuffer(buff, copy);
        return dest;
    }

}

