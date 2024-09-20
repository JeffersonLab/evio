//
// Copyright 2024, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_EVENTWRITERV4_H
#define EVIO_EVENTWRITERV4_H


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
#include <thread>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <stdexcept>
#include <mutex>


#ifdef USE_FILESYSTEMLIB
#include <experimental/filesystem>
#endif


//#include "HeaderType.h"
#include "FileHeader.h"
#include "ByteBuffer.h"
#include "ByteOrder.h"
#include "RecordOutput.h"
#include "Compressor.h"
#include "RecordSupply.h"
#include "RecordCompressor.h"
#include "Util.h"
#include "EvioException.h"
#include "EvioBank.h"
#include "BlockHeaderV4.h"
#include "FileWritingSupport.h"


//#include "Disruptor/Util.h"
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include <boost/asio.hpp>

#ifdef USE_FILESYSTEMLIB
namespace fs = std::experimental::filesystem;
#endif



namespace evio {

    /**
     * Class used to close files, each in its own thread,
     * to avoid slowing down while file splitting.
     */
    class FileCloserV4 {
    private:
        boost::asio::io_context ioContext;
        boost::thread_group threadPool;

    public:
        FileCloserV4(int poolSize) {
            // Create a work object to keep the io_context busy
            boost::asio::io_context::work work(ioContext);

            // Add multiple threads to pool
            for (int i = 0; i < poolSize; ++i) {
                threadPool.create_thread([&]() {
                    ioContext.run();
                });
            }
        }

        void close() {
            threadPool.join_all();
        }

        void closeAsyncFile(std::shared_ptr<std::fstream> afc,
                            std::shared_ptr<std::future<void>> future) {
            ioContext.post([=]() {
                try {
                    // There may be a simultaneous write in progress,
                    // wait for it to finish.
                    if (future != nullptr && future->valid()) {
                        // Wait for last write to end before we continue
                        future->get();
                    }
                }
                catch (std::exception & e) {}

                afc->close();
            });
        }
    };


        /**
         * Class to write data into a file or buffer in the evio version 4 format.
         * This is included so that CODA DAQ systems can avoid using the cumbersome evio version 6 format.
         * This class is not thread-safe.
         *
         * @author Carl Timmer.
         * @date 5/14/24.
         */
        class EventWriterV4 {


        public:

            /**
              * The default maximum size, in bytes, for a single block used for writing.
              * It's set to 16MB (2^24) since that's an efficient number for writing to modern disks.
              * And it is consistent with the java version of this class.
              * It is a soft limit since a single event larger than this limit may need to be written.
              */
            static const int DEFAULT_BLOCK_SIZE = 16777216;

            /** The default maximum event count for a single block used for writing. */
            static const int DEFAULT_BLOCK_COUNT = 10000;

            /**
             * The default byte size of internal buffer.
             * It's enforced to be, at a minimum, DEFAULT_BLOCK_SIZE + a little.
             */
            static const size_t DEFAULT_BUFFER_SIZE = DEFAULT_BLOCK_SIZE + 1024;


        private:

            /**
             * Offset to where the block length is written in the byte buffer,
             * which always has a physical record header at the top.
             */
            static const int BLOCK_LENGTH_OFFSET = 0;

            /**
             * Offset to where the block number is written in the byte buffer,
             * which always has a physical record header at the top.
             */
            static const int BLOCK_NUMBER_OFFSET = 4;

            /**
             * Offset to where the header length is written in the byte buffer,
             * which always has a physical record header at the top.
             */
            static const int HEADER_LENGTH_OFFSET = 8;

            /**
             * Offset to where the event count is written in the byte buffer,
             * which always has a physical record header at the top.
             */
            static const int EVENT_COUNT_OFFSET = 12;

            /**
             * Offset to where the first reserved word is written in the byte buffer,
             * which always has a physical record header at the top.
             */
            static const int RESERVED1_COUNT_OFFSET = 16;

            /**
             * Offset to where the bit information is written in the byte buffer,
             * which always has a physical record header at the top.
             */
            static const int BIT_INFO_OFFSET = 20;

            /**
             * Offset to where the magic number is written in the byte buffer,
             * which always has a physical record header at the top.
             */
            static const int MAGIC_OFFSET = 28;

            /** Mask to get version number from 6th int in block. */
            static const int VERSION_MASK = 0xff;

            /**
             * The upper limit of maximum size for a single block used for writing
             * is randomly chosen to be ~134MB (actually 2^27).
             * Make this consistent with java implementation.
             * It is a soft limit since a single event larger than this limit
             * may need to be written.
             */
            static const int MAX_BLOCK_SIZE = 134217728;

            /** The upper limit of maximum event count for a single block used for writing. */
            static const int MAX_BLOCK_COUNT = 1000000;

            /**
             * The lower limit of maximum size for a single block used for writing, in bytes.
             */
            static const int MIN_BLOCK_SIZE = 32768;

            /** The lower limit of maximum event count for a single block used for writing. */
            static const int MIN_BLOCK_COUNT = 1;

            /** Size of block header in bytes. */
            static const int headerBytes = 32;

            /** Size of block header in words. */
            static const int headerWords = 8;

            /** Turn on or off the debug printout. */
            static const bool debug = false;


        private:

            /**
             * Mutex to protect close, flush, isClosed, writeEvent, getByteBuffer, setFirstEvent,
             * and examineFirstBlockHeader from stepping on each other's toes.
             */
            std::recursive_mutex mtx;


            /**
             * Maximum block size (32 bit words) for a single block (block header & following events).
             * There may be exceptions to this limit if the size of an individual
             * event exceeds this limit.
             * Default is {@link #DEFAULT_BLOCK_SIZE}.
             */
            uint32_t maxBlockSize;

            /**
             * Maximum number of events in a block (events following a block header).
             * Default is {@link #DEFAULT_BLOCK_COUNT}.
             */
            uint32_t maxEventCount;

            /** Running count of the block number. The next one to use starting with 1. */
            uint32_t blockNumber = 1;

            /**
             * Dictionary to include in xml format in the first event of the first block
             * when writing the file.
             */
            std::string xmlDictionary;

            /** True if wrote dictionary to a single file (not all splits taken together). */
            bool wroteDictionary = false;

            /** Byte array containing dictionary in evio format but <b>without</b> block header. */
            std::vector<uint8_t> dictionaryByteArray;

            /** The number of bytes it takes to hold the dictionary
             *  in a bank of evio format. Basically the size of
             *  {@link #dictionaryByteArray} with the length of a bank header added. */
            uint32_t dictionaryBytes = 0;

            /** Has a first event been defined? */
            bool haveFirstEvent = false;

            /** Byte array containing firstEvent in evio format but <b>without</b> block header. */
            std::vector<uint8_t> firstEventByteArray;

            /** The number of bytes it takes to hold the first event
             *  in a bank of evio format. Basically the size of
             *  {@link #firstEventByteArray} with the length of a bank header added. */
            uint32_t firstEventBytes = 0;

            /** The number of bytes it takes to hold both the dictionary and the first event
             *  in banks of evio format (evio headers, not block headers, included). */
            uint32_t commonBlockByteSize = 0;

            /** Number of events in the common block. At most 2 - dictionary & firstEvent. */
            uint32_t commonBlockCount = 0;

            /**
             * Bit information in the block headers:<p>
             * <ul>
             * <li>Bit one: is the first event a dictionary?  Used in first block only.
             * <li>Bit two: is this the last block?
             * </ul>
             */
            std::bitset<24> bitInfo;

            /** <code>True</code> if {@link #close()} was called, else <code>false</code>. */
            bool closed = false;

            /** <code>True</code> if writing to file, else <code>false</code>. */
            bool toFile = false;

            /** <code>True</code> if appending to file/buffer, <code>false</code> if (over)writing. */
            bool append = false;

            /** <code>True</code> if appending to file/buffer with dictionary, <code>false</code>. */
            bool hasAppendDictionary = false;

            /** Number of bytes of data to shoot for in a single block including header. */
            uint32_t targetBlockSize = 0;

            /** Version 4 block header reserved int 1. Used by CODA for source ID in event building. */
            uint32_t reserved1 = 0;

            /** Version 4 block header reserved int 2. */
            uint32_t reserved2 = 0;

            /** Number of bytes written to the current buffer. */
            uint64_t bytesWrittenToBuffer = 0;

            /** Number of events written to final destination buffer or file's internal buffer
             * including dictionary (although may not be flushed yet). */
            uint32_t eventsWrittenToBuffer = 0;

            /**
             * Total number of events written to buffer or file (although may not be flushed yet).
             * Will be the same as eventsWrittenToBuffer (- dictionary) if writing to buffer.
             * If the file being written to is split, this value refers to all split files
             * taken together. Does NOT include dictionary(ies).
             */
            uint32_t eventsWrittenTotal = 0;

            /** When writing to a buffer, keep tabs on the front of the last (non-ending) header written. */
            uint32_t currentHeaderPosition = 0;

            /** Size in 32-bit words of the currently-being-used block (includes entire block header). */
            uint32_t currentBlockSize = 0;

            /** Number of events written to the currently-being-used block (including dictionary if first blk). */
            uint32_t currentBlockEventCount = 0;

            /** Total size of the buffer in bytes. */
            size_t bufferSize;

            /**
             * The output buffer when writing to a buffer.
             * The internal buffer when writing to a file which is
             * a reference to one of the internalBuffers.
             */
            std::shared_ptr<ByteBuffer> buffer;

            /** Buffer last used in the future1 write. */
            std::shared_ptr<ByteBuffer> usedBuffer;

            /** Three internal buffers used for writing to a file. */
            std::vector<std::shared_ptr<ByteBuffer>> internalBuffers;

            /** The byte order in which to write a file or buffer. */
            ByteOrder byteOrder {ByteOrder::ENDIAN_LOCAL};

            //-----------------------
            // File related members
            //-----------------------

            /** Total size of the internal buffers in bytes. */
            size_t internalBufSize;

            /** Variable used to stop accepting events to be included in the inner buffer
             * holding the current block. Used when disk space is inadequate. */
            bool diskIsFull;

            /** Variable used to stop accepting events to be included in the inner buffer
            *  holding the current block. Used when disk space is inadequate.
            *  This is atomic and therefore works between threads. */
            std::atomic_bool diskIsFullVolatile{false};

            /** File is open for writing or not. */
            bool fileOpen = false;

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

//
//            /** File object corresponding to file currently being written. */
//            File currentFile;
//
//            /** Path object corresponding to file currently being written. */
//            Path currentFilePath;
//

//            /** Index for selecting which future (1 or 2) to use for next file write. */
//            uint32_t futureIndex = 0;

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

            /**
             * Id of this specific data stream.
             * In CODA, a data stream is a chain of ROCS and EBs ending in a final EB (SEB or PEB) or ER.
             */
            uint32_t streamId = 0;

            /** The total number of data streams in DAQ. */
            uint32_t streamCount = 1;

            /** Is it OK to overwrite a previously existing file? */
            bool overWriteOK = false;

            /** Number of bytes written to the current file (including ending header),
             * all split files - including dictionary. */
            uint64_t bytesWrittenToFile = 0;

            /** Number of events actually written to the current file - not the total in
             * all split files - including dictionary. */
            uint32_t eventsWrittenToFile = 0;

            /** <code>True</code> if internal buffer has the last empty block header
             * written and buffer position is immediately after it, else <code>false</code>. */
            bool lastEmptyBlockHeaderExists;

            /** Object used to close files in a separate thread when splitting
             *  so as to allow writing speed not to dip so low. */
            std::shared_ptr<FileCloserV4> fileCloser = nullptr;


        public:



            //---------------------------------------------
            // FILE Constructors
            //---------------------------------------------


            explicit EventWriterV4(std::string & filename,
                                   const ByteOrder & byteOrder = ByteOrder::nativeOrder(),
                                   bool append = false);

            EventWriterV4(std::string & filename,
                          const std::string & xmlDictionary,
                          const ByteOrder & byteOrder = ByteOrder::nativeOrder(),
                          bool append = false);

            EventWriterV4(std::string & baseName, const std::string & directory, const std::string & runType,
                          uint32_t runNumber = 1, uint64_t split = 0,
                          uint32_t maxBlockSize = DEFAULT_BLOCK_SIZE,
                          uint32_t maxEventCount = DEFAULT_BLOCK_COUNT,
                          const ByteOrder & byteOrder = ByteOrder::nativeOrder(),
                          const std::string & xmlDictionary = "",
                          bool overWriteOK = true, bool append = false,
                          std::shared_ptr<EvioBank> firstEvent = nullptr,
                          uint32_t streamId = 0, uint32_t splitNumber = 0,
                          uint32_t splitIncrement = 1, uint32_t streamCount = 1,
                          size_t bufferSize = DEFAULT_BUFFER_SIZE,
                          std::bitset<24> *bitInfo = nullptr);


            //---------------------------------------------
            // BUFFER Constructors
            //---------------------------------------------


            explicit EventWriterV4(std::shared_ptr<ByteBuffer> buf, const std::string & xmlDictionary = "", bool append = false);


            EventWriterV4(std::shared_ptr<ByteBuffer> buf,
                        int maxBlockSize, int maxEventCount,
                        const std::string & xmlDictionary = "", std::bitset < 24 > *bitInfo = nullptr,
                        int reserved1 = 0, int blockNumber = 1, bool append = false,
                        std::shared_ptr<EvioBank> firstEvent = nullptr);


        private:

            void initializeBuffer(std::shared_ptr<ByteBuffer> buf, int maxBlockSize, int maxEventCount,
                                    const std::string & xmlDictionary, std::bitset < 24 > *bitInfo,
                                    int reserved1, int blockNumber, bool append,
                                    std::shared_ptr<EvioBank> firstEvent);

            void reInitializeBuffer(std::shared_ptr<ByteBuffer> buf, std::bitset < 24 > *bitInfo, int blockNumber);
            static void staticWriteFunction(EventWriterV4 *pWriter, const char* data, size_t len);
            static void staticDoNothingFunction(EventWriterV4 *pWriter);


        public:

            bool isDiskFull() const;
            uint64_t getBytesWrittenToBuffer() const;
            void setBuffer(std::shared_ptr<ByteBuffer> buf, std::bitset < 24 > *bitInfo, int blockNumber);
            void setBuffer(std::shared_ptr<ByteBuffer> buf);
            std::shared_ptr<ByteBuffer> getByteBuffer();
            bool isToFile() const;
            bool isClosed();
            std::string getCurrentFilename() const;
            std::string getCurrentFilePath() const;
            uint32_t getSplitNumber() const;
            uint32_t getSplitCount() const;
            uint32_t getBlockNumber() const;
            uint32_t getEventsWritten() const;
            ByteOrder getByteOrder() const;
            void setStartingBlockNumber(int startingBlockNumber);

            void setFirstEvent(std::shared_ptr<EvioNode> node);
            void setFirstEvent(std::shared_ptr<ByteBuffer> buffer);
            void setFirstEvent(std::shared_ptr<EvioBank> bank);

            void flush();
            void close();
            bool hasRoom(size_t bytes);

            bool writeEvent(std::shared_ptr<EvioNode> node, bool force);
            bool writeEvent(std::shared_ptr<EvioNode> node, bool force, bool duplicate);
            bool writeEventToFile(std::shared_ptr<EvioNode> node, bool force, bool duplicate);
            bool writeEvent(std::shared_ptr<ByteBuffer> eventBuffer);
            bool writeEvent(std::shared_ptr<EvioBank> bank);
            bool writeEvent(std::shared_ptr<ByteBuffer> bankBuffer, bool force);
            bool writeEvent(std::shared_ptr<EvioBank> bank, bool force);
            bool writeEventToFile(std::shared_ptr<EvioBank> bank, std::shared_ptr<ByteBuffer> bankBuffer, bool force);


       protected:
            void examineFirstBlockHeader();

       private:
            std::shared_ptr<ByteBuffer> getBuffer();
            void toAppendPosition();
            void writeNewHeader(uint32_t eventCount, uint32_t blockNumber, std::bitset<24> *bitInfo,
                                bool hasDictionary, bool isLast, bool hasFirstEv = false);

            void writeCommonBlock();
            void resetBuffer(bool beforeDictionary);
            void expandBuffer(size_t newSize);
            void writeEventToBuffer(std::shared_ptr<EvioBank> bank, std::shared_ptr<ByteBuffer> bankBuffer,
                                    int currentEventBytes);
            bool writeEvent(std::shared_ptr<EvioBank> bank, std::shared_ptr<ByteBuffer> bankBuffer, bool force);
            bool fullDisk();
            bool flushToFile(bool force, bool checkDisk);
            void splitFile();

        };

}

#endif //EVIO_EVENTWRITERV4_H
