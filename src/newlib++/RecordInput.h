//
// Created by timmer on 4/9/19.
//

#ifndef EVIO_6_0_RECORDINPUT_H
#define EVIO_6_0_RECORDINPUT_H


#include <iostream>
#include <fstream>
#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "RecordHeader.h"
#include "Compressor.h"

/**
 *
 *  Class which reads data to create an Evio or HIPO Record.
 *  This class is NOT thread safe!<p>
 *
 * <pre>
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
 * </pre>
 *
 * @version 6.0
 * @since 6.0 10/13/17
 * @author gavalian
 */
class RecordInput {

private:

    /** Default internal buffer size in bytes. */
    static const uint32_t DEFAULT_BUF_SIZE = 8 * 1024 * 1024;

    /** General header of this record. */
    RecordHeader header;

    /** This buffer contains uncompressed data consisting of, in order,
     *  1) index array, 2) user header, 3) events. */
    ByteBuffer dataBuffer;

    /** This buffer contains compressed data. */
    ByteBuffer recordBuffer;

    /** Record's header is read into this buffer. */
    ByteBuffer headerBuffer;

    /** Number of event in record. */
    uint32_t nEntries;

    /** Offset, in uncompressed dataBuffer, from just past header to user header
     *  (past index). */
    uint32_t userHeaderOffset;

    /** Offset, in uncompressed dataBuffer, from just past header to event data
     *  (past index + user header). */
    uint32_t eventsOffset;

    /** Length in bytes of uncompressed data (events) in dataBuffer, not including
     * header, index or user header. */
    uint32_t uncompressedEventsLength;

    /** Byte order of internal ByteBuffers. */
    ByteOrder byteOrder = ByteOrder::ENDIAN_LITTLE;

private:

    void allocate(size_t size);
    void setByteOrder(const ByteOrder & order);
    void showIndex();

public:

    RecordInput();
    explicit RecordInput(ByteOrder & order);
    RecordHeader & getHeader();

    const ByteOrder & getByteOrder();
    ByteBuffer & getUncompressedDataBuffer();

    bool hasIndex();
    bool hasUserHeader();

    ByteBuffer & getEvent(ByteBuffer & buffer, uint32_t index);
    ByteBuffer & getEvent(ByteBuffer & buffer, size_t bufOffset, uint32_t index);
    ByteBuffer & getUserHeader(ByteBuffer & buffer, size_t bufOffset);

    uint8_t* getEvent(uint32_t index);
    uint32_t getEventLength(uint32_t index);
    uint8_t* getUserHeader();
    uint32_t getEntries();

    bool getUserHeaderAsRecord(ByteBuffer & buffer, size_t bufOffset, RecordInput & record);

    void readRecord(ifstream & file, size_t position);
    void readRecord(ByteBuffer & buffer, size_t offset);

    static uint32_t uncompressRecord(ByteBuffer & srcBuf, size_t srcOff, ByteBuffer & dstBuf,
                                     RecordHeader & header);
};
#endif //EVIO_6_0_RECORDINPUT_H
