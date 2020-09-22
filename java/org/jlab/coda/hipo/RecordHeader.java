/*
 *   Copyright (c) 2016.  Jefferson Lab (JLab). All rights reserved. Permission
 *   to use, copy, modify, and distribute  this software and its documentation for
 *   educational, research, and not-for-profit purposes, without fee and without a
 *   signed licensing agreement.
 */
package org.jlab.coda.hipo;

import org.jlab.coda.jevio.ByteDataTransformer;
import org.jlab.coda.jevio.EvioException;
import org.jlab.coda.jevio.IBlockHeader;
import org.jlab.coda.jevio.Utilities;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.BitSet;
import java.util.List;

/**
 * <pre>
 *
 * GENERAL RECORD HEADER STRUCTURE ( 56 bytes, 14 integers (32 bit) )
 *
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
 *     9    = true if this record has "first" event (to be in every split file)
 *    10    = true if this record is the last in file or stream
 *    11-14 = type of events contained: 0 = ROC Raw,
 *                                      1 = Physics
 *                                      2 = PartialPhysics
 *                                      3 = DisentangledPhysics
 *                                      4 = User
 *                                      5 = Control
 *                                     15 = Other
 *    15-19 = reserved
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
public class RecordHeader implements IBlockHeader {

    /** Array to help find number of bytes to pad data. */
    private final static int[] padValue = {0,3,2,1};

    /** Number of 32-bit words in a normal sized header. */
    public final static int   HEADER_SIZE_WORDS = 14;
    /** Number of bytes in a normal sized header. */
    public final static int   HEADER_SIZE_BYTES = 56;
    /** Magic number used to track endianness. */
    public final static int   HEADER_MAGIC = 0xc0da0100;

    // Byte offset to header words

    /** Byte offset from beginning of header to the record length. */
    public final static int   RECORD_LENGTH_OFFSET = 0;
    /** Byte offset from beginning of header to the record number. */
    public final static int   RECORD_NUMBER_OFFSET = 4;
    /** Byte offset from beginning of header to the header length. */
    public final static int   HEADER_LENGTH_OFFSET = 8;
    /** Byte offset from beginning of header to the event index count. */
    public final static int   EVENT_COUNT_OFFSET = 12;
    /** Byte offset from beginning of header to the index array length. */
    public final static int   INDEX_ARRAY_OFFSET = 16;
    /** Byte offset from beginning of header to bit info word. */
    public final static int   BIT_INFO_OFFSET = 20;
    /** Byte offset from beginning of header to the user header length. */
    public final static int   USER_LENGTH_OFFSET = 24;
    /** Byte offset from beginning of header to the record length. */
    public final static int   MAGIC_OFFSET = 28;
    /** Byte offset from beginning of header to the uncompressed data length. */
    public final static int   UNCOMPRESSED_LENGTH_OFFSET = 32;
    /** Byte offset from beginning of header to the compression type and compressed data length word. */
    public final static int   COMPRESSION_TYPE_OFFSET = 36;
    /** Byte offset from beginning of header to the user register #1. */
    public final static int   REGISTER1_OFFSET = 40;
    /** Byte offset from beginning of header to the user register #2. */
    public final static int   REGISTER2_OFFSET = 48;

    // Bits in bit info word
    
    /** 8th bit set in bitInfo word in header means contains dictionary. */
    final static int   DICTIONARY_BIT = 0x100;
    /** 9th bit set in bitInfo word in header means every split file has same first event. */
    final static int   FIRST_EVENT_BIT = 0x200;
    /** 10th bit set in bitInfo word in header means is last in stream or file. */
    final static int   LAST_RECORD_BIT = 0x400;

    /** 11-14th bits in bitInfo word in header for CODA data type, ROC raw = 0. */
    final static int   DATA_ROC_RAW_BITS = 0x000;
    /** 11-14th bits in bitInfo word in header for CODA data type, physics = 1. */
    final static int   DATA_PHYSICS_BITS = 0x800;
    /** 11-14th bits in bitInfo word in header for CODA data type, partial physics = 2. */
    final static int   DATA_PARTIAL_BITS = 0x1000;
    /** 11-14th bits in bitInfo word in header for CODA data type, disentangled = 3. */
    final static int   DATA_DISENTANGLED_BITS = 0x1800;
    /** 11-14th bits in bitInfo word in header for CODA data type, user = 4. */
    final static int   DATA_USER_BITS    = 0x2000;
    /** 11-14th bits in bitInfo word in record header for CODA data type, control = 5. */
    final static int   DATA_CONTROL_BITS = 0x2800;
    /** 11-14th bits in bitInfo word in record header for CODA data type, other = 15. */
    final static int   DATA_OTHER_BITS   = 0x7800;

    // Bit masks

    /** Mask to get version number from 6th int in header. */
    public final static int VERSION_MASK = 0xff;
    /** "Last record" is 11th bit in bitInfo word. */
    public static final int LAST_RECORD_MASK = 0x400;

    /** Compressed data padding mask. */
    private static final int COMP_PADDING_MASK = 0x03000000;
    /** Uncompressed data padding mask. */
    private static final int DATA_PADDING_MASK = 0x00C00000;
    /** User header padding mask. */
    private static final int USER_PADDING_MASK = 0x00300000;
    /** Header type mask. */
    private static final int HEADER_TYPE_MASK = 0xF0000000;

    //-------------------

    /** First user-defined 64-bit register. 11th and 12th words. */
    private long recordUserRegisterFirst;
    /** Second user-defined 64-bit register. 13th and 14th words. */
    private long recordUserRegisterSecond;
    /** Position of this header in a file. */
    private long position;
    /** Length of the entire record this header is a part of (bytes). */
    private int  recordLength;
    /** Length of the entire record this header is a part of (32-bit words). 1st word. */
    private int  recordLengthWords;
    /** Record number. 2nd word. */
    private int  recordNumber = 1;


    /** Event or record count. 4th word. */
    private int  entries;
    /** BitInfo & version. 6th word. */
    private int  bitInfo = -1;
    /**
     * Type of events in record, encoded in bitInfo word
     * (0=ROC raw, 1=Physics, 2=Partial Physics, 3=Disentangled,
     * 4=User, 5=Control, 15=Other).
     */
    private int  eventType;
    /** Length of this header NOT including user header or index (bytes). */
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
    /** Evio format version number. It is 6 when being written, else
     * the version of file/buffer being read. Lowest byte of 6th word. */
    private int  headerVersion = 6;
    /** Magic number for tracking endianness. 8th word. */
    private int  headerMagicWord = HEADER_MAGIC;

    /** Number of bytes required to bring uncompressed
      * user header to 4-byte boundary. Stored in 6th word.
      * Updated automatically when lengths are set. */
    private int  userHeaderLengthPadding;
    /** Number of bytes required to bring uncompressed
      * data to 4-byte boundary. Stored in 6th word.
      * Updated automatically when lengths are set. */
    private int  dataLengthPadding;
    /** Number of bytes required to bring compressed
      * data to 4-byte boundary. Stored in 6th word.
      * Updated automatically when lengths are set. */
    private int  compressedDataLengthPadding;

    /** Type of header this is. Normal HIPO record by default. */
    private HeaderType headerType = HeaderType.EVIO_RECORD;
    
    /** Byte order of file/buffer this header was read from. */
    private ByteOrder  byteOrder = ByteOrder.LITTLE_ENDIAN;

    /** Type of data compression (0=none, 1=LZ4 fast, 2=LZ4 best, 3=gzip).
      * Highest 4 bits of 10th word. */
    private CompressionType compressionType = CompressionType.RECORD_UNCOMPRESSED;



    /** Default, no-arg constructor. */
    public RecordHeader() {bitInfoInit();}

    /**
     * Constructor which sets the type of header this is.
     * @param type  type of header this is
     * @throws HipoException if type is for file
     */
    public RecordHeader(HeaderType type) throws HipoException {
        if (type != null) {
            headerType = type;
        }
        
        if (headerType.isFileHeader()) {
            throw new HipoException("use FileHeader class for a file");
        }
        bitInfoInit();
    }

    /**
     * Constructor which copies another header.
     * @param header  header to copy.
     */
    public RecordHeader(RecordHeader header) {copy(header);}

    /**
     * Constructor.
     * @param _pos   position in file.
     * @param _l     length of record in bytes
     * @param _e     number of events
     */
    public RecordHeader(long _pos, int _l, int _e){
        position = _pos; recordLength = _l; entries = _e;
        bitInfoInit();
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

        entries                   = head.entries;
        bitInfo                   = head.bitInfo;
        eventType                 = head.eventType;
        headerLength              = head.headerLength;
        headerLengthWords         = head.headerLengthWords;
        userHeaderLength          = head.userHeaderLength;
        userHeaderLengthWords     = head.userHeaderLengthWords;
        indexLength               = head.indexLength;
        dataLength                = head.dataLength;
        dataLengthWords           = head.dataLengthWords;
        compressedDataLength      = head.compressedDataLength;
        compressedDataLengthWords = head.compressedDataLengthWords;
        headerMagicWord           = head.headerMagicWord;
        // don't bother with version as must be same

        userHeaderLengthPadding     = head.userHeaderLengthPadding;
        dataLengthPadding           = head.dataLengthPadding;
        compressedDataLengthPadding = head.compressedDataLengthPadding;

        byteOrder                   = head.byteOrder;
        headerType                  = head.headerType;
        compressionType             = head.compressionType;
    }

    /** Reset generated data. */
    public void reset(){
        // Do NOT reset header type which is only set in constructor!
        // Do NOT reset the compression type

        // What about byteOrder?
        // When reading, it's automatically set. When writing,
        // it's determined by the ByteBuffer we're writing into.

        position = 0L;
        recordLength = 0;
        recordNumber = 1;
        recordLengthWords = 0;
        recordUserRegisterFirst = 0L;
        recordUserRegisterSecond = 0L;

        entries = 0;
        bitInfoInit();
        eventType = 0;
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
     * Get the padded length in bytes of the entire uncompressed record.
     * @return padded length in bytes of the entire uncompressed record.
     */
    public int getUncompressedRecordLength() {
        return (headerLength + indexLength + userHeaderLength + dataLength +
                userHeaderLengthPadding + dataLengthPadding);
    }

    /**
     * Get the padded length in bytes of the entire compressed record.
     * If the data is not compressed, then this returns -1;
     * @return padded length in bytes of the entire compressed record, else -1 if not compressed.
     */
    public int getCompressedRecordLength() {
        if (compressionType != CompressionType.RECORD_UNCOMPRESSED) {
            return (recordLength + compressedDataLengthPadding);
        }
        return  -1;
    }

    /**
     * Get the byte order of the file/buffer this header was read from.
     * Defaults to little endian.
     * @return byte order of the file/buffer this header was read from.
     */
    public ByteOrder getByteOrder() {return byteOrder;}

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
     * Get the total length of this record in 32 bit words.
     * @return total length of this record in 32 bit words.
     */
    public int  getLengthWords() {return recordLengthWords;}

    /**
     * Get the number of events or entries in index.
     * @return number of events or entries in index.
     */
    public int  getEntries() {return entries;}

    /**
     * Get the type of compression used. 0=none, 1=LZ4 fast, 2=LZ4 best, 3=gzip.
     * @return type of compression used.
     */
    public CompressionType getCompressionType() {return compressionType;}

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
     * Get the length of this header data in bytes (NOT including user header or index).
     * @return length of this header data in bytes.
     */
    public int  getHeaderLength() {return headerLength;}

    /**
     * Get the length of this header data in words.
     * @return length of this header data in words.
   	 */
   	public int getHeaderWords() {return headerLengthWords;}

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

    /** Initialize bitInfo word to this value. */
    private void bitInfoInit() {
        bitInfo = (headerType.getValue() << 28) | (headerVersion & 0xFF);
    }

    /**
     * Set the bit info word for a record header.
     * Current value of bitInfo is lost.
     * @param isLastRecord   true if record is last in stream or file.
     * @param haveFirstEvent true if record has first event in user header.
     * @param haveDictionary true if record has dictionary in user header.
     * @return new bit info word.
     */
    public int  setBitInfo(boolean isLastRecord,
                           boolean haveFirstEvent,
                           boolean haveDictionary) {

        bitInfo = (headerType.getValue()       << 28) |
                  (compressedDataLengthPadding << 24) |
                  (dataLengthPadding           << 22) |
                  (userHeaderLengthPadding     << 20) |
                  (headerVersion & 0xFF);

        if (haveDictionary) bitInfo |= DICTIONARY_BIT;
        if (haveFirstEvent) bitInfo |= FIRST_EVENT_BIT;
        if (isLastRecord)   bitInfo |= LAST_RECORD_BIT;

        return bitInfo;
    }

    /**
     * Get the bit info word. Will initialize if not already done.
     * @return bit info word.
     */
    public int getBitInfoWord() {return bitInfo;}

    /**
     * Set the bit info word and related values.
     * NOT FOR GENERAL USE!
     * @param word  bit info word.
     */
    public void setBitInfoWord(int word) {
        bitInfo = word;
        decodeBitInfoWord(word);
    }

    /**
     * Set the bit info word and related values.
     * NOT FOR GENERAL USE!
     * @param set  bit info object.
     */
    public void setBitInfoWord(BitSet set) {
        bitInfo = generateSixthWord(set);
        decodeBitInfoWord(bitInfo);
    }


    /**
     * Calculates the sixth word of this header which has the version
     * number (6) in the lowest 8 bits and the set in the upper 24 bits.
     * NOT FOR GENERAL USE!
     *
     * @param set Bitset containing all bits to be set
     * @return generated sixth word of this header.
     */
    static public int generateSixthWord(BitSet set) {

        // version
        int v = 6;

        if (set == null) return v;

        for (int i=0; i < set.length(); i++) {
            if (i > 23) {
                break;
            }
            if (set.get(i)) {
                v |= (0x1 << (8+i));
            }
        }

        return v;
    }


    /**
     * Calculates the sixth word of this header which has the version number
     * in the lowest 8 bits. The arg hasDictionary
     * is set in the 9th bit and isEnd is set in the 10th bit. Four bits of an int
     * (event type) are set in bits 11-14.
     *
     * @param version evio version number
     * @param hasDictionary does this block include an evio xml dictionary as the first event?
     * @param isEnd is this the last block of a file or a buffer?
     * @param eventType 4 bit type of events header is containing
     * @return generated sixth word of this header.
     */
    static public int generateSixthWord(int version, boolean hasDictionary,
                                        boolean isEnd, int eventType) {

        return generateSixthWord(null, version, hasDictionary, isEnd, eventType);
    }


    /**
      * Calculates the sixth word of this header which has the version number (4)
      * in the lowest 8 bits and the set in the upper 24 bits. The arg isDictionary
      * is set in the 9th bit and isEnd is set in the 10th bit. Four bits of an int
      * (event type) are set in bits 11-14.
      *
      * @param bSet Bitset containing all bits to be set
      * @param version evio version number
      * @param hasDictionary does this block include an evio xml dictionary as the first event?
      * @param isEnd is this the last block of a file or a buffer?
      * @param eventType 4 bit type of events header is containing
      * @return generated sixth word of this header.
      */
     static public int generateSixthWord(BitSet bSet, int version,
                                         boolean hasDictionary,
                                         boolean isEnd, int eventType) {
         int v = version; // version

         if (bSet != null) {
             for (int i=0; i < bSet.length(); i++) {
                 if (i > 23) {
                     break;
                 }
                 if (bSet.get(i)) {
                     v |= (0x1 << (8+i));
                 }
             }
         }

         v =  hasDictionary ? (v | 0x100) : v;
         v =  isEnd ? (v | 0x200) : v;
         v |= ((eventType & 0xf) << 10);

         return v;
     }

    /**
     * Decodes the padding and header type info.
     * @param word int to decode.
     */
    private void decodeBitInfoWord(int word) {
        // Padding
        compressedDataLengthPadding = (word >>> 24) & 0x3;
        dataLengthPadding           = (word >>> 22) & 0x3;
        userHeaderLengthPadding     = (word >>> 20) & 0x3;

        // Evio version
        headerVersion = (word & 0xff);

        // Header type
        headerType =  headerType.getHeaderType(word >>> 28);
//System.out.println("decodeBitInfoWord: header type = " + headerType);
        if (headerType == null) {
            headerType = headerType.EVIO_RECORD;
        }

        // Data type
        eventType = (word >> 11) & 0xf;
    }


    /**
     * Set the bit which says record has a first event in the user header.
     * @param hasFirst  true if record has a first event in the user header.
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
     * Set the bit which says record has a dictionary in the user header.
     * @param hasFirst  true if record has a dictionary in the user header.
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
     * Does this record have a dictionary in the user header?
     * @return true if record has a dictionary in the user header, else false.
     */
    public boolean hasDictionary() {return ((bitInfo & DICTIONARY_BIT) != 0);}

    /**
     * Does this bitInfo arg indicate the existence of a dictionary in the user header?
     * @param bitInfo bitInfo word.
     * @return true if header has a dictionary in the user header, else false.
     */
    static public boolean hasDictionary(int bitInfo) {return ((bitInfo & DICTIONARY_BIT) != 0);}

    /**
     * Set the bit which says record is last in file/buffer.
     * @param isLast  true if record is last in file/buffer.
     * @return new bitInfo word.
     */
    public int isLastRecord(boolean isLast) {
        if (isLast) {
            // set bit
            bitInfo |= LAST_RECORD_BIT;
        }
        else {
            // clear bit
            bitInfo &= ~LAST_RECORD_BIT;
        }

        return bitInfo;
    }

    /**
     * Is this the header of the last record?
     * @return true this is the header of the last record, else false.
     */
    public boolean isLastRecord() {return ((bitInfo & LAST_RECORD_BIT) != 0);}

    /**
     * Does this word indicate this is the header of the last record?
     * @param bitInfo bitInfo word.
     * @return true this is the header of the last record, else false.
     */
    static public boolean isLastRecord(int bitInfo) {return ((bitInfo & LAST_RECORD_BIT) != 0);}


    /**
     * Does this header indicate compressed data?
     * @return true if header indicates compressed data, else false.
     */
    public boolean isCompressed() {return compressionType != CompressionType.RECORD_UNCOMPRESSED;}

    /**
     * Is this header an evio trailer?
     * @return true if this is an evio trailer, else false.
     */
    public boolean isEvioTrailer() {return headerType == HeaderType.EVIO_TRAILER;}

    /**
     * Is this header a hipo trailer?
     * @return true if this is a hipo trailer, else false.
     */
    public boolean isHipoTrailer() {return headerType == HeaderType.HIPO_TRAILER;}


    /**
     * Is this header an evio record?
     * @return true if this is an evio record, else false.
     */
    public boolean isEvioRecord() {return headerType == HeaderType.EVIO_RECORD;}

    /**
     * Is this header a hipo record?
     * @return true if this is a hipo record, else false.
     */
    public boolean isHipoRecord() {return headerType == HeaderType.HIPO_RECORD;}

    /**
     * Does this arg indicate its header is an evio trailer?
     * @param bitInfo bitInfo word.
     * @return true if arg represents an evio trailer, else false.
     */
    static public boolean isEvioTrailer(int bitInfo) {
        return (bitInfo >>> 28) == HeaderType.EVIO_TRAILER.getValue();
    }

    /**
     * Does this arg indicate its header is a hipo trailer?
     * @param bitInfo bitInfo word.
     * @return true if arg represents a hipo trailer, else false.
     */
    static public boolean isHipoTrailer(int bitInfo) {
        return (bitInfo >>> 28) == HeaderType.HIPO_TRAILER.getValue();
    }

    /**
     * Does this arg indicate its header is an evio record?
     * @param bitInfo bitInfo word.
     * @return true if arg represents an evio record, else false.
     */
    static public boolean isEvioRecord(int bitInfo) {
        return (bitInfo >>> 28) == HeaderType.EVIO_RECORD.getValue();
    }

    /**
     * Does this arg indicate its header is a hipo record?
     * @param bitInfo bitInfo word.
     * @return true if arg represents a hipo record, else false.
     */
    static public boolean isHipoRecord(int bitInfo) {
        return (bitInfo >>> 28) == HeaderType.HIPO_RECORD.getValue();
    }


    /**
     * Clear the bit in the given arg to indicate it is NOT the last record.
     * @param i integer in which to clear the last-record bit
     * @return arg with last-record bit cleared
     */
    static public int clearLastRecordBit(int i) {return (i & ~LAST_RECORD_MASK);}

    /**
     * Set the bit info of a record header for a specified CODA event type.
     * Must be called AFTER {@link #setBitInfo(boolean, boolean, boolean)} or
     * {@link #setBitInfoWord(int)} in order to have change preserved.
     * @param type event type (0=ROC raw, 1=Physics, 2=Partial Physics,
     *             3=Disentangled, 4=User, 5=Control, 15=Other,
     *             else = nothing set).
     * @return new bit info word.
     */
    public int  setBitInfoEventType (int type) {
        switch(type) {
            case 0:
                bitInfo |= DATA_ROC_RAW_BITS;
                eventType = type;
                break;
            case 1:
                bitInfo |= DATA_PHYSICS_BITS;
                eventType = type;
                break;
            case 2:
                bitInfo |= DATA_PARTIAL_BITS;
                eventType = type;
                break;
            case 3:
                bitInfo |= DATA_DISENTANGLED_BITS;
                eventType = type;
                break;
            case 4:
                bitInfo |= DATA_USER_BITS;
                eventType = type;
                break;
            case 5:
                bitInfo |= DATA_CONTROL_BITS;
                eventType = type;
                break;
            case 15:
                bitInfo |= DATA_OTHER_BITS;
                eventType = type;
                break;
            default:
        }
        return bitInfo;
    }

    // Setters

    /**
     * Set this header's type. Normally done in constructor. Limited access.
     * @param type type of header.
     * @return this object.
     */
    RecordHeader  setHeaderType(HeaderType type) {

//        if (isCompressed() && type.isTrailer()) {
//System.out.println("Doesn't make sense to set trailer type of data is compressed");
//        }

        headerType = type;

        // Update bitInfo by first clearing then setting the 2 header type bits
        bitInfo = (bitInfo & (~HEADER_TYPE_MASK)) | (type.getValue() << 28);

        return this;
    }

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
     * Set the record length in bytes and words.
     * If length is not a multiple of 4, you're on your own!
     * @param length  length of record in bytes.
     * @return this object.
     */
    public RecordHeader  setLength(int length) {
        recordLength      = length;
        recordLengthWords = length/4;
//System.out.println("  RecordHeader: set LENGTH = " + recordLength + "  WORDS = " + recordLengthWords
//                   + "  SIZE = " + recordLengthWords*4 );
        return this;
    }

    /**
     * Set the uncompressed data length in bytes and words and the padding.
     * @param length  length of uncompressed data in bytes.
     * @return this object.
     */
    public RecordHeader  setDataLength(int length) {
        dataLength = length;
        dataLengthWords = getWords(length);
        dataLengthPadding = getPadding(length);

        // Update bitInfo by first clearing then setting the 2 padding bits
        bitInfo = (bitInfo & (~DATA_PADDING_MASK)) |
                 ((dataLengthPadding << 22) & DATA_PADDING_MASK);

        return this;
    }

    /**
     * Set the compressed data length in bytes and words and the padding.
     * @param length  length of compressed data in bytes.
     * @return this object.
     */
    public RecordHeader  setCompressedDataLength(int length) {
        compressedDataLength = length;
        compressedDataLengthWords = getWords(length);
        compressedDataLengthPadding = getPadding(length);

        // Update bitInfo by first clearing then setting the 2 padding bits
        bitInfo = (bitInfo & (~COMP_PADDING_MASK)) |
                 ((compressedDataLengthPadding << 24) & COMP_PADDING_MASK);

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
    public RecordHeader  setCompressionType(CompressionType type) {

//        if (type.isCompressed() && headerType.isTrailer()) {
//System.out.println("Doesn't make sense to set compressed data if trailer");
//        }

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
     * Set the user-defined header's length in bytes and words and the padding.
     * @param length  user-defined header's length in bytes.
     * @return this object.
     */
    public RecordHeader  setUserHeaderLength(int length) {
        userHeaderLength = length;
        userHeaderLengthWords   = getWords(length);
        userHeaderLengthPadding = getPadding(length);

        // Update bitInfo by first clearing then setting the 2 padding bits
        bitInfo = (bitInfo & (~USER_PADDING_MASK)) |
                 ((userHeaderLengthPadding << 20) & USER_PADDING_MASK);

        return this;
    }

    /**
     * Set the this header's length in bytes and words.
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
     * Position and limit of given buffer does NOT change.
     * @param buf byte buffer to write header into.
     * @param off position in buffer to begin writing.
     * @throws HipoException if buffer is null or contains too little room.
     */
    public void writeHeader(ByteBuffer buf, int off) throws HipoException {

        // Check arg
        if (buf == null || (buf.limit() - off) < HEADER_SIZE_BYTES) {
            throw new HipoException("buf null or too small");
        }

        int compressedWord =  (compressedDataLengthWords & 0x0FFFFFFF) |
                              (compressionType.getValue() << 28);

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
     * Position and limit of given buffer does NOT change.
     * @param buffer byte buffer to write header into.
     * @throws HipoException if buffer arg is null, contains too little room.
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
        writeTrailer(array, 0, recordNumber, order, null);
    }

    /**
     * Writes a trailer with an optional index array into the given byte array.
     * @param array byte array to write trailer into.
     * @param off   offset into array to start writing.
     * @param recordNumber record number of trailer.
     * @param order byte order of data to be written.
     * @param index list of record lengths interspersed with event counts
     *              to be written to trailer. Null if no index list.
     * @throws HipoException if array arg is null, array too small to hold trailer + index.
     */
    static public void writeTrailer(byte[] array, int off, int recordNumber,
                                    ByteOrder order, List<Integer> index)
            throws HipoException {

        int indexLength = 0;
        int wholeLength = HEADER_SIZE_BYTES;
        if (index != null) {
            indexLength = 4*index.size();
            wholeLength += indexLength;
        }

        // Check arg
        if (array == null || array.length < wholeLength + off) {
            throw new HipoException("null or too small array arg");
        }

        int bitInfo = (HeaderType.EVIO_TRAILER.getValue() << 28) | RecordHeader.LAST_RECORD_BIT | 6;

        try {
            // First the general header part
            ByteDataTransformer.toBytes(wholeLength/4,     order, array,      off); // 0*4
            ByteDataTransformer.toBytes(recordNumber,      order, array, 4  + off); // 1*4
            ByteDataTransformer.toBytes(HEADER_SIZE_WORDS, order, array, 8  + off); // 2*4
            ByteDataTransformer.toBytes(0,                 order, array, 12 + off); // 3*4
            ByteDataTransformer.toBytes(indexLength,       order, array, 16 + off); // 4*4
            ByteDataTransformer.toBytes(bitInfo,           order, array, 20 + off); // 5*4
            ByteDataTransformer.toBytes(0,                 order, array, 24 + off); // 6*4
            ByteDataTransformer.toBytes(HEADER_MAGIC,      order, array, 28 + off); // 7*4

            // The rest is all 0's, 8*4 (inclusive) -> 14*4 (exclusive)
            Arrays.fill(array, 32 + off, 56 + off, (byte)0);

            // Second the index
            if (indexLength > 0) {
                System.out.println("writeTrailer []: put index");
                for (int i=0; i < index.size(); i++) {
                    ByteDataTransformer.toBytes(index.get(i), order, array, 56+off+4*i);
                }
            }
        }
        catch (EvioException e) {/* never happen */}
    }


    /**
     * Writes a trailer with an optional index array into the given buffer.
     * @param buf   ByteBuffer to write trailer into.
     * @param off   offset into buffer to start writing.
     * @param recordNumber record number of trailer.
     * @param index list of record lengths interspersed with event counts
     *              to be written to trailer. Null if no index list.
     * @throws HipoException if buf arg is null, buf too small to hold trailer + index.
     */
    static public void writeTrailer(ByteBuffer buf, int off, int recordNumber,
                                    List<Integer> index)
            throws HipoException {

        int indexLength = 0;
        int wholeLength = HEADER_SIZE_BYTES;
        if (index != null) {
            indexLength = 4*index.size();
            wholeLength += indexLength;
        }

        // Check arg
        if (buf == null || (buf.capacity() < wholeLength + off)) {
            throw new HipoException("buf null or too small");
        }
System.out.println("writeTrailer buf: writing with order = " + buf.order());
        // Make sure the limit allows writing
        buf.limit(off + wholeLength).position(off);

        if (buf.hasArray()) {
            writeTrailer(buf.array(), buf.arrayOffset() + off, recordNumber, buf.order(), index);
            buf.position(buf.limit());
        }
        else {
            int bitInfo = (HeaderType.EVIO_TRAILER.getValue() << 28) | RecordHeader.LAST_RECORD_BIT | 6;

            // First the general header part
            buf.putInt(wholeLength/4);
            buf.putInt(recordNumber);
            buf.putInt(HEADER_SIZE_WORDS);
            buf.putInt(0);
            buf.putInt(0);
            buf.putInt(bitInfo);
            buf.putInt(0);
            buf.putInt(HEADER_MAGIC);
            buf.putLong(0L);
            buf.putLong(0L);
            buf.putLong(0L);

            // Second the index
            if (indexLength > 0) {
                //public ByteBuffer put(byte[] src, int offset, int length) {
                // relative bulk copy
System.out.println("writeTrailer buf: put index");
                for (int i : index) {
                    buf.putInt(i);
                }
            }
        }
    }


    /**
     * Writes an empty trailer into the given buffer.
     * @param buf   ByteBuffer to write trailer into.
     * @param recordNumber record number of trailer.
     * @throws HipoException if buf arg is null or too small to hold trailer
     */
    static public void writeTrailer(ByteBuffer buf, int recordNumber)
            throws HipoException {
        writeTrailer(buf, 0, recordNumber, null);
    }


    /**
     * Quickly check to see if this buffer contains compressed data or not.
     * The offset must point to the beginning of a valid hipo/evio record
     * in the buffer.
     *
     * @param buffer buffer to read from.
     * @param offset position of record header to be read.
     * @return true if data in record is compressed, else false.
     * @throws HipoException if buffer is null, contains too little data,
     *                       or is not in proper format.
     */
    static public boolean isCompressed(ByteBuffer buffer, int offset) throws HipoException {

        if (buffer == null || (buffer.limit() - offset) < 40) {
            throw new HipoException("buffer is null or too small");
        }

        // First read the magic word to establish endianness
        int headerMagicWord = buffer.getInt(MAGIC_OFFSET + offset);
        ByteOrder byteOrder;
        
        // If it's NOT in the proper byte order ...
        if (headerMagicWord != HEADER_MAGIC) {
            // If it needs to be switched ...
            if (headerMagicWord == Integer.reverseBytes(HEADER_MAGIC)) {
                if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                    byteOrder = ByteOrder.LITTLE_ENDIAN;
                }
                else {
                    byteOrder = ByteOrder.BIG_ENDIAN;
                }
                buffer.order(byteOrder);
            }
            else {
                throw new HipoException("buffer arg not in evio/hipo format, magic int = 0x" +
                                                Integer.toHexString(headerMagicWord));
            }
        }

        int compressionWord = buffer.getInt(COMPRESSION_TYPE_OFFSET + offset);
        return ((compressionWord >>> 28) != 0);
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
//System.out.println("RecordHeader.readHeader: buf lim = " + buffer.limit() +
//                ", cap = " + buffer.capacity() + ", pos = " + buffer.position());
        // First read the magic word to establish endianness
        headerMagicWord = buffer.getInt(MAGIC_OFFSET + offset);    // 7*4

        // If it's NOT in the proper byte order ...
        if (headerMagicWord != HEADER_MAGIC) {
            // If it needs to be switched ...
            if (headerMagicWord == Integer.reverseBytes(HEADER_MAGIC)) {
                if (buffer.order() == ByteOrder.BIG_ENDIAN) {
                    byteOrder = ByteOrder.LITTLE_ENDIAN;
                }
                else {
                    byteOrder = ByteOrder.BIG_ENDIAN;
                }
                buffer.order(byteOrder);
                headerMagicWord = HEADER_MAGIC;
//System.out.println("RecordHeader.readHeader: switch endian to " + byteOrder);
            }
            else {
                // ERROR condition, bad magic word
Utilities.printBuffer(buffer, 0, 40, "BAD MAGIC WORD BUFFER:");
                throw new HipoException("buffer arg not in evio/hipo format, magic int = 0x" +
                                                Integer.toHexString(headerMagicWord));
            }
        }
        else {
            byteOrder = buffer.order();
        }
//Utilities.printBuffer(buffer, 0, 40, "RecordHeader: BUFFER:");

        // Look at the bit-info word
        bitInfo = buffer.getInt(BIT_INFO_OFFSET + offset);   // 5*4

        // Set padding and header type
        decodeBitInfoWord(bitInfo);

        // Look at the version #
        if (headerVersion < 6) {
            throw new HipoException("buffer is in evio format version " + (bitInfo & 0xff));
        }

        recordLengthWords   = buffer.getInt(RECORD_LENGTH_OFFSET + offset);         //  0*4
        recordLength        = 4*recordLengthWords;
        recordNumber        = buffer.getInt( RECORD_NUMBER_OFFSET + offset);        //  1*4
        headerLengthWords   = buffer.getInt( HEADER_LENGTH_OFFSET + offset);        //  2*4
        setHeaderLength(4*headerLengthWords);
        entries             = buffer.getInt(EVENT_COUNT_OFFSET + offset);           //  3*4

        indexLength         = buffer.getInt(INDEX_ARRAY_OFFSET + offset);           //  4*4
        setIndexLength(indexLength);

        userHeaderLength    = buffer.getInt(USER_LENGTH_OFFSET + offset);           //  6*4
        setUserHeaderLength(userHeaderLength);

        // uncompressed data length
        dataLength          = buffer.getInt(UNCOMPRESSED_LENGTH_OFFSET + offset);   //  8*4
        setDataLength(dataLength);

        int compressionWord = buffer.getInt(COMPRESSION_TYPE_OFFSET + offset);      //  9*4
        compressionType     = CompressionType.getCompressionType(compressionWord >>> 28);
        compressedDataLengthWords = (compressionWord & 0x0FFFFFFF);
        compressedDataLengthPadding = (bitInfo >>> 24) & 0x3;
        compressedDataLength = compressedDataLengthWords*4 - compressedDataLengthPadding;
        recordUserRegisterFirst  = buffer.getLong(REGISTER1_OFFSET + offset);       // 10*4
        recordUserRegisterSecond = buffer.getLong(REGISTER2_OFFSET + offset);       // 12*4
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

    //-----------------------------------------------------
    // Additional methods for implementing IBlockHeader
    //-----------------------------------------------------

    /**
     * Returns a string representation of the record.
     * @return a string representation of the record.
     */
    @Override
    public String toString(){

        StringBuilder str = new StringBuilder();
        str.append(String.format("%24s : %d\n","version",headerVersion));
        str.append(String.format("%24s : %b\n","compressed", (compressionType.isCompressed())));
        str.append(String.format("%24s : %d\n","record #",recordNumber));

        str.append(String.format("%24s :     bytes,     words,    padding\n",""));
        str.append(String.format("%24s : %8d  %8d  %8d\n","user header length",
                                 userHeaderLength, userHeaderLengthWords, userHeaderLengthPadding));
        str.append(String.format("%24s : %8d  %8d  %8d\n","uncompressed data length",
                                 dataLength, dataLengthWords, dataLengthPadding));
        str.append(String.format("%24s : %8d  %8d\n","record length",
                                 recordLength, recordLengthWords));
        str.append(String.format("%24s : %8d  %8d  %8d\n","compressed length",
                                 compressedDataLength, compressedDataLengthWords,
                                 compressedDataLengthPadding));
        str.append(String.format("%24s : %d\n","header length",headerLength));
        str.append(String.format("%24s : %d\n","index length",indexLength));

        str.append(String.format("%24s : 0x%X\n","magic word",headerMagicWord));
        str.append(String.format("%24s : %s\n","bit info word bin",Integer.toBinaryString(bitInfo)));
        str.append(String.format("%24s : 0x%s\n","bit info word hex",Integer.toHexString(bitInfo)));
        str.append(String.format("%24s : %b\n","has dictionary",hasDictionary()));
        str.append(String.format("%24s : %b\n","has 1st event",hasFirstEvent()));
        str.append(String.format("%24s : %b\n","is last record",isLastRecord()));
        str.append(String.format("%24s : %s (%d)\n","data type", eventTypeToString(), eventType));


        str.append(String.format("%24s : %d\n","event count",entries));
        str.append(String.format("%24s : %s\n","compression type",compressionType));

        str.append(String.format("%24s : %d\n","user register #1",recordUserRegisterFirst));
        str.append(String.format("%24s : %d\n","user register #2",recordUserRegisterSecond));

        return str.toString();
    }

    /** {@inheritDoc} */
    public int getSize() {return recordLengthWords;}

    /** {@inheritDoc} */
    public int getNumber() {return recordNumber;}

    /** {@inheritDoc} */
    public int getMagicNumber() {return headerMagicWord;}

    /** {@inheritDoc} */
    public boolean isLastBlock() {return isLastRecord();}

    /** {@inheritDoc} */
    public int getSourceId() {return (int)recordUserRegisterFirst;}

    /** {@inheritDoc} */
    public int getEventType() {return eventType;}

    /**
     * Return  a meaningful string associated with event type.
     * @return a meaningful string associated with event type.
     */
    private String eventTypeToString() {
        switch (eventType) {
            case 0:
                return "ROC Raw";
            case 1:
                return "Physics";
            case 2:
                return "Partial Physics";
            case 3:
                return "Disentangled";
            case 4:
                return "User";
            case 5:
                return "Control";
            case 15:
                return "Other";
            default:
                return "Unknown";
        }
    }

    /** {@inheritDoc} */
    public int write(ByteBuffer byteBuffer) {
        try {
            writeHeader(byteBuffer, byteBuffer.position());
        }
        catch (HipoException e) {
            System.out.println("RecordHeader.write(): buffer is null or contains too little room");
            return 0;
        }
        return HEADER_SIZE_BYTES;
    }

    // Following methods are not used in this class but must be part of IBlockHeader interface

    /** {@inheritDoc} */
    public long getBufferEndingPosition() {return 0L;}

    /** {@inheritDoc} */
    public long getBufferStartingPosition() {return 0L;}

    /** {@inheritDoc} */
    public void setBufferStartingPosition(long bufferStartingPosition) {}

    /** {@inheritDoc} */
    public long nextBufferStartingPosition() {return 0L;}

    /** {@inheritDoc} */
    public long firstEventStartingPosition() {return 0L;}

    /** {@inheritDoc} */
    public int bytesRemaining(long position) throws EvioException {return 0;}

    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

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
     * @param buffer byffer to print out.
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
     * @param args args.
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
        header.setCompressionType(CompressionType.RECORD_COMPRESSION_LZ4);
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
