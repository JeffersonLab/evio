//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_EVENTWRITER_H
#define EVIO_6_0_EVENTWRITER_H


#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
//#include <queue>
#include <chrono>
#include <memory>
#include <bitset>
#include <exception>
#include <atomic>
#include <algorithm>
#include <future>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include <boost/filesystem.hpp>


#ifdef USE_FILESYSTEMLIB
    #include <filesystem>
    //namespace fs = std::experimental::filesystem;
#else
    #include <boost/filesystem.hpp>
#endif


//#include "HeaderType.h"
#include "FileHeader.h"
#include "ByteBuffer.h"
#include "ByteOrder.h"
#include "RecordOutput.h"
//#include "RecordHeader.h"
#include "Compressor.h"
#include "RecordSupply.h"
#include "RecordCompressor.h"
//#include "Util.h"
#include "EvioException.h"
#include "EvioBank.h"
#include "FileWritingSupport.h"


//#include "Disruptor/Util.h"
#include <boost/thread.hpp>
#include <boost/chrono.hpp>



namespace evio {

    /**
     * An EventWriter object is used for writing events to a file or to a byte buffer.
     * This class does NOT write versions 1-4 data, only version 6!
     * This class is not thread-safe.
     *
     * <pre><code>
     *
     *            FILE Uncompressed
     *
     *    +----------------------------------+
     *    +                                  +
     *    +      General File Header         +
     *    +                                  +
     *    +----------------------------------+
     *    +----------------------------------+
     *    +                                  +
     *    +     Index Array (optional)       +
     *    +                                  +
     *    +----------------------------------+
     *    +----------------------------------+
     *    +      User Header (optional)      +
     *    +        --------------------------+
     *    +       |        Padding           +
     *    +----------------------------------+
     *    +----------------------------------+
     *    +                                  +
     *    +          Data Record 1           +
     *    +                                  +
     *    +----------------------------------+
     *                    ___
     *                    ___
     *                    ___
     *    +----------------------------------+
     *    +                                  +
     *    +          Data Record N           +
     *    +                                  +
     *    +----------------------------------+
     *
     * =============================================
     * =============================================
     *
     *              FILE Compressed
     *
     *    +----------------------------------+
     *    +                                  +
     *    +      General File Header         +
     *    +                                  +
     *    +----------------------------------+
     *    +----------------------------------+
     *    +                                  +
     *    +     Index Array (optional)       +
     *    +                                  +
     *    +----------------------------------+
     *    +----------------------------------+
     *    +      User Header (optional)      +
     *    +        --------------------------+
     *    +       |         Padding          +
     *    +----------------------------------+
     *    +----------------------------------+
     *    +           Compressed             +
     *    +          Data Record 1           +
     *    +                                  +
     *    +----------------------------------+
     *                    ___
     *                    ___
     *                    ___
     *    +----------------------------------+
     *    +           Compressed             +
     *    +          Data Record N           +
     *    +                                  +
     *    +----------------------------------+
     *
     *    The User Header contains a data record which
     *    holds the dictionary and first event, if any.
     *    The general file header, index array, and
     *    user header are never compressed.
     *
     *    Writing a buffer is done without the general file header
     *    and the index array and user header which follow.
     *
     *
     * </code></pre>
     *
     * @date 01/21/2020
     * @author timmer
     */
    class EventWriter {

    private:


        /**
         * Class used to take data-filled records from a RingBuffer-backed
         * RecordSupply, and writes them to file.
         * It is an interruptible thread from the boost library, and only 1 exists.
         */
        class RecordWriter {

        private:

            /** Object which owns this thread. */
            EventWriter * writer = nullptr;
            /** Supply of RecordRingItems. */
            std::shared_ptr<RecordSupply> supply;
            /** Thread which does the file writing. */
            boost::thread thd;
            /** The highest sequence to have been currently processed. */
            std::atomic_long lastSeqProcessed{-1};

            /** Place to store event when disk is full. */
            std::shared_ptr<RecordRingItem> storedItem;
            /** Force write to disk. */
            std::atomic_bool forceToDisk{false};
            /** Id of RecordRingItem that initiated the forced write. */
            std::atomic<std::uint64_t> forcedRecordId{0};

        public:

            /**
             * Constructor.
             * @param pwriter pointer to EventWriter object which owns this thread.
             * @param recordSupply shared pointer to an object supplying compressed records that need to be written to file.
             */
            RecordWriter(EventWriter * pwriter, std::shared_ptr<RecordSupply> recordSupply) :
                    writer(pwriter), supply(recordSupply)  {
            }

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
                try {
                    stopThread();
                }
                catch (const std::exception& e) {
                    std::cerr << "Exception during thread cleanup: " << e.what() << std::endl;
                }
            }

            /** Create and start a thread to execute the run() method of this class. */
            void startThread() {
                thd = boost::thread([this]() {this->run();});
            }

            /** Stop the thread. */
            void stopThread() {
                if (thd.joinable()) {
                    // Send signal to interrupt it
                    thd.interrupt();

                    // Wait for it to stop
                    if (thd.try_join_for(boost::chrono::milliseconds(1))) {
                        //std::cout << "RecordWriter JOINED from interrupt" << std::endl;
                        return;
                    }

                    // If that didn't work, send Alert signal to ring
                    supply->errorAlert();

                    if (thd.joinable()) {
                        thd.join();
                        //std::cout << "RecordWriter JOINED from alert" << std::endl;
                    }
                }
            }

            /** Wait for the last item to be processed, then exit thread. */
            void waitForLastItem() {
                try {
                    //cout << "WRITE: supply last = " << supply->getLastSequence() << ", lasSeqProcessed = " << lastSeqProcessed <<
                    //" supply->getLast > lastSeq = " <<  (supply->getLastSequence() > lastSeqProcessed)  <<  endl;
                    while (supply->getLastSequence() > lastSeqProcessed.load()) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }

                    // Stop this thread, not the calling thread
                    stopThread();
                }
                catch (Disruptor::AlertException & e) {
                    // Woken up in getToWrite through user call to supply.errorAlert()
                    //std::cout << "RecordWriter: quit thread through alert" << std::endl;
                }
            }

            /**
             * Store the id of the record which is forcing a write to disk,
             * even if disk is "full".
             * The idea is that we look for this record and once it has been
             * written, then we don't force any following records to disk
             * (unless we're told to again by the calling of this function).
             * Generally, for an emu, this method gets called when control events
             * arrive. In particular, when the END event comes, it must be written
             * to disk with all the events that preceded it.
             *
             * @param id id of record causing the forced write to disk.
             */
            void setForcedRecordId(uint64_t id) {
                forcedRecordId = id;
                forceToDisk = true;
            }

            std::shared_ptr<RecordRingItem> storeRecordCopy(std::shared_ptr<RecordRingItem> rec) {
                // Call copy constructor of RecordRingItem, then make into shared pointer
                storedItem = std::make_shared<RecordRingItem>(*(rec.get()));
                return storedItem;
            }

            /** Run this method in thread. */
            void run() {

                try {
                    while (true) {
                        // Get the next record for this thread to write
                        // shared_ptr<RecordRingItem>
                        auto item = supply->getToWrite();

                        {
                            // Only allow interruption when blocked on trying to get item
                            boost::this_thread::disable_interruption d1;

                            int64_t currentSeq = item->getSequence();
                            // Pull record out of wrapping object
                            //auto record = item->getRecord();

                            // Only need to check the disk when writing the first record following
                            // a file split. That first write will create the file. If there isn't
                            // enough room, then flag will be set.
                            bool checkDisk = item->isCheckDisk();

                            // Check the disk before we try to write if about to create another file,
                            // we're told to check the disk, and we're not forcing to disk
                            if ((writer->bytesWritten < 1) && checkDisk && (!forceToDisk.load())) {

                                // If there isn't enough free space to write the complete, projected
                                // size file, and we're not trying to force the write ...
                                // store a COPY of the record for now and release the original so
                                // that writeEventToFile() does not block.
                                //
                                // So here is the problem. We are stuck in a loop here if disk is full.
                                // If events are flowing and since writing data to file is the bottleneck,
                                // it is likely that all records have been filled and published onto
                                // the ring. AND, writeEventToFile() blocks in a spin as it tries to get the
                                // next record from the ring which, unfortunately, never comes.
                                //
                                // When writeEventToFile() blocks, it can't respond by returning a "false" value.
                                // This plays havoc with code like the emu which is not expecting the write
                                // to block (at least not for very long).
                                //
                                // To break writeEventToFile() out of its spinning block, we make a copy of the
                                // item we're trying to write and release the original record. This allows
                                // writeEventToFile() to grab a new (newly released) record, write an event into
                                // it, and return to the caller. From then on, writeEventToFile() can prevent
                                // blocking by checking for a full disk (which it couldn't do before since
                                // the signal came too late).

                                while (writer->fullDisk() && (!forceToDisk.load())) {
                                    // Wait for a sec and try again
                                    std::this_thread::sleep_for(std::chrono::seconds(1));

                                    // If we released the item in a previous loop, don't do it again
                                    if (!item->isAlreadyReleased()) {
                                        // Copy item
                                        auto copiedItem = storeRecordCopy(item);
                                        // Release original so we don't block writeEvent()
                                        supply->releaseWriter(item);
                                        item = copiedItem;
                                        item->setAlreadyReleased(true);
                                    }

                                    // Wait until space opens up
                                }

                                // If we're here, there must be free space available even
                                // if there previously wasn't.
                            }

                            // Do write
                            // Write current item to file
                            //cout << "EventWriter: Calling writeToFileMT(item)\n";
                            writer->writeToFileMT(item, forceToDisk.load());

                            // Turn off forced write to disk, if the record which
                            // initially triggered it, has now been written.
                            if (forceToDisk.load() && (forcedRecordId.load() == item->getId())) {
                                //cout << "EventWriter: WROTE the record that triggered force, reset to false\n";
                                forceToDisk = false;
                            }

                            // Now we're done with this sequence
                            lastSeqProcessed = currentSeq;

                            // Split file if needed
                            if (item->splitFileAfterWrite()) {
                                writer->splitFile();
                            }

                            // Release back to supply
                            supply->releaseWriter(item);
                        }
                    }
                }
                catch (Disruptor::AlertException & e) {
                    // Woken up in getToWrite through user call to supply.errorAlert()
                    //std::cout << "RecordWriter: quit thread through alert" << std::endl;
                }
                catch (boost::thread_interrupted & e) {
                    // Interrupted while blocked in getToWrite which means we're all done
                    //std::cout << "RecordWriter: quit thread through interrupt" << std::endl;
                }
                catch (std::runtime_error & e) {
                    std::string err = e.what();
                    supply->haveError(true);
                    supply->setError(err);
                }
            }
        };


        //-------------------------------------------------------------------------------------


    private:

        /** Dictionary and first event are stored in user header part of file header.
         *  They're written as a record which allows multiple events. */
        std::shared_ptr<RecordOutput> commonRecord;

        /** Record currently being filled. */
        std::shared_ptr<RecordOutput> currentRecord;

        /** Record supply item from which current record comes from. */
        std::shared_ptr<RecordRingItem> currentRingItem;

        /** Fast supply of record items for filling, compressing and writing. */
        std::shared_ptr<RecordSupply> supply;

        /** Max number of bytes held by all records in the supply. */
        uint32_t maxSupplyBytes = 0;

        /** Type of compression being done on data
         *  (0=none, 1=LZ4fastest, 2=LZ4best, 3=gzip). */
        Compressor::CompressionType compressionType{Compressor::UNCOMPRESSED};

        /** The estimated ratio of compressed to uncompressed data.
         *  (Used to figure out when to split a file). Percentage of original size. */
        uint32_t compressionFactor;

        /** List of record length followed by count to be optionally written in trailer.
         *  Easiest to make this a shared pointer since it gets passed as a method arg. */
        std::shared_ptr<std::vector<uint32_t>> recordLengths;

        /** Number of uncompressed bytes written to the current file/buffer at the moment,
         * including ending header and NOT the total in all split files. */
        //TODO: DOES THIS NEED TO BE ATOMIC IF MT write????????????????????????????????
        size_t bytesWritten = 0ULL;

        /** Do we add a last header or trailer to file/buffer? */
        bool addingTrailer = true;

        /** Do we add a record index to the trailer? */
        bool addTrailerIndex = false;

        /** Byte array large enough to hold a header/trailer. */
        std::vector<uint8_t> headerArray;

        /** Threads used to compress data. */
        std::vector<RecordCompressor> recordCompressorThreads;

        /** Thread used to write data to file/buffer.
         *  Easier to use vector here so we don't have to construct it immediately. */
        std::vector<EventWriter::RecordWriter> recordWriterThread;

        /** Number of records written to split-file/buffer at current moment. */
        uint32_t recordsWritten = 0;

        /** Running count of the record number. The next one to use starting with 1.
         *  Current value is generally for the next record. */
        uint32_t recordNumber = 1;

        /**
         * Dictionary to include in xml format in the first event of the first record
         * when writing the file.
         */
        std::string xmlDictionary;

        /** Byte array containing dictionary in evio format but <b>without</b> record header. */
        std::vector<uint8_t> dictionaryByteArray;

        /** Byte array containing firstEvent in evio format but <b>without</b> record header. */
        std::vector<uint8_t> firstEventByteArray;

        /** <code>True</code> if we have a "first event" to be written, else <code>false</code>. */
        bool haveFirstEvent = false;

        /** <code>True</code> if {@link #close()} was called, else <code>false</code>. */
        bool closed = false;

        /** <code>True</code> if writing to file, else <code>false</code>. */
        bool toFile = false;

        /** <code>True</code> if appending to file, <code>false</code> if (over)writing. */
        bool append = false;

        /** <code>True</code> if appending to file/buffer with dictionary, <code>false</code>. */
        bool hasAppendDictionary = false;

        /**
         * Total number of events written to buffer or file (although may not be flushed yet).
         * Will be the same as eventsWrittenToBuffer (- dictionary) if writing to buffer.
         * If the file being written to is split, this value refers to all split files
         * taken together. Does NOT include dictionary(ies).
         */
        uint32_t eventsWrittenTotal = 0;

        /** Byte order in which to write file or buffer. Initialize to local endian. */
        ByteOrder byteOrder {ByteOrder::ENDIAN_LOCAL};

        //-----------------------
        // Buffer related members
        //-----------------------

        /** CODA id of buffer sender. */
        uint32_t sourceId = 0;

        /** Total size of the buffer in bytes. */
        size_t bufferSize = 0;

        /**
         * The output buffer when writing to a buffer.
         * The buffer internal to the currentRecord when writing to a file
         * and which is a reference to one of the internalBuffers.
         * When dealing with files, this buffer does double duty and is
         * initially used to read in record headers before appending data
         * to an existing file and such.
         */
        std::shared_ptr<ByteBuffer> buffer;

        /** Buffer last used in the future1 write. */
        std::shared_ptr<ByteBuffer> usedBuffer;

        /** Three internal buffers used for writing to a file. */
        std::vector<std::shared_ptr<ByteBuffer>> internalBuffers;

        /** Number of bytes written to the current buffer for the common record. */
        uint32_t commonRecordBytesToBuffer = 0;

        /** Number of events written to final destination buffer or file's current record
         * NOT including dictionary (& first event?). */
        uint32_t eventsWrittenToBuffer = 0;

        //-----------------------
        // File related members
        //-----------------------

        /** Total size of the internal buffers in bytes. */
        size_t internalBufSize;

        /** Variable used to stop accepting events to be included in the inner buffer
         *  holding the current block. Used when disk space is inadequate. */
        bool diskIsFull = false;

        /** Variable used to stop accepting events to be included in the inner buffer
         *  holding the current block. Used when disk space is inadequate.
         *  This is atomic and therefore works between threads. */
        std::atomic_bool diskIsFullVolatile{false};

        bool fileOpen = false;

        /** When forcing events to disk, this identifies which events for the writing thread. */
        uint64_t idCounter = 0ULL;

        /** Header for file only. */
        FileHeader fileHeader;

        /** Header of file being appended to. */
        FileHeader appendFileHeader;

        /** File currently being written to. */
        std::string currentFileName;

#ifdef USE_FILESYSTEMLIB
        /** Path object corresponding to file currently being written. */
        fs::path currentFilePath;
#endif

        /** Object to allow efficient, asynchronous file writing. */
        std::shared_ptr<std::future<void>> future1;

        /** RingItem1 is associated with future1, etc. When a write is finished,
         * the associated ring item need to be released - but not before! */
        std::shared_ptr<RecordRingItem> ringItem1;

        /** The asynchronous file channel, used for writing a file. */
        std::shared_ptr<std::fstream> asyncFileChannel = nullptr;

        /** The location of the next write in the file. */
        uint64_t fileWritingPosition = 0ULL;

        /** Split number associated with output file to be written next. */
        uint32_t splitNumber = 0;

        /** Number of split files produced by this writer. */
        uint32_t splitCount = 0;

        /** Part of filename without run or split numbers. */
        std::string baseFileName;

        /** Number of C-style int format specifiers contained in baseFileName. */
        uint32_t specifierCount = 0;

        /** Run number possibly used in naming split files. */
        uint32_t runNumber = 0;

        /**
         * Do we split the file into several smaller ones (val > 0)?
         * If so, this gives the maximum number of bytes to make each file in size.
         */
        uint64_t split = 0ULL;

        /**
         * If splitting file, the amount to increment the split number each time another
         * file is written.
         */
        uint32_t splitIncrement = 0;

        /** Track bytes written to help split a file. */
        uint64_t splitEventBytes = 0ULL;

        /** Track events written to help split a file. */
        uint32_t splitEventCount = 0;

        /**
         * Id of this specific data stream.
         * In CODA, a data stream is a chain of ROCS and EBs ending in a single specific ER.
         */
        uint32_t streamId = 0;

        /** The total number of data streams in DAQ. */
        uint32_t streamCount = 1;

        /** Writing to file with single thread? */
        bool singleThreadedCompression = false;

        /** Is it OK to overwrite a previously existing file? */
        bool overWriteOK = false;

        /** Number of events actually written to the current file - not the total in
         * all split files - including dictionary. */
        uint32_t eventsWrittenToFile = 0;

        /** Does file have a trailer with record indexes? */
        bool hasTrailerWithIndex = false;

        /** File header's user header length in bytes. */
        uint32_t userHeaderLength = 0;

        /** File header's user header's padding in bytes. */
        uint32_t userHeaderPadding = 0;

        /** File header's index array length in bytes. */
        uint32_t indexLength = 0;

        /** Object used to close files in a separate thread when splitting
         *  so as to allow writing speed not to dip so low. */
        std::shared_ptr<FileCloser> fileCloser;

        //-----------------------
        /**
         * Flag to do everything except the actual writing of data to file.
         * Set true for testing purposes ONLY.
         */
        bool noFileWriting = false;
        //-----------------------


    public:


        explicit EventWriter(std::string & filename,
                             const ByteOrder & byteOrder = ByteOrder::nativeOrder(),
                             bool append = false);

        EventWriter(std::string & filename,
                    std::string & dictionary,
                    const ByteOrder & byteOrder = ByteOrder::nativeOrder(),
                    bool append = false);

        EventWriter(std::string & baseName, const std::string & directory, const std::string & runType,
                    uint32_t runNumber = 1, uint64_t split = 0,
                    uint32_t maxRecordSize = 4194304, uint32_t maxEventCount = 10000,
                    const ByteOrder & byteOrder = ByteOrder::nativeOrder(),
                    const std::string & xmlDictionary = "",
                    bool overWriteOK = true, bool append = false,
                    std::shared_ptr<EvioBank> firstEvent = nullptr,
                    uint32_t streamId = 1, uint32_t splitNumber = 0,
                    uint32_t splitIncrement = 1, uint32_t streamCount = 1,
                    Compressor::CompressionType compressionType = Compressor::UNCOMPRESSED,
                    uint32_t compressionThreads = 1, uint32_t ringSize = 0,
                    size_t bufferSize = 32100000);


        //---------------------------------------------
        // BUFFER Constructors
        //---------------------------------------------

        explicit EventWriter(std::shared_ptr<ByteBuffer> buf, const std::string & xmlDictionary = "");
        EventWriter(std::shared_ptr<ByteBuffer> buf, uint32_t maxRecordSize, uint32_t maxEventCount,
                    const std::string & xmlDictionary, uint32_t recordNumber,
                    Compressor::CompressionType compressionType);
        EventWriter(std::shared_ptr<ByteBuffer> buf, uint32_t maxRecordSize, uint32_t maxEventCount,
                    const std::string & xmlDictionary, uint32_t recordNumber,
                    Compressor::CompressionType compressionType, int eventType);

    private:

        void reInitializeBuffer(std::shared_ptr<ByteBuffer> buf, const std::bitset<24> *bitInfo,
                                uint32_t recordNumber, bool useCurrentBitInfo);

        static void staticWriteFunction(EventWriter *pWriter, const char* data, size_t len);
        static void staticDoNothingFunction(EventWriter *pWriter);

    public:

        bool isDiskFull();
        void setBuffer(std::shared_ptr<ByteBuffer> buf, std::bitset<24> *bitInfo, uint32_t recNumber);
        void setBuffer(std::shared_ptr<ByteBuffer> buf);

    private:

        std::shared_ptr<ByteBuffer> getBuffer();
        void expandInternalBuffers(size_t bytes);


    public:

        std::shared_ptr<ByteBuffer> getByteBuffer();
        void setSourceId(int sId);
        void setEventType(int type);

        bool writingToFile() const;
        bool isClosed() const;

        std::string getCurrentFilename() const;
        size_t getBytesWrittenToBuffer() const;
        std::string getCurrentFilePath() const;
        uint32_t getSplitNumber() const;
        uint32_t getSplitCount() const;
        uint32_t getRecordNumber() const;
        uint32_t getEventsWritten() const;
        ByteOrder getByteOrder() const;

        void setStartingRecordNumber(uint32_t startingRecordNumber);

        void setFirstEvent(std::shared_ptr<EvioNode> node);
        void setFirstEvent(std::shared_ptr<ByteBuffer> buf);
        void setFirstEvent(std::shared_ptr<EvioBank> bank);

    private:

        void createCommonRecord(const std::string & xmlDict,
                                std::shared_ptr<EvioBank> firstBank,
                                std::shared_ptr<EvioNode> firstNode,
                                std::shared_ptr<ByteBuffer> firstBuf);

        void writeFileHeader() ;


    public :

        void flush();
        void close();

    protected:

        void examineFileHeader();

    private:

        void toAppendPosition();

    public:

        bool hasRoom(uint32_t bytes);

        bool writeEvent(std::shared_ptr<EvioNode> node, bool force = false,
                        bool duplicate = true, bool ownRecord = false);

        bool writeEventToFile(std::shared_ptr<EvioNode> node, bool force = false,
                              bool duplicate = true, bool ownRecord = false);

        bool writeEventToFile(std::shared_ptr<ByteBuffer> bb, bool force = false,
                              bool duplicate = true, bool ownRecord = false);

        bool writeEvent(std::shared_ptr<ByteBuffer> bankBuffer, bool force = false , bool ownRecord = false);

        bool writeEvent(std::shared_ptr<EvioBank> bank, bool force = false, bool ownRecord = false);

        bool writeEventToFile(std::shared_ptr<EvioBank> bank, bool force = false, bool ownRecord = false);

    private:

        bool writeEvent(std::shared_ptr<EvioBank> bank,
                        std::shared_ptr<ByteBuffer> bankBuffer,
                        bool force, bool ownRecord);
        bool writeEventToFile(std::shared_ptr<EvioBank> bank,
                              std::shared_ptr<ByteBuffer> bankBuffer,
                              bool force, bool ownRecord);

        bool fullDisk();

        void compressAndWriteToFile(bool force);
        bool tryCompressAndWriteToFile(bool force);

        bool writeToFile(bool force, bool checkDisk);
        void writeToFileMT(std::shared_ptr<RecordRingItem> item, bool force);

        void splitFile();
        void writeTrailerToFile(bool writeIndex);
        void flushCurrentRecordToBuffer() ;
        bool writeToBuffer(std::shared_ptr<EvioBank> bank, std::shared_ptr<ByteBuffer> bankBuffer) ;

        uint32_t trailerBytes();
        void writeTrailerToBuffer(bool writeIndex);

    };

}


#endif //EVIO_6_0_EVENTWRITER_H
