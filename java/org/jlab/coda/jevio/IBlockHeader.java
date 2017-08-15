package org.jlab.coda.jevio;

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

    /** Offset to get block size from start of block. */
    int BLOCK_SIZE_OFFSET = 0;

    /** Offset in bytes to get magic number from start of file. */
    int MAGIC_OFFSET = 28;

    /** Offset in bytes to get version number from start of file. */
    int VERSION_OFFSET = 20;

    /** Mask in bytes to get version number from 6th int in block. */
    int VERSION_MASK = 0xff;


    /**
     * Was the data from buffer from which this header was read, swapped?
     *
     * @return <code>true</code> if data from buffer was swapped, else <code>false</code>
     */
    boolean isSwapped();

    /**
	 * Get the size of the block (physical record).
	 *
	 * @return the size of the block (physical record) in ints.
	 */
    int getSize();

    /**
	 * Get the block number for this block (physical record).
     * In a file, this is usually sequential.
     *
     * @return the block number for this block (physical record).
     */
    int getNumber();

    /**
	 * Get the block header length, in ints. This should be 8.
     *
     * @return the block header length. This should be 8.
     */
    int getHeaderLength();

    /**
     * Get the evio version of the block (physical record) header.
     *
     * @return the evio version of the block (physical record) header.
     */
    int getVersion();

    /**
	 * Get the magic number the block (physical record) header which should be 0xc0da0100.
     *
     * @return the magic number in the block (physical record).
     */
    int getMagicNumber();

    /**
	 * Get the byte order of the data being read.
     *
     * @return the byte order of the data being read.
     */
    ByteOrder getByteOrder();

    /**
     * Get the position in the buffer (in bytes) of this block's last data word.<br>
     *
     * @return the position in the buffer (in bytes) of this block's last data word.
     */
    long getBufferEndingPosition();

    /**
     * Get the starting position in the buffer (in bytes) from which this header was read--if that happened.<br>
     * This is not part of the block header proper. It is a position in a memory buffer of the start of the block
     * (physical record). It is kept for convenience. It is up to the reader to set it.
     *
     * @return the starting position in the buffer (in bytes) from which this header was read--if that happened.
     */
    long getBufferStartingPosition();

    /**
	 * Set the starting position in the buffer (in bytes) from which this header was read--if that happened.<br>
     * This is not part of the block header proper. It is a position in a memory buffer of the start of the block
     * (physical record). It is kept for convenience. It is up to the reader to set it.
     *
     * @param bufferStartingPosition the starting position in the buffer from which this header was read--if that
     *            happened.
     */
    void setBufferStartingPosition(long bufferStartingPosition);

    /**
	 * Determines where the start of the next block (physical record) header in some buffer is located (in bytes).
     * This assumes the start position has been maintained by the object performing the buffer read.
     *
     * @return the start of the next block (physical record) header in some buffer is located (in bytes).
     */
    long nextBufferStartingPosition();

    /**
	 * Determines where the start of the first event (logical record) in this block (physical record) is located
     * (in bytes). This assumes the start position has been maintained by the object performing the buffer read.
     *
     * @return where the start of the first event (logical record) in this block (physical record) is located
     *         (in bytes). Returns 0 if start is 0, signaling that this entire physical record is part of a
     *         logical record that spans at least three physical records.
     */
    long firstEventStartingPosition();

    /**
	 * Gives the bytes remaining in this block (physical record) given a buffer position.
     * The position is an absolute position in a byte buffer. This assumes that the
     * absolute position in <code>bufferStartingPosition</code> is being maintained
     * properly by the reader. No block is longer than 2.1GB - 31 bits of length.
     * This is for practical reasons - so a block can be read into a single byte array.
     *
     * @param position the absolute current position is a byte buffer.
     * @return the number of bytes remaining in this block (physical record.)
     * @throws EvioException if position arg out of bounds
     */
    int bytesRemaining(long position) throws EvioException;

    /**
	 * Gives the data (not header) bytes remaining in this block given a buffer position.
     * The position is an absolute position in a byte buffer. However, in the case of
     * compressed data, no header is contained in this buffer. Therefore this needs
     * to be accounted for. Otherwise this method is like {@link #bytesRemaining(long)}.
     *
     * @param position the absolute current position is a byte buffer.
     * @return the number of data bytes remaining in this block.
     * @throws EvioException if position arg out of bounds
     */
    int dataBytesRemaining(long position) throws EvioException;

    /**
     * Is this block's first event is an evio dictionary?
     *
     * @return <code>true</code> if this block's first event is an evio dictionary, else <code>false</code>
     */
    public boolean hasDictionary();

    /**
     * Is this the last block in the file or being sent over the network?
     *
     * @return <code>true</code> if this is the last block in the file or being sent
     *         over the network, else <code>false</code>
     */
    public boolean isLastBlock();

    /**
     * Is the data following this block header compressed in LZ4 format or not?
     *
     * @return <code>true</code> if this block's data is compressed, else <code>false</code>.
     */
    public boolean isCompressed();

    /**
	 * Write myself out a byte buffer. This write is relative--i.e., it uses the current position of the buffer.
     *
     * @param byteBuffer the byteBuffer to write to.
     * @return the number of bytes written, which for a BlockHeader is 32 (except when compressing).
     */
    int write(ByteBuffer byteBuffer);

    /**
     * Obtain a string representation of the block (physical record) header.
     *
     * @return a string representation of the block (physical record) header.
     */
    @Override
    public String toString();
}
