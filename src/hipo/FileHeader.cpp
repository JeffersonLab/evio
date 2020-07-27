//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "FileHeader.h"


namespace evio {


    /** Set static array to help find number of bytes to pad data. */
    uint32_t FileHeader::padValue[4] = {0,3,2,1};


    /** Default, no-arg constructor. */
    FileHeader::FileHeader() {bitInfoInit();}


    /**
     * Constructor which sets the type of header this is and file id.
     * @param isEvio if true, file has EVIO file id and header type, else is HIPO.
     */
    FileHeader::FileHeader(bool isEvio) {
        if (isEvio) {
            fileId = EVIO_FILE_UNIQUE_WORD;
            headerType = HeaderType::EVIO_FILE;
        }
        else {
            fileId = HIPO_FILE_UNIQUE_WORD;
            headerType = HeaderType::HIPO_FILE;
        }
        bitInfoInit();
    }


    /**
     * Copy constructor.
     * @param header file header to copy.
     */
    FileHeader::FileHeader(const FileHeader & header) {
        copy(header);
    }


    /**
     * Copy the contents of the arg into this object.
     * @param head object to copy
     */
    void FileHeader::copy(const FileHeader & head) {

        if (this != &head) {
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
            headerVersion           = head.headerVersion;
        }
    }


    /** Reset most internal variables (not file id & header type). */
    void FileHeader::reset() {
        // Do NOT reset fileId which is only set in constructor.
        // Do NOT reset header type.

        fileNumber = 1;
        userRegister = 0;
        trailerPosition = 0;
        userIntFirst  = 0;
        userIntSecond = 0;
        position = 0;
        entries = 0;
        bitInfoInit();
        totalLength  = HEADER_SIZE_BYTES;
        headerLength = HEADER_SIZE_BYTES;
        headerLengthWords = HEADER_SIZE_WORDS;
        userHeaderLength = 0;
        userHeaderLengthWords = 0;
        indexLength = 0;
        headerMagicWord = HEADER_MAGIC;
        userHeaderLengthPadding = 0;
        headerVersion = 6;
    }


    /**
     * Returns length padded to 4-byte boundary for given length in bytes.
     * @param length length in bytes.
     * @return length in bytes padded to 4-byte boundary.
     */
    uint32_t FileHeader::getWords(uint32_t length) {
        uint32_t words = length/4;
        if (getPadding(length) > 0) words++;
        return words;
    }


    /**
     * Returns number of bytes needed to pad to 4-byte boundary for the given length.
     * @param length length in bytes.
     * @return number of bytes needed to pad to 4-byte boundary.
     */
    uint32_t FileHeader::getPadding(uint32_t length) {return padValue[length%4];}


    //----------
    // Getters
    //----------


    /**
     * Get the byte order of the file this header was read from.
     * Defaults to little endian.
     * @return byte order of the file this header was read from.
     */
    const ByteOrder & FileHeader::getByteOrder() const {return byteOrder;}


    /**
     * Get the type of header this is.
     * @return type of header this is.
     */
    const HeaderType & FileHeader::getHeaderType() const {return headerType;}


    /**
     * Get the file number or split number.
     * @return file number.
     */
    uint32_t  FileHeader::getFileNumber() const {return fileNumber;}


    /**
     * Get the file id.
     * @return file id.
     */
    uint32_t  FileHeader::getFileId() const {return fileId;}


    /**
     * Get the user register value.
     * @return user register value.
     */
    uint64_t  FileHeader::getUserRegister() const {return userRegister;}


    /**
     * Get the trailer's (trailing header's) file position in bytes.
     * @return trailer's (trailing header's) file position in bytes.
     */
    size_t  FileHeader::getTrailerPosition() const {return trailerPosition;}


    /**
     * Get the first user integer value.
     * @return first user integer value.
     */
    uint32_t  FileHeader::getUserIntFirst() const {return userIntFirst;}


    /**
     * Get the second user integer value.
     * @return second user integer value.
     */
    uint32_t  FileHeader::getUserIntSecond() const {return userIntSecond;}


    /**
     * Get the position of this record in a file.
     * @return position of this record in a file.
     */
    size_t  FileHeader::getPosition() const {return position;}


    /**
     * Get the number of events or entries in index.
     * @return number of events or entries in index.
     */
    uint32_t  FileHeader::getEntries() const {return entries;}


    /**
     * Get the length of the user-defined header in bytes.
     * @return length of the user-defined header in bytes.
     */
    uint32_t  FileHeader::getUserHeaderLength() const {return userHeaderLength;}


    /**
     * Get the length of the user-defined header in words.
     * @return length of the user-defined header in words.
     */
    uint32_t  FileHeader::getUserHeaderLengthWords() const {return userHeaderLengthWords;}


    /**
     * Get the Evio format version number.
     * @return Evio format version number.
     */
    uint32_t  FileHeader::getVersion() const {return headerVersion;}


    /**
     * Get the length of the index array in bytes.
     * @return length of the index array in bytes.
     */
    uint32_t  FileHeader::getIndexLength() const {return indexLength;}


    /**
     * Get the length of this header data in bytes.
     * @return length of this header data in bytes.
     */
    uint32_t  FileHeader::getHeaderLength() const {return headerLength;}


    /**
    * Get the total length of header + index + user header (including padding) in bytes.
    * Never compressed.
    * @return total length of header + index + user header (including padding) in bytes.
    */
    uint32_t FileHeader::getLength() const {return totalLength;}


    /**
     * Get the user header's padding - the number of bytes required to bring uncompressed
     * user header to 4-byte boundary.
     * @return  user header's padding
     */
    uint32_t FileHeader::getUserHeaderLengthPadding() const {return userHeaderLengthPadding;}


    //--------------------
    // Bit info methods
    //--------------------


    /**
     * Decodes the bit-info word into version, padding and header type.
     * @param word bit-info word.
     */
    void FileHeader::decodeBitInfoWord(uint32_t word) {
        // Padding
        userHeaderLengthPadding = (word >> 20) & 0x3;

        // Evio version
        headerVersion = (word & 0xff);

        // Header type
        headerType = HeaderType::getHeaderType((word >> 28) & 0xf);
        if (headerType == HeaderType::UNKNOWN) {
            headerType = HeaderType::EVIO_RECORD;
        }
    }


    /** Initialize bitInfo word to proper value. */
    void FileHeader::bitInfoInit() {
        bitInfo = (headerType.getValue() << 28) | (headerVersion & 0xFF);
    }


    /**
     * Get the bit info word.
     * @return bit info word.
     */
    uint32_t FileHeader::getBitInfoWord() const {return bitInfo;}


    /**
     * Set the bit info word and related values.
     * @param word  bit info word.
     */
    void  FileHeader::setBitInfoWord(uint32_t word) {
        bitInfo = word;
        decodeBitInfoWord(word);
    }


    /**
     * Set the bit info word for a file header.
     * Retains current header type, user header length padding, and version.
     * @param haveFirst  true if file has first event.
     * @param haveDictionary  true if file has dictionary in user header.
     * @param haveTrailerWithIndex  true if file has trailer with record length index.
     * @return new bit info word.
     */
    uint32_t FileHeader::setBitInfo(bool haveFirst,
                                    bool haveDictionary,
                                    bool haveTrailerWithIndex) {

        bitInfo = (headerType.getValue() << 28) |
                  (userHeaderLengthPadding << 20) |
                  (headerVersion & 0xFF);

        if (haveFirst) bitInfo |= FIRST_EVENT_BIT;
        if (haveDictionary) bitInfo |= DICTIONARY_BIT;
        if (haveTrailerWithIndex) bitInfo |= TRAILER_WITH_INDEX_BIT;

        return bitInfo;
    }


    /**
     * Calculates the bit info (6th) word of this header which has the version number
     * in the lowest 8 bits (0-7). The arg hasDictionary
     * is set in the 8th bit, hasFirst is set in the 9th bit, and trailerWithIndex is
     * set in the 10th bit.
     * Four bits of this header type are set in bits 28-31
     * (defaults to 1 which is an evio file header).
     *
     * @param version evio version number
     * @param hasDictionary does this file include an evio xml dictionary?
     * @param hasFirst does this file include a first event in every file split?
     * @param trailerWithIndex does this file have a trailer with an event index?
     * @param headerType 4 bit type of this header (defaults to 1 which is an evio file header).
     * @return generated bit-info (6th) word of a file header.
     */
    uint32_t FileHeader::generateBitInfoWord(uint32_t version, bool hasDictionary,
                                             bool hasFirst, bool trailerWithIndex,
                                             uint32_t headerType) {
        uint32_t v = version; // version
        v =  hasDictionary ? (v | 0x100) : v;
        v =  hasFirst ? (v | 0x200) : v;
        v =  trailerWithIndex ? (v | 0x400) : v;
        v |= ((headerType & 0xf) << 28);

        return v;
    }



    /**
     * Set the bit which says file has a first event.
     * @param hasFirst  true if file has a first event.
     * @return new bitInfo word.
     */
    uint32_t FileHeader::hasFirstEvent(bool hasFirst) {
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
     * Does this header have a first event in the file header?
     * @return true if header has a first event in the file header, else false.
     */
    bool FileHeader::hasFirstEvent() const {return ((bitInfo & FIRST_EVENT_BIT) != 0);}


    /**
      * Does this bitInfo arg indicate the existence of a first event in the file header? Static.
      * @param bitInfo bitInfo word.
      * @return true if header has a first event in the file header, else false.
      */
    bool FileHeader::hasFirstEvent(uint32_t bitInfo) {return ((bitInfo & FIRST_EVENT_BIT) != 0);}


    /**
     * Set the bit in the file header which says there is a dictionary.
     * @param hasDictionary  true if file has a dictionary.
     * @return new bitInfo word.
     */
    uint32_t FileHeader::hasDictionary(bool hasDictionary) {
        if (hasDictionary) {
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
     * Does this header have a dictionary in the file header?
     * @return true if header has a dictionary in the file header, else false.
     */
    bool FileHeader::hasDictionary() const {return ((bitInfo & DICTIONARY_BIT) != 0);}


    /**
     * Does this bitInfo arg indicate the existence of a dictionary in the file header?
     * @param bitInfo bitInfo word.
     * @return true if header has a dictionary in the file header, else false.
     */
    bool FileHeader::hasDictionary(uint32_t bitInfo) {return ((bitInfo & DICTIONARY_BIT) != 0);}


    /**
     * Set the bit in the file header which says there is a trailer with a record length index.
     * @param hasTrailerWithIndex  true if file has a trailer with a record length index.
     * @return new bitInfo word.
     */
    uint32_t FileHeader::hasTrailerWithIndex(bool hasTrailerWithIndex) {
        if (hasTrailerWithIndex) {
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
     * Does this file have a trailer with a record length index?
     * @return true if file has a trailer with a record length index, else false.
     */
    bool FileHeader::hasTrailerWithIndex() const {return ((bitInfo & TRAILER_WITH_INDEX_BIT) != 0);}


    /**
    * Does this bitInfo arg indicate the existence of a trailer with a record length index?
    * @param bitInfo bitInfo word.
    * @return true if file has a trailer with a record length index, else false.
    */
    bool FileHeader::hasTrailerWithIndex(uint32_t bitInfo) {
        return ((bitInfo & TRAILER_WITH_INDEX_BIT) != 0);
    }


    /**
     * Is this header followed by a user header?
     * @return true if header followed by a user header, else false.
     */
    bool FileHeader::hasUserHeader() const {return userHeaderLength > 0;}


    /**
     * Does this file have a valid index of record lengths immediately following header?
     * There should be at least one integer for valid index.
     * Must have integral number of entries.
     * @return true if file has a valid index, else false.
     */
    bool FileHeader::hasIndex() const {return ((indexLength > 3) && (indexLength % 4 == 0));}


    //-----------
    // Setters
    //-----------


    /**
     * Set the file number which is the split number starting at 1.
     * @param num  file number starting at 1.
     */
    FileHeader & FileHeader::setFileNumber(uint32_t num) {fileNumber = num; return *this;}


    /**
     * Set the first user register.
     * @param val  first user register.
     */
    FileHeader & FileHeader::setUserRegister(uint64_t val) {userRegister = val; return *this;}


    /**
     * Set the first user integer.
     * @param val  first user integer.
     */
    FileHeader & FileHeader::setUserIntFirst(uint32_t val) {userIntFirst = val; return *this;}


    /**
     * Set the second user integer.
     * @param val  second user integer.
     */
    FileHeader & FileHeader::setUserIntSecond(uint32_t val) {userIntSecond = val; return *this;}


    /**
     * Set this header's type. Normally done in constructor. Limited access.
     * @param type type of header.
     * @return this object.
     */
    FileHeader & FileHeader::setHeaderType(HeaderType & type) {headerType = type; return *this;}


    /**
     * Set the position of this record in a file.
     * @param pos position of this record in a file.
     * @return this object.
     */
    FileHeader & FileHeader::setPosition(size_t pos) {position = pos; return *this;}


    /**
     * Set the length of the index array in bytes.
     * Length is forced to be a multiple of 4!
     * Sets the total length too.
     * @param length  length of index array in bytes.
     * @return this object.
     */
    FileHeader & FileHeader::setIndexLength(uint32_t length) {
        indexLength = (length/4)*4;
        setLength(headerLength + indexLength + userHeaderLength + userHeaderLengthPadding);
        return *this;
    }


    /**
     * Set the number of record entries.
     * No compression for other values.
     * @param n number of record entries.
     * @return this object.
     */
    FileHeader & FileHeader::setEntries(uint32_t n) {entries = n; return *this;}


    /**
     * Set the user-defined header's length in bytes & words and the padding.
     * Sets the total length too.
     * @param length  user-defined header's length in bytes.
     * @return this object.
     */
    FileHeader & FileHeader::setUserHeaderLength(uint32_t length) {
        userHeaderLength = length;
        userHeaderLengthWords = getWords(length);
        // Set value and update associated value in bitInfo word
        setUserHeaderLengthPadding(getPadding(length));
        setLength(headerLength + indexLength + userHeaderLength + userHeaderLengthPadding);
        return *this;
    }


    /**
      * Set the user header's padding - the number of bytes required to bring uncompressed
      * user header to 4-byte boundary. Sets the associated value in bitInfo word.
      * @param padding user header's padding.
      */
    void FileHeader::setUserHeaderLengthPadding(uint32_t padding) {
        userHeaderLengthPadding = padding;
        bitInfo = bitInfo | (userHeaderLengthPadding << 20);
    }


    /**
     * Set the this header's length in bytes & words.
     * If length is not a multiple of 4, you're on your own!
     * Sets the total length too.
     * @param length  this header's length in bytes.
     * @return this object.
     */
    FileHeader & FileHeader::setHeaderLength(uint32_t length) {
        headerLength = length;
        headerLengthWords = length/4;
        setLength(headerLength + indexLength + userHeaderLength + userHeaderLengthPadding);
        return *this;
    }


    /**
     * Set the total length in bytes, header + index + user header.
     * This includes padding and is on a 4-byte boundary.
     * Never compressed.
     * @return this object.
     * @param length  total length in bytes, header + index + user header.
     */
    FileHeader & FileHeader::setLength(uint32_t length) {
        totalLength = length;
        return *this;
    }


    //-------------------------------------------------


    /**
     * Writes the file (not record!) header into the given byte buffer.
     * @param buf byte buffer to write file header into.
     * @param off position in buffer to begin writing.
     * @throws EvioException if remaining buffer space (limit - off) is too small.
     */
    void FileHeader::writeHeader(ByteBuffer & buf, size_t off) {

        if ((buf.limit() - off) < HEADER_SIZE_BYTES) {
            throw EvioException("buffer too small");
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
     * Writes the file (not record!) header into the given byte buffer.
     * @param buf byte buffer to write file header into.
     * @param off position in buffer to begin writing.
     * @throws EvioException if remaining buffer space (limit - off) is too small.
     */
    void FileHeader::writeHeader(std::shared_ptr<ByteBuffer> & buf, size_t off) {
        return writeHeader(*(buf.get()), off);
    }


    /**
     * Reads the file header information from a byte buffer and validates
     * it by checking the magic word (8th word). This magic word
     * also determines the byte order. The given buffer's position does
     * NOT change.
     *
     * @param buffer buffer to read from.
     * @param offset position of first word to be read.
     * @throws EvioException if remaining buffer space (limit - off) is too small,
     *                       data is not in proper format, or version earlier than 6.
     */
    void FileHeader::readHeader(ByteBuffer & buffer, size_t offset) {

        if ((buffer.limit() - offset) < HEADER_SIZE_BYTES) {
            throw EvioException("buf is too small");
        }

        // First read the magic word to establish endianness
        headerMagicWord = buffer.getInt(MAGIC_OFFSET + offset);

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

        // Next look at the version #
        bitInfo = buffer.getInt(BIT_INFO_OFFSET + offset);
        decodeBitInfoWord(bitInfo);
        if (headerVersion < 6) {
            throw EvioException("evio version < 6, = " + to_string(headerVersion));
        }

        fileId            = buffer.getInt(FILE_ID_OFFSET + offset);
        fileNumber        = buffer.getInt(FILE_NUMBER_OFFSET + offset);
        headerLengthWords = buffer.getInt(HEADER_LENGTH_OFFSET + offset);
        setHeaderLength(4*headerLengthWords);

        entries           = buffer.getInt(RECORD_COUNT_OFFSET + offset);
        indexLength       = buffer.getInt(INDEX_ARRAY_OFFSET + offset);
        setIndexLength(indexLength);

        userHeaderLength  = buffer.getInt(USER_LENGTH_OFFSET + offset);
        setUserHeaderLength(userHeaderLength);

        userRegister     = buffer.getLong(REGISTER1_OFFSET + offset);
        trailerPosition  = buffer.getLong(TRAILER_POSITION_OFFSET + offset);
        userIntFirst     = buffer.getInt (INT1_OFFSET + offset);
        userIntSecond    = buffer.getInt (INT2_OFFSET + offset);
    }


    /**
     * Reads the file header information from a byte buffer and validates
     * it by checking the magic word (8th word). This magic word
     * also determines the byte order. The given buffer's position does
     * NOT change.
     *
     * @param buffer buffer to read from.
     * @param offset position of first word to be read.
     * @throws EvioException if remaining buffer space (limit - off) is too small,
     *                       data is not in proper format, or version earlier than 6.
     */
    void FileHeader::readHeader(std::shared_ptr<ByteBuffer> & buffer, size_t offset) {
        readHeader(*(buffer.get()), offset);
    }


    /**
     * Returns a string representation of the record.
     * @return a string representation of the record.
     */
    string FileHeader::toString() const {

        stringstream ss;

        // show hex and 0x, and true for boolean
        ss << showbase << boolalpha << hex;

        ss << setw(24) << "ID" << " : " << fileId << (fileId == EVIO_FILE_UNIQUE_WORD ? ", Evio" : ", Hipo") << " file" << endl;
        ss << dec;
        ss << setw(24) << "version" << " : " << headerVersion << endl;
        ss << setw(24) << "file #" << " : " << fileNumber << ",  bytes,     words,    padding" << endl;

        ss << setw(24) << "user header length" << " : " << setw(8) << userHeaderLength << " / " <<
           setw(8) << userHeaderLengthWords << " / " << setw(8) << userHeaderLengthPadding << endl;

        ss << setw(24) << "header length"    << " : " << headerLength << endl;
        ss << hex;
        ss << setw(24) << "magic word"       << " : " << headerMagicWord << endl;
        std::bitset<32> infoBits(bitInfo);
        ss << setw(24) << "bit info bits"    << " : " << infoBits << endl;
        ss << setw(24) << "bit info word"    << " : " << bitInfo         << endl;
        ss << setw(24) << "has dictionary"   << " : " << hasDictionary() << endl;
        ss << setw(24) << "has firstEvent"   << " : " << hasFirstEvent() << endl;
        ss << setw(24) << "has trailer w/ index" << " : " << hasTrailerWithIndex() << endl;
        ss << dec;
        ss << setw(24) << "record entries"   << " : " << entries << endl;
        ss << setw(24) << "index length"     << " : " << indexLength << endl;
        ss << setw(24) << "trailer position" << " : " << trailerPosition << endl;
        ss << hex;
        ss << setw(24) << "user register"    << " : " << userRegister << endl;
        ss << setw(24) << "user int #1"      << " : " << userIntFirst << endl;
        ss << setw(24) << "user int #2"      << " : " << userIntSecond << endl;

        return ss.str();
    }

}


