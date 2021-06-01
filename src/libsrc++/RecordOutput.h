//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_RECORDOUTPUT_H
#define EVIO_6_0_RECORDOUTPUT_H


#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <memory>


#include "ByteBuffer.h"
#include "ByteOrder.h"
#include "EvioBank.h"
#include "EvioNode.h"
#include "RecordHeader.h"
#include "FileHeader.h"
#include "Compressor.h"
#include "EvioException.h"


namespace evio {


    /**
     * Class which handles the creation and use of Evio & HIPO Records.<p>
     *
     * <pre><code>
     * RECORD STRUCTURE:
     *
     *               Uncompressed                                      Compressed
     *
     *    +----------------------------------+            +----------------------------------+
     *    |       General Record Header      |            |       General Record Header      |
     *    +----------------------------------+            +----------------------------------+
     *
     *    +----------------------------------+ ---------> +----------------------------------+
     *    |      Index Array (bytes)         |            |        Compressed Data           |
     *    +----------------------------------+            |             Record               |
     *                                                    |                                  |
     *    +----------------------------------+            |                                  |
     *    |           User Header            |            |                  ----------------|
     *    |           (Optional)             |            |                  |    Pad 3      |
     *    |                  ----------------|            +----------------------------------+
     *    |                  |    Pad 1      |           ^
     *    +----------------------------------+          /
     *                                                 /
     *    +----------------------------------+        /
     *    |           Data Record            |       /
     *    |                                  |      /
     *    |                  ----------------|     /
     *    |                  |    Pad 2      |    /
     *    +----------------------------------+----
     *
     *
     *
     *
     * GENERAL RECORD HEADER STRUCTURE ( see RecordHeader.java )
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
     * 10 +  CT  |  Data Length Compressed   | // CT = compression type (4 bits)
     *    +----------------------------------+
     * 11 +        General Register 1        | // UID 1st (64 bits)
     *    +--                              --+
     * 12 +                                  |
     *    +----------------------------------+
     * 13 +        General Register 2        | // UID 2nd (64 bits)
     *    +--                              --+
     * 14 +                                  |
     *    +----------------------------------+
     * </code></pre>
     *
     * @version 6.0
     * @since 6.0 4/9/2019
     * @author timmer
     */
    class RecordOutput {

    public:

        /** Maximum number of events per record. */
        static constexpr int ONE_MEG = 1024*1024;

        /** Maximum number of events per record. */
        uint32_t MAX_EVENT_COUNT = 1000000;

        /**
         * Size of some internal buffers in bytes. If the recordBinary buffer is passed
         * into the constructor or given through {@link #setBuffer(std::shared_ptr<ByteBuffer> &)}, then this
         * value is 91% of the its size (from position to capacity). This allows some
         * margin for compressed data to be larger than the uncompressed - which may
         * happen if data is random. It also allows other records to have been previously
         * stored in the given buffer (eg. common record) since it starts writing at the
         * buffer position which may not be 0.
         */
        uint32_t MAX_BUFFER_SIZE = 8*ONE_MEG;

        /**
         * Size of buffer holding built record in bytes. If the recordBinary buffer is passed
         * into the constructor or given through {@link #setBuffer(std::shared_ptr<ByteBuffer> &)}, then this
         * value is set to be 10% bigger than {@link #MAX_BUFFER_SIZE}. This allows some
         * margin for compressed data to be larger than the uncompressed - which may
         * happen if data is random.
         */
        uint32_t RECORD_BUFFER_SIZE = 9*ONE_MEG;

        /** The number of initially available bytes to be written into in the user-given buffer,
         *  that go from position to limit. The user-given buffer is stored in recordBinary.
         */
        uint32_t userBufferSize = 0;

        /** Number of events written to this Record. */
        uint32_t eventCount = 0;

        /** Number of valid bytes in recordIndex buffer.
         *  Will always be multiple of 4 since indexes are ints. */
        uint32_t indexSize = 0;

        /** Number of valid bytes in recordEvents buffer. */
        uint32_t eventSize = 0;

        /** The starting position of a user-given buffer.
         * No data will be written before this position. */
        size_t startingPosition = 0;

        /** This buffer stores event lengths (in bytes) ONLY. */
        std::shared_ptr<ByteBuffer> recordIndex;

        /** This buffer stores event data ONLY. */
        std::shared_ptr<ByteBuffer> recordEvents;

        /** This buffer stores data that will be compressed. */
        std::shared_ptr<ByteBuffer> recordData;

        /** Buffer in which to put constructed (& compressed) binary record.
         *  May be provided by user. */
        std::shared_ptr<ByteBuffer> recordBinary;

        /** Header of this Record. */
        std::shared_ptr<RecordHeader> header;

        /** Byte order of record byte arrays to build. */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LOCAL};

        /** Is recordBinary a user provided buffer? */
        bool userProvidedBuffer = false;


    public:


        RecordOutput();

        explicit RecordOutput(const ByteOrder & order,
                              uint32_t maxEventCount = 1000000,
                              uint32_t maxBufferSize = 8*ONE_MEG,
                              Compressor::CompressionType compressionType = Compressor::UNCOMPRESSED,
                              HeaderType hType = HeaderType::EVIO_RECORD);

        RecordOutput(std::shared_ptr<ByteBuffer> & buffer, uint32_t maxEventCount,
                     Compressor::CompressionType compressionType, HeaderType hType);

        RecordOutput(const RecordOutput & srcRec);
        RecordOutput(RecordOutput && srcBuf) noexcept;

        RecordOutput & operator=(RecordOutput&& other) noexcept;
        RecordOutput & operator=(const RecordOutput& other);

        ~RecordOutput() = default;


    private:


        void allocate();
        bool allowedIntoRecord(uint32_t length);
        void copy(const RecordOutput & rec);


    public:


        void setBuffer(std::shared_ptr<ByteBuffer> & buf);
        void transferDataForReading(const RecordOutput & rec);

        uint32_t getUserBufferSize() const;
        uint32_t getUncompressedSize() const;
        uint32_t getInternalBufferCapacity() const;
        uint32_t getMaxEventCount() const;
        uint32_t getEventCount() const;

        std::shared_ptr<RecordHeader> & getHeader();
        const ByteOrder & getByteOrder() const;
        const std::shared_ptr<ByteBuffer> getBinaryBuffer() const;
        const Compressor::CompressionType getCompressionType() const;
        const HeaderType getHeaderType() const;

        bool hasUserProvidedBuffer() const;
        bool roomForEvent(uint32_t length) const;
        bool oneTooMany() const;

        bool addEvent(const uint8_t * event, uint32_t eventLen, uint32_t extraDataLen = 0);

        bool addEvent(const std::vector<uint8_t> & event);
        bool addEvent(const std::vector<uint8_t> & event, size_t offset, uint32_t eventLen, uint32_t extraDataLen = 0);

        bool addEvent(const ByteBuffer & event, uint32_t extraDataLen = 0);
        bool addEvent(const std::shared_ptr<ByteBuffer> & event, uint32_t extraDataLen = 0);

        bool addEvent(EvioNode & node, uint32_t extraDataLen = 0);
        bool addEvent(std::shared_ptr<EvioNode> & node, uint32_t extraDataLen = 0);

        bool addEvent(EvioBank & event, uint32_t extraDataLen);
        bool addEvent(std::shared_ptr<EvioBank> & event, uint32_t extraDataLen = 0);

        void reset();

        void setStartingBufferPosition(size_t pos);

        void build();
        void build(std::shared_ptr<ByteBuffer> userHeader);
        void build(const ByteBuffer & userHeader);

    };

}


#endif //EVIO_6_0_RECORDOUTPUT_H
