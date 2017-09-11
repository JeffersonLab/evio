/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package org.jlab.coda.hipo;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;
import net.jpountz.lz4.LZ4Compressor;
import net.jpountz.lz4.LZ4Exception;
import net.jpountz.lz4.LZ4Factory;
import net.jpountz.lz4.LZ4SafeDecompressor;

/**
 * Class used to provide data compression and decompression in a variety of formats.
 * @author gavalian
 */
public class Compressor {
    
    /** No data compression. */
    public static final int         RECORD_UNCOMPRESSED = 0;
    /** Data compression using fastest LZ4 method. */
    public static final int      RECORD_COMPRESSION_LZ4 = 1;
    /** Data compression using slowest but most compact LZ4 method. */
    public static final int RECORD_COMPRESSION_LZ4_BEST = 2;
    /** Data compression using gzip method. */
    public static final int     RECORD_COMPRESSION_GZIP = 3;

    /** Number of bytes to read in a single call while doing gzip decompression. */
    private static final int MTU = 1024*1024;
    
    private LZ4Factory factory = null;
    private LZ4Compressor lz4_compressor = null;
    private LZ4Compressor lz4_compressor_best = null;
    private LZ4SafeDecompressor lz4_decompressor = null;


    /** No-arg constructor. */
    public Compressor(){
        factory = LZ4Factory.fastestInstance();
        lz4_compressor = factory.fastCompressor();
        lz4_compressor_best = factory.highCompressor();
        lz4_decompressor = factory.safeDecompressor();
    }
    
    /**
     * Returns compressed buffer. Depends on compression type.
     * @param compressionType compression type.
     * @param buffer uncompressed buffer.
     * @return compressed buffer.
     */
    public static byte[] getCompressedBuffer(int compressionType, byte[] buffer){
        if (compressionType == RECORD_COMPRESSION_GZIP){
            return Compressor.compressGZIP(buffer);
        }
        return buffer;
    }

    /**
     * Returns uncompressed buffer. Depends on compression type.
     * @param compressionType compression type.
     * @param compressedBuffer compressed buffer.
     * @return uncompressed buffer.
     */
    public static byte[] getUnCompressedBuffer(int compressionType, byte[] compressedBuffer){
        if(compressionType == RECORD_COMPRESSION_GZIP){
            return Compressor.uncompressGZIP(compressedBuffer);
        }
        return compressedBuffer;
    }

    /**
     * GZIP compression. Returns compressed byte array.
     * @param ungzipped  uncompressed data.
     * @return compressed data.
     */
    public static byte[] compressGZIP(byte[] ungzipped){
        final ByteArrayOutputStream bytes = new ByteArrayOutputStream();        
        try {
            final GZIPOutputStream gzipOutputStream = new GZIPOutputStream(bytes);
            gzipOutputStream.write(ungzipped);
            gzipOutputStream.close();
        } catch (IOException e) {
           // LOG.error("Could not gzip " + Arrays.toString(ungzipped));
            System.out.println("[iG5DataCompressor] ERROR: Could not gzip the array....");
        }
        return bytes.toByteArray();
    }

    /**
     * GZIP compression. Returns compressed byte array.
     * @param ungzipped  uncompressed data.
     * @param offset     offset into ungzipped array
     * @param length     length of valid data in bytes
     * @return compressed data.
     */
    public static byte[] compressGZIP(byte[] ungzipped, int offset, int length){
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
    public static byte[] uncompressGZIP(byte[] gzipped){
        byte[] ungzipped = new byte[0];
        try {
            final GZIPInputStream inputStream = new GZIPInputStream(new ByteArrayInputStream(gzipped), Compressor.MTU);
            
            ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream(gzipped.length);
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
    public int compressLZ4(ByteBuffer src, int srcSize, ByteBuffer dst, int maxSize)
            throws HipoException {
        //System.out.println("----> compressing " + srcSize + " max size = " + maxSize);
        if (lz4_compressor.maxCompressedLength(srcSize) > maxSize) {
            throw new HipoException("maxSize (" + maxSize + ") is < max # of compressed bytes (" +
                                    lz4_compressor.maxCompressedLength(srcSize) + ")");
        }
        return lz4_compressor.compress(src.array(), 0, srcSize, dst.array(), 0, maxSize);
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
    public int compressLZ4(byte[] src, int srcOff, int srcSize,
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
    public int compressLZ4(ByteBuffer src, int srcOff, int srcSize,
                            ByteBuffer dst, int dstOff, int maxSize)
            throws HipoException {
        //System.out.println("----> compressing " + srcSize + " max size = " + maxSize);
        if (lz4_compressor.maxCompressedLength(srcSize) > maxSize) {
            throw new HipoException("maxSize (" + maxSize + ") is < max # of compressed bytes (" +
                                    lz4_compressor.maxCompressedLength(srcSize) + ")");
        }
        return lz4_compressor.compress(src, srcOff, srcSize, dst, dstOff, maxSize);
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
    public int compressLZ4Best(ByteBuffer src, int srcSize, ByteBuffer dst, int maxSize)
            throws HipoException {
        //System.out.println("----> compressing " + srcSize + " max size = " + maxSize);
        if (lz4_compressor_best.maxCompressedLength(srcSize) > maxSize) {
            throw new HipoException("maxSize (" + maxSize + ") is < max # of compressed bytes (" +
                                            lz4_compressor_best.maxCompressedLength(srcSize) + ")");
        }
        return lz4_compressor_best.compress(src.array(), 0, srcSize, dst.array(), 0, maxSize);
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
    public int compressLZ4Best(byte[] src, int srcOff, int srcSize,
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
    public int compressLZ4Best(ByteBuffer src, int srcOff, int srcSize,
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
     * @throws org.jlab.coda.hipo.HipoException if dst is too small to hold uncompressed data
     */
    public int uncompressLZ4(ByteBuffer src, int srcSize, ByteBuffer dst)
            throws HipoException {

        try {
            int i = lz4_decompressor.decompress(src.array(), 0, srcSize, dst.array(), 0);
            // Prepare buffer for reading
            dst.position(0).limit(i);
            return i;
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
    public int uncompressLZ4(byte[] src, int srcOff, int srcSize, byte[] dst, int dstOff)
            throws HipoException {

        try {
            return lz4_decompressor.decompress(src, srcOff, srcSize, dst, dstOff);
        }
        catch (LZ4Exception e) {
            throw new HipoException(e);
        }
    }

}
