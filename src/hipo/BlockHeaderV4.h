/**
 * Copyright (c) 2020, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * EPSCI Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 06/10/2020
 * @author timmer
 */

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
     * <pre>
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
     *
     *      Block Length       = number of ints in block (including this one).
     *      Block Number       = id number (starting at 1)
     *      Header Length      = number of ints in this header (8)
     *      Event Count        = number of events in this block (always an integral #).
     *                           NOTE: this value should not be used to parse the following
     *                           events since the first block may have a dictionary whose
     *                           presence is not included in this count.
     *      Reserved 1         = If bits 11-14 in bit info are RocRaw (1), then (in the first block)
     *                           this contains the CODA id of the source
     *      Bit info &amp; Version = Lowest 8 bits are the version number (4).
     *                           Upper 24 bits contain bit info.
     *                           If a dictionary is included as the first event, bit #9 is set (=1)
     *                           If a last block, bit #10 is set (=1)
     *      Reserved 2         = unused
     *      Magic Int          = magic number (0xc0da0100) used to check endianness
     *
     *
     *
     * Bit info has the following bits defined (bit #s start with 1):
     *   Bit  9     = true if dictionary is included (relevant for first block only)
     *
     *   Bit  10    = true if this block is the last block in file or network transmission
     *
     *   Bits 11-14 = type of events following (ROC Raw = 0, Physics = 1, PartialPhysics = 2,
     *                DisentangledPhysics = 3, User = 4, Control = 5, Other = 15).
     *
     *   Bit 15     = true if next (non-dictionary) event in this block is a "first event" to
     *                be placed at the beginning of each written file and its splits.
     *
     *                Bits 11-15 are useful ONLY for the CODA online use of evio.
     *                That's because only a single CODA event type is placed into
     *                a single (ET, cMsg) buffer, and each user or control event has its own
     *                buffer as well. That buffer then is parsed by an EvioReader or
     *                EvioCompactReader object. Thus all events will be of a single CODA type.
     *
     *
     *
     * </pre>
     *
     *
     * @author heddle
     * @author timmer
     *
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
        bitset<24> bitInfo;

        /** This is the magic word, 0xc0da0100, used to check endianness. */
        uint32_t magicNumber = MAGIC_NUMBER;

        /** This is the byte order of the data being read. */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LITTLE};

        /**
         * This is not part of the block header proper. It is a position in a memory buffer of the start of the block
         * (physical record). It is kept for convenience.
         */
// TODO: This was sset to -1. Check it out.
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
         *<li><code>magicNumber</code> is initialized to <code>MAGIC_NUMBER</code><br>
         *</ul>
         * @param size the size of the block in ints.
         * @param number the block number--usually sequential.
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

        /**
        * Get the size of the block (physical record).
        * @return the size of the block (physical record) in ints.
        */
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


        /**
          * Get the block number for this block (physical record).
          * In a file, this is usually sequential, starting at 1.
          * @return the block number for this block (physical record).
          */
        uint32_t getNumber() override {return number;}

        /**
         * Set the block number for this block (physical record).
         * In a file, this is usually sequential, starting at 1.
         * This is not checked.
         * @param number the number of the block (physical record).
         */
        void setNumber(uint32_t num) {number = num;}

        /**
         * Get the block header length in ints. This should be 8.
         * @return block header length in ints.
         */
        uint32_t getHeaderLength() const {return headerLength;}

        /**
         * Get the block header length, in 32 bit words (ints). This should be 8.
         * This is needed to implement IBlockHeader interface. The {@link #getHeaderLength()}
         * method cannot be used since the new hipo code uses that method to return a
         * length in bytes.
         * @return block header length.
         */
        uint32_t getHeaderWords() override {return headerLength;}

        /**
         * Set the block header length, in ints. Although technically speaking this value
         * is variable, it should be 8. However, since this is usually read as part of reading
         * the physical record header, it is a good check to have a setter rather than just fix its value at 8.
         *
         * @param headerLength the new block header length. This should be 8.
         */
        void setHeaderLength(uint32_t len) {
            if (len != HEADER_SIZE) {
                std::cout << "Warning: Block Header Length = " << headerLength << std::endl;
            }
            headerLength = len;
        }

        /**
         * Get the evio version of the block (physical record) header.
         * @return the evio version of the block (physical record) header.
         */
        uint32_t getVersion() override {return version;}

        /**
         * Sets the evio version. Should be 4 but no check is performed here.
         * @param ver the evio version of evio.
         */
        void setVersion(uint32_t ver) {version = ver;}

        /**
        * Does this block/record contain the "first event"
        * (first event to be written to each file split)?
        * @return <code>true</code> if this contains the first event, else <code>false</code>
        */
        bool hasFirstEvent() override {return bitInfo[6];}

        /**
         * Does this integer indicate that there is an evio dictionary
         * (assuming it's the header's sixth word)?
         * @param i integer to examine.
         * @return <code>true</code> if this int indicates an evio dictionary, else <code>false</code>
         */
        static bool hasDictionary(uint32_t i) {return ((i & EV_DICTIONARY_MASK) > 0);}

        /**
         * Is this block's first event an evio dictionary?
         * @return <code>true</code> if this block's first event is an evio dictionary, else <code>false</code>
         */
        bool hasDictionary() override {return bitInfo[0];}

        /**
         * Is this the last block in the file/buffer or being sent over the network?
         * @return <code>true</code> if this is the last block in the file or being sent
         *         over the network, else <code>false</code>
         */
        bool isLastBlock() override {return bitInfo[1];}

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

        /**
         * Get the value of bits 2-5. It represents the type of event being sent.
         * @return bits 2-5 as an integer, representing event type.
         */
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
        static void setFirstEvent(bitset<24> & bSet) {
            // Encoding bit #15 (#6 since first is bit #9)
            bSet[6] = true;
        }

        /**
         * Encode the "is NOT first event" into the bit info word
         * which will be in evio block header.
         *
         * @param bSet bit set which will become part of the bit info word
         */
        static void unsetFirstEvent(bitset<24> & bSet) {
            // Encoding bit #15 (#6 since first is bit #9)
            bSet[6] = false;
        }

        /**
         * Gets a copy of all stored bit information.
         * @return copy of bitset containing all stored bit information.
         */
        bitset<24> getBitInfo() {return bitInfo;}

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
        static void setEventType(bitset<24> & bSet, uint32_t type) {
            if (type < 0) type = 0;
            else if (type > 15) type = 15;

            for (uint32_t i=2; i < 6; i++) {
                bSet[i] = ((type >> i-2) & 0x1) > 0;
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
        static uint32_t generateSixthWord(bitset<24> const & set) {
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
        static uint32_t generateSixthWord(bitset<24> const & bSet, bool hasDictionary, bool isEnd) {
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
        static uint32_t generateSixthWord(bitset<24> bSet, uint32_t version,
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
                bitInfo[i] = ((word >> 8+i) & 0x1) > 0;
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
         * @param r1 the value for reserved2.
         */
        void setReserved2(uint32_t r2) {reserved2 = r2;}


        /** {@inheritDoc} */
        uint32_t getMagicNumber() override {return magicNumber;}

        /**
         * Sets the value of magicNumber. This should match the constant MAGIC_NUMBER.
         * If it doesn't, some obvious possibilities: <br>
         * 1) The evio data (perhaps from a file) is screwed up.<br>
         * 2) The reading algorithm is screwed up. <br>
         * 3) The endianness is not being handled properly.
         *
         * @param magicNumber the new value for magic number.
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

        /**
         * Obtain a string representation of the block (physical record) header.
         * @return a string representation of the block (physical record) header.
         */
        string toString() override {
            stringstream ss;

            ss << "block size:    " << size << endl;
            ss << "number:        " << number << endl;
            ss << "headerLen:     " << headerLength << endl;
            ss << "event count:   " << eventCount << endl;
            ss << "reserved1:     " << reserved1 << endl;
            ss << "bitInfo  bits: " << bitInfo.to_string() << endl;

            ss << "bitInfo/ver:   " << getSixthWord() << endl;
            ss << "has dict:      " << hasDictionary() << endl;
            ss << "is last blk:   " << isLastBlock() << endl;

            ss << "version:       " << version << endl;
            ss << "magicNumber:   " << magicNumber << endl;
            ss << "  *buffer start: " << getBufferStartingPosition() << endl;
            ss << "  *next   start: " << nextBufferStartingPosition() << endl;

            return ss.str();
        }


        /**
         * Get the position in the buffer (in bytes) of this block's last data word.<br>
         * @return the position in the buffer (in bytes) of this block's last data word.
         */
        uint64_t getBufferEndingPosition() override {return bufferStartingPosition + 4*size;}

        /**
         * Get the starting position in the buffer (in bytes) from which this header was read--if that happened.<br>
         * This is not part of the block header proper. It is a position in a memory buffer of the start of the block
         * (physical record). It is kept for convenience. It is up to the reader to set it.
         *
         * @return the starting position in the buffer (in bytes) from which this header was read--if that happened.
         */
        uint64_t getBufferStartingPosition() override {return bufferStartingPosition;}

        /**
         * Set the starting position in the buffer (in bytes) from which this header was read--if that happened.<br>
         * This is not part of the block header proper. It is a position in a memory buffer of the start of the block
         * (physical record). It is kept for convenience. It is up to the reader to set it.
         *
         * @param pos the starting position in the buffer from which this header was read--if that happened.
         */
        void setBufferStartingPosition(uint64_t pos) override {bufferStartingPosition = pos;}

        /**
         * Determines where the start of the next block (physical record) header in some buffer is located (in bytes).
         * This assumes the start position has been maintained by the object performing the buffer read.
         *
         * @return the start of the next block (physical record) header in some buffer is located (in bytes).
         */
        uint64_t nextBufferStartingPosition() override {return getBufferEndingPosition();}

        /**
         * Determines where the start of the first event (logical record) in this block (physical record) is located
         * (in bytes). This assumes the start position has been maintained by the object performing the buffer read.
         *
         * @return where the start of the first event (logical record) in this block (physical record) is located
         *         (in bytes). Returns 0 if start is 0, signaling that this entire physical record is part of a
         *         logical record that spans at least three physical records.
         */
        uint64_t firstEventStartingPosition() override {return bufferStartingPosition + 4*headerLength;}

        /**
         * Gives the bytes remaining in this block (physical record) given a buffer position. The position is an absolute
         * position in a byte buffer. This assumes that the absolute position in <code>bufferStartingPosition</code> is
         * being maintained properly by the reader. No block is longer than 2.1GB - 31 bits of length. This is for
         * practical reasons - so a block can be read into a single byte array.
         *
         * @param position the absolute current position is a byte buffer.
         * @return the number of bytes remaining in this block (physical record.)
         * @throws EvioException if position &lt; buffer starting position or &gt; buffer end position
         */
        uint32_t bytesRemaining(uint64_t position) override {
            if (position < bufferStartingPosition) {
                throw EvioException("Provided position is less than buffer starting position.");
            }

            uint64_t nextBufferStart = nextBufferStartingPosition();
            if (position > nextBufferStart) {
                throw EvioException("Provided position beyond buffer end position.");
            }

            return (uint32_t)(nextBufferStart - position);
        }

        /**
         * Write myself out a byte buffer. This write is relative--i.e., it uses the current position of the buffer.
         *
         * @param byteBuffer the byteBuffer to write to.
         * @return the number of bytes written, which for a BlockHeader is 32.
         * @throws overflow_error if insufficient room to write header into buffer.
         */
        size_t write(ByteBuffer & byteBuffer) override {
            if (byteBuffer.remaining() < 32) {
                throw overflow_error("not enough room in buffer to write");
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
