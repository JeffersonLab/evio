//
// Created by Carl Timmer on 2019-07-05.
//

#ifndef EVIO_6_0_READWRITETEST_H
#define EVIO_6_0_READWRITETEST_H


#include <functional>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <chrono>
#include <memory>

#include "ByteBuffer.h"
#include "ByteOrder.h"
#include "Writer.h"
#include "Reader.h"
#include "WriterMT.h"
#include "RecordHeader.h"
#include "HipoException.h"
#include "RecordRingItem.h"
#include "EvioNode.h"
#include "Compressor.h"



class ReadWriteTest {


public:


    static uint8_t* generateArray() {
        //double d = static_cast<double> (rand()) / static_cast<double> (RAND_MAX);
        int size = rand() % 35;
        size += 100;
        auto buffer = new uint8_t[size];
        for(int i = 0; i < size; i++){
            buffer[i] = (uint8_t)(rand() % 126);
        }
        return buffer;
    }

    static uint8_t* generateArray(int size) {
        uint8_t* buffer = new uint8_t[size];
        for(int i = 0; i < size; i++){
            buffer[i] =  (uint8_t) ((rand() % 125) + 1);
        }
        return buffer;
    }

    /**
     * Write ints.
     * @param size number of INTS
     * @param order byte order of ints in memory
     * @return
     */
    static uint8_t* generateSequentialInts(int size, ByteOrder & order) {
        auto buffer = new uint32_t[size];
        for(int i = 0; i < size; i++) {
            if (ByteOrder::needToSwap(order)) {
                buffer[i] = SWAP_32(i);
                //buffer[i] = SWAP_32(1);
            }
            else {
                buffer[i] = i;
                //buffer[i] = 1;
            }
        }
        return reinterpret_cast<uint8_t *>(buffer);
    }

    /**
     * Write shorts.
     * @param size number of SHORTS
     * @param order byte order of shorts in memory
     * @return
     */
    static uint8_t* generateSequentialShorts(int size, ByteOrder & order) {
        auto buffer = new uint16_t[size];
        for(int i = 0; i < size; i++) {
            if (ByteOrder::needToSwap(order)) {
                buffer[i] = SWAP_16(i);
                //buffer[i] = SWAP_16(1);
            }
            else {
                buffer[i] = i;
                //buffer[i] = 1;
            }
        }
        return reinterpret_cast<uint8_t *>(buffer);
    }

    static void print(uint8_t* array, int arrayLen) {
        int wrap = 20;
        for (int i = 0; i < arrayLen; i++) {
            cout << setw(3) << array[i];
            if ((i+1)%wrap == 0) cout << endl;
        }
        cout << endl;
    }


    static ByteBuffer generateEvioBuffer(ByteOrder & order) {
        // Create an evio bank of ints
        ByteBuffer evioDataBuf(20);
        evioDataBuf.order(order);
        evioDataBuf.putInt(4);  // length in words, 5 ints
        evioDataBuf.putInt(0xffd10100);  // 2nd evio header word   (prestart event)
        evioDataBuf.putInt(0x1234);  // time
        evioDataBuf.putInt(0x5);  // run #
        evioDataBuf.putInt(0x6);  // run type
        evioDataBuf.flip();
        Util::printBytes(evioDataBuf, 0, 20, "Original buffer");
        return evioDataBuf;
    }


    static void writeFile(string finalFilename) {

        // Variables to track record build rate
        double freqAvg;
        long totalC = 0;
        long loops = 3;

        string dictionary = "This is a dictionary";
        //dictionary = "";
        uint8_t firstEvent[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        uint32_t firstEventLen = 10;
        bool addTrailerIndex = true;
        ByteOrder order = ByteOrder::ENDIAN_LITTLE;
        //Compressor::CompressionType compType = Compressor::GZIP;
        Compressor::CompressionType compType = Compressor::UNCOMPRESSED;

        // Possible user header data
        auto userHdr = new uint8_t[10];
        for (uint8_t i = 0; i < 10; i++) {
            userHdr[i] = i;
        }

        // Create files
        Writer writer(HeaderType::EVIO_FILE, order, 0, 0,
                      dictionary, firstEvent, 10, compType, addTrailerIndex);
        //writer.open(finalFilename);
        writer.open(finalFilename, userHdr, 10);
        cout << "Past creating writer1" << endl;

        //uint8_t *dataArray = generateSequentialInts(100, order);
        uint8_t *dataArray = generateSequentialShorts(13, order);
        // Calling the following method makes a shared pointer out of dataArray, so don't delete
        ByteBuffer dataBuffer(dataArray, 26);

        // Create an evio bank of ints
        ByteBuffer evioDataBuf = generateEvioBuffer(order);
        // Create node from this buffer
        std::shared_ptr<EvioNode> node = EvioNode::extractEventNode(evioDataBuf,0,0,0);

        auto t1 = std::chrono::high_resolution_clock::now();

        while (true) {
            // random data array
            //writer.addEvent(dataArray, 0, 26);
            writer.addEvent(dataBuffer);

//cout << int(20000000 - loops) << endl;
            totalC++;

            if (--loops < 1) break;
        }

        cout << " node's type = " << node->getTypeObj().toString() << endl;
        writer.addEvent(*node.get());

        auto t2 = std::chrono::high_resolution_clock::now();
        auto deltaT = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);

        freqAvg = (double) totalC / deltaT.count() * 1000;

        cout << "Time = " << deltaT.count() << " msec,  Hz = " << freqAvg << endl;
        cout << "Finished all loops, count = " << totalC << endl;

        //------------------------------
        // Add entire record at once
        //------------------------------

        RecordOutput recOut(order);
        recOut.addEvent(dataArray, 0, 26);
        writer.writeRecord(recOut);

        //------------------------------

        //writer1.addTrailer(true);
        //writer.addTrailerWithIndex(addTrailerIndex);
        cout << "Past write" << endl;

        writer.close();
        cout << "Past close" << endl;

        // Doing a diff between files shows they're identical!

        cout << "Finished writing file " << finalFilename << " now read it" << endl;

        //delete[] dataArray;
        delete[] userHdr;
    }


    static void writeFileMT(string fileName) {

        // Variables to track record build rate
        double freqAvg;
        long totalC = 0;
        long loops = 3;


        string dictionary = "This is a dictionary";
        //dictionary = "";
        uint8_t firstEvent[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        uint32_t firstEventLen = 10;
        bool addTrailerIndex = true;
        ByteOrder order = ByteOrder::ENDIAN_LITTLE;
        //Compressor::CompressionType compType = Compressor::GZIP;
        Compressor::CompressionType compType = Compressor::UNCOMPRESSED;

        // Possible user header data
        auto userHdr = new uint8_t[10];
        for (uint8_t i = 0; i < 10; i++) {
            userHdr[i] = i;
        }

        // Create files
        string finalFilename1 = fileName;
        WriterMT writer1(HeaderType::EVIO_FILE, order, 0, 0,
                         dictionary, firstEvent, 10, compType, 2, addTrailerIndex, 16);
        writer1.open(finalFilename1, userHdr, 10);
        cout << "Past creating writer1" << endl;

        //uint8_t *dataArray = generateSequentialInts(100, order);
        uint8_t *dataArray = generateSequentialShorts(13, order);
        // Calling the following method makes a shared pointer out of dataArray, so don't delete
        ByteBuffer dataBuffer(dataArray, 26);

        // Create an evio bank of ints
        ByteBuffer evioDataBuf = generateEvioBuffer(order);
        // Create node from this buffer
        std::shared_ptr<EvioNode> node = EvioNode::extractEventNode(evioDataBuf,0,0,0);

        auto t1 = std::chrono::high_resolution_clock::now();

        while (true) {
            // random data dataArray
            //writer1.addEvent(buffer, 0, 26);
            writer1.addEvent(dataBuffer);

//cout << int(20000000 - loops) << endl;
            totalC++;

            if (--loops < 1) break;
        }

        writer1.addEvent(*node.get());

        auto t2 = std::chrono::high_resolution_clock::now();
        auto deltaT = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);

        freqAvg = (double) totalC / deltaT.count() * 1000;

        cout << "Time = " << deltaT.count() << " msec,  Hz = " << freqAvg << endl;
        cout << "Finished all loops, count = " << totalC << endl;

        //------------------------------
        // Add entire record at once
        //------------------------------

        RecordOutput recOut(order);
        recOut.addEvent(dataArray, 0, 26);
        writer1.writeRecord(recOut);

        //------------------------------

        //writer1.addTrailer(true);
        writer1.addTrailerWithIndex(addTrailerIndex);
        cout << "Past write" << endl;

        writer1.close();
        cout << "Past close" << endl;

        // Doing a diff between files shows they're identical!

        cout << "Finished writing file " << fileName << ", now read it in" << endl;

        //delete[] dataArray;
        delete[] userHdr;
    }

    static void readFile(string finalFilename) {

        Reader reader1(finalFilename);
        ByteOrder order = reader1.getByteOrder();

        int32_t evCount = reader1.getEventCount();
        cout << "Read in file " << finalFilename << ", got " << evCount << " events" << endl;

        string dict = reader1.getDictionary();
        cout << "   Got dictionary = " << dict << endl;

        shared_ptr<uint8_t> & pFE = reader1.getFirstEvent();
        if (pFE != nullptr) {
            int32_t feBytes = reader1.getFirstEventSize();
            cout << "   First Event bytes = " << feBytes << endl;
            cout << "   First Event values = " << endl << "   ";
            for (int i = 0; i < feBytes; i++) {
                cout << (uint32_t) ((pFE.get())[i]) << ",  ";
            }
            cout << endl;
        }

        cout << "reader.getEvent(0)" << endl;
        shared_ptr<uint8_t> data = reader1.getEvent(0);
        cout << "got event" << endl;
//        uint32_t wordLen = reader.getEventLength(0)/4;
//        cout << "got event len" << endl;
//
//        if (data != nullptr) {
//            int *pData = reinterpret_cast<int *>(data.get());
//            cout <<  "   Event #0, values =" << endl << "   ";
//            for (int i = 0; i < wordLen; i++) {
//                if (order.isLocalEndian()) {
//                    cout << *pData << ",  ";
//                }
//                else {
//                    cout << SWAP_32(*pData) <<",  ";
//                }
//                pData++;
//                if ((i+1)%5 == 0) cout << endl;
//            }
//            cout << endl;
//        }

        uint32_t wordLen = reader1.getEventLength(0)/2;
        if (data != nullptr) {
            short *pData = reinterpret_cast<short *>(data.get());
            cout <<  "   Event #0, values =" << endl << "   ";
            for (int i = 0; i < wordLen; i++) {
                if (order.isLocalEndian()) {
                    cout << *pData << ",  ";
                }
                else {
                    cout << SWAP_16(*pData) <<",  ";
                }
                pData++;
                if ((i+1)%5 == 0) cout << endl;
            }
            cout << endl;
        }

//        if (!onlyOneWriter) {
//            Reader reader2(finalFilename);
//
//            int32_t evCount2 = reader2.getEventCount();
//            cout << "    Read in file2 " << finalFilename << ", got " << evCount2 << " events" << endl;
//
//            cout << "reader2.getEvent(0)" << endl;
//            shared_ptr<uint8_t> data = reader2.getEvent(0);
//            cout << "    got event" << endl;
//
//            uint32_t wordLen = reader2.getEventLength(0)/2;
//            if (data != nullptr) {
//                short *pData = reinterpret_cast<short *>(data.get());
//                cout <<  "       Event #0, values =" << endl << "   ";
//                for (int i = 0; i < wordLen; i++) {
//                    if (order.isLocalEndian()) {
//                        cout << *pData << ",  ";
//                    }
//                    else {
//                        cout << SWAP_16(*pData) <<",  ";
//                    }
//                    pData++;
//                    if ((i+1)%5 == 0) cout << endl;
//                }
//                cout << endl;
//            }
//        }

    }



    static void convertor() {
        string filenameIn("/dev/shm/hipoTest1.evio");
        string filenameOut("/dev/shm/hipoTestOut.evio");
        try {
            Reader reader(filenameIn);
            uint32_t nevents = reader.getEventCount();

            cout << "     OPENED FILE " << filenameOut << " for writing " << nevents << " events to " << filenameOut << endl;
            Writer writer(filenameOut, ByteOrder::ENDIAN_LITTLE, 10000, 8*1024*1024);
            //writer.setCompressionType(Compressor::LZ4);

            for (int i = 0; i < nevents; i++) {
                cout << "     Try getting EVENT # " << i << endl;
                shared_ptr<uint8_t> pEvent = reader.getEvent(i);
                cout << "     Got event " << i << endl;
                uint32_t eventLen = reader.getEventLength(i);
                cout << "     Got event len = " << eventLen << endl;

                writer.addEvent(pEvent.get(), 0, eventLen);
            }
            cout << "     converter END" << endl;
            writer.close();
        } catch (HipoException & ex) {
            cout << ex.what() << endl;
        }

    }


//    static void disruptorTest() {
//
//        const size_t ringBufSize = 8;
//
//        std::array<int, ringBufSize> events;
//        for (size_t i = 0; i < ringBufSize; i++) events[i] = 2*i;
//
//
//        // For single threaded producer which spins to wait.
//        // This creates and contains a RingBuffer object.
//        disruptor::Sequencer<int, ringBufSize, disruptor::SingleThreadedStrategy<ringBufSize>,
//                disruptor::BusySpinStrategy> sequencer(events);
//
//        disruptor::Sequence readSequence(disruptor::kInitialCursorValue);
//        std::vector<disruptor::Sequence*> dependents = {&readSequence};
//
////        int64_t cursor = sequencer.GetCursor();
////        bool hasCap = sequencer.HasAvailableCapacity();
////
////        const disruptor::SequenceBarrier<disruptor::BusySpinStrategy> & barrier = sequencer.NewBarrier(dependents);
////        int64_t seq = barrier.get_sequence();
//
//        disruptor::Sequence & cursorSequence = sequencer.GetCursorSequence();
//        disruptor::SequenceBarrier<disruptor::BusySpinStrategy> barrier(cursorSequence, dependents);
//
//        shared_ptr<disruptor::SequenceBarrier<disruptor::BusySpinStrategy>> barrierPtr = sequencer.NewBarrier(dependents);
//        int64_t seq = barrierPtr->get_sequence();
//
//    }

};




int main(int argc, char **argv) {
    string filename   = "/dev/shm/hipoTest.evio";
    string filenameMT = "/dev/shm/hipoTestMT.evio";
//    string filename   = "/dev/shm/hipoTest-j.evio";
//    string filenameMT = "/dev/shm/hipoTestMT-j.evio";

    // Write files
    ReadWriteTest::writeFile(filename);
    ReadWriteTest::writeFileMT(filenameMT);

    // Read files just written
    ReadWriteTest::readFile(filename);
    cout << endl << endl << "----------------------------------------" << endl << endl;
    ReadWriteTest::readFile(filenameMT);

    return 0;
}




#endif //EVIO_6_0_READWRITETEST_H
