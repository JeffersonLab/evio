package org.jlab.coda.jevio.test;

import org.jlab.coda.hipo.Compressor;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.zip.DataFormatException;
import java.util.zip.Deflater;
import java.util.zip.Inflater;



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


public class CompressionTest2 {

    /** For WRITING a local file. */
    public static void main(String args[]) {

        Compressor comp = Compressor.getInstance();

        System.out.println("Done getting Compressor");
    }

}
