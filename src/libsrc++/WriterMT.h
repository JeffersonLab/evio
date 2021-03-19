//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


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
#include <atomic>


#include "FileHeader.h"
#include "ByteBuffer.h"
#include "ByteOrder.h"
#include "RecordOutput.h"
#include "RecordHeader.h"
#include "Compressor.h"
#include "Writer.h"
#include "RecordSupply.h"
#include "RecordCompressor.h"
#include "Util.h"
#include "EvioException.h"


#include "Disruptor/Util.h"
#include <boost/thread.hpp>
#include <boost/chrono.hpp>


namespace evio {


   /**
    * This class is for writing Evio/HIPO files only (not buffers).
    * It's able to multithread the compression of data.<p>
    *
    * At the center of how this class works is an ultra-fast ring buffer containing
    * a store of empty records. As the user calls one of the {@link #addEvent} methods,
    * it gradually fills one of those empty records with data. When the record is full,
    * it's put back into the ring to wait for one of the compression thread to grab it and compress it.
    * After compression, it's again placed back into the ring and waits for a final thread to
    * write it to file. After being written, the record is freed up for reuse.
    * This entire ring functionality is encapsulated in 2 classes,
    * {@link RecordSupply} and {@link RecordRingItem}.
    *
    * @version 6.0
    * @since 6.0 5/13/19
    * @author timmer
    */
    class WriterMT {


    private:

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
            std::atomic_long lastSeqProcessed{-1};

        public:

            /**
             * Constructor.
             * @param pwriter pointer to WriterMT object which owns this thread.
             * @param recordSupply shared pointer to an object supplying compressed records that need to be written to file.
             */
            RecordWriter(WriterMT * pwriter, std::shared_ptr<RecordSupply> & recordSupply) :
                    writer(pwriter), supply(recordSupply)  {
            }

            /**
             * Move constructor.
             * @param obj object to move.
             */
            RecordWriter(RecordWriter && obj) noexcept :
                    writer(obj.writer),
                    supply(std::move(obj.supply)),
                    thd(std::move(obj.thd)) {

                lastSeqProcessed.store(obj.lastSeqProcessed);
            }

            RecordWriter & operator=(RecordWriter && obj) noexcept {
                if (this != &obj) {
                    writer = obj.writer;
                    lastSeqProcessed.store(obj.lastSeqProcessed);
                    supply = std::move(obj.supply);
                    thd  = std::move(obj.thd);
                }
                return *this;
            }

            // Do not free writer!
            ~RecordWriter() {
                thd.interrupt();
                if (thd.try_join_for(boost::chrono::milliseconds(500))) {
                    std::cout << "RecordWriter thread did not quit after 1/2 sec" << std::endl;
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
//std::cout << "WRITE: supply last = " << supply->getLastSequence() << ", lasSeqProcessed = " << lastSeqProcessed <<
//" supply->getLast > lastSeq = " <<  (supply->getLastSequence() > lastSeqProcessed)  <<  std::endl;
                while (supply->getLastSequence() > lastSeqProcessed.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                // Stop this thread, not the calling thread
                stopThread();
            }

            /** Run this method in thread. */
            void run() {
                int64_t currentSeq;

                try {
                    while (true) {

                        std::cout << "   RecordWriter: try getting record to write" << std::endl;
                        // Get the next record for this thread to write
                        auto item = supply->getToWrite();

                        {
                            // Only allow interruption when blocked on trying to get item
                            boost::this_thread::disable_interruption d1;

                            currentSeq = item->getSequence();
                            // Pull record out of wrapping object
                            std::shared_ptr<RecordOutput> & record = item->getRecord();

                            // Do write
                            auto & header = record->getHeader();
                            int bytesToWrite = header->getLength();
                            // Record length of this record
                            writer->recordLengths->push_back(bytesToWrite);
                            // Followed by events in record
                            writer->recordLengths->push_back(header->getEntries());
                            writer->writerBytesWritten += bytesToWrite;

                            auto buf = record->getBinaryBuffer();
                            std::cout << "   RecordWriter: use outFile to write file, buf pos = " << buf->position() <<
                                 ", lim = " << buf->limit() << ", bytesToWrite = " << bytesToWrite << std::endl;
                            writer->outFile.write(reinterpret_cast<const char *>(buf->array()), bytesToWrite);
                            if (writer->outFile.fail()) {
                                throw EvioException("failed write to file");
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

        /** File name. */
        std::string fileName = "";

        /** Object for writing file. */
        std::ofstream outFile;

        /** Header to write to file, created in constructor. */
        FileHeader fileHeader;

        /** String containing evio-format XML dictionary to store in file header's user header. */
        std::string dictionary;

        /** If dictionary and or firstEvent exist, this buffer contains them both as a record. */
        std::shared_ptr<ByteBuffer> dictionaryFirstEventBuffer;

        /** Byte order of data to write to file/buffer. */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LOCAL};

        /** Internal Record. */
        std::shared_ptr<RecordOutput> outputRecord;

        /** Byte array large enough to hold a header/trailer. This array may increase. */
        std::vector<uint8_t> headerArray;

        /** Type of compression to use on file. Default is none. */
        Compressor::CompressionType compressionType {Compressor::UNCOMPRESSED};

        /** List of record lengths interspersed with record event counts
          * to be optionally written in trailer. */
        std::shared_ptr<std::vector<uint32_t>> recordLengths;

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
                const std::string & dictionary = "",
                uint8_t* firstEvent = nullptr,
                uint32_t firstEventLen = 0,
                Compressor::CompressionType compressionType = Compressor::UNCOMPRESSED,
                uint32_t compressionThreads = 1,
                bool addTrailerIndex = false,
                uint32_t ringSize = 16);

        explicit WriterMT(const std::string & filename);

        WriterMT(const std::string & filename, const ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize,
                 Compressor::CompressionType compressionType, uint32_t compressionThreads);

        ~WriterMT() = default;

//////////////////////////////////////////////////////////////////////

    private:

        std::shared_ptr<ByteBuffer> createDictionaryRecord();
        void writeTrailer(bool writeIndex, uint32_t recordNum);

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

        void open(const std::string & filename);
        void open(const std::string & filename, uint8_t* userHdr, uint32_t userLen);

        std::shared_ptr<ByteBuffer> createHeader(uint8_t* userHdr, uint32_t userLen);
        std::shared_ptr<ByteBuffer> createHeader(ByteBuffer & userHdr);

        void writeRecord(RecordOutput & record);

        // Use internal RecordOutput to write individual events

        void addEvent(uint8_t* buffer, uint32_t offset, uint32_t length);
        void addEvent(ByteBuffer & buffer);
//    void addEvent(EvioBank & bank);
        void addEvent(EvioNode & node);

        void reset();
        void close();

    };

}


#endif //EVIO_6_0_WRITERMT_H
