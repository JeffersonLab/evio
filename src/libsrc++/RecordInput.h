//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_RECORDINPUT_H
#define EVIO_6_0_RECORDINPUT_H


#include <iostream>
#include <fstream>
#include <memory>


#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "RecordHeader.h"
#include "Compressor.h"
#include "EvioException.h"


namespace evio {


    /**
     *
     *  Class which reads data to create an Evio or HIPO Record.
     *  This class is NOT thread safe!<p>
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
     *    |           Index Array            |            |        Compressed Data           |
     *    +----------------------------------+            |             Record               |
     *                                                    |                                  |
     *    +----------------------------------+            |                                  |
     *    |           User Header            |            |                  ----------------|
     *    |           (Optional)             |            |                  |    Pad 3      |
     *    |                  ----------------|            +----------------------------------+
     *    |                  |    Pad 1      |           ^
     *    +----------------------------------+          /
     *                                                 /
     *    +----------------------------------+       /
     *    |           Data Record            |     /
     *    |                                  |    /
     *    |                  ----------------|   /
     *    |                  |    Pad 2      | /
     *    +----------------------------------+
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
     * @since 6.0 10/13/17
     * @author gavalian
     * @author timmer (C++ version)
     */
    class RecordInput {

    private:

        /** Default internal buffer size in bytes. */
        static const uint32_t DEFAULT_BUF_SIZE = 8 * 1024 * 1024;

        /** Number of event in record. */
        uint32_t nEntries = 0;

        /** Offset, in uncompressed dataBuffer, from just past header to user header
         *  (past index) in bytes. */
        uint32_t userHeaderOffset = 0;

        /** Offset, in uncompressed dataBuffer, from just past header to event data
         *  (past index + user header) in bytes. */
        uint32_t eventsOffset = 0;

        /** Length in bytes of uncompressed data (events) in dataBuffer, not including
         * header, index or user header. */
        uint32_t uncompressedEventsLength = 0;

        /** Byte order of internal ByteBuffers. */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LOCAL};

        /** General header of this record. */
        std::shared_ptr<RecordHeader> header;

        /** This buffer contains uncompressed data consisting of, in order,
         *  1) index array, 2) user header, 3) events. */
        std::shared_ptr<ByteBuffer> dataBuffer;

        /** This buffer contains compressed data. */
        ByteBuffer recordBuffer;

        /** Record's header is read into this buffer. */
        ByteBuffer headerBuffer;


    private:

        void allocate(size_t size);
        void setByteOrder(const ByteOrder & order);
        void showIndex() const;

    public:

        RecordInput();
        explicit RecordInput(const ByteOrder & order);
        RecordInput(const RecordInput & recordIn);
        RecordInput(RecordInput && srcRec) noexcept;

        ~RecordInput() = default;

        RecordInput & operator=(RecordInput&& other) noexcept;
        RecordInput & operator=(const RecordInput& other);

        std::shared_ptr<RecordHeader> getHeader();
        const ByteOrder & getByteOrder();
        std::shared_ptr<ByteBuffer> getUncompressedDataBuffer();

        bool hasIndex() const;
        bool hasUserHeader() const;

        std::shared_ptr<ByteBuffer> getEvent(std::shared_ptr<ByteBuffer> & buffer, uint32_t index, size_t bufOffset = 0);
        ByteBuffer & getEvent(ByteBuffer & buffer, uint32_t index, size_t bufOffset = 0);
        std::shared_ptr<uint8_t> getUserHeader();
        std::shared_ptr<ByteBuffer> getUserHeader(std::shared_ptr<ByteBuffer> & buffer, size_t bufOffset = 0);
        ByteBuffer & getUserHeader(ByteBuffer & buffer, size_t bufOffset = 0);

        std::shared_ptr<uint8_t> getEvent(uint32_t index, uint32_t * len);
        uint32_t getEventLength(uint32_t index) const;
        uint32_t getEntries() const;

        std::shared_ptr<RecordInput> getUserHeaderAsRecord(ByteBuffer & buffer,
                                                           size_t bufOffset);

        void readRecord(std::ifstream & file, size_t position);
        void readRecord(ByteBuffer & buffer, size_t offset);

        static uint32_t uncompressRecord(std::shared_ptr<ByteBuffer> & srcBuf, size_t srcOff,
                                               std::shared_ptr<ByteBuffer> & dstBuf,
                                               RecordHeader & hdr);
       static uint32_t uncompressRecord(ByteBuffer & srcBuf, size_t srcOff,
                                        ByteBuffer & dstBuf,
                                        RecordHeader & header);
    };

}
#endif //EVIO_6_0_RECORDINPUT_H
