/**
 * Copyright (c) 2020, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 06/5/2020
 * @author timmer
 */

#ifndef EVIO_6_0_IBLOCKHEADER_H
#define EVIO_6_0_IBLOCKHEADER_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "ByteOrder.h"
#include "ByteBuffer.h"


namespace evio {


        /**
         * Make a common interface for different versions of the BlockHeader arising from
         * different evio versions. In evio version 4 and later, blocks are called records.
         * @author timmer
         */
        class IBlockHeader {

        public:

            /** The magic number, should be the value of <code>magicNumber</code>. */
            static const uint32_t MAGIC_NUMBER = 0xc0da0100;

            /**
             * Get the size of the block (record) in 32 bit words.
             * @return size of the block (record) in 32 bit words.
             */
            virtual uint32_t getSize() = 0;

            /**
             * Get the block number for this block (record).
             * In a file, this is usually sequential.
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
             * @return <code>true</code> if this record has the first event, else <code>false</code>
             */
            virtual bool hasFirstEvent() = 0;

            /**
             * Get the type of events in block/record (see values of {@link DataType}.
             * @return type of events in block/record.
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
            virtual uint64_t getBufferEndingPosition() = 0;

            /**
             * Get the starting position in the buffer (bytes) from which this header was read--if that happened.<br>
             * This is not part of the block header proper. It is a position in a memory buffer of the start of the block
             * (record). It is kept for convenience. It is up to the reader to set it.
             *
             * @return starting position in buffer (bytes) from which this header was read--if that happened.
             */
            virtual uint64_t getBufferStartingPosition() = 0;

            /**
             * Set the starting position in the buffer (bytes) from which this header was read--if that happened.<br>
             * This is not part of the block header proper. It is a position in a memory buffer of the start of the block
             * (record). It is kept for convenience. It is up to the reader to set it.
             *
             * @param bufferStartingPosition starting position in buffer from which this header was read--if that
             *            happened.
             */
            virtual void setBufferStartingPosition(uint64_t bufferStartingPosition) = 0;

            /**
             * Determines where the start of the next block (record) header in some buffer is located (bytes).
             * This assumes the start position has been maintained by the object performing the buffer read.
             *
             * @return the start of the next block (record) header in some buffer is located (bytes).
             */
            virtual uint64_t nextBufferStartingPosition() = 0;

            /**
             * Determines where the start of the first event in this block (record) is located
             * (bytes). This assumes the start position has been maintained by the object performing the buffer read.
             *
             * @return where the start of the first event in this block (record) is located
             *         (bytes). In evio format version 2, returns 0 if start is 0, signaling
             *         that this entire record is part of a logical record that spans at least
             *         three physical records.
             */
            virtual uint64_t firstEventStartingPosition() = 0;

            /**
             * Gives the bytes remaining in this block (record) given a buffer position. The position is an absolute
             * position in a byte buffer. This assumes that the absolute position in <code>bufferStartingPosition</code> is
             * being maintained properly by the reader. No block is longer than 2.1GB - 31 bits of length. This is for
             * practical reasons - so a block can be read into a single byte array.
             *
             * @param position the absolute current position in a byte buffer.
             * @return the number of bytes remaining in this block (record).
             * @throws EvioException if position out of bounds
             */
            virtual uint32_t bytesRemaining(uint64_t position) = 0;

            /**
             * Does this block contain an evio dictionary?
             * @return <code>true</code> if this block contains an evio dictionary, else <code>false</code>.
             */
            virtual bool hasDictionary();

            /**
             * Is this the last block in the file or being sent over the network?
             * @return <code>true</code> if this is the last block in the file or being sent
             *         over the network, else <code>false</code>.
             */
            virtual bool isLastBlock();

// TODO: this needs to throw an exception right?? Or is the return enough?
            /**
             * Write myself out into a byte buffer. This write is relative--i.e.,
             * it uses the current position of the buffer.
             * @param byteBuffer the byteBuffer to write to.
             * @return the number of bytes written.
             */
            virtual size_t write(ByteBuffer byteBuffer) = 0;

            /**
             * Get the string representation of the block (record) header.
             * @return string representation of the block (record) header.
             */
            virtual string toString() = 0;
        };



}

#endif //EVIO_6_0_IBLOCKHEADER_H