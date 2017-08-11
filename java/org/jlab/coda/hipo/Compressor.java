/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package org.jlab.coda.hipo;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;

/**
 *
 * @author gavalian
 */
public class Compressor {
    
    public static final int MTU = 1024*1024;
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
}
