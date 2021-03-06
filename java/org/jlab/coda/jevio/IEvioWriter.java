package org.jlab.coda.jevio;

import java.nio.BufferOverflowException;
import java.nio.ByteBuffer;

/**
 * This is implemented by objects that will be writing themselves to en evio file.
 * @author heddle
 *
 */
public interface IEvioWriter {

	/**
	 * Write myself out a byte buffer.
	 * @param byteBuffer the byteBuffer to write to.
	 * @return the number of bytes written.
	 * @throws BufferOverflowException if insufficient room to write header into buffer.
	 */
	int write(ByteBuffer byteBuffer) throws BufferOverflowException;
}
