//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "CompactEventBuilder.h"


namespace evio {



    /**
     * This is the constructor to use for building a new event
     * (just the event in a buffer, not the full evio file format).
     * A buffer is created in this constructor.
     *
     * @param bufferSize size of byte buffer (in bytes) to create.
     * @param order      byte order of created buffer.
     * @param generateNodes generate and store an EvioNode object
     *                      for each evio structure created.
     *
     * @throws EvioException if bufferSize arg too small
     */
    CompactEventBuilder::CompactEventBuilder(size_t bufferSize, ByteOrder const & order, bool generateNodes) :
            order(order), generateNodes(generateNodes) {

        if (bufferSize < 8) {
            throw EvioException("bufferSize arg too small");
        }

        // Create buffer
        buffer = std::make_shared<ByteBuffer>(bufferSize);
        array = buffer->array();
        buffer->order(order);

        // Init variables
        createdBuffer = true;

        totalLengths.assign(MAX_LEVELS, 0);
        stackArray.reserve(MAX_LEVELS);

        // Fill stackArray vector with pre-allocated objects so each openBank/Seg/TagSeg
        // doesn't have to create a new StructureContent object each time they're called.
        for (int i=0; i < MAX_LEVELS; i++) {
            stackArray[i] = std::make_shared<StructureContent>();
        }
    }


    /**
     * This is the constructor to use for building a new event
     * (just the event in a buffer, not the full evio file format)
     * with a user-given buffer.
     *
     * @param buffer the byte buffer to write into.
     * @param generateNodes generate and store an EvioNode object
     *                      for each evio structure created.
     *
     * @throws EvioException if arg is null;
     *                       if buffer is too small
     */
    CompactEventBuilder::CompactEventBuilder(std::shared_ptr<ByteBuffer> buffer, bool generateNodes) {

        if (buffer == nullptr) {
            throw EvioException("null arg(s)");
        }

        totalLengths.assign(MAX_LEVELS, 0);
        stackArray.reserve(MAX_LEVELS);

        for (int i=0; i < MAX_LEVELS; i++) {
            stackArray[i] = std::make_shared<StructureContent>();
        }

        initBuffer(buffer, generateNodes);
    }


    /**
     * Set the buffer to be written into.
     * @param buffer buffer to be written into.
     * @param generateNodes generate and store an EvioNode object
     *                      for each evio structure created.
     * @throws EvioException if arg is null;
     *                       if buffer is too small
     */
    void CompactEventBuilder::setBuffer(std::shared_ptr<ByteBuffer> buffer, bool generateNodes) {
        if (buffer == nullptr) {
            throw EvioException("null buffer arg");
        }
        initBuffer(buffer, generateNodes);
    }


    /**
     * Set the buffer to be written into.
     *
     * @param buffer buffer to be written into.
     * @param generateNodes generate and store an EvioNode object
     *                      for each evio structure created.
     * @throws EvioException if arg is null;
     *                       if buffer is too small
     */
    void CompactEventBuilder::initBuffer(std::shared_ptr<ByteBuffer> buffer, bool generateNodes) {

        this->buffer = buffer;
        this->generateNodes = generateNodes;

        // Prepare buffer
        buffer->clear();
        totalLengths.assign(MAX_LEVELS, 0);
        order = buffer->order();

        if (buffer->limit() < 8) {
            throw EvioException("buffer too small");
        }

        array = buffer->array();
        arrayOffset = buffer->arrayOffset();

        // Init variables
        nodes.clear();
        currentLevel = -1;
        createdBuffer = false;
        currentStructure = nullptr;
    }


    /**
     * Get the buffer being written into.
     * The buffer is ready to read.
     * @return buffer being written into.
     */
    std::shared_ptr<ByteBuffer> CompactEventBuilder::getBuffer() {
        buffer->limit(position).position(0);
        return buffer;
    }


    /**
     * Get the total number of bytes written into the buffer.
     * @return total number of bytes written into the buffer.
     */
    size_t CompactEventBuilder::getTotalBytes() const {return position;}


    /**
     * This method adds an evio segment structure to the buffer.
     *
     * @param tag tag of segment header
     * @param dataType type of data to be contained by this segment
     * @return if told to generate nodes in constructor, then return
     *         the node created here; else return null
     * @throws EvioException if containing structure does not hold segments;
     *                       if no room in buffer for segment header;
     *                       if too many nested evio structures;
     *                       if top-level bank has not been added first.
     */
    std::shared_ptr<EvioNode> CompactEventBuilder::openSegment(uint16_t tag, DataType const & dataType) {

        if (currentStructure == nullptr) {
            throw EvioException("add a bank (event) first");
        }

        // Segments not allowed if parent holds different type
        if (currentStructure->dataType != DataType::SEGMENT &&
            currentStructure->dataType != DataType::ALSOSEGMENT) {
            throw EvioException("may NOT add segment type, expecting " +
                                (currentStructure->dataType).toString());
        }

        // Make sure we can use all of the buffer in case external changes
        // were made to it (e.g. by doing buffer->flip() in order to read).
        // All this does is set pos = 0, limit = capacity, it does NOT
        // clear the data. We're keep track of the position to write at
        // in our own variable, "position".
        buffer->clear();

        if ((buffer->limit() - position) < 4) {
            throw EvioException("no room in buffer");
        }

        // For now, assume length and padding = 0
        if (order == ByteOrder::ENDIAN_BIG) {
            array[arrayOffset + position]   = (uint8_t)tag;
            array[arrayOffset + position+1] = (uint8_t)(dataType.getValue() & 0x3f);
        }
        else {
            array[arrayOffset + position+2] = (uint8_t)(dataType.getValue() & 0x3f);
            array[arrayOffset + position+3] = (uint8_t)tag;
        }

        // currentLevel starts at -1
        if (++currentLevel >= MAX_LEVELS) {
            throw EvioException("too many nested evio structures, increase MAX_LEVELS from " +
                                std::to_string(MAX_LEVELS));
        }

        // currentStructure is the bank we are creating right now
        currentStructure = stackArray[currentLevel];
        currentStructure->setData(position, DataType::SEGMENT, dataType);

        // We just wrote a bank header so this and all
        // containing levels increase in length by 1.
        addToAllLengths(1);

        // Do we generate an EvioNode object corresponding to this segment?
        std::shared_ptr<EvioNode> node = nullptr;
        if (generateNodes) {
            // This constructor does not have lengths or create allNodes list, blockNode
            node = std::make_shared<EvioNode>(tag, 0, position, position+4,
                                              DataType::SEGMENT, dataType,
                                              buffer);
            nodes.push_back(node);
        }

        position += 4;

        return node;
    }


    /**
     * This method adds an evio tagsegment structure to the buffer.
     *
     * @param tag tag of tagsegment header
     * @param dataType type of data to be contained by this tagsegment
     * @return if told to generate nodes in constructor, then return
     *         the node created here; else return null
     * @throws EvioException if containing structure does not hold tagsegments;
     *                       if no room in buffer for tagsegment header;
     *                       if too many nested evio structures;
     *                       if top-level bank has not been added first.
     */
    std::shared_ptr<EvioNode> CompactEventBuilder::openTagSegment(int tag, DataType dataType) {

        if (currentStructure == nullptr) {
            throw EvioException("add a bank (event) first");
        }

        // Tagsegments not allowed if parent holds different type
        if (currentStructure->dataType != DataType::TAGSEGMENT) {
            throw EvioException("may NOT add tagsegment type, expecting " +
                                currentStructure->dataType.toString());
        }

        // Make sure we can use all of the buffer in case external changes
        // were made to it (e.g. by doing buffer->flip() in order to read).
        // All this does is set pos = 0, limit = capacity, it does NOT
        // clear the data. We're keep track of the position to write at
        // in our own variable, "position".
        buffer->clear();

        if (buffer->limit() - position < 4) {
            throw EvioException("no room in buffer");
        }

        // Because Java automatically sets all members of a new
        // byte array to zero, we don't have to specifically write
        // a length of zero into the new tagsegment header (which is
        // what the length is if no data written).

        // For now, assume length = 0
        uint16_t compositeWord = (uint16_t) ((tag << 4) | (dataType.getValue() & 0xf));

        if (order == ByteOrder::ENDIAN_BIG) {
            array[arrayOffset + position]   = (uint8_t) (compositeWord >> 8);
            array[arrayOffset + position+1] = (uint8_t)  compositeWord;
        }
        else {
            array[arrayOffset + position+2] = (uint8_t)  compositeWord;
            array[arrayOffset + position+3] = (uint8_t) (compositeWord >> 8);
        }

        // currentLevel starts at -1
        if (++currentLevel >= MAX_LEVELS) {
            throw EvioException("too many nested evio structures, increase MAX_LEVELS from " +
                                std::to_string(MAX_LEVELS));
        }

        // currentStructure is the bank we are creating right now
        currentStructure = stackArray[currentLevel];
        currentStructure->setData(position, DataType::TAGSEGMENT, dataType);

        // We just wrote a bank header so this and all
        // containing levels increase in length by 1.
        addToAllLengths(1);

        // Do we generate an EvioNode object corresponding to this segment?
        std::shared_ptr<EvioNode> node = nullptr;
        if (generateNodes) {
            // This constructor does not have lengths or create allNodes list, blockNode
            node = std::make_shared<EvioNode>(tag, 0, position, position+4,
                                              DataType::TAGSEGMENT, dataType,
                                              buffer);
            nodes.push_back(node);
        }

        position += 4;

        return node;
    }


    /**
     * This method adds an evio bank to the buffer.
     *
     * @param tag tag of bank header
     * @param num num of bank header
     * @param dataType data type to be contained in this bank
     * @return if told to generate nodes in constructor, then return
     *         the node created here; else return null
     * @throws EvioException if containing structure does not hold banks;
     *                       if no room in buffer for bank header;
     *                       if too many nested evio structures;
     */
    std::shared_ptr<EvioNode> CompactEventBuilder::openBank(uint16_t tag, uint8_t num, DataType const & dataType) {

        // Banks not allowed if parent holds different type
        if (currentStructure != nullptr &&
            (currentStructure->dataType != DataType::BANK &&
             currentStructure->dataType != DataType::ALSOBANK)) {
            throw EvioException("may NOT add bank type, expecting " +
                                currentStructure->dataType.toString());
        }

        // Make sure we can use all of the buffer in case external changes
        // were made to it (e.g. by doing buffer->flip() in order to read).
        // All this does is set pos = 0, limit = capacity, it does NOT
        // clear the data. We're keeping track of the position to write at
        // in our own variable, "position".
        buffer->clear();

        if (buffer->limit() - position < 8) {
            throw EvioException("no room in buffer");
        }

        // Write bank header into buffer, assuming padding = 0.
        // Bank w/ no data has len = 1

        if (order == ByteOrder::ENDIAN_BIG) {
            // length word
            array[arrayOffset + position]   = (uint8_t)0;
            array[arrayOffset + position+1] = (uint8_t)0;
            array[arrayOffset + position+2] = (uint8_t)0;
            array[arrayOffset + position+3] = (uint8_t)1;

            array[arrayOffset + position+4] = (uint8_t)(tag >> 8);
            array[arrayOffset + position+5] = (uint8_t)tag;
            array[arrayOffset + position+6] = (uint8_t)(dataType.getValue() & 0x3f);
            array[arrayOffset + position+7] = num;
        }
        else {
            array[arrayOffset + position]   = (uint8_t)1;
            array[arrayOffset + position+1] = (uint8_t)0;
            array[arrayOffset + position+2] = (uint8_t)0;
            array[arrayOffset + position+3] = (uint8_t)0;

            array[arrayOffset + position+4] = num;
            array[arrayOffset + position+5] = (uint8_t)(dataType.getValue() & 0x3f);
            array[arrayOffset + position+6] = (uint8_t)tag;
            array[arrayOffset + position+7] = (uint8_t)(tag >> 8);
        }

        // currentLevel starts at -1
        if (++currentLevel >= MAX_LEVELS) {
            throw EvioException("too many nested evio structures, increase MAX_LEVELS from " +
                                std::to_string(MAX_LEVELS));
        }

        // currentStructure is the bank we are creating right now
        currentStructure = stackArray[currentLevel];
        currentStructure->setData(position, DataType::BANK, dataType);

        // We just wrote a bank header so this and all
        // containing levels increase in length by 2.
        addToAllLengths(2);

        // Do we generate an EvioNode object corresponding to this segment?
        std::shared_ptr<EvioNode> node = nullptr;
        if (generateNodes) {
            // This constructor does not have lengths or create allNodes list, blockNode
            node = std::make_shared<EvioNode>(tag, 0, position, position+8,
                                              DataType::BANK, dataType,
                                              buffer);
            nodes.push_back(node);
        }

        position += 8;

        return node;
    }


    /**
     * This method ends the writing of the current evio structure and
     * makes sure the length and padding fields are properly set.
     * Its parent, if any, then becomes the current structure.
     *
     * @return {@code true} if top structure was reached (is current), else {@code false}.
     */
    bool CompactEventBuilder::closeStructure() {
        // Cannot go up any further
        if (currentLevel < 0) {
            return true;
        }

        //std::cout << "closeStructure: set currentLevel(" << currentLevel <<
        //             ") len to " << (totalLengths[currentLevel] - 1) << std::endl;

        // Set the length of current structure since we're done adding to it.
        // The length contained in an evio header does NOT include itself,
        // thus the -1.
        setCurrentHeaderLength(totalLengths[currentLevel] - 1);

        // Set padding if necessary
        if (currentStructure->padding > 0) {
            setCurrentHeaderPadding(currentStructure->padding);
        }

        // Clear out old data
        totalLengths[currentLevel] = 0;

        // Go up a level
        if (--currentLevel > -1) {
            currentStructure = stackArray[currentLevel];
        }

        // Have we reached the top ?
        return (currentLevel < 0);
    }


    /**
     * This method finishes the event writing by setting all the
     * proper lengths &amp; padding and ends up at the event or top level.
     */
    void CompactEventBuilder::closeAll() {
        while (!closeStructure()) {}
    }


    /**
     * In the emu software package, it's necessary to change the top level
     * tag after the top level bank has already been created. This method
     * allows changing it after initially written.
     * @param tag  new tag value of top-level, event bank.
     */
    void CompactEventBuilder::setTopLevelTag(short tag) {
        if (order == ByteOrder::ENDIAN_BIG) {
            array[arrayOffset + 4] = (uint8_t)(tag >> 8);
            array[arrayOffset + 5] = (uint8_t)tag;
        }
        else {
            array[arrayOffset + 6] = (uint8_t)tag;
            array[arrayOffset + 7] = (uint8_t)(tag >> 8);
        }
    }


    /**
     * This method adds the given length to the current structure's existing
     * length and all its ancestors' lengths. Nothing is written into the
     * buffer. The lengths are keep in an array for later use in writing to
     * the buffer.
     *
     * @param len length to add
     */
    void CompactEventBuilder::addToAllLengths(uint32_t len) {
        // Go up the chain of structures from current structure, inclusive
        for (int i=currentLevel; i >= 0; i--) {
            totalLengths[i] += len;
            //std::cout << "Set len at level " << i << " to " << totalLengths[i] << std::endl;
        }
        //std::cout << std::endl;
    }


    /**
     * This method sets the length of the current evio structure in the buffer.
     * @param len length of current structure
     */
    void CompactEventBuilder::setCurrentHeaderLength(uint32_t len) {

        DataType const & type = currentStructure->type;
        bool isBigEndian = buffer->order().isBigEndian();

        if (type == DataType::BANK || type == DataType::ALSOBANK) {
            try {
                Util::toBytes(len, order, array + arrayOffset + currentStructure->pos);
            }
            catch (EvioException e) {/* never happen*/}
        }
        else if (type == DataType::SEGMENT || type == DataType::ALSOSEGMENT || type == DataType::TAGSEGMENT) {
            try {
                if (isBigEndian) {
                    Util::toBytes((uint16_t)len, order, array + arrayOffset + currentStructure->pos + 2);
                }
                else {
                    Util::toBytes((uint16_t)len, order, array + arrayOffset + currentStructure->pos);
                }
            }
            catch (EvioException & e) {/* never happen*/}
        }
    }


    /**
     * This method sets the padding of the current evio structure in the buffer->
     * @param padding padding of current structure's data
     */
    void CompactEventBuilder::setCurrentHeaderPadding(uint32_t padding) {

        // byte containing padding
        uint8_t b = (uint8_t)((currentStructure->dataType.getValue() & 0x3f) | (padding << 6));

        DataType const & type = currentStructure->type;
        bool isBigEndian = buffer->order().isBigEndian();

        if (type == DataType::BANK || type == DataType::ALSOBANK) {
            if (isBigEndian) {
                array[arrayOffset + currentStructure->pos + 6] = b;
            }
            else {
                array[arrayOffset + currentStructure->pos + 5] = b;
            }
        }
        else if (type == DataType::SEGMENT || type == DataType::ALSOSEGMENT) {
            if (isBigEndian) {
                array[arrayOffset + currentStructure->pos + 1] = b;
            }
            else {
                array[arrayOffset + currentStructure->pos + 2] = b;
            }
        }
    }


    /**
     * This method takes the node (which has been scanned and that
     * info stored in that object) and rewrites it into the buffer.
     * This is equivalent to a swap if the node is opposite endian in
     * its original buffer. If the node is not opposite endian,
     * there is no point in calling this method.
     *
     * @param node node whose header data must be written
     */
    void CompactEventBuilder::writeHeader(std::shared_ptr<EvioNode> & node) {

        DataType const & type = currentStructure->type;
        bool isBigEndian = order.isBigEndian();
        uint32_t len = node->getLength();
        uint16_t tag = node->getTag();
        uint8_t  num = node->getNum();
        uint32_t pad = node->getPad();
        uint32_t typ = node->getDataType();

        if (type == DataType::BANK || type == DataType::ALSOBANK) {
            if (isBigEndian) {
                array[arrayOffset + position]     = (uint8_t)(len >> 24);
                array[arrayOffset + position + 1] = (uint8_t)(len >> 16);
                array[arrayOffset + position + 2] = (uint8_t)(len >> 8);
                array[arrayOffset + position + 3] = (uint8_t) len;

                array[arrayOffset + position + 4] = (uint8_t)(tag >> 8);
                array[arrayOffset + position + 5] = (uint8_t) tag;
                array[arrayOffset + position + 6] = (uint8_t)((typ & 0x3f) | (pad << 6));
                array[arrayOffset + position + 7] = num;
            }
            else {
                array[arrayOffset + position]     = (uint8_t)(len);
                array[arrayOffset + position + 1] = (uint8_t)(len >> 8);
                array[arrayOffset + position + 2] = (uint8_t)(len >> 16);
                array[arrayOffset + position + 3] = (uint8_t)(len >> 24);

                array[arrayOffset + position + 4] = num;
                array[arrayOffset + position + 5] = (uint8_t)((typ & 0x3f) | (pad << 6));
                array[arrayOffset + position + 6] = (uint8_t) tag;
                array[arrayOffset + position + 7] = (uint8_t)(tag >> 8);
            }

            position += 8;
        }
        else if (type == DataType::SEGMENT || type == DataType::ALSOSEGMENT) {
            if (isBigEndian) {
                array[arrayOffset + position]     = (uint8_t) tag;
                array[arrayOffset + position + 1] = (uint8_t)((typ & 0x3f) | (pad << 6));
                array[arrayOffset + position + 2] = (uint8_t)(len >> 8);
                array[arrayOffset + position + 3] = (uint8_t) len;
            }
            else {
                array[arrayOffset + position]     = (uint8_t) len;
                array[arrayOffset + position + 1] = (uint8_t)(len >> 8);
                array[arrayOffset + position + 2] = (uint8_t)((typ & 0x3f) | (pad << 6));
                array[arrayOffset + position + 3] = (uint8_t) tag;
            }

            position += 4;
        }
        else if (type == DataType::TAGSEGMENT) {

            uint16_t compositeWord = (uint16_t) ((tag << 4) | (typ & 0xf));

            if (isBigEndian) {
                array[arrayOffset + position]     = (uint8_t)(compositeWord >> 8);
                array[arrayOffset + position + 1] = (uint8_t) compositeWord;
                array[arrayOffset + position + 2] = (uint8_t)(len >> 8);
                array[arrayOffset + position + 3] = (uint8_t) len;
            }
            else {
                array[arrayOffset + position]     = (uint8_t) len;
                array[arrayOffset + position + 1] = (uint8_t)(len >> 8);
                array[arrayOffset + position + 2] = (uint8_t) compositeWord;
                array[arrayOffset + position + 3] = (uint8_t)(compositeWord >> 8);
            }

            position += 4;
        }
    }


    /**
     * Take the node object and write its data into buffer in the
     * proper endian while also swapping primitive data if desired.
     *
     * @param node node to be written (is never null)
     * @param swapData do we swap the primitive data or not?
     * @throws EvioException if data needs to, but cannot be swapped.
     */
    void CompactEventBuilder::writeNode(std::shared_ptr<EvioNode> & node, bool swapData) {

        // Write header in endianness of buffer
        writeHeader(node);

        // Does this node contain other containers?
        if (node->getDataTypeObj().isStructure()) {
            // Iterate through list of children
            auto kids = node->getChildNodes();
            if (kids.empty()) return;
            for (auto child : kids) {
                writeNode(child, swapData);
            }
        }
        else {
            auto nodeBuf = node->getBuffer();

            if (swapData) {
                EvioSwap::swapLeafData(node->getDataTypeObj(), nodeBuf,
                                       buffer, node->getDataPosition(), position,
                                       node->getDataLength(), false);
            }
            else {
                std::memcpy(array + arrayOffset + position,
                            nodeBuf->array() + nodeBuf->arrayOffset() + node->getDataPosition(),
                            4*node->getDataLength());
            }

            position += 4*node->getDataLength();
        }
    }


    /**
     * Adds the evio structure represented by the EvioNode object
     * into the buffer.
     *
     * @param node the EvioNode object representing the evio structure to data.
     * @throws EvioException if data is null or empty;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    void CompactEventBuilder::addEvioNode(std::shared_ptr<EvioNode> node) {
        if (node == nullptr) {
            throw EvioException("no data to add");
        }

        if (currentStructure == nullptr) {
            throw EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure->dataType != node->getTypeObj()) {
            throw EvioException("may only add " + currentStructure->dataType.toString() + " data");
        }

        // Sets pos = 0, limit = capacity, & does NOT clear data
        buffer->clear();

        uint32_t len = node->getTotalBytes();
        //std::cout << "addEvioNode: buf lim = " << buffer->limit() <<
        //                           " - pos = " << position << " = (" << (buffer->limit() - position) <<
        //                           ") must be >= " << node->getTotalBytes() << " node total bytes");
        if ((buffer->limit() - position) < len) {
            throw EvioException("no room in buffer");
        }

        addToAllLengths(len/4);  // # 32-bit words

        auto nodeBuf = node->getBuffer();

        if (nodeBuf->order() == buffer->order()) {
            //std::cout << "addEvioNode: arraycopy node (same endian)" << std::endl;
            std::memcpy(array + arrayOffset + position,
                        nodeBuf->array() + nodeBuf->arrayOffset() + node->getPosition(),
                        node->getTotalBytes());

            position += len;
        }
        else {
            // If node is opposite endian as this buffer,
            // rewrite all evio header data, but leave
            // primitive data alone.
            writeNode(node, false);
        }
    }


    /**
     * Appends byte data to the structure.
     *
     * @param data the byte data to append.
     * @param len number of bytes to append.
     * @throws EvioException if data is null or empty;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    void CompactEventBuilder::addByteData(uint8_t * data, uint32_t len) {
        if (data == nullptr || len < 1) {
            throw EvioException("no data to add");
        }

        if (currentStructure == nullptr) {
            throw EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure->dataType != DataType::CHAR8  &&
            currentStructure->dataType != DataType::UCHAR8)  {
            throw EvioException("may NOT add " + currentStructure->dataType.toString() + " data");
        }

        // Sets pos = 0, limit = capacity, & does NOT clear data
        buffer->clear();

        if ((buffer->limit() - position) < len) {
            throw EvioException("no room in buffer");
        }

        // Things get tricky if this method is called multiple times in succession.
        // Keep track of how much data we write each time so length and padding
        // are accurate.

        // Last increase in length
        uint32_t lastWordLen = (currentStructure->dataLen + 3)/4;

        // If we are adding to existing data,
        // place position before previous padding.
        if (currentStructure->dataLen > 0) {
            position -= currentStructure->padding;
        }

        // Total number of bytes to write & already written
        currentStructure->dataLen += len;

        // New total word len of this data
        uint32_t totalWordLen = (currentStructure->dataLen + 3)/4;

        // Increase lengths by the difference
        addToAllLengths(totalWordLen - lastWordLen);

        // Copy the data in one chunk
        std::memcpy(array + arrayOffset + position, data, len);

        // Calculate the padding
        currentStructure->padding = padCount[currentStructure->dataLen % 4];

        // Advance buffer position
        position += len + currentStructure->padding;
    }


    /**
     * Appends int data to the structure.
     *
     * @param data the int data to append.
     * @param len  the number of ints from data to append.
     * @throws EvioException if data is null or empty;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    void CompactEventBuilder::addIntData(uint32_t * data, uint32_t len) {

        if (data == nullptr) {
            throw EvioException("data arg null");
        }

        if (len == 0) return;

        if (currentStructure == nullptr) {
            throw EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure->dataType != DataType::INT32  &&
            currentStructure->dataType != DataType::UINT32 &&
            currentStructure->dataType != DataType::UNKNOWN32)  {
            throw EvioException("may NOT add " + currentStructure->dataType.toString() + " data");
        }

        // Sets pos = 0, limit = capacity, & does NOT clear data
        buffer->clear();

        if ((buffer->limit() - position) < 4*len) {
            throw EvioException("no room in buffer");
        }

        addToAllLengths(len);  // # 32-bit words

        if (order.isLocalEndian()) {
            std::memcpy(array + arrayOffset + position, data, 4*len);
        }
        else {
            size_t pos = position;
            for (int i=0; i < len; i++) {
                uint8_t* bytes = reinterpret_cast<uint8_t *>(data);
                array[arrayOffset + pos++] = bytes[3];
                array[arrayOffset + pos++] = bytes[2];
                array[arrayOffset + pos++] = bytes[1];
                array[arrayOffset + pos++] = bytes[0];
                data++;
            }
        }
        position += 4*len;     // # bytes
    }


    /**
     * Appends short data to the structure.
     *
     * @param data    the short data to append.
     * @param len     the number of shorts from data to append.
     * @throws EvioException if data is null;
     *                       if count/offset negative or too large;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    void CompactEventBuilder::addShortData(uint16_t * data, uint32_t len) {

        if (data == nullptr) {
            throw EvioException("data arg null");
        }

        if (len == 0) return;

        if (currentStructure == nullptr) {
            throw EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure->dataType != DataType::SHORT16  &&
            currentStructure->dataType != DataType::USHORT16)  {
            throw new EvioException("may NOT add " + currentStructure->dataType.toString() + " data");
        }

        // Sets pos = 0, limit = capacity, & does NOT clear data
        buffer->clear();

        if ((buffer->limit() - position) < 2*len) {
            throw EvioException("no room in buffer");
        }

        // Things get tricky if this method is called multiple times in succession.
        // Keep track of how much data we write each time so length and padding
        // are accurate.

        // Last increase in length
        uint32_t lastWordLen = (currentStructure->dataLen + 1)/2;
        //std::cout << "lastWordLen = " << lastWordLen << std::endl;

        // If we are adding to existing data,
        // place position before previous padding.
        if (currentStructure->dataLen > 0) {
            position -= currentStructure->padding;
            //std::cout << "Back up before previous padding of " << currentStructure->padding << std::endl;
        }

        // Total number of bytes to write & already written
        currentStructure->dataLen += len;
        //std::cout << "Total bytes to write = " << currentStructure->dataLen << std::endl

        // New total word len of this data
        uint32_t totalWordLen = (currentStructure->dataLen + 1)/2;
        //std::cout << "Total word len = " << totalWordLen << std::endl;

        // Increase lengths by the difference
        addToAllLengths(totalWordLen - lastWordLen);

        if (order.isLocalEndian()) {
            std::memcpy(array + arrayOffset + position, data, 2*len);
        }
        else {
            size_t pos = position;
            for (int i=0; i < len; i++) {
                uint8_t* bytes = reinterpret_cast<uint8_t *>(data);
                array[arrayOffset + pos++] = bytes[1];
                array[arrayOffset + pos++] = bytes[0];
                data++;
            }
        }

        currentStructure->padding = 2*(currentStructure->dataLen % 2);
        // Advance position
        position += 2*len + currentStructure->padding;
    }


    /**
     * Appends long data to the structure.
     *
     * @param data    the long data to append.
     * @param len     the number of longs from data to append.
     * @throws EvioException if data is null;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    void CompactEventBuilder::addLongData(uint64_t * data, uint32_t len) {

        if (data == nullptr) {
            throw new EvioException("data arg null");
        }

        if (len == 0) return;

        if (currentStructure == nullptr) {
            throw EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure->dataType != DataType::LONG64  &&
            currentStructure->dataType != DataType::ULONG64)  {
            throw EvioException("may NOT add " + currentStructure->dataType.toString() + " data");
        }

        // Sets pos = 0, limit = capacity, & does NOT clear data
        buffer->clear();

        if ((buffer->limit() - position) < 8*len) {
            throw EvioException("no room in buffer");
        }

        addToAllLengths(2*len);  // # 32-bit words

        if (order.isLocalEndian()) {
            std::memcpy(array + arrayOffset + position, data, 8*len);
        }
        else {
            size_t pos = position;
            for (int i = 0; i < len; i++) {
                uint8_t *bytes = reinterpret_cast<uint8_t *>(data);
                array[arrayOffset + pos++] = bytes[7];
                array[arrayOffset + pos++] = bytes[6];
                array[arrayOffset + pos++] = bytes[5];
                array[arrayOffset + pos++] = bytes[4];
                array[arrayOffset + pos++] = bytes[3];
                array[arrayOffset + pos++] = bytes[2];
                array[arrayOffset + pos++] = bytes[1];
                array[arrayOffset + pos++] = bytes[0];
                data++;
            }
        }

        position += 8*len;       // # bytes
    }


    /**
     * Appends float data to the structure.
     *
     * @param data the float data to append.
     * @param len number of the floats to append.
     * @throws EvioException if data is null or empty;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    void CompactEventBuilder::addFloatData(float * data, uint32_t len) {
        if (data == nullptr || len < 1) {
            throw EvioException("no data to add");
        }

        if (currentStructure == nullptr) {
            throw EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure->dataType != DataType::FLOAT32) {
            throw EvioException("may NOT add " + currentStructure->dataType.toString() + " data");
        }

        // Sets pos = 0, limit = capacity, & does NOT clear data
        buffer->clear();

        if ((buffer->limit() - position) < 4*len) {
            throw EvioException("no room in buffer");
        }

        addToAllLengths(len);  // # 32-bit words

        if (order.isLocalEndian()) {
            std::memcpy(array + arrayOffset + position, data, 4*len);
        }
        else {
            size_t pos = position;
            for (int i=0; i < len; i++) {
                uint8_t* bytes = reinterpret_cast<uint8_t *>(data);
                array[arrayOffset + pos++] = bytes[3];
                array[arrayOffset + pos++] = bytes[2];
                array[arrayOffset + pos++] = bytes[1];
                array[arrayOffset + pos++] = bytes[0];
                data++;
            }
        }

        position += 4*len;     // # bytes
    }


    /**
     * Appends double data to the structure.
     *
     * @param data the double data to append.
     * @param len  the number of doubles from data to append.
     * @throws EvioException if data is null or empty;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    void CompactEventBuilder::addDoubleData(double * data, uint32_t len) {
        if (data == nullptr || len < 1) {
            throw EvioException("no data to add");
        }

        if (currentStructure == nullptr) {
            throw EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure->dataType != DataType::DOUBLE64) {
            throw EvioException("may NOT add " + currentStructure->dataType.toString() + " data");
        }

        // Sets pos = 0, limit = capacity, & does NOT clear data
        buffer->clear();

        addToAllLengths(2*len);  // # 32-bit words

        if (order.isLocalEndian()) {
            std::memcpy(array + arrayOffset + position, data, 8*len);
        }
        else {
            size_t pos = position;
            for (int i=0; i < len; i++) {
                uint8_t* bytes = reinterpret_cast<uint8_t *>(data);
                array[arrayOffset + pos++] = bytes[7];
                array[arrayOffset + pos++] = bytes[6];
                array[arrayOffset + pos++] = bytes[5];
                array[arrayOffset + pos++] = bytes[4];
                array[arrayOffset + pos++] = bytes[3];
                array[arrayOffset + pos++] = bytes[2];
                array[arrayOffset + pos++] = bytes[1];
                array[arrayOffset + pos++] = bytes[0];
                data++;
            }
        }

        position += 8*len;     // # bytes
    }


    /**
     * Appends string data to the structure.
     *
     * @param strings vector of strings to append.
     * @throws EvioException if strings is empty;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for strings.
     */
    void CompactEventBuilder::addStringData(std::vector<std::string> & strings) {
        if (strings.empty()) {
            throw EvioException("no data to add");
        }

        if (currentStructure == nullptr) {
            throw EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure->dataType != DataType::CHARSTAR8) {
            throw EvioException("may NOT add " + currentStructure->dataType.toString() + " data");
        }

        // Convert strings into byte array (already padded)
        std::vector<uint8_t> data;
        Util::stringsToRawBytes(strings, data);
        size_t len = data.size();

        // Sets pos = 0, limit = capacity, & does NOT clear data
        buffer->clear();

        // Things get tricky if this method is called multiple times in succession.
        // In fact, it's too difficult to bother with so just throw an exception.
        if (currentStructure->dataLen > 0) {
            throw EvioException("addStringData() may only be called once per structure");
        }

        std::memcpy(array + arrayOffset + position, data.data(), len);
        currentStructure->dataLen += len;
        addToAllLengths(len/4);

        position += len;
    }


    /**
     * Appends CompositeData objects to the structure.
     *
     * @param data the CompositeData objects to append, or set if there is no existing data.
     * @throws EvioException if data is empty;
     *                       if adding wrong data type to structure;
     *                       if structure not added first;
     *                       if no room in buffer for data.
     */
    void CompactEventBuilder::addCompositeData(std::vector<std::shared_ptr<CompositeData>> & data) {

        if (data.empty()) {
            throw EvioException("no data to add");
        }

        if (currentStructure == nullptr) {
            throw EvioException("add a bank, segment, or tagsegment first");
        }

        if (currentStructure->dataType != DataType::COMPOSITE) {
            throw EvioException("may only add " + currentStructure->dataType.toString() + " data");
        }

        // Composite data is always in the local (in this case, BIG) endian
        // because if generated in JVM that's true, and if read in, it is
        // swapped to local if necessary. Either case it's big endian.

        // Convert composite data into byte array (already padded)
        std::vector<uint8_t> rawBytes;
        CompositeData::generateRawBytes(data, rawBytes, order);

        // Sets pos = 0, limit = capacity, & does NOT clear data
        buffer->clear();

        size_t len = rawBytes.size();
        if (buffer->limit() - position < len) {
            throw EvioException("no room in buffer");
        }

        // This method cannot be called multiple times in succession.
        if (currentStructure->dataLen > 0) {
            throw EvioException("addCompositeData() may only be called once per structure");
        }

        std::memcpy(array + arrayOffset + position, rawBytes.data(), len);
        currentStructure->dataLen += len;
        addToAllLengths(len/4);

        position += len;
    }


    /**
     * This method writes a file in proper evio format with block header
     * containing the single event constructed by this object. This is
     * useful as a test to see if event is being properly constructed.
     * Use this in conjunction with the event viewer in order to check
     * that format is correct. This only works with array backed buffers.
     *
     * @param filename name of file
     */
    void CompactEventBuilder::toFile(std::string const & fileName) {

        if (fileName.empty()) {
            throw EvioException("empty fileName arg");
        }

        std::ofstream ostrm(fileName, std::ios::binary);

        ////////////////////////////////////
        // First write the evio file header
        ////////////////////////////////////
        ByteBuffer fileHeader(FileHeader::HEADER_SIZE_BYTES);
        fileHeader.order(order);
        // evio id
        fileHeader.putInt(FileHeader::EVIO_FILE_UNIQUE_WORD);
        // split #
        fileHeader.putInt(1);
        // header len
        fileHeader.putInt(FileHeader::HEADER_SIZE_BYTES);
        // record count
        fileHeader.putInt(1);
        // index array len
        fileHeader.putInt(0);
        // bit info word
        uint32_t bi = FileHeader::generateBitInfoWord(6, false, false, false, 1);
        fileHeader.putInt(bi);
        // user header len
        fileHeader.putInt(0);
        // magic #
        fileHeader.putInt(FileHeader::HEADER_MAGIC);
        // user register
        fileHeader.putLong(0);
        // trailer pos
        fileHeader.putLong(0);
        // user int 1
        fileHeader.putInt(0);
        // user int 2
        fileHeader.putInt(0);

        // Write it
        fileHeader.flip();
        ostrm.write(reinterpret_cast<char*>(fileHeader.array() + fileHeader.arrayOffset()),
                    fileHeader.remaining());


        ///////////////////////////////////////
        // Second write the evio record header
        ///////////////////////////////////////
        ByteBuffer recHeader(RecordHeader::HEADER_SIZE_BYTES);
        recHeader.order(order);
        // rec len
        recHeader.putInt(position + RecordHeader::HEADER_SIZE_BYTES + FileHeader::HEADER_SIZE_BYTES);
        // rec #
        recHeader.putInt(1);
        // header len
        recHeader.putInt(RecordHeader::HEADER_SIZE_BYTES);
        // event count
        recHeader.putInt(1);
        // index array len
        recHeader.putInt(0);
        // bit info word
        bi = RecordHeader::generateSixthWord(6, false, true, 4);
        recHeader.putInt(bi);
        // user header len
        recHeader.putInt(0);
        // magic #
        recHeader.putInt(RecordHeader::HEADER_MAGIC);
        // uncompressed data len
        recHeader.putInt(position);
        // compressed len & type
        recHeader.putInt(0);
        // user register 1
        recHeader.putLong(0);
        // user register 2
        recHeader.putLong(0);

        // Write it
        recHeader.flip();
        ostrm.write(reinterpret_cast<char*>(recHeader.array() + recHeader.arrayOffset()),
                    recHeader.remaining());


        ///////////////////////////////////////
        // Third write the constructed event
        ///////////////////////////////////////

        // Get buffer ready to read
        buffer->limit(position);
        buffer->position(0);

        ostrm.write(reinterpret_cast<char*>(buffer->array() + buffer->arrayOffset() + buffer->position()),
                    buffer->remaining());
        ostrm.close();

        // Reset position & limit
        buffer->clear();
    }


}