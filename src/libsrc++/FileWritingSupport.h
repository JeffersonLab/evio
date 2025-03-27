//
// Created by Carl Timmer on 5/15/24.
//

#ifndef EVIO_FILEWRITINGSUPPORT_H
#define EVIO_FILEWRITINGSUPPORT_H




#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <exception>
#include <atomic>
#include <future>
#include <sys/stat.h>
#include <sys/statvfs.h>


#include "RecordHeader.h"
#include "RecordSupply.h"
#include "EvioException.h"


#include <boost/thread.hpp>
#include <boost/chrono.hpp>




namespace evio {

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
            std::shared_ptr <std::fstream> afChannel;
            std::shared_ptr <std::future<void>> future;
            std::shared_ptr <RecordSupply> supply;
            std::shared_ptr <RecordRingItem> item;
            FileHeader fHeader;
            std::shared_ptr <std::vector<uint32_t>> recLengths;
            uint64_t bytesWrittenToFile;
            uint32_t recordNum;
            bool addTrailer;
            bool writeIndx;
            bool noFileWriting;
            ByteOrder byteOrder;

            // A couple of things used to clean up after thread is done
            FileCloser *closer;
            std::shared_ptr <CloseAsyncFChan> sharedPtrOfMe;

            // Local storage
            uint32_t hdrBufferBytes = RecordHeader::HEADER_SIZE_BYTES + 2048;
            ByteBuffer hdrBuffer{hdrBufferBytes};
            uint8_t *hdrArray = hdrBuffer.array();

            // Thread which does the file writing
            boost::thread thd;

        public:

            /** Constructor.  */
            CloseAsyncFChan(std::shared_ptr <std::fstream> &afc,
                            std::shared_ptr <std::future<void>> &future1,
                            std::shared_ptr <RecordSupply> &supply,
                            std::shared_ptr <RecordRingItem> &ringItem,
                            FileHeader &fileHeader, std::shared_ptr <std::vector<uint32_t>> recordLengths,
                            uint64_t bytesWritten, uint32_t recordNumber,
                            bool addingTrailer, bool writeIndex, bool noWriting,
                            ByteOrder &order, FileCloser *fc) :

                    afChannel(afc), future(future1), supply(supply),
                    item(ringItem), byteOrder(order) {

                fHeader = fileHeader;
                recLengths = recordLengths;
                bytesWrittenToFile = bytesWritten;
                recordNum = recordNumber;
                addTrailer = addingTrailer;
                writeIndx = writeIndex;
                noFileWriting = noWriting;
                closer = fc;

                hdrBuffer.order(order);

                startThread();
            }


            /** Destructor. */
            ~CloseAsyncFChan() {
                try {
                    stopThread();
                }
                catch (const std::exception& e) {
                    std::cerr << "Exception during thread cleanup: " << e.what() << std::endl;
                }
            }


            /**
             * Set the shared pointer to this object for later used in removing from
             * FileCloser's vector of these pointers (threads).
             * @param mySharedPtr shared pointer to this object
             */
            void setSharedPointerOfThis(std::shared_ptr <CloseAsyncFChan> &mySharedPtr) {
                sharedPtrOfMe = mySharedPtr;
            }


            /** Stop the thread. */
            void stopThread() {
                if (thd.joinable()) {
                    // Send signal to interrupt it
                    thd.interrupt();

                    // Wait for it to stop
                    if (thd.try_join_for(boost::chrono::milliseconds(1))) {
                        //std::cout << "EventWriter JOINED from interrupt" << std::endl;
                        return;
                    }

                    // If that didn't work, send Alert signal to ring
                    supply->errorAlert();

                    if (thd.joinable()) {
                        thd.join();
                        //std::cout << "EventWriter JOINED from alert" << std::endl;
                    }
                }

                try {
                    // When this thread is done, remove itself from vector
                    closer->removeThread(sharedPtrOfMe);
                }
                catch (std::exception &e) {}
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
                std::cout << "Closer: releaseWriterSequential, will release item seq = " << item->getSequence()
                          << std::endl;
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
                catch (std::exception &e) {}
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
                    uint32_t bytesToWrite = RecordHeader::HEADER_SIZE_BYTES + 4 * recLengths->size();

                    // Make sure our array can hold everything
                    if (hdrBufferBytes < bytesToWrite) {
                        hdrBuffer = ByteBuffer(bytesToWrite);
                        hdrArray = hdrBuffer.array();
                    }
                    hdrBuffer.limit(bytesToWrite).position(0);

                    // Place data into hdrBuffer - both header and index
                    try {
                        RecordHeader::writeTrailer(hdrBuffer, (size_t) 0, recordNum, recLengths);
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
        std::vector <std::shared_ptr<CloseAsyncFChan>> threads;


    public:


        /** Stop & delete every thread that was started. */
        void close() {
            for (const std::shared_ptr <CloseAsyncFChan> &thread: threads) {
                thread->stopThread();
            }
            threads.clear();
        }


        /**
         * Remove thread from vector.
         * @param thread thread object to remove.
         */
        void removeThread(std::shared_ptr <CloseAsyncFChan> &thread) {
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
        void closeAsyncFile(std::shared_ptr <std::fstream> &afc,
                            std::shared_ptr <std::future<void>> &future1,
                            std::shared_ptr <RecordSupply> &supply,
                            std::shared_ptr <RecordRingItem> &ringItem,
                            FileHeader &fileHeader, std::shared_ptr <std::vector<uint32_t>> &recordLengths,
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

}

#endif //EVIO_FILEWRITINGSUPPORT_H
