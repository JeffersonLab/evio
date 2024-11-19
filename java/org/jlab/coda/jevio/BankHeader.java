package org.jlab.coda.jevio;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * This the header for an evio bank structure (<code>EvioBank</code>). It does not contain the raw data, just the
 * header. Note: since an "event" is really just the outermost bank, this is also the header for an
 * <code>EvioEvent</code>.
 * 
 * @author heddle
 * 
 */
public final class BankHeader extends BaseStructureHeader {

	/**
	 * Null constructor.
	 */
	public BankHeader() {
	}

	/**
	 * Constructor
	 * @param tag the tag for the bank header.
	 * @param dataType the data type for the content of the bank.
	 * @param num sometimes, but not necessarily, an ordinal enumeration.
	 */
	public BankHeader(int tag, DataType dataType, int num) {
		super(tag, dataType, num);
	}

	/**
	 * {@inheritDoc}
	 */
	public int getDataLength() {return length - 1;}

    /**
     * {@inheritDoc}
     */
    public int getHeaderLength() {return 2;}

    /**
     * {@inheritDoc}
     */
    protected void toArray(byte[] bArray, int offset, ByteOrder order) {
        try {
            // length first
            ByteDataTransformer.toBytes(length, order, bArray, offset);
			int word = (tag << 16 | (byte)((dataType.getValue() & 0x3f) | (padding << 6)) << 8 | (number & 0xffff));
			ByteDataTransformer.toBytes(word, order, bArray, offset+4);
        }
        catch (EvioException e) {e.printStackTrace();}
    }


	/**
	 * Obtain a string representation of the bank header.
	 * @return a string representation of the bank header.
	 */
	@Override
	public String toString() {
		return String.format("bank length: %d\nnumber:      %d\ndata type:   %s\ntag:         %d\npadding:     %d\n",
					  length, number, getDataTypeName(), tag, padding);
	}

	/**
	 * Write myself out a byte buffer. This write is relative--i.e., it uses the current position of the buffer.
	 * 
	 * @param byteBuffer the byteBuffer to write to.
	 * @return the number of bytes written, which for a BankHeader is 8.
	 */
	public int write(ByteBuffer byteBuffer) {
		byteBuffer.putInt(length);
		int word = (tag << 16 | ((dataType.getValue() & 0x3f) | (padding << 6)) << 8 | (number & 0xffff));
		byteBuffer.putInt(word);

		return 8;
	}

}
