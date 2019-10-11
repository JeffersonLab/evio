/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package org.jlab.coda.hipo;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;

import com.aha.compression.AHACompressionAPI;
import net.jpountz.lz4.*;

/**
 * Singleton class used to provide data compression and decompression in a variety of formats.
 * @author gavalian
 * @author timmer
 */
public class Compressor {


    /** Do we have an AHA374 data compression board on the local host? */
    private static boolean haveHardwareGzipCompression = false;

    /** Number of bytes to read in a single call while doing gzip decompression. */
    private static final int MTU = 1024*1024;

    /** Object used to create compressors. */
    private static final LZ4Factory factory;
    /** Fastest LZ4 compressor. */
    private static final LZ4Compressor lz4_compressor;
    /** Slowest but best LZ4 compressor. */
    private static final LZ4Compressor lz4_compressor_best;
    /** Decompressor for LZ4 if decompressed size unknown. */
    private static final LZ4SafeDecompressor lz4_decompressor;

 
    private static int getYear(ByteBuffer buf){
      int rv = 0;
      rv |= (int)buf.get(6);
      rv &= 0x000000ff;
      rv |= (int)buf.get(7) << 8;
      rv &= 0x0000ffff;
      return rv;
    }

    private static int getRevisionId(ByteBuffer buf, int board_id){
      int rv = 0;
      rv |= buf.get((9 + board_id));
      rv &= 0x000000ff;
      return rv;
    }

    private static int getSubsystemId(ByteBuffer buf, int board_id){
      int rv = 0;
      int offset = (26 + (board_id * 2));
      rv |= buf.get(offset);
      rv &= 0x000000ff;
      rv |= buf.get((offset + 1)) << 8;
      rv &= 0x0000ffff;
      return rv;
    }

    private static int getDeviceId(ByteBuffer buf, int board_id){
      int rv = 0;
      int offset = (58 + (board_id * 2));
      rv |= buf.get(offset);
      rv &= 0x000000ff;
      rv |= buf.get((offset + 1)) << 8;
      rv &= 0x0000ffff;
      return rv;
    }


    static {
        factory = LZ4Factory.fastestInstance();
        lz4_compressor = factory.fastCompressor();
        lz4_compressor_best = factory.highCompressor();
        lz4_decompressor = factory.safeDecompressor();

        // Check for an AHA374 FPGA data compression board for hardware (de)compression.
        // Only want to do this once. Need the jar which uses native methods,
        // the C lib which it wraps using jni, and the C lib which does the compression.
        // Both C libs must have their directory in LD_LIBRARY_PATH for this to work!
        try {
            // Call this instead of AHACompressionAPI.LoadAHALibrary("");
            System.loadLibrary("aha_compression_api");

            AHACompressionAPI apiObj = new AHACompressionAPI();
            ByteBuffer as = ByteBuffer.allocateDirect(8);
            as.order(ByteOrder.nativeOrder());
            int rv = AHACompressionAPI.Open(apiObj, as, 0, 0);
            System.out.println("rv = " + rv);

             rv = 0;
             String betaString;
             ByteBuffer versionArg;
             int versionArgSize = 89;
             versionArg = ByteBuffer.allocateDirect(versionArgSize);
             versionArg.order(ByteOrder.nativeOrder());

             long longrv = AHACompressionAPI.Version(apiObj, as, versionArg);
             if(longrv != 0){
               rv = -1;
             }

             int loopMax = versionArg.get(8);
             if(versionArg.get(0) != 0){
               betaString = " Beta " + versionArg.get(0);
             }else{
                 betaString = "";
             }

            String receivedDriverVersion = (versionArg.get(3) + "." + versionArg.get(2) + "." + versionArg.get(1) + betaString);
            String receivedReleaseDate = versionArg.get(5) + "/" + versionArg.get(4) + "/" + getYear(versionArg);
            String receivedNumBoards = "" + versionArg.get(8);

            System.out.println("driver version  = " + receivedDriverVersion);
            System.out.println("release date    = " + receivedReleaseDate);
            System.out.println("number of chips = " + receivedNumBoards);

            for(int j = 0; j < loopMax; j++){
                String receivedHwRevisionId = String.format("0x%02X", getRevisionId(versionArg, j));
                String receivedHwSubsystemId = String.format("0x%04X", getSubsystemId(versionArg, j));
                String receivedHwDeviceId = String.format("0x%04X", getDeviceId(versionArg, j));
                System.out.println("revision  id (" + j + ")= " + receivedHwRevisionId);
                System.out.println("subsystem id (" + j + ")= " + receivedHwSubsystemId);
                System.out.println("device    id (" + j + ")= " + receivedHwDeviceId);
            }



            haveHardwareGzipCompression = true;
            System.out.println("\nSuccessfully loaded aha3xx jni shared lib for gzip hardware compression available\n");
        }
        catch (Error e) {
            // If the library cannot be found, we can still do compression -
            // just not with the AHA374 compression board.
            System.out.println("\nCannot load aha3xx jni shared lib so no gzip hardware compression available\n");
        }
    }

    
    /** One instance of this class. */
    private static Compressor instance = null;


    /** Use this class when decompressing GZIP data from a ByteBuffer. */
    public static class ByteBufferBackedInputStream extends InputStream {

        private ByteBuffer backendBuffer;

        public ByteBufferBackedInputStream(ByteBuffer backendBuffer) {
            this.backendBuffer = backendBuffer;
        }

        public void close() {this.backendBuffer = null;}

        private void ensureStreamAvailable() throws IOException {
            if (this.backendBuffer == null) {
                throw new IOException("read on a closed InputStream!");
            }
        }

        public int read() throws IOException {
            this.ensureStreamAvailable();
            return this.backendBuffer.hasRemaining() ? this.backendBuffer.get() & 0xFF : -1;
        }

        public int read(byte[] buffer) throws IOException {
            return this.read(buffer, 0, buffer.length);
        }

        public int read(byte[] buffer, int offset, int length) throws IOException {
            this.ensureStreamAvailable();
            if (offset >= 0 && length >= 0 && length <= buffer.length - offset) {
                if (length == 0) {
                    return 0;
                }
                else {
                    int remainingSize = Math.min(this.backendBuffer.remaining(), length);
                    if (remainingSize == 0) {
                        return -1;
                    }
                    else {
                        this.backendBuffer.get(buffer, offset, remainingSize);
                        return remainingSize;
                    }
                }
            }
            else {
                throw new IndexOutOfBoundsException();
            }
        }

        public long skip(long n) throws IOException {
            this.ensureStreamAvailable();
            if (n <= 0L) {
                return 0L;
            }
            int length = (int) n;
            int remainingSize = Math.min(this.backendBuffer.remaining(), length);
            this.backendBuffer.position(this.backendBuffer.position() + remainingSize);
            return (long) length;
        }

        public int available() throws IOException {
            this.ensureStreamAvailable();
            return this.backendBuffer.remaining();
        }

        public void mark(int var1) {}

        public void reset() throws IOException {
            throw new IOException("mark/reset not supported");
        }

        public boolean markSupported() {return false;}
    }



    /** Constructor to defeat instantiation. */
    protected Compressor() {}

    /**
     * Get the single instance of this class.
     * @return single instance of this class.
     */
    public static Compressor getInstance() {
        if (instance == null) {
            instance = new Compressor();
        }
        return instance;
    }

    /**
     * Returns the maximum number of bytes needed to compress the given length
     * of uncompressed data. Depends on compression type. Unknown for gzip.
     *
     * @param compressionType type of data compression to do
     *                        (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip).
     *                        Default to none.
     * @param uncompressedLength uncompressed data length in bytes.
     * @return maximum compressed length in bytes or -1 if unknown.
     */
    public static int getMaxCompressedLength(CompressionType compressionType, int uncompressedLength) {
        switch (compressionType) {
            case RECORD_COMPRESSION_GZIP:
                return -1;
            case RECORD_COMPRESSION_LZ4_BEST:
                return lz4_compressor_best.maxCompressedLength(uncompressedLength);
            case RECORD_COMPRESSION_LZ4:
                return lz4_compressor.maxCompressedLength(uncompressedLength);
            case RECORD_UNCOMPRESSED:
            default:
                return uncompressedLength;
        }
    }

    /**
     * Returns compressed buffer. Depends on compression type.
     * @param compressionType type of data compression to do
     *                        (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip).
     *                        Default to none.
     * @param buffer uncompressed buffer.
     * @return compressed buffer.
     */
    public static byte[] getCompressedBuffer(CompressionType compressionType, byte[] buffer) {
        switch(compressionType) {
            case RECORD_COMPRESSION_GZIP:
                return Compressor.compressGZIP(buffer);
            case RECORD_COMPRESSION_LZ4_BEST:
                return lz4_compressor_best.compress(buffer);
            case RECORD_COMPRESSION_LZ4:
                return lz4_compressor.compress(buffer);
            case RECORD_UNCOMPRESSED:
            default:
                return buffer;
        }
    }

    /**
     * Returns uncompressed buffer. Depends on compression type.
     * @param compressionType type of data compression to undo
     *                        (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip).
     *                        Default to none.
     * @param compressedBuffer uncompressed array.
     * @return uncompressed array.
     * @throws LZ4Exception if not enough room in allocated array
     *                      (3x compressed) to hold LZ4 uncompressed data.
     */
    public static byte[] getUnCompressedBuffer(CompressionType compressionType, byte[] compressedBuffer)
                                throws LZ4Exception {
        switch(compressionType) {
            case RECORD_COMPRESSION_GZIP:
                return Compressor.uncompressGZIP(compressedBuffer);
            case RECORD_COMPRESSION_LZ4_BEST:
            case RECORD_COMPRESSION_LZ4:
                // Only works if did NOT get more than 3x compression originally.
                // Computationally expensive method.
                return lz4_decompressor.decompress(compressedBuffer, 3*compressedBuffer.length);
            case RECORD_UNCOMPRESSED:
            default:
                return compressedBuffer;
        }
    }

    /**
     * GZIP compression. Returns compressed byte array.
     * @param ungzipped  uncompressed data.
     * @return compressed data.
     */
    public static byte[] compressGZIP(byte[] ungzipped){
        return compressGZIP(ungzipped, 0, ungzipped.length);
    }

    /**
     * GZIP compression. Returns compressed byte array.
     * @param ungzipped  uncompressed data.
     * @param offset     offset into ungzipped array
     * @param length     length of valid data in bytes
     * @return compressed data.
     */
    public static byte[] compressGZIP(byte[] ungzipped, int offset, int length) {
        final ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try {
            final GZIPOutputStream gzipOutputStream = new GZIPOutputStream(bytes);
            gzipOutputStream.write(ungzipped, offset, length);
            gzipOutputStream.close();
        } catch (IOException e) {
           // LOG.error("Could not gzip " + Arrays.toString(ungzipped));
            System.out.println("[iG5DataCompressor] ERROR: Could not gzip the array....");
        }
        return bytes.toByteArray();
    }

    /**
     * GZIP decompression. Returns uncompressed byte array.
     * @param gzipped compressed data.
     * @return uncompressed data.
     */
    public static byte[] uncompressGZIP(byte[] gzipped) {
        return uncompressGZIP(gzipped, 0, gzipped.length);
    }

    /**
     * GZIP decompression. Returns uncompressed byte array.
     * @param gzipped compressed data.
     * @param off     offset into gzipped array.
     * @param length  max number of bytes to read from gzipped.
     * @return uncompressed data.
     */
    public static byte[] uncompressGZIP(byte[] gzipped, int off, int length) {
        byte[] ungzipped = new byte[0];
        try {
            final GZIPInputStream inputStream = new GZIPInputStream(
                    new ByteArrayInputStream(gzipped, off, length), Compressor.MTU);

            ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream(2*length);
            final byte[] buffer = new byte[Compressor.MTU];
            int bytesRead = 0;
            while (bytesRead != -1) {
                bytesRead = inputStream.read(buffer, 0, Compressor.MTU);
                if (bytesRead != -1) {
                    byteArrayOutputStream.write(buffer, 0, bytesRead);
                }
            }
            ungzipped = byteArrayOutputStream.toByteArray();
            inputStream.close();
            byteArrayOutputStream.close();
        } catch (IOException e) {
            //LOG.error("Could not ungzip. Heartbeat will not be working. " + e.getMessage());
            System.out.println("[Evio::compressor] ERROR: could not uncompress the array. \n"
                    + e.getMessage());
        }
        return ungzipped;
    }

    /**
     * GZIP decompression. Returns uncompressed byte array.
     * @param gzipped compressed data.
     * @return uncompressed data.
     */
    public static byte[] uncompressGZIP(ByteBuffer gzipped) {
        byte[] ungzipped = new byte[0];
        try {
            int length = gzipped.remaining();

            final GZIPInputStream inputStream = new GZIPInputStream(
                    new ByteBufferBackedInputStream(gzipped), Compressor.MTU);

            ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream(2*length);
            final byte[] buffer = new byte[Compressor.MTU];
            int bytesRead = 0;
            while (bytesRead != -1) {
                bytesRead = inputStream.read(buffer, 0, Compressor.MTU);
                if (bytesRead != -1) {
                    byteArrayOutputStream.write(buffer, 0, bytesRead);
                }
            }
            ungzipped = byteArrayOutputStream.toByteArray();
            inputStream.close();
            byteArrayOutputStream.close();
        } catch (IOException e) {
            //LOG.error("Could not ungzip. Heartbeat will not be working. " + e.getMessage());
            System.out.println("[Evio::compressor] ERROR: could not uncompress the array. \n"
                    + e.getMessage());
        }
        return ungzipped;
    }

    /**
     * Fastest LZ4 compression. Returns length of compressed data in bytes.
     *
     * @param src      source of uncompressed data.
     * @param srcSize  number of bytes to compress.
     * @param dst      destination buffer.
     * @param maxSize  maximum number of bytes to write in dst.
     * @return length of compressed data in bytes.
     * @throws org.jlab.coda.hipo.HipoException if maxSize < max # of compressed bytes
     */
    public static int compressLZ4(ByteBuffer src, int srcSize, ByteBuffer dst, int maxSize)
            throws HipoException {
        //System.out.println("----> compressing " + srcSize + " max size = " + maxSize);
        if (lz4_compressor.maxCompressedLength(srcSize) > maxSize) {
            throw new HipoException("maxSize (" + maxSize + ") is < max # of compressed bytes (" +
                                    lz4_compressor.maxCompressedLength(srcSize) + ")");
        }

        if (src.hasArray() && dst.hasArray()) {
            return lz4_compressor.compress(src.array(), src.position() + src.arrayOffset(),
                                           srcSize,
                                           dst.array(), dst.position() + dst.arrayOffset(),
                                           maxSize);
        }
        else {
            return lz4_compressor.compress(src, src.position(), srcSize, dst, dst.position(), maxSize);
        }

    }

    /**
     * Fastest LZ4 compression. Returns length of compressed data in bytes.
     *
     * @param src      source of uncompressed data.
     * @param srcOff   start offset in src.
     * @param srcSize  number of bytes to compress.
     * @param dst      destination array.
     * @param dstOff   start offset in dst.
     * @param maxSize  maximum number of bytes to write in dst.
     * @return length of compressed data in bytes.
     * @throws org.jlab.coda.hipo.HipoException if maxSize < max # of compressed bytes
     */
    public static int compressLZ4(byte[] src, int srcOff, int srcSize,
                                  byte[] dst, int dstOff, int maxSize)
            throws HipoException {
        //System.out.println("----> compressing " + srcSize + " max size = " + maxSize);
        if (lz4_compressor.maxCompressedLength(srcSize) > maxSize) {
            throw new HipoException("maxSize (" + maxSize + ") is < max # of compressed bytes (" +
                                    lz4_compressor.maxCompressedLength(srcSize) + ")");
        }
        return lz4_compressor.compress(src, srcOff, srcSize, dst, dstOff, maxSize);
    }

    /**
     * Fastest LZ4 compression. Returns length of compressed data in bytes.
     *
     * @param src      source of uncompressed data.
     * @param srcOff   start offset in src.
     * @param srcSize  number of bytes to compress.
     * @param dst      destination array.
     * @param dstOff   start offset in dst.
     * @param maxSize  maximum number of bytes to write in dst.
     * @return length of compressed data in bytes.
     * @throws org.jlab.coda.hipo.HipoException if maxSize < max # of compressed bytes
     */
    public static int compressLZ4(ByteBuffer src, int srcOff, int srcSize,
                                  ByteBuffer dst, int dstOff, int maxSize)
            throws HipoException {
        //System.out.println("----> compressing " + srcSize + " max size = " + maxSize);
        if (lz4_compressor.maxCompressedLength(srcSize) > maxSize) {
            throw new HipoException("maxSize (" + maxSize + ") is < max # of compressed bytes (" +
                                    lz4_compressor.maxCompressedLength(srcSize) + ")");
        }

        if (src.hasArray() && dst.hasArray()) {
            return lz4_compressor.compress(src.array(), srcOff + src.arrayOffset(),
                                           srcSize,
                                           dst.array(), dstOff + dst.arrayOffset(),
                                           maxSize);
        }
        else {
            return lz4_compressor.compress(src, srcOff, srcSize, dst, dstOff, maxSize);
        }
    }

    /**
     * Highest LZ4 compression. Returns length of compressed data in bytes.
     *
     * @param src      source of uncompressed data.
     * @param srcSize  number of bytes to compress.
     * @param dst      destination buffer.
     * @param maxSize  maximum number of bytes to write in dst.
     * @return length of compressed data in bytes.
     * @throws org.jlab.coda.hipo.HipoException if maxSize < max # of compressed bytes
     */
    public static int compressLZ4Best(ByteBuffer src, int srcSize, ByteBuffer dst, int maxSize)
            throws HipoException {
        //System.out.println("----> compressing " + srcSize + " max size = " + maxSize);
        if (lz4_compressor_best.maxCompressedLength(srcSize) > maxSize) {
            throw new HipoException("maxSize (" + maxSize + ") is < max # of compressed bytes (" +
                                            lz4_compressor_best.maxCompressedLength(srcSize) + ")");
        }
        
        if (src.hasArray() && dst.hasArray()) {
            return lz4_compressor_best.compress(src.array(), src.position() + src.arrayOffset(),
                                                srcSize,
                                                dst.array(), dst.position() + dst.arrayOffset(),
                                                maxSize);
        }
        else {
            return lz4_compressor_best.compress(src, src.position(), srcSize,
                                                dst, dst.position(), maxSize);
        }
    }

    /**
     * Highest LZ4 compression. Returns length of compressed data in bytes.
     *
     * @param src      source of uncompressed data.
     * @param srcOff   start offset in src.
     * @param srcSize  number of bytes to compress.
     * @param dst      destination array.
     * @param dstOff   start offset in dst.
     * @param maxSize  maximum number of bytes to write in dst.
     * @return length of compressed data in bytes.
     * @throws org.jlab.coda.hipo.HipoException if maxSize < max # of compressed bytes
     */
    public static int compressLZ4Best(byte[] src, int srcOff, int srcSize,
                                      byte[] dst, int dstOff, int maxSize)
            throws HipoException {
        //System.out.println("----> compressing " + srcSize + " max size = " + maxSize);
        if (lz4_compressor_best.maxCompressedLength(srcSize) > maxSize) {
            throw new HipoException("maxSize (" + maxSize + ") is < max # of compressed bytes (" +
                                            lz4_compressor_best.maxCompressedLength(srcSize) + ")");
        }
        return lz4_compressor_best.compress(src, srcOff, srcSize, dst, dstOff, maxSize);
    }

    /**
     * Highest LZ4 compression. Returns length of compressed data in bytes.
     *
     * @param src      source of uncompressed data.
     * @param srcOff   start offset in src.
     * @param srcSize  number of bytes to compress.
     * @param dst      destination array.
     * @param dstOff   start offset in dst.
     * @param maxSize  maximum number of bytes to write in dst.
     * @return length of compressed data in bytes.
     * @throws org.jlab.coda.hipo.HipoException if maxSize < max # of compressed bytes
     */
    public static int compressLZ4Best(ByteBuffer src, int srcOff, int srcSize,
                                      ByteBuffer dst, int dstOff, int maxSize)
            throws HipoException {
        //System.out.println("----> compressing " + srcSize + " max size = " + maxSize);
        if (lz4_compressor_best.maxCompressedLength(srcSize) > maxSize) {
            throw new HipoException("maxSize (" + maxSize + ") is < max # of compressed bytes (" +
                                            lz4_compressor_best.maxCompressedLength(srcSize) + ")");
        }
        return lz4_compressor_best.compress(src, srcOff, srcSize, dst, dstOff, maxSize);
    }

    /**
     * LZ4 decompression. Returns original length of decompressed data in bytes.
     *
     * @param src      source of compressed data.
     * @param srcSize  number of compressed bytes.
     * @param dst      destination array.
     * @return original (uncompressed) input size.
     * @throws HipoException if dst is too small to hold uncompressed data
     */
    public static int uncompressLZ4(ByteBuffer src, int srcSize, ByteBuffer dst)
            throws HipoException {
        return uncompressLZ4(src, src.position(), srcSize, dst);
    }

    /**
     * LZ4 decompression. Returns original length of decompressed data in bytes.
     *
     * @param src      source of compressed data.
     * @param srcOff   start offset in src.
     * @param srcSize  number of compressed bytes.
     * @param dst      destination array.
     * @return original (uncompressed) input size.
     * @throws HipoException if dst is too small to hold uncompressed data
     */
    public static int uncompressLZ4(ByteBuffer src, int srcOff, int srcSize, ByteBuffer dst)
            throws HipoException {

        try {
            int size;
            int destOff = dst.position();

            if (src.hasArray() && dst.hasArray()) {
//System.out.println("uncompressLZ4:  src off = " + (src.arrayOffset() + srcOff) +
//                  ", src len = " + srcSize);
                size = lz4_decompressor.decompress(src.array(), srcOff + src.arrayOffset(), srcSize,
                                                   dst.array(), destOff + dst.arrayOffset());
            }
            else {
                size = lz4_decompressor.decompress(src, srcOff, srcSize,
                                                   dst, destOff, dst.capacity() - destOff);
            }

            // Prepare buffer for reading
            dst.limit(destOff + size).position(destOff);
            return size;
        }
        catch (LZ4Exception e) {
            throw new HipoException(e);
        }
    }

    /**
     * LZ4 decompression. Returns original length of decompressed data in bytes.
     *
     * @param src      source of compressed data.
     * @param srcOff   start offset in src.
     * @param srcSize  number of compressed bytes.
     * @param dst      destination array.
     * @param dstOff   start offset in dst.
     * @return original (uncompressed) input size.
     * @throws org.jlab.coda.hipo.HipoException if (dst.length - dstOff) is too small
     *                                          to hold uncompressed data
     */
    public static int uncompressLZ4(byte[] src, int srcOff, int srcSize, byte[] dst, int dstOff)
            throws HipoException {

        try {
            return lz4_decompressor.decompress(src, srcOff, srcSize, dst, dstOff);
        }
        catch (LZ4Exception e) {
            throw new HipoException(e);
        }
    }

    /**
     * LZ4 decompression. Returns an array containing the original decompressed data in bytes.
     *
     * @param src      source of compressed data.
     * @param srcOff   start offset in src.
     * @param srcSize  number of compressed bytes.
     * @param maxDestLen    max available bytes to hold uncompressed data.
     * @return original, uncompressed data. Length of array is length of valid data.
     * @throws HipoException
     */
    public static byte[] uncompressLZ4(byte[] src, int srcOff, int srcSize, int maxDestLen) {
            return lz4_decompressor.decompress(src, srcOff, srcSize, maxDestLen);
    }


}
