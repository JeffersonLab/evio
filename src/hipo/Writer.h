//
// Created by Carl Timmer on 2019-05-06.
//

#ifndef EVIO_6_0_WRITER_H
#define EVIO_6_0_WRITER_H


#include <fstream>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <future>
#include <chrono>

#include "FileHeader.h"
#include "ByteBuffer.h"
#include "ByteOrder.h"
#include "RecordOutput.h"
#include "RecordHeader.h"
#include "Compressor.h"

/**
 * Class to write Evio/HIPO files.
 *
 * @version 6.0
 * @since 6.0 8/10/17
 * @author timmer
 */
class Writer {

private:

    /** Do we write to a file or a buffer? */
    bool toFile = true;

    // If writing to file ...

    /** File name. */
    std::string fileName = nullptr;
    /** Object for writing file. */
    std::ofstream outFile;
    /** Header to write to file, created in constructor. */
    FileHeader fileHeader;
    /** Used to write file asynchronously. Allow 1 write with 1 simultaneous record filling. */
    std::future<void> future;

    // If writing to buffer ...

    /** Buffer being written to. */
    ByteBuffer buffer;
    /** Has the first record been written already? */
    bool firstRecordWritten;

    // For both files & buffers

    /** Buffer containing user Header. */
    ByteBuffer userHeaderBuffer;
    /** Byte array containing user Header. */
    uint8_t* userHeader;
    /** Size in bytes of userHeader array. */
    uint32_t userHeaderLength;

    /** String containing evio-format XML dictionary to store in file header's user header. */
    string dictionary;
    /** Evio format "first" event to store in file header's user header. */
    uint8_t* firstEvent;
    /** Length in bytes of firstEvent. */
    uint32_t firstEventLength;
    /** If dictionary and or firstEvent exist, this buffer contains them both as a record. */
    ByteBuffer dictionaryFirstEventBuffer;

    /** Byte order of data to write to file/buffer. */
    ByteOrder byteOrder = ByteOrder::ENDIAN_LITTLE;

    /** Record currently being filled. */
    RecordOutput outputRecord;
    /** Temp storage for next record to be written to. */
    RecordOutput unusedRecord;
    /** Record currently being written to file. */
    RecordOutput beingWrittenRecord;

    /** Byte array large enough to hold a header/trailer. This array may increase. */
    vector<uint8_t> headerArray;
    /** List of record lengths interspersed with record event counts
     * to be optionally written in trailer. */
    vector<uint32_t> recordLengths;

    /** Type of compression to use on file. Default is none. */
    Compressor::CompressionType compressionType;
    /** Number of bytes written to file/buffer at current moment. */
    size_t writerBytesWritten;
    /** Number which is incremented and stored with each successive written record starting at 1. */
    uint32_t recordNumber = 1;

    /** Do we add a last header or trailer to file/buffer? */
    bool addingTrailer;
    /** Do we add a record index to the trailer? */
    bool addTrailerIndex;
    /** Has close() been called? */
    bool closed;
    /** Has open() been called? */
    bool opened;

public:

    // Writing to file

    Writer();
    Writer(const ByteOrder  & order, uint32_t maxEventCount, uint32_t maxBufferSize);
    Writer(const HeaderType & hType, const ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize);
    Writer(const HeaderType & hType, const ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize,
           const string & dictionary, uint8_t* firstEvent, uint32_t firstEventLength);
    Writer(string & filename);
    Writer(string & filename, const ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize);

    // Writing to buffer

    Writer(ByteBuffer & buf);
    Writer(ByteBuffer & buf, const ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize,
           const string & dictionary, uint8_t* firstEvent, uint32_t firstEventLength);

    ~Writer() {
       // delete[] headerArray;
    }


//    Writer & operator=(Writer&& other) noexcept;
    Writer & operator=(const Writer& other);


//////////////////////////////////////////////////////////////////////

private:

    ByteBuffer createDictionaryRecord();
    void writeOutput();
    void writeOutputToBuffer();

    // TODO: this should be part of an Utilities class ...
    static void toBytes(uint32_t data, const ByteOrder & byteOrder,
                        uint8_t* dest, uint32_t off, uint32_t destMaxSize);

    static void staticWriteFunction(Writer *pWriter, const char* data, size_t len);

public:

    const ByteOrder & getByteOrder() const;
    ByteBuffer   & getBuffer();
    FileHeader   & getFileHeader();
    RecordHeader & getRecordHeader();
    RecordOutput & getRecord();
    uint32_t getCompressionType();

    bool addTrailer() const;
    void addTrailer(bool add);
    bool addTrailerWithIndex();
    void addTrailerWithIndex(bool addTrailingIndex);

    void open(string & filename);
    void open(string & filename, uint8_t* userHdr, uint32_t userLen);
    void open(ByteBuffer & buf,  uint8_t* userHdr, uint32_t userLen);

    static ByteBuffer createRecord(const string & dictionary,
                                     uint8_t* firstEvent, uint32_t firstEventLen,
                                     const ByteOrder & byteOrder,
                                     FileHeader* fileHeader,
                                     RecordHeader* recordHeader);

    Writer & setCompressionType(Compressor::CompressionType compression);


    //ByteBuffer & createHeader(uint8_t* userHeader, size_t len);
    ByteBuffer createHeader(uint8_t* userHdr, uint32_t userLen);
    ByteBuffer createHeader(ByteBuffer & userHdr);
    void createHeader(ByteBuffer & buf, uint8_t* userHdr, uint32_t userLen);
    void createHeader(ByteBuffer & buf, ByteBuffer & userHdr);
        //ByteBuffer & createHeader(ByteBuffer & userHeader);

    void writeTrailer(bool writeIndex);
    void writeRecord(RecordOutput & record);

    // Use internal RecordOutput to write individual events

//    void addEvent(uint8_t* buffer);
    void addEvent(uint8_t* buffer, uint32_t offset, uint32_t length);
    void addEvent(ByteBuffer & buffer);
//    void addEvent(EvioBank & bank);
//    void addEvent(EvioNode & node);

    void reset();
    void close();

};



#endif //EVIO_6_0_WRITER_H
