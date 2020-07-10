/**
 * Copyright (c) 2019, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 04/09/2019
 * @author timmer
 */


#ifndef EVIO_6_0_RECORDHEADER_H
#define EVIO_6_0_RECORDHEADER_H


#include <cstdint>
#include <string>
#include <bitset>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <memory>


#include "HeaderType.h"
#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "IBlockHeader.h"
#include "EvioException.h"
#include "Compressor.h"
#include "Util.h"


namespace evio {


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

class RecordHeader : public IBlockHeader {

private:

    /** Array to help find number of bytes to pad data. */
    static uint32_t padValue[4];

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


    //-------------------

    /** First user-defined 64-bit register. 11th and 12th words. */
    uint64_t recordUserRegisterFirst = 0ULL;
    /** Second user-defined 64-bit register. 13th and 14th words. */
    uint64_t recordUserRegisterSecond = 0ULL;
    /** Position of this header in a file. */
    size_t position = 0ULL;
    /** Length of the entire record this header is a part of (bytes). */
    uint32_t  recordLength = 0;
    /** Length of the entire record this header is a part of (32-bit words). 1st word. */
    uint32_t  recordLengthWords = 0;
    /** Record number. 2nd word. */
    uint32_t  recordNumber = 1;


    /** Event or record count. 4th word. */
    uint32_t  entries = 0;
    /** BitInfo & version. 6th word. */
    uint32_t  bitInfo = 0;
    /**
     * Type of events in record, encoded in bitInfo word
     * (0=ROC raw, 1=Physics, 2=Partial Physics, 3=Disentangled,
     * 4=User, 5=Control, 15=Other).
     */
    uint32_t  eventType = 0;
    /** Length of this header NOT including user header or index (bytes). */
    uint32_t  headerLength = HEADER_SIZE_BYTES;
    /** Length of this header (words). 3rd word. */
    uint32_t  headerLengthWords = HEADER_SIZE_WORDS;
    /** Length of user-defined header (bytes). 7th word. */
    uint32_t  userHeaderLength = 0;
    /** Length of user-defined header when padded (words). */
    uint32_t  userHeaderLengthWords = 0;
    /** Length of index array (bytes). 5th word. */
    uint32_t  indexLength = 0;
    /** Uncompressed data length (bytes). 9th word. */
    uint32_t  dataLength = 0;
    /** Uncompressed data length when padded (words). */
    uint32_t  dataLengthWords = 0;
    /** Compressed data length (bytes). */
    uint32_t  compressedDataLength = 0;
    /** Compressed data length (words) when padded. Lowest 28 bits of 10th word. */
    uint32_t  compressedDataLengthWords = 0;
    /** Evio format version number. It is 6 when being written, else
     * the version of file/buffer being read. Lowest byte of 6th word. */
    uint32_t  headerVersion = 6;
    /** Magic number for tracking endianness. 8th word. */
    uint32_t  headerMagicWord = HEADER_MAGIC;

    /** Number of bytes required to bring uncompressed
      * user header to 4-byte boundary. Stored in 6th word.
      * Updated automatically when lengths are set. */
    uint32_t  userHeaderLengthPadding = 0;
    /** Number of bytes required to bring uncompressed
     * data to 4-byte boundary. Stored in 6th word.
     * Updated automatically when lengths are set. */
    uint32_t  dataLengthPadding = 0;
    /** Number of bytes required to bring compressed
     * data to 4-byte boundary. Stored in 6th word.
     * Updated automatically when lengths are set. */
    uint32_t  compressedDataLengthPadding = 0;

    /** Type of header this is. Normal EVIO record by default. */
    HeaderType headerType {HeaderType::EVIO_RECORD};

    /** Byte order of file/buffer this header was read from. */
    ByteOrder byteOrder {ByteOrder::ENDIAN_LITTLE};

    /** Type of data compression (0=none, 1=LZ4 fast, 2=LZ4 best, 3=gzip).
      * Highest 4 bits of 10th word. */
    Compressor::CompressionType compressionType {Compressor::UNCOMPRESSED};


public:

    RecordHeader();
    RecordHeader(const RecordHeader & header);
    explicit RecordHeader(const HeaderType & type);
    RecordHeader(long pos, int l, int e);
    ~RecordHeader() = default;

    RecordHeader & operator=(const RecordHeader& head);

private:

    void bitInfoInit();
    void decodeBitInfoWord(uint32_t word);

public:

    void copy(std::shared_ptr<RecordHeader> const & head);
    void reset();

    static uint32_t getWords(int length);
    static uint32_t getPadding(int length);

    // Getters

    const HeaderType & getHeaderType() const;
    // getByteOrder part of IBlockHeader below ...

    uint32_t  getUncompressedRecordLength() const;
    uint32_t  getCompressedRecordLength() const;
    uint32_t  getLength() const;
    uint32_t  getLengthWords() const;
    uint32_t  getEntries() const;
    uint32_t  getUserHeaderLength() const;
    uint32_t  getUserHeaderLengthWords() const;
    //  getVersion() part of IBlockHeader below ...
    uint32_t  getDataLength() const;
    uint32_t  getDataLengthWords() const;
    uint32_t  getIndexLength() const;
    uint32_t  getCompressedDataLength() const;
    uint32_t  getCompressedDataLengthPadding() const;
    uint32_t  getCompressedDataLengthWords() const;
    uint32_t  getHeaderLength() const;
    // getHeaderWords part of IBlockHeader below ...
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

    uint32_t    hasFirstEvent(bool hasFirst);
    // hasFirstEvent() is part of IBlockHeader below

    uint32_t    hasDictionary(bool hasFirst);
    // hasDictionary() part of IBlockHeader below ...
    static bool hasDictionary(int bitInfo);

    uint32_t    isLastRecord(bool isLast);
    bool        isLastRecord() const;
    static bool isLastRecord(uint32_t bitInfo);

    bool        isCompressed() const;

    bool        isEvioTrailer() const;
    bool        isEvioRecord()  const;
    bool        isHipoTrailer() const;
    bool        isHipoRecord()  const;

    static bool isEvioTrailer(uint32_t bitInfo);
    static bool isEvioRecord(uint32_t bitInfo);
    static bool isHipoTrailer(uint32_t bitInfo);
    static bool isHipoRecord(uint32_t bitInfo);

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


    void writeHeader(ByteBuffer & buf, size_t off);
    void writeHeader(ByteBuffer & buffer);
    void writeHeader(std::shared_ptr<ByteBuffer> & buffer, size_t off);

    static void writeTrailer(uint8_t* array, size_t arrayLen, size_t off,
                             uint32_t recordNum, const ByteOrder & order);

    static void writeTrailer(uint8_t* array, size_t arrayLen, size_t off,
                             uint32_t recordNum, const ByteOrder & order,
                             const std::shared_ptr<std::vector<uint32_t>> & recordLengths);

    static void writeTrailer(std::vector<uint8_t> & array, size_t off,
                             uint32_t recordNum, const ByteOrder & order);

    static void writeTrailer(std::vector<uint8_t> & array, size_t off,
                             uint32_t recordNum, const ByteOrder & order,
                             const std::shared_ptr<std::vector<uint32_t>> & recordLengths);

    static void writeTrailer(ByteBuffer & buf, size_t off, uint32_t recordNum);
    static void writeTrailer(ByteBuffer & buf, size_t off, uint32_t recordNum,
                             const std::shared_ptr<std::vector<uint32_t>> & recordLengths);

    static bool isCompressed(ByteBuffer & buffer, size_t offset);

    void readHeader(ByteBuffer & buffer, size_t offset);
    void readHeader(ByteBuffer & buffer);

    std::string eventTypeToString() const ;

    // Methods for implementing IBlockHeader

    uint32_t getSize() override ;
    uint32_t getNumber() override ;
    uint32_t getHeaderWords() override ;
    uint32_t getSourceId() override ;
    bool     hasFirstEvent() override ;
    uint32_t getEventType() override ;
    uint32_t getVersion() override ;
    uint32_t getMagicNumber() override ;
    ByteOrder & getByteOrder() override ;

    bool         hasDictionary() override ;
    bool         isLastBlock() override ;
    std::string  toString() override ;
    size_t       write(ByteBuffer & byteBuffer) override ;

    // Methods are not used in this class but must be part of IBlockHeader interface

    size_t getBufferEndingPosition() override;
    size_t getBufferStartingPosition() override;
    void   setBufferStartingPosition(size_t bufferStartingPosition) override;
    size_t nextBufferStartingPosition() override;
    size_t firstEventStartingPosition() override;
    size_t bytesRemaining(size_t pos) override;

    //-/////////////////////////////////////////////

    int main(int argc, char **argv);

};

}


#endif //EVIO_6_0_RECORDHEADER_H
