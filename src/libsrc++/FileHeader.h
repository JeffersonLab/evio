//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_FILEHEADER_H
#define EVIO_6_0_FILEHEADER_H


#include <string>
#include <iostream>
#include <ios>
#include <iomanip>
#include <sstream>


#include "ByteOrder.h"
#include "HeaderType.h"
#include "ByteBuffer.h"
#include "Compressor.h"
#include "EvioException.h"
#include "Util.h"


namespace evio {


    /**
     * <pre><code>
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
     *    10    = File trailer with index array of record lengths exists
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
     * </code></pre>
     *
     * @version 6.0
     * @since 6.0 10/16/19
     * @author timmer
     */
    class FileHeader {


    private:

    public:

        /** First word in every HIPO file for identification purposes. */
        static const uint32_t   HIPO_FILE_UNIQUE_WORD = 0x4F504948; // 0x4849504F = HIPO
        /** First word in every Evio file for identification purposes. */
        static const uint32_t   EVIO_FILE_UNIQUE_WORD = 0x4556494F; // = EVIO
        /** Number of 32-bit words in a normal sized header. */
        static const uint32_t   HEADER_SIZE_WORDS = 14;
        /** Number of bytes in a normal sized header. */
        static const uint32_t   HEADER_SIZE_BYTES = 56;
        /** Magic number used to track endianness. */
        static const uint32_t   HEADER_MAGIC = 0xc0da0100;

        // Byte offset to header words

        /** Byte offset from beginning of header to the file id. */
        static const uint32_t   FILE_ID_OFFSET = 0;
        /** Byte offset from beginning of header to the file number. */
        static const uint32_t   FILE_NUMBER_OFFSET = 4;
        /** Byte offset from beginning of header to the header length. */
        static const uint32_t   HEADER_LENGTH_OFFSET = 8;
        /** Byte offset from beginning of header to the record count. */
        static const uint32_t   RECORD_COUNT_OFFSET = 12;
        /** Byte offset from beginning of header to the index array length. */
        static const uint32_t   INDEX_ARRAY_OFFSET = 16;
        /** Byte offset from beginning of header to bit info word. */
        static const uint32_t   BIT_INFO_OFFSET = 20;
        /** Byte offset from beginning of header to the user header length. */
        static const uint32_t   USER_LENGTH_OFFSET = 24;
        /** Byte offset from beginning of header to the record length. */
        static const uint32_t   MAGIC_OFFSET = 28;
        /** Byte offset from beginning of header to the user register #1. */
        static const uint32_t   REGISTER1_OFFSET = 32;
        /** Byte offset from beginning of header to write trailer position. */
        static const uint32_t   TRAILER_POSITION_OFFSET = 40;
        /** Byte offset from beginning of header to the user integer #1. */
        static const uint32_t   INT1_OFFSET = 48;
        /** Byte offset from beginning of header to the user integer #2. */
        static const uint32_t   INT2_OFFSET = 52;

        // Bits in bit info word

        /** 8th bit set in bitInfo word in record/file header means contains dictionary. */
        static const uint32_t   DICTIONARY_BIT = 0x100;
        /** 9th bit set in bitInfo word in file header means every split file has same first event. */
        static const uint32_t   FIRST_EVENT_BIT = 0x200;
        /** 10th bit set in bitInfo word in file header means file trailer with index array exists. */
        static const uint32_t   TRAILER_WITH_INDEX_BIT = 0x400;

    private:

        /** File id for file identification purposes. Defaults to HIPO file. 1st word. */
        uint32_t  fileId = HIPO_FILE_UNIQUE_WORD;
        /** File number or split file number, starting at 1. 2nd word. */
        uint32_t  fileNumber = 1;
        /** User-defined 64-bit register. 9th and 10th words. */
        uint64_t userRegister = 0L;
        /** Position of trailing header from start of file in bytes. 11th word. */
        uint64_t trailerPosition = 0L;
        /** First user-defined integer in file header. 13th word. */
        uint32_t  userIntFirst = 0;
        /** Second user-defined integer in file header. 14th word. */
        uint32_t  userIntSecond = 0;
        /** Position of this header in a file. */
        size_t position = 0;

        /** Event or record count. 4th word. */
        uint32_t  entries = 0;
        /** BitInfo & version. 6th word. */
        uint32_t  bitInfo = 0;
        /** Length of this header (bytes). */
        uint32_t  headerLength = HEADER_SIZE_BYTES;
        /** Length of this header (words). 3rd word. */
        uint32_t  headerLengthWords = HEADER_SIZE_WORDS;
        /** Length of user-defined header (bytes). 7th word. */
        uint32_t  userHeaderLength = 0;
        /** Length of user-defined header when padded (words). */
        uint32_t  userHeaderLengthWords = 0;
        /** Length of index array (bytes). 5th word. */
        uint32_t  indexLength = 0;

        /** Final, total length of header + index + user header (bytes) + padding.
         *  Not stored in any word. */
        uint32_t  totalLength = HEADER_SIZE_BYTES;

        /** Evio format version number. It is 6 when being written, else
         * the version of file/buffer being read. Lowest byte of 6th word. */
        uint32_t  headerVersion = 6;
        /** Magic number for tracking endianness. 8th word. */
        uint32_t  headerMagicWord = HEADER_MAGIC;

        /** Number of bytes required to bring uncompressed
          * user header to 4-byte boundary. Stored in 6th word.
          * Updated automatically when lengths are set. */
        uint32_t  userHeaderLengthPadding = 0;

        /** Byte order of file. */
        ByteOrder  byteOrder {ByteOrder::ENDIAN_LOCAL};

        /** Type of header this is. Normal HIPO record by default. */
        HeaderType headerType {HeaderType::HIPO_FILE};

    public:

        FileHeader();
        FileHeader(const FileHeader & header);
        explicit FileHeader(bool isEvio);
        ~FileHeader() = default;

        void copy(const FileHeader & head);
        void reset();

    private:

        void bitInfoInit();
        void decodeBitInfoWord(uint32_t word);
        void setUserHeaderLengthPadding(uint32_t padding);

    public:

        // Getters

        const ByteOrder  & getByteOrder()  const;
        const HeaderType & getHeaderType() const;

        uint32_t  getFileNumber() const;
        uint32_t  getFileId()  const;
        uint32_t  getVersion() const;
        uint32_t  getEntries() const;

        uint32_t  getLength() const;
        uint32_t  getIndexLength() const;
        uint32_t  getHeaderLength() const;

        uint32_t  getUserHeaderLength() const;
        uint32_t  getUserHeaderLengthWords() const;
        uint32_t  getUserHeaderLengthPadding() const;

        uint32_t  getUserIntFirst()  const;
        uint32_t  getUserIntSecond() const;
        uint64_t  getUserRegister()  const;

        size_t    getPosition() const;
        size_t    getTrailerPosition() const;

        //--------------------
        // Bit info methods
        //--------------------

        uint32_t getBitInfoWord() const;
        void     setBitInfoWord(uint32_t word);
        uint32_t setBitInfo(bool haveFirst, bool haveDictionary, bool haveTrailerWithIndex);
        static uint32_t generateBitInfoWord(uint32_t version, bool hasDictionary,
                                            bool hasFirst, bool trailerWithIndex,
                                            uint32_t headerType = 1);

        uint32_t hasFirstEvent(bool hasFirst);
        bool     hasFirstEvent() const;

        uint32_t hasDictionary(bool hasDictionary);
        bool     hasDictionary() const;

        uint32_t hasTrailerWithIndex(bool hasTrailerWithIndex);
        bool     hasTrailerWithIndex() const;

        bool hasUserHeader() const;
        bool hasIndex() const;

        static bool hasFirstEvent(uint32_t bitInfo);
        static bool hasDictionary(uint32_t bitInfo);
        static bool hasTrailerWithIndex(uint32_t bitInfo);

        // Setters

        FileHeader & setFileNumber(uint32_t num);
        FileHeader & setUserRegister(uint64_t val);
        FileHeader & setUserIntFirst(uint32_t val);
        FileHeader & setUserIntSecond(uint32_t val);
        FileHeader & setHeaderType(HeaderType & type);
        FileHeader & setPosition(size_t pos);
        FileHeader & setIndexLength(uint32_t length);
        FileHeader & setEntries(uint32_t n);
        FileHeader & setUserHeaderLength(uint32_t length);
        FileHeader & setHeaderLength(uint32_t length);
        FileHeader & setLength(uint32_t length);


        // Writing, reading, printing
        void writeHeader(ByteBuffer & buf, size_t off);
        void writeHeader(std::shared_ptr<ByteBuffer> & buf, size_t off = 0);
        void readHeader(ByteBuffer & buffer, size_t offset = 0);
        void readHeader(std::shared_ptr<ByteBuffer> & buffer, size_t offset = 0);

        std::string toString() const;
    };

}


#endif //EVIO_6_0_FILEHEADER_H
