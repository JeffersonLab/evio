//
// Created by timmer on 4/9/19.
//

#ifndef EVIO_6_0_RECORDOUTPUT_H
#define EVIO_6_0_RECORDOUTPUT_H


#include <cstdlib>
#include <cstdint>
#include <cstdio>

#include "ByteBuffer.h"
#include "ByteOrder.h"
#include "RecordHeader.h"
#include "FileHeader.h"
#include "Compressor.h"

/**
 * Class which handles the creation and use of Evio & HIPO Records.<p>
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
 * @since 6.0 9/6/17
 * @author gavalian
 * @author timmer
 */
class RecordOutput {

private:

    /** Maximum number of events per record. */
    static constexpr int ONE_MEG = 1024*1024;


    /** Maximum number of events per record. */
    uint32_t MAX_EVENT_COUNT = 1000000;

    /**
     * Size of some internal buffers in bytes. If the recordBinary buffer is passed
     * into the constructor or given through {@link #setBuffer(ByteBuffer)}, then this
     * value is 91% of the its size (from position to capacity). This allows some
     * margin for compressed data to be larger than the uncompressed - which may
     * happen if data is random. It also allows other records to have been previously
     * stored in the given buffer (eg. common record) since it starts writing at the
     * buffer position which may not be 0.
     */
    uint32_t MAX_BUFFER_SIZE = 8*ONE_MEG;

    /**
     * Size of buffer holding built record in bytes. If the recordBinary buffer is passed
     * into the constructor or given through {@link #setBuffer(ByteBuffer)}, then this
     * value is set to be 10% bigger than {@link #MAX_BUFFER_SIZE}. This allows some
     * margin for compressed data to be larger than the uncompressed - which may
     * happen if data is random.
     */
    uint32_t RECORD_BUFFER_SIZE = 9*ONE_MEG;

    /** This buffer stores event lengths ONLY. */
    ByteBuffer recordIndex;

    /** This buffer stores event data ONLY. */
    ByteBuffer recordEvents;

    /** This buffer stores data that will be compressed. */
    ByteBuffer recordData;

    /** Buffer in which to put constructed (& compressed) binary record.
     * Code is written so that it works whether or not it's backed by an array. */
    ByteBuffer recordBinary;

    /** The number of initially available bytes to be written into in the user-given buffer,
     * that go from position to limit. The user-given buffer is stored in recordBinary.
     */
    uint32_t userBufferSize;

    /** Is recordBinary a user provided buffer? */
    bool userProvidedBuffer;

    /** Header of this Record. */
    RecordHeader header;

    /** Number of events written to this Record. */
    uint32_t eventCount;

    /** Number of valid bytes in recordIndex buffer */
    uint32_t indexSize;

    /** Number of valid bytes in recordEvents buffer. */
    uint32_t eventSize;

    /** The starting position of a user-given buffer.
     * No data will be written before this position. */
    size_t startingPosition;

    /** Byte order of record byte arrays to build. */
    ByteOrder byteOrder = ByteOrder::ENDIAN_LITTLE;


public:

    /** Default, no-arg constructor. Little endian. LZ4 compression. */
    RecordOutput();

    RecordOutput(const ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize,
                 Compressor::CompressionType compressionType, HeaderType hType = HeaderType::HIPO_RECORD);

    RecordOutput(ByteBuffer & buffer, uint32_t maxEventCount,
                 Compressor::CompressionType compressionType, HeaderType & hType);

private:

    void allocate();
    bool allowedIntoRecord(uint32_t length);

public:

    void setBuffer(ByteBuffer & buf);
    void copy(const RecordOutput & rec);

    int getUserBufferSize() const;
    int getUncompressedSize() const;
    int getInternalBufferCapacity() const;
    int getEventCount() const;

    RecordHeader & getHeader();
    const ByteOrder & getByteOrder() const;
    ByteBuffer & getBinaryBuffer();

    bool hasUserProvidedBuffer() const;
    bool roomForEvent(uint32_t length) const;
    bool oneTooMany() const;

//    bool addEvent(uint8_t event[]);
    bool addEvent(const uint8_t* event, size_t position, uint32_t eventLen);
    bool addEvent(const uint8_t* event, size_t position, uint32_t eventLen, uint32_t extraDataLen);

    bool addEvent(const ByteBuffer & event);
    bool addEvent(const ByteBuffer & event, uint32_t extraDataLen);

//    bool addEvent(EvioNode & node);
//    bool addEvent(EvioNode & node, uint32_t extraDataLen);
//    bool addEvent(EvioBank & event);
//    bool addEvent(EvioBank & event, uint32_t extraDataLen);

    void reset();

    void setStartingBufferPosition(size_t pos);
    void setByteOrder(const ByteOrder & order);

    void build();
    void build(ByteBuffer & userHeader);

};


#endif //EVIO_6_0_RECORDOUTPUT_H
