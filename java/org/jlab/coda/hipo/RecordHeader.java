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
import java.util.Arrays;

/**
 * <pre>
 *
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
 *    +-----------------------+----------+
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
    /** Number of 32-bit words in a normal sized header. */
    public final static int   HEADER_SIZE_WORDS = 14;
    /** Number of bytes in a normal sized header. */
    public final static int   HEADER_SIZE_BYTES = 56;
    /** Magic number used to track endianness. */
    public final static int   HEADER_MAGIC = 0xc0da0100;
    /** Magic number for HIPO's little endian uses. */
    final static int   HEADER_MAGIC_LE = HEADER_MAGIC;
    /** Magic number for HIPO's big endian uses (byte swapped from HEADER_MAGIC_LE). */
    final static int   HEADER_MAGIC_BE = Integer.reverseBytes(HEADER_MAGIC);

    // Byte offset to header words

    /** Number of bytes from beginning of file header to write bit info word. */
    public final static int   BIT_INFO_OFFSET = 20;

    // Bits in bit info word
    
    /** 8th bit set in bitInfo word in record/file header means contains dictionary. */
    final static int   DICTIONARY_BIT = 0x100;
    /** 9th bit set in bitInfo word in record header means is last in stream or file. */
    final static int   LAST_RECORD_BIT = 0x200;

    /** 10-13th bits in bitInfo word in record header for CODA data type, ROC raw = 0. */
    final static int   DATA_ROC_RAW_BITS = 0x000;
    /** 10-13th bits in bitInfo word in record header for CODA data type, physics = 1. */
    final static int   DATA_PHYSICS_BITS = 0x400;
    /** 10-13th bits in bitInfo word in record header for CODA data type, partial physics = 2. */
    final static int   DATA_PARTIAL_BITS = 0x800;
    /** 10-13th bits in bitInfo word in record header for CODA data type, disentangled = 3. */
    final static int   DATA_DISENTANGLED_BITS = 0xC00;
    /** 10-13th bits in bitInfo word in record header for CODA data type, user = 4. */
    final static int   DATA_USER_BITS    = 0x1000;
    /** 10-13th bits in bitInfo word in record header for CODA data type, control = 5. */
    final static int   DATA_CONTROL_BITS = 0x1400;
    /** 10-13th bits in bitInfo word in record header for CODA data type, other = 15. */
    final static int   DATA_OTHER_BITS   = 0x3C00;


    /** Position of this header in a file. */
    private long position;
    /** Length of the entire record this header is a part of (bytes). */
    private int  recordLength;
    /** Length of the entire record this header is a part of (32-bit words). 1st word. */
    private int  recordLengthWords;
    /** Record number. 2nd word. */
    private int  recordNumber;
    /** First user-defined 64-bit register. 11th and 12th words. */
    private long recordUserRegisterFirst;
    /** Second user-defined 64-bit register. 13th and 14th words. */
    private long recordUserRegisterSecond;


    /** Type of header this is. Normal HIPO record by default. */
    private HeaderType headerType = HeaderType.HIPO_RECORD;
    /** Event or record count. 4th word. */
    private int  entries;
    /** BitInfo & version. 6th word. */
    private int  bitInfo = -1;
    /** Length of this header (bytes). */
    private int  headerLength = HEADER_SIZE_BYTES;
    /** Length of this header (words). 3rd word. */
    private int  headerLengthWords = HEADER_SIZE_WORDS;
    /** Length of user-defined header (bytes). 7th word. */
    private int  userHeaderLength;
    /** Length of user-defined header when padded (words). */
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
    public RecordHeader() {}

    /**
     * Constructor which sets the type of header this is.
     * @param type  type of header this is
     * @throws HipoException if type is for file
     */
    public RecordHeader(HeaderType type) throws HipoException {
        headerType = type;
        if (type.isFileHeader()) {
            throw new HipoException("use FileHeader class for a file");
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

    /**
     * Copy the contents of the arg into this object.
     * @param head object to copy
     */
    public void copy(RecordHeader head) {
        if (head == null) return;

        position                 = head.position;

        recordLength             = head.recordLength;
        recordNumber             = head.recordNumber;
        recordLengthWords        = head.recordLengthWords;
        recordUserRegisterFirst  = head.recordUserRegisterFirst;
        recordUserRegisterSecond = head.recordUserRegisterSecond;

        headerType                = head.headerType;
        entries                   = head.entries;
        bitInfo                   = head.bitInfo;
        headerLength              = head.headerLength;
        headerLengthWords         = head.headerLengthWords;
        userHeaderLength          = head.userHeaderLength;
        userHeaderLengthWords     = head.userHeaderLengthWords;
        indexLength               = head.indexLength;
        dataLength                = head.dataLength;
        dataLengthWords           = head.dataLengthWords;
        compressedDataLength      = head.compressedDataLength;
        compressedDataLengthWords = head.compressedDataLengthWords;
        compressionType           = head.compressionType;
        headerMagicWord           = head.headerMagicWord;
        // don't bother with version as must be same

        userHeaderLengthPadding     = head.userHeaderLengthPadding;
        dataLengthPadding           = head.dataLengthPadding;
        compressedDataLengthPadding = head.compressedDataLengthPadding;
    }

    /** Reset generated data. */
    public void reset(){
        // Do NOT reset header type which is only set in constructor!
        // Do NOT reset the compression type
        position = 0L;

        recordLength = 0;
        recordNumber = 0;
        recordLengthWords = 0;
        recordUserRegisterFirst = 0L;
        recordUserRegisterSecond = 0L;

        entries = 0;
        bitInfo = -1;
        headerLength = HEADER_SIZE_BYTES;
        headerLengthWords = HEADER_SIZE_WORDS;
        userHeaderLength = 0;
        userHeaderLengthWords = 0;
        indexLength = 0;
        dataLength = 0;
        dataLengthWords = 0;
        compressedDataLength = 0;
        compressedDataLengthWords = 0;

        userHeaderLengthPadding = 0;
        dataLengthPadding = 0;
        compressedDataLengthPadding = 0;
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
     * Get the padding of the compressed data in bytes.
     * @return padding of the compressed data in bytes.
     */
    public int getCompressedDataLengthPadding() {return this.compressedDataLengthPadding;}
    
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

    // Bit info methods

    /**
     * Get the bit info word. Will initialize if not already done.
     * @return bit info word.
     */
    public int getBitInfoWord() {
        // If bitInfo uninitialized, do so now
        if (bitInfo < 0) {
            // This will init the same whether file or record header
            setBitInfo(false, false);
        }
        return bitInfo;
    }

    /**
     * Set the bit info word for a record header.
     * @param isLastRecord   true if record is last in stream or file.
     * @param haveDictionary true if record has dictionary in user header.
     * @return new bit info word.
     */
    public int  setBitInfo(boolean isLastRecord,
                           boolean haveDictionary) {

        bitInfo = (headerType.getValue()       << 28) |
                  (compressedDataLengthPadding << 24) |
                  (dataLengthPadding           << 22) |
                  (userHeaderLengthPadding     << 20) |
                  (headerVersion & 0xFF);

        if (isLastRecord)   bitInfo |= LAST_RECORD_BIT;
        if (haveDictionary) bitInfo |= DICTIONARY_BIT;

        return bitInfo;
    }

    /**
     * Set the bit info of a record header for a specified CODA data type.
     * Must be called AFTER {@link #setBitInfo(boolean, boolean)} or
     * {@link #setBitInfoWord(int)} in order to have change preserved.
     * @return new bit info word.
     * @param type data type (0=ROC raw, 1=Physics, 2=Partial Physics,
     *             3=Disentangled, 4=User, 5=Control, 15=Other,
     *             else = nothing set).
     */
    public int  setBitInfoDataType (int type) {
        switch(type) {
            case 0:
                bitInfo |= DATA_ROC_RAW_BITS; break;
            case 1:
                bitInfo |= DATA_PHYSICS_BITS; break;
            case 2:
                bitInfo |= DATA_PARTIAL_BITS; break;
            case 3:
                bitInfo |= DATA_DISENTANGLED_BITS; break;
            case 4:
                bitInfo |= DATA_USER_BITS; break;
            case 5:
                bitInfo |= DATA_CONTROL_BITS; break;
            case 13:
                bitInfo |= DATA_OTHER_BITS; break;
            default:
        }

        return bitInfo;
    }

    /**
     * Set the bit info word and related values.
     * @param word  bit info word.
     */
    void  setBitInfoWord(int word) {
        bitInfo = word;
        decodeBitInfoWord(word);
    }

    /**
     * decodes the padding words
     * @param word
     */
    private void decodeBitInfoWord(int word){
        // Padding
        this.compressedDataLengthPadding = (word >>> 24) & 0x3;
        this.dataLengthPadding           = (word >>> 22) & 0x3;
        this.userHeaderLengthPadding     = (word >>> 20) & 0x3;

        // Header type
        headerType =  HeaderType.getHeaderType(word >>> 28);
        if (headerType == null) {
            headerType = HeaderType.EVIO_RECORD;
        }
    }

    /**
     * Does this header have a dictionary in the user header?
     * @return true if header has a dictionary in the user header, else false.
     */
    public boolean hasDictionary() {
        return ((bitInfo & DICTIONARY_BIT) != 0);
    }

    /**
     * Is this the header of the last record?
     * @return true this is the header of the last record, else false.
     */
    public boolean isLastRecord() {
        return ((bitInfo & LAST_RECORD_BIT) != 0);
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
        recordLength      = length;
        recordLengthWords = length/4;
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
     * Position & limit of given buffer does NOT change.
     * @param buf byte buffer to write header into.
     * @param off position in buffer to begin writing.
     * @throws HipoException if buffer is null or contains too little room.
     */
    public void writeHeader(ByteBuffer buf, int off) throws HipoException {

        // Check arg
        if (buf == null || (buf.capacity() - off) < HEADER_SIZE_BYTES) {
            throw new HipoException("null or too small buf arg");
        }

        int compressedWord =  (compressedDataLengthWords & 0x0FFFFFFF) |
                             ((compressionType & 0xF) << 28);

        buf.putInt (     off, recordLengthWords);        //  0*4
        buf.putInt ( 4 + off, recordNumber);             //  1*4
        buf.putInt ( 8 + off, headerLengthWords);        //  2*4
        buf.putInt (12 + off, entries);                  //  3*4
        buf.putInt (16 + off, indexLength);              //  4*4
        buf.putInt (20 + off, getBitInfoWord());         //  5*4
        buf.putInt (24 + off, userHeaderLength);         //  6*4
        buf.putInt (28 + off, headerMagicWord);          //  7*4
        buf.putInt (32 + off, dataLength);               //  8*4
        buf.putInt (36 + off, compressedWord);           //  9*4
        buf.putLong(40 + off, recordUserRegisterFirst);  // 10*4
        buf.putLong(48 + off, recordUserRegisterSecond); // 12*4
    }

    /**
     * Writes this header into the given byte buffer starting at the beginning.
     * Position & limit of given buffer does NOT change.
     * @param buffer byte buffer to write header into.
     * @throws HipoException if buffer is null, contains too little room.
     */
    public void writeHeader(ByteBuffer buffer) throws HipoException {
        writeHeader(buffer,0);
    }

    /**
     * Writes an empty trailer into the given byte array.
     * @param array byte array to write trailer into.
     * @param recordNumber record number of trailer.
     * @param order byte order of data to be written.
     * @throws HipoException if array arg is null or too small to hold trailer
     */
    static public void writeTrailer(byte[] array, int recordNumber, ByteOrder order)
            throws HipoException {
        writeTrailer(array, 0, recordNumber, order);
    }

    /**
     * Writes an empty trailer into the given byte array.
     * @param array byte array to write trailer into.
     * @param off   offset into array to start writing.
     * @param recordNumber record number of trailer.
     * @param order byte order of data to be written.
     * @throws HipoException if array arg is null or too small to hold trailer
     */
    static public void writeTrailer(byte[] array, int off, int recordNumber,
                                    ByteOrder order)
            throws HipoException {

        int indexLength = 0;
        int totalLength = HEADER_SIZE_BYTES;

        // Check arg
        if (array == null || array.length < totalLength) {
            throw new HipoException("null or too small array arg");
        }

        // TODO: the header type and "last record" bit are redundant
        int bitInfo = (HeaderType.EVIO_TRAILER.getValue() << 28) | LAST_RECORD_BIT | 6;

        try {
            // First the general header part
            ByteDataTransformer.toBytes(totalLength,  order, array, off);          // 0*4
            ByteDataTransformer.toBytes(recordNumber, order, array, 4 + off);      // 1*4
            ByteDataTransformer.toBytes(HEADER_SIZE_WORDS, order, array, 8 + off); // 2*4
            ByteDataTransformer.toBytes(0, order, array, 12 + off);                // 3*4
            ByteDataTransformer.toBytes(indexLength, order, array, 16 + off);      // 4*4
            ByteDataTransformer.toBytes(bitInfo, order, array, 20 + off);          // 5*4
            ByteDataTransformer.toBytes(0, order, array, 24 + off);                // 6*4
            ByteDataTransformer.toBytes(HEADER_MAGIC, order, array, 28 + off);     // 7*4
            // The rest is all 0's, 8*4 (inclusive) -> 14*4 (exclusive)
            Arrays.fill(array, 32 + off, 56 + off, (byte)0);
        }
        catch (EvioException e) {/* never happen */}
    }

    /**
     * Writes an empty trailer into the given buffer.
     * @param buf   ByteBuffer to write trailer into.
     * @param off   offset into buffer to start writing.
     * @param recordNumber record number of trailer.
     * @param order byte order of data to be written.
     * @throws HipoException if buf arg is null or too small to hold trailer
     */
    static public void writeTrailer(ByteBuffer buf, int off, int recordNumber,
                                    ByteOrder order)
            throws HipoException {

        int totalLength = HEADER_SIZE_BYTES;

        // Check arg
        if (buf == null || (buf.capacity() - off < totalLength)) {
            throw new HipoException("null or too small buf arg");
        }

        if (buf.hasArray()) {
            writeTrailer(buf.array(), off, recordNumber, order);
        }
        else {
            // TODO: the header type and "last record" bit are redundant
            int bitInfo = (HeaderType.EVIO_TRAILER.getValue() << 28) | LAST_RECORD_BIT | 6;

            buf.position(off);

            // First the general header part
            buf.putInt(totalLength);
            buf.putInt(recordNumber);
            buf.putInt(HEADER_SIZE_WORDS);
            buf.putInt(0);
            buf.putInt(bitInfo);
            buf.putInt(0);
            buf.putInt(HEADER_MAGIC);
            buf.putLong(0L);
            buf.putLong(0L);
            buf.putLong(0L);
        }
    }

    /**
     * Reads the header information from a byte buffer and validates
     * it by checking the magic word (8th word). This magic word
     * also determines the byte order.
     *
     * @param buffer buffer to read from.
     * @param offset position of first word to be read.
     * @throws HipoException if buffer is null, contains too little data,
     *                       is not in proper format, or version earlier than 6.
     */
    public void readHeader(ByteBuffer buffer, int offset) throws HipoException {
        if (buffer == null || (buffer.capacity() - offset) < HEADER_SIZE_BYTES) {
            throw new HipoException("null or too small buffer arg");
        }

        // First read the magic word to establish endianness
        headerMagicWord = buffer.getInt(28 + offset);    // 7*4

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

        // Look at the bit-info word
        int bitInfoWord = buffer.getInt(20 + offset);   // 5*4

        // Set padding and header type
        decodeBitInfoWord(bitInfoWord);
        
        // Look at the version #
        int version  = (bitInfoWord & 0xFF);
        if (version < headerVersion) {
            throw new HipoException("buffer is in evio format version " + version);
        }

        recordLengthWords   = buffer.getInt(     offset);        //  0*4
        recordLength        = 4*recordLengthWords;
        recordNumber        = buffer.getInt( 4 + offset);        //  1*4
        headerLengthWords   = buffer.getInt( 8 + offset);        //  2*4
        setHeaderLength(4*headerLengthWords);
        entries             = buffer.getInt(12 + offset);        //  3*4

        indexLength         = buffer.getInt(16 + offset);        //  4*4
        setIndexLength(indexLength);

        userHeaderLength    = buffer.getInt(24 + offset);        //  6*4
        setUserHeaderLength(userHeaderLength);

        // uncompressed data length
        dataLength          = buffer.getInt(32 + offset);        //  8*4
        setDataLength(dataLength);

        int compressionWord = buffer.getInt(36 + offset);        //  9*4
        compressionType     = (compressionWord >>> 28);
        compressedDataLengthWords = (compressionWord & 0x0FFFFFFF);
        compressedDataLengthPadding = (bitInfoWord >>> 24) & 0x3;
        compressedDataLength = compressedDataLengthWords*4 - compressedDataLengthPadding;

        recordUserRegisterFirst  = buffer.getLong(40 + offset);  // 10*4
        recordUserRegisterSecond = buffer.getLong(48 + offset);  // 12*4
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
    private static String padLeft(String original, String pad, int upTo) {
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
    public static void byteBufferBinaryString(ByteBuffer buffer) {
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


        try {
            header.writeHeader(buffer);
        }
        catch (HipoException e) {/* never happen */}

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
