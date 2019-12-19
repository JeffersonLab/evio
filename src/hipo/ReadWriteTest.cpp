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




    static void testFile() {

        // Variables to track record build rate
        double freqAvg;
        long totalC = 0;
        long loops = 3;

        string fileName = "/dev/shm/hipoTestRegular.evio";

        string dictionary = "This is a dictionary";
        //dictionary = "";
        uint8_t firstEvent[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        uint32_t firstEventLen = 10;
        ByteOrder order = ByteOrder::ENDIAN_LITTLE;

        // Possible user header data
        auto userHdr = new uint8_t[10];
        for (uint8_t i = 0; i < 10; i++) {
            userHdr[i] = i;
        }

        // Create files
        string finalFilename1 = fileName + ".1";
        Writer writer(HeaderType::EVIO_FILE, order, 0, 0,
                      dictionary, firstEvent, 10, Compressor::LZ4);
        writer.open(finalFilename1);
        cout << "Past creating writer1" << endl;

        //uint8_t *buffer = generateSequentialInts(100, order);
        uint8_t *buffer = generateSequentialShorts(13, order);

        auto t1 = std::chrono::high_resolution_clock::now();

        while (true) {
            // random data array
            //writer1.addEvent(buffer, 0, 400);

            writer.addEvent(buffer, 0, 26);

//cout << int(20000000 - loops) << endl;
            totalC++;

            if (--loops < 1) break;
        }

        auto t2 = std::chrono::high_resolution_clock::now();
        auto deltaT = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);

        freqAvg = (double) totalC / deltaT.count() * 1000;

        cout << "Time = " << deltaT.count() << " msec,  Hz = " << freqAvg << endl;
        cout << "Finished all loops, count = " << totalC << endl;

        //writer1.addTrailer(true);
        writer.addTrailerWithIndex(true);
        cout << "Past write 1" << endl;

        writer.close();
        cout << "Past close 1" << endl;

        // Doing a diff between files shows they're identical!

        cout << "Finished writing files " << fileName << " + .1, .2" << endl;
        cout << "Now read file " << fileName << " + .1, .2" << endl;

        Reader reader1(finalFilename1);

        int32_t evCount = reader1.getEventCount();
        cout << "Read in file " << finalFilename1 << ", got " << evCount << " events" << endl;

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


        delete[] buffer;
        delete[] userHdr;

        //for (reader)
    }



    static void testFileMT() {

        // Variables to track record build rate
        double freqAvg;
        long totalC = 0;
        long loops = 3;

        string fileName = "/dev/shm/hipoTestMT.evio";

        string dictionary = "This is a dictionary";
        //dictionary = "";
        uint8_t firstEvent[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        uint32_t firstEventLen = 10;
        bool onlyOneWriter = true;
        ByteOrder order = ByteOrder::ENDIAN_LITTLE;

        // Possible user header data
        auto userHdr = new uint8_t[10];
        for (uint8_t i = 0; i < 10; i++) {
            userHdr[i] = i;
        }

        // Create files
        string finalFilename1 = fileName + ".1";
        WriterMT writer1(HeaderType::EVIO_FILE, order, 0, 0,
                         Compressor::LZ4, 2, true, dictionary, firstEvent, 10, 16);
        writer1.open(finalFilename1);
        cout << "Past creating writer1" << endl;

        string finalFilename2 = fileName + ".2";
        //WriterMT writer2(ByteOrder::ENDIAN_LITTLE, 0, 0, Compressor::UNCOMPRESSED, 2);
        WriterMT writer2(HeaderType::EVIO_FILE, order, 0, 0,
                         Compressor::LZ4, 2, true, dictionary, firstEvent, 10, 16);

        if (!onlyOneWriter) {
            //writer2.open(finalFilename, userHdr, 10);
            writer2.open(finalFilename2);
            cout << "Past creating writer2" << endl;
        }

        //uint8_t *buffer = generateSequentialInts(100, order);
        uint8_t *buffer = generateSequentialShorts(13, order);

        auto t1 = std::chrono::high_resolution_clock::now();

        while (true) {
            // random data array
            //writer1.addEvent(buffer, 0, 400);
            //if (!onlyOneWriter) {
            //    writer2.addEvent(buffer, 0, 400);
            //}

            writer1.addEvent(buffer, 0, 26);

            if (!onlyOneWriter) {
                writer2.addEvent(buffer, 0, 26);
            }

//cout << int(20000000 - loops) << endl;
            totalC++;

            if (--loops < 1) break;
        }

        auto t2 = std::chrono::high_resolution_clock::now();
        auto deltaT = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);

        freqAvg = (double) totalC / deltaT.count() * 1000;

        cout << "Time = " << deltaT.count() << " msec,  Hz = " << freqAvg << endl;
        cout << "Finished all loops, count = " << totalC << endl;

        //writer1.addTrailer(true);
        writer1.addTrailerWithIndex(true);
        cout << "Past write 1" << endl;

        if (!onlyOneWriter) {
            //writer2.addTrailer(true);
            writer2.addTrailerWithIndex(true);
            cout << "Past write 2" << endl;
        }

        writer1.close();
        cout << "Past close 1" << endl;

        if (!onlyOneWriter) {
            writer2.close();
            cout << "Past close 2" << endl;
        }

        // Doing a diff between files shows they're identical!

        cout << "Finished writing files " << fileName << " + .1, .2" << endl;
        cout << "Now read file " << fileName << " + .1, .2" << endl;

        Reader reader1(finalFilename1);

        int32_t evCount = reader1.getEventCount();
        cout << "Read in file " << finalFilename1 << ", got " << evCount << " events" << endl;

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

        if (!onlyOneWriter) {
            Reader reader2(finalFilename1);

            int32_t evCount2 = reader2.getEventCount();
            cout << "    Read in file2 " << finalFilename2 << ", got " << evCount2 << " events" << endl;

            cout << "reader2.getEvent(0)" << endl;
            shared_ptr<uint8_t> data = reader2.getEvent(0);
            cout << "    got event" << endl;

            uint32_t wordLen = reader2.getEventLength(0)/2;
            if (data != nullptr) {
                short *pData = reinterpret_cast<short *>(data.get());
                cout <<  "       Event #0, values =" << endl << "   ";
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
        }


        delete[] buffer;
        delete[] userHdr;

        //for (reader)
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
    ReadWriteTest::testFileMT();
    return 0;
}




#endif //EVIO_6_0_READWRITETEST_H
