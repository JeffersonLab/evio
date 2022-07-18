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


#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <queue>
#include <chrono>
#include <memory>
#include <bitset>
#include <exception>
#include <atomic>
#include <algorithm>
#include <future>
#include <sys/stat.h>
#include <sys/statvfs.h>

#ifdef USE_FILESYSTEMLIB
    #include <experimental/filesystem>
#endif


#include "HeaderType.h"
#include "FileHeader.h"
#include "ByteBuffer.h"
#include "ByteOrder.h"
#include "RecordOutput.h"
#include "RecordHeader.h"
#include "Compressor.h"
#include "RecordSupply.h"
#include "RecordCompressor.h"
#include "Util.h"
#include "EvioException.h"
#include "EvioBank.h"


#include "Disruptor/Util.h"
#include <boost/thread.hpp>
#include <boost/chrono.hpp>

#ifdef USE_FILESYSTEMLIB
namespace fs = std::experimental::filesystem;
#endif


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
             * @param pwriter pointer to WriterMT object which owns this thread.
             * @param recordSupply shared pointer to an object supplying compressed records that need to be written to file.
             */
            RecordWriter(EventWriter * pwriter, std::shared_ptr<RecordSupply> & recordSupply) :
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
                thd.interrupt();
                if (thd.try_join_for(boost::chrono::milliseconds(500))) {
                    std::cout << "     RecordWriter thread did not quit after 1/2 sec" << std::endl;
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
                while (supply->getLastSequence() > lastSeqProcessed.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                // Stop this thread, not the calling thread
                stopThread();
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

            std::shared_ptr<RecordRingItem> storeRecordCopy(std::shared_ptr<RecordRingItem> & rec) {
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
                            auto record = item->getRecord();

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
                catch (boost::thread_interrupted & e) {
                    std::cout << "EventWriter: INTERRUPTED, return" << std::endl;
                }
            }
        };



        /**
         * Class used to close files, each in its own thread,
         * to avoid slowing down while file splitting. Unlike Java, C++
         * has no built-in thread pools so just create threads as needed.
         */
        class FileCloser {

            /** Class used to start thread to do closing. */
            class CloseAsyncFChan {

                // Store quantities from exterior classes or store quantities that
                // may change between when this object is created and when this thread is run.
                std::shared_ptr<std::fstream> afChannel;
                std::shared_ptr<std::future<void>> future;
                std::shared_ptr<RecordSupply> supply;
                std::shared_ptr<RecordRingItem> item;
                FileHeader fHeader;
                std::shared_ptr<std::vector<uint32_t>> recLengths;
                uint64_t bytesWrittenToFile;
                uint32_t recordNum;
                bool addTrailer;
                bool writeIndx;
                bool noFileWriting;
                ByteOrder byteOrder;

                // A couple of things used to clean up after thread is done
                FileCloser *closer;
                std::shared_ptr<CloseAsyncFChan> sharedPtrOfMe;

                // Local storage
                uint32_t hdrBufferBytes = RecordHeader::HEADER_SIZE_BYTES + 2048;
                ByteBuffer hdrBuffer{hdrBufferBytes};
                uint8_t *hdrArray = hdrBuffer.array();

                // Thread which does the file writing
                boost::thread thd;

            public:

                /** Constructor.  */
                CloseAsyncFChan(std::shared_ptr<std::fstream> &afc,
                                std::shared_ptr<std::future<void>> &future1,
                                std::shared_ptr<RecordSupply> &supply,
                                std::shared_ptr<RecordRingItem> &ringItem,
                                FileHeader &fileHeader, std::shared_ptr<std::vector<uint32_t>> recordLengths,
                                uint64_t bytesWritten, uint32_t recordNumber,
                                bool addingTrailer, bool writeIndex, bool noWriting,
                                ByteOrder &order, FileCloser *fc) :

                        afChannel(afc), future(future1), supply(supply),
                        item(ringItem), byteOrder(order) {

                    fHeader            = fileHeader;
                    recLengths         = recordLengths;
                    bytesWrittenToFile = bytesWritten;
                    recordNum          = recordNumber;
                    addTrailer         = addingTrailer;
                    writeIndx          = writeIndex;
                    noFileWriting      = noWriting;
                    closer             = fc;

                    hdrBuffer.order(order);

                    startThread();
                }


                /**
                 * Set the shared pointer to this object for later used in removing from
                 * FileCloser's vector of these pointers (threads).
                 * @param mySharedPtr shared pointer to this object
                 */
                void setSharedPointerOfThis(std::shared_ptr<CloseAsyncFChan> & mySharedPtr) {
                    sharedPtrOfMe = mySharedPtr;
                }


                /** Stop the thread. */
                void stopThread() {
                    // Send signal to interrupt it
                    thd.interrupt();

                    // Wait for it to stop
                    if (thd.try_join_for(boost::chrono::milliseconds(500))) {
                        std::cout << "CloseAsyncFChan thread did not quit after 1/2 sec" << std::endl;
                    }

                    try {
                        // When this thread is done, remove itself from vector
                        closer->removeThread(sharedPtrOfMe);
                    }
                    catch (std::exception & e) {}
                }


            private:


                /** Create and start a thread to execute the run() method of this class. */
                void startThread() {
                    thd = boost::thread([this]() { this->run(); });
                }

                void run() {
                    // Finish writing to current file
                    if (future != nullptr) {
                        try {
                            // Wait for last write to end before we continue
                            future->get();
                        }
                        catch (std::exception &e) {}
                    }

                    // Release resources back to the ring
                    std::cout << "Closer: releaseWriterSequential, will release item seq = " << item->getSequence() << std::endl;
                    supply->releaseWriterSequential(item);

                    try {
                        if (addTrailer && !noFileWriting) {
                            writeTrailerToFile();
                        }
                    }
                    catch (std::exception &e) {}

                    try {
                        afChannel->close();
                    }
                    catch (std::exception &e) {
                        std::cout << e.what() << std::endl;
                    }

                    try {
                        // When this thread is done, remove itself from vector
                        closer->removeThread(sharedPtrOfMe);
                    }
                    catch (std::exception & e) {}
                }


                /**
                 * Write a general header as the last "header" or trailer in the file
                 * optionally followed by an index of all record lengths.
                 * This writes synchronously.
                 * This is a modified version of {@link #writeTrailerToFile()} that allows
                 * writing the trailer to the file being closed without affecting the
                 * file currently being written.
                 *
                 * @throws IOException if problems writing to file.
                 */
                void writeTrailerToFile() {

                    // Keep track of where we are right now which is just before trailer
                    uint64_t trailerPosition = bytesWrittenToFile;

                    // If we're NOT adding a record index, just write trailer
                    if (!writeIndx) {
                        try {
                            // hdrBuffer is only used in this method
                            hdrBuffer.position(0).limit(RecordHeader::HEADER_SIZE_BYTES);
                            RecordHeader::writeTrailer(hdrBuffer, 0, recordNum, nullptr);
                        }
                        catch (EvioException &e) {/* never happen */}

                        afChannel->write(reinterpret_cast<char *>(hdrArray),
                                         RecordHeader::HEADER_SIZE_BYTES);
                        if (afChannel->fail()) {
                            throw EvioException("error writing to file");
                        }
                    }
                    else {
                        // Write trailer with index

                        // How many bytes are we writing here?
                        uint32_t bytesToWrite = RecordHeader::HEADER_SIZE_BYTES + 4*recLengths->size();

                        // Make sure our array can hold everything
                        if (hdrBufferBytes < bytesToWrite) {
                            hdrBuffer = ByteBuffer(bytesToWrite);
                            hdrArray = hdrBuffer.array();
                        }
                        hdrBuffer.limit(bytesToWrite).position(0);

                        // Place data into hdrBuffer - both header and index
                        try {
                            RecordHeader::writeTrailer(hdrBuffer, (size_t)0, recordNum, recLengths);
                        }
                        catch (EvioException &e) {/* never happen */}
                        afChannel->write(reinterpret_cast<char *>(hdrArray), bytesToWrite);
                        if (afChannel->fail()) {
                            throw EvioException("error writing to file");
                        }
                    }

                    // Update file header's trailer position word
                    if (!byteOrder.isLocalEndian()) {
                        trailerPosition = SWAP_64(trailerPosition);
                    }

                    afChannel->seekg(FileHeader::TRAILER_POSITION_OFFSET);
                    afChannel->write(reinterpret_cast<char *>(&trailerPosition), sizeof(trailerPosition));
                    if (afChannel->fail()) {
                        throw EvioException("error writing to file");
                    }

                    // Update file header's bit-info word
                    if (writeIndx) {
                        uint32_t bitInfo = fHeader.setBitInfo(fHeader.hasFirstEvent(),
                                                              fHeader.hasDictionary(),
                                                              true);
                        if (!byteOrder.isLocalEndian()) {
                            bitInfo = SWAP_32(bitInfo);
                        }
                        afChannel->seekg(FileHeader::BIT_INFO_OFFSET);
                        afChannel->write(reinterpret_cast<char *>(&bitInfo), sizeof(bitInfo));
                        if (afChannel->fail()) {
                            throw EvioException("error writing to  file");
                        }
                    }

                    // Update file header's record count word
                    uint32_t recordCount = recordNum - 1;
                    if (!byteOrder.isLocalEndian()) {
                        recordCount = SWAP_32(recordCount);
                    }
                    afChannel->seekg(FileHeader::RECORD_COUNT_OFFSET);
                    afChannel->write(reinterpret_cast<char *>(&recordCount), sizeof(recordCount));
                    if (afChannel->fail()) {
                        throw EvioException("error writing to file");
                    }
                }
            };


        private:


            /** Store all currently active closing threads. */
            std::vector<std::shared_ptr<CloseAsyncFChan>> threads;


        public:


            /** Stop & delete every thread that was started. */
            void close() {
                for (const std::shared_ptr<CloseAsyncFChan> & thread : threads) {
                    thread->stopThread();
                }
                threads.clear();
            }


            /**
             * Remove thread from vector.
             * @param thread thread object to remove.
             */
            void removeThread(std::shared_ptr<CloseAsyncFChan> & thread) {
                // Look for this pointer among the shared pointers
                threads.erase(std::remove(threads.begin(), threads.end(), thread), threads.end());
            }


             /**
              * Close the given file, in the order received, in a separate thread.
              * @param afc file channel to close
              * @param future1
              * @param supply
              * @param ringItem
              * @param fileHeader
              * @param recordLengths
              * @param bytesWritten
              * @param recordNumber
              * @param addingTrailer
              * @param writeIndex
              * @param noFileWriting
              * @param order
              */
            void closeAsyncFile( std::shared_ptr<std::fstream> &afc,
                                 std::shared_ptr<std::future<void>> &future1,
                                 std::shared_ptr<RecordSupply> &supply,
                                 std::shared_ptr<RecordRingItem> &ringItem,
                                 FileHeader &fileHeader, std::shared_ptr<std::vector<uint32_t>> &recordLengths,
                                 uint64_t bytesWritten, uint32_t recordNumber,
                                 bool addingTrailer, bool writeIndex, bool noFileWriting,
                                 ByteOrder &order) {

                auto a = std::make_shared<CloseAsyncFChan>(afc, future1, supply, ringItem,
                                                           fileHeader, recordLengths,
                                                           bytesWritten, recordNumber,
                                                           addingTrailer, writeIndex,
                                                           noFileWriting, order, this);

                threads.push_back(a);
                a->setSharedPointerOfThis(a);
            }

            ~FileCloser() {
                close();
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
        uint32_t bufferSize = 0;

        /**
         * The output buffer when writing to a buffer.
         * The buffer internal to the currentRecord when writing to a file
         * and which is a reference to one of the internalBuffers.
         * When dealing with files, this buffer does double duty and is
         * initially used to read in record headers before appending data
         * to an existing file and such.
         */
        std::shared_ptr<ByteBuffer> buffer;

        /** Two internal buffers, first element last used in the future1 write,
         * the second last used in future2 write. */
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
        int internalBufSize;

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

        /** Objects to allow efficient, asynchronous file writing. */
        std::shared_ptr<std::future<void>> future1;

        /** RingItem1 is associated with future1, etc. When a write is finished,
         * the associated ring item need to be released - but not before! */
        std::shared_ptr<RecordRingItem> ringItem1;

        /** Index for selecting which future (1 or 2) to use for next file write. */
        uint32_t futureIndex = 0;

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

        EventWriter(std::string baseName, const std::string & directory, const std::string & runType,
                    uint32_t runNumber, uint64_t split, uint32_t maxRecordSize, uint32_t maxEventCount,
                    const ByteOrder & byteOrder, const std::string & xmlDictionary, bool overWriteOK,
                    bool append, std::shared_ptr<EvioBank> firstEvent, uint32_t streamId, uint32_t splitNumber,
                    uint32_t splitIncrement, uint32_t streamCount, Compressor::CompressionType compressionType,
                    uint32_t compressionThreads, uint32_t ringSize, uint32_t bufferSize);

        //---------------------------------------------
        // BUFFER Constructors
        //---------------------------------------------

        explicit EventWriter(std::shared_ptr<ByteBuffer> & buf);
        EventWriter(std::shared_ptr<ByteBuffer> & buf, std::string & xmlDictionary);
        EventWriter(std::shared_ptr<ByteBuffer> & buf, uint32_t maxRecordSize, uint32_t maxEventCount,
                    const std::string & xmlDictionary, uint32_t recordNumber,
                    Compressor::CompressionType compressionType);

    private:

        void reInitializeBuffer(std::shared_ptr<ByteBuffer> & buf, const std::bitset<24> *bitInfo,
                                uint32_t recordNumber, bool useCurrentBitInfo);

        static void staticWriteFunction(EventWriter *pWriter, const char* data, size_t len);
        static void staticDoNothingFunction(EventWriter *pWriter);

    public:

        bool isDiskFull();
        void setBuffer(std::shared_ptr<ByteBuffer> & buf, std::bitset<24> *bitInfo, uint32_t recNumber);
        void setBuffer(std::shared_ptr<ByteBuffer> & buf);

    private:

        std::shared_ptr<ByteBuffer> getBuffer();
        void expandInternalBuffers(int bytes);


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

        void setFirstEvent(std::shared_ptr<EvioNode> & node);
        void setFirstEvent(std::shared_ptr<ByteBuffer> & buf);
        void setFirstEvent(std::shared_ptr<EvioBank> bank);

    private:

        void createCommonRecord(const std::string & xmlDict,
                                std::shared_ptr<EvioBank> const & firstBank,
                                std::shared_ptr<EvioNode> const & firstNode,
                                std::shared_ptr<ByteBuffer> const & firstBuf);

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

        bool writeEvent(std::shared_ptr<EvioNode> & node, bool force = false,
                        bool duplicate = true, bool ownRecord = false);

        bool writeEventToFile(std::shared_ptr<EvioNode> & node, bool force = false,
                              bool duplicate = true, bool ownRecord = false);

        bool writeEventToFile(std::shared_ptr<ByteBuffer> & bb, bool force = false,
                              bool duplicate = true, bool ownRecord = false);

        bool writeEvent(std::shared_ptr<ByteBuffer> & bankBuffer, bool force = false , bool ownRecord = false);

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
        void writeToFileMT(std::shared_ptr<RecordRingItem> & item, bool force);

        void splitFile();
        void writeTrailerToFile(bool writeIndex);
        void flushCurrentRecordToBuffer() ;
        bool writeToBuffer(std::shared_ptr<EvioBank> & bank, std::shared_ptr<ByteBuffer> & bankBuffer) ;

        uint32_t trailerBytes();
        void writeTrailerToBuffer(bool writeIndex);

    };

}


#endif //EVIO_6_0_EVENTWRITER_H
