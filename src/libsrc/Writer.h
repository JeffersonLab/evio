//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


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
#include <memory>


#include "FileHeader.h"
#include "ByteBuffer.h"
#include "ByteOrder.h"
#include "EvioNode.h"
#include "RecordOutput.h"
#include "RecordHeader.h"
#include "Compressor.h"
#include "Util.h"
#include "EvioException.h"


namespace evio {


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
        std::shared_ptr<RecordOutput> unusedRecord = nullptr;

        // If writing to buffer ...

        /** Buffer being written to. */
        std::shared_ptr<ByteBuffer> buffer;

        // For both files & buffers

        /** Buffer containing user Header. */
        std::shared_ptr<ByteBuffer> userHeaderBuffer = nullptr;
        /** Byte array containing user Header. */
        uint8_t* userHeader = nullptr;
        /** Size in bytes of userHeader array. */
        uint32_t userHeaderLength = 0;
        /** Evio format "first" event to store in file header's user header. */

        /** String containing evio-format XML dictionary to store in file header's user header. */
        std::string dictionary;
        /** If dictionary and or firstEvent exist, this buffer contains them both as a record. */
        std::shared_ptr<ByteBuffer> dictionaryFirstEventBuffer;
        /** Evio format "first" event to store in file header's user header. */
        uint8_t* firstEvent = nullptr;
        /** Length in bytes of firstEvent. */
        uint32_t firstEventLength = 0;

        /** Byte order of data to write to file/buffer. */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LOCAL};
        /** Record currently being filled. */
        std::shared_ptr<RecordOutput> outputRecord = nullptr;
        /** Record currently being written to file. */
        std::shared_ptr<RecordOutput> beingWrittenRecord = nullptr;
        /** Byte array large enough to hold a header/trailer. This array may increase. */
        std::vector<uint8_t> headerArray;

        /** Type of compression to use on file. Default is none. */
        Compressor::CompressionType compressionType {Compressor::UNCOMPRESSED};

        /** List of record lengths interspersed with record event counts
         * to be optionally written in trailer. */
        std::shared_ptr<std::vector<uint32_t>> recordLengths;

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

        Writer(const std::string & filename,
               const ByteOrder & order,
               uint32_t maxEventCount = 0,
               uint32_t maxBufferSize = 0);

        explicit Writer(const HeaderType & hType,
                        const ByteOrder & order = ByteOrder::ENDIAN_LOCAL,
                        uint32_t maxEventCount = 0,
                        uint32_t maxBufferSize = 0,
                        const std::string & dictionary = std::string(""),
                        uint8_t* firstEvent = nullptr,
                        uint32_t firstEventLength = 0,
                        const Compressor::CompressionType & compressionType = Compressor::UNCOMPRESSED,
                        bool addTrailerIndex = false);

        // Writing to buffer

        explicit Writer(std::shared_ptr<ByteBuffer> & buf);
        Writer(std::shared_ptr<ByteBuffer> & buf, uint8_t * userHdr, uint32_t len);
        Writer(std::shared_ptr<ByteBuffer> & buf, uint32_t maxEventCount, uint32_t maxBufferSize,
               const std::string & dictionary, uint8_t* firstEvent, uint32_t firstEventLength);

        ~Writer() = default;


//////////////////////////////////////////////////////////////////////

    private:

        //    Writer & operator=(Writer&& other) noexcept;
        // Don't allow assignment
        Writer & operator=(const Writer& other);

        std::shared_ptr<ByteBuffer> createDictionaryRecord();
        void writeOutput();
        void writeOutputToBuffer();

        static void staticWriteFunction(Writer *pWriter, const char* data, size_t len);

    public:

        const ByteOrder & getByteOrder() const;
        std::shared_ptr<ByteBuffer> getBuffer();
        FileHeader  & getFileHeader();
//    RecordHeader & getRecordHeader();
//    RecordOutput & getRecord();
        Compressor::CompressionType getCompressionType();
        void setCompressionType(Compressor::CompressionType compression);

        bool addTrailer() const;
        void addTrailer(bool add);
        bool addTrailerWithIndex();
        void addTrailerWithIndex(bool addTrailingIndex);

        void open(const std::string & filename);
        void open(const std::string & filename, uint8_t* userHdr, uint32_t len);
        void open(std::shared_ptr<ByteBuffer> & buf,  uint8_t* userHdr, uint32_t len);

        static std::shared_ptr<ByteBuffer> createRecord(const std::string & dictionary,
                                                        uint8_t* firstEvent, uint32_t firstEventLen,
                                                        const ByteOrder & byteOrder,
                                                        FileHeader* fileHeader,
                                                        RecordHeader* recordHeader);

        std::shared_ptr<ByteBuffer> createHeader(uint8_t* userHdr, uint32_t userLen);
        std::shared_ptr<ByteBuffer> createHeader(ByteBuffer & userHdr);
        void createHeader(ByteBuffer & buf, uint8_t* userHdr, uint32_t userLen);
        void createHeader(ByteBuffer & buf, ByteBuffer & userHdr);

        void writeRecord(RecordOutput & record);

        // Use internal RecordOutput to write individual events

        void addEvent(uint8_t* buffer, uint32_t length);
        void addEvent(std::shared_ptr<ByteBuffer> & buffer);
        void addEvent(ByteBuffer & buffer);
        void addEvent(std::shared_ptr<EvioBank> & bank);
        void addEvent(std::shared_ptr<EvioNode> & node);
        void addEvent(EvioNode & node);

        void reset();
        void close();

    private:

        void writeTrailer(bool writeIndex, uint32_t recordNum, uint64_t trailerPos);

    };

}


#endif //EVIO_6_0_WRITER_H
