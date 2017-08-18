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
import net.jpountz.lz4.LZ4Factory;

/**
 *
 * @author gavalian
 */
public class Compressor {
    
    
    public static int         RECORD_UNCOMPRESSED = 0;
    public static int      RECORD_COMPRESSION_LZ4 = 1;
    public static int RECORD_COMPRESSION_LZ4_BEST = 2;
    public static int     RECORD_COMPRESSION_GZIP = 3;
    
    public static final int MTU = 1024*1024;
    
    LZ4Factory factory = null; 
    LZ4Compressor lz4_compressor = null;
    
    public Compressor(){
        factory = LZ4Factory.fastestInstance();
        lz4_compressor = factory.fastCompressor();
    }
    
    /**
     * returns compressed buffer. depends on compression type.
     * @param compressionType type of compression
     * @param buffer uncompressed buffer
     * @return compressed buffer
     */
    public static byte[] getCompressedBuffer(int compressionType, byte[] buffer){
        if(compressionType==1){
            return Compressor.compressGZIP(buffer);
        }
        return buffer;
    }
    /**
     * return un compressed buffer.
     * @param compressionType compression type 
     * @param compressedBuffer compressed buffer
     * @return uncompressed buffer
     */
    public static byte[] getUnCompressedBuffer(int compressionType, byte[] compressedBuffer){
        if(compressionType==1){
            return Compressor.uncompressGZIP(compressedBuffer);
        }
        return compressedBuffer;
    }
    /**
     * GZIP compression. Returns compressed byte array.
     * @param ungzipped
     * @return 
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
    
    public static byte[] uncompressGZIP(byte[] gzipped){
        byte[] ungzipped = new byte[0];
        int    internalBufferSize  = 1*1024*1024;
        try {
            final GZIPInputStream inputStream = new GZIPInputStream(new ByteArrayInputStream(gzipped),1024*1024);
            
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
    
    public int compressLZ4(ByteBuffer src, int srcSize, ByteBuffer dst, int maxSize){
        //System.out.println("----> compressing " + srcSize + " max size = " + maxSize);
        int compressedLength = lz4_compressor.compress(src.array(), 0, 
                srcSize, dst.array(), 0, maxSize);
        return compressedLength;
    }
}
