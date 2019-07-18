//
// Created by Carl Timmer on 2019-07-05.
//

#ifndef EVIO_6_0_READWRITETEST_H
#define EVIO_6_0_READWRITETEST_H


#include <cstdint>
#include <chrono>
#include "ByteBuffer.h"
#include "Writer.h"
#include "WriterMT.h"
#include "HipoException.h"

class ReadWriteTest {



public:


    static uint8_t * generateBuffer(int size){
        auto buffer = new uint8_t[size];
        for(int i = 0; i < size; i++){
            buffer[i] = i;
        }
        return buffer;
    }



    static void testStreamRecord() {

        // Variables to track record build rate
        double freqAvg;
        long totalC=0;
        // Ignore the first N values found for freq in order
        // to get better avg statistics. Since the JIT compiler in java
        // takes some time to analyze & compile code, freq may initially be low.
        long ignore = 10000;
        long loops  = 2000000;

        // Create file
        auto writer = Writer();
        writer.getRecordHeader().setCompressionType(Compressor::CompressionType);
        writer.open("/daqfs/home/timmer/exampleFile.v6.evio");


        uint8_t* buffer = generateBuffer(400);

        auto t1 = std::chrono::high_resolution_clock::now();

        while (true) {
            // random data array
            writer.addEvent(buffer);

//System.out.println(""+ (20000000 - loops));
            // Ignore beginning loops to remove JIT compile time
            if (ignore-- > 0) {
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
//        RecordOutputStream myRecord = new RecordOutputStream(writer.getByteOrder());
//        buffer = TestWriter.generateBuffer(200);
//        myRecord.addEvent(buffer);
//        myRecord.addEvent(buffer);
//        writer.writeRecord(myRecord);

//        writer.addTrailer(true);
//        writer.addTrailerWithIndex(true);

        writer.close();

        cout << "Finished writing file" << endl;


    }


    static void testStreamRecordMT(){

        // Variables to track record build rate
        double freqAvg;
        long totalC=0;
        // Ignore the first N values found for freq in order
        // to get better avg statistics. Since the JIT compiler in java
        // takes some time to analyze & compile code, freq may initially be low.
        long ignore = 0;
        long loops  = 6;

        string fileName = "/daqfs/home/timmer/exampleFile.v6.evio";

        // Create files
        string finalFilename = fileName + ".1";
        WriterMT writer1 = WriterMT(finalFilename, ByteOrder::ENDIAN_LITTLE, 0, 0, Compressor::LZ4, 1);
        finalFilename = fileName + ".2";
        WriterMT writer2 = WriterMT(finalFilename, ByteOrder::ENDIAN_LITTLE, 0, 0, Compressor::LZ4, 2);
        finalFilename = fileName + ".3";
        WriterMT writer3 = WriterMT(finalFilename, ByteOrder::ENDIAN_LITTLE, 0, 0, Compressor::LZ4, 3);

        uint8_t* buffer = generateBuffer(400);

        auto t1 = std::chrono::high_resolution_clock::now();

        while (true) {
            // random data array
            writer1.addEvent(buffer);
            writer2.addEvent(buffer);
            writer3.addEvent(buffer);

//System.out.println(""+ (20000000 - loops));
            // Ignore beginning loops to remove JIT compile time
            if (ignore-- > 0) {
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
        string filename = "/Users/gavalian/Work/Software/project-1a.0.0/clas_000810.evio.324";
        try {
            EvioCompactReader  reader = new EvioCompactReader(filename);
            int nevents = reader.getEventCount();
            string userHeader = "File is written with new version=6 format";
            Writer writer = Writer("converted_000810.evio",ByteOrder::ENDIAN_LITTLE, 10000, 8*1024*1024);
            writer.setCompressionType(Compressor::LZ4);

            System.out.println(" OPENED FILE EVENT COUNT = " + nevents);

            auto myHeader = new uint8_t[233];
            ByteBuffer header = ByteBuffer.wrap(myHeader);
            //nevents = 560;
            for(int i = 1; i < nevents; i++){
                ByteBuffer buffer = reader.getEventBuffer(i,true);
                writer.addEvent(buffer.array());
                //String data = DataUtils.getStringArray(buffer, 10, 30);
                //System.out.println(data);
                //if(i>10) break;
                //System.out.println(" EVENT # " + i + "  size = " + buffer.array().length );
            }
            writer.close();
        } catch (HipoException ex) {
            //Logger.getLogger(TestWriter.class.getName()).log(Level.SEVERE, null, ex);
        }

    }


    static int main(int argc, char **argv) {

        testStreamRecordMT();

    }





};


#endif //EVIO_6_0_READWRITETEST_H
