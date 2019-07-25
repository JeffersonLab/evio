/*
 * Copyright (c) 2019, Jefferson Science Associates, all rights reserved.
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000 Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 */


#ifndef EVIO_6_0_RECORDHEADER_H
#define EVIO_6_0_RECORDHEADER_H

#include <cstdint>
#include <string>
#include <bitset>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <bitset>

#include "HeaderType.h"
#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "HipoException.h"
#include "Compressor.h"

using std::string;

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
 * @since 6.0 4/9/19
 * @author timmer
 */
//class RecordHeader /*: IBlockHeader*/ {
class RecordHeader {

public:

    /** Number of 32-bit words in a normal sized header. */
    static const uint32_t   HEADER_SIZE_WORDS = 14;
    /** Number of bytes in a normal sized header. */
    static const uint32_t   HEADER_SIZE_BYTES = 56;
    /** Magic number used to track endianness. */
    static const uint32_t   HEADER_MAGIC = 0xc0da0100;

    // Byte offset to header words

    /** Byte offset from beginning of header to the record length. */
    static const uint32_t   RECORD_LENGTH_OFFSET = 0;
    /** Byte offset from beginning of header to the record number. */
    static const uint32_t   RECORD_NUMBER_OFFSET = 4;
    /** Byte offset from beginning of header to the header length. */
    static const uint32_t   HEADER_LENGTH_OFFSET = 8;
    /** Byte offset from beginning of header to the event index count. */
    static const uint32_t   EVENT_COUNT_OFFSET = 12;
    /** Byte offset from beginning of header to the index array length. */
    static const uint32_t   INDEX_ARRAY_OFFSET = 16;
    /** Byte offset from beginning of header to bit info word. */
    static const uint32_t   BIT_INFO_OFFSET = 20;
    /** Byte offset from beginning of header to the user header length. */
    static const uint32_t   USER_LENGTH_OFFSET = 24;
    /** Byte offset from beginning of header to the record length. */
    static const uint32_t   MAGIC_OFFSET = 28;
    /** Byte offset from beginning of header to the uncompressed data length. */
    static const uint32_t   UNCOMPRESSED_LENGTH_OFFSET = 32;
    /** Byte offset from beginning of header to the compression type & compressed data length word. */
    static const uint32_t   COMPRESSION_TYPE_OFFSET = 36;
    /** Byte offset from beginning of header to the user register #1. */
    static const uint32_t   REGISTER1_OFFSET = 40;
    /** Byte offset from beginning of header to the user register #2. */
    static const uint32_t   REGISTER2_OFFSET = 48;

    // Bits in bit info word

    /** 8th bit set in bitInfo word in header means contains dictionary. */
    static const uint32_t   DICTIONARY_BIT = 0x100;
    /** 9th bit set in bitInfo word in header means every split file has same first event. */
    static const uint32_t   FIRST_EVENT_BIT = 0x200;
    /** 10th bit set in bitInfo word in header means is last in stream or file. */
    static const uint32_t   LAST_RECORD_BIT = 0x400;

    /** 11-14th bits in bitInfo word in header for CODA data type, ROC raw = 0. */
    static const uint32_t   DATA_ROC_RAW_BITS = 0x000;
    /** 11-14th bits in bitInfo word in header for CODA data type, physics = 1. */
    static const uint32_t   DATA_PHYSICS_BITS = 0x800;
    /** 11-14th bits in bitInfo word in header for CODA data type, partial physics = 2. */
    static const uint32_t   DATA_PARTIAL_BITS = 0x1000;
    /** 11-14th bits in bitInfo word in header for CODA data type, disentangled = 3. */
    static const uint32_t   DATA_DISENTANGLED_BITS = 0x1800;
    /** 11-14th bits in bitInfo word in header for CODA data type, user = 4. */
    static const uint32_t   DATA_USER_BITS    = 0x2000;
    /** 11-14th bits in bitInfo word in record header for CODA data type, control = 5. */
    static const uint32_t   DATA_CONTROL_BITS = 0x2800;
    /** 11-14th bits in bitInfo word in record header for CODA data type, other = 15. */
    static const uint32_t   DATA_OTHER_BITS   = 0x7800;

    // Bit masks

    /** Mask to get version number from 6th int in header. */
    static const uint32_t VERSION_MASK = 0xff;
    /** "Last record" is 11th bit in bitInfo word. */
    static const uint32_t LAST_RECORD_MASK = 0x400;

private:

    /** Compressed data padding mask. */
    static const uint32_t COMP_PADDING_MASK = 0x03000000;
    /** Uncompressed data padding mask. */
    static const uint32_t DATA_PADDING_MASK = 0x00C00000;
    /** User header padding mask. */
    static const uint32_t USER_PADDING_MASK = 0x00300000;
    /** Header type mask. */
    static const uint32_t HEADER_TYPE_MASK = 0xF0000000;

    /** Array to help find number of bytes to pad data. */
    static constexpr uint32_t padValue[4] = {0,3,2,1};

    //-------------------

    /** Position of this header in a file. */
    size_t position;
    /** Length of the entire record this header is a part of (bytes). */
    uint32_t  recordLength;
    /** Length of the entire record this header is a part of (32-bit words). 1st word. */
    uint32_t  recordLengthWords;
    /** Record number. 2nd word. */
    uint32_t  recordNumber;
    /** First user-defined 64-bit register. 11th and 12th words. */
    uint64_t recordUserRegisterFirst;
    /** Second user-defined 64-bit register. 13th and 14th words. */
    uint64_t recordUserRegisterSecond;


    /** Type of header this is. Normal HIPO record by default. */
    HeaderType headerType = HeaderType::HIPO_RECORD;
    /** Event or record count. 4th word. */
    uint32_t  entries;
    /** BitInfo & version. 6th word. */
    uint32_t  bitInfo;
    /**
     * Type of events in record, encoded in bitInfo word
     * (0=ROC raw, 1=Physics, 2=Partial Physics, 3=Disentangled,
     * 4=User, 5=Control, 15=Other).
     */
    uint32_t  eventType;
    /** Length of this header NOT including user header or index (bytes). */
    uint32_t  headerLength = HEADER_SIZE_BYTES;
    /** Length of this header (words). 3rd word. */
    uint32_t  headerLengthWords = HEADER_SIZE_WORDS;
    /** Length of user-defined header (bytes). 7th word. */
    uint32_t  userHeaderLength;
    /** Length of user-defined header when padded (words). */
    uint32_t  userHeaderLengthWords;
    /** Length of index array (bytes). 5th word. */
    uint32_t  indexLength;
    /** Uncompressed data length (bytes). 9th word. */
    uint32_t  dataLength;
    /** Uncompressed data length when padded (words). */
    uint32_t  dataLengthWords;
    /** Compressed data length (bytes). */
    uint32_t  compressedDataLength;
    /** Compressed data length (words) when padded. Lowest 28 bits of 10th word. */
    uint32_t  compressedDataLengthWords;
    /** Type of data compression (0=none, 1=LZ4 fast, 2=LZ4 best, 3=gzip).
      * Highest 4 bits of 10th word. */
    Compressor::CompressionType  compressionType;
    /** Evio format version number. It is 6 when being written, else
     * the version of file/buffer being read. Lowest byte of 6th word. */
    uint32_t  headerVersion = 6;
    /** Magic number for tracking endianness. 8th word. */
    uint32_t  headerMagicWord = HEADER_MAGIC;
    /** Byte order of file/buffer this header was read from. */
    ByteOrder byteOrder = ByteOrder::ENDIAN_LITTLE;

    // These quantities are updated automatically when lengths are set

    /** Number of bytes required to bring uncompressed
      * user header to 4-byte boundary. Stored in 6th word. */
    uint32_t  userHeaderLengthPadding;
    /** Number of bytes required to bring uncompressed
      * data to 4-byte boundary. Stored in 6th word. */
    uint32_t  dataLengthPadding;
    /** Number of bytes required to bring compressed
      * data to 4-byte boundary. Stored in 6th word. */
    uint32_t  compressedDataLengthPadding;


public:

    RecordHeader();
    RecordHeader(const RecordHeader & header);
    explicit RecordHeader(const HeaderType & type);
    RecordHeader(long pos, int l, int e);
    ~RecordHeader() = default;

private:

    void bitInfoInit();
    void decodeBitInfoWord(uint32_t word);

public:

    void reset();

    static uint32_t getWords(int length);
    static uint32_t getPadding(int length);

    // Getters

    const HeaderType & getHeaderType() const;
    const ByteOrder  & getByteOrder() const;

    uint32_t  getUncompressedRecordLength() const;
    uint32_t  getCompressedRecordLength() const;
    uint32_t  getLength() const;
    uint32_t  getLengthWords() const;
    uint32_t  getEntries() const;
    uint32_t  getUserHeaderLength() const;
    uint32_t  getUserHeaderLengthWords() const;
    uint32_t  getVersion() const;
    uint32_t  getDataLength() const;
    uint32_t  getDataLengthWords() const;
    uint32_t  getIndexLength() const;
    uint32_t  getCompressedDataLength() const;
    uint32_t  getCompressedDataLengthPadding() const;
    uint32_t  getCompressedDataLengthWords() const;
    uint32_t  getHeaderLength() const;
    uint32_t  getHeaderWords() const;
    uint32_t  getRecordNumber() const;
    uint64_t  getUserRegisterFirst() const;
    uint64_t  getUserRegisterSecond() const;
    size_t    getPosition() const;
    Compressor::CompressionType  getCompressionType() const;

    // Bit info methods

    uint32_t  setBitInfo(bool isLastRecord, bool haveFirstEvent, bool haveDictionary);
    uint32_t  getBitInfoWord() const;
    void setBitInfoWord(uint32_t word);
    void setBitInfoWord(std::bitset<24> const & set);

    static uint32_t generateSixthWord(std::bitset<24> const & set);
    static uint32_t generateSixthWord(uint32_t version, bool hasDictionary,
                                      bool isEnd, uint32_t eventType);
    static uint32_t generateSixthWord(std::bitset<24> const & set, uint32_t version,
                                      bool hasDictionary, bool isEnd, uint32_t uint32_t);

    // Boolean setters/getters

    uint32_t  hasFirstEvent(bool hasFirst);
    bool      hasFirstEvent() const;

    uint32_t   hasDictionary(bool hasFirst);
    bool        hasDictionary() const;
    static bool hasDictionary(int bitInfo);

    uint32_t    isLastRecord(bool isLast);
    bool        isLastRecord() const;
    static bool isLastRecord(int bitInfo);


    // Setters

    static uint32_t clearLastRecordBit(uint32_t i);
    uint32_t  setBitInfoEventType (uint32_t type);

    RecordHeader & setHeaderType(HeaderType const & type);
    RecordHeader & setPosition(size_t pos);
    RecordHeader & setRecordNumber(uint32_t num);
    RecordHeader & setLength(uint32_t length);
    RecordHeader & setDataLength(uint32_t length);
    RecordHeader & setCompressedDataLength(uint32_t length);
    RecordHeader & setIndexLength(uint32_t length);
    RecordHeader & setCompressionType(Compressor::CompressionType type);
    RecordHeader & setEntries(uint32_t n);
    RecordHeader & setUserHeaderLength(uint32_t length);
    RecordHeader & setHeaderLength(uint32_t length);
    RecordHeader & setUserRegisterFirst(uint64_t reg);
    RecordHeader & setUserRegisterSecond(uint64_t reg);


    static void toBytes(uint32_t data, const ByteOrder & byteOrder,
                        uint8_t* dest, size_t destLen, size_t off);

    void writeHeader(ByteBuffer & buf, size_t off);
    void writeHeader(ByteBuffer & buffer);

    static void writeTrailer(uint8_t* array, size_t arrayLen, size_t off,
                             uint32_t recordNumber, const ByteOrder & order);

    static void writeTrailer(uint8_t* array, size_t arrayLen, size_t off,
                             uint32_t recordNumber, const ByteOrder & order,
                             const uint32_t* index, size_t indexLength);

    static void writeTrailer(ByteBuffer & buf, size_t off, uint32_t recordNumber);
    static void writeTrailer(ByteBuffer & buf, size_t off, uint32_t recordNumber,
                             const uint32_t* index, size_t indexLen);


    static bool isCompressed(ByteBuffer & buffer, size_t offset);
    void readHeader(ByteBuffer & buffer, size_t offset);
    void readHeader(ByteBuffer & buffer);


    // Methods for implementing IBlockHeader

    uint32_t getSize() const;
    uint32_t getNumber() const;
    uint32_t getMagicNumber() const;
    uint32_t getSourceId() const;
    uint32_t getEventType() const;
    bool     isLastBlock() const;
    string   eventTypeToString() const;
    string   toString() const;
    uint32_t write(ByteBuffer & byteBuffer);

    // Methods are not used in this class but must be part of IBlockHeader interface

    size_t getBufferEndingPosition() const;
    size_t getBufferStartingPosition() const;
    void   setBufferStartingPosition(size_t bufferStartingPosition);
    size_t nextBufferStartingPosition() const;
    size_t firstEventStartingPosition() const;
    size_t bytesRemaining(size_t pos) const;

};


#endif //EVIO_6_0_RECORDHEADER_H
