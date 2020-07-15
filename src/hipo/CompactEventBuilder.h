//
// Created by Carl Timmer on 7/15/20.
//

#ifndef EVIO_6_0_COMPACTEVENTBUILDER_H
#define EVIO_6_0_COMPACTEVENTBUILDER_H

#include <vector>
#include <memory>

#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "DataType.h"
#include "EvioNode.h"


namespace evio {




    /**
     * This class is used for creating events
     * and their substructures while minimizing use of Java objects.
     * Evio format data of a single event is written directly,
     * and sequentially, into a buffer. The buffer contains
     * only the single event, not the full, evio file format.<p>
     *
     * The methods of this class are not synchronized so it is NOT
     * threadsafe. This is done for speed. The buffer retrieved by
     * {@link #getBuffer()} is ready to read.
     *
     * @author timmer
     * (Feb 6 2014)
     */
    class CompactEventBuilder {

    private:

        /** Buffer in which to write. */
        std::shared_ptr<ByteBuffer> buffer = nullptr;

        /** Byte array which backs the buffer. */
        uint8_t * array = nullptr;

        /** Current writing position in the buffer. */
        size_t position = 0;

        /** Byte order of the buffer, convenience variable. */
        ByteOrder order {ByteOrder::ENDIAN_LITTLE};

        /** Two alternate ways of writing data, one which is direct to the
         *  backing array and the other using ByteBuffer object methods. */
        bool useByteBuffer = false;

        /** Did this object create the byte buffer? */
        bool createdBuffer = false;

        /** When writing to buffer, generate EvioNode objects as evio
         *  structures are being created. */
        bool generateNodes = false;

        /** If {@link #generateNodes} is {@code true}, then store
         *  generated node objects in this list (in buffer order. */
        std::vector<std::shared_ptr<EvioNode>> nodes;

        /** Maximum allowed evio structure levels in buffer. */
        static const uint32_t MAX_LEVELS = 50;

        /** Number of bytes to pad short and byte data. */
        static const uint32_t padCount[] = {0,3,2,1};


        /**
         * This class stores information about an evio structure
         * (bank, segment, or tagsegment) which allows us to write
         * a length or padding in that structure in the buffer.
         */
        class StructureContent {
            /** Starting position of this structure in the buffer. */
            size_t pos;
            /** Keep track of amount of primitive data written for finding padding.
             *  Can be either bytes or shorts. */
            uint32_t dataLen;
            /** Padding for byte and short data. */
            uint32_t padding;
            /** Type of evio structure this is. */
            DataType type;
            /** Type of evio structures or data contained. */
            DataType dataType;


            void setData(size_t pos, DataType type, DataType dataType) {
                this.pos = pos;
                this.type = type;
                this.dataType = dataType;
                padding = dataLen = 0;
            }
        }


        /** The top (first element) of the stack is the first structure
         *  created or the event bank.
         *  Each level is the parent of the next one down (index + 1) */
        std::vector<std::shared_ptr<StructureContent>> stackArray;

        /** Each element of the array is the total length of all evio
         *  data in a structure including the full length of that
         *  structure's header. There is one length for each level
         *  of evio structures with the 0th element being the top
         *  structure (or event) level. */
        std::vector<uint32_t> totalLengths;

        /** Current evio structure being created. */
        std::shared_Ptr<StructureContent> currentStructure = nullptr;

        /** Level of the evio structure currently being created.
         *  Starts at 0 for the event bank. */
        int32_t currentLevel = -1;


    public:


        /**
         * This is the constructor to use for building a new event
         * (just the event in a buffer, not the full evio file format).
         * A buffer is created in this constructor.
         *
         * @param bufferSize size of byte buffer (in bytes) to create.
         * @param order      byte order of created buffer.
         *
         * @throws EvioException if arg is null;
         *                       if bufferSize arg too small
         */
        CompactEventBuilder(size_t bufferSize, ByteOrder const & order) :
                CompactEventBuilder(bufferSize, order, false) {
        }


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
        CompactEventBuilder(size_t bufferSize, ByteOrder const & order, bool generateNodes) :
                order(order), generateNodes(generateNodes) {

            // Create buffer
            buffer = std::make_shared<ByteBuffer>(bufferSize);
            array = buffer.array();
            buffer.order(order);

            // Init variables
            createdBuffer = true;

            totalLengths.reserve(MAX_LEVELS);
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
         *
         * @throws EvioException if arg is null;
         *                       if buffer is too small
         */
        CompactEventBuilder(std::shared_ptr<ByteBuffer> & buffer) :
                CompactEventBuilder(buffer,false) {}


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
        CompactEventBuilder(std::shared_ptr<ByteBuffer> buffer, bool generateNodes) {

            if (buffer == nullptr) {
                throw EvioException("null arg(s)");
            }

            totalLengths.reserve(MAX_LEVELS);
            stackArray.reserve(MAX_LEVELS);

            for (int i=0; i < MAX_LEVELS; i++) {
                stackArray[i] = std::make_shared<StructureContent>();
            }

            initBuffer(buffer, generateNodes);
        }


        /**
         * Set the buffer to be written into.
         * @param buffer buffer to be written into.
         * @throws EvioException if arg is null;
         *                       if buffer is too small
         */
        void setBuffer(std::shared_ptr<ByteBuffer> buffer) {
            setBuffer(buffer, false);
        }


        /**
         * Set the buffer to be written into.
         * @param buffer buffer to be written into.
         * @param generateNodes generate and store an EvioNode object
         *                      for each evio structure created.
         * @throws EvioException if arg is null;
         *                       if buffer is too small
         */
        void setBuffer(std::shared_ptr<ByteBuffer> buffer, bool generateNodes) {
            if (buffer == nullptr) {
                throw EvioException("null buffer arg");
            }
            initBuffer(buffer, generateNodes);
        }


    private:

        /**
         * Set the buffer to be written into but do NOT reallocate
         * stackArray and totalLengths arrays and do NOT refill
         * stackArray with objects. This avoid unnecessary garbage
         * generation.
         *
         * @param buffer buffer to be written into.
         * @param generateNodes generate and store an EvioNode object
         *                      for each evio structure created.
         * @throws EvioException if arg is null;
         *                       if buffer is too small
         */
        void initBuffer(std::shared_ptr<ByteBuffer> buffer, bool generateNodes) {

            this->buffer = buffer;
            this->generateNodes = generateNodes;

            // Prepare buffer
            buffer.clear();
            Arrays.fill(totalLengths, 0);
            order = buffer.order();

            if (buffer.limit() < 8) {
                throw EvioException("compact buffer too small");
            }

            // Protect against using the backing array of slices
            if (buffer.hasArray() && buffer.array().length == buffer.capacity()) {
                array = buffer.array();
            }
            else {
                useByteBuffer = true;
            }

            // Init variables
            if (nodes != null) {
                nodes.clear();
            }
            currentLevel = -1;
            createdBuffer = false;
            currentStructure = nullptr;
        }


    public:

        /**
         * Get the buffer being written into.
         * The buffer is ready to read.
         * @return buffer being written into.
         */
        std::shared_ptr<ByteBuffer> getBuffer() {
            buffer.limit(position).position(0);
            return buffer;
        }


        /**
         * Get the total number of bytes written into the buffer.
         * @return total number of bytes written into the buffer.
         */
        size_t getTotalBytes() {return position;}


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
        EvioNode openSegment(int tag, DataType dataType) {

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
            // were made to it (e.g. by doing buffer.flip() in order to read).
            // All this does is set pos = 0, limit = capacity, it does NOT
            // clear the data. We're keep track of the position to write at
            // in our own variable, "position".
            buffer->clear();

            if (buffer->limit() - position < 4) {
                throw EvioException("no room in buffer");
            }

            // For now, assume length and padding = 0
            if (useByteBuffer) {
                if (order == ByteOrder.BIG_ENDIAN) {
                    buffer->put(position,   (uint8_t)tag);
                    buffer->put(position+1, (uint8_t)(dataType.getValue() & 0x3f));
                }
                else {
                    buffer.put(position+2, (uint8_t)(dataType.getValue() & 0x3f));
                    buffer.put(position+3, (uint8_t)tag);
                }
            }
            else {
                if (order == ByteOrder.BIG_ENDIAN) {
                    array[position]   = (uint8_t)tag;
                    array[position+1] = (uint8_t)(dataType.getValue() & 0x3f);
                }
                else {
                    array[position+2] = (uint8_t)(dataType.getValue() & 0x3f);
                    array[position+3] = (uint8_t)tag;
                }
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
                nodes.add(node);
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
         EvioNode openTagSegment(int tag, DataType dataType) {

            if (currentStructure == nullptr) {
                throw EvioException("add a bank (event) first");
            }

            // Tagsegments not allowed if parent holds different type
            if (currentStructure.dataType != DataType::TAGSEGMENT) {
                throw EvioException("may NOT add tagsegment type, expecting " +
                                    currentStructure->dataType.toString());
            }

            // Make sure we can use all of the buffer in case external changes
            // were made to it (e.g. by doing buffer.flip() in order to read).
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

            if (useByteBuffer) {
                if (buffer.order() == ByteOrder::ENDIAN_BIG) {
                    buffer.putShort(position, compositeWord);
                }
                else {
                    buffer.putShort(position+2, compositeWord);
                }
            }
            else {
                if (order == ByteOrder::ENDIAN_BIG) {
                    array[position]   = (uint8_t) (compositeWord >> 8);
                    array[position+1] = (uint8_t)  compositeWord;
                }
                else {
                    array[position+2] = (uint8_t)  compositeWord;
                    array[position+3] = (uint8_t) (compositeWord >> 8);
                }
            }


            // currentLevel starts at -1
            if (++currentLevel >= MAX_LEVELS) {
                throw EvioException("too many nested evio structures, increase MAX_LEVELS from " +
                                    std""tp_string(MAX_LEVELS));
            }

            // currentStructure is the bank we are creating right now
            currentStructure = stackArray[currentLevel];
            currentStructure->setData(position, DataType::TAGSEGMENT, dataType);

            // We just wrote a bank header so this and all
            // containing levels increase in length by 1.
            addToAllLengths(1);

            // Do we generate an EvioNode object corresponding to this segment?
            std::shared_Ptr<EvioNode> node = nullptr;
            if (generateNodes) {
                // This constructor does not have lengths or create allNodes list, blockNode
                node = std::make_shared<EvioNode>(tag, 0, position, position+4,
                                                  DataType::TAGSEGMENT, dataType,
                                                  buffer);
                nodes.add(node);
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
        EvioNode openBank(uint16_t tag, uint8_t num, DataType const & dataType) {

            // Banks not allowed if parent holds different type
            if (currentStructure != nullptr &&
                (currentStructure->dataType != DataType::BANK &&
                 currentStructure->dataType != DataType::ALSOBANK)) {
                throw EvioException("may NOT add bank type, expecting " +
                                        currentStructure->dataType.toString());
            }

            // Make sure we can use all of the buffer in case external changes
            // were made to it (e.g. by doing buffer.flip() in order to read).
            // All this does is set pos = 0, limit = capacity, it does NOT
            // clear the data. We're keeping track of the position to write at
            // in our own variable, "position".
            buffer->clear();

            if (buffer->limit() - position < 8) {
                throw EvioException("no room in buffer");
            }

            // Write bank header into buffer, assuming padding = 0
            if (useByteBuffer) {
                // Bank w/ no data has len = 1
                buffer->putInt(position, 1);

                if (buffer->order() == ByteOrder::ENDIAN_BIG) {
                    buffer->putShort(position + 4, tag);
                    buffer->put(position + 6, (uint8_t)(dataType.getValue() & 0x3f));
                    buffer->put(position + 7, num);
                }
                else {
                    buffer->put(position + 4, num);
                    buffer->put(position + 5, (uint8_t)(dataType.getValue() & 0x3f));
                    buffer->putShort(position + 6, tag);
                }
            }
            else {
                // Bank w/ no data has len = 1

                if (order == ByteOrder::ENDIAN_BIG) {
                    // length word
                    array[position]   = (uint8_t)0;
                    array[position+1] = (uint8_t)0;
                    array[position+2] = (uint8_t)0;
                    array[position+3] = (uint8_t)1;

                    array[position+4] = (uint8_t)(tag >> 8);
                    array[position+5] = (uint8_t)tag;
                    array[position+6] = (uint8_t)(dataType.getValue() & 0x3f);
                    array[position+7] = num;
                }
                else {
                    array[position]   = (uint8_t)1;
                    array[position+1] = (uint8_t)0;
                    array[position+2] = (uint8_t)0;
                    array[position+3] = (uint8_t)0;

                    array[position+4] = num;
                    array[position+5] = (uint8_t)(dataType.getValue() & 0x3f);
                    array[position+6] = (uint8_t)tag;
                    array[position+7] = (uint8_t)(tag >> 8);
                }
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
                nodes.add(node);
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
        bool closeStructure() {
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
        void closeAll() {
            while (!closeStructure()) {}
        }


        /**
         * In the emu software package, it's necessary to change the top level
         * tag after the top level bank has already been created. This method
         * allows changing it after initially written.
         * @param tag  new tag value of top-level, event bank.
         */
        void setTopLevelTag(short tag) {

            if (useByteBuffer) {
                if (buffer.order() == ByteOrder::ENDIAN_BIG) {
                    buffer.putShort(4, tag);
                }
                else {
                    buffer.putShort(6, tag);
                }
            }
            else {
                if (order == ByteOrder::ENDIAN_BIG) {
                    array[4] = (uint8_t)(tag >> 8);
                    array[5] = (uint8_t)tag;
                }
                else {
                    array[6] = (uint8_t)tag;
                    array[7] = (uint8_t)(tag >> 8);
                }
            }
        }


    private:

        /**
         * This method adds the given length to the current structure's existing
         * length and all its ancestors' lengths. Nothing is written into the
         * buffer. The lengths are keep in an array for later use in writing to
         * the buffer.
         *
         * @param len length to add
         */
     void addToAllLengths(uint32_t len) {
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
     void setCurrentHeaderLength(uint32_t len) {

         DataType cont & type = currentStructure.type;
            bool isBigEndian = buffer.order().isBigEndian();

            if (type == DataType::BANK || type == DataType::ALSOBANK) {
                if (useByteBuffer) {
                    buffer.putInt(currentStructure.pos, len);
                }
                else {
                    try {
                        ByteDataTransformer.toBytes(len, order, array, currentStructure.pos);
                    }
                    catch (EvioException e) {/* never happen*/}
                }
            }
            else if (type == DataType::SEGMENT || type == DataType::ALSOSEGMENT || type == DataType::TAGSEGMENT) {
                if (useByteBuffer) {
                    if (isBigEndian) {
                        buffer.putShort(currentStructure.pos + 2, (uint16_t) len);
                    }
                    else {
                        buffer.putShort(currentStructure.pos, (uint16_t) len);
                    }
                }
                else {
                    try {
                        if (isBigEndian) {
                            ByteDataTransformer.toBytes((uint16_t) len, order,
                                                        array, currentStructure.pos + 2);
                        }
                        else {
                            ByteDataTransformer.toBytes((uint16_t) len, order,
                                                        array, currentStructure.pos);
                        }
                    }
                    catch (EvioException e) {/* never happen*/}
                }
            }
        }



        /**
         * This method sets the padding of the current evio structure in the buffer.
         * @param padding padding of current structure's data
         */
        void setCurrentHeaderPadding(uint32_t padding) {

            // byte containing padding
            uint8_t b = (uint8_t)((currentStructure.dataType.getValue() & 0x3f) | (padding << 6));

            DataType cont & type = currentStructure.type;
            bool isBigEndian = buffer.order().isBigEndian();

            if (type == DataType::BANK || type == DataType::ALSOBANK) {
                if (useByteBuffer) {
                    if (isBigEndian) {
                        buffer.put(currentStructure.pos + 6, b);
                    }
                    else {
                        buffer.put(currentStructure.pos + 5, b);
                    }
                }
                else {
                    if (isBigEndian) {
                        array[currentStructure.pos + 6] = b;
                    }
                    else {
                        array[currentStructure.pos + 5] = b;
                    }
                }
            }
            else if (type == DataType::SEGMENT || type == DataType::ALSOSEGMENT) {
                if (useByteBuffer) {
                    if (isBigEndian) {

                        buffer.put(currentStructure.pos + 1, b);
                    }
                    else {
                        buffer.put(currentStructure.pos + 2, b);
                    }
                }
                else {
                    if (isBigEndian) {
                        array[currentStructure.pos + 1] = b;
                    }
                    else {
                        array[currentStructure.pos + 2] = b;
                    }
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
        void writeHeader(std::shared_ptr<EvioNode> & node) {

            DataType cont & type = currentStructure.type;
            bool isBigEndian = order.isBigEndian();

            if (type == DataType::BANK || type == DataType::ALSOBANK) {
                if (useByteBuffer) {
                    buffer.putInt(position, node.len);
                    if (isBigEndian) {
                        buffer.putShort(position + 4, node.tag);
                        buffer.put(position + 6, (uint8_t)((node.dataType & 0x3f) | (node.pad << 6)));
                        buffer.put(position + 7, node.num);
                    }
                    else {
                        buffer.put(position + 4, node.num);
                        buffer.put(position + 5, (uint8_t)((node.dataType & 0x3f) | (node.pad << 6)));
                        buffer.putShort(position + 6, node.tag);
                    }
                }
                else {
                    if (isBigEndian) {
                        array[position]     = (uint8_t)(node.len >> 24);
                        array[position + 1] = (uint8_t)(node.len >> 16);
                        array[position + 2] = (uint8_t)(node.len >> 8);
                        array[position + 3] = (uint8_t) node.len;

                        array[position + 4] = (uint8_t)(node.tag >> > 8);
                        array[position + 5] = (uint8_t) node.tag;
                        array[position + 6] = (uint8_t)((node.dataType & 0x3f) | (node.pad << 6));
                        array[position + 7] = node.num;
                    }
                    else {
                        array[position]     = (uint8_t)(node.len);
                        array[position + 1] = (uint8_t)(node.len >> 8);
                        array[position + 2] = (uint8_t)(node.len >> 16);
                        array[position + 3] = (uint8_t)(node.len >> 24);

                        array[position + 4] = node.num;
                        array[position + 5] = (uint8_t)((node.dataType & 0x3f) | (node.pad << 6));
                        array[position + 6] = (uint8_t) node.tag;
                        array[position + 7] = (uint8_t)(node.tag >> > 8);
                    }
                }

                position += 8;
            }
            else if (type == DataType::SEGMENT || type == DataType::ALSOSEGMENT) {
                if (useByteBuffer) {
                    if (isBigEndian) {
                        buffer.put(position, (uint8_t) node.tag);
                        buffer.put(position + 1, (uint8_t)((node.dataType & 0x3f) | (node.pad << 6)));
                        buffer.putShort(position + 2, (uint16_t) node.len);
                    }
                    else {
                        buffer.putShort(position, (uint16_t) node.len);
                        buffer.put(position + 1, (uint8_t)((node.dataType & 0x3f) | (node.pad << 6)));
                        buffer.put(position + 2, (uint8_t) node.tag);
                    }
                }
                else {
                    if (isBigEndian) {
                        array[position]     = (uint8_t) node.tag;
                        array[position + 1] = (uint8_t)((node.dataType & 0x3f) | (node.pad << 6));
                        array[position + 2] = (uint8_t)(node.len >> 8);
                        array[position + 3] = (uint8_t) node.len;
                    }
                    else {
                        array[position]     = (uint8_t) node.len;
                        array[position + 1] = (uint8_t)(node.len >> 8);
                        array[position + 2] = (uint8_t)((node.dataType & 0x3f) | (node.pad << 6));
                        array[position + 3] = (uint8_t) node.tag;
                    }
                }

                position += 4;
            }
            else if (type == DataType::TAGSEGMENT) {

                uint16_t compositeWord = (uint16_t) ((node.tag << 4) | (node.dataType & 0xf));

                if (useByteBuffer) {
                    if (isBigEndian) {
                        buffer.putShort(position, compositeWord);
                        buffer.putShort(position + 2, (uint16_t) node.len);
                    }
                    else {
                        buffer.putShort(position, (uint16_t) node.len);
                        buffer.putShort(position + 2, compositeWord);
                    }
                }
                else {
                    if (isBigEndian) {
                        array[position]     = (uint8_t)(compositeWord >> 8);
                        array[position + 1] = (uint8_t) compositeWord;
                        array[position + 2] = (uint8_t)(node.len >> 8);
                        array[position + 3] = (uint8_t) node.len;
                    }
                    else {
                        array[position]     = (uint8_t) node.len;
                        array[position + 1] = (uint8_t)(node.len >> 8);
                        array[position + 2] = (uint8_t) compositeWord;
                        array[position + 3] = (uint8_t)(compositeWord >> 8);
                    }
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
         */
        void writeNode(std::share_ptr<EvioNode> & node, bool swapData) {

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
                    try {
                        ByteDataTransformer.swapData(node->getDataTypeObj(), nodeBuf,
                                                     buffer, node->dataPos, position,
                                                     node->dataLen, false);
                    }
                    catch (EvioException & e) {
                        std::cout << e.what() << std::endl;
                    }
                }
                else {
                    if (!useByteBuffer && nodeBuf->hasArray() && buffer->hasArray() &&
                        nodeBuf->array().length == nodeBuf->capacity()) {
                        System.arraycopy(nodeBuf->array(), node->dataPos,
                                         array, position, 4*node->dataLen);
                    }
                    else {
// TODO: IS THIS NECESSARY????
                        auto duplicateBuf = nodeBuf.duplicate();
                        duplicateBuf->limit(node->dataPos + 4*node->dataLen)->position(node->dataPos);

                        buffer->position(position);
                        buffer->put(duplicateBuf); // this method is relative to position
                        buffer->position(0);    // get ready for external reading
                    }
                }

                position += 4*node->dataLen;
            }
        }

    public:

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
        void addEvioNode(std::share_ptr<EvioNode> node) {
            if (node == nullptr) {
                throw EvioException("no data to add");
            }

            if (currentStructure == nullptr) {
                throw EvioException("add a bank, segment, or tagsegment first");
            }

            if (currentStructure->dataType != node->getTypeObj()) {
                throw EvioException("may only add " + currentStructure.dataType.toString() + " data");
            }

            // Sets pos = 0, limit = capacity, & does NOT clear data
            buffer->clear();

            uint32_t len = node.getTotalBytes();
//std::cout << "addEvioNode: buf lim = " << buffer.limit() <<
//                           " - pos = " << position << " = (" << (buffer->limit() - position) <<
//                           ") must be >= " << node->getTotalBytes() << " node total bytes");
            if ((buffer->limit() - position) < len) {
                throw EvioException("no room in buffer");
            }

            addToAllLengths(len/4);  // # 32-bit words

            auto nodeBuf = node.getBuffer();

            if (nodeBuf->order() == buffer->order()) {
                if (!useByteBuffer && nodeBuf->hasArray() && buffer->hasArray() &&
                    nodeBuf->array().length == nodeBuf->capacity()) {
//std::cout << "addEvioNode: arraycopy node (same endian)" << std::endl;
                    System.arraycopy(nodeBuf.array(), node.pos,
                                     array, position, node.getTotalBytes());
                }
                else {
//std::cout << "addEvioNode: less efficient node copy (same endian)" << std::endl;
                    // Better performance not to slice/duplicate buffer (5 - 10% faster)
                    auto duplicateBuf = nodeBuf.duplicate();
                    duplicateBuf->limit(node->pos + node->getTotalBytes()).position(node->pos);

                    buffer->position(position);
                    buffer->put(duplicateBuf); // this method is relative to position
                    buffer->position(0);       // get ready for external reading
                }

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
        void addByteData(uint8_t * data, uint32_t len) {
            if (data == nullptr || len < 1) {
                throw EvioException("no data to add");
            }

            if (currentStructure == nullptr) {
                throw EvioException("add a bank, segment, or tagsegment first");
            }

            if (currentStructure->dataType != DataType::CHAR8  &&
                currentStructure->dataType != DataType::UCHAR8)  {
                throw EvioException("may NOT add " + currentStructure->dataType + " data");
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
            if (useByteBuffer) {
                buffer->position(position);
                buffer->put(data);
                buffer->position(0);
            }
            else {
                System.arraycopy(data, 0, array, position, len);
            }

            // Calculate the padding
            currentStructure->padding = padCount[currentStructure.dataLen % 4];

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
        void addIntData(uint32_t * data, uint32_t len) {

            if (data == nullptr) {
                throw EvioException("data arg null");
            }

            if (len == 0) return;

            if (currentStructure == nullptr) {
                throw EvioException("add a bank, segment, or tagsegment first");
            }

            if (currentStructure.dataType != DataType::INT32  &&
                currentStructure.dataType != DataType::UINT32 &&
                currentStructure.dataType != DataType::UNKNOWN32)  {
                throw EvioException("may NOT add " + currentStructure.dataType + " data");
            }

            // Sets pos = 0, limit = capacity, & does NOT clear data
            buffer->clear();

            if ((buffer->limit() - position) < 4*len) {
                throw EvioException("no room in buffer");
            }

            addToAllLengths(len);  // # 32-bit words

            if (useByteBuffer) {
                buffer->position(position);
                IntBuffer db = buffer.asIntBuffer();
                db.put(data, 0, len);
                buffer->position(0);
            }
            else {
                uint32_t j, pos = position;
                if (order == ByteOrder::ENDIAN_BIG) {
                    for (int i=0; i < len + offset; i++) {
                        j = data[i];
                        array[pos++] = (uint8_t) (j >> 24);
                        array[pos++] = (uint8_t) (j >> 16);
                        array[pos++] = (uint8_t) (j >> 8);
                        array[pos++] = (uint8_t) (j);
                    }
                }
                else {
                    for (int i=0; i < len + offset; i++) {
                        j = data[i];
                        array[pos++] = (uint8_t) (j);
                        array[pos++] = (uint8_t) (j >> 8);
                        array[pos++] = (uint8_t) (j >> 16);
                        array[pos++] = (uint8_t) (j >> 24);
                    }
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
        void addShortData(uint16_t * data, uint32_t len) {

            if (data == nullptr) {
                throw EvioException("data arg null");
            }

            if (len == 0) return;

            if (currentStructure == nullptr) {
                throw EvioException("add a bank, segment, or tagsegment first");
            }

            if (currentStructure.dataType != DataType::SHORT16  &&
                currentStructure.dataType != DataType::USHORT16)  {
                throw new EvioException("may NOT add " + currentStructure.dataType + " data");
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
            UINT32_T lastWordLen = (currentStructure.dataLen + 1)/2;
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

            if (useByteBuffer) {
                buffer->position(position);
                ShortBuffer db = buffer.asShortBuffer();
                db.put(data, 0, len);
                buffer->position(0);
            }
            else {
                size_t pos = position;
                if (order == ByteOrder::ENDIAN_BIG) {
                    for (int i=0; i < len + offset; i++) {
                        uint16_t aData = data[i];
                        array[pos++] = (uint8_t) (aData >> 8);
                        array[pos++] = (uint8_t) (aData);
                    }
                }
                else {
                    for (int i=0; i < len + offset; i++) {
                        uint16_t aData = data[i];
                        array[pos++] = (uint8_t) (aData);
                        array[pos++] = (uint8_t) (aData >> 8);
                    }
                }
            }

            currentStructure->padding = 2*(currentStructure->dataLen % 2);
            // Advance position
            position += 2*len + currentStructure->padding;
        }


        /**
         * Appends a short integer to the structure.
         *
         * @param data the short data to append.
         * @throws EvioException if adding wrong data type to structure;
         *                       if structure not added first;
         *                       if no room in buffer for data.
         */
        void addShortData(uint16_t data) {

            if (currentStructure == nullptr) {
                throw EvioException("add a bank, segment, or tagsegment first");
            }

            if (currentStructure.dataType != DataType::SHORT16  &&
                currentStructure.dataType != DataType::USHORT16)  {
                throw new EvioException("may NOT add " + currentStructure.dataType + " data");
            }

            // Sets pos = 0, limit = capacity, & does NOT clear data
            buffer->clear();

            if ((buffer->limit() - position) < 2) {
                throw EvioException("no room in buffer");
            }

            // Things get tricky if this method is called multiple times in succession.
            // Keep track of how much data we write each time so length and padding
            // are accurate.

            // Last increase in length
            UINT32_T lastWordLen = (currentStructure->dataLen + 1)/2;

            // If we are adding to existing data,
            // place position before previous padding.
            if (currentStructure->dataLen > 0) {
                position -= currentStructure->padding;
            }

            // Total number of bytes to write & already written
            currentStructure->dataLen += 1;

            // New total word len of this data
            uint32_t totalWordLen = (currentStructure->dataLen + 1)/2;

            // Increase lengths by the difference
            addToAllLengths(totalWordLen - lastWordLen);

            if (useByteBuffer) {
                buffer->position(position);
                buffer->putShort(data);
                buffer->position(0);
            }
            else {
                size_t pos = position;
                if (order == ByteOrder::ENDIAN_BIG) {
                    array[pos++] = (uint8_t) (data >> 8);
                    array[pos]   = (uint8_t) (data);
                }
                else {
                    array[pos++] = (uint8_t) (data);
                    array[pos]   = (uint8_t) (data >> 8);
                }
            }

            currentStructure->padding = 2*(currentStructure->dataLen % 2);

            // Advance position
            position += 2 + currentStructure->padding;
        }


        /**
         * Appends long data to the structure.
         *
         * @param data    the long data to append.
         * @param offset  offset into data array.
         * @param len     the number of longs from data to append.
         * @throws EvioException if data is null;
         *                       if adding wrong data type to structure;
         *                       if structure not added first;
         *                       if no room in buffer for data.
         */
        void addLongData(uint64_t * data, uint32_t len) {

            if (data == null) {
                throw new EvioException("data arg null");
            }

            if (len == 0) return;

            if (currentStructure == nullptr) {
                throw EvioException("add a bank, segment, or tagsegment first");
            }

            if (currentStructure.dataType != DataType::LONG64  &&
                currentStructure.dataType != DataType::ULONG64)  {
                throw new EvioException("may NOT add " + currentStructure.dataType + " data");
            }

            // Sets pos = 0, limit = capacity, & does NOT clear data
            buffer->clear();

            if ((buffer->limit() - position) < 8*len) {
                throw EvioException("no room in buffer");
            }

            addToAllLengths(2*len);  // # 32-bit words

            if (useByteBuffer) {
                buffer->position(position);
                LongBuffer db = buffer.asLongBuffer();
                db.put(data, 0, len);
                buffer->position(0);
            }
            else {
                size_t pos = position;
                if (order == ByteOrder::ENDIAN_BIG) {
                    for (int i=0; i < len + offset; i++) {
                        uint64_t aData = data[i];
                        array[pos++] = (uint8_t) (aData >> 56);
                        array[pos++] = (uint8_t) (aData >> 48);
                        array[pos++] = (uint8_t) (aData >> 40);
                        array[pos++] = (uint8_t) (aData >> 32);
                        array[pos++] = (uint8_t) (aData >> 24);
                        array[pos++] = (uint8_t) (aData >> 16);
                        array[pos++] = (uint8_t) (aData >> 8);
                        array[pos++] = (uint8_t) (aData);
                    }
                }
                else {
                    for (int i=0; i < len + offset; i++) {
                        uint64_t aData = data[i];
                        array[pos++] = (uint8_t) (aData);
                        array[pos++] = (uint8_t) (aData >> 8);
                        array[pos++] = (uint8_t) (aData >> 16);
                        array[pos++] = (uint8_t) (aData >> 24);
                        array[pos++] = (uint8_t) (aData >> 32);
                        array[pos++] = (uint8_t) (aData >> 40);
                        array[pos++] = (uint8_t) (aData >> 48);
                        array[pos++] = (uint8_t) (aData >> 56);
                    }
                }
            }

            position += 8*len;       // # bytes
        }


        /**
         * Appends float data to the structure.
         *
         * @param data the float data to append.
         * @throws EvioException if data is null or empty;
         *                       if adding wrong data type to structure;
         *                       if structure not added first;
         *                       if no room in buffer for data.
         */
        void addFloatData(float * data, uint32_t len) {
            if (data == nullptr || len < 1) {
                throw EvioException("no data to add");
            }

            if (currentStructure == nullptr) {
                throw new EvioException("add a bank, segment, or tagsegment first");
            }

            if (currentStructure.dataType != DataType::FLOAT32) {
                throw new EvioException("may NOT add " + currentStructure.dataType + " data");
            }

            // Sets pos = 0, limit = capacity, & does NOT clear data
            buffer->clear();

            if ((buffer->limit() - position) < 4*len) {
                throw EvioException("no room in buffer");
            }

            addToAllLengths(len);  // # 32-bit words

            if (useByteBuffer) {
                buffer->position(position);
                FloatBuffer db = buffer.asFloatBuffer();
                db.put(data, 0, len);
                buffer->position(0);
            }
            else {
                int aData, pos = position;
                if (order == ByteOrder.BIG_ENDIAN) {
                    for (float fData : data) {
                        aData = Float.floatToRawIntBits(fData);
                        array[pos++] = (byte) (aData >> 24);
                        array[pos++] = (byte) (aData >> 16);
                        array[pos++] = (byte) (aData >> 8);
                        array[pos++] = (byte) (aData);
                    }
                }
                else {
                    for (float fData : data) {
                        aData = Float.floatToRawIntBits(fData);
                        array[pos++] = (byte) (aData);
                        array[pos++] = (byte) (aData >> 8);
                        array[pos++] = (byte) (aData >> 16);
                        array[pos++] = (byte) (aData >> 24);
                    }
                }
            }

            position += 4*len;     // # bytes
        }




        /**
         * Appends double data to the structure.
         *
         * @param data the double data to append.
         * @throws EvioException if data is null or empty;
         *                       if adding wrong data type to structure;
         *                       if structure not added first;
         *                       if no room in buffer for data.
         */
        void addDoubleData(double * data, uinr21_t len) {
            if (data == nullptr || len < 1) {
                throw EvioException("no data to add");
            }

            if (currentStructure == nullptr) {
                throw EvioException("add a bank, segment, or tagsegment first");
            }

            if (currentStructure->dataType != DataType::DOUBLE64) {
                throw EvioException("may NOT add " + currentStructure->dataType + " data");
            }

            // Sets pos = 0, limit = capacity, & does NOT clear data
            buffer->clear();

            addToAllLengths(2*len);  // # 32-bit words

            if (useByteBuffer) {
                buffer->position(position);
                DoubleBuffer db = buffer.asDoubleBuffer();
                db.put(data, 0, len);
                buffer->position(0);
            }
            else {
                uint64_t aData;
                size_t pos = position;
                if (order == ByteOrder::ENDIAN_BIG) {
                    for (double dData : data) {
                        aData = Double.doubleToRawLongBits(dData);
                        array[pos++] = (uint8_t) (aData >> 56);
                        array[pos++] = (uint8_t) (aData >> 48);
                        array[pos++] = (uint8_t) (aData >> 40);
                        array[pos++] = (uint8_t) (aData >> 32);
                        array[pos++] = (uint8_t) (aData >> 24);
                        array[pos++] = (uint8_t) (aData >> 16);
                        array[pos++] = (uint8_t) (aData >> 8);
                        array[pos++] = (uint8_t) (aData);
                    }
                }
                else {
                    for (double dData : data) {
                        aData = Double.doubleToRawLongBits(dData);
                        array[pos++] = (uint8_t) (aData);
                        array[pos++] = (uint8_t) (aData >> 8);
                        array[pos++] = (uint8_t) (aData >> 16);
                        array[pos++] = (uint8_t) (aData >> 24);
                        array[pos++] = (uint8_t) (aData >> 32);
                        array[pos++] = (uint8_t) (aData >> 40);
                        array[pos++] = (uint8_t) (aData >> 48);
                        array[pos++] = (uint8_t) (aData >> 56);
                    }
                }
            }

            position += 8*len;     // # bytes
        }


        /**
         * Appends string array to the structure.
         *
         * @param strings the strings to append.
         * @throws EvioException if data is null or empty;
         *                       if adding wrong data type to structure;
         *                       if structure not added first;
         *                       if no room in buffer for data.
         */
        void addStringData(String strings[]) {
            if (strings == null || strings.length < 1) {
                throw new EvioException("no data to add");
            }

            if (currentStructure == null) {
                throw new EvioException("add a bank, segment, or tagsegment first");
            }

            if (currentStructure.dataType != DataType.CHARSTAR8) {
                throw new EvioException("may only add " + currentStructure.dataType + " data");
            }

            // Convert strings into byte array (already padded)
            byte[] data = BaseStructure.stringsToRawBytes(strings);

            // Sets pos = 0, limit = capacity, & does NOT clear data
            buffer.clear();

            int len = data.length;
            if (buffer.limit() - position < len) {
                throw new EvioException("no room in buffer");
            }

            // Things get tricky if this method is called multiple times in succession.
            // In fact, it's too difficult to bother with so just throw an exception.
            if (currentStructure.dataLen > 0) {
                throw new EvioException("addStringData() may only be called once per structure");
            }

            if (useByteBuffer) {
                buffer.position(position);
                buffer.put(data);
                buffer.position(0);
            }
            else {
                System.arraycopy(data, 0, array, position, len);
            }
            currentStructure.dataLen += len;
            addToAllLengths(len/4);

            position += len;
        }


        /**
         * Appends CompositeData objects to the structure.
         *
         * @param data the CompositeData objects to append, or set if there is no existing data.
         * @throws EvioException if data is null or empty;
         *                       if adding wrong data type to structure;
         *                       if structure not added first;
         *                       if no room in buffer for data.
         */
    public void addCompositeData(CompositeData[] data) throws EvioException {

                if (data == null || data.length < 1) {
                    throw new EvioException("no data to add");
                }

                if (currentStructure == null) {
                    throw new EvioException("add a bank, segment, or tagsegment first");
                }

                if (currentStructure.dataType != DataType.COMPOSITE) {
                    throw new EvioException("may only add " + currentStructure.dataType + " data");
                }

                // Composite data is always in the local (in this case, BIG) endian
                // because if generated in JVM that's true, and if read in, it is
                // swapped to local if necessary. Either case it's big endian.

                // Convert composite data into byte array (already padded)
                byte[] rawBytes = CompositeData.generateRawBytes(data, order);

                // Sets pos = 0, limit = capacity, & does NOT clear data
                buffer.clear();

                int len = rawBytes.length;
                if (buffer.limit() - position < len) {
                    throw new EvioException("no room in buffer");
                }

                // This method cannot be called multiple times in succession.
                if (currentStructure.dataLen > 0) {
                    throw new EvioException("addCompositeData() may only be called once per structure");
                }

                if (useByteBuffer) {
                    buffer.position(position);
                    buffer.put(rawBytes);
                    buffer.position(0);
                }
                else {
                    System.arraycopy(rawBytes, 0, array, position, len);
                }
                currentStructure.dataLen += len;
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
    public void toFile(String filename) {
            try {
                File file = new File(filename);
                FileOutputStream fos = new FileOutputStream(file);
                java.io.DataOutputStream dos = new java.io.DataOutputStream(fos);

                bool writeLastBlock = false;

                // Get buffer ready to read
                buffer.limit(position);
                buffer.position(0);

                // Write beginning 8 word header
                // total length
                int len = buffer.remaining();
//System.out.println("Buf len = " + len + " bytes");
                int len2 = len/4 + 8;
//System.out.println("Tot len = " + len2 + " words");
                // blk #
                int blockNum = 1;
                // header length
                int headerLen = 8;
                // event count
                int evCount = 1;
                // bit & version (last block, evio version 4 */
                int info = 0x204;
                if (writeLastBlock) {
                    info = 0x4;
                }

                // endian checker
                int magicNum = IBlockHeader.MAGIC_NUMBER;

                if (order == ByteOrder.LITTLE_ENDIAN) {
                    len2      = Integer.reverseBytes(len2);
                    blockNum  = Integer.reverseBytes(blockNum);
                    headerLen = Integer.reverseBytes(headerLen);
                    evCount   = Integer.reverseBytes(evCount);
                    info      = Integer.reverseBytes(info);
                    magicNum  = Integer.reverseBytes(magicNum);
                }

                // Write the block header

                dos.writeInt(len2);       // len
                dos.writeInt(blockNum);   // block num
                dos.writeInt(headerLen);  // header len
                dos.writeInt(evCount);    // event count
                dos.writeInt(0);          // reserved 1
                dos.writeInt(info);       // bit & version info
                dos.writeInt(0);          // reserved 2
                dos.writeInt(magicNum);   // magic #

                // Now write the event data
                dos.write(array, 0, len);

                dos.close();
            }
            catch (IOException e) {
                e.printStackTrace();
            }

            // Reset position & limit
            buffer.clear();
        }

    };


}


#endif //EVIO_6_0_COMPACTEVENTBUILDER_H
