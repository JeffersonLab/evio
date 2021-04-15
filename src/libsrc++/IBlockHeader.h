//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_IBLOCKHEADER_H
#define EVIO_6_0_IBLOCKHEADER_H


#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>


#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "Compressor.h"


namespace evio {


    /**
     * Make a common interface for different versions of the BlockHeader arising from
     * different evio versions. In evio version 4 and later, blocks are called records.
     *
     * @date 06/5/2020
     * @author timmer
     */
    class IBlockHeader {

    public:

        /** The magic number, should be the value of <code>magicNumber</code>. */
        static const uint32_t MAGIC_NUMBER = 0xc0da0100;

        /** Byte offset from beginning of header to the magic number. */
        static const uint32_t   MAGIC_OFFSET = 28;

        /** Byte offset from beginning of header to bit info word. */
        static const uint32_t   BIT_INFO_OFFSET = 20;

        /** Mask to get version number from bitinfo word in header. */
        static const uint32_t VERSION_MASK = 0xff;

        /**
         * Get the size of the block (record) in 32 bit words.
         * @return size of the block (record) in 32 bit words.
         */
        virtual uint32_t getSize() = 0;

        /**
         * Get the block number for this block (record).
         * In a file, this is usually sequential, starting at 1.
         * @return the block number for this block (record).
         */
        virtual uint32_t getNumber() = 0;

        /**
         * Get the block (record) header length, in 32 bit words.
         * @return block (record) header length, in 32 bit words.
         */
        virtual uint32_t getHeaderWords() = 0;

        /**
         * Get the source ID number if in CODA online context and data is coming from ROC.
         * @return source ID number if in CODA online context and data is coming from ROC.
         */
        virtual uint32_t getSourceId() = 0;

        /**
         * Does this block/record contain the "first event"
         * (first event to be written to each file split)?
         * @return <code>true</code> if this record has the first event, else <code>false</code>.
         *         Evio versions 1-3 always return false.
         */
        virtual bool hasFirstEvent() = 0;

        /**
         * Get the type of events in block/record (see values of {@link DataType}.
         * This is not supported by versions 1-3 which returns 0.
         * @return type of events in block/record, or 0 if evio version 1-3.
         */
        virtual uint32_t getEventType() = 0;

        /**
         * Get the evio version of the block (record) header.
         * @return evio version of the block (record) header.
         */
        virtual uint32_t getVersion() = 0;

        /**
         * Get the magic number the block (record) header which should be 0xc0da0100.
         * @return magic number in the block (record).
         */
        virtual uint32_t getMagicNumber() = 0;

        /**
         * Get the byte order of the data being read.
         * @return byte order of the data being read.
         */
        virtual ByteOrder & getByteOrder() = 0;

        /**
         * Get the position in the buffer (bytes) of this block's last data word.<br>
         * @return position in the buffer (bytes) of this block's last data word.
         */
        virtual size_t getBufferEndingPosition() = 0;

        /**
         * Get the starting position in the buffer (bytes) from which this header was read--if that happened.<br>
         * This is not part of the block header proper. It is a position in a memory buffer of the start of the block
         * (record). It is kept for convenience. It is up to the reader to set it.
         *
         * @return starting position in buffer (bytes) from which this header was read--if that happened.
         */
        virtual size_t getBufferStartingPosition() = 0;

        /**
         * Set the starting position in the buffer (bytes) from which this header was read--if that happened.<br>
         * This is not part of the block header proper. It is a position in a memory buffer of the start of the block
         * (record). It is kept for convenience. It is up to the reader to set it.
         *
         * @param pos starting position in buffer from which this header was read--if that happened.
         */
        virtual void setBufferStartingPosition(size_t pos) = 0;

        /**
         * Determines where the start of the next block (record) header in some buffer is located (bytes).
         * This assumes the start position has been maintained by the object performing the buffer read.
         *
         * @return the start of the next block (record) header in some buffer is located (bytes).
         */
        virtual size_t nextBufferStartingPosition() = 0;

        /**
         * Determines where the start of the first event in this block (record) is located
         * (bytes). This assumes the start position has been maintained by the object performing the buffer read.
         *
         * @return where the start of the first event in this block (record) is located
         *         (bytes). In evio format version 2, returns 0 if start is 0, signaling
         *         that this entire record is part of a logical record that spans at least
         *         three physical records.
         */
        virtual size_t firstEventStartingPosition() = 0;

        /**
         * Gives the bytes remaining in this block (record) given a buffer position. The position is an absolute
         * position in a byte buffer. This assumes that the absolute position in <code>bufferStartingPosition</code> is
         * being maintained properly by the reader.
         *
         * @param position the absolute current position in a byte buffer.
         * @return the number of bytes remaining in this block (record).
         * @throws EvioException if position out of bounds
         */
        virtual size_t bytesRemaining(size_t position) = 0;

        /**
         * Does this block contain an evio dictionary?
         * @return <code>true</code> if this block contains an evio dictionary, else <code>false</code>.
         *         Always returns false for versions 1-3 (not implemented).
         */
        virtual bool hasDictionary() = 0;

        /**
         * Is this the last block in the file or being sent over the network?
         * @return <code>true</code> if this is the last block in the file or being sent
         *         over the network, else <code>false</code>. Always returns false for
         *         versions 1-3 (not implemented).
         */
        virtual bool isLastBlock() = 0;

        /**
         * Is this the data in this block compressed?
         * @return <code>true</code> if the data in this block is compressed, else <code>false</code>.
         */
        virtual bool isCompressed() = 0;

        /**
         * Get the type of data compression used.
         * @return type of data compression used.
         */
        virtual Compressor::CompressionType getCompressionType() = 0;

        /**
         * Write myself out into a byte buffer. This write is relative--i.e.,
         * it uses the current position of the buffer.
         * @param byteBuffer the byteBuffer to write to.
         * @return the number of bytes written.
         * @throws overflow_error if insufficient room to write header into buffer.
         */
        virtual size_t write(ByteBuffer & byteBuffer) = 0;

        /**
         * Get the string representation of the block (record) header.
         * @return string representation of the block (record) header.
         */
        virtual std::string toString() = 0;
    };



}

#endif //EVIO_6_0_IBLOCKHEADER_H
