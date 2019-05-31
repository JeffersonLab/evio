/*
 * Copyright (c) 2019, Jefferson Science Associates, all rights reserved.
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000 Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 */

//
// Created by timmer on 4/9/19.
//


#include "RecordHeader.h"

using namespace std;

/** Default, no-arg constructor. */
RecordHeader::RecordHeader() {
    headerType = HeaderType::EVIO_FILE;
    reset();
    compressionType = Compressor::UNCOMPRESSED;
    bitInfoInit();
}

/**
 * Constructor which sets the type of header this is.
 * @param type  type of header this is
 * @throws HipoException if type is for file
 */
RecordHeader::RecordHeader(const HeaderType & type) {

    headerType = type;

    if (headerType.isFileHeader()) {
        throw HipoException("RecordHeader cannot be set to FileHeader type");
    }

    reset();
    compressionType = Compressor::UNCOMPRESSED;
    bitInfoInit();
}

/**
 * Constructor.
 * @param pos   position in file.
 * @param len   length of record in bytes
 * @param ent   number of events
 */
RecordHeader::RecordHeader(long pos, int len, int ent) {
    headerType = HeaderType::EVIO_FILE;
    reset();
    compressionType = Compressor::UNCOMPRESSED;
    bitInfoInit();

    position = pos;
    recordLength = len;
    entries = ent;
}

/**
 * Copy constructor.
 * @param header  header to copy.
 */
RecordHeader::RecordHeader(const RecordHeader & head) {
    position                 = head.position;

    recordLength             = head.recordLength;
    recordNumber             = head.recordNumber;
    recordLengthWords        = head.recordLengthWords;
    recordUserRegisterFirst  = head.recordUserRegisterFirst;
    recordUserRegisterSecond = head.recordUserRegisterSecond;

    headerType                = head.headerType;
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
    compressionType           = head.compressionType;
    headerMagicWord           = head.headerMagicWord;
    byteOrder                 = head.byteOrder;
    // don't bother with version as must be same

    userHeaderLengthPadding     = head.userHeaderLengthPadding;
    dataLengthPadding           = head.dataLengthPadding;
    compressedDataLengthPadding = head.compressedDataLengthPadding;
}

/** Reset generated data. */
void RecordHeader::reset() {
    // Do NOT reset header type which is only set in constructor!
    // Do NOT reset the compression type
    position = 0ULL;

    recordLength = 0;
    recordNumber = 0;
    recordLengthWords = 0;
    recordUserRegisterFirst = 0ULL;
    recordUserRegisterSecond = 0ULL;

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
    // TODO: what about byteOrder???
    byteOrder = ByteOrder::ENDIAN_LITTLE;

    userHeaderLengthPadding = 0;
    dataLengthPadding = 0;
    compressedDataLengthPadding = 0;
}

/**
 * Returns length padded to 4-byte boundary for given length in bytes.
 * @param length length in bytes.
 * @return length in bytes padded to 4-byte boundary.
 */
uint32_t RecordHeader::getWords(int length) {
    uint32_t words = length/4;
    if (getPadding(length) > 0) words++;
    return words;
}

/**
 * Returns number of bytes needed to pad to 4-byte boundary for the given length.
 * @param length length in bytes.
 * @return number of bytes needed to pad to 4-byte boundary.
 */
uint32_t RecordHeader::getPadding(int length) {return padValue[length%4];}


// Getters

/**
 * Get the padded length in bytes of the entire uncompressed record.
 * @return padded length in bytes of the entire uncompressed record.
 */
uint32_t RecordHeader::getUncompressedRecordLength() const {
    return (headerLength + indexLength + userHeaderLength + dataLength +
            userHeaderLengthPadding + dataLengthPadding);
}

/**
 * Get the padded length in bytes of the entire compressed record.
 * If the data is not compressed, then this returns -1;
 * @return padded length in bytes of the entire compressed record, else -1 if not compressed.
 */
uint32_t RecordHeader::getCompressedRecordLength() const {
    if (compressionType != Compressor::UNCOMPRESSED) {
        return (recordLength + compressedDataLengthPadding);
    }
    return  -1;
}

/**
 * Get the byte order of the file/buffer this header was read from.
 * Defaults to little endian.
 * @return byte order of the file/buffer this header was read from.
 */
const ByteOrder & RecordHeader::getByteOrder() const {return byteOrder;}

/**
 * Get the position of this record in a file.
 * @return position of this record in a file.
 */
size_t  RecordHeader::getPosition() const {return position;}

/**
 * Get the total length of this record in bytes.
 * @return total length of this record in bytes.
 */
uint32_t  RecordHeader::getLength() const {return recordLength;}

/**
 * Get the total length of this record in 32 bit words.
 * @return total length of this record in 32 bit words.
 */
uint32_t  RecordHeader::getLengthWords() const {return recordLengthWords;}

/**
 * Get the number of events or entries in index.
 * @return number of events or entries in index.
 */
uint32_t  RecordHeader::getEntries() const {return entries;}

/**
 * Get the type of compression used. 0=none, 1=LZ4 fast, 2=LZ4 best, 3=gzip.
 * @return type of compression used.
 */
Compressor::CompressionType  RecordHeader::getCompressionType() const {return compressionType;}

/**
 * Get the length of the user-defined header in bytes.
 * @return length of the user-defined header in bytes.
 */
uint32_t  RecordHeader::getUserHeaderLength() const {return userHeaderLength;}

/**
 * Get the length of the user-defined header in words.
 * @return length of the user-defined header in words.
 */
uint32_t  RecordHeader::getUserHeaderLengthWords() const {return userHeaderLengthWords;}

/**
 * Get the Evio format version number.
 * @return Evio format version number.
 */
uint32_t  RecordHeader::getVersion() const {return headerVersion;}

/**
 * Get the length of the uncompressed data in bytes.
 * @return length of the uncompressed data in bytes.
 */
uint32_t  RecordHeader::getDataLength() const {return dataLength;}

/**
 * Get the length of the uncompressed data in words (padded).
 * @return length of the uncompressed data in words (padded).
 */
uint32_t  RecordHeader::getDataLengthWords() const {return dataLengthWords;}

/**
 * Get the length of the index array in bytes.
 * @return length of the index array in bytes.
 */
uint32_t  RecordHeader::getIndexLength() const {return indexLength;}

/**
 * Get the length of the compressed data in bytes.
 * @return length of the compressed data in bytes.
 */
uint32_t  RecordHeader::getCompressedDataLength() const {return compressedDataLength;}

/**
 * Get the padding of the compressed data in bytes.
 * @return padding of the compressed data in bytes.
 */
uint32_t  RecordHeader::getCompressedDataLengthPadding() const {return compressedDataLengthPadding;}

/**
 * Get the length of the compressed data in words (padded).
 * @return length of the compressed data in words (padded).
 */
uint32_t  RecordHeader::getCompressedDataLengthWords() const {return compressedDataLengthWords;}

/**
 * Get the length of this header data in bytes (NOT including user header or index).
 * @return length of this header data in bytes.
 */
uint32_t  RecordHeader::getHeaderLength() const {return headerLength;}

/**
 * Get the length of this header data in words.
 * @return length of this header data in words.
    */
uint32_t  RecordHeader::getHeaderWords() const {return headerLengthWords;}

/**
 * Get the record number.
 * @return record number.
 */
uint32_t  RecordHeader::getRecordNumber() const {return recordNumber;}

/**
 * Get the first user-defined 64-bit register.
 * @return first user-defined 64-bit register.
 */
uint64_t  RecordHeader::getUserRegisterFirst() const {return recordUserRegisterFirst;}

/**
 * Get the second user-defined 64-bit register.
 * @return second user-defined 64-bit register.
 */
uint64_t  RecordHeader::getUserRegisterSecond() const {return recordUserRegisterSecond;}

/**
 * Get the type of header this is.
 * @return type of header this is.
 */
const HeaderType & RecordHeader::getHeaderType() const {return headerType;}

// Bit info methods

/** Initialize bitInfo word to this value. */
void RecordHeader::bitInfoInit() {
    bitInfo = headerType.getValue() << 28 | (headerVersion & 0xFF);
}

/**
 * Set the bit info word for a record header.
 * Current value of bitInfo is lost.
 * @param isLastRecord   true if record is last in stream or file.
 * @param haveDictionary true if record has dictionary in user header.
 * @return new bit info word.
 */
uint32_t  RecordHeader::setBitInfo(bool isLastRecord,
                bool haveFirstEvent,
                bool haveDictionary) {

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
uint32_t RecordHeader::getBitInfoWord() const {return bitInfo;}

/**
 * Set the bit info word and related values.
 * NOT FOR GENERAL USE!
 * @param word  bit info word.
 */
void RecordHeader::setBitInfoWord(uint32_t word) {
    bitInfo = word;
    decodeBitInfoWord(word);
}

/**
 * Set the bit info word and related values.
 * NOT FOR GENERAL USE!
 * @param set reference to bitset containing all bits to be set
 */
void RecordHeader::setBitInfoWord(std::bitset<24> const & set) {
    bitInfo = generateSixthWord(set);
    decodeBitInfoWord(bitInfo);
}


/**
 * Calculates the sixth word of this header which has the version
 * number (6) in the lowest 8 bits and the set in the upper 24 bits.
 * NOT FOR GENERAL USE!
 *
 * @param set reference to bitset containing all bits to be set
 * @return generated sixth word of this header.
 */
uint32_t RecordHeader::generateSixthWord(std::bitset<24> const & set) {

    // version
    int v = 6;

    for (int i=0; i < set.size(); i++) {
        if (i > 23) {
            break;
        }
        if (set[i]) {
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
uint32_t RecordHeader::generateSixthWord(uint32_t version, bool hasDictionary,
                                         bool isEnd, uint32_t eventType) {
    std::bitset<24> noBits;
    return generateSixthWord(noBits, version, hasDictionary, isEnd, eventType);
}


/**
  * Calculates the sixth word of this header which has the version number (4)
  * in the lowest 8 bits and the set in the upper 24 bits. The arg isDictionary
  * is set in the 9th bit and isEnd is set in the 10th bit. Four bits of an int
  * (event type) are set in bits 11-14.
  *
  * @param set reference to bitset containing all bits to be set
  * @param version evio version number
  * @param hasDictionary does this block include an evio xml dictionary as the first event?
  * @param isEnd is this the last block of a file or a buffer?
  * @param eventType 4 bit type of events header is containing
  * @return generated sixth word of this header.
  */
uint32_t RecordHeader::generateSixthWord(std::bitset<24> const & set,
                                        uint32_t version, bool hasDictionary,
                                        bool isEnd, uint32_t eventType) {
    uint32_t v = version; // version

    for (int i=0; i < set.size(); i++) {
        if (i > 23) {
            break;
        }
        if (set[i]) {
            v |= (0x1 << (8+i));
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
void RecordHeader::decodeBitInfoWord(uint32_t word) {
    // Padding
    compressedDataLengthPadding = (word >> 24) & 0x3;
    dataLengthPadding           = (word >> 22) & 0x3;
    userHeaderLengthPadding     = (word >> 20) & 0x3;

    // Evio version
    headerVersion = (word & 0xff);

    // Header type
    headerType = headerType.getHeaderType((word >> 28) & 0x0fffffff);
    if (headerType == HeaderType::UNKNOWN) {
        headerType = HeaderType::EVIO_RECORD;
    }

    // Data type
    eventType = (word >> 11) & 0xf;
}

// Boolean Getters & Setters

/**
 * Set the bit which says record has a first event in the user header.
 * @param hasFirst  true if record has a first event in the user header.
 * @return new bitInfo word.
 */
uint32_t RecordHeader::hasFirstEvent(bool hasFirst) {
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
bool RecordHeader::hasFirstEvent() const {return ((bitInfo & FIRST_EVENT_BIT) != 0);}

/**
 * Set the bit which says record has a dictionary in the user header.
 * @param hasFirst  true if record has a dictionary in the user header.
 * @return new bitInfo word.
 */
uint32_t RecordHeader::hasDictionary(bool hasFirst) {
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
bool RecordHeader::hasDictionary() const {return ((bitInfo & DICTIONARY_BIT) != 0);}

/**
 * Does this bitInfo arg indicate the existence of a dictionary in the user header?
 * @param bitInfo bitInfo word.
 * @return true if header has a dictionary in the user header, else false.
 */
bool RecordHeader::hasDictionary(int bitInfo) {return ((bitInfo & DICTIONARY_BIT) != 0);}

/**
 * Set the bit which says record is last in file/buffer.
 * @param isLast  true if record is last in file/buffer.
 * @return new bitInfo word.
 */
uint32_t RecordHeader::isLastRecord(bool isLast) {
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
bool RecordHeader::isLastRecord() const {return ((bitInfo & LAST_RECORD_BIT) != 0);}

/**
 * Does this word indicate this is the header of the last record?
 * @param bitInfo bitInfo word.
 * @return true this is the header of the last record, else false.
 */
bool RecordHeader::isLastRecord(int bitInfo) {return ((bitInfo & LAST_RECORD_BIT) != 0);}


// Setters


/**
 * Clear the bit in the given arg to indicate it is NOT the last record.
 * @param i integer in which to clear the last-record bit
 * @return arg with last-record bit cleared
 */
uint32_t RecordHeader::clearLastRecordBit(uint32_t i) {return (i & ~LAST_RECORD_MASK);}

/**
 * Set the bit info of a record header for a specified CODA event type.
 * Must be called AFTER {@link #setBitInfo(boolean, boolean, boolean)} or
 * {@link #setBitInfoWord(int)} in order to have change preserved.
 * @param type event type (0=ROC raw, 1=Physics, 2=Partial Physics,
 *             3=Disentangled, 4=User, 5=Control, 15=Other,
 *             else = nothing set).
 * @return new bit info word.
 */
uint32_t  RecordHeader::setBitInfoEventType (uint32_t type) {
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
            bitInfo |= DATA_OTHER_BITS;
            eventType = 15;
            break;
    }
    return bitInfo;
}

/**
 * Set this header's type. Normally done in constructor. Limited access.
 * @param type type of header.
 * @return this object.
 */
RecordHeader & RecordHeader::setHeaderType(HeaderType const & type)  {
    headerType = type;

    // Update bitInfo by first clearing then setting the 2 header type bits
    bitInfo = (bitInfo & (~HEADER_TYPE_MASK)) | (type.getValue() << 28);

    return *this;
}

/**
 * Set the position of this record in a file.
 * @param pos position of this record in a file.
 * @return this object.
 */
RecordHeader & RecordHeader::setPosition(size_t pos) {position = pos; return *this;}

/**
 * Set the record number.
 * @param num record number.
 * @return this object.
 */
RecordHeader & RecordHeader::setRecordNumber(uint32_t num) {recordNumber = num; return *this;}

/**
 * Set the record length in bytes & words.
 * If length is not a multiple of 4, you're on your own!
 * @param length  length of record in bytes.
 * @return this object.
 */
RecordHeader & RecordHeader::setLength(uint32_t length) {
    recordLength      = length;
    recordLengthWords = length/4;
    //printf(" LENGTH = " + recordLength + "  WORDS = " + this.recordLengthWords
    //+ "  SIZE = " + recordLengthWords*4 );
    return *this;
}

/**
 * Set the uncompressed data length in bytes & words and the padding.
 * @param length  length of uncompressed data in bytes.
 * @return this object.
 */
RecordHeader & RecordHeader::setDataLength(uint32_t length) {
    dataLength = length;
    dataLengthWords = getWords(length);
    dataLengthPadding = getPadding(length);

    // Update bitInfo by first clearing then setting the 2 padding bits
    bitInfo = (bitInfo & (~DATA_PADDING_MASK)) |
              ((dataLengthPadding << 22) & DATA_PADDING_MASK);

    return *this;
}

/**
 * Set the compressed data length in bytes & words and the padding.
 * @param length  length of compressed data in bytes.
 * @return this object.
 */
RecordHeader & RecordHeader::setCompressedDataLength(uint32_t length) {
    compressedDataLength = length;
    compressedDataLengthWords = getWords(length);
    compressedDataLengthPadding = getPadding(length);

    // Update bitInfo by first clearing then setting the 2 padding bits
    bitInfo = (bitInfo & (~COMP_PADDING_MASK)) |
              ((compressedDataLengthPadding << 24) & COMP_PADDING_MASK);

    return *this;
}

/**
 * Set the length of the index array in bytes.
 * Length is forced to be a multiple of 4!
 * @param length  length of index array in bytes.
 * @return this object.
 */
RecordHeader & RecordHeader::setIndexLength(uint32_t length) {
    indexLength = (length/4)*4;
    return *this;
}

/**
 * Set the compression type. 0=none, 1=LZ4 fast, 2=LZ4 best, 3=gzip.
 * No compression for other values.
 * @param type compression type.
 * @return this object.
 */
RecordHeader & RecordHeader::setCompressionType(Compressor::CompressionType type) {
    compressionType = type;
    return *this;
}

/**
 * Set the number of events or index entries.
 * No compression for other values.
 * @param n number of events or index entries.
 * @return this object.
 */
RecordHeader & RecordHeader::setEntries(uint32_t n) {entries = n; return *this;}

/**
 * Set the user-defined header's length in bytes & words and the padding.
 * @param length  user-defined header's length in bytes.
 * @return this object.
 */
RecordHeader & RecordHeader::setUserHeaderLength(uint32_t length) {
    userHeaderLength = length;
    userHeaderLengthWords   = getWords(length);
    userHeaderLengthPadding = getPadding(length);

    // Update bitInfo by first clearing then setting the 2 padding bits
    bitInfo = (bitInfo & (~USER_PADDING_MASK)) |
              ((userHeaderLengthPadding << 20) & USER_PADDING_MASK);

    return *this;
}

/**
 * Set the this header's length in bytes & words.
 * If length is not a multiple of 4, you're on your own!
 * @param length  this header's length in bytes.
 * @return this object.
 */
RecordHeader & RecordHeader::setHeaderLength(uint32_t length) {
    headerLength = length;
    headerLengthWords = length/4;
    return *this;
}

/**
 * Set the first, 64-bit, user-defined register.
 * @param reg  first, 64-bit, user-defined register.
 * @return this object.
 */
RecordHeader & RecordHeader::setUserRegisterFirst(uint64_t reg) {
    recordUserRegisterFirst = reg;
    return *this;
}

/**
 * Set the second, 64-bit, user-defined register.
 * @param reg  second, 64-bit, user-defined register.
 * @return this object.
 */
RecordHeader & RecordHeader::setUserRegisterSecond(uint64_t reg) {
    recordUserRegisterSecond = reg;
    return *this;
}


//-------------------------------------------------


/**
 * Writes this header into the given byte buffer.
 * Position & limit of given buffer does NOT change.
 * @param buf  byte buffer to write header into.
 * @param off  position in buffer to begin writing.
 * @throws HipoException if buffer contains too little room.
 */
void RecordHeader::writeHeader(ByteBuffer & buf, size_t off) {

    // Check args
    if ((buf.capacity() - off) < HEADER_SIZE_BYTES) {
        throw HipoException("buffer null or too small");
    }

    uint32_t compressedWord = (compressedDataLengthWords & 0x0FFFFFFF) |
                              (compressionType << 28);

    // Endian issues must be handled explicitly!

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
 * @throws HipoException if buffer contains too little room.
 */
void RecordHeader::writeHeader(ByteBuffer & buffer) {writeHeader(buffer,0);}


/**
 * Writes an empty trailer into the given byte array.
 *
 * @param array     byte array to write trailer into.
 * @param arrayLen  number of available bytes in array to write into.
 * @param off       offset into array to start writing.
 * @param recordNum record number of trailer.
 * @param order     byte order of data to be written.
 * @throws HipoException if array arg is null or too small to hold trailer
 */
void RecordHeader::writeTrailer(uint8_t* array, size_t arrayLen, size_t off,
                                uint32_t recordNum, const ByteOrder & order) {
    writeTrailer(array, arrayLen, off, recordNum, order, nullptr, 0);
}

/**
  * Writes a trailer with an optional index array into the given byte array.
  *
  * @param array        byte array to write trailer into.
  * @param arrayLen     number of available bytes in array to write into.
  * @param off          offset into array to start writing.
  * @param recordNumber record number of trailer.
  * @param order        byte order of data to be written.
  * @param index        array of record lengths interspersed with event counts
  *                     to be written to trailer (must be multiple of 4 bytes).
  *                     Null if no index array.
  * @param indexLen     number of valid bytes in index.
  * @throws HipoException if array arg is null, array too small to hold trailer + index,
  *                       or index not multiple of 4 bytes.
  */
void RecordHeader::writeTrailer(uint8_t* array, size_t arrayLen, size_t off,
                                uint32_t recordNumber, const ByteOrder & order,
                                const uint32_t* index, size_t indexLen) {

    uint32_t wholeLength = HEADER_SIZE_BYTES;
    if (index != nullptr) {
        if ((indexLen % 4) != 0) {
            throw HipoException("index length not multiple of 4 bytes");
        }
        wholeLength += indexLen;
    }
    else {
        indexLen = 0;
    }

    // Check args
    if (array == nullptr || arrayLen < wholeLength) {
        throw HipoException("null or too small array arg");
    }

    int bitInfo = (HeaderType::EVIO_TRAILER.getValue() << 28) | RecordHeader::LAST_RECORD_BIT | 6;

    try {
        // First the general header part
        toBytes(wholeLength/4,     order, array, arrayLen,      off); // 0*4
        toBytes(recordNumber,      order, array, arrayLen, 4  + off); // 1*4
        toBytes(HEADER_SIZE_WORDS, order, array, arrayLen, 8  + off); // 2*4
        toBytes(0,                 order, array, arrayLen, 12 + off); // 3*4
        toBytes(indexLen,          order, array, arrayLen, 16 + off); // 4*4
        toBytes(bitInfo,           order, array, arrayLen, 20 + off); // 5*4
        toBytes(0,                 order, array, arrayLen, 24 + off); // 6*4
        toBytes(HEADER_MAGIC,      order, array, arrayLen, 28 + off); // 7*4

        // The rest of header is all 0's, 8*4 (inclusive) -> 14*4 (exclusive)
        memset((void *)(array + 32 + off), 0, 24);

        // Second the index
        if (indexLen > 0) {
            memcpy((void *)(array+56+off), (const void *)index, indexLen);
            // Swap in place if necessary
            if (!order.isLocalEndian()) {
                byteSwap(reinterpret_cast<uint32_t *>(array+56+off), indexLen/4);
            }
        }
    }
    catch (HipoException & e) {/* never happen */}
}

/**
  * Write int into byte array.
  *
  * @param data int to convert
  * @param byteOrder byte order of returned bytes (big endian if null)
  * @param dest array in which to store transformed int
  * @param off offset into dest array where returned bytes are placed
  * @throws HipoException if dest is null or too small or offset negative
  */
void RecordHeader::toBytes(uint32_t data, const ByteOrder & byteOrder,
                           uint8_t* dest, size_t destLen, size_t off) {

    if (dest == nullptr || destLen < 4+off) {
        throw HipoException("bad arg(s)");
    }

    if (byteOrder == ByteOrder::ENDIAN_BIG) {
        dest[off  ] = (uint8_t)(data >> 24);
        dest[off+1] = (uint8_t)(data >> 16);
        dest[off+2] = (uint8_t)(data >>  8);
        dest[off+3] = (uint8_t)(data      );
    }
    else {
        dest[off  ] = (uint8_t)(data      );
        dest[off+1] = (uint8_t)(data >>  8);
        dest[off+2] = (uint8_t)(data >> 16);
        dest[off+3] = (uint8_t)(data >> 24);
    }
}


/**
 * Writes an empty trailer into the given buffer.
 * @param buf   ByteBuffer to write trailer into.
 * @param off   offset into buffer to start writing.
 * @param recordNum record number of trailer.
 * @throws HipoException if buf arg is null or too small to hold trailer
 */
void RecordHeader::writeTrailer(ByteBuffer & buf, size_t off, uint32_t recordNum) {
    writeTrailer(buf, off, recordNum, nullptr, 0);
}

/**
 * Writes a trailer with an optional index array into the given buffer.
 * @param buf   ByteBuffer to write trailer into.
 * @param off   offset into buffer to start writing.
 * @param recordNum record number of trailer.
 * @param index array of record lengths interspersed with event counts
 *              to be written to trailer
 *              (must be multiple of 4 bytes). Null if no index array.
 * @param indexLen length in bytes of index array.
 * @throws HipoException if buf too small to hold trailer + index,
 *                       or index not multiple of 4 bytes.
 */
void RecordHeader::writeTrailer(ByteBuffer & buf, size_t off, uint32_t recordNum,
                                const uint32_t* index, size_t indexLen) {

    int wholeLen = HEADER_SIZE_BYTES;
    if (index != nullptr) {
        if ((indexLen % 4) != 0) {
            throw HipoException("index length not multiple of 4 bytes");
        }
        wholeLen += indexLen;
    }
    else {
        indexLen = 0;
    }

    // Check arg
    if (buf.capacity() - off < wholeLen) {
        throw HipoException("null or too small buf arg");
    }

    // Make sure the limit allows writing
    buf.limit(off + wholeLen).position(off);

    if (buf.hasArray()) {
        writeTrailer(buf.array(), buf.remaining(), buf.arrayOffset() + off,
                     recordNum, buf.order(), index, indexLen);
    }
    else {
        uint32_t bitinfo = (HeaderType::EVIO_TRAILER.getValue() << 28) | RecordHeader::LAST_RECORD_BIT | 6;

        // First the general header part
        buf.putInt(wholeLen/4);
        buf.putInt(recordNum);
        buf.putInt(HEADER_SIZE_WORDS);
        buf.putInt(0);
        buf.putInt(0);
        buf.putInt(bitinfo);
        buf.putInt(0);
        buf.putInt(HEADER_MAGIC);
        buf.putLong(0L);
        buf.putLong(0L);
        buf.putLong(0L);

        // Second the index
        if (indexLen > 0) {
            for (int i=0; i < indexLen/4; i++) {
                buf.putInt(index[0]);
            }
        }
    }
}


//------------------------------------------------------------------------------------------


/**
 * Quickly check to see if this buffer contains compressed data or not.
 * The offset must point to the beginning of a valid hipo/evio record
 * in the buffer.
 *
 * @param buffer buffer to read from.
 * @param offset position of record header to be read.
 * @return true if data in record is compressed, else false.
 * @throws HipoException if buffer contains too little data,
 *                       or is not in proper format.
 */
bool RecordHeader::isCompressed(ByteBuffer & buffer, size_t offset) {

    if ((buffer.capacity() - offset) < 40) {
        throw HipoException("data underflow");
    }

    // First read the magic word to establish endianness
    int magicWord = buffer.getInt(28 + offset);

    // If it's NOT in the proper byte order ...
    if (magicWord != HEADER_MAGIC) {
        // If it needs to be switched ...
        if (magicWord == SWAP_32(HEADER_MAGIC)) {
            if (buffer.order() == ByteOrder::ENDIAN_BIG) {
                buffer.order(ByteOrder::ENDIAN_LITTLE);
            }
            else {
                buffer.order(ByteOrder::ENDIAN_BIG);
            }
        }
        else {
            stringstream ss;
            ss << "buffer not in evio/hipo format? magic int = 0x" << hex << magicWord;
            throw HipoException(ss.str());
        }
    }

    uint32_t compressionWord = buffer.getInt(36 + offset);
    return (((compressionWord >> 28) & 0xf) == 0);
}


/**
 * Reads the header information from a byte buffer and validates
 * it by checking the magic word (8th word). This magic word
 * also determines the byte order.
 *
 * @param buffer buffer to read from.
 * @param offset position of first word to be read.
 * @throws HipoException if buffer contains too little data,
 *                       is not in proper format, or version earlier than 6.
 */
void RecordHeader::readHeader(ByteBuffer & buffer, size_t offset) {
    if ((buffer.capacity() - offset) < HEADER_SIZE_BYTES) {
        throw HipoException("null or too small buffer arg");
    }

    // First read the magic word to establish endianness
    headerMagicWord = buffer.getInt(28 + offset);    // 7*4

    // If it's NOT in the proper byte order ...
    if (headerMagicWord != HEADER_MAGIC) {
        // If it needs to be switched ...
        if (headerMagicWord == SWAP_32(HEADER_MAGIC)) {
            if (buffer.order() == ByteOrder::ENDIAN_BIG) {
                byteOrder = ByteOrder::ENDIAN_LITTLE;
            }
            else {
                byteOrder = ByteOrder::ENDIAN_BIG;
            }
            buffer.order(byteOrder);
            headerMagicWord = HEADER_MAGIC;
        }
        else {
            // ERROR condition, bad magic word
            buffer.printBytes(0, 40, "Bad Magic Word, buffer:");
            stringstream ss;
            ss << "buffer not in evio/hipo format? magic int = 0x" << hex << headerMagicWord;
            throw HipoException(ss.str());
        }
    }
    else {
        byteOrder = buffer.order();
    }

    // Look at the bit-info word
    bitInfo = buffer.getInt(20 + offset);   // 5*4

    // Set padding and header type
    decodeBitInfoWord(bitInfo);

    // Look at the version #
    if (headerVersion < 6) {
        throw HipoException("buffer is in evio format version " + to_string(bitInfo & 0xff));
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

    uint32_t compressionWord = buffer.getInt(36 + offset);   //  9*4
    compressionType = Compressor::toCompressionType((compressionWord >> 28) & 0xf);
    compressedDataLengthWords = (compressionWord & 0x0FFFFFFF);
    compressedDataLengthPadding = (bitInfo >> 24) & 0x3;
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
void RecordHeader::readHeader(ByteBuffer & buffer) {
    readHeader(buffer,0);
}

//-----------------------------------------------------
// Additional methods for implementing IBlockHeader
//-----------------------------------------------------

/**
 * Returns a string representation of this record.
 * @return a string representation of this record.
 */
string RecordHeader::toString() const {

    stringstream ss;

    // show 0x for hex and true for boolean
    ss << showbase << boolalpha;

    ss << setw(24) << "version"    << " : " << headerVersion << endl;
    ss << setw(24) << "compressed" << " : " << (compressionType != Compressor::UNCOMPRESSED) << endl;
    ss << setw(24) << "record #"   << " : " << recordNumber << endl;
    ss << setw(24) << "" << " :     bytes,     words,    padding" << endl;

//    printf("%24s : %d\n","version",headerVersion);
//    printf("%24s : %b\n","compressed", (compressionType != UNCOMPRESSED));
//    printf("%24s : %d\n","record #",recordNumber);
//    printf("%24s :     bytes,     words,    padding\n","");

    ss << setw(24) << "user header length" << " : " << setw(8) << userHeaderLength <<
          setw(10) << userHeaderLengthWords << setw(10) << userHeaderLengthPadding << endl;
    ss << setw(24) << "uncompressed header length" << " : " << setw(8) << dataLength <<
          setw(10) << dataLengthWords << setw(10) << dataLengthPadding << endl;
    ss << setw(24) << "record length" << " : " << setw(8) << recordLength <<
          setw(10) << recordLengthWords << endl;
    ss << setw(24) << "compressed length" << " : " << setw(8) << compressedDataLength <<
       setw(10) << compressedDataLengthWords << setw(10) << compressedDataLengthPadding << endl;

//    printf("%24s : %8d  %8d  %8d\n","user header length",
//                             userHeaderLength, userHeaderLengthWords, userHeaderLengthPadding);
//    printf("%24s : %8d  %8d  %8d\n","uncompressed data length",
//                             dataLength, dataLengthWords, dataLengthPadding);
//    printf("%24s : %8d  %8d\n","record length",
//                             recordLength, recordLengthWords);
//    printf("%24s : %8d  %8d  %8d\n","compressed length",
//                             compressedDataLength, compressedDataLengthWords,
//                             compressedDataLengthPadding);

    ss << setw(24) << "header length" << " : " << headerLength << endl;
    ss << setw(24) << "index length"  << " : " << indexLength << endl;
    ss << hex;
    ss << setw(24) << "magic word"    << " : " << headerMagicWord << endl;

//    printf("%24s : %d\n","header length",headerLength);
//    printf("%24s : %d\n","index length",indexLength);
//    printf("%24s : 0x%X\n","magic word",headerMagicWord);

    ss << setw(24) << "bit info word"   << " : " << bitInfo         << endl;
    ss << setw(24) << "has dictionary"  << " : " << hasDictionary() << endl;
    ss << setw(24) << "has 1st event"   << " : " << hasFirstEvent() << endl;
    ss << setw(24) << "is last record"  << " : " << isLastRecord()  << endl;

//    printf("%24s : %s\n","bit info word bin",Integer.toBinaryString(bitInfo));
//    printf("%24s : 0x%s\n","bit info word hex",Integer.toHexString(bitInfo));
//    printf("%24s : %b\n","has dictionary",hasDictionary());
//    printf("%24s : %b\n","has 1st event",hasFirstEvent());
//    printf("%24s : %b\n","is last record",isLastRecord());

    ss << dec;
    ss << setw(24) << "data type"  << " : " << eventTypeToString() << " (" << eventType << ")" << endl;
    ss << setw(24) << "event count"   << " : " << entries << endl;
    ss << setw(24) << "compression type"   << " : " << compressionType << endl;
    ss << hex;
    ss << setw(24) << "user register #1"   << " : " << recordUserRegisterFirst << endl;
    ss << setw(24) << "user register #2"   << " : " << recordUserRegisterSecond << endl;

//    printf("%24s : %s (%d)\n","data type", eventTypeToString(), eventType);
//    printf("%24s : %d\n","event count",entries);
//    printf("%24s : %d\n","compression type",compressionType);
//
//    printf("%24s : %ull\n","user register #1",recordUserRegisterFirst);
//    printf("%24s : %ull\n","user register #2",recordUserRegisterSecond);

    return ss.str();
}

/** {@inheritDoc} */
uint32_t RecordHeader::getSize() const {return recordLengthWords;}

/** {@inheritDoc} */
uint32_t RecordHeader::getNumber() const {return recordNumber;}

/** {@inheritDoc} */
uint32_t RecordHeader::getMagicNumber() const {return headerMagicWord;}

/** {@inheritDoc} */
bool RecordHeader::isLastBlock() const {return isLastRecord();}

/** {@inheritDoc} */
uint32_t RecordHeader::getSourceId() const {return (uint32_t)recordUserRegisterFirst;}

/** {@inheritDoc} */
uint32_t RecordHeader::getEventType() const {return eventType;}

/**
 * Return  a meaningful string associated with event type.
 * @return a meaningful string associated with event type.
 */
string RecordHeader::eventTypeToString() const {
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
uint32_t RecordHeader::write(ByteBuffer & byteBuffer) {
    writeHeader(byteBuffer, byteBuffer.position());
    return HEADER_SIZE_BYTES;
}

//
///** {@inheritDoc} */
//uint32_t RecordHeader::write(ByteBuffer & byteBuffer) {
//    try {
//        writeHeader(byteBuffer, byteBuffer.position());
//    }
//    catch (HipoException e) {
//        System.out.println("RecordHeader.write(): buffer is null or contains too little room");
//        return 0;
//    }
//    return HEADER_SIZE_BYTES;
//}


// Following methods are not used in this class but must be part of IBlockHeader interface

/** {@inheritDoc} */
size_t RecordHeader::getBufferEndingPosition() const {return 0L;}

/** {@inheritDoc} */
size_t RecordHeader::getBufferStartingPosition() const {return 0L;}

/** {@inheritDoc} */
void RecordHeader::setBufferStartingPosition(size_t bufferStartingPosition) {}

/** {@inheritDoc} */
size_t RecordHeader::nextBufferStartingPosition() const {return 0;}

/** {@inheritDoc} */
size_t RecordHeader::firstEventStartingPosition() const {return 0;}

/** {@inheritDoc} */
size_t RecordHeader::bytesRemaining(size_t pos) const {return 0;}

//----------------------------------------------------------------------------
// Utility methods
//----------------------------------------------------------------------------


///**
// * Run this class as an executable which tests the writing and reading of a record.
// * @param args
// */
//void RecordHeader::main(string args[]){
//
//    RecordHeader header;
//
//    header.setCompressedDataLength(861);
//    header.setDataLength(12457);
//    header.setUserHeaderLength(459);
//    header.setIndexLength(324);
//    header.setLength(16 + header.getCompressedDataLengthWords());
//    header.setUserRegisterFirst(1234567L);
//    header.setUserRegisterSecond(4567890L);
//    header.setRecordNumber(23);
//    header.setEntries(3245);
//    header.setHeaderLength(14);
//    header.setCompressionType(1);
//
//    cout << header.toString();
//
//    ByteBuffer buffer(4*14);
//    buffer.order(ByteOrder::ENDIAN_LITTLE);
//
//
//    try {
//        header.writeHeader(buffer);
//    }
//    catch (HipoException e) {/* never happen */}
//
//
//    RecordHeader header2;
//    try {
//        header2.readHeader(buffer);
//        cout << header2.toString();
//    }
//    catch (HipoException e) {
//        cout << "error" << endl;
//    }
//}

