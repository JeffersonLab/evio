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
#include "EvioNode.h"
#include "RecordOutput.h"
#include "RecordHeader.h"
#include "Compressor.h"
#include "Util.h"

/**
 * Class to write Evio-6.0/HIPO files.
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
    std::string fileName = "";
    /** Object for writing file. */
    std::ofstream outFile;
    /** Header to write to file, created in constructor. */
    FileHeader fileHeader;
    /** Used to write file asynchronously. Allow 1 write with 1 simultaneous record filling. */
    std::future<void> future;
    /** Temp storage for next record to be written to. */
    RecordOutput unusedRecord;

    // If writing to buffer ...

    /** Buffer being written to. */
    ByteBuffer buffer;
//    /** Has the first record been written to buffer already? */
//    bool firstRecordWritten = false;

    // For both files & buffers

    /** Buffer containing user Header. */
    ByteBuffer userHeaderBuffer;
    /** Byte array containing user Header. */
    uint8_t* userHeader = nullptr;
    /** Size in bytes of userHeader array. */
    uint32_t userHeaderLength = 0;
    /** Evio format "first" event to store in file header's user header. */

    /** String containing evio-format XML dictionary to store in file header's user header. */
    string dictionary;
    /** If dictionary and or firstEvent exist, this buffer contains them both as a record. */
    ByteBuffer dictionaryFirstEventBuffer;
    /** Evio format "first" event to store in file header's user header. */
    uint8_t* firstEvent = nullptr;
    /** Length in bytes of firstEvent. */
    uint32_t firstEventLength = 0;

    /** Byte order of data to write to file/buffer. */
    ByteOrder byteOrder {ByteOrder::ENDIAN_LITTLE};
    /** Record currently being filled. */
    RecordOutput outputRecord;
    /** Record currently being written to file. */
    RecordOutput beingWrittenRecord;
    /** Byte array large enough to hold a header/trailer. This array may increase. */
    vector<uint8_t> headerArray;

    /** Type of compression to use on file. Default is none. */
    Compressor::CompressionType compressionType {Compressor::UNCOMPRESSED};

    /** List of record lengths interspersed with record event counts
     * to be optionally written in trailer. */
    vector<uint32_t> recordLengths;

    /** Number of bytes written to file/buffer at current moment. */
    size_t writerBytesWritten = 0;
    /** Number which is incremented and stored with each successive written record starting at 1. */
    uint32_t recordNumber = 1;

    /** Do we add a last header or trailer to file/buffer? */
    bool addingTrailer = true;
    /** Do we add a record index to the trailer? */
    bool addTrailerIndex = false;
    /** Has close() been called? */
    bool closed = false;
    /** Has open() been called? */
    bool opened = false;
    /** Has the first record been written already? */
    bool firstRecordWritten = false;
    /** Has a dictionary been defined? */
    bool haveDictionary = false;
    /** Has a first event been defined? */
    bool haveFirstEvent = false;
    /** Has caller defined a file header's user-header which is not dictionary/first-event? */
    bool haveUserHeader = false;

public:

    // Writing to file

    Writer();

    explicit Writer(const ByteOrder & order,
                    uint32_t maxEventCount = 0,
                    uint32_t maxBufferSize = 0);

    Writer(string & filename,
           const ByteOrder & order,
           uint32_t maxEventCount = 0,
           uint32_t maxBufferSize = 0);

    explicit Writer(const HeaderType & hType,
                    const ByteOrder & order = ByteOrder::ENDIAN_LITTLE,
                    uint32_t maxEventCount = 0,
                    uint32_t maxBufferSize = 0,
                    const string & dictionary = string(""),
                    uint8_t* firstEvent = nullptr,
                    uint32_t firstEventLength = 0,
                    const Compressor::CompressionType & compressionType = Compressor::UNCOMPRESSED,
                    bool addTrailerIndex = false);

    // Writing to buffer

    explicit Writer(ByteBuffer & buf);
    Writer(ByteBuffer & buf, uint32_t maxEventCount, uint32_t maxBufferSize,
           const string & dictionary, uint8_t* firstEvent, uint32_t firstEventLength);

    ~Writer() = default;


//////////////////////////////////////////////////////////////////////

private:

    //    Writer & operator=(Writer&& other) noexcept;
    // Don't allow assignment
    Writer & operator=(const Writer& other);

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
//    RecordHeader & getRecordHeader();
//    RecordOutput & getRecord();
    Compressor::CompressionType getCompressionType();

    bool addTrailer() const;
    void addTrailer(bool add);
    bool addTrailerWithIndex();
    void addTrailerWithIndex(bool addTrailingIndex);

    void open(string & filename);
    void open(string & filename, uint8_t* userHdr, uint32_t len);
    void open(ByteBuffer & buf,  uint8_t* userHdr, uint32_t len);

    static ByteBuffer createRecord(const string & dictionary,
                                     uint8_t* firstEvent, uint32_t firstEventLen,
                                     const ByteOrder & byteOrder,
                                     FileHeader* fileHeader,
                                     RecordHeader* recordHeader);

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
    void addEvent(EvioNode & node);

    void reset();
    void close();

};



#endif //EVIO_6_0_WRITER_H
