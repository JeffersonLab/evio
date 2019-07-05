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

#include "FileHeader.h"
#include "ByteBuffer.h"
#include "ByteOrder.h"
#include "RecordOutput.h"
#include "RecordHeader.h"
#include "ConcurrentFixedQueue.h"
#include "Compressor.h"
#include "Stoppable.h"
#include "Writer.h"


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
     * Class used to create a thread which takes data-filled records from a queue,
     * compresses them, and writes them to an output queue.
     * Make it a "stoppable" thread.
     */
    class CompressingThread : Stoppable {

        private:

            /** Keep track of this thread with id number. */
            uint32_t threadNumber;
            /** Record number to set in header. */
            uint32_t recordNumber;
            /** Type of compression to perform. */
            Compressor::CompressionType compressionType;
            /** Queue containing uncompressed input records. */
            ConcurrentFixedQueue<RecordOutput> & queueIn;
            /** Queue on which to place compressed records. */
            ConcurrentFixedQueue<RecordOutput> & queueOut;
            /** Thread which does the compression. */
            std::thread thd;

        public:

             /**
              * Constructor.
              * @param thdNum unique thread number starting at 0.
              * @param recNum
              * @param type
              * @param recordQ
              */
            CompressingThread(uint32_t thdNum, uint32_t recNum, Compressor::CompressionType type,
                             ConcurrentFixedQueue<RecordOutput> & qIn,
                             ConcurrentFixedQueue<RecordOutput> & qOut) :
                                threadNumber(thdNum),
                                recordNumber(recNum),
                                compressionType(type),
                                queueIn(qIn),
                                queueOut(qOut)  {};

            /** Create and start a thread to execute the run() method of this class. */
            void startThread() {
                thd = std::thread([&]() {this->run();});
            }

            /** Stop the thread. */
            void stopThread() {
                // Send signal to stop it
                this->stop();
                // Wait for it to stop
                thd.join();
            }

            /** Method to run in a thread. */
            void run() override {
                while (true) {
                    if (stopRequested()) {
                        return;
                    }
                    cout << "   Compressor: try getting record to compress" << endl;

                    // Get the next record for this thread to compress
                    RecordOutput record;
                    // Wait up to 1 sec, then try again
                    bool gotOne = queueIn.waitPop(record, 1000);
                    if (!gotOne) continue;

                    // Set compression type and record #
                    RecordHeader header = record.getHeader();

                    // Fortunately for us, the record # is also the sequence # + 1 !
                    header.setRecordNumber(recordNumber);
                    header.setCompressionType(compressionType);
                    cout << "   Compressor: set record # to " << header.getRecordNumber() << endl;

                    // Do compression
                    record.build();
                    cout << "   Compressor: got record, header = \n" << header.toString() << endl;

                    //cout << "   Compressor: pass item to writer thread" << endl;
                    // Pass on to writer thread
                    queueOut.push(record);
                }
            }
    };


    /**
     * Class used to take data-filled records from queues and write them to file.
     * Only one of these threads exist which makes tracking indexes and lengths easy.
     */
    class WritingThread : Stoppable {

        private:

            /** Vector of input queues with (possibly) compressed records. */
            vector<ConcurrentFixedQueue<RecordOutput>> queues;
            /** Object which owns this thread. */
            WriterMT * writer;
            /** Thread which does the file writing. */
            std::thread thd;

        public:

            /**
             * Default constructor.
             * @param threadNumber unique thread number starting at 0.
             */
            WritingThread() : writer(nullptr), queues(vector<ConcurrentFixedQueue<RecordOutput>>()) {}

            /**
             * Constructor.
             * @param pWriter pointer to WriterMT object which owns this thread.
             * @param qs vector of input queues containing compressed records that need to be written to file.
             *           Each queue is being filled by a CompressionThread.
             */
            WritingThread(WriterMT * pwriter, vector<ConcurrentFixedQueue<RecordOutput>> & qs) :
                    writer(pwriter), queues(qs)  {}

            /** Create and start a thread to execute the run() method of this class. */
            void startThread() {
                thd = std::thread([&]() {this->run();});
            }

            /** Stop the thread. */
            void stopThread() {
                // Send signal to stop it
                this->stop();
                // Wait for it to stop
                thd.join();
            }

            /** Wait for the last item to be processed, then exit thread. */
            void waitForLastItem() {
                for (auto & queue : queues) {
                    while (!queue.isEmpty()) {
                        //std::this_thread::yield();
                        std::this_thread::sleep_for(chrono::milliseconds(1));
                    }
                }

                // Stop this thread, not the calling thread
                this->stopThread();
            }

            /** Run this method in thread. */
            void run() override {
                //long currentSeq;

                while (true) {
                    if (stopRequested()) {
                        return;
                    }
                    cout << "   Writer: try getting record to write" << endl;

                    // Get the next record for this thread to compress
                    RecordOutput record;

                    // Go round robin through the queues to keep events in order
                    for (auto & queue : queues) {
                        bool gotOne = false;
                        while (!gotOne) {
                            if (stopRequested()) {
                                return;
                            }
                            // Check every 1 sec for something to write from this q
                            gotOne = queue.waitPop(record, 1000);
                        }

                        //cout << "   Writer: try getting record to write" << endl;
                        //currentSeq = item.getSequence();

                        // Do write
                        RecordHeader header = record.getHeader();

                        cout << "   Writer: got record, header = \n" << header.toString() << endl;
                        int bytesToWrite = header.getLength();

                        // Record length of this record
                        writer->recordLengths.push_back(bytesToWrite);
                        // Followed by events in record
                        writer->recordLengths.push_back(header.getEntries());
                        writer->writerBytesWritten += bytesToWrite;

                        ByteBuffer buf = record.getBinaryBuffer();
                        cout << "   Writer: use outStream to write file, buf pos = " << buf.position() <<
                             ", lim = " << buf.limit() << ", bytesToWrite = " << bytesToWrite << endl;
                        writer->outFile.write(reinterpret_cast<const char *>(buf.array()), bytesToWrite);

                        //TODO: Check for errors in write
                        if (writer->outFile.fail()) {
                            throw HipoException("failed write to file");
                        }

                        record.reset();

                        // Release back to supply
                        //cout << "   Writer: release ring item back to supply" << endl;
                        //supply.releaseWriter(item);

                        // Now we're done with this sequence
                        //lastSeqProcessed = currentSeq;
                    }
                }
            }
    };


private:

    /** Object for writing file. */
    std::ofstream outFile;
    /** Header to write to file, created in constructor. */
    FileHeader fileHeader;

    /** Buffer containing user Header. */
    ByteBuffer userHeaderBuffer;
    /** Byte array containing user Header. */
    uint8_t* userHeader;
    /** Size in bytes of userHeader array. */
    uint32_t userHeaderLength;

    /** Has the first record been written already? */
    bool firstRecordWritten;

    // For both files & buffers

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
    /** Max number of events an internal record can hold. */
    uint32_t maxEventCount;
    /** Max number of uncompressed data bytes an internal record can hold. */
    uint32_t maxBufferSize;

    /** Internal Record. */
    RecordOutput outputRecord;

    /** Byte array large enough to hold a header/trailer. This array may increase. */
    vector<uint8_t> headerArray;

    /** Type of compression to use on file. Default is none. */
    Compressor::CompressionType compressionType = Compressor::UNCOMPRESSED;

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

    /** List of record lengths interspersed with record event counts
     * to be optionally written in trailer. */
    //ArrayList<Integer> recordLengths = new ArrayList<Integer>(1500);
    vector<uint32_t> recordLengths;

    /** Number of threads doing compression simultaneously. */
    uint32_t compressionThreadCount;

    /** Next queue in which a record to be compressed will be placed. */
    uint32_t nextQueueIndex = 0;

    /** Takes the place of disruptor ring buffers used in Java. */
    ConcurrentFixedQueue<RecordOutput> writeQueue;
    vector<ConcurrentFixedQueue<RecordOutput>> queues;

    /** Thread used to write data to file/buffer. */
    WritingThread recordWriterThread;

    /** Threads used to compress data. */
    vector<CompressingThread> recordCompressorThreads;

public:

    WriterMT();

    WriterMT(const ByteOrder  & order, uint32_t maxEventCount, uint32_t maxBufferSize,
             Compressor::CompressionType compType, uint32_t compressionThreads);

    WriterMT(const HeaderType & hType, const ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize,
             Compressor::CompressionType compressionType, uint32_t compressionThreads,
             const string & dictionary, uint8_t* firstEvent, uint32_t firstEventLen);

    explicit WriterMT(string & filename);

    WriterMT(string & filename, const ByteOrder & order, uint32_t maxEventCount, uint32_t maxBufferSize,
             Compressor::CompressionType compressionType, uint32_t compressionThreads);

    ~WriterMT() = default;

//////////////////////////////////////////////////////////////////////

private:

    ByteBuffer createDictionaryRecord();
    void writeInternalRecord(RecordOutput & internalRecord);

    // TODO: this should be part of an Utilities class ...
    static void toBytes(uint32_t data, const ByteOrder & byteOrder,
                        uint8_t* dest, uint32_t off, uint32_t destMaxSize);

public:

    const ByteOrder & getByteOrder() const;
    //ByteBuffer   & getBuffer();
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

    WriterMT & setCompressionType(Compressor::CompressionType compression);

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
