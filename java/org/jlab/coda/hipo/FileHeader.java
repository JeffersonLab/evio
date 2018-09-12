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
 *    +-----------------------+----------+
 *  6 +       Bit Info        | Version  | // version (8 bits)
 *    +-----------------------+----------+
 *  7 +      User Header Length          | // bytes
 *    +----------------------------------+
 *  8 +          Magic Number            | // 0xc0da0100
 *    +----------------------------------+
 *  9 +          User Register           | 
 *    +--                              --+
 * 10 +                                  |
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
public class FileHeader {

    /** Array to help find number of bytes to pad data. */
    private  final static int[] padValue = {0,3,2,1};

    /** First word in every HIPO file for identification purposes. */
    public final static int   HIPO_FILE_UNIQUE_WORD = 0x4F504948; // 0x4849504F = HIPO
    /** First word in every Evio file for identification purposes. */
    public final static int   EVIO_FILE_UNIQUE_WORD = 0x4556494F; // = EVIO
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

    /** Number of bytes from beginning of file header to write trailer position. */
    public final static int   TRAILER_POSITION_OFFSET = 40;
    /** Number of bytes from beginning of file header to write record index count. */
    public final static int   RECORD_COUNT_OFFSET = 12;
    /** Number of bytes from beginning of file header to write bit info word. */
    public final static int   BIT_INFO_OFFSET = 20;

    // Bits in bit info word

    /** 8th bit set in bitInfo word in record/file header means contains dictionary. */
    final static int   DICTIONARY_BIT = 0x100;
    /** 9th bit set in bitInfo word in file header means every split file has same first event. */
    final static int   FIRST_EVENT_BIT = 0x200;
    /** 10th bit set in bitInfo word in file header means file trailer with index array exists. */
    final static int   TRAILER_WITH_INDEX_BIT = 0x400;



    /** File id for file identification purposes. Defaults to HIPO file. 1st word. */
    private int  fileId = HIPO_FILE_UNIQUE_WORD;
    /** File number or split file number, starting at 1. 2nd word. */
    private int  fileNumber = 1;
    /** User-defined 64-bit register. 9th and 10th words. */
    private long userRegister;
    /** Position of trailing header from start of file in bytes. 11th word. */
    private long trailerPosition;
    /** First user-defined integer in file header. 13th word. */
    private int  userIntFirst;
    /** Second user-defined integer in file header. 14th word. */
    private int  userIntSecond;
    /** Position of this header in a file. */
    private long position;

    /** Type of header this is. Normal HIPO record by default. */
    private HeaderType headerType = HeaderType.HIPO_FILE;
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

    /** Final, total length of header + index + user header (bytes).
     *  Not stored in any word. */
    private int  totalLength = HEADER_SIZE_BYTES;


    /** Evio format version number. It is 6 when being written, else
     * the version of file/buffer being read. Lowest byte of 6th word. */
    private int  headerVersion = 6;
    /** Magic number for tracking endianness. 8th word. */
    private int  headerMagicWord = HEADER_MAGIC;
    /** Byte order of file. */
    private ByteOrder  byteOrder = ByteOrder.LITTLE_ENDIAN;

    // These quantities are updated automatically when lengths are set

    /** Number of bytes required to bring uncompressed
      * user header to 4-byte boundary. Stored in 6th word. */
    private int  userHeaderLengthPadding;



    /** Default, no-arg constructor. */
    public FileHeader() {bitInfoInit();}

    /**
     * Constructor which sets the type of header this is and file id.
     * @param isEvio if true, file has EVIO file id and header type, else is HIPO.
     */
    public FileHeader(boolean isEvio) {
        if (isEvio) {
            fileId = EVIO_FILE_UNIQUE_WORD;
            headerType = HeaderType.EVIO_FILE;
        }
        else {
            fileId = HIPO_FILE_UNIQUE_WORD;
            headerType = HeaderType.HIPO_FILE;
        }
        bitInfoInit();
    }

    /**
     * Copy the contents of the arg into this object.
     * @param head object to copy
     */
    public void copy(FileHeader head) {
        if (head == null) return;

        fileId                  = head.fileId;
        fileNumber              = head.fileNumber;
        userRegister            = head.userRegister;
        trailerPosition         = head.trailerPosition;
        userIntFirst            = head.userIntFirst;
        userIntSecond           = head.userIntSecond;
        position                = head.position;
        totalLength             = head.totalLength;
        headerType              = head.headerType;
        entries                 = head.entries;
        bitInfo                 = head.bitInfo;
        headerLength            = head.headerLength;
        headerLengthWords       = head.headerLengthWords;
        userHeaderLength        = head.userHeaderLength;
        userHeaderLengthWords   = head.userHeaderLengthWords;
        indexLength             = head.indexLength;
        headerMagicWord         = head.headerMagicWord;
        userHeaderLengthPadding = head.userHeaderLengthPadding;

        // version must be the same
    }

    /** Reset most internal variables (not file id & header type). */
    public void reset(){
        // Do NOT reset fileId which is only set in constructor.
        // Do NOT reset header type.

        fileNumber = 1;
        userRegister = 0L;
        trailerPosition = 0L;
        userIntFirst  = 0;
        userIntSecond = 0;
        position = 0L;
        entries = 0;
        bitInfoInit();
        totalLength  = HEADER_SIZE_BYTES;
        headerLength = HEADER_SIZE_BYTES;
        headerLengthWords = HEADER_SIZE_WORDS;
        userHeaderLength = 0;
        userHeaderLengthWords = 0;
        indexLength = 0;
        userHeaderLengthPadding = 0;
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
     * Get the byte order of the file this header was read from.
     * Defaults to little endian.
     * @return byte order of the file this header was read from.
     */
    public ByteOrder getByteOrder() {return byteOrder;}

    /**
     * Get the file number or split number.
     * @return file number.
     */
    public int  getFileNumber() {return fileNumber;}

    /**
     * Get the file id.
     * @return file id.
     */
    public int  getFileId() {return fileId;}

    /**
     * Get the user register value.
     * @return user register value.
     */
    public long  getUserRegister() {return userRegister;}

    /**
     * Get the trailer's (trailing header's) file position in bytes.
     * @return trailer's (trailing header's) file position in bytes.
     */
    public long  getTrailerPosition() {return trailerPosition;}

    /**
     * Get the first user integer value.
     * @return first user integer value.
     */
    public int  getUserIntFirst() {return userIntFirst;}

    /**
     * Get the second user integer value.
     * @return second user integer value.
     */
    public int  getUserIntSecond() {return userIntSecond;}

    /**
     * Get the position of this record in a file.
     * @return position of this record in a file.
     */
    public long  getPosition() {return position;}

    /**
     * Get the number of events or entries in index.
     * @return number of events or entries in index.
     */
    public int  getEntries() {return entries;}

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
     * Get the length of the index array in bytes.
     * @return length of the index array in bytes.
     */
    public int  getIndexLength() {return indexLength;}

    /**
     * Get the length of this header data in bytes.
     * @return length of this header data in bytes.
     */
    public int  getHeaderLength() {return headerLength;}

    /**
     * Get the type of header this is.
     * @return type of header this is.
     */
    public HeaderType getHeaderType() {return headerType;}

    /**
     * Get the total length of header + index + user header in bytes.
     * Never compressed.
     * @return total length of header + index + user header in bytes.
     */
    public int getLength() {return totalLength;}

    // Bit info methods

    /** Initialize bitInfo word to this value. */
    private void bitInfoInit() {
        bitInfo = (headerType.getValue() << 28) | (headerVersion & 0xFF);
    }

    /**
     * Get the bit info word.
     * @return bit info word.
     */
    public int getBitInfoWord() {return bitInfo;}

    /**
     * Set the bit info word for a file header.
     * Current value of bitInfo is lost.
     * @param haveFirst  true if file has first event.
     * @param haveDictionary  true if file has dictionary in user header.
     * @param haveTrailerWithIndex  true if file has trailer with record length index.
     * @return new bit info word.
     */
    public int setBitInfo(boolean haveFirst,
                          boolean haveDictionary,
                          boolean haveTrailerWithIndex) {

        bitInfo = (headerType.getValue() << 28) |
                  (userHeaderLengthPadding << 20) |
                  (headerVersion & 0xFF);

        if (haveFirst) bitInfo |= FIRST_EVENT_BIT;
        if (haveDictionary) bitInfo |= DICTIONARY_BIT;
        if (haveTrailerWithIndex) bitInfo |= TRAILER_WITH_INDEX_BIT;

        return bitInfo;
    }

    /**
     * Set the bit which says file has a first event.
     * @param hasFirst  true if file has a first event.
     * @return new bitInfo word.
     */
    public int hasFirstEvent(boolean hasFirst) {
        if (hasFirst) {
            // set bit
            bitInfo |= FIRST_EVENT_BIT;
        }
        else {
            // clear bit
            bitInfo &= ~FIRST_EVENT_BIT;
        }

        return bitInfo;
    }

    /**
     * Does this header have a first event in the user header?
     * @return true if header has a first event in the user header, else false.
     */
    public boolean hasFirstEvent() {return ((bitInfo & FIRST_EVENT_BIT) != 0);}

    /**
     * Set the bit in the file header which says there is a dictionary.
     * @param hasFirst  true if file has a dictionary.
     * @return new bitInfo word.
     */
    public int hasDictionary(boolean hasFirst) {
        if (hasFirst) {
            // set bit
            bitInfo |= DICTIONARY_BIT;
        }
        else {
            // clear bit
            bitInfo &= ~DICTIONARY_BIT;
        }

        return bitInfo;
    }

    /**
     * Does this header have a dictionary in the user header?
     * @return true if header has a dictionary in the user header, else false.
     */
    public boolean hasDictionary() {return ((bitInfo & DICTIONARY_BIT) != 0);}

    /**
     * Set the bit in the file header which says there is a trailer with a record index.
     * @param hasFirst  true if file has a  trailer with a record index.
     * @return new bitInfo word.
     */
    public int hasTrailerWithIndex(boolean hasFirst) {
        if (hasFirst) {
            // set bit
            bitInfo |= TRAILER_WITH_INDEX_BIT;
        }
        else {
            // clear bit
            bitInfo &= ~TRAILER_WITH_INDEX_BIT;
        }

        return bitInfo;
    }

    /**
     * Does this file have a trailer with an index?
     * @return true if file has a trailer with an index, else false.
     */
    public boolean hasTrailerWithIndex() {return ((bitInfo & TRAILER_WITH_INDEX_BIT) != 0);}

    /**
     * Set the bit info word and related values.
     * @param word  bit info word.
     */
    void  setBitInfoWord(int word) {
        bitInfo = word;
        decodeBitInfoWord(word);
    }

    /**
     * Decodes the bit-info word into version, padding and header type.
     * @param word bit-info word.
     */
    private void decodeBitInfoWord(int word){
        // Padding
        userHeaderLengthPadding = (word >>> 20) & 0x3;

        // Evio version
        headerVersion = (word & 0xff);

        // Header type
        headerType =  HeaderType.getHeaderType(word >>> 28);
        if (headerType == null) {
            headerType = HeaderType.EVIO_RECORD;
        }
    }

    //--------------------------------------------------------------

    /**
     * Is this header followed by a user header?
     * @return true if header followed by a user header, else false.
     */
    public boolean hasUserHeader() {return userHeaderLength > 0;}

    /**
     * Does this file have a valid index of record lengths immediately following header?
     * There should be at least one integer for valid index.
     * Must have integral number of entries.
     * @return true if file has a valid index, else false.
     */
    public boolean hasIndex() {return ((indexLength > 3) && (indexLength % 4 == 0));}

    // Setters

    /**
     * Set the file number which is the split number starting at 1.
     * @param num  file number starting at 1.
     */
    public FileHeader setFileNumber(int num) {fileNumber = num; return this;}

    /**
     * Set the first user register.
     * @param val  first user register.
     */
    public FileHeader setUserRegister(long val) {userRegister = val; return this;}

    /**
     * Set the first user integer.
     * @param val  first user integer.
     */
    public FileHeader setUserIntFirst(int val) {userIntFirst  = val; return this;}

    /**
     * Set the second user integer.
     * @param val  second user integer.
     */
    public FileHeader setUserIntSecond(int val) {userIntSecond = val; return this;}

    /**
     * Set this header's type. Normally done in constructor. Limited access.
     * @param type type of header.
     * @return this object.
     */
    FileHeader setHeaderType(HeaderType type) {headerType = type; return this;}

    /**
     * Set the position of this record in a file.
     * @param pos position of this record in a file.
     * @return this object.
     */
    public FileHeader setPosition(long pos) {position = pos; return this;}

    /**
     * Set the length of the index array in bytes.
     * Length is forced to be a multiple of 4!
     * Sets the total length too.
     * @param length  length of index array in bytes.
     * @return this object.
     */
    public FileHeader setIndexLength(int length) {
        indexLength = (length/4)*4;
        setLength(headerLength + indexLength + userHeaderLength + userHeaderLengthPadding);
        return this;
    }

    /**
     * Set the number of record entries.
     * No compression for other values.
     * @param n number of record entries.
     * @return this object.
     */
    public FileHeader setEntries(int n) {entries = n; return this;}

    /**
     * Set the user-defined header's length in bytes & words and the padding.
     * Sets the total length too.
     * @param length  user-defined header's length in bytes.
     * @return this object.
     */
    public FileHeader setUserHeaderLength(int length) {
        userHeaderLength = length;
        userHeaderLengthWords = getWords(length);
        // Set value and update associated value in bitInfo word
        setUserHeaderLengthPadding(getPadding(length));
        setLength(headerLength + indexLength + userHeaderLength + userHeaderLengthPadding);
        return this;
    }

    /**
      * Set the user header's padding - the number of bytes required to bring uncompressed
      * user header to 4-byte boundary. Sets the associated value in bitInfo word.
      * @param padding user header's padding.
      */
     private void setUserHeaderLengthPadding(int padding) {
System.out.println("setUserHeaderLengthPadding: IN, fe bit = " + hasFirstEvent());
         userHeaderLengthPadding = padding;
         bitInfo = bitInfo | (userHeaderLengthPadding << 20);
System.out.println("setUserHeaderLengthPadding: END, fe bit = " + hasFirstEvent());
     }

    /**
     * Set the this header's length in bytes & words.
     * If length is not a multiple of 4, you're on your own!
     * Sets the total length too.
     * @param length  this header's length in bytes.
     * @return this object.
     */
    public FileHeader setHeaderLength(int length) {
        headerLength = length;
        headerLengthWords = length/4;
        setLength(headerLength + indexLength + userHeaderLength + userHeaderLengthPadding);
        return this;
    }

    /**
     * Set the total length in bytes, header + index + user header.
     * This includes padding and is on a 4-byte boundary.
     * Never compressed.
     * @return this object.
     * @param length  total length in bytes, header + index + user header.
     */
    public FileHeader setLength(int length) {totalLength = length; return this;}

    //-------------------------------------------------

    /**
     * Writes the file (not record!) header into the given byte buffer.
     * @param buf byte buffer to write file header into.
     * @param off position in buffer to begin writing.
     * @throws HipoException if buffer is null or contains too little room.
     */
     public void writeHeader(ByteBuffer buf, int off) throws HipoException {

         if (buf == null || (buf.capacity() - off) < HEADER_SIZE_BYTES) {
             throw new HipoException("null or too small buf arg");
         }

         buf.putInt (     off, fileId);            //  0*4
         buf.putInt ( 4 + off, fileNumber);        //  1*4
         buf.putInt ( 8 + off, headerLengthWords); //  2*4
         buf.putInt (12 + off, entries);           //  3*4
         buf.putInt (16 + off, indexLength);       //  4*4
         buf.putInt (20 + off, getBitInfoWord());  //  5*4
         buf.putInt (24 + off, userHeaderLength);  //  6*4
         buf.putInt (28 + off, headerMagicWord);   //  7*4
         buf.putLong(32 + off, userRegister);      //  8*4
         buf.putLong(40 + off, trailerPosition);   // 10*4
         buf.putInt (48 + off, userIntFirst);      // 12*4
         buf.putInt (52 + off, userIntSecond);     // 13*4
     }

     /**
      * Writes the file (not record!) header into the given byte buffer starting at beginning.
      * @param buffer byte buffer to write file header into.
      * @throws HipoException if buffer is null or contains too little room.
      */
     public void writeHeader(ByteBuffer buffer) throws HipoException {
         writeHeader(buffer, 0);
     }

    /**
     * Writes a trailer with an optional index array into the given byte array.
     * @param array byte array to write trailer into.
     * @param recordNumber record number of trailer.
     * @param order byte order of data to be written.
     * @param index array of record lengths to be written to trailer
     *              (must be multiple of 4 bytes). Null if no index array.
     * @throws HipoException if array arg is null or too small to hold trailer + index
     */
    static public void writeTrailer(byte[] array, int recordNumber,
                                    ByteOrder order, byte[] index)
            throws HipoException {
        writeTrailer(array, 0, recordNumber, order, index);
    }

    /**
     * Writes a trailer with an optional index array into the given byte array.
     * @param array byte array to write trailer into.
     * @param off   offset into array to start writing.
     * @param recordNumber record number of trailer.
     * @param order byte order of data to be written.
     * @param index array of record lengths interspersed with event counts
     *              to be written to trailer
     *              (must be multiple of 4 bytes). Null if no index array.
     * @throws HipoException if array arg is null or too small to hold trailer + index
     */
    static public void writeTrailer(byte[] array, int off, int recordNumber,
                                    ByteOrder order, byte[] index)
            throws HipoException {

        int indexLength = 0;
        int wholeLength = HEADER_SIZE_BYTES;
        if (index != null) {
            indexLength = index.length;
            wholeLength += indexLength;
        }

        // Check arg
        if (array == null || array.length < wholeLength) {
            throw new HipoException("null or too small array arg");
        }

        int bitInfo = (HeaderType.EVIO_TRAILER.getValue() << 28) | RecordHeader.LAST_RECORD_BIT | 6;

        try {
            // First the general header part
            ByteDataTransformer.toBytes(wholeLength,  order, array, off);          // 0*4
            ByteDataTransformer.toBytes(recordNumber, order, array, 4 + off);      // 1*4
            ByteDataTransformer.toBytes(HEADER_SIZE_WORDS, order, array, 8 + off); // 2*4
            ByteDataTransformer.toBytes(0, order, array, 12 + off);                // 3*4
            ByteDataTransformer.toBytes(indexLength, order, array, 16 + off);      // 4*4
            ByteDataTransformer.toBytes(bitInfo, order, array, 20 + off);          // 5*4
            ByteDataTransformer.toBytes(0, order, array, 24 + off);                // 6*4
            ByteDataTransformer.toBytes(HEADER_MAGIC, order, array, 28 + off);     // 7*4
            // The rest is all 0's, 8*4 (inclusive) -> 14*4 (exclusive)
            Arrays.fill(array, 32 + off, 56 + off, (byte)0);
//
//            ByteDataTransformer.toBytes(0L, order, array,  8*4 + off);
//            ByteDataTransformer.toBytes(0L, order, array, 10*4 + off);
//            ByteDataTransformer.toBytes(0L, order, array, 12*4 + off);

            // Second the index
            if (indexLength > 0) {
                System.arraycopy(index, 0, array, 14 * 4 + off, indexLength);
            }
        }
        catch (EvioException e) {/* never happen */}
    }

    /**
     * Writes a trailer with an optional index array into the given buffer.
     * @param buf   ByteBuffer to write trailer into.
     * @param off   offset into buffer to start writing.
     * @param recordNumber record number of trailer.
     * @param order byte order of data to be written.
     * @param index array of record lengths interspersed with event counts
     *              to be written to trailer
     *              (must be multiple of 4 bytes). Null if no index array.
     * @throws HipoException if buf arg is null or too small to hold trailer + index
     */
    static public void writeTrailer(ByteBuffer buf, int off, int recordNumber,
                                    ByteOrder order, byte[] index)
            throws HipoException {

        int indexLength = 0;
        int wholeLength = HEADER_SIZE_BYTES;
        if (index != null) {
            indexLength = index.length;
            wholeLength += indexLength;
        }

        // Check arg
        if (buf == null || (buf.capacity() - off < wholeLength)) {
            throw new HipoException("null or too small buf arg");
        }

        if (buf.hasArray()) {
            writeTrailer(buf.array(), off, recordNumber, order, index);
        }
        else {
            int bitInfo = (HeaderType.EVIO_TRAILER.getValue() << 28) | RecordHeader.LAST_RECORD_BIT | 6;

            buf.position(off);

            // First the general header part
            buf.putInt(wholeLength);
            buf.putInt(recordNumber);
            buf.putInt(HEADER_SIZE_WORDS);
            buf.putInt(0);
            buf.putInt(bitInfo);
            buf.putInt(0);
            buf.putInt(HEADER_MAGIC);
            buf.putLong(0L);
            buf.putLong(0L);
            buf.putLong(0L);

            // Second the index
            if (indexLength > 0) {
                buf.put(index, 0, indexLength);
            }
        }
    }

    /**
     * Reads the file header information from a byte buffer and validates
     * it by checking the magic word (8th word). This magic word
     * also determines the byte order.
     *
     * @param buffer buffer to read from.
     * @param offset position of first word to be read.
     * @throws HipoException if buffer is null, contains too little data,
     *                       is not in proper format, or version earlier than 6.
     */
    public void readHeader(ByteBuffer buffer, int offset) throws HipoException {
System.out.println("PARSING HEADER");
        if (buffer == null || (buffer.capacity() - offset) < HEADER_SIZE_BYTES) {
            throw new HipoException("null or too small buffer arg");
        }

        // First read the magic word to establish endianness
        headerMagicWord = buffer.getInt(28 + offset);   // 7*4
        // If it's NOT in the proper byte order ...
        if (headerMagicWord != HEADER_MAGIC_LE) {
            // If it needs to be switched ...
            if (headerMagicWord == HEADER_MAGIC_BE) {
                if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                    byteOrder = ByteOrder.LITTLE_ENDIAN;
                }
                else {
                    byteOrder = ByteOrder.BIG_ENDIAN;
                }
                buffer.order(byteOrder);
            }
            else {
                // ERROR condition, bad magic word
                
                throw new HipoException("buffer arg not in evio/hipo format");
            }
        }
        else {
            byteOrder = buffer.order();
        }

        // Next look at the version #
        bitInfo = buffer.getInt(20 + offset);  // 5*4
        decodeBitInfoWord(bitInfo);
        if (headerVersion < 6) {
            throw new HipoException("buffer is in evio format version " + headerVersion);
        }

        fileId            = buffer.getInt(     offset);   // 0*4
        fileNumber        = buffer.getInt( 4 + offset);   // 1*4
        headerLengthWords = buffer.getInt( 8 + offset);   // 2*4
        setHeaderLength(4*headerLengthWords);
System.out.println("fileHeader: header len = " + headerLength);
        entries           = buffer.getInt(12 + offset);   // 3*4

        indexLength       = buffer.getInt(16 + offset);   // 4*4
System.out.println("fileHeader: index len = " + indexLength);
        setIndexLength(indexLength);

        userHeaderLength  = buffer.getInt(24 + offset);   // 6*4
        setUserHeaderLength(userHeaderLength);
System.out.println("fileHeader: user header len = " + userHeaderLength +
                   ", padding = " + userHeaderLengthPadding);

System.out.println("fileHeader: total len = " + totalLength);

        userRegister     = buffer.getLong(32 + offset);   // 8*4
        trailerPosition  = buffer.getLong(40 + offset);   // 10*4
        userIntFirst     = buffer.getInt (48 + offset);   // 12*4
        userIntSecond    = buffer.getInt (52 + offset);   // 13*4
    }

    /**
     * Reads the file header information from a byte buffer and validates
     * it by checking the magic word (8th word). This magic word
     * also determines the byte order.
     *
     * @param buffer buffer to read from starting at the beginning.
     * @throws HipoException if buffer is not in the proper format or earlier than version 6
     */
    public void readHeader(ByteBuffer buffer) throws HipoException {
        readHeader(buffer, 0);
    }

    /**
     * Returns a string representation of the record.
     * @return a string representation of the record.
     */
    @Override
    public String toString(){

        StringBuilder str = new StringBuilder();
        str.append(String.format("%24s : %d\n","version",headerVersion));
        str.append(String.format("%24s : %d    bytes,     words,    padding\n","file #",fileNumber));
        str.append(String.format("%24s : %8d / %8d / %8d\n","user header length",
                                 userHeaderLength, userHeaderLengthWords, userHeaderLengthPadding));
        str.append(String.format("%24s : %d\n","header length",headerLength));
        str.append(String.format("%24s : 0x%X\n","magic word",headerMagicWord));
        Integer bitInfo = getBitInfoWord();
        str.append(String.format("%24s : %s\n","bit info word",Integer.toBinaryString(bitInfo)));
        str.append(String.format("%24s : %d\n","record entries",entries));

        str.append(String.format("%24s : %d\n","  index length", indexLength));

        str.append(String.format("%24s : %d\n","trailer position", trailerPosition));
        str.append(String.format("%24s : %d\n","user register", userRegister));
        str.append(String.format("%24s : %d\n","user int #1", userIntFirst));
        str.append(String.format("%24s : %d\n","user int #2", userIntSecond));

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
        FileHeader header = new FileHeader();

        header.setUserHeaderLength(459);
        header.setIndexLength(324);
        header.setUserRegister(123123123123123L);
        header.setUserIntFirst(1234567);
        header.setUserIntSecond(4567890);
        header.setFileNumber(23);
        header.setEntries(3245);
        header.setHeaderLength(14);
        System.out.println(header);

        byte[] array = new byte[14*4];
        ByteBuffer buffer = ByteBuffer.wrap(array);
        buffer.order(ByteOrder.LITTLE_ENDIAN);

        try {
            header.writeHeader(buffer);
        }
        catch (HipoException e) {/* never happen */}

        FileHeader header2 = new FileHeader();
        try {
            header2.readHeader(buffer);
            System.out.println(header2);
        }
        catch (HipoException e) {
            e.printStackTrace();
        }
    }
}
