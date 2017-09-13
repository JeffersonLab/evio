/*
 *   Copyright (c) 2016.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.hipo;

import org.jlab.coda.jevio.ByteDataTransformer;
import org.jlab.coda.jevio.EvioException;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Generic HEADER class to compose a record header or file header
 * with given buffer sizes and paddings and compression types.<p>
 *
 * <pre>
 * GENERAL RECORD HEADER STRUCTURE ( 56 bytes, 14 integers (32 bit) )
 *
 *    +----------------------------------+
 *  1 |         Record Length            | // 32bit words, inclusive
 *    +----------------------------------+
 *  2 +         Record Number            |
 *    +----------------------------------+
 *  3 +         Header Length            | // 14 (words)
 *    +----------------------------------+
 *  4 +       Event (Index) Count        |
 *    +----------------------------------+
 *  5 +      Index Array Length          | // bytes
 *    +-----------------------+---------+
 *  6 +       Bit Info        | Version  | // version (8 bits)
 *    +-----------------------+----------+
 *  7 +      User Header Length          | // bytes
 *    +----------------------------------+
 *  8 +          Magic Number            | // 0xc0da0100
 *    +----------------------------------+
 *  9 +     Uncompressed Data Length     | // bytes
 *    +------+---------------------------+
 * 10 +  CT  |  Data Length Compressed   | // CT = compression type (4 bits); compressed len in words
 *    +----------------------------------+
 * 11 +          User Register 1         | // UID 1st (64 bits)
 *    +--                              --+
 * 12 +                                  |
 *    +----------------------------------+
 * 13 +          User Register 2         | // UID 2nd (64 bits)
 *    +--                              --+
 * 14 +                                  |
 *    +----------------------------------+
 *
 * -------------------
 *   Compression Type
 * -------------------
 *     0  = none
 *     1  = LZ4 fastest
 *     2  = LZ4 best
 *     3  = gzip
 *
 * -------------------
 *   Bit Info Word
 * -------------------
 *     0-7  = version
 *     8    = true if dictionary is included (relevant for first record only)
 *     9    = true if this record is the last in file or stream
 *    10-13 = type of events contained: 0 = ROC Raw,
 *                                      1 = Physics
 *                                      2 = PartialPhysics
 *                                      3 = DisentangledPhysics
 *                                      4 = User
 *                                      5 = Control
 *                                     15 = Other
 *    14-19 = reserved
 *    20-21 = pad 1
 *    22-23 = pad 2
 *    24-25 = pad 3
 *    26-27 = reserved
 *    28-31 = general header type: 0 = Evio record,
 *                                 3 = Evio file trailer
 *                                 4 = HIPO record,
 *                                 7 = HIPO file trailer
 *
 *
 *
 * ------------------------------------------------------------
 * ------------------------------------------------------------
 *
 * FILE HEADER STRUCTURE ( 56 bytes, 14 integers (32 bit) )
 *
 *    +----------------------------------+
 *  1 |              ID                  | // HIPO: 0x43455248, Evio: 0x4556494F
 *    +----------------------------------+
 *  2 +          File Number             | // split file #
 *    +----------------------------------+
 *  3 +         Header Length            | // 14 (words)
 *    +----------------------------------+
 *  4 +      Record (Index) Count        |
 *    +----------------------------------+
 *  5 +      Index Array Length          | // bytes
 *    +-----------------------+---------+
 *  6 +       Bit Info        | Version  | // version (8 bits)
 *    +-----------------------+----------+
 *  7 +      User Header Length          | // bytes
 *    +----------------------------------+
 *  8 +          Magic Number            | // 0xc0da0100
 *    +----------------------------------+
 *  9 +     Uncompressed Data Length     | // 0 (bytes)
 *    +------+---------------------------+
 * 10 +  CT  |  Data Length Compressed   | // CT = compression type (4 bits); compressed len in words
 *    +----------------------------------+
 * 11 +         Trailer Position         | // File offset to trailer head (64 bits).
 *    +--                              --+ // 0 = no offset available or no trailer exists.
 * 12 +                                  |
 *    +----------------------------------+
 * 13 +          User Integer 1          |
 *    +----------------------------------+
 * 14 +          User Integer 2          |
 *    +----------------------------------+
 *
 * -------------------
 *   Bit Info Word
 * -------------------
 *     0-7  = version
 *     8    = true if dictionary is included (relevant for first record only)
 *     9    = true if this file has "first" event (in every split file)
 *    10    = File trailer with index array exists
 *    11-19 = reserved
 *    20-21 = pad 1
 *    22-23 = pad 2
 *    24-25 = pad 3 (always 0)
 *    26-27 = reserved
 *    28-31 = general header type: 1 = Evio file
 *                                 2 = Evio extended file
 *                                 5 = HIPO file
 *                                 6 = HIPO extended file
 *
 * </pre>
 *
 * @version 6.0
 * @since 6.0 9/6/17
 * @author gavalian
 * @author timmer
 */
public class RecordHeader {

    /** Array to help find number of bytes to pad data. */
    private final static int[] padValue = {0,3,2,1};
    /** First word in every HIPO file for identification purposes. */
    final static int   HIPO_FILE_UNIQUE_WORD = 0x4F504948; // 0x4849504F = HIPO
    /** First word in every Evio file for identification purposes. */
    final static int   EVIO_FILE_UNIQUE_WORD = 0x4556494F; // = EVIO
    /** Number of 32-bit words in a normal sized header. */
    final static int   HEADER_SIZE_WORDS = 14;
    /** Number of bytes in a normal sized header. */
    final static int   HEADER_SIZE_BYTES = 56;
    /** Magic number used to track endianness. */
    final static int   HEADER_MAGIC = 0xc0da0100;
    /** Magic number for HIPO's little endian uses. */
    final static int   HEADER_MAGIC_LE = HEADER_MAGIC;
    /** Magic number for HIPO's big endian uses (byte swapped from HEADER_MAGIC_LE). */
    final static int   HEADER_MAGIC_BE = Integer.reverseBytes(HEADER_MAGIC);
    /** Number of bytes from beginning of file header to write trailer position. */
    final static long  TRAILER_POSITION_OFFSET = 40L;
    /** 9th bit set in bitInfo word in record header means record is last in stream or file. */
    final static int   LAST_RECORD_BIT = 0x100;
    /** 10th bit set in bitInfo word in file header means file trailer with index array exists. */
    final static int   TRAILER_WITH_INDEX = 0x200;


    /** Type of header this is. Normal evio record by default. */
    private HeaderType headerType = HeaderType.EVIO_RECORD;

    // File header only members

    /** File id for file identification purposes. Defaults to evio file.
      * Only valid if file header. 1st word. */
    private int  fileId = EVIO_FILE_UNIQUE_WORD;
    /** File number or split file number, starting at 1. Only valid if file header. 2nd word. */
    private int  fileNumber = 1;
    /** Position of trailing header from start of file in bytes. Only valid if file header. 11th word. */
    private long trailerPosition;
    /** First user-defined integer in file header. Only valid if file header. 13th word. */
    private int  userIntFirst;
    /** Second user-defined integer in file header. Only valid if file header. 14th word. */
    private int  userIntSecond;

    // Record header members

    /** Position of this header in a file. */
    private long position;
    /** Length of the entire record this header is a part of (bytes). */
    private int  recordLength;
    /** Length of the entire record this header is a part of (32-bit words). 1st word. */
    private int  recordLengthWords;
    /** Record number. 2nd word. */
    private int  recordNumber;
    /** Event or index count. 4th word. */
    private int  entries;
    /** Length of this header (bytes). */
    private int  headerLength;
    /** Length of this header (words). 3rd word. */
    private int  headerLengthWords;
    /** Length of user-defined header (bytes). 7th word. */
    private int  userHeaderLength;
    /** Length of user-defined header when padded (words). 7th word. */
    private int  userHeaderLengthWords;
    /** Length of index array (bytes). 5th word. */
    private int  indexLength;
    /** Uncompressed data length (bytes). 9th word. */
    private int  dataLength;
    /** Uncompressed data length when padded (words). */
    private int  dataLengthWords;
    /** Compressed data length (bytes). */
    private int  compressedDataLength;
    /** Compressed data length (words) when padded. Lowest 28 bits of 10th word. */
    private int  compressedDataLengthWords;
    /** Type of data compression (0=none, 1=LZ4 fast, 2=LZ4 best, 3=gzip).
      * Highest 4 bits of 10th word. */
    private int  compressionType;
    /** First user-defined 64-bit register. 11th and 12th words. */
    private long  recordUserRegisterFirst;
    /** Second user-defined 64-bit register. 13th and 14th words. */
    private long recordUserRegisterSecond;
    /** Evio format version number. Lowest byte of 6th word. */
    private int  headerVersion = 6;
    /** Magic number for tracking endianness. 8th word. */
    private int  headerMagicWord = HEADER_MAGIC;

    // These quantities are updated automatically when lengths are set

    /** Number of bytes required to bring uncompressed
      * user header to 4-byte boundary. Stored in 6th word. */
    private int  userHeaderLengthPadding;
    /** Number of bytes required to bring uncompressed
      * data to 4-byte boundary. Stored in 6th word. */
    private int  dataLengthPadding;
    /** Number of bytes required to bring compressed
      * data to 4-byte boundary. Stored in 6th word. */
    private int  compressedDataLengthPadding;

    

    /** Default, no-arg constructor. */
    public RecordHeader() {
        setHeaderLength(HEADER_SIZE_BYTES);
    }

    /**
     * Constructor which sets the type of header this is.
     * @param type  type of header this is
     */
    public RecordHeader(HeaderType type) {
        setHeaderLength(HEADER_SIZE_BYTES);
        headerType = type;
        if (type.isEvioFileHeader()) {
            fileId = EVIO_FILE_UNIQUE_WORD;
        }
        else if (type.isHipoFileHeader()) {
            fileId = HIPO_FILE_UNIQUE_WORD;
        }
    }

    /**
     * Constructor.
     * @param _pos   position in file.
     * @param _l     length of record in bytes
     * @param _e     number of events
     */
    public RecordHeader(long _pos, int _l, int _e){
        position = _pos; recordLength = _l; entries = _e;
    }

    /** Reset internal variables. */
    public void reset(){

        fileId = EVIO_FILE_UNIQUE_WORD;
        fileNumber = 1;
        trailerPosition = 0L;
        userIntFirst  = 0;
        userIntSecond = 0;

        position = 0L;
        recordLength = 0;
        recordNumber = 0;
        entries = 0;

        headerLength = HEADER_SIZE_BYTES;
        userHeaderLength = 0;
        indexLength = 0;
        dataLength = 0;
        compressedDataLength = 0;
        compressionType = 0;

        userHeaderLengthPadding = 0;
        dataLengthPadding = 0;
        compressedDataLengthPadding = 0;

        dataLengthWords = 0;
        compressedDataLengthWords = 0;
        userHeaderLengthWords = 0;
        recordLengthWords = 0;
        headerLengthWords = HEADER_SIZE_WORDS;

        recordUserRegisterFirst = 0L;
        recordUserRegisterSecond = 0L;
    }

    /**
     * Returns length padded to 4-byte boundary for given length in bytes.
     * @param length length in bytes.
     * @return length in bytes padded to 4-byte boundary.
     */
    private static int getWords(int length){
        int words = length/4;
        if (getPadding(length) > 0) words++;
        return words;
    }

    /**
     * Returns number of bytes needed to pad to 4-byte boundary for the given length.
     * @param length length in bytes.
     * @return number of bytes needed to pad to 4-byte boundary.
     */
    private static int getPadding(int length) {return padValue[length%4];}

    //-----------------------
    // File header related
    //-----------------------

    /**
     * Get the file number or split number. Valid only if this is a file header.
     * @return file number.
     */
    public int  getFileNumber() {return fileNumber;}

    /**
     * Get the file id. Valid only if this is a file header.
     * @return file id.
     */
    public int  getFileId() {return fileId;}

    /**
     * Get the trailer's (trailing header's) file position in bytes.
     * Valid only if this is a file header.
     * @return trailer's (trailing header's) file position in bytes.
     */
    public long  getTrailerPosition() {return trailerPosition;}

    /**
     * Get the first user integer value. Valid only if this is a file header.
     * @return first user integer value.
     */
    public int  getUserIntFirst() {return userIntFirst;}

    /**
     * Get the second user integer value. Valid only if this is a file header.
     * @return second user integer value.
     */
    public int  getUserIntSecond() {return userIntSecond;}

    /**
     * Get the file number which is the split number starting at 1. Valid only if this is a file header.
     * @return file number starting at 1.
     */
    public RecordHeader  setFileNumber(int num) {fileNumber = num; return this;}

    /**
     * Get the first user integer. Valid only if this is a file header.
     * @return first user integer.
     */
    public RecordHeader  setUserIntFirst(int val) {userIntFirst  = val; return this;}

    /**
     * Get the second user integer. Valid only if this is a file header.
     * @return second user integer.
     */
    public RecordHeader  setUserIntSecond(int val){userIntSecond = val; return this;}

    //-----------------------
    // Record header related
    //-----------------------

    // Getters

    /**
     * Get the position of this record in a file.
     * @return position of this record in a file.
     */
    public long  getPosition() {return position;}

    /**
     * Get the total length of this record in bytes.
     * @return total length of this record in bytes.
     */
    public int  getLength() {return recordLength;}

    /**
     * Get the number of events or entries in index.
     * @return number of events or entries in index.
     */
    public int  getEntries() {return entries;}

    /**
     * Get the type of compression used. 0=none, 1=LZ4 fast, 2=LZ4 best, 3=gzip.
     * @return type of compression used.
     */
    public int  getCompressionType() {return compressionType;}

    /**
     * Get the length of the user-defined header in bytes.
     * @return length of the user-defined header in bytes.
     */
    public int  getUserHeaderLength() {return userHeaderLength;}

    /**
     * Get the length of the user-defined header in words.
     * @return length of the user-defined header in words.
     */
    public int  getUserHeaderLengthWords() {return userHeaderLengthWords;}

    /**
     * Get the Evio format version number.
     * @return Evio format version number.
     */
    public int  getVersion() {return headerVersion;}

    /**
     * Get the length of the uncompressed data in bytes.
     * @return length of the uncompressed data in bytes.
     */
    public int  getDataLength() {return dataLength;}

    /**
     * Get the length of the uncompressed data in words (padded).
     * @return length of the uncompressed data in words (padded).
     */
    public int  getDataLengthWords() {return dataLengthWords;}

    /**
     * Get the length of the index array in bytes.
     * @return length of the index array in bytes.
     */
    public int  getIndexLength() {return indexLength;}

    /**
     * Get the length of the compressed data in bytes.
     * @return length of the compressed data in bytes.
     */
    public int  getCompressedDataLength() {return compressedDataLength;}

    /**
     * Get the length of the compressed data in words (padded).
     * @return length of the compressed data in words (padded).
     */
    public int  getCompressedDataLengthWords() {return compressedDataLengthWords;}

    /**
     * Get the length of this header data in bytes.
     * @return length of this header data in bytes.
     */
    public int  getHeaderLength() {return headerLength;}

    /**
     * Get the record number.
     * @return record number.
     */
    public int  getRecordNumber() {return recordNumber;}

    /**
     * Get the first user-defined 64-bit register.
     * @return first user-defined 64-bit register.
     */
    public long  getUserRegisterFirst()  {return recordUserRegisterFirst;}

    /**
     * Get the second user-defined 64-bit register.
     * @return second user-defined 64-bit register.
     */
    public long  getUserRegisterSecond() {return recordUserRegisterSecond;}

    /**
     * Get the type of header this is.
     * @return type of header this is.
     */
    public HeaderType getHeaderType() {return headerType;}

    /**
     * Get the bit info word.
     * @return bit info word.
     */
    private int getBitInfoWord() {
        return  (headerType.getValue() << 28) |
                (compressedDataLengthPadding << 24) |
                (dataLengthPadding << 22) |
                (userHeaderLengthPadding << 20) |
                (headerVersion & 0xFF);
    }

    // Setters

    /**
     * Set this header's type. Normally done in constructor. Limited access.
     * @param type type of header.
     * @return this object.
     */
    RecordHeader  setHeaderType(HeaderType type)  {headerType = type; return this;}

    /**
     * Set the position of this record in a file.
     * @param pos position of this record in a file.
     * @return this object.
     */
    public RecordHeader  setPosition(long pos) {position = pos; return this;}

    /**
     * Set the record number.
     * @param num record number.
     * @return this object.
     */
    public RecordHeader  setRecordNumber(int num) {recordNumber = num; return this;}

    /**
     * Set the record length in bytes & words.
     * If length is not a multiple of 4, you're on your own!
     * @param length  length of record in bytes.
     * @return this object.
     */
    public RecordHeader  setLength(int length) {
        recordLength        = length;
        recordLengthWords   = length/4;
        //System.out.println(" LENGTH = " + recordLength + "  WORDS = " + this.recordLengthWords
        //+ "  SIZE = " + recordLengthWords*4 );
        return this;
    }

    /**
     * Set the uncompressed data length in bytes & words and the padding.
     * @param length  length of uncompressed data in bytes.
     * @return this object.
     */
    public RecordHeader  setDataLength(int length) {
        dataLength = length;
        dataLengthWords = getWords(length);
        dataLengthPadding = getPadding(length);
        return this;
    }

    /**
     * Set the compressed data length in bytes & words and the padding.
     * @param length  length of compressed data in bytes.
     * @return this object.
     */
    public RecordHeader  setCompressedDataLength(int length) {
        compressedDataLength = length;
        compressedDataLengthWords = getWords(length);
        compressedDataLengthPadding = getPadding(length);
        return this;
    }

    /**
     * Set the length of the index array in bytes.
     * Length is forced to be a multiple of 4!
     * @param length  length of index array in bytes.
     * @return this object.
     */
    public RecordHeader  setIndexLength(int length) {indexLength = (length/4)*4; return this;}

    /**
     * Set the compression type. 0=none, 1=LZ4 fast, 2=LZ4 best, 3=gzip.
     * No compression for other values.
     * @param type compression type.
     * @return this object.
     */
    public RecordHeader  setCompressionType(int type) {
        // Set to no compression if arg out-of-bounds
        if (type < 0 || type > 3) {
            type = 0;
        }
        compressionType = type;
        return this;
    }

    /**
     * Set the number of events or index entries.
     * No compression for other values.
     * @param n number of events or index entries.
     * @return this object.
     */
    public RecordHeader  setEntries(int n) {entries = n; return this;}

    /**
     * Set the user-defined header's length in bytes & words and the padding.
     * @param length  user-defined header's length in bytes.
     * @return this object.
     */
    public RecordHeader  setUserHeaderLength(int length) {
        userHeaderLength = length;
        userHeaderLengthWords   = getWords(length);
        userHeaderLengthPadding = getPadding(length);
        return this;
    }

    /**
     * Set the this header's length in bytes & words.
     * If length is not a multiple of 4, you're on your own!
     * @param length  this header's length in bytes.
     * @return this object.
     */
    public RecordHeader  setHeaderLength(int length) {
        headerLength = length;
        headerLengthWords = length/4;
        return this;
    }

    /**
     * Set the first, 64-bit, user-defined register.
     * @param reg  first, 64-bit, user-defined register.
     * @return this object.
     */
    public RecordHeader setUserRegisterFirst(long reg) {recordUserRegisterFirst = reg; return this;}

    /**
     * Set the second, 64-bit, user-defined register.
     * @param reg  second, 64-bit, user-defined register.
     * @return this object.
     */
    public RecordHeader setUserRegisterSecond(long reg) {recordUserRegisterSecond = reg; return this;}

    //-------------------------------------------------

    /**
     * Writes this header into the given byte buffer.
     * @param buffer byte buffer to write header into.
     * @param offset position in buffer to begin writing.
     */
    public void writeHeader(ByteBuffer buffer, int offset){

        buffer.putInt( 0*4 + offset, recordLengthWords);
        buffer.putInt( 1*4 + offset, recordNumber);
        buffer.putInt( 2*4 + offset, headerLengthWords);
        buffer.putInt( 3*4 + offset, entries);
        buffer.putInt( 4*4 + offset, indexLength);
        buffer.putInt( 5*4 + offset, getBitInfoWord());
        buffer.putInt( 6*4 + offset, userHeaderLength);
        buffer.putInt( 7*4 + offset, headerMagicWord);

        int compressedWord = ( compressedDataLengthWords & 0x0FFFFFFF) |
                ((compressionType & 0x0000000F) << 28);
        buffer.putInt( 8*4 + offset, dataLength);
        buffer.putInt( 9*4 + offset, compressedWord);

        buffer.putLong( 10*4 + offset, recordUserRegisterFirst);
        buffer.putLong( 12*4 + offset, recordUserRegisterSecond);
    }

    /**
     * Writes this header into the given byte buffer starting at the beginning.
     * @param buffer byte buffer to write header into.
     */
    public void writeHeader(ByteBuffer buffer){
        writeHeader(buffer,0);
    }

    /**
     * Writes a trailer with an optional index array into the given byte array.
     * @param array byte array to write trailer into.
     * @param recordNumber record number of trailer.
     * @param order byte order of data to be written.
     * @param index array of record lengths to be written to trailer
     *              (must be multiple of 4 bytes). Null if no index array.
     */
    static public void writeTrailer(byte[] array, int recordNumber,
                                    ByteOrder order, byte[] index) {

        int indexLength = 0;
        int totalLength = HEADER_SIZE_BYTES;
        if (index != null) {
            indexLength = index.length;
            totalLength += indexLength;
        }

        // Check arg
        if (array == null || array.length < totalLength) {
            // TODO: ERROR
            return;
        }

        // TODO: the header type and "last record" bit are redundant
        int bitInfo = (HeaderType.EVIO_TRAILER.getValue() << 28) | LAST_RECORD_BIT | 6;

        try {
            // First the general header part
            ByteDataTransformer.toBytes(totalLength, order, array, 0*4);
            ByteDataTransformer.toBytes(recordNumber, order, array, 1*4);
            ByteDataTransformer.toBytes(HEADER_SIZE_WORDS, order, array, 2*4);
            ByteDataTransformer.toBytes(0, order, array, 3*4);
            ByteDataTransformer.toBytes(indexLength, order, array, 4*4);
            ByteDataTransformer.toBytes(bitInfo, order, array, 5*4);
            ByteDataTransformer.toBytes(0, order, array, 6*4);
            ByteDataTransformer.toBytes(HEADER_MAGIC, order, array, 7*4);
            ByteDataTransformer.toBytes(0, order, array, 8*4);
            ByteDataTransformer.toBytes(0, order, array, 9*4);

            ByteDataTransformer.toBytes(0L, order, array, 10*4);
            ByteDataTransformer.toBytes(0L, order, array, 12*4);

            // Second the index
            if (indexLength > 0) {
                System.arraycopy(index, 0, array, 14 * 4, indexLength);
            }
        }
        catch (EvioException e) {/* never happen */}
    }

   /**
     * Writes the file (not record!) header into the given byte buffer.
     * @param buffer byte buffer to write file header into.
     * @param offset position in buffer to begin writing.
     */
    public void writeFileHeader(ByteBuffer buffer, int offset){

        buffer.putInt( 0*4 + offset, fileId);
        buffer.putInt( 1*4 + offset, fileNumber);
        buffer.putInt( 2*4 + offset, headerLengthWords);
        buffer.putInt( 3*4 + offset, entries);
        buffer.putInt( 4*4 + offset, indexLength);
        buffer.putInt( 5*4 + offset, getBitInfoWord());
        buffer.putInt( 6*4 + offset, userHeaderLength);
        buffer.putInt( 7*4 + offset, headerMagicWord);

        int compressedWord = ( compressedDataLengthWords & 0x0FFFFFFF) |
                ((compressionType & 0x0000000F) << 28);
        buffer.putInt( 8*4 + offset, dataLength);
        buffer.putInt( 9*4 + offset, compressedWord);

        buffer.putLong( 10*4 + offset, trailerPosition);
        buffer.putInt ( 12*4 + offset, userIntFirst);
        buffer.putInt ( 13*4 + offset, userIntSecond);
    }

    /**
     * Writes the file (not record!) header into the given byte buffer starting at beginning.
     * @param buffer byte buffer to write file header into.
     */
    public void writeFileHeader(ByteBuffer buffer) {writeFileHeader(buffer,0);}

    /**
     * Reads the header information from a byte buffer and validates
     * it by checking the magic word (8th word). This magic word
     * also determines the byte order.
     *
     * @param buffer buffer to read from.
     * @param offset position of first word to be read.
     * @throws HipoException if buffer is not in the proper format or earlier than version 6
     */
    public void readHeader(ByteBuffer buffer, int offset) throws HipoException {

        // First read the magic word to establish endianness
        headerMagicWord = buffer.getInt( 7*4 + offset);

        // If it's NOT in the proper byte order ...
        if (headerMagicWord != HEADER_MAGIC_LE) {
            // If it needs to be switched ...
            if (headerMagicWord == HEADER_MAGIC_BE) {
                ByteOrder bufEndian = buffer.order();
                if (bufEndian == ByteOrder.BIG_ENDIAN) {
                    buffer.order(ByteOrder.LITTLE_ENDIAN);
                }
                else {
                    buffer.order(ByteOrder.BIG_ENDIAN);
                }
            }
            else {
                // ERROR condition, bad magic word
                throw new HipoException("buffer arg not in evio/hipo format");
            }
        }

        // Next look at the version #
        int bitInoWord = buffer.getInt(  5*4 + offset);
        int version  = (bitInoWord & 0xFF);
        if (version < headerVersion) {
            throw new HipoException("buffer is in evio format version " + version);
        }

        recordLengthWords = buffer.getInt(    0 + offset );
        recordLength      = 4*recordLengthWords;

        recordNumber      = buffer.getInt(  1*4 + offset );
        headerLengthWords = buffer.getInt(  2*4 + offset);
        setHeaderLength(4*headerLengthWords);
        entries           = buffer.getInt(  3*4 + offset);

        indexLength  = buffer.getInt( 4*4 + offset);
        setIndexLength(indexLength);

        userHeaderLength = buffer.getInt( 6*4 + offset);
        setUserHeaderLength(userHeaderLength);

        // uncompressed data length
        dataLength       = buffer.getInt( 8*4 + offset);
        setDataLength(dataLength);

        int compressionWord   = buffer.getInt( 9*4 + offset);
        compressionType      = (compressionWord >>> 28);
        compressedDataLength = (compressionWord & 0x0FFFFFFF);
        setCompressedDataLength(compressedDataLength);

        recordUserRegisterFirst  = buffer.getLong( 10*4 + offset);
        recordUserRegisterSecond = buffer.getLong( 12*4 + offset);
    }

    /**
     * Reads the header information from a byte buffer and validates
     * it by checking the magic word (8th word). This magic word
     * also determines the byte order.
     *
     * @param buffer buffer to read from starting at beginning.
     * @throws HipoException if buffer is not in the proper format or earlier than version 6
     */
    public void readHeader(ByteBuffer buffer) throws HipoException {
        readHeader(buffer,0);
    }

    /**
     * Reads the file header information from a byte buffer and validates
     * it by checking the magic word (8th word). This magic word
     * also determines the byte order.
     *
     * @param buffer buffer to read from.
     * @param offset position of first word to be read.
     * @throws HipoException if buffer is not in the proper format or earlier than version 6
     */
    public void readFileHeader(ByteBuffer buffer, int offset) throws HipoException {

        // First read the magic word to establish endianness
        headerMagicWord = buffer.getInt( 7*4 + offset);

        // If it's NOT in the proper byte order ...
        if (headerMagicWord != HEADER_MAGIC_LE) {
            // If it needs to be switched ...
            if (headerMagicWord == HEADER_MAGIC_BE) {
                ByteOrder bufEndian = buffer.order();
                if (bufEndian == ByteOrder.BIG_ENDIAN) {
                    buffer.order(ByteOrder.LITTLE_ENDIAN);
                }
                else {
                    buffer.order(ByteOrder.BIG_ENDIAN);
                }
            }
            else {
                // ERROR condition, bad magic word
                throw new HipoException("buffer arg not in evio/hipo format");
            }
        }

        // Next look at the version #
        int bitInoWord = buffer.getInt(  5*4 + offset);
        int version  = (bitInoWord & 0xFF);
        if (version < headerVersion) {
            throw new HipoException("buffer is in evio format version " + version);
        }

        fileId            = buffer.getInt(    0 + offset );
        fileNumber        = buffer.getInt(  1*4 + offset );
        headerLengthWords = buffer.getInt(  2*4 + offset);
        setHeaderLength(4*headerLengthWords);
        entries           = buffer.getInt(  3*4 + offset);

        indexLength  = buffer.getInt( 4*4 + offset);
        setIndexLength(indexLength);

        userHeaderLength = buffer.getInt( 6*4 + offset);
        setUserHeaderLength(userHeaderLength);

        dataLength       = buffer.getInt( 8*4 + offset);
        setDataLength(dataLength);

        int compressionWord   = buffer.getInt( 9*4 + offset);
        compressionType      = (compressionWord >>> 28);
        compressedDataLength = (compressionWord & 0x0FFFFFFF);
        setCompressedDataLength(compressedDataLength);

        trailerPosition  = buffer.getLong( 10*4 + offset);
        userIntFirst  = buffer.getInt( 12*4 + offset);
        userIntSecond = buffer.getInt( 13*4 + offset);
    }

    /**
     * Reads the file header information from a byte buffer and validates
     * it by checking the magic word (8th word). This magic word
     * also determines the byte order.
     *
     * @param buffer buffer to read from starting at the beginning.
     * @throws HipoException if buffer is not in the proper format or earlier than version 6
     */
    public void readFileHeader(ByteBuffer buffer) throws HipoException {
        readHeader(buffer,0);
    }

    /**
     * Returns a string representation of the record.
     * @return a string representation of the record.
     */
    @Override
    public String toString(){

        StringBuilder str = new StringBuilder();
        str.append(String.format("%24s : %d\n","version",headerVersion));
        str.append(String.format("%24s : %d\n","record #",recordNumber));
        str.append(String.format("%24s : %8d / %8d / %8d\n","user header length",
                                 userHeaderLength, userHeaderLengthWords, userHeaderLengthPadding));
        str.append(String.format("%24s : %8d / %8d / %8d\n","   data length",
                                 dataLength, dataLengthWords, dataLengthPadding));
        str.append(String.format("%24s : %8d / %8d\n","record length",
                                 recordLength, recordLengthWords));
        str.append(String.format("%24s : %8d / %8d / %8d\n","compressed length",
                                 compressedDataLength, compressedDataLengthWords,
                                 compressedDataLengthPadding));
        str.append(String.format("%24s : %d\n","header length",headerLength));
        str.append(String.format("%24s : 0x%X\n","magic word",headerMagicWord));
        Integer bitInfo = getBitInfoWord();
        str.append(String.format("%24s : %s\n","bit info word",Integer.toBinaryString(bitInfo)));
        str.append(String.format("%24s : %d\n","record entries",entries));
        str.append(String.format("%24s : %d\n","   compression type",compressionType));

        str.append(String.format("%24s : %d\n","  index length",indexLength));

        str.append(String.format("%24s : %d\n","user register #1",recordUserRegisterFirst));
        str.append(String.format("%24s : %d\n","user register #2",recordUserRegisterSecond));

        return str.toString();
    }

    /**
     * Take a string and add padding characters to its left side.
     * For pretty printing purposes.
     * @param original  string to modify
     * @param pad       character to pad with
     * @param upTo      length to make original string plus padding
     * @return
     */
    private String padLeft(String original, String pad, int upTo) {
        int npadding = upTo - original.length();
        StringBuilder str = new StringBuilder();
        if(npadding>0) for(int i = 0;i < npadding; i++) str.append(pad);
        str.append(original);
        return str.toString();
    }

    /**
     * Print out each word of the given buffer as binary, hex, and decimal.
     * @param buffer
     */
    public void byteBufferBinaryString(ByteBuffer buffer) {
        int nwords = buffer.array().length/4;
        for(int i = 0; i < nwords; i++){
            int value = buffer.getInt(i*4);
            System.out.println(String.format("  word %4d : %36s  0x%8s : %18d", i,
                                             padLeft(Integer.toBinaryString(value),"0",32),
                                             padLeft(Integer.toHexString(value),"0",8),
                                             value));
        }
    }

    /**
     * Run this class as an executable which tests the writing and reading of a record.
     * @param args
     */
    public static void main(String[] args){
        RecordHeader header = new RecordHeader();

        header.setCompressedDataLength(861);
        header.setDataLength(12457);
        header.setUserHeaderLength(459);
        header.setIndexLength(324);
        header.setLength(16 + header.getCompressedDataLengthWords());
        header.setUserRegisterFirst(1234567L);
        header.setUserRegisterSecond(4567890L);
        header.setRecordNumber(23);
        header.setEntries(3245);
        header.setHeaderLength(14);
        header.setCompressionType(1);
        System.out.println(header);

        byte[] array = new byte[14*4];
        ByteBuffer buffer = ByteBuffer.wrap(array);
        buffer.order(ByteOrder.LITTLE_ENDIAN);


        header.writeHeader(buffer);

        //header.byteBufferBinaryString(buffer);

        RecordHeader header2 = new RecordHeader();
        try {
            header2.readHeader(buffer);
            System.out.println(header2);
        }
        catch (HipoException e) {
            e.printStackTrace();
        }
    }
}
