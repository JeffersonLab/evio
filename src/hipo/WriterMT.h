//
// Created by Carl Timmer on 2019-05-13.
//

#ifndef EVIO_6_0_WRITERMT_H
#define EVIO_6_0_WRITERMT_H


#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <queue>
#include <chrono>
#include <memory>

#include "FileHeader.h"
#include "ByteBuffer.h"
#include "ByteOrder.h"
#include "RecordOutput.h"
#include "RecordHeader.h"
#include "ConcurrentFixedQueue.h"
#include "Compressor.h"
#include "Writer.h"
#include "RecordSupply.h"

#include "Disruptor/Util.h"
#include <boost/thread.hpp>
#include <boost/chrono.hpp>

/**
 * Class to write Evio/HIPO files only (not to buffers).
 * Able to multithread the compression of data.
 *
 * @version 6.0
 * @since 6.0 5/13/19
 * @author timmer
 */

class WriterMT {


private:

    /**
     * Class used to create a thread which takes data-filled records from a RingBuffer-backed
     * RecordSupply, compresses them, and places them back into the supply.
     * It is an interruptible thread from the boost library.
     */
    class RecordCompressor {

    private:

        /** Keep track of this thread with id number. */
        uint32_t threadNumber;
        /** Type of compression to perform. */
        Compressor::CompressionType compressionType;
        /** Supply of RecordRingItems. */
        std::shared_ptr<RecordSupply> supply;
        /** Thread which does the compression. */
        boost::thread thd;

    public:

        /**
         * Constructor.
         * @param thdNum   unique thread number starting at 0.
         * @param type     type of compression to do.
         * @param qIn      input queue of records to compress.
         * @param qOut     output queue of compressed records.
         */
        RecordCompressor(uint32_t thdNum, Compressor::CompressionType & type,
                         std::shared_ptr<RecordSupply> & recordSupply) :
                threadNumber(thdNum),
                compressionType(type),
                supply(recordSupply) {
        }

        RecordCompressor(RecordCompressor && obj) noexcept :
                threadNumber(obj.threadNumber),
                compressionType(obj.compressionType),
                supply(std::move(obj.supply)),
                thd(std::move(obj.thd)) {
        }

        RecordCompressor & operator=(RecordCompressor && obj) noexcept {
            if (this != &obj) {
                threadNumber = obj.threadNumber;
                compressionType = obj.compressionType;
                supply = std::move(obj.supply);
                thd  = std::move(obj.thd);
            }
            return *this;
        }

        ~RecordCompressor() {
            thd.interrupt();
            if (thd.try_join_for(boost::chrono::milliseconds(500))) {
                cout << "RecordCompressor thread did not quit after 1/2 sec" << endl;
            }
        }

        /** Create and start a thread to execute the run() method of this class. */
        void startThread() {
            thd = boost::thread([this]() {this->run();});
        }

        /** Stop the thread. */
        void stopThread() {
            // Send signal to interrupt it
            thd.interrupt();
            // Wait for it to stop
            thd.join();
        }

        /** Method to run in the thread. */
        void run() {
            try {
                while (true) {
cout << "   Compressor " << threadNumber << ": try getting record to compress" << endl;

                    // Get the next record for this thread to compress
                    auto item = supply->getToCompress(threadNumber);

                    {
                        // Only allow interruption when blocked on trying to get item
                        boost::this_thread::disable_interruption d1;

                        // Pull record out of wrapping object
                        std::shared_ptr<RecordOutput> & record = item->getRecord();
                        // Set compression type and record #
                        RecordHeader &header = record->getHeader();
                        // Fortunately for us, the record # is also the sequence # + 1 !
                        header.setRecordNumber((uint32_t) (item->getSequence() + 1L));
                        header.setCompressionType(compressionType);
cout << "   Compressor " << threadNumber << ": got record, set rec # to " << header.getRecordNumber() << endl;
                        // Do compression
                        record->build();
                        const ByteBuffer & buf = record->getBinaryBuffer();
                        // Release back to supply
                        supply->releaseCompressor(item);
                    }
                }
            }
            catch (boost::thread_interrupted & e) {
                //cout << "   Compressor " << threadNumber << ": INTERRUPTED, return" << endl;
            }
        }
    };


    /**
     * Class used to take data-filled records from a RingBuffer-backed
     * RecordSupply, and writes them to file.
     * It is an interruptible thread from the boost library, and only 1 exists.
     */
    class RecordWriter {

    private:

        /** Object which owns this thread. */
        WriterMT * writer = nullptr;
        /** Supply of RecordRingItems. */
        std::shared_ptr<RecordSupply> supply;
        /** Thread which does the file writing. */
        boost::thread thd;
        /** The highest sequence to have been currently processed. */
        volatile long lastSeqProcessed = -1;

    public:

        /**
         * Constructor.
         * @param pWriter pointer to WriterMT object which owns this thread.
         * @param qs vector of input queues containing compressed records that need to be written to file.
         *           Each queue is being filled by a CompressionThread.
         */
        RecordWriter(WriterMT * pwriter, std::shared_ptr<RecordSupply> & recordSupply) :
                writer(pwriter), supply(recordSupply)  {
        }

        RecordWriter(RecordWriter && obj) noexcept :
                writer(obj.writer),
                lastSeqProcessed(obj.lastSeqProcessed),
                supply(std::move(obj.supply)),
                thd(std::move(obj.thd)) {
        }

        RecordWriter & operator=(RecordWriter && obj) noexcept {
            if (this != &obj) {
                writer = obj.writer;
                lastSeqProcessed = obj.lastSeqProcessed;
                supply = std::move(obj.supply);
                thd  = std::move(obj.thd);
            }
            return *this;
        }

        // Do not free writer!
        ~RecordWriter() {
            thd.interrupt();
            if (thd.try_join_for(boost::chrono::milliseconds(500))) {
                cout << "RecordWriter thread did not quit after 1/2 sec" << endl;
            }
        }

        /** Create and start a thread to execute the run() method of this class. */
        void startThread() {
            thd = boost::thread([this]() {this->run();});
        }

        /** Stop the thread. */
        void stopThread() {
            // Send signal to interrupt it
            thd.interrupt();
            // Wait for it to stop
            thd.join();
        }

        /** Wait for the last item to be processed, then exit thread. */
        void waitForLastItem() {
//cout << "WRITE: supply last = " << supply->getLastSequence() << ", lasSeqProcessed = " << lastSeqProcessed <<
//" supply->getLast > lastSeq = " <<  (supply->getLastSequence() > lastSeqProcessed)  <<  endl;
            while (supply->getLastSequence() > lastSeqProcessed) {
                std::this_thread::sleep_for(chrono::milliseconds(1));
            }

            // Stop this thread, not the calling thread
            stopThread();
        }

        /** Run this method in thread. */
        void run() {
            int64_t currentSeq;

            try {
                while (true) {

cout << "   RecordWriter: try getting record to write" << endl;
                    // Get the next record for this thread to write
                    auto item = supply->getToWrite();

                    {
                        // Only allow interruption when blocked on trying to get item
                        boost::this_thread::disable_interruption d1;

                        currentSeq = item->getSequence();
                        // Pull record out of wrapping object
                        std::shared_ptr<RecordOutput> & record = item->getRecord();

                        // Do write
                        RecordHeader header = record->getHeader();
                        int bytesToWrite = header.getLength();
                        // Record length of this record
                        writer->recordLengths.push_back(bytesToWrite);
                        // Followed by events in record
                        writer->recordLengths.push_back(header.getEntries());
                        writer->writerBytesWritten += bytesToWrite;

                        const ByteBuffer &buf = record->getBinaryBuffer();
 cout << "   RecordWriter: use outFile to write file, buf pos = " << buf.position() <<
                             ", lim = " << buf.limit() << ", bytesToWrite = " << bytesToWrite << endl;
                        writer->outFile.write(reinterpret_cast<const char *>(buf.array()), bytesToWrite);
                        if (writer->outFile.fail()) {
                            throw HipoException("failed write to file");
                        }

                        record->reset();

                        // Release back to supply
                        supply->releaseWriter(item);

                        // Now we're done with this sequence
                        lastSeqProcessed = currentSeq;
                    }
                }
            }
            catch (boost::thread_interrupted & e) {
                //cout << "   RecordWriter: INTERRUPTED, return" << endl;
            }
        }
    };



private:

    /** Number of bytes written to file/buffer at current moment. */
    size_t writerBytesWritten = 0ULL;
    /** Byte array containing user Header. */
    uint8_t* userHeader = nullptr;
    /** Size in bytes of userHeader array. */
    uint32_t userHeaderLength = 0;
    /** Evio format "first" event to store in file header's user header. */
    uint8_t* firstEvent = nullptr;
    /** Length in bytes of firstEvent. */
    uint32_t firstEventLength = 0;
    /** Max number of events an internal record can hold. */
    uint32_t maxEventCount = 0;
    /** Max number of uncompressed data bytes an internal record can hold. */
    uint32_t maxBufferSize = 0;
    /** Number which is incremented and stored with each successive written record starting at 1. */
    uint32_t recordNumber = 1;
    /** Number of threads doing compression simultaneously. */
    uint32_t compressionThreadCount = 1;

    /** Object for writing file. */
    std::ofstream outFile;

    /** Header to write to file, created in constructor. */
    FileHeader fileHeader;

//    /** Buffer containing user Header. */
//    ByteBuffer userHeaderBuffer;

    /** String containing evio-format XML dictionary to store in file header's user header. */
    string dictionary;

    /** If dictionary and or firstEvent exist, this buffer contains them both as a record. */
    ByteBuffer dictionaryFirstEventBuffer;

    /** Byte order of data to write to file/buffer. */
    ByteOrder byteOrder {ByteOrder::ENDIAN_LITTLE};

    /** Internal Record. */
    shared_ptr<RecordOutput> outputRecord;

    /** Byte array large enough to hold a header/trailer. This array may increase. */
    std::vector<uint8_t> headerArray;
    uint8_t headerArray2[RecordHeader::HEADER_SIZE_BYTES];

    /** Type of compression to use on file. Default is none. */
    Compressor::CompressionType compressionType {Compressor::UNCOMPRESSED};

    /** List of record lengths interspersed with record event counts
      * to be optionally written in trailer. */
    std::vector<uint32_t> recordLengths;

    /** Fast, thread-safe, lock-free supply of records. */
    std::shared_ptr<RecordSupply> supply;

    /** Vector to hold thread used to write data to file/buffer.
     *  Easier to use vector here so we don't have to construct it immediately. */
    std::vector<RecordWriter> recordWriterThreads;

    /** Threads used to compress data. */
    std::vector<RecordCompressor> recordCompressorThreads;

    /** Current ring Item from which current record is taken. */
    std::shared_ptr<RecordRingItem> ringItem;


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

    WriterMT();

    WriterMT(const ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize,
             Compressor::CompressionType compType, uint32_t compressionThreads);

    explicit WriterMT(
             const HeaderType & hType,
             const ByteOrder & order = ByteOrder::ENDIAN_LITTLE,
             uint32_t maxEventCount = 0,
             uint32_t maxBufferSize = 0,
             Compressor::CompressionType compressionType = Compressor::UNCOMPRESSED,
             uint32_t compressionThreads = 1,
             bool addTrailerIndex = false,
             const string & dictionary = "",
             uint8_t* firstEvent = nullptr,
             uint32_t firstEventLen = 0,
             uint32_t ringSize = 16);

    explicit WriterMT(string & filename);

    WriterMT(string & filename, const ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize,
             Compressor::CompressionType compressionType, uint32_t compressionThreads);

    ~WriterMT() = default;

//////////////////////////////////////////////////////////////////////

private:

    ByteBuffer createDictionaryRecord();

    // TODO: this should be part of an Utilities class ...
    static void toBytes(uint32_t data, const ByteOrder & byteOrder,
                        uint8_t* dest, uint32_t off, uint32_t destMaxSize);

public:

    const ByteOrder & getByteOrder() const;
//    ByteBuffer   & getBuffer();
    FileHeader   & getFileHeader();
//    RecordHeader & getRecordHeader();
//    RecordOutput & getRecord();
    Compressor::CompressionType getCompressionType();

    bool addTrailer() const;
    void addTrailer(bool add);
    bool addTrailerWithIndex();
    void addTrailerWithIndex(bool addTrailingIndex);

    void open(string & filename);
    void open(string & filename, uint8_t* userHdr, uint32_t userLen);

    ByteBuffer createHeader(uint8_t* userHdr, uint32_t userLen);
    ByteBuffer createHeader(ByteBuffer & userHdr);

    void writeTrailer(bool writeIndex);
    void writeRecord(RecordOutput & record);

    // Use internal RecordOutput to write individual events

    void addEvent(uint8_t* buffer, uint32_t offset, uint32_t length);
    void addEvent(ByteBuffer & buffer);
//    void addEvent(EvioBank & bank);
//    void addEvent(EvioNode & node);

    void reset();
    void close();

};


#endif //EVIO_6_0_WRITERMT_H
