//
// Created by Carl Timmer on 2019-07-05.
//

#ifndef EVIO_6_0_READWRITETEST_H
#define EVIO_6_0_READWRITETEST_H


#include <string>
#include <cstdint>
#include <cstdlib>
#include <chrono>

#include "ByteBuffer.h"
#include "Writer.h"
#include "Reader.h"
#include "WriterMT.h"
#include "RecordHeader.h"
#include "HipoException.h"

class ReadWriteTest {


public:


    static uint8_t* generateArray() {
        //double d = static_cast<double> (rand()) / static_cast<double> (RAND_MAX);
        int size = (35*rand()/RAND_MAX);
        size += 100;
        uint8_t* buffer = new uint8_t[size];
        for(int i = 0; i < size; i++){
            buffer[i] = (uint8_t)(126*rand()/RAND_MAX);
        }
        return buffer;
    }

    static uint8_t* generateArray(int size) {
        uint8_t* buffer = new uint8_t[size];
        for(int i = 0; i < size; i++){
            buffer[i] =  (uint8_t) (125*rand()/RAND_MAX + 1.0);
        }
        return buffer;
    }

    static void print(uint8_t* array, int arrayLen) {
        int wrap = 20;
        for (int i = 0; i < arrayLen; i++) {
            cout << setw(3) << array[i];
            if ((i+1)%wrap == 0) cout << endl;
        }
        cout << endl;
    }



    static void testStreamRecord() {

        // Variables to track record build rate
        double freqAvg=0.;
        long totalC=0L;
        // Ignore the first N values found for freq in order
        // to get better avg statistics. Since the JIT compiler in java
        // takes some time to analyze & compile code, freq may initially be low.
        long ignoreLoops = 1L;
        long loops  = 20L;

        try {
            // Create file
            string filename("/dev/shm/hipoTest1.evio");

            cout << "testStreamRecord: 1.5, " << ByteOrder::ENDIAN_LITTLE.getName() << ", " <<HeaderType::EVIO_FILE.getName() << endl;
            Writer writer(ByteOrder::ENDIAN_LITTLE, 10000, 10000000);
            cout << "testStreamRecord: 2" << endl;
            writer.getRecordHeader().setCompressionType(Compressor::CompressionType::UNCOMPRESSED);
            cout << "testStreamRecord: 3" << endl;
            writer.open(filename);

            cout << "testStreamRecord: 4" << endl;

            cout << "output record size = " << writer.getRecord().getInternalBufferCapacity() << " bytes" << endl;
            cout << "bin buf lim = " << writer.getBuffer().limit() << " bytes" << endl;

            uint8_t* array = generateArray(100);

            auto t1 = std::chrono::high_resolution_clock::now();

            while (true) {
                // random data array
                writer.addEvent(array, 0, 100);
                cout << "bin buf lim = " << writer.getBuffer().limit() << " bytes" << endl;

                //cout << int(20000000 - loops) << endl;
                // Ignore beginning loops to remove JIT compile time
                if (ignoreLoops-- > 0) {
                    t1 = std::chrono::high_resolution_clock::now();
                }
                else {
                    totalC++;
                }

                if (--loops < 1) break;
            }

            auto t2 = std::chrono::high_resolution_clock::now();
            auto deltaT = std::chrono::duration_cast<std::chrono::milliseconds> (t2 - t1);

            freqAvg = (double) totalC / deltaT.count() * 1000;

            cout << "Time = " << deltaT.count() << " msec,  Hz = " << freqAvg << endl;
            cout << "Finished all loops, count = " << totalC << endl;

            //        // Create our own record
            //        RecordOutput myRecord(ByteOrder::ENDIAN_LOCAL);
            //        buffer = generateArray(200);
            //        myRecord.addEvent(buffer, 0, 200);
            //        myRecord.addEvent(buffer, 0, 200);
            //        writer.writeRecord(myRecord);
            //        writer.addTrailer(true);
            //        writer.addTrailerWithIndex(true);

            writer.close();

            cout << "Finished writing file" << endl;
        }
        catch (std::exception & ex) {
            if (ex.what() == nullptr) {
                cout << "error msg is NULL!!!!" << endl;
            } else {
            cout << ex.what() << endl;
        }

        }


    }


    static void testStreamRecordMT(){

        // Variables to track record build rate
        double freqAvg;
        long totalC=0;
        // Ignore the first N values found for freq in order
        // to get better avg statistics. Since the JIT compiler in java
        // takes some time to analyze & compile code, freq may initially be low.
        long ignoreLoops = 0;
        long loops  = 6;

        string fileName = "/dev/shm/hipoTest2.evio";

        // Create files
        string finalFilename = fileName + ".1";
        WriterMT writer1(finalFilename, ByteOrder::ENDIAN_LITTLE, 0, 0, Compressor::LZ4, 1);
        finalFilename = fileName + ".2";
        WriterMT writer2(finalFilename, ByteOrder::ENDIAN_LITTLE, 0, 0, Compressor::LZ4, 2);
        finalFilename = fileName + ".3";
        WriterMT writer3(finalFilename, ByteOrder::ENDIAN_LITTLE, 0, 0, Compressor::LZ4, 3);

        uint8_t* buffer = generateArray(400);

        auto t1 = std::chrono::high_resolution_clock::now();

        while (true) {
            // random data array
            writer1.addEvent(buffer, 0, 400);
            writer2.addEvent(buffer, 0, 400);
            writer3.addEvent(buffer, 0, 400);

//cout << int(20000000 - loops) << endl;
            // Ignore beginning loops to remove JIT compile time
            if (ignoreLoops-- > 0) {
                t1 = std::chrono::high_resolution_clock::now();
            }
            else {
                totalC++;
            }

            if (--loops < 1) break;
        }

        auto t2 = std::chrono::high_resolution_clock::now();
        auto deltaT = std::chrono::duration_cast<std::chrono::milliseconds> (t2 - t1);

        freqAvg = (double) totalC / deltaT.count() * 1000;

        cout << "Time = " << deltaT.count() << " msec,  Hz = " << freqAvg << endl;
        cout << "Finished all loops, count = " << totalC << endl;


        writer1.addTrailer(true);
        writer1.addTrailerWithIndex(true);

        writer2.addTrailer(true);
        writer2.addTrailerWithIndex(true);

        writer3.addTrailer(true);
        writer3.addTrailerWithIndex(true);


        writer1.close();
        writer2.close();
        writer3.close();

        // Doing a diff between files shows they're identical!

        cout << "Finished writing files" << endl;
    }


    static void convertor() {
        string filenameIn("/dev/shm/hipoTest1.evio");
        string filenameOut("/dev/shm/hipoTestOut.evio");
        try {
            Reader reader(filenameIn);
            uint32_t nevents = reader.getEventCount();
            string userHeader("File is written with new version=6 format");
            Writer writer(filenameOut, ByteOrder::ENDIAN_LITTLE, 10000, 8*1024*1024);
            writer.setCompressionType(Compressor::LZ4);

            cout << " OPENED FILE EVENT COUNT = " << nevents << endl;

            for(int i = 1; i < nevents; i++){
                shared_ptr<uint8_t> pEvent = reader.getEvent(i);
                uint32_t eventLen = reader.getEventLength(i);
                writer.addEvent(pEvent.get(), 0, eventLen);
                cout << " EVENT # " << i << "  size = " << eventLen << endl;
            }
            writer.close();
        } catch (HipoException & ex) {
            cout << ex.what() << endl;
        }

    }





};



int main(int argc, char **argv) {

//    const char ascii[] = {67,79,68,65}; // CODA in ascii
//    std::string str = std::string(ascii, 4);
//    cout << "This string = " << str << endl;

cout << "Try writing to file /dev/shm/hipoTest1.evio" << endl;
    ReadWriteTest::testStreamRecord();
cout << "Try converting from file /dev/shm/hipoTest1.evio" << endl;
    ReadWriteTest::convertor();
cout << "DONE" << endl;

    return 0;
}




#endif //EVIO_6_0_READWRITETEST_H
