//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "RecordHeader.h"


using namespace std;


namespace evio {


    /** Set static array to help find number of bytes to pad data. */
    uint32_t RecordHeader::padValue[4] = {0,3,2,1};


    /** Default, no-arg constructor. */
    RecordHeader::RecordHeader() {bitInfoInit();}


    /**
     * Constructor which sets the type of header this is.
     * @param type  type of header this is
     * @throws EvioException if type is for file
     */
    RecordHeader::RecordHeader(const HeaderType & type) {
        headerType = type;

        if (headerType.isFileHeader()) {
            throw EvioException("RecordHeader cannot be set to FileHeader type");
        }
        bitInfoInit();
    }


    /**
     * Constructor.
     * @param pos   position in file.
     * @param len   length of record in bytes
     * @param ent   number of events
     */
    RecordHeader::RecordHeader(size_t pos, uint32_t len, uint32_t ent) {
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

        if (this != &head) {
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
    }


    RecordHeader & RecordHeader::operator=(const RecordHeader& head) {

        if (this != &head) {
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

        return *this;
    }


    /**
     * Copy method.
     * @param header  header to copy.
     */
    void RecordHeader::copy(std::shared_ptr<RecordHeader> const & head) {

        if (this != head.get()) {
            position                 = head->position;
            recordLength             = head->recordLength;
            recordNumber             = head->recordNumber;
            recordLengthWords        = head->recordLengthWords;
            recordUserRegisterFirst  = head->recordUserRegisterFirst;
            recordUserRegisterSecond = head->recordUserRegisterSecond;

            entries                   = head->entries;
            bitInfo                   = head->bitInfo;
            eventType                 = head->eventType;
            headerLength              = head->headerLength;
            headerLengthWords         = head->headerLengthWords;
            userHeaderLength          = head->userHeaderLength;
            userHeaderLengthWords     = head->userHeaderLengthWords;
            indexLength               = head->indexLength;
            dataLength                = head->dataLength;
            dataLengthWords           = head->dataLengthWords;
            compressedDataLength      = head->compressedDataLength;
            compressedDataLengthWords = head->compressedDataLengthWords;
            headerMagicWord           = head->headerMagicWord;
            // don't bother with version as must be same

            userHeaderLengthPadding     = head->userHeaderLengthPadding;
            dataLengthPadding           = head->dataLengthPadding;
            compressedDataLengthPadding = head->compressedDataLengthPadding;

            byteOrder                   = head->byteOrder;
            headerType                  = head->headerType;
            compressionType             = head->compressionType;
        }
    }


    /** Reset generated data. */
    void RecordHeader::reset() {
        // Do NOT reset header type which is only set in constructor!
        // Do NOT reset the compression type

        // What about byteOrder?
        // When reading, it's automatically set. When writing,
        // it's determined by the ByteBuffer we're writing into.

        position = 0ULL;
        recordLength = 0;
        recordNumber = 1;
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

        userHeaderLengthPadding = 0;
        dataLengthPadding = 0;
        compressedDataLengthPadding = 0;
    }


    /**
     * Returns length padded to 4-byte boundary for given length in bytes.
     * @param length length in bytes.
     * @return length in bytes padded to 4-byte boundary.
     */
    uint32_t RecordHeader::getWords(uint32_t length) {
        uint32_t words = length/4;
        if (getPadding(length) > 0) words++;
        return words;
    }


    /**
     * Returns number of bytes needed to pad to 4-byte boundary for the given length.
     * @param length length in bytes.
     * @return number of bytes needed to pad to 4-byte boundary.
     */
    uint32_t RecordHeader::getPadding(uint32_t length) {return padValue[length%4];}


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
     * If the data is not compressed, then this returns 0;
     * @return padded length in bytes of the entire compressed record, else 0 if not compressed.
     */
    uint32_t RecordHeader::getCompressedRecordLength() const {
        if (compressionType != Compressor::UNCOMPRESSED) {
            return (recordLength + compressedDataLengthPadding);
        }
        return 0;
    }


    /**
     * Get the byte order of the file/buffer this header was read from.
     * Defaults to little endian.
     * @return byte order of the file/buffer this header was read from.
     */
    ByteOrder & RecordHeader::getByteOrder() {return byteOrder;}


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
    uint32_t  RecordHeader::getVersion() {return headerVersion;}


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
    uint32_t  RecordHeader::getHeaderWords() {return headerLengthWords;}


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
     * Four bits of this header type are set in bits 28-31
     * (defaults to 0 which is an evio record header).
     *
     * @param set reference to bitset containing all bits to be set
     * @param version evio version number
     * @param hasDictionary does this block include an evio xml dictionary as the first event?
     * @param isEnd is this the last block of a file or a buffer?
     * @param eventType 4 bit type of events header is containing
     * @param headerType 4 bit type of this header (defaults to 0 which is an evio record header).
     * @return generated sixth word of this header.
     */
    uint32_t RecordHeader::generateSixthWord(std::bitset<24> const & set,
                                             uint32_t version, bool hasDictionary, bool isEnd,
                                             uint32_t eventType, uint32_t headerType) {
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
        v |= ((headerType & 0xf) << 28);

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
        headerType = headerType.getHeaderType((word >> 28) & 0xf);
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
    bool RecordHeader::hasFirstEvent() {return ((bitInfo & FIRST_EVENT_BIT) != 0);}


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
    bool RecordHeader::hasDictionary() {return ((bitInfo & DICTIONARY_BIT) != 0);}


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
     * @return true if this is the header of the last record, else false.
     */
    bool RecordHeader::isLastRecord() const {return ((bitInfo & LAST_RECORD_BIT) != 0);}


    /**
     * Does this word indicate this is the header of the last record?
     * @param bitInfo bitInfo word.
     * @return true if this is the header of the last record, else false.
     */
    bool RecordHeader::isLastRecord(uint32_t bitInfo) {return ((bitInfo & LAST_RECORD_BIT) != 0);}


    /**
     * Clear the bit in the given arg to indicate it is NOT the last record.
     * @param i integer in which to clear the last-record bit
     * @return arg with last-record bit cleared
     */
    uint32_t RecordHeader::clearLastRecordBit(uint32_t i) {return (i & ~LAST_RECORD_MASK);}


    /**
     * Does this header indicate compressed data?
     * @return true if header indicates compressed data, else false.
     */
    bool RecordHeader::isCompressed() const {return compressionType != Compressor::UNCOMPRESSED;}


    /**
     * Is this header an evio trailer?
     * @return true if this is an evio trailer, else false.
     */
    bool RecordHeader::isEvioTrailer() const {return headerType == HeaderType::EVIO_TRAILER;}


    /**
     * Is this header a hipo trailer?
     * @return true if this is a hipo trailer, else false.
     */
    bool RecordHeader::isHipoTrailer() const {return headerType == HeaderType::HIPO_TRAILER;}


    /**
     * Is this header an evio record?
     * @return true if this is an evio record, else false.
     */
    bool RecordHeader::isEvioRecord() const {return headerType == HeaderType::EVIO_RECORD;}


    /**
     * Is this header a hipo record?
     * @return true if this is a hipo record, else false.
     */
    bool RecordHeader::isHipoRecord() const {return headerType == HeaderType::HIPO_RECORD;}


    /**
     * Does this arg indicate its header is an evio trailer?
     * @param bitInfo bitInfo word.
     * @return true if arg represents an evio trailer, else false.
     */
    bool RecordHeader::isEvioTrailer(uint32_t bitInfo) {
        return ((bitInfo >> 28) & 0xf) == HeaderType::EVIO_TRAILER.getValue();
    }


    /**
     * Does this arg indicate its header is a hipo trailer?
     * @param bitInfo bitInfo word.
     * @return true if arg represents a hipo trailer, else false.
     */
    bool RecordHeader::isHipoTrailer(uint32_t bitInfo) {
        return ((bitInfo >> 28) & 0xf) == HeaderType::HIPO_TRAILER.getValue();
    }


    /**
     * Does this arg indicate its header is an evio record?
     * @param bitInfo bitInfo word.
     * @return true if arg represents an evio record, else false.
     */
    bool RecordHeader::isEvioRecord(uint32_t bitInfo) {
        return ((bitInfo >> 28) & 0xf) == HeaderType::EVIO_RECORD.getValue();
    }


    /**
     * Does this arg indicate its header is a hipo record?
     * @param bitInfo bitInfo word.
     * @return true if arg represents a hipo record, else false.
     */
    bool RecordHeader::isHipoRecord(uint32_t bitInfo) {
        return ((bitInfo >> 28) & 0xf) == HeaderType::HIPO_RECORD.getValue();
    }


    // Setters


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


    // Setters


    /**
     * Set this header's type. Normally done in constructor. Limited access.
     * @param type type of header.
     * @return this object.
     */
    RecordHeader & RecordHeader::setHeaderType(HeaderType const & type)  {

        //    if (isCompressed() && type.isTrailer()) {
        //cout << "Doesn't make sense to set trailer type of data is compressed" << endl;
        //    }

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
        //    cout << " LENGTH = " << recordLength << "  WORDS = " << recordLengthWords <<
        //    "  SIZE = " << (recordLengthWords*4) << endl;
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

        //    if (type != Compressor::UNCOMPRESSED && headerType.isTrailer()) {
        //cout << "Doesn't make sense to set compressed data if trailer" << endl;
        //    }

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
     * @throws EvioException if buffer contains too little room.
     */
    void RecordHeader::writeHeader(ByteBuffer & buf, size_t off) {

        // Check args
        if ((buf.limit() - off) < HEADER_SIZE_BYTES) {
            throw EvioException("buffer too small");
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
     * Writes this header into the given byte buffer.
     * Position & limit of given buffer does NOT change.
     * @param buf  byte buffer to write header into.
     * @param off  position in buffer to begin writing.
     * @throws EvioException if buffer contains too little room.
     */
    void RecordHeader::writeHeader(std::shared_ptr<ByteBuffer> & buffer, size_t off) {
        writeHeader(*(buffer.get()), off);
    }


    /**
      * Writes this header at the given pointer.
      *
      * @param array         byte array to write header into.
      * @param arrayLen      number of available bytes in array to write into.
      * @param order         byte order of data to be written.
      * @param indexLen      number of valid bytes in index.
      * @throws EvioException if array arg is null.
      */
    void RecordHeader::writeHeader(uint8_t* array, const ByteOrder & order) {

        // Check args
        if (array == nullptr) {
            throw EvioException("null or too small array arg");
        }

        uint32_t compressedWord = (compressedDataLengthWords & 0x0FFFFFFF) |
                                  (compressionType << 28);

        // Endian issues must be handled explicitly!

        Util::toBytes(recordLengthWords,    order, array);            // 0*4
        Util::toBytes(recordNumber,         order, array +  4); // 1*4
        Util::toBytes(headerLengthWords,    order, array +  8); // 2*4
        Util::toBytes(entries,              order, array + 12); // 3*4
        Util::toBytes(indexLength,          order, array + 16); // 4*4
        Util::toBytes(getBitInfoWord(),     order, array + 20); // 5*4
        Util::toBytes(userHeaderLength,     order, array + 24); // 6*4
        Util::toBytes(headerMagicWord,      order, array + 28); // 7*4

        Util::toBytes(dataLength,           order, array +  4); // 8*4
        Util::toBytes(compressedWord,       order, array +  4); // 9*4
        Util::toBytes(recordUserRegisterFirst,  order, array +  4); // 10*4
        Util::toBytes(recordUserRegisterSecond, order, array +  4); // 12*4
    }


    /**
      * Writes a trailer with an optional index array into the given byte array.
      *
      * @param array         byte array to write trailer into.
      * @param arrayLen      number of available bytes in array to write into.
      * @param recordNum     record number of trailer.
      * @param order         byte order of data to be written.
      * @param recordLengths vector containing record lengths interspersed with event counts
      *                      to be written to trailer. Null if no index array.
      * @param indexLen      number of valid bytes in index.
      * @throws EvioException if array arg is null, array too small to hold trailer + index.
      */
    void RecordHeader::writeTrailer(uint8_t* array, size_t arrayLen,
                                    uint32_t recordNum, const ByteOrder & order,
                                    const std::shared_ptr<std::vector<uint32_t>> & recordLengths) {

        uint32_t indexLen = 0;
        uint32_t wholeLen = HEADER_SIZE_BYTES;
        if (recordLengths != nullptr) {
            indexLen = 4*recordLengths->size();
            wholeLen += indexLen;
        }

        // Check args
        if (array == nullptr || arrayLen < wholeLen) {
            throw EvioException("null or too small array arg");
        }

        uint32_t bitInfo = (HeaderType::EVIO_TRAILER.getValue() << 28) | RecordHeader::LAST_RECORD_BIT | 6;

        try {
            // First the general header part
            Util::toBytes(wholeLen/4,        order, array); // 0*4
            Util::toBytes(recordNum,         order, array +  4); // 1*4
            Util::toBytes(HEADER_SIZE_WORDS, order, array +  8); // 2*4
            Util::toBytes((uint32_t)0,       order, array + 12); // 3*4
            Util::toBytes(indexLen,          order, array + 16); // 4*4
            Util::toBytes(bitInfo,           order, array + 20); // 5*4
            Util::toBytes((uint32_t)0,       order, array + 24); // 6*4
            Util::toBytes(HEADER_MAGIC,      order, array + 28); // 7*4

            // The rest of header is all 0's, 8*4 (inclusive) -> 14*4 (exclusive)
            memset((void *)(array + 32), 0, 24);

            // Second the index
            if (indexLen > 0) {
                // Get vector of ints out of shared pointer
                std::vector<uint32_t> vec = *(recordLengths.get());
                for (int i=0; i < recordLengths->size(); i++) {
                    Util::toBytes(vec[i], order, array + HEADER_SIZE_BYTES + 4*i);
                }
            }
        }
        catch (EvioException & e) {/* never happen */}
    }


    /**
     * Writes a trailer with an optional index array into the given vector.
     *
     * @param array         vector to write trailer into.
     * @param off           offset into vector's array to start writing.
     * @param recordNum     record number of trailer.
     * @param order         byte order of data to be written.
     * @param recordLengths vector containing record lengths interspersed with event counts
     *                      to be written to trailer. Null if no index array.
     */
    void RecordHeader::writeTrailer(std::vector<uint8_t> & array, size_t off,
                                    uint32_t recordNum, const ByteOrder & order,
                                    const std::shared_ptr<std::vector<uint32_t>> & recordLengths) {

        uint32_t indexLen = 0;
        uint32_t wholeLen = HEADER_SIZE_BYTES;
        if (recordLengths != nullptr) {
            indexLen = 4*recordLengths->size();
            wholeLen += indexLen;
        }

        array.resize(wholeLen + off, 0);

        uint32_t bitInfo = (HeaderType::EVIO_TRAILER.getValue() << 28) | RecordHeader::LAST_RECORD_BIT | 6;

        try {
            // First the general header part
            Util::toBytes(wholeLen/4,        order, array,      off); // 0*4
            Util::toBytes(recordNum,         order, array, 4  + off); // 1*4
            Util::toBytes(HEADER_SIZE_WORDS, order, array, 8  + off); // 2*4
            Util::toBytes((uint32_t)0,       order, array, 12 + off); // 3*4
            Util::toBytes(indexLen,          order, array, 16 + off); // 4*4
            Util::toBytes(bitInfo,           order, array, 20 + off); // 5*4
            Util::toBytes((uint32_t)0,       order, array, 24 + off); // 6*4
            Util::toBytes(HEADER_MAGIC,      order, array, 28 + off); // 7*4

            // The rest of header is all 0's, 8*4 (inclusive) -> 14*4 (exclusive)
            memset((void *)(array.data() + 32 + off), 0, 24);

            // Second the index
            if (indexLen > 0) {
                // Get vector of ints out of shared pointer
                std::vector<uint32_t> vec = *(recordLengths.get());
                for (int i=0; i < recordLengths->size(); i++) {
                    Util::toBytes(vec[i], order, array, HEADER_SIZE_BYTES + off + 4*i);
                }
            }
        }
        catch (EvioException & e) {/* never happen */}
    }


    /**
     * Writes a trailer with an optional index array into the given buffer.
     * @param buf   ByteBuffer to write trailer into.
     * @param off   offset into buffer to start writing.
     * @param recordNum record number of trailer.
     * @param recordLengths vector containing record lengths interspersed with event counts
     *                      to be written to trailer. Null if no index array.
     * @throws EvioException if buf too small to hold trailer + index.
     */
    void RecordHeader::writeTrailer(ByteBuffer & buf, size_t off, uint32_t recordNum,
                                    const std::shared_ptr<std::vector<uint32_t>> & recordLengths) {

        uint32_t indexLen = 0;
        uint32_t wholeLen = HEADER_SIZE_BYTES;
        if (recordLengths != nullptr) {
            indexLen = 4*recordLengths->size();
            wholeLen += indexLen;
        }

        // Check arg
        if (buf.capacity() < wholeLen + off) {
            throw EvioException("buf too small");
        }

        // Make sure the limit allows writing
        buf.limit(off + wholeLen).position(off);

        if (buf.hasArray()) {
            writeTrailer(buf.array() + buf.arrayOffset() + off, buf.remaining(),
                         recordNum, buf.order(), recordLengths);
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
                // Get vector of ints out of shared pointer
                std::vector<uint32_t> vec = *(recordLengths.get());
                for (int i=0; i < recordLengths->size(); i++) {
                    buf.putInt(vec[i]);
                }
            }
        }
    }


    /**
    * Writes a trailer with an optional index array into the given buffer.
    * @param buf   ByteBuffer to write trailer into.
    * @param off   offset into buffer to start writing.
    * @param recordNum record number of trailer.
    * @param recordLengths vector containing record lengths interspersed with event counts
    *                      to be written to trailer. Null if no index array.
    * @throws EvioException if buf too small to hold trailer + index.
    */
    void RecordHeader::writeTrailer(std::shared_ptr<ByteBuffer> & buf, size_t off, uint32_t recordNum,
                                    const std::shared_ptr<std::vector<uint32_t>> & recordLengths) {
        writeTrailer(*(buf.get()), off, recordNum, recordLengths);
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
     * @throws EvioException if buffer contains too little data,
     *                       or is not in proper format.
     */
    bool RecordHeader::isCompressed(ByteBuffer & buffer, size_t offset) {

        if ((buffer.capacity() - offset) < 40) {
            throw EvioException("data underflow");
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
                throw EvioException(ss.str());
            }
        }

        uint32_t compressionWord = buffer.getInt(36 + offset);
        return (((compressionWord >> 28) & 0xf) == 0);
    }

    /**
     * Quickly check to see if this buffer contains compressed data or not.
     * The offset must point to the beginning of a valid hipo/evio record
     * in the buffer.
     *
     * @param buffer buffer to read from.
     * @param offset position of record header to be read.
     * @return true if data in record is compressed, else false.
     * @throws EvioException if buffer contains too little data,
     *                       or is not in proper format.
     */
    bool RecordHeader::isCompressed(std::shared_ptr<ByteBuffer> & buffer, size_t offset) {
        return isCompressed(*(buffer.get()), offset);
    }


    /**
     * Reads the header information from a byte buffer and validates
     * it by checking the magic word (8th word). This magic word
     * also determines the byte order.
     *
     * @param buffer buffer to read from.
     * @param offset position of first word to be read.
     * @throws EvioException if buffer contains too little data,
     *                       is not in proper format, or version earlier than 6.
     */
    void RecordHeader::readHeader(ByteBuffer & buffer, size_t offset) {
        if ((buffer.capacity() - offset) < HEADER_SIZE_BYTES) {
            throw EvioException("null or too small buffer arg");
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
                throw EvioException(ss.str());
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
            throw EvioException("buffer is in evio format version " + to_string(bitInfo & 0xff));
        }

        recordLengthWords   = buffer.getInt(     offset);        //  0*4
        recordLength        = 4*recordLengthWords;
        recordNumber        = buffer.getInt( 4 + offset);        //  1*4
        headerLengthWords   = buffer.getInt( 8 + offset);        //  2*4
        setHeaderLength(4*headerLengthWords);
        entries             = buffer.getInt(12 + offset);        //  3*4

        indexLength         = buffer.getInt(16 + offset);        //  4*4
        //cout << "readHeader (Record): indexLen = " << indexLength << endl;
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
     * @param buffer buffer to read from.
     * @param offset position of first word to be read.
     * @throws EvioException if buffer contains too little data,
     *                       is not in proper format, or version earlier than 6.
     */
    void RecordHeader::readHeader(std::shared_ptr<ByteBuffer> & buffer, size_t offset) {
        readHeader(*(buffer.get()), offset);
    }


    /**
     * Reads the header information from a byte buffer and validates
     * it by checking the magic word (8th word). This magic word
     * also determines the byte order.
     *
     * @param buffer buffer to read from.
     * @param order  byte order when reading.
     * @throws EvioException if src arg is null,
     *                       is not in proper format, or version earlier than 6.
     */
    void RecordHeader::readHeader(uint8_t* src, ByteOrder order) {
        if (src == nullptr) {
            throw EvioException("arg is null");
        }

        // First read the magic word to establish endianness
        headerMagicWord = Util::toInt(src + MAGIC_OFFSET, order);

        // If it's NOT in the proper byte order ...
        if (headerMagicWord != HEADER_MAGIC) {
            // If it needs to be switched ...
            if (headerMagicWord == SWAP_32(HEADER_MAGIC)) {
                if (order == ByteOrder::ENDIAN_BIG) {
                    byteOrder = ByteOrder::ENDIAN_LITTLE;
                }
                else {
                    byteOrder = ByteOrder::ENDIAN_BIG;
                }
                order = byteOrder;
                headerMagicWord = HEADER_MAGIC;
            }
            else {
                // ERROR condition, bad magic word
                stringstream ss;
                ss << "buffer not in evio/hipo format? magic int = 0x" << hex << headerMagicWord;
                throw EvioException(ss.str());
            }
        }
        else {
            byteOrder = order;
        }

        // Look at the bit-info word
        bitInfo = Util::toInt(src + BIT_INFO_OFFSET, order);

        // Set padding and header type
        decodeBitInfoWord(bitInfo);

        // Look at the version #
        if (headerVersion < 6) {
            throw EvioException("buffer is in evio format version " + to_string(bitInfo & 0xff));
        }

        recordLengthWords   = Util::toInt(src + RECORD_LENGTH_OFFSET, order);     //  0*4
        recordLength        = 4*recordLengthWords;
        recordNumber        = Util::toInt(src + RECORD_NUMBER_OFFSET, order);     //  1*4
        headerLengthWords   = Util::toInt(src + HEADER_LENGTH_OFFSET, order);     //  2*4
        setHeaderLength(4*headerLengthWords);
        entries             = Util::toInt(src + EVENT_COUNT_OFFSET, order);      //  3*4
        indexLength         = Util::toInt(src + INDEX_ARRAY_OFFSET, order);      //  4*4
        //cout << "readHeader (Record): indexLen = " << indexLength << endl;
        setIndexLength(indexLength);
        userHeaderLength    = Util::toInt(src + USER_LENGTH_OFFSET, order);      //  6*4
        setUserHeaderLength(userHeaderLength);

        // uncompressed data length
        dataLength          = Util::toInt(src + UNCOMPRESSED_LENGTH_OFFSET, order);    //  8*4
        setDataLength(dataLength);

        uint32_t compressionWord = Util::toInt(src + COMPRESSION_TYPE_OFFSET, order);  //  9*4
        compressionType = Compressor::toCompressionType((compressionWord >> 28) & 0xf);
        compressedDataLengthWords = (compressionWord & 0x0FFFFFFF);
        compressedDataLengthPadding = (bitInfo >> 24) & 0x3;
        compressedDataLength = compressedDataLengthWords*4 - compressedDataLengthPadding;

        recordUserRegisterFirst  = Util::toLong(src + REGISTER1_OFFSET, order);  // 10*4
        recordUserRegisterSecond = Util::toLong(src + REGISTER2_OFFSET, order);  // 12*4
    }


    //-----------------------------------------------------
    // Additional methods
    //-----------------------------------------------------


    /**
     * Returns a string representation of this record.
     * @return a string representation of this record.
     */
    string RecordHeader::toString() {

        stringstream ss;

        // show 0x for hex and true for boolean
        ss << showbase << boolalpha;

        ss << setw(24) << "version"    << "   : " << headerVersion << endl;
        ss << setw(24) << "compressed" << "   : " << (compressionType != Compressor::UNCOMPRESSED) << endl;
        ss << setw(24) << "record #"   << "   : " << recordNumber << endl;
        ss << setw(24) << "" << "   :     bytes,     words,    padding" << endl;

        ss << setw(24) << "user header length" << "   : " << setw(8) << userHeaderLength <<
           setw(10) << userHeaderLengthWords << setw(10) << userHeaderLengthPadding << endl;
        ss << setw(24) << "uncompressed header length" << " : " << setw(8) << dataLength <<
           setw(10) << dataLengthWords << setw(10) << dataLengthPadding << endl;
        ss << setw(24) << "record length" << "   : " << setw(8) << recordLength <<
           setw(10) << recordLengthWords << endl;
        ss << setw(24) << "compressed length" << "   : " << setw(8) << compressedDataLength <<
           setw(10) << compressedDataLengthWords << setw(10) << compressedDataLengthPadding << endl;

        ss << setw(24) << "header length" << "   : " << headerLength << endl;
        ss << setw(24) << "index length"  << "   : " << indexLength << endl;
        ss << hex;
        ss << setw(24) << "magic word"    << "   : " << headerMagicWord << endl;

        ss << setw(24) << "bit info word"   << "   : " << bitInfo         << endl;
        ss << setw(24) << "has dictionary"  << "   : " << hasDictionary() << endl;
        ss << setw(24) << "has 1st event"   << "   : " << hasFirstEvent() << endl;
        ss << setw(24) << "is last record"  << "   : " << isLastRecord()  << endl;

        ss << dec;
        ss << setw(24) << "data type"  << "   : " << eventTypeToString() << " (" << eventType << ")" << endl;
        ss << setw(24) << "event count"   << "   : " << entries << endl;
        ss << setw(24) << "compression type"   << "   : " << compressionType << endl;
        ss << hex;
        ss << setw(24) << "user register #1"   << "   : " << recordUserRegisterFirst << endl;
        ss << setw(24) << "user register #2"   << "   : " << recordUserRegisterSecond << endl;

        return ss.str();
    }


    /**
     * Get the size of the record)in 32 bit words.
     * @return size of the record)in 32 bit words.
     */
    uint32_t RecordHeader::getSize() {return recordLengthWords;}


    /**
     * Get the block number for this record)
     * In a file, this is usually sequential.
     * @return the number for this record)
     */
    uint32_t RecordHeader::getNumber() {return recordNumber;}


    /**
     * Get the magic number the record)header which should be 0xc0da0100.
     * @return magic number in the record)
     */
    uint32_t RecordHeader::getMagicNumber() {return headerMagicWord;}


    /**
     * Is this the last record in the file or being sent over the network?
     * @return <code>true</code> if this is the last record in the file or being sent
     *         over the network, else <code>false</code>.
     */
    bool RecordHeader::isLastBlock() {return isLastRecord();}


    /**
     * Get the source ID number if in CODA online context and data is coming from ROC.
     * @return source ID number if in CODA online context and data is coming from ROC.
     */
    uint32_t RecordHeader::getSourceId() {return (uint32_t)recordUserRegisterFirst;}


    /**
     * Get the type of events in record (see values of {@link DataType}.
     * @return type of events in record.
     */
    uint32_t RecordHeader::getEventType() {return eventType;}


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


    /**
     * Write myself out into a byte buffer. This write is relative--i.e.,
     * it uses the current position of the buffer.
     * @param byteBuffer the byteBuffer to write to.
     * @return the number of bytes written.
     */
    size_t RecordHeader::write(ByteBuffer & byteBuffer) {
        writeHeader(byteBuffer, byteBuffer.position());
        return HEADER_SIZE_BYTES;
    }


    /**
     * Get the position in the buffer (bytes) of this record's last data word.<br>
     * @return position in the buffer (bytes) of this record's last data word.
     */
    size_t RecordHeader::getBufferEndingPosition() {return 0ULL;}


    /**
     * Get the starting position in the buffer (bytes) from which this header was read--if that happened.<br>
     * This is not part of the record header proper. It is a position in a memory buffer of the start of the record
     * It is kept for convenience. It is up to the reader to set it.
     *
     * @return starting position in buffer (bytes) from which this header was read--if that happened.
     */
    size_t RecordHeader::getBufferStartingPosition() {return 0ULL;}


    /**
     * Set the starting position in the buffer (bytes) from which this header was read--if that happened.<br>
     * This is not part of the record header proper. It is a position in a memory buffer of the start of the record
     * It is kept for convenience. It is up to the reader to set it.
     *
     * @param bufferStartingPosition starting position in buffer from which this header was read--if that
     *            happened.
     */
    void RecordHeader::setBufferStartingPosition(size_t bufferStartingPosition) {}


    /**
     * Determines where the start of the next record header in some buffer is located (bytes).
     * This assumes the start position has been maintained by the object performing the buffer read.
     *
     * @return the start of the next record header in some buffer is located (bytes).
     */
    size_t RecordHeader::nextBufferStartingPosition() {return 0ULL;}


    /**
     * Determines where the start of the first event in this record is located
     * (bytes). This assumes the start position has been maintained by the object performing the buffer read.
     *
     * @return where the start of the first event in this record is located
     *         (bytes). In evio format version 2, returns 0 if start is 0, signaling
     *         that this entire record is part of a logical record that spans at least
     *         three physical records.
     */
    size_t RecordHeader::firstEventStartingPosition() {return 0ULL;}


    /**
     * Gives the bytes remaining in this record given a buffer position. The position is an absolute
     * position in a byte buffer. This assumes that the absolute position in <code>bufferStartingPosition</code> is
     * being maintained properly by the reader.
     *
     * @param position the absolute current position in a byte buffer.
     * @return the number of bytes remaining in this record.
     * @throws EvioException if position out of bounds
     */
    size_t RecordHeader::bytesRemaining(size_t pos) {return 0U;}


    //----------------------------------------------------------------------------
    // Utility methods
    //----------------------------------------------------------------------------


    /**
     * Run this class as an executable which tests the writing and reading of a record.
     * @param args
     */
    int RecordHeader::main(int argc, char **argv) {

        RecordHeader header;

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
        header.setCompressionType(Compressor::LZ4);

        cout << header.toString();

        ByteBuffer buffer(14*4);
        buffer.order(ByteOrder::ENDIAN_LITTLE);


        try {
            header.writeHeader(buffer);
        }
        catch (EvioException & e) {/* never happen */}


        RecordHeader header2;
        try {
            header2.readHeader(buffer);
            cout << header2.toString();
        }
        catch (EvioException & e) {
            cout << "error" << endl;
        }

        return 0;
    }

}

