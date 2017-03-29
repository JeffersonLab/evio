package org.jlab.coda.jevio;

import java.io.OutputStream;
import java.io.IOException;
import java.util.zip.Deflater;
import java.util.zip.DeflaterOutputStream;
import java.util.zip.GZIPOutputStream;

/**
 * Extends GZIPOutputStream in order to avoid synchronized write method.
 * C. Timmer <p>
 * This class implements a stream filter for writing compressed data in
 * the GZIP file format.
 * @author      David Connelly
 *
 */
public
class EvioGZIPOutputStream extends GZIPOutputStream {

    /*
     * GZIP header magic number.
     */
    private final static int GZIP_MAGIC = 0x8b1f;

    /*
     * Trailer size in bytes.
     *
     */
    private final static int TRAILER_SIZE = 8;

    /**
     * Creates a new output stream with the specified buffer size.
     *
     * <p>The new output stream instance is created as if by invoking
     * the 3-argument constructor GZIPOutputStream(out, size, false).
     *
     * @param out the output stream
     * @param size the output buffer size
     * @exception IOException If an I/O error has occurred.
     * @exception IllegalArgumentException if {@code size <= 0}
     */
    public EvioGZIPOutputStream(OutputStream out, int size) throws IOException {
        super(out, size, false);
    }

    /**
     * Creates a new output stream with a default buffer size.
     *
     * <p>The new output stream instance is created as if by invoking
     * the 2-argument constructor GZIPOutputStream(out, false).
     *
     * @param out the output stream
     * @exception IOException If an I/O error has occurred.
     */
    public EvioGZIPOutputStream(OutputStream out) throws IOException {
        super(out, 512, false);
    }

    /**
     * Creates a new output stream with a default buffer size and
     * the specified flush mode.
     *
     * @param out the output stream
     * @param syncFlush
     *        if {@code true} invocation of the inherited
     *        {@link DeflaterOutputStream#flush() flush()} method of
     *        this instance flushes the compressor with flush mode
     *        {@link Deflater#SYNC_FLUSH} before flushing the output
     *        stream, otherwise only flushes the output stream
     *
     * @exception IOException If an I/O error has occurred.
     *
     * @since 1.7
     */
    public EvioGZIPOutputStream(OutputStream out, boolean syncFlush)
        throws IOException
    {
        super(out, 512, syncFlush);
    }

    /**
     * Avoid calling the parent's synchronized write method by extending
     * and calling the unsynchronized grandparent method directly.<p>
     *
     * Writes array of bytes to the compressed output stream. This method
     * will block until all the bytes are written.
     * @param buf the data to be written
     * @param off the start offset of the data
     * @param len the length of the data
     * @exception IOException If an I/O error has occurred.
     */
    public void write(byte[] buf, int off, int len)
        throws IOException
    {
        // Skip over the synchronized write() method of parent, GZIPOutputStream
        ((DeflaterOutputStream) this).write(buf, off, len);
        crc.update(buf, off, len);
    }

}
