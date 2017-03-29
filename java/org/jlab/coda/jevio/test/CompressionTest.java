package org.jlab.coda.jevio.test;

import org.jlab.coda.jevio.ByteDataTransformer;
import org.jlab.coda.jevio.EvioByteArrayOutputStream;
import org.jlab.coda.jevio.EvioException;

import java.io.*;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.nio.channels.FileChannel;
import java.util.Scanner;
import java.util.zip.*;



// You can determine that it is likely to be one of those formats
// by looking at the first few bytes. You should then test to see
// if it really is one of those, using an integrity check from the
// associated utility for that format, or by actually proceeding to
// decompress.
//
// You can find the header formats in the descriptions:
//
//    Zip (.zip) format description, starts with 0x50, 0x4b, 0x03, 0x04
//              (unless empty ? then the last two are 0x05, 0x06 or 0x06, 0x06)
//    Gzip (.gz) format description, starts with 0x1f, 0x8b, 0x08
//    xz (.xz) format description, starts with 0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00
//
// Others:
//
//    zlib (.zz) format description, starts with (in bits) 0aaa1000 bbbccccc,
//               where ccccc is chosen so that the first byte times 256 plus
//               the second byte is a multiple of 31.
//    compress (.Z) starts with 0x1f, 0x9d
//    bzip2 (.bz2) starts with 0x42, 0x5a, 0x68




//0   1
//+---+---+
//|CMF|FLG|
//+---+---+
//
//CMF (Compression Method and flags) This byte is divided into a 4-bit compression method and a 4- bit information field depending on the compression method.
//
//bits 0 to 3  CM     Compression method
//bits 4 to 7  CINFO  Compression info
//
//CM (Compression method) This identifies the compression method used in the file. CM = 8 denotes the "deflate" compression method with a window size up to 32K. This is the method used by gzip and PNG and almost everything else. CM = 15 is reserved.
//
//CINFO (Compression info) For CM = 8, CINFO is the base-2 logarithm of the LZ77 window size, minus eight (CINFO=7 indicates a 32K window size). Values of CINFO above 7 are not allowed in this version of the specification. CINFO is not defined in this specification for CM not equal to 8.
//
//In practice, this means the first byte is almost always 78 (hex)
//
//FLG (FLaGs) This flag byte is divided as follows:
//
//bits 0 to 4  FCHECK  (check bits for CMF and FLG)
//bit  5       FDICT   (preset dictionary)
//bits 6 to 7  FLEVEL  (compression level)
//
//The FCHECK value must be such that CMF and FLG, when viewed as a 16-bit unsigned integer stored in MSB order (CMF*256 + FLG), is a multiple of 31.
//
//FLEVEL (Compression level) These flags are available for use by specific compression methods. The "deflate" method (CM = 8) sets these flags as follows:
//
//        0 - compressor used fastest algorithm
//        1 - compressor used fast algorithm
//        2 - compressor used default algorithm
//        3 - compressor used maximum compression, slowest algorithm
//


//
//
//zlib magic headers
//
//78 01 - No Compression/low
//78 9C - Default Compression
//78 DA - Best Compression
//
//
// This helped me figure out what type of compression I was dealing with.
// I knew the file was compressed, but was doing searches for some header
// bytes and this came up. Thanks! ? ProVega Feb 1 '14 at 5:41
//
// When using the Java Inflator (uses ZLIB) I'm seeing header values of 120, -100.
// This equates to 78 9C. Backs up what you said above. ? Dan Oct 2 '14 at 21:33


//
//
//Following is the Zlib compressed data format.
//
// +---+---+
// |CMF|FLG| (2 bytes. Defines the compression mode)
// +---+---+
// +---+---+---+---+
// |     DICTID    | (4 bytes. Present only when FLG.FDICT is set.) - Mostly not set
// +---+---+---+---+
// +=====================+
// |...compressed data...| (variable size of data)
// +=====================+
// +---+---+---+---+
// |     ADLER32   |  (4 bytes of checksum)
// +---+---+---+---+
//
//The header values(CMF and FLG) with no dictionary(mostly there will be no dictionary) are defined as follows.
//
//CMF  FLG
//78   01 - No Compression/low
//78   9C - Default Compression
//78   DA - Best Compression
//


//
//
//All answers here are most probably correct, however - if you want to manipulate ZLib compression stream directly, and it was produced by using gz_open, gzwrite, gzclose functions - then there is extra 10 leading bytes header before zlib compression steam comes - and those are produced by function gz_open - header looks like this:
//
//    fprintf(s->file, "%c%c%c%c%c%c%c%c%c%c", gz_magic[0], gz_magic[1],
//         Z_DEFLATED, 0 /*flags*/, 0,0,0,0 /*time*/, 0 /*xflags*/, OS_CODE);
//
//And results in following hex dump: 1F 8B 08 00 00 00 00 00 00 0B followed by zlib compression stream.
//
//But there is also trailing 8 bytes - they are uLong - crc over whole file, uLong - uncompressed file size - look for following bytes at end of stream:
//
//    putLong (s->file, s->crc);
//    putLong (s->file, (uLong)(s->in & 0xffffffff));
//


/**
 * Test program.
 * @author timmer
 * Date: Oct 21, 2013
 */


public class CompressionTest {

    // ZLIB format compression/decompression

    public static byte[] compress(byte[] data) throws IOException {
        Deflater deflater = new Deflater();
        deflater.setInput(data);
        deflater.finish();

        ByteArrayOutputStream outputStream = new ByteArrayOutputStream(data.length);
        byte[] buffer = new byte[1024];

        while (!deflater.finished()) {
            int count = deflater.deflate(buffer); // returns the generated code... index
            outputStream.write(buffer, 0, count);
        }

        outputStream.close();

        byte[] output = outputStream.toByteArray();

        System.out.println("Original: " + data.length + " Bytes");
        System.out.println("Compressed: " + output.length  + " Bytes");

        return output;
    }

    public static byte[] decompress(byte[] data) throws IOException, DataFormatException {
        Inflater inflater = new Inflater();
        inflater.setInput(data);

        ByteArrayOutputStream outputStream = new ByteArrayOutputStream(data.length);
        byte[] buffer = new byte[1024];

        while (!inflater.finished()) {
            int count = inflater.inflate(buffer);
            outputStream.write(buffer, 0, count);
        }

        outputStream.close();
        byte[] output = outputStream.toByteArray();

        System.out.println("Original: " + data.length);
        System.out.println("Compressed: " + output.length);

        return output;
    }


    /** For WRITING a local file. */
      public static void main(String args[]) {
          byte[] originalData = new byte[2048];
          for (int i=0; i < 2048; i++) {
              originalData[i] = (byte) (i % 256);
          }

          byte[] original = new byte[0];
          try {
              System.out.println("Call Deflater:");
              byte[] compressedData = compress(originalData);


              System.out.println("Call Inflater:");
              original = decompress(compressedData);
          }
          catch (IOException e) {
              e.printStackTrace();
          }
          catch (DataFormatException e) {
              e.printStackTrace();
          }

          for (int i = 0; i < 30; i++) {
              System.out.println("data[" + i + "] = " + original[i]);
          }

          System.out.println("Done compressing and decompressing data");
      }

    }



/**
 * Example program to demonstrate how to use zlib compression with
 * Java.
 * Inspired by http://stackoverflow.com/q/6173920/600500.
 */
class ZlibCompression {

    /**
     * Compresses a file with zlib compression.
     */
    public static void compressFile(File raw, File compressed)
        throws IOException {

        InputStream in = new FileInputStream(raw);
        OutputStream out = new DeflaterOutputStream(new FileOutputStream(compressed));
        shovelInToOut(in, out);
        in.close();
        out.close();
    }

    /**
     * Decompresses a zlib compressed file.
     */
    public static void decompressFile(File compressed, File raw)
        throws IOException {

        InputStream in = new InflaterInputStream(new FileInputStream(compressed));
        OutputStream out = new FileOutputStream(raw);
        shovelInToOut(in, out);
        in.close();
        out.close();
    }

    /**
     * Shovels all data from an input stream to an output stream.
     */
    private static void shovelInToOut(InputStream in, OutputStream out)
        throws IOException  {

        byte[] buffer = new byte[1000];
        int len;
        while((len = in.read(buffer)) > 0) {
            out.write(buffer, 0, len);
        }
    }


    /**
     * Main method to test it all.
     */
    public static void main(String[] args) throws IOException, DataFormatException {
        File compressed = new File("book1out.dfl");
        compressFile(new File("book1"), compressed);
        decompressFile(compressed, new File("decompressed.txt"));
    }
}


class ZLibCompression2 {
    public static void compress(File raw, File compressed) throws IOException  {
        try (InputStream inputStream = new FileInputStream(raw);
             OutputStream outputStream = new DeflaterOutputStream(new FileOutputStream(compressed))) {
            copy(inputStream, outputStream);
        }
    }

    public static void decompress(File compressed, File raw)
            throws IOException {
        try (InputStream inputStream = new InflaterInputStream(new FileInputStream(compressed));
             OutputStream outputStream = new FileOutputStream(raw)) {
            copy(inputStream, outputStream);
        }
    }

    public static String decompress(File compressed) throws IOException {
        try (InputStream inputStream = new InflaterInputStream(new FileInputStream(compressed))) {
            return toString(inputStream);
        }
    }

    private static String toString(InputStream inputStream) {
        try (Scanner scanner = new Scanner(inputStream).useDelimiter("\\A")) {
            return scanner.hasNext() ? scanner.next() : "";
        }
    }

    private static void copy(InputStream inputStream, OutputStream outputStream)
            throws IOException {
        byte[] buffer = new byte[1000];
        int length;

        while ((length = inputStream.read(buffer)) > 0) {
            outputStream.write(buffer, 0, length);
        }
    }


}

//////////////////////////////
// GZIP
//////////////////////////////

/**
 * Example program to demonstrate how to use gzip compression with
 * Java.
 * Inspired by http://stackoverflow.com/q/6173920/600500.
 */
class GzipCompression {

    /**
     * Compresses a file with zlib compression.
     */
    public static void compressFile(File raw, File compressed)
        throws IOException {

        InputStream in = new FileInputStream(raw);
        OutputStream out = new GZIPOutputStream(new FileOutputStream(compressed));
        shovelInToOut(in, out);
        in.close();
        out.close();
    }

    /**
     * Decompresses a zlib compressed file.
     */
    public static void decompressFile(File compressed, File raw)
        throws IOException {

        InputStream in = new GZIPInputStream(new FileInputStream(compressed));
        OutputStream out = new FileOutputStream(raw);
        shovelInToOut(in, out);
        in.close();
        out.close();
    }

    /**
     * Shovels all data from an input stream to an output stream.
     */
    private static void shovelInToOut(InputStream in, OutputStream out)
        throws IOException  {

        byte[] buffer = new byte[1000];
        int len;
        while((len = in.read(buffer)) > 0) {
            out.write(buffer, 0, len);
        }
    }


    /**
     * Main method to test it all.
     */
    public static void main(String[] args) throws IOException, DataFormatException {
        File compressed = new File("book1out.dfl");
        compressFile(new File("book1"), compressed);
        decompressFile(compressed, new File("decompressed.txt"));
    }
}


// Use try-with-resources

class GzipCompression2 {
    public static void compress(File raw, File compressed) throws IOException  {
        try (InputStream inputStream = new FileInputStream(raw);
             OutputStream outputStream = new GZIPOutputStream(new FileOutputStream(compressed))) {
            copy(inputStream, outputStream);
        }
    }

    public static void decompress(File compressed, File raw)
            throws IOException {
        try (InputStream inputStream = new GZIPInputStream(new FileInputStream(compressed));
             OutputStream outputStream = new FileOutputStream(raw)) {
            copy(inputStream, outputStream);
        }
    }

//    public static String decompress(File compressed) throws IOException {
//        try (InputStream inputStream = new GZIPInputStream(new FileInputStream(compressed))) {
//            return toString(inputStream);
//        }
//    }
//
//    private static String toString(InputStream inputStream) {
//        try (Scanner scanner = new Scanner(inputStream).useDelimiter("\\A")) {
//            return scanner.hasNext() ? scanner.next() : "";
//        }
//    }

    private static void copy(InputStream inputStream, OutputStream outputStream)
            throws IOException {
        byte[] buffer = new byte[1000];
        int length;

        while ((length = inputStream.read(buffer)) > 0) {
            outputStream.write(buffer, 0, length);
        }
    }


}



class GzipCompression3 {
    /** Stream used to hold compressed data. */
    EvioByteArrayOutputStream byteArrayOut;

    /** Stream used to compress data. */
    GZIPOutputStream gzipOut;

    ByteBuffer buffer = ByteBuffer.allocate(201);

    public GzipCompression3() {
        byteArrayOut = new EvioByteArrayOutputStream(buffer.capacity() + 101);
        try {
            gzipOut = new GZIPOutputStream(byteArrayOut);
        }
        catch (IOException e) {
            e.printStackTrace();
        }

    }

    public void writeFile() throws IOException {
        // This data does NOT compress at all
        for (int i=0; i < buffer.capacity(); i++) {
//System.out.println("Writing " + ((byte)i));
            buffer.put((byte)i);
        }
        buffer.flip();

        int bytesWritten = buffer.remaining();
        System.out.println("Writing " + bytesWritten + " uncompressed bytes");
        ByteBuffer compressedBuf;

        if (buffer.hasArray()) {
            gzipOut.write(buffer.array(),
                          buffer.arrayOffset() + buffer.position(),
                          bytesWritten);
        }
        else {
            while (buffer.hasRemaining()) {
                gzipOut.write(buffer.get());
            }
        }

        gzipOut.finish();
        gzipOut.close();
        byteArrayOut.close();
        compressedBuf = byteArrayOut.getByteBuffer();

        System.out.println("Compressed buf: cap = " + compressedBuf.capacity() +
                           ", pos = " + compressedBuf.position() + ", lim = " + compressedBuf.limit());

        /** The object used for writing a file. */
        File currentFile = new File("/scratch/timmer/myDataFile5");
        RandomAccessFile raf = new RandomAccessFile(currentFile, "rw");

        /** The file channel, used for writing a file, derived from raf. */
        FileChannel fileChannel = raf.getChannel();

        // Write everything in internal buffer out to file
        while (compressedBuf.hasRemaining()) {
            fileChannel.write(compressedBuf);
        }
        fileChannel.close();

        System.out.println("\nFINISH Gzip output and close stream\n");
    }

    public void readAndPrintFile() throws IOException {

        File inFile = new File("/scratch/timmer/myDataFile5");
        int fileLength = (int)inFile.length();

        InputStream in = new GZIPInputStream(new FileInputStream(inFile));

        byte[] buffer = new byte[fileLength];
        int len;
        while ((len = in.read(buffer)) > 0) {
            for (int i=0; i < len; i++) {
                if (i%20 == 0) {
                    System.out.print("\n  Buf(" + (i + 1) + "-" + (i + 20) + ") =  ");
                }
                else if (i%4 == 0) {
                    System.out.print("  ");
                }

                System.out.print(String.format("%02x", buffer[i]) + " ");
            }
            System.out.println();
            System.out.println();
        }

        in.close();
    }


    public static void main(String[] args) throws IOException, DataFormatException {
        GzipCompression3 comp = new GzipCompression3();
        comp.writeFile();
        comp.readAndPrintFile();

    }


}




class GzipCompression4 {
    /** Stream used to hold compressed data. */
    EvioByteArrayOutputStream byteArrayOut;

    /** Stream used to compress data. */
    GZIPOutputStream gzipOut;

    ByteBuffer buffer;
    int[] intData;


    public GzipCompression4() {
        // This data compresses well
        intData = new int[50];
        for (int i=0; i < intData.length; i++) {
            intData[i] = i;
        }

        byte[] bytes = new byte[0];
        try {
            bytes = ByteDataTransformer.toBytes(intData, ByteOrder.BIG_ENDIAN);
        }
        catch (EvioException e) {
            e.printStackTrace();
        }
        buffer = ByteBuffer.wrap(bytes);

        byteArrayOut = new EvioByteArrayOutputStream(buffer.capacity() + 101);
        try {
            gzipOut = new GZIPOutputStream(byteArrayOut);
        }
        catch (IOException e) {
            e.printStackTrace();
        }

    }

    public void writeFile() throws IOException {

        int bytesWritten = buffer.remaining();
        System.out.println("Writing " + bytesWritten + " uncompressed bytes");
        ByteBuffer compressedBuf;

        if (buffer.hasArray()) {
            gzipOut.write(buffer.array(),
                          buffer.arrayOffset() + buffer.position(),
                          bytesWritten);
        }
        else {
            while (buffer.hasRemaining()) {
                gzipOut.write(buffer.get());
            }
        }

        gzipOut.finish();
        gzipOut.close();
        byteArrayOut.close();
        compressedBuf = byteArrayOut.getByteBuffer();

        System.out.println("Compressed buf: cap = " + compressedBuf.capacity() +
                           ", pos = " + compressedBuf.position() + ", lim = " + compressedBuf.limit());

        /** The object used for writing a file. */
        File currentFile = new File("/scratch/timmer/myIntDataFile");
        RandomAccessFile raf = new RandomAccessFile(currentFile, "rw");

        /** The file channel, used for writing a file, derived from raf. */
        FileChannel fileChannel = raf.getChannel();

        // Write everything in internal buffer out to file
        while (compressedBuf.hasRemaining()) {
            fileChannel.write(compressedBuf);
        }
        fileChannel.close();

        System.out.println("\nFINISH Gzip output and close stream\n");
    }

    public void readAndPrintFile() throws IOException {

        File inFile = new File("/scratch/timmer/myIntDataFile");
        int fileLength = (int)inFile.length();

        InputStream in = new GZIPInputStream(new FileInputStream(inFile));

        byte[] buf = new byte[2*fileLength + 1024];
        int len;
        while ((len = in.read(buf)) > 0) {

            int indx, intArrayLen = len/4;
            int[] ints = new int[intArrayLen];
            for (int i = 0; i < intArrayLen; i++) {
                indx = i*4;
                ints[i] = ByteDataTransformer.toInt(buf[indx],
                                                    buf[indx+1],
                                                    buf[indx+2],
                                                    buf[indx+3],
                                                    ByteOrder.BIG_ENDIAN);
            }

            for (int i=0; i < intArrayLen; i++) {
                if (i%5 == 0) {
                    System.out.print("\n  Buf(" + (i + 1) + "-" + (i + 5) + ") =  ");
                }

                System.out.print(String.format("%08x", ints[i]) + "  ");
            }

            System.out.println();
            System.out.println();
        }

        in.close();
    }


    public static void main(String[] args) throws IOException, DataFormatException {
        GzipCompression4 comp = new GzipCompression4();
        comp.writeFile();
        comp.readAndPrintFile();

    }


}
