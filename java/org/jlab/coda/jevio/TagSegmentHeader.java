package org.jlab.coda.jevio;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * This the the header for an evio tag segment structure (<code>EvioTagSegment</code>). It does not contain the raw data, just the
 * header. 
 * 
 * @author heddle
 * 
 */
public final class TagSegmentHeader extends BaseStructureHeader {

	/**
	 * Null constructor.
	 */
	public TagSegmentHeader() {
	}

	/**
	 * Constructor.
	 * @param tag the tag for the tag segment header.
	 * @param dataType the data type for the content of the tag segment.
	 */
	public TagSegmentHeader(int tag, DataType dataType) {
        super(tag, dataType);
	}

	/**
	 * {@inheritDoc}
	 */
	public int getDataLength() {return length;}

    /**
     * {@inheritDoc}
     */
    public int getHeaderLength() {return 1;}

    /**
     * {@inheritDoc}
     */
    protected void toArray(byte[] bArray, int offset, ByteOrder order) {
		try {
			int word = (tag << 20 | ((dataType.getValue() & 0xf)) << 16 | (length & 0xffff));
			ByteDataTransformer.toBytes(word, order, bArray, offset);
        }
        catch (EvioException e) {e.printStackTrace();}
    }
    
	/**
	 * Obtain a string representation of the bank header.
	 * @return a string representation of the bank header.
	 */
	@Override
	public String toString() {
		return String.format("tag-seg length: %d\ndata type:      %s\ntag:            %d\n",
				length, getDataTypeName(), tag);
	}
	
	/**
	 * Write myself out a byte buffer. This write is relative--i.e., it uses the current position of the buffer.
	 * 
	 * @param byteBuffer the byteBuffer to write to.
	 * @return the number of bytes written, which for a TagSegmentHeader is 4.
	 */
	public int write(ByteBuffer byteBuffer) {
		int word = (tag << 20 | ((dataType.getValue() & 0xf)) << 16 | (length & 0xffff));
		byteBuffer.putInt(word);
		return 4;
	}

}