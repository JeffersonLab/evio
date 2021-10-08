package org.jlab.coda.jevio;

import org.jlab.coda.hipo.CompressionType;

import java.nio.BufferOverflowException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Make a common interface for different versions of the BlockHeader.
 *
 * @author timmer
 */
public interface IBlockHeader {

    /** The magic number, should be the value of <code>magicNumber</code>. */
    int MAGIC_NUMBER = 0xc0da0100;

    /**
	 * Get the size of the block (record) in 32 bit words.
	 * @return size of the block (record) in 32 bit words.
	 */
    int getSize();

    /**
	 * Get the block number for this block (record).
     * In a file, this is usually sequential.
     * @return the block number for this block (record).
     */
    int getNumber();

    /**
	 * Get the block (record) header length, in 32 bit words.
     * @return block (record) header length, in 32 bit words.
     */
    int getHeaderWords();

    /**
	 * Get the source ID number if in CODA online context and data is coming from ROC.
     * @return source ID number if in CODA online context and data is coming from ROC.
     */
    int getSourceId();

    /**
     * Does this block/record contain the "first event"
     * (first event to be written to each file split)?
     * @return <code>true</code> if this record has the first event, else <code>false</code>
     */
    boolean hasFirstEvent();

    /**
     * Is the data in this block from a streaming (not triggered) DAQ system?
     * @return <code>true</code> if the data in this block is from a streaming (not triggered)
     *         DAQ system, else <code>false</code>
     */
    boolean isStreaming();

    /**
     * Get the type of events in block/record (see values of {@link DataType}.
     * @return type of events in block/record.
     */
    int getEventType();

    /**
     * Get the evio version of the block (record) header.
     * @return evio version of the block (record) header.
     */
    int getVersion();

    /**
	 * Get the magic number the block (record) header which should be 0xc0da0100.
     * @return magic number in the block (record).
     */
    int getMagicNumber();

    /**
	 * Get the byte order of the data being read.
     * @return byte order of the data being read.
     */
    ByteOrder getByteOrder();

    /**
     * Get the position in the buffer (bytes) of this block's last data word.<br>
     * @return position in the buffer (bytes) of this block's last data word.
     */
    long getBufferEndingPosition();

    /**
     * Get the starting position in the buffer (bytes) from which this header was read--if that happened.<br>
     * This is not part of the block header proper. It is a position in a memory buffer of the start of the block
     * (record). It is kept for convenience. It is up to the reader to set it.
     *
     * @return starting position in buffer (bytes) from which this header was read--if that happened.
     */
    long getBufferStartingPosition();

    /**
	 * Set the starting position in the buffer (bytes) from which this header was read--if that happened.<br>
     * This is not part of the block header proper. It is a position in a memory buffer of the start of the block
     * (record). It is kept for convenience. It is up to the reader to set it.
     *
     * @param bufferStartingPosition starting position in buffer from which this header was read--if that
     *            happened.
     */
    void setBufferStartingPosition(long bufferStartingPosition);

    /**
	 * Determines where the start of the next block (record) header in some buffer is located (bytes).
     * This assumes the start position has been maintained by the object performing the buffer read.
     *
     * @return the start of the next block (record) header in some buffer is located (bytes).
     */
    long nextBufferStartingPosition();

    /**
	 * Determines where the start of the first event in this block (record) is located
     * (bytes). This assumes the start position has been maintained by the object performing the buffer read.
     *
     * @return where the start of the first event in this block (record) is located
     *         (bytes). In evio format version 2, returns 0 if start is 0, signaling
     *         that this entire record is part of a logical record that spans at least
     *         three physical records.
     */
    long firstEventStartingPosition();

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
    int bytesRemaining(long position) throws EvioException;

    /**
     * Does this block contain an evio dictionary?
     * @return <code>true</code> if this block contains an evio dictionary, else <code>false</code>.
     */
    boolean hasDictionary();

    /**
     * Is this the last block in the file or being sent over the network?
     * @return <code>true</code> if this is the last block in the file or being sent
     *         over the network, else <code>false</code>.
     */
    boolean isLastBlock();

    /**
     * Is this the data in this block compressed?
     * @return <code>true</code> if the data in this block is compressed, else <code>false</code>.
     */
    boolean isCompressed();

    /**
     * Get the type of data compression used.
     * @return type of data compression used.
     */
    CompressionType getCompressionType();

// TODO: this needs to throw an exception right?? Or is the return enough?
    /**
	 * Write myself out into a byte buffer. This write is relative--i.e.,
     * it uses the current position of the buffer.
     * @param byteBuffer the byteBuffer to write to.
     * @return the number of bytes written.
     * @throws BufferOverflowException if insufficient room to write header into buffer.
     */
    int write(ByteBuffer byteBuffer) throws BufferOverflowException;

    /**
     * Get the string representation of the block (record) header.
     * @return string representation of the block (record) header.
     */
    @Override
    String toString();
}
