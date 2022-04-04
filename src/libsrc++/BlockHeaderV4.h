//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_BLOCKHEADERV4_H
#define EVIO_6_0_BLOCKHEADERV4_H


#include <bitset>
#include <sstream>


#include "IBlockHeader.h"
#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "EvioException.h"


namespace evio {


    /**
     * This holds an evio block header, also known as a physical record header.
     * Unfortunately, in versions 1, 2 &amp; 3, evio files impose an anachronistic
     * block structure. The complication that arises is that logical records
     * (events) will sometimes cross physical record boundaries. This block structure
     * is changed in version 4 so that blocks only contain integral numbers of events.
     * The information stored in this block header has also changed.
     *
     *
     * <pre><code>
     * ################################
     * Evio block header, version 4:
     * ################################
     *
     * MSB(31)                          LSB(0)
     * &lt;---  32 bits ------------------------&gt;
     * _______________________________________
     * |            Block Length             |
     * |_____________________________________|
     * |            Block Number             |
     * |_____________________________________|
     * |          Header Length = 8          |
     * |_____________________________________|
     * |             Event Count             |
     * |_____________________________________|
     * |             reserved 1              |
     * |_____________________________________|
     * |          Bit info &amp; Version         |
     * |_____________________________________|
     * |             reserved 2              |
     * |_____________________________________|
     * |             Magic Int               |
     * |_____________________________________|
     *
     * The following bit #s start with 0.
     *
     *      Block Length       = number of ints in block (including this one).
     *      Block Number       = id number (starting at 1)
     *      Header Length      = number of ints in this header (8)
     *      Event Count        = number of events in this block (always an integral #).
     *                           NOTE: this value should not be used to parse the following
     *                           events since the first block may have a dictionary whose
     *                           presence is not included in this count.
     *      Reserved 1         = If bits 10-13 in bit info are RocRaw (1), then (in the first block)
     *                           this contains the CODA id of the source
     *      Bit info &amp; Version = Lowest 8 bits are the version number (4).
     *                           Upper 24 bits contain bit info.
     *                           If a dictionary is included as the first event, bit #8 is set (=1)
     *                           If a last block, bit #9 is set (=1)
     *      Reserved 2         = unused
     *      Magic Int          = magic number (0xc0da0100) used to check endianness
     *
     *
     *
     * Bit info has the following bits defined (bit #s start with 0):
     *   Bit  8     = true if dictionary is included (relevant for first block only)
     *
     *   Bit  9     = true if this block is the last block in file or network transmission
     *
     *   Bits 10-13 = type of events following (ROC Raw = 0, Physics = 1, PartialPhysics = 2,
     *                DisentangledPhysics = 3, User = 4, Control = 5, Other = 15).
     *
     *   Bit 14     = true if next (non-dictionary) event in this block is a "first event" to
     *                be placed at the beginning of each written file and its splits.
     *
     *                Bits 10-14 are useful ONLY for the CODA online use of evio.
     *                That's because only a single CODA event type is placed into
     *                a single (ET, cMsg) buffer, and each user or control event has its own
     *                buffer as well. That buffer then is parsed by an EvioReader or
     *                EvioCompactReader object. Thus all events will be of a single CODA type.
     *
     *
     * </code></pre>
     *
     *
     * @author heddle
     * @author timmer
     */
    class BlockHeaderV4 : public IBlockHeader {

    public:

        /** The minimum and expected block header size in 32 bit ints. */
        static const uint32_t HEADER_SIZE = 8;

        /** Dictionary presence is 9th bit in version/info word */
        static const uint32_t EV_DICTIONARY_MASK = 0x100;

        /** "Last block" is 10th bit in version/info word */
        static const uint32_t EV_LASTBLOCK_MASK  = 0x200;

        /** "Event type" is 11-14th bits` in version/info word */
        static const uint32_t EV_EVENTTYPE_MASK  = 0x3C00;

        /** "First event" is 15th bit in version/info word */
        static const uint32_t EV_FIRSTEVENT_MASK  = 0x4000;

        /** Position of word for size of block in 32-bit words. */
        static const uint32_t EV_BLOCKSIZE = 0;
        /** Position of word for block number, starting at 1. */
        static const uint32_t EV_BLOCKNUM = 1;
        /** Position of word for size of header in 32-bit words (=8). */
        static const uint32_t EV_HEADERSIZE = 2;
        /** Position of word for number of events in block. */
        static const uint32_t EV_COUNT = 3;
        /** Position of word for reserved. */
        static const uint32_t EV_RESERVED1 = 4;
        /** Position of word for version of file format. */
        static const uint32_t EV_VERSION = 5;
        /** Position of word for reserved. */
        static const uint32_t EV_RESERVED2 = 6;
        /** Position of word for magic number for endianness tracking. */
        static const uint32_t EV_MAGIC = 7;

        ///////////////////////////////////

        /** The block (physical record) size in 32 bit ints. */
        uint32_t size = 0;

        /** The block number. In a file, this is usually sequential.  */
        uint32_t number = 1;

        /** The block header length. Should be 8 in all cases, so getting this correct constitutes a check. */
        uint32_t headerLength = 8;

        /**
         * Since blocks only contain whole events in this version,
         * this stores the number of events contained in a block.
         */
        uint32_t eventCount = 0;

        /** The evio version, always 4.  */
        uint32_t version = 4;

        /** Value of first reserved word. */
        uint32_t reserved1 = 0;

        /** Value of second reserved word. */
        uint32_t reserved2 = 0;

        /** Bit information. Bit one: is the first event a dictionary? */
        std::bitset<24> bitInfo;

        /** This is the magic word, 0xc0da0100, used to check endianness. */
        uint32_t magicNumber = MAGIC_NUMBER;

        /** This is the byte order of the data being read. */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LOCAL};

        /**
         * This is not part of the block header proper. It is a position in a memory buffer of the start of the block
         * (physical record). It is kept for convenience.
         */
        int64_t bufferStartingPosition = 0L;


    public:

        /** Constructor initializes all fields to default values. */
        BlockHeaderV4() = default;


        /**
         * Creates a BlockHeader for evio version 4 format. Only the <code>block size</code>
         * and <code>block number</code> are provided. The other words, which can be
         * modified by setters, are initialized to these values:<br>
         *<ul>
         *<li><code>headerLength</code> is initialized to 8<br>
         *<li><code>version</code> is initialized to 4<br>
         *<li><code>bitInfo</code> is initialized to all bits off<br>
         *<li><code>magicNumber</code> is initialized to <code> {@link #MAGIC_NUMBER}.</code><br>
         *</ul>
         * @param sz the size of the block in ints.
         * @param num the block number--usually sequential.
         */
        BlockHeaderV4(uint32_t sz, uint32_t num) {
            size = sz;
            number = num;
        }


        /**
         * This copy constructor creates an evio version 4 BlockHeader
         * from another object of this class.
         * @param blkHeader block header object to copy
         */
        explicit BlockHeaderV4(std::shared_ptr<BlockHeaderV4> & blkHeader) {
            copy(blkHeader);
        }


        /**
        * This method copies another header's contents.
        * @param blkHeader block header object to copy
        */
        void copy(std::shared_ptr<BlockHeaderV4> & blkHeader) {
            size         = blkHeader->size;
            number       = blkHeader->number;
            headerLength = blkHeader->headerLength;
            version      = blkHeader->version;
            eventCount   = blkHeader->eventCount;
            reserved1    = blkHeader->reserved1;
            reserved2    = blkHeader->reserved2;
            byteOrder    = blkHeader->byteOrder;
            magicNumber  = blkHeader->magicNumber;
            bitInfo      = blkHeader->bitInfo;
            bufferStartingPosition = blkHeader->bufferStartingPosition;
        }


        /** {@inheritDoc} */
        uint32_t getSize() override {return size;}


        /**
         * Set the size of the block (physical record). Some trivial checking is done.
         * @param sz the new value for the size, in ints.
         * @throws EvioException if size &lt; 8
         */
        void setSize(uint32_t sz) {
            if (sz < 8) {
                throw EvioException("Bad value for size in block (physical record) header: " + std::to_string(sz));
            }
            size = sz;
        }


        /**
         * Get the number of events completely contained in the block.
         * NOTE: There are no partial events, only complete events stored in one block.
         *
         * @return the number of events in the block.
         */
        uint32_t getEventCount() const {return eventCount;}


        /**
         * Set the number of events completely contained in the block.
         * NOTE: There are no partial events, only complete events stored in one block.
         *
         * @param count the new number of events in the block.
         * @throws EvioException if count &lt; 0
         */
        void setEventCount(uint32_t count) {
            if (count < 0) {
                throw EvioException("Bad value for event count in block (physical record) header: " +
                                    std::to_string(count));
            }
            eventCount = count;
        }


        /** {@inheritDoc} */
        uint32_t getNumber() override {return number;}


        /**
         * Set the block number for this block (physical record).
         * In a file, this is usually sequential, starting at 1.
         * This is not checked.
         * @param num the number of the block (physical record).
         */
        void setNumber(uint32_t num) {number = num;}


        /**
         * Get the block header length in ints. This should be 8.
         * @return block header length in ints.
         */
        uint32_t getHeaderLength() const {return headerLength;}


        /** {@inheritDoc} */
        uint32_t getHeaderWords() override {return headerLength;}


        /**
         * Set the block header length, in ints. Although technically speaking this value
         * is variable, it should be 8. However, since this is usually read as part of reading
         * the physical record header, it is a good check to have a setter rather than just fix its value at 8.
         *
         * @param len the new block header length. This should be 8.
         */
        void setHeaderLength(uint32_t len) {
            if (len != HEADER_SIZE) {
                std::cout << "Warning: Block Header Length = " << headerLength << std::endl;
            }
            headerLength = len;
        }


        /** {@inheritDoc} */
        uint32_t getVersion() override {return version;}


        /**
         * Sets the evio version. Should be 4 but no check is performed here.
         * @param ver the evio version of evio.
         */
        void setVersion(uint32_t ver) {version = ver;}


        /** {@inheritDoc} */
        bool hasFirstEvent() override {return bitInfo[6];}


        /**
         * Does this integer indicate that there is an evio dictionary
         * (assuming it's the header's sixth word)?
         * @param i integer to examine.
         * @return <code>true</code> if this int indicates an evio dictionary, else <code>false</code>
         */
        static bool hasDictionary(uint32_t i) {return ((i & EV_DICTIONARY_MASK) > 0);}


        /** {@inheritDoc} */
        bool hasDictionary() override {return bitInfo[0];}


        /** {@inheritDoc} */
        bool isLastBlock() override {return bitInfo[1];}


        /** {@inheritDoc} */
        bool isCompressed() override {return false;}


        /** {@inheritDoc} */
        Compressor::CompressionType getCompressionType() override {return Compressor::UNCOMPRESSED;}


        /**
         * Does this block contain the "first event"
         * (first event to be written to each file split)?
         * @return <code>true</code> if this contains the first event, else <code>false</code>
         */
        bool hasFirstEvent() const {return bitInfo[6];}




        /**
         * Does this integer indicate that this is the last block
         * (assuming it's the header's sixth word)?
         * @param i integer to examine.
         * @return <code>true</code> if this int indicates the last block, else <code>false</code>
         */
        static bool isLastBlock(uint32_t i) {return ((i & EV_LASTBLOCK_MASK) > 0);}


        /**
         * Set the bit in the given arg which indicates this is the last block.
         * @param i integer in which to set the last-block bit
         * @return  arg with last-block bit set
         */
        static uint32_t setLastBlockBit(uint32_t i)  {return (i |= EV_LASTBLOCK_MASK);}


        /**
         * Clear the bit in the given arg to indicate it is NOT the last block.
         * @param i integer in which to clear the last-block bit
         * @return arg with last-block bit cleared
         */
        static uint32_t clearLastBlockBit(uint32_t i) {return (i &= ~EV_LASTBLOCK_MASK);}


        /**
         * Does this integer indicate that block has the first event
         * (assuming it's the header's sixth word)? Only makes sense if the
         * integer arg comes from the first block header of a file or buffer.
         *
         * @param i integer to examine.
         * @return <code>true</code> if this int indicates the block has a first event,
         *         else <code>false</code>
         */
        static bool hasFirstEvent(uint32_t i) {return ((i & EV_FIRSTEVENT_MASK) > 0);}


        /**
         * Set the bit in the given arg which indicates this block has a first event.
         * @param i integer in which to set the last-block bit
         * @return  arg with first event bit set
         */
        static uint32_t setFirstEventBit(uint32_t i)  {return (i |= EV_FIRSTEVENT_MASK);}


        /**
         * Clear the bit in the given arg to indicate this block does NOT have a first event.
         * @param i integer in which to clear the first event bit
         * @return arg with first event bit cleared
         */
        static uint32_t clearFirstEventBit(uint32_t i) {return (i &= ~EV_FIRSTEVENT_MASK);}


        //-//////////////////////////////////////////////////////////////////
        //  BitInfo methods
        //-//////////////////////////////////////////////////////////////////


        /** {@inheritDoc} */
        uint32_t getEventType() override {
            uint32_t type=0;
            bool bitisSet;
            for (uint32_t i=0; i < 4; i++) {
                bitisSet = bitInfo[i+2];
                if (bitisSet) type |= (uint32_t)1 << i;
            }
            return type;
        }


        /**
         * Encode the "is first event" into the bit info word
         * which will be in evio block header.
         * @param bSet bit set which will become part of the bit info word
         */
        static void setFirstEvent(std::bitset<24> & bSet) {
            // Encoding bit #15 (#6 since first is bit #9)
            bSet[6] = true;
        }


        /**
         * Encode the "is NOT first event" into the bit info word
         * which will be in evio block header.
         *
         * @param bSet bit set which will become part of the bit info word
         */
        static void unsetFirstEvent(std::bitset<24> & bSet) {
            // Encoding bit #15 (#6 since first is bit #9)
            bSet[6] = false;
        }


        /**
         * Gets a copy of all stored bit information.
         * @return copy of bitset containing all stored bit information.
         */
        std::bitset<24> getBitInfo() {return bitInfo;}


        /**
         * Gets the value of a particular bit in the bitInfo field.
         * @param bitIndex index of bit to get
         * @return BitSet containing all stored bit information.
         */
        bool getBitInfo(uint32_t bitIndex) {
            if (bitIndex > 23) {
                return false;
            }
            return bitInfo[bitIndex];
        }


        /**
         * Sets a particular bit in the bitInfo field.
         * @param bitIndex index of bit to change
         * @param value value to set bit to
         */
        void setBit(uint32_t bitIndex, bool value) {
            if (bitIndex > 23) {
                return;
            }
            bitInfo[bitIndex] = value;
        }


        /**
         * Sets the right bits in bit set (2-5 when starting at 0)
         * to hold 4 bits of the given type value. Useful when
         * generating a bitset for use with {@link EventWriter}
         * constructor.
         *
         * @param bSet Bitset containing all bits to be set
         * @param type event type as int
         */
        static void setEventType(std::bitset<24> & bSet, uint32_t type) {
            if (type < 0) type = 0;
            else if (type > 15) type = 15;

            for (uint32_t i=2; i < 6; i++) {
                bSet[i] = ((type >> (i-2)) & 0x1) > 0;
            }
        }


        /**
         * Calculates the sixth word of this header which has the version number
         * in the lowest 8 bits and the bit info in the highest 24 bits.
         *
         * @return sixth word of this header.
         */
        uint32_t getSixthWord() {
            uint32_t v = version & 0xff;

            for (uint32_t i=0; i < bitInfo.size(); i++) {
                if (bitInfo[i]) {
                    v |= (0x1 << (8+i));
                }
            }

            return v;
        }


        /**
         * Calculates the sixth word of this header which has the version number (4)
         * in the lowest 8 bits and the set in the upper 24 bits.
         *
         * @param set Bitset containing all bits to be set
         * @return generated sixth word of this header.
         */
        static uint32_t generateSixthWord(std::bitset<24> const & set) {
            uint32_t v = 4; // version

            for (uint32_t i=0; i < set.size(); i++) {
                if (i > 23) {
                    break;
                }
                if (set[i]) {
                    v |= (0x1 << (8+i));
                }
            }

            return v;
        }


        /**
         * Calculates the sixth word of this header which has the version number (4)
         * in the lowest 8 bits and the set in the upper 24 bits. The arg isDictionary
         * is set in the 9th bit and isEnd is set in the 10th bit.
         *
         * @param bSet Bitset containing all bits to be set
         * @param hasDictionary does this block include an evio xml dictionary as the first event?
         * @param isEnd is this the last block of a file or a buffer?
         * @return generated sixth word of this header.
         */
        static uint32_t generateSixthWord(std::bitset<24> const & bSet, bool hasDictionary, bool isEnd) {
            uint32_t v = 4; // version

            for (uint32_t i=0; i < bSet.size(); i++) {
                if (i > 23) {
                    break;
                }
                if (bSet[i]) {
                    v |= (0x1 << (8+i));
                }
            }

            v =  hasDictionary ? (v | 0x100) : v;
            v =  isEnd ? (v | 0x200) : v;

            return v;
        }


        /**
         * Calculates the sixth word of this header which has the version number
         * in the lowest 8 bits. The arg hasDictionary
         * is set in the 9th bit and isEnd is set in the 10th bit. Four bits of an int
         * (event type) are set in bits 11-14.
         *
         * @param version evio version number
         * @param hasDictionary does this block include an evio xml dictionary as the first event?
         * @param isEnd is this the last block of a file or a buffer?
         * @param eventType 4 bit type of events header is containing
         * @return generated sixth word of this header.
         */
        static uint32_t generateSixthWord(uint32_t version, bool hasDictionary,
                                          bool isEnd, uint32_t eventType) {
            uint32_t v = version;
            v =  hasDictionary ? (v | 0x100) : v;
            v =  isEnd ? (v | 0x200) : v;
            v |= ((eventType & 0xf) << 10);
            return v;
        }


        /**
          * Calculates the sixth word of this header which has the version number (4)
          * in the lowest 8 bits and the set in the upper 24 bits. The arg isDictionary
          * is set in the 9th bit and isEnd is set in the 10th bit. Four bits of an int
          * (event type) are set in bits 11-14.
          *
          * @param bSet Bitset containing all bits to be set
          * @param version evio version number
          * @param hasDictionary does this block include an evio xml dictionary as the first event?
          * @param isEnd is this the last block of a file or a buffer?
          * @param eventType 4 bit type of events header is containing
          * @return generated sixth word of this header.
          */
        static uint32_t generateSixthWord(std::bitset<24> bSet, uint32_t version,
                                          bool hasDictionary,
                                          bool isEnd, uint32_t eventType) {
            uint32_t v = version; // version

            for (int i=0; i < bSet.size(); i++) {
                if (i > 23) {
                    break;
                }
                if (bSet[i]) {
                    v |= (0x1 << (8+i));
                }
            }

            v =  hasDictionary ? (v | 0x100) : v;
            v =  isEnd ? (v | 0x200) : v;
            v |= ((eventType & 0xf) << 10);

            return v;
        }


        /**
         * Parses the argument into the bit info fields.
         * This ignores the version in the lowest 8 bits.
         * @param word integer to parse into bit info fields
         */
        void parseToBitInfo(uint32_t word) {
            for (int i=0; i < bitInfo.size(); i++) {
                bitInfo[i] = ((word >> (8+i)) & 0x1) > 0;
            }
        }


        //-//////////////////////////////////////////////////////////////////


        /** {@inheritDoc} */
        uint32_t getSourceId() override {return reserved1;}


        /**
         * Get the first reserved word.
         * @return the first reserved word.
         */
        uint32_t getReserved1() const {return reserved1;}


        /**
         * Sets the value of reserved1.
         * @param r1 the value for reserved1.
         */
        void setReserved1(uint32_t r1) {reserved1 = r1;}


        /**
          * Get the 2nd reserved word.
          * @return the 2nd reserved word.
          */
        uint32_t getReserved2() const {return reserved2;}


        /**
         * Sets the value of reserved2.
         * @param r2 the value for reserved2.
         */
        void setReserved2(uint32_t r2) {reserved2 = r2;}


        /** {@inheritDoc} */
        uint32_t getMagicNumber() override {return magicNumber;}


        /**
         * Sets the value of magicNumber. This should match the constant {@link #MAGIC_NUMBER}.
         * If it doesn't, some obvious possibilities: <br>
         * 1) The evio data (perhaps from a file) is screwed up.<br>
         * 2) The reading algorithm is screwed up. <br>
         * 3) The endianness is not being handled properly.
         *
         * @param magicNum the new value for magic number.
         * @throws EvioException if magic number not the correct value.
         */
        void setMagicNumber(uint32_t magicNum) {
            if (magicNum != MAGIC_NUMBER) {
                throw EvioException("Value for magicNumber, " + std::to_string(magicNum) +
                                    " does not match MAGIC_NUMBER 0xc0da0100");
            }
            magicNumber = MAGIC_NUMBER;
        }


        /** {@inheritDoc} */
        ByteOrder & getByteOrder() override {return byteOrder;}


        /**
        * Sets the byte order of data being read.
        * @param order the new value for data's byte order.
        */
        void setByteOrder(ByteOrder & order) {byteOrder = order;}


        /** {@inheritDoc} */
        std::string toString() override {
            std::stringstream ss;

            ss << "block size:    " << size << std::endl;
            ss << "number:        " << number << std::endl;
            ss << "headerLen:     " << headerLength << std::endl;
            ss << "event count:   " << eventCount << std::endl;
            ss << "reserved1:     " << reserved1 << std::endl;
            ss << "bitInfo  bits: " << bitInfo.to_string() << std::endl;

            ss << "bitInfo/ver:   " << getSixthWord() << std::endl;
            ss << "has dict:      " << hasDictionary() << std::endl;
            ss << "is last blk:   " << isLastBlock() << std::endl;

            ss << "version:       " << version << std::endl;
            ss << "magicNumber:   " << magicNumber << std::endl;
            ss << "  *buffer start: " << getBufferStartingPosition() << std::endl;
            ss << "  *next   start: " << nextBufferStartingPosition() << std::endl;

            return ss.str();
        }


        /** {@inheritDoc} */
        size_t getBufferEndingPosition() override {return bufferStartingPosition + 4*size;}


        /** {@inheritDoc} */
        size_t getBufferStartingPosition() override {return bufferStartingPosition;}


        /** {@inheritDoc} */
        void setBufferStartingPosition(size_t pos) override {bufferStartingPosition = pos;}


        /** {@inheritDoc} */
        size_t nextBufferStartingPosition() override {return getBufferEndingPosition();}


        /** {@inheritDoc} */
        size_t firstEventStartingPosition() override {return bufferStartingPosition + 4*headerLength;}


        /** {@inheritDoc} */
        size_t bytesRemaining(size_t position) override {
            if (position < bufferStartingPosition) {
                throw EvioException("Provided position is less than buffer starting position.");
            }

            size_t nextBufferStart = nextBufferStartingPosition();
            if (position > nextBufferStart) {
                throw EvioException("Provided position beyond buffer end position.");
            }

            return nextBufferStart - position;
        }


        /** {@inheritDoc} */
        size_t write(ByteBuffer & byteBuffer) override {
            if (byteBuffer.remaining() < 32) {
                throw std::overflow_error("not enough room in buffer to write");
            }
            byteBuffer.putInt(size);
            byteBuffer.putInt(number);
            byteBuffer.putInt(headerLength); // should always be 8
            byteBuffer.putInt(eventCount);
            byteBuffer.putInt(0);       // unused
            byteBuffer.putInt(getSixthWord());
            byteBuffer.putInt(0);       // unused
            byteBuffer.putInt(magicNumber);
            return 32;
        }

    };


}

#endif //EVIO_6_0_BLOCKHEADERV4_H
