/**
 * Copyright (c) 2019, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 07/05/2019
 * @author timmer
 */


#include <functional>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <memory>
#include <regex>
#include <experimental/filesystem>

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
#include "EventWriter.h"

using namespace std;
namespace fs = std::experimental::filesystem;


namespace evio {


class ReadWriteTest {


public:


    static string xmlDict;


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


    static std::shared_ptr<ByteBuffer> generateEvioBuffer(ByteOrder & order) {
        // Create an evio bank of ints
        auto evioDataBuf = std::make_shared<ByteBuffer>(20);
        evioDataBuf->order(order);
        evioDataBuf->putInt(4);  // length in words, 5 ints
        evioDataBuf->putInt(0xffd10100);  // 2nd evio header word   (prestart event)
        evioDataBuf->putInt(0x1234);  // time
        evioDataBuf->putInt(0x5);  // run #
        evioDataBuf->putInt(0x6);  // run type
        evioDataBuf->flip();
        Util::printBytes(*(evioDataBuf.get()), 0, 20, "Original buffer");
        return evioDataBuf;
    }


    // Create a fake Evio Event
    static std::shared_ptr<ByteBuffer> generateEvioBuffer(ByteOrder & order, uint32_t dataWords) {

        // Create an evio bank of banks, containing a bank of ints
        auto evioDataBuf = std::make_shared<ByteBuffer>(16 + 4*dataWords);
        evioDataBuf->order(order);
        evioDataBuf->putInt(3+dataWords);  // event length in words

        uint32_t tag  = 0x1234;
        uint32_t type = 0x10;  // contains evio banks
        uint32_t num  = 0x12;
        uint32_t secondWord = tag << 4 | type << 2 | num;

        evioDataBuf->putInt(secondWord);  // 2nd evio header word

        // now put in a bank of ints
        evioDataBuf->putInt(1+dataWords);  // bank of ints length in words
        tag = 0x5678; type = 0x1; num = 0x56;
        secondWord = tag << 4 | type << 2 | num;
        evioDataBuf->putInt(secondWord);  // 2nd evio header word

        // Int data
        for (uint32_t i=0; i < dataWords; i++) {
            evioDataBuf->putInt(i);
        }

        evioDataBuf->flip();
        //Util::printBytes(*(evioDataBuf.get()), 0, 20, "Original buffer");
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
        auto evioDataBuf = generateEvioBuffer(order);
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
        auto evioDataBuf = generateEvioBuffer(order);
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


//    explicit EventWriter(string & filename,
//    const ByteOrder & byteOrder = ByteOrder::nativeOrder(),
//    bool append = false);
//
//    EventWriter(string & file,
//    string & dictionary,
//    const ByteOrder & byteOrder = ByteOrder::nativeOrder(),
//    bool append = false);
//
//    EventWriter(string baseName, const string & directory, const string & runType,
//            uint32_t runNumber, uint64_t split, uint32_t maxRecordSize, uint32_t maxEventCount,
//    const ByteOrder & byteOrder, const string & xmlDictionary, bool overWriteOK,
//    bool append, EvioBank * firstEvent, uint32_t streamId, uint32_t splitNumber,
//    uint32_t splitIncrement, uint32_t streamCount, Compressor::CompressionType compressionType,
//            uint32_t compressionThreads, uint32_t ringSize, uint32_t bufferSize);
//
//    //---------------------------------------------
//    // BUFFER Constructors
//    //---------------------------------------------
//
//    EventWriter(std::shared_ptr<ByteBuffer> & buf);
//    EventWriter(std::shared_ptr<ByteBuffer> & buf, string & xmlDictionary);
//    EventWriter(std::shared_ptr<ByteBuffer> & buf, uint32_t maxRecordSize, uint32_t maxEventCount,
//    const string & xmlDictionary, uint32_t recordNumber, EvioBank *firstEvent,
//            Compressor::CompressionType compressionType);
//


    static void eventWriteFileMT(const string & filename) {

        // Variables to track record build rate
        double freqAvg;
        long totalC = 0;
        long loops = 3;


        string dictionary {
                "<xmlDict>\n"
                "  <bank name=\"HallD\"             tag=\"6-8\"  type=\"bank\" >\n"
                "      <description format=\"New Format\" >hall_d_tag_range</description>\n"
                "      <bank name=\"DC(%t)\"        tag=\"6\" num=\"4\" >\n"
                "          <leaf name=\"xpos(%n)\"  tag=\"6\" num=\"5\" />\n"
                "          <bank name=\"ypos(%n)\"  tag=\"6\" num=\"6\" />\n"
                "      </bank >\n"
                "      <bank name=\"TOF\"     tag=\"8\" num=\"0\" >\n"
                "          <leaf name=\"x\"   tag=\"8\" num=\"1\" />\n"
                "          <bank name=\"y\"   tag=\"8\" num=\"2\" />\n"
                "      </bank >\n"
                "      <bank name=\"BCAL\"      tag=\"7\" >\n"
                "          <leaf name=\"x(%n)\" tag=\"7\" num=\"1-3\" />\n"
                "      </bank >\n"
                "  </bank >\n"
                "  <dictEntry name=\"JUNK\" tag=\"5\" num=\"0\" />\n"
                "  <dictEntry name=\"SEG5\" tag=\"5\" >\n"
                "       <description format=\"Old Format\" >tag 5 description</description>\n"
                "  </dictEntry>\n"
                "  <bank name=\"Rangy\" tag=\"75 - 78\" >\n"
                "      <leaf name=\"BigTag\" tag=\"76\" />\n"
                "  </bank >\n"
                "</xmlDict>\n"
        };

        uint8_t firstEvent[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        uint32_t firstEventLen = 10;
        bool addTrailerIndex = true;
        ByteOrder order = ByteOrder::ENDIAN_LITTLE;
        //Compressor::CompressionType compType = Compressor::GZIP;
        Compressor::CompressionType compType = Compressor::UNCOMPRESSED;

        // Create files
        string directory;
        uint32_t runNum = 123;
        uint64_t split = 2000000; // 2 MB
        uint32_t maxRecordSize = 0; // use default
        uint32_t maxEventCount = 0; // use default
        bool overWriteOK = false;
        bool append = false;
        uint32_t streamId = 1;
        uint32_t splitNumber = 2;
        uint32_t splitIncrement = 1;
        uint32_t streamCount = 1;
        uint32_t compThreads = 1;
        uint32_t ringSize = 16;
        uint32_t bufSize = 1;

        EventWriter writer(filename, directory, "runType",
                           runNum, split, maxRecordSize, maxEventCount,
                           order, dictionary, overWriteOK, append,
                           nullptr, streamId, splitNumber, splitIncrement, streamCount,
                           compType, compThreads, ringSize, bufSize);

        string firstEv = "This is the first event";
        ByteBuffer firstEvBuf(firstEv.size());
        Util::stringToASCII(firstEv, firstEvBuf);
        writer.setFirstEvent(firstEvBuf);

        cout << "Past creating writer" << endl;

        //uint8_t *dataArray = generateSequentialInts(100, order);
        uint8_t *dataArray = generateSequentialShorts(13, order);
        // Calling the following method makes a shared pointer out of dataArray, so don't delete
        ByteBuffer dataBuffer(dataArray, 26);

        // Create an event containing a bank of 100 ints
        auto evioDataBuf = generateEvioBuffer(order, 100);

        // Create node from this buffer
        std::shared_ptr<EvioNode> node = EvioNode::extractEventNode(evioDataBuf,0,0,0);

        auto t1 = std::chrono::high_resolution_clock::now();

        while (true) {
            // event in evio format
            writer.writeEvent(*(evioDataBuf.get()));

            totalC++;

            if (--loops < 1) break;
        }

        //writer.addEvent(*node.get());

        auto t2 = std::chrono::high_resolution_clock::now();
        auto deltaT = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);

        freqAvg = (double) totalC / deltaT.count() * 1000;

        cout << "Time = " << deltaT.count() << " msec,  Hz = " << freqAvg << endl;
        cout << "Finished all loops, count = " << totalC << endl;

        //------------------------------

        cout << "Past write" << endl;

        writer.close();
        cout << "Past close" << endl;

        // Doing a diff between files shows they're identical!

        cout << "Finished writing file " << filename << ", now read it in" << endl;
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

        }


        static void readFile2(string finalFilename) {

            Reader reader1(finalFilename);
            ByteOrder order = reader1.getByteOrder();

            int32_t evCount = reader1.getEventCount();
            cout << "Read in file " << finalFilename << ", got " << evCount << " events" << endl;

            string dict = reader1.getDictionary();
            cout << "   Got dictionary = " << dict << endl;

            shared_ptr<uint8_t> & pFE = reader1.getFirstEvent();
            if (pFE != nullptr) {
                int32_t feBytes = reader1.getFirstEventSize();
                auto feData = reinterpret_cast<char *>(pFE.get());
                string feString(feData, feBytes);

                cout << "First event = " << feString << endl;
            }

            cout << "reader.getEvent(0)" << endl;
            shared_ptr<uint8_t> data = reader1.getEvent(0);
            cout << "got event" << endl;

            uint32_t wordLen = reader1.getEventLength(0)/4;
            if (data != nullptr) {
                uint32_t *pData = reinterpret_cast<uint32_t *>(data.get());
                cout <<  "   Event #0, values =" << endl << "   ";
                for (int i = 0; i < wordLen; i++) {
                    if (order.isLocalEndian()) {
                        cout << *pData << ",  ";
                    }
                    else {
                        cout << SWAP_32(*pData) <<",  ";
                    }
                    pData++;
                    if ((i+1)%5 == 0) cout << endl;
                }
                cout << endl;
            }

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


    string ReadWriteTest::xmlDict {
            "<xmlDict>\n"
            "  <bank name=\"HallD\"             tag=\"6-8\"  type=\"bank\" >\n"
            "      <description format=\"New Format\" >hall_d_tag_range</description>\n"
            "      <bank name=\"DC(%t)\"        tag=\"6\" num=\"4\" >\n"
            "          <leaf name=\"xpos(%n)\"  tag=\"6\" num=\"5\" />\n"
            "          <bank name=\"ypos(%n)\"  tag=\"6\" num=\"6\" />\n"
            "      </bank >\n"
            "      <bank name=\"TOF\"     tag=\"8\" num=\"0\" >\n"
            "          <leaf name=\"x\"   tag=\"8\" num=\"1\" />\n"
            "          <bank name=\"y\"   tag=\"8\" num=\"2\" />\n"
            "      </bank >\n"
            "      <bank name=\"BCAL\"      tag=\"7\" >\n"
            "          <leaf name=\"x(%n)\" tag=\"7\" num=\"1-3\" />\n"
            "      </bank >\n"
            "  </bank >\n"
            "  <dictEntry name=\"JUNK\" tag=\"5\" num=\"0\" />\n"
            "  <dictEntry name=\"SEG5\" tag=\"5\" >\n"
            "       <description format=\"Old Format\" >tag 5 description</description>\n"
            "  </dictEntry>\n"
            "  <bank name=\"Rangy\" tag=\"75 - 78\" >\n"
            "      <leaf name=\"BigTag\" tag=\"76\" />\n"
            "  </bank >\n"
            "</xmlDict>\n"
    };

}


int main(int argc, char **argv) {
    string filename   = "/dev/shm/EventWriterTest.evio";
    cout << endl << "Try writing " << filename << endl;

    // Write files
    evio::ReadWriteTest::eventWriteFileMT(filename);
    cout << endl << "Finished writing, now try reading " << filename << endl;

    // Read files just written
    string readFilename   = "/dev/shm/EventWriterTest.evio.2";
    evio::ReadWriteTest::readFile2(readFilename);
    cout << endl << endl << "----------------------------------------" << endl << endl;

    return 0;
}



int main1(int argc, char **argv) {
    string filename   = "/dev/shm/hipoTest.evio";
    string filenameMT = "/dev/shm/hipoTestMT.evio";
//    string filename   = "/dev/shm/hipoTest-j.evio";
//    string filenameMT = "/dev/shm/hipoTestMT-j.evio";

    // Write files
    evio::ReadWriteTest::writeFile(filename);
    evio::ReadWriteTest::writeFileMT(filenameMT);

    // Read files just written
    evio::ReadWriteTest::readFile(filename);
    cout << endl << endl << "----------------------------------------" << endl << endl;
    evio::ReadWriteTest::readFile(filenameMT);

    return 0;
}


//static void expandEnvironmentalVariables(string & text) {
//    static std::regex env("\\$\\(([^)]+)\\)");
//    std::smatch match;
//    while ( std::regex_search(text, match, env) ) {
//        const char * s = getenv(match[1].str().c_str());
//        const std::string var(s == nullptr ? "" : s);
//        text.replace(match[0].first, match[0].second, var);
//    }
//}
//
//static uint32_t countAndFixIntSpecifiers1(string & text) {
//    static std::regex specifier("%(\\d*)([xd])");
//    uint32_t specifierCount = 0;
//    std::smatch match;
//
//    while ( std::regex_search(text, match, specifier) ) {
//        specifierCount++;
//        cout << "spec count = " << specifierCount << endl;
//        // Make sure any number preceding "x" or "d" starts with a 0 or else
//        // there will be empty spaces in the resulting string (i.e. file name).
//        std::string specWidth = match[1].str();
//        if (specWidth.length() > 0 && specWidth[0] != '0') {
//            cout << "in fix it loop" << endl;
//            text.replace(match[1].first, match[1].second, "0" + specWidth);
//        }
//    }
//
//    return specifierCount;
//}
//
//static uint32_t countAndFixIntSpecifiers(string text, string & returnString) {
//    static std::regex specifier("%(\\d*)([xd])");
//
//    auto begin = std::sregex_iterator(text.begin(), text.end(), specifier);
//    auto end   = std::sregex_iterator();
//    uint32_t specifierCount = std::distance(begin, end);
//    std::cout << "countAndFixIntSpecifiers: Found " << specifierCount << " specs" << endl;
//
//    // Make sure any number preceding "x" or "d" starts with a 0 or else
//    // there will be empty spaces in the resulting string (i.e. file name).
//    // We have to fix, at most, specifierCount number of specs.
//
//    std::sregex_iterator i = begin;
//
//    // Go thru all specifiers in text, only once
//    for (int j = 0; j < specifierCount; j++) {
//        if (j > 0) {
//            // skip over specifiers previously dealt with (text can change each loop)
//            i = std::sregex_iterator(text.begin(), text.end(), specifier);
//            int k=j;
//            while (k-- > 0) i++;
//        }
//
//        std::smatch match = *i;
//        std::string specWidth = match[1].str();
//        if (specWidth.length() > 0 && specWidth[0] != '0') {
//            text.replace(match[1].first, match[1].second, "0" + specWidth);
//        }
//    }
//
//    returnString = text;
//
//    return specifierCount;
//}
//
//
//int main2(int argc, char **argv) {
//
//    // TEST experimental file stuff
//    string fileName = "/daqfs/home/timmer/coda/evio-6.0/README";
//    cout << "orig file name = " << fileName << endl;
//    fs::path currentFilePath(fileName);
//    //currentFile = currentFilePath.toFile();
//    fs::path filePath = currentFilePath.filename();
//    string file_name = filePath.generic_string();
//    cout << "file name from path = " << file_name << endl;
//    cout << "dir  name from path = " << currentFilePath.parent_path() << endl;
//
//    bool fileExists = fs::exists(filePath);
//    bool isRegFile = fs::is_regular_file(filePath);
//
//    cout << "file is really there? = " << fileExists << endl;
//    cout << "file is regular file? = " << isRegFile << endl;
//
//    cout << "file " << currentFilePath.parent_path() << " is dir ? " <<
//                       fs::is_directory(currentFilePath.parent_path()) << endl;
//
//    fs::space_info dirInfo = fs::space(currentFilePath);
//
//    cout << "free space for dir in bytes is " << dirInfo.free << endl;
//    cout << "available space for dir in bytes is " << dirInfo.available << endl;
//    cout << "capacity of file system in bytes is " << dirInfo.capacity << endl;
//
//    cout << "size of file in bytes = " << fs::file_size(filePath) << endl;
//
//    string s  = "myfilename.$(ENV_1).junk$(ENV_2).otherJunk";
//    cout << s << endl << endl;
//
//    regex regExp("\\$\\((.*?)\\)");
//
//
//    // reset string
//    s = "myfilename.$(ENV_1).junk$(ENV_2).otherJunk";
//    regex rep("\\$\\((.*?)\\)");
//    cout << regex_replace(s, rep, "Sub1") << endl;
//
//    s = "myfilename.$(ENV_1).junk$(ENV_2).otherJunk";
//    expandEnvironmentalVariables(s);
//
//    cout << "call function, final string = " << s << endl;
//
//    s = "myfilename.%3d.junk%7x.otherJunk--%0123d---%x--%d";
//    cout << "fix specs, orig string = " << s << endl;
//    string another = s;
//    string returnStr("blah blah");
//    countAndFixIntSpecifiers(another, returnStr);
//    cout << "fix specs, final string = " << returnStr << endl;
//    cout << "fix specs, arg string = " << another << endl;
//
//
//    //fileName = string.format(fileName, runNumber);
//    int runNumber = 123;
//    int splitNumber = 45;
//    int streamId = 6;
//
//    s = "myfilename--%3d--0x%x--%012d--%03d";
//    char *temp = new char[1000];
//    cout << "string = " << s << endl;
//
//    std::sprintf(temp, s.c_str(), runNumber, splitNumber, streamId, streamId, streamId);
//    cout << "sub runNumber temp, string = " << temp << endl;
//
//    string fileName(temp);
//    cout << "string version of temp = " << fileName << endl;
//
//    fileName += "." + std::to_string(streamId) +
//                "." + std::to_string(splitNumber);
//
//    cout << "tack onto string = " << fileName << endl;
//
//
//
//    string a("twoSpecs%03d-----%4d~~~~~");
//    cout << endl << "try remove 2nd os spec into = " << a << endl;
//
//    static std::regex specifier("(%\\d*[xd])");
//    auto begin = std::sregex_iterator(a.begin(), a.end(), specifier);
//
//    // Go to 2nd match
//    begin++;
//
//    std::smatch match = *begin;
//    a.replace(match[0].first, match[0].second, "");
//
////    static std::regex specifier("(%\\d*[xd])");
////    auto begin = std::sregex_iterator(a.begin(), a.end(), specifier);
////    //auto end   = std::sregex_iterator();
////    // uint32_t specifierCount = std::distance(begin, end);
////
////    // Go to 2nd match
////    begin++;
////
////    std::smatch match = *begin;
////    a.replace(match[0].first, match[0].second, "%d." + match.str());
//
//    cout << "final tacked string = " << a << endl;
//
//
//    return 0;
//}
