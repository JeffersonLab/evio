package org.jlab.coda.jevio;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * This the the header for an evio segment structure (<code>EvioSegment</code>). It does not contain the raw data, just the
 * header. 
 * 
 * @author heddle
 * 
 */
public final class SegmentHeader extends BaseStructureHeader {
	
	
	/**
	 * Null constructor.
	 */
	public SegmentHeader() {
	}

	/**
	 * Constructor.
	 * @param tag the tag for the segment header.
	 * @param dataType the data type for the content of the segment.
	 */
	public SegmentHeader(int tag, DataType dataType) {
		super(tag, dataType);
	}

    /**
     * {@inheritDoc}
     */
    public int getHeaderLength() {return 1;}

    /**
     * {@inheritDoc}
     */
    protected void toArray(byte[] bArray, int offset, ByteOrder order) {
        try {
            if (order == ByteOrder.BIG_ENDIAN) {
                bArray[offset]   = (byte)tag;
                bArray[offset+1] = (byte)((dataType.getValue() & 0x3f) | (padding << 6));
                ByteDataTransformer.toBytes((short)length, order, bArray, offset+2);
            }
            else {
                ByteDataTransformer.toBytes((short)length, order, bArray, offset);
                bArray[offset+2] = (byte)((dataType.getValue() & 0x3f) | (padding << 6));
                bArray[offset+3] = (byte)tag;
            }
        }
        catch (EvioException e) {e.printStackTrace();}
    }

	/**
	 * Obtain a string representation of the segment header.
	 * @return a string representation of the segment header.
	 */
	@Override
	public String toString() {
		return String.format("segment length: %d\ndata type:      %s\ntag:            %d\npadding:        %d\n",
				length, getDataTypeName(), tag, padding);
	}
	
	/**
	 * Write myself out a byte buffer. This write is relative--i.e., it uses the current position of the buffer.
	 * 
	 * @param byteBuffer the byteBuffer to write to.
	 * @return the number of bytes written, which for a SegmentHeader is 4.
	 */
	public int write(ByteBuffer byteBuffer) {
        if (byteBuffer.order() == ByteOrder.BIG_ENDIAN) {
            byteBuffer.put((byte)tag);
            byteBuffer.put((byte)((dataType.getValue() & 0x3f) | (padding << 6)));
            byteBuffer.putShort((short)length);
        }
        else {
            byteBuffer.putShort((short)length);
            byteBuffer.put((byte)((dataType.getValue() & 0x3f) | (padding << 6)));
            byteBuffer.put((byte)tag);
        }
		return 4;
	}


}