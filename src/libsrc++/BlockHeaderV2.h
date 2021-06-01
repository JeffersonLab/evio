//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_BLOCKHEADERV2_H
#define EVIO_6_0_BLOCKHEADERV2_H


#include <string>
#include <stdexcept>


#include "ByteOrder.h"
#include "IBlockHeader.h"


namespace evio {

    /**
     * This holds an evio block header, also known as a physical record header.
     * Unfortunately, in versions 1, 2 &amp; 3, evio files impose an anachronistic
     * block structure. The complication that arises is that logical records
     * (events) will sometimes cross physical record boundaries.
     *
     *
     * <pre><code>
     * ####################################
     * Evio block header, versions 1,2 &amp; 3:
     * ####################################
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
     * |               Start                 |
     * |_____________________________________|
     * |                End                  |
     * |_____________________________________|
     * |              Version                |
     * |_____________________________________|
     * |             Reserved 1              |
     * |_____________________________________|
     * |            Magic Number             |
     * |_____________________________________|
     *
     *
     *      Block Length  = number of ints in block (including this one).
     *                      This is fixed for versions 1-3, generally at 8192 (32768 bytes)
     *      Block Number  = id number (starting at 0)
     *      Header Length = number of ints in this header (always 8)
     *      Start         = offset to first event header in block relative to start of block
     *      End           = # of valid words (header + data) in block (normally = block size)
     *      Version       = evio format version
     *      Reserved 1    = reserved
     *      Magic #       = magic number (0xc0da0100) used to check endianness
     *
     * </code></pre>
     *
     *
     * @author heddle (original java version)
     * @author timmer
     */
    class BlockHeaderV2 : public IBlockHeader {

    public:

        /**
          * The maximum block size in 32 bit ints in this implementation of evio.
          * There is, in actuality, no limit on size; however, the versions 1-3 C
          * library only used 8192 as the block size.
          */
        static const uint32_t MAX_BLOCK_SIZE = 32768;

    private:

        /** The block (physical record) size in 32 bit ints. */
        uint32_t size = 0;

        /** The block number. In a file, this is usually sequential.  */
        uint32_t number = 1;

        /** The block header length. Should be 8 in all cases, so getting this correct constitutes a check. */
        uint32_t headerLength = 8;

        /**
         * Offset (in ints, relative to start of block) to the start of the first event (logical record) that begins in this
         * block. For the first event it will just be = 8, the size of the block header. For subsequent physical records it
         * will generally not be 8. Note that a logical record (event) that spans three blocks (physical records) will have
         * <code>start = 0</code>.
         */
        uint32_t start = 8;

        /**
         * The number of valid words (header + data) in the block (physical record.) This is normally the same as the block
         * size, except for the last block (physical record) in the file. <br>
         * NOTE: for evio files, even if end < size (blocksize) for the last block (physical record), the data behind it
         * will be padded with zeroes so that the file size is an integer multiple of the block size.
         */
        uint32_t end = 0;

        /** The evio version, always 2.  */
        uint32_t version = 2;

        /**
         * First reserved word. Sometimes this is used to indicate the ordinal number of the last event that starts
         * within this block--but that is not mandated. In that case, if the previous block had a value of
         * reserved1 = 6 and this block has a value of 9, then this block contains the end of event 6, all of events
         * 7 and 8, and the start of event 9--unless it ends exactly on the end of event 8.<br>
         */
        uint32_t reserved1 = 0;

        /** This is the magic word: 0xc0da0100 (formerly reserved2). Used to check endianness.  */
        uint32_t magicNumber = MAGIC_NUMBER;

        /** This is the byte order of the data being read. */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LOCAL};

        /**
         * This is not part of the block header proper. It is a position in a memory buffer of the start of the block
         * (physical record). It is kept for convenience.
         */
        uint64_t bufferStartingPosition = 0L;

    public:

        /** Constructor initializes all fields to default values. */
        BlockHeaderV2() = default;


        /**
         * Creates a BlockHeader for evio versions 1-3 format. Only the <code>block size</code>
         * and <code>block number</code> are provided. The other six words, which can be
         * modified by setters, are initialized to these values:<br>
         *<ul>
         *<li><code>headerLength</code> is initialized to 8<br>
         *<li><code>start</code> is initialized to 8<br>
         *<li><code>end</code> is initialized to <code>size</code><br>
         *<li><code>version</code> is initialized to 2<br>
         *<li><code>reserved1</code> is initialized to 0<br>
         *<li><code>magicNumber</code> is initialized to <code>{@link #MAGIC_NUMBER}</code><br>
         *</ul>
         * @param sz the size of the block in ints.
         * @param num the block number--usually sequential.
         */
        BlockHeaderV2(uint32_t sz, uint32_t num) {
            size = sz;
            number = num;
            end = size;
        }


        /**
        * This copy constructor creates an evio version 1-3 BlockHeader
        * from another object of this class.
        * @param blkHeader block header object to copy
        */
        explicit BlockHeaderV2(std::shared_ptr<BlockHeaderV2> & blkHeader) {
            copy(blkHeader);
        }


        /**
        * This method copies another header's contents.
        * @param blkHeader block header object to copy
        */
        void copy(std::shared_ptr<BlockHeaderV2> & blkHeader) {
            size         = blkHeader->size;
            number       = blkHeader->number;
            headerLength = blkHeader->headerLength;
            version      = blkHeader->version;
            end          = blkHeader->end;
            start        = blkHeader->start;
            reserved1    = blkHeader->reserved1;
            byteOrder    = blkHeader->byteOrder;
            magicNumber  = blkHeader->magicNumber;
            bufferStartingPosition = blkHeader->bufferStartingPosition;
        }


        /** {@inheritDoc} */
        bool hasDictionary() override {return false;}


        /** {@inheritDoc} */
        bool isLastBlock() override {return false;}


        /** {@inheritDoc} */
        bool isCompressed() override {return false;}


        /** {@inheritDoc} */
        Compressor::CompressionType getCompressionType() override {return Compressor::UNCOMPRESSED;}


        /** {@inheritDoc} */
        uint32_t getSize() override {return size;}


        /**
         * Set the size of the block (physical record). Some trivial checking is done.
         * @param sz the new value for the size, in ints.
         * @throws EvioException if size &lt; 8 or &gt; {@link #MAX_BLOCK_SIZE}.
         */
        void setSize(uint32_t sz) {
            if ((sz < 8) || (sz > MAX_BLOCK_SIZE)) {
                throw EvioException("Bad value for size in block (physical record) header: " + std::to_string(sz));
            }
            size = sz;
        }


        /**
         * Get the starting position of the block (physical record.). This is the offset (in ints, relative to start of
         * block) to the start of the first event (logical record) that begins in this block. For the first event it will
         * just be = 8, the size of the block header. For subsequent blocks it will generally not be 8. Note that a
         * an event that spans three blocks (physical records) will have <code>start = 0</code>.
         *
         * NOTE: a logical record (event) that spans three blocks (physical records) will have <code>start = 0</code>.
         *
         * @return the starting position of the block (physical record.)
         */
        uint32_t getStart() const {return start;}


        /**
         * Set the starting position of the block (physical record.). This is the offset (in ints, relative to start of
         * block) to the start of the first event (logical record) that begins in this block. For the first event it will
         * just be = 8, the size of the block header. For subsequent blocks it will generally not be 8. Some trivial
         * checking is done. Note that an event that spans three blocks (physical records) will have
         * <code>start = 0</code>.
         *
         * NOTE: a logical record (event) that spans three blocks (physical records) will have <code>start = 0</code>.
         *
         * @param strt the new value for the start.
         * @throws EvioException if start &lt; 8 or &gt; {@link #MAX_BLOCK_SIZE}.
         */
        void setStart(uint32_t strt) {
            if ((strt < 0) || (strt > MAX_BLOCK_SIZE)) {
                throw EvioException("Bad value for start in block (physical record) header: " + std::to_string(strt));
            }
            start = strt;
        }


        /**
         * Get the ending position of the block (physical record.) This is the number of valid words (header + data) in the
         * block (physical record.) This is normally the same as the block size, except for the last block (physical record)
         * in the file.<br>
         * NOTE: for evio files, even if end &lt; size (blocksize) for the last block (physical record), the data behind it
         * will be padded with zeroes so that the file size is an integer multiple of the block size.
         *
         * @return the ending position of the block (physical record.)
         */
        uint32_t getEnd() const {return end;}


        /**
         * Set the ending position of the block (physical record.) This is the number of valid words (header + data) in the
         * block (physical record.) This is normally the same as the block size, except for the last block (physical record)
         * in the file. Some trivial checking is done.<br>
         * NOTE: for evio files, even if end &lt; size (blocksize) for the last block (physical record), the data behind it
         * will be padded with zeroes so that the file size is an integer multiple of the block size.
         *
         * @param endd the new value for the end.
         * @throws EvioException if end &lt; 8 or &gt; {@link #MAX_BLOCK_SIZE}.
         */
        void setEnd(uint32_t endd) {
            if ((endd < 8) || (endd > MAX_BLOCK_SIZE)) {
                throw EvioException("Bad value for end in block (physical record) header: " + std::to_string(endd));
            }
            end = endd;
        }


        /** {@inheritDoc} */
        uint32_t getNumber() override {return number;}


        /**
         * Set the block number for this block (physical record). In a file, this is usually sequential. This is not
         * checked.
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
         * Set the block header length, in ints. This should be 8. However, since this is usually read as part of reading
         * the physical record header, it is a good check to have a setter rather than just fix its value at 8.
         *
         * @param len the new block header length. This should be 8.
         * @throws EvioException if headerLength is not 8.
         */
        void setHeaderLength(uint32_t len) {
            if (len != 8) {
                throw EvioException("Bad Block (Physical Record) Header Length: " + std::to_string(len));
            }
            headerLength = len;
        }


        /** {@inheritDoc} */
        bool hasFirstEvent() override {return false;}


        /** {@inheritDoc} */
        uint32_t getEventType() override {return 0;}


        /** {@inheritDoc} */
        uint32_t getVersion() override {return version;}


        /**
         * Sets the evio version. Should be 1, 2 or 3 but no check is performed here.
         * @param ver the evio version of evio.
         */
        void setVersion(uint32_t ver) {version = ver;}


        /** {@inheritDoc} */
        uint32_t getSourceId() override {return reserved1;}


        /**
         * Get the first reserved word in the block (physical record) header. Used in evio versions 1-3 only.
         * @return the first reserved word in the block (physical record). Used in evio versions 1-3 only.
         */
        uint32_t getReserved1() const {return reserved1;}


        /**
         * Sets the value of reserved1.
         * @param r1 the value for reserved1.
         */
        void setReserved1(uint32_t r1) {reserved1 = r1;}


        /** {@inheritDoc} */
        uint32_t getMagicNumber() override {return magicNumber;}


        /**
         * Sets the value of magicNumber. This should match the constant {@link #MAGIC_NUMBER} ...
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
            ss << "start:         " << start << std::endl;
            ss << "end:           " << end << std::endl;
            ss << "version:       " << version << std::endl;
            ss << "reserved1:     " << reserved1 << std::endl;
            ss << "magicNumber:   " << magicNumber << std::endl;
            ss << "  *buffer start: " << getBufferStartingPosition() << std::endl;
            ss << "  *next   start: " << nextBufferStartingPosition() << std::endl;

            return ss.str();
        }


        /** {@inheritDoc} */
        size_t getBufferEndingPosition() override {return bufferStartingPosition + 4*end;}


        /** {@inheritDoc} */
        size_t getBufferStartingPosition() override {return bufferStartingPosition;}


        /** {@inheritDoc} */
        void setBufferStartingPosition(size_t pos) override {bufferStartingPosition = pos;}


        /** {@inheritDoc} */
        size_t nextBufferStartingPosition() override {return bufferStartingPosition + 4*size;}


        /** {@inheritDoc} */
        size_t firstEventStartingPosition() override {
            if (start == 0) {
                return 0UL;
            }
            return bufferStartingPosition + 4*start;
        }


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
            byteBuffer.putInt(start);
            byteBuffer.putInt(end);
            byteBuffer.putInt(version);
            byteBuffer.putInt(reserved1);
            byteBuffer.putInt(magicNumber);
            return 32;
        }
    };


}

#endif //EVIO_6_0_BLOCKHEADERV2_H
