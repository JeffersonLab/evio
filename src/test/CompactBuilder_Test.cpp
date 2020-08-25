//
// Created by timmer on 2/19/20.
//

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


#include <string>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <memory>
#include <limits>
#include <unistd.h>


#include "evio.h"


using namespace std;


namespace evio {


    class CompactBuilderTest {

    public:

        uint32_t *int1;
        uint8_t *byte1;
        uint16_t *short1;
        uint64_t *long1;
        float *float1;
        double *double1;

        std::vector<uint32_t> intVec;
        std::vector<uint8_t> byteVec;
        std::vector<uint16_t> shortVec;
        std::vector<uint64_t> longVec;
        std::vector<float> floatVec;
        std::vector<double> doubleVec;
        std::vector<std::string> stringsVec;

        int runLoops = 1;
        int bufferLoops = 1;
        int dataElementCount = 3;
        int skip = 0;
        int bufSize = 200000;

        bool oldEvio = false;
        bool useBuf = false;

        std::shared_ptr<ByteBuffer> buffer;

        // files for input & output
        std::string writeFileName1 = "./compactEvioBuild.ev";
        std::string writeFileName0 = "./compactEvioBuildOld.ev";
        std::string writeFileName2 = "./rawEvioStructure.ev";

        ByteOrder order{ByteOrder::ENDIAN_LOCAL};


        CompactBuilderTest() {

            uint16_t tag = 1;
            uint8_t num = 1;
            buffer = std::make_shared<ByteBuffer>(bufSize);
            buffer->order(order);


            std::cout << "Running with:" << std::endl;
            std::cout << " data elements = " << dataElementCount << std::endl;
            std::cout << "       bufSize = " << bufSize << std::endl;
            std::cout << "         loops = " << bufferLoops << std::endl;
            std::cout << "          runs = " << runLoops << std::endl;
            std::cout << "        useBuf = " << useBuf << std::endl;
            std::cout << "      old evio = " << oldEvio << std::endl;


            setDataSize(dataElementCount);

            //            if (oldEvio) {
            //                createObjectEvents(tag, num);
            //            }
            //            else {
            //                createCompactEvents(tag, num);
            //            }
        }


        void setDataSize(int elementCount) {

            int1 = new uint32_t[elementCount];
            byte1 = new uint8_t[elementCount];
            short1 = new uint16_t[elementCount];
            long1 = new uint64_t[elementCount];
            float1 = new float[elementCount];
            double1 = new double[elementCount];

            intVec.reserve(elementCount);
            byteVec.reserve(elementCount);
            shortVec.reserve(elementCount);
            longVec.reserve(elementCount);
            floatVec.reserve(elementCount);
            doubleVec.reserve(elementCount);
            stringsVec.reserve(elementCount);

            for (int i = 0; i < elementCount; i++) {
                int1[i] = i + 1;
                byte1[i] = (uint8_t) ((i + 1) % numeric_limits<uint8_t>::max());
                short1[i] = (uint16_t) ((i + 1) % numeric_limits<uint16_t>::max());
                long1[i] = i + 1;
                float1[i] = (float) (i + 1);
                double1[i] = (double) (i + 1);

                intVec.push_back(i + 1);
                byteVec.push_back((uint8_t) ((i + 1) % numeric_limits<uint8_t>::max()));
                shortVec.push_back((uint16_t) ((i + 1) % numeric_limits<uint16_t>::max()));
                longVec.push_back(i + 1);
                floatVec.push_back(static_cast<float>(i + 1));
                doubleVec.push_back(static_cast<double>(i + 1));

                stringsVec.push_back("0x" + std::to_string(i + 1));
            }
        }

        // Search the buffer created by createObjectEvents
        std::shared_ptr<EvioNode> searchBuffer(uint16_t tag, uint8_t num) {

            std:
            vector<std::shared_ptr<EvioNode>> returnList;
            std::shared_ptr<EvioNode> node = nullptr;

            try {
                std::cout << "searchBuffer: write previously created event (in buffer)" << std::endl;
                std::cout << "            : buffer = \n" << buffer->toString() << std::endl;
                auto writeBuf = std::make_shared<ByteBuffer>(20000);
                EventWriter writer(writeBuf);
                writer.writeEvent(buffer);
                writer.close();
                writeBuf = writer.getByteBuffer();

                std::cout << "searchBuffer: create compact reader to read newly created writeBuf" << std::endl;
                EvioCompactReader reader(writeBuf);

//                std::shared_ptr<EvioNode> evNode = reader.getEvent(1);
//                std::cout << "\nEv node = " << evNode->toString() << std::endl;
//                std::cout << "   allNodes size = " << evNode->getAllNodes().size() << std::endl << std::endl;

                std::shared_ptr<EvioNode> evScannedNode = reader.getScannedEvent(1);
                std::cout << "\nEv scanned node = " << evScannedNode->toString() << std::endl;
                std::cout << "   allNodes size = " << evScannedNode->getAllNodes().size() << std::endl << std::endl;

                // search event #1 for struct with tag, num
                std::cout << "searchBuffer: search event #1" << std::endl;
                reader.searchEvent(1, tag, num, returnList);
                if (returnList.size() < 1) {
                    std::cout << "GOT NOTHING IN SEARCH for ev 1, tag = " << tag << ", num = " << +num << std::endl;
                    return nullptr;
                }
                else {
                    std::cout << "Found " << returnList.size() << " structs" << std::endl;
                    for (auto node : returnList) {
                        std::cout << "NODE: " << node->toString() << std::endl << std::endl;
                    }
                }

                // Match only tags, not num
                std::cout << "searchBuffer: create Regular reader to read newly created writeBuf" << std::endl;
                EvioReader reader2(writeBuf);
                auto event = reader2.parseEvent(1);
                std::vector<std::shared_ptr<BaseStructure>> vec;
                tag = 41;
                std::cout << "searchBuffer: get matching struct for tag = " << tag << std::endl;
                StructureFinder::getMatchingStructures(event, tag, vec);
                if (vec.size() < 1) {
                    std::cout << "GOT NOTHING IN SEARCH for ev 1, tag = " << tag << std::endl;
                    return nullptr;
                }
                else {
                    std::cout << "Using StructureFinder , found " << vec.size() << " structs" << std::endl;
                    for (auto ev : vec) {
                        std::cout << "Struct: " << ev->toString() << std::endl << std::endl;
                    }
                }


            }
            catch (EvioException &e) {
                std::cout << e.what() << std::endl;
            }
            catch (std::exception &e) {
                std::cout << e.what() << std::endl;
            }

            return nullptr;
        }


        /** Writing to a buffer using new interface. */
        void insertEvioNode(std::shared_ptr<EvioNode> node, uint16_t tag, uint8_t num, bool useBuf) {
            try {
                size_t total = 0L;
                auto start = chrono::high_resolution_clock::now();

                for (int j = 0; j < runLoops; j++) {
                    auto t1 = chrono::high_resolution_clock::now();

                    for (int i = 0; i < bufferLoops; i++) {
                        CompactEventBuilder builder(buffer);

                        // add top/event level bank of banks
                        builder.openBank(tag, DataType::BANK, num);

                        builder.addEvioNode(node);

                        builder.closeAll();

                        if (i == 0 && !writeFileName1.empty()) {
                            std::cout << "insertEvioNode: write new event to file= " << writeFileName1 << std::endl;
                            builder.toFile(writeFileName1);
                        }
                    }

                    auto t2 = chrono::high_resolution_clock::now();
                    auto duration = chrono::duration_cast<chrono::milliseconds>(t2 - t1);
                    cout << duration.count() << endl;
                    std::cout << "Time = " << duration.count() << " milliseconds" << std::endl;

                    if (j >= skip) {
                        auto end = chrono::high_resolution_clock::now();
                        auto dif = chrono::duration_cast<chrono::milliseconds>(end - start);
                        cout << dif.count() << endl;
                        std::cout << "Total Time = " << dif.count() << " milliseconds" << std::endl;

                        total += dif.count();
                    }
                }

                std::cout << "Avg Time = " << (total / (runLoops - skip)) << " milliseconds" << std::endl;
                std::cout << "runs used = " << (runLoops - skip) << std::endl;
            }
            catch (EvioException &e) {
                std::cout << e.what() << std::endl;
            }
        }


        /** Writing to a buffer using new interface. */
        void createCompactEvents(uint16_t tag, uint8_t num) {
            try {

                for (int j = 0; j < runLoops; j++) {
                    auto t1 = chrono::high_resolution_clock::now();

                    for (int i = 0; i < bufferLoops; i++) {

                        CompactEventBuilder builder(buffer);

                        // add top/event level bank of banks
                        builder.openBank(tag, DataType::BANK, num);

                        // add bank of banks
                        builder.openBank(tag + 1, DataType::BANK, num + 1);

                        // add bank of ints
                        builder.openBank(tag + 2, DataType::UINT32, num + 2);
                        builder.addIntData(int1, dataElementCount);
                        builder.closeStructure();

                        // add bank of bytes
                        builder.openBank(tag + 3, DataType::UCHAR8, num + 3);
                        builder.addByteData(byte1, dataElementCount);
                        builder.closeStructure();

                        // add bank of shorts
                        builder.openBank(tag + 4, DataType::USHORT16, num + 4);
                        builder.addShortData(short1, dataElementCount);
                        builder.closeStructure();

                        // add bank of longs
                        builder.openBank(tag + 40, DataType::ULONG64, num + 40);
                        builder.addLongData(long1, dataElementCount);
                        builder.closeStructure();

                        // add bank of floats
                        builder.openBank(tag + 5, DataType::FLOAT32, num + 5);
                        builder.addFloatData(float1, dataElementCount);
                        builder.closeStructure();

                        // add bank of doubles
                        builder.openBank(tag + 6, DataType::DOUBLE64, num + 6);
                        builder.addDoubleData(double1, dataElementCount);
                        builder.closeStructure();

                        // add bank of strings
                        builder.openBank(tag + 7, DataType::CHARSTAR8, num + 7);
                        builder.addStringData(stringsVec);
                        builder.closeStructure();

                        builder.closeStructure();


                        // add bank of segs
                        builder.openBank(tag + 14, DataType::SEGMENT, num + 14);

                        // add seg of ints
                        builder.openSegment(tag + 8, DataType::INT32);
                        builder.addIntData(int1, dataElementCount);
                        builder.closeStructure();

                        // add seg of bytes
                        builder.openSegment(tag + 9, DataType::CHAR8);
                        builder.addByteData(byte1, dataElementCount);
                        builder.closeStructure();

                        // add seg of shorts
                        builder.openSegment(tag + 10, DataType::SHORT16);
                        builder.addShortData(short1, dataElementCount);
                        builder.closeStructure();

                        // add seg of longs
                        builder.openSegment(tag + 40, DataType::LONG64);
                        builder.addLongData(long1, dataElementCount);
                        builder.closeStructure();

                        // add seg of floats
                        builder.openSegment(tag + 11, DataType::FLOAT32);
                        builder.addFloatData(float1, dataElementCount);
                        builder.closeStructure();

                        // add seg of doubles
                        builder.openSegment(tag + 12, DataType::DOUBLE64);
                        builder.addDoubleData(double1, dataElementCount);
                        builder.closeStructure();

                        // add seg of strings
                        builder.openSegment(tag + 13, DataType::CHARSTAR8);
                        builder.addStringData(stringsVec);
                        builder.closeStructure();

                        builder.closeStructure();


                        // add bank of tagsegs
                        builder.openBank(tag + 15, DataType::TAGSEGMENT, num + 15);

                        // add tagseg of ints
                        builder.openTagSegment(tag + 16, DataType::UINT32);
                        builder.addIntData(int1, dataElementCount);
                        builder.closeStructure();

                        // add tagseg of bytes
                        builder.openTagSegment(tag + 17, DataType::UCHAR8);
                        builder.addByteData(byte1, dataElementCount);
                        builder.closeStructure();

                        // add tagseg of shorts
                        builder.openTagSegment(tag + 18, DataType::USHORT16);
                        builder.addShortData(short1, dataElementCount);
                        builder.closeStructure();

                        // add tagseg of longs
                        builder.openTagSegment(tag + 40, DataType::ULONG64);
                        builder.addLongData(long1, dataElementCount);
                        builder.closeStructure();

                        // add tagseg of floats
                        builder.openTagSegment(tag + 19, DataType::FLOAT32);
                        builder.addFloatData(float1, dataElementCount);
                        builder.closeStructure();

                        // add tagseg of doubles
                        builder.openTagSegment(tag + 20, DataType::DOUBLE64);
                        builder.addDoubleData(double1, dataElementCount);
                        builder.closeStructure();

                        // add tagseg of strings
                        builder.openTagSegment(tag + 21, DataType::CHARSTAR8);
                        builder.addStringData(stringsVec);
                        builder.closeStructure();

                        builder.closeAll();

                        if (i == 0 && !writeFileName1.empty()) {
                            builder.toFile(writeFileName1);
                            // buffer is left in a readable state after above method

                            // Read event back out of file
                            EvioReader reader(writeFileName1);
                            std::cout << "createCompactEvents: try getting first ev from file: j = " << j << ", i = " << i << std::endl;
                            auto ev = reader.parseEvent(1);
                            std::cout << "createCompactEvents: event ->\n" << ev->treeToString("") << std::endl;
                            // This sets the proper pos and lim in buffer
                            auto bb = builder.getBuffer();
                            std::cout << "createCompactEvents: buffer = \n" << bb->toString() << std::endl;
                        }
                    }

                    auto t2 = chrono::high_resolution_clock::now();
                    auto duration = chrono::duration_cast<chrono::milliseconds>(t2 - t1);
                    std::cout << "createCompactEvents: time = " << duration.count() << " milliseconds" << std::endl;
                }

            }
            catch (EvioException &e) {
                std::cout << e.what() << std::endl;
            }
        }


        /** Writing to a buffer using original evio interface. */
        void createObjectEvents(uint16_t tag, uint8_t num) {

            try {

                for (int j = 0; j < runLoops; j++) {
                    auto t1 = chrono::high_resolution_clock::now();

                    for (int i = 0; i < bufferLoops; i++) {

                        // Build event (bank of banks) with EventBuilder object
                        EventBuilder builder(tag, DataType::BANK, num);
                        auto event = builder.getEvent();

                        // bank of banks
                        auto bankBanks = EvioBank::getInstance(tag + 1, DataType::BANK, num + 1);
                        builder.addChild(event, bankBanks);

                        // bank of ints
                        auto bankInts = EvioBank::getInstance(tag + 2, DataType::UINT32, num + 2);
                        auto &iData = bankInts->getUIntData();
                        iData.insert(iData.begin(), intVec.begin(), intVec.end());
                        bankInts->updateUIntData();
                        builder.addChild(bankBanks, bankInts);

                        // bank of bytes
                        auto bankBytes = EvioBank::getInstance(tag + 3, DataType::UCHAR8, num + 3);
                        auto &cData = bankBytes->getUCharData();
                        cData.insert(cData.begin(), byteVec.begin(), byteVec.end());
                        bankBytes->updateUCharData();
                        builder.addChild(bankBanks, bankBytes);

                        // bank of shorts
                        auto bankShorts = EvioBank::getInstance(tag + 4, DataType::USHORT16, num + 4);
                        auto &sData = bankShorts->getUShortData();
                        sData.insert(sData.begin(), shortVec.begin(), shortVec.end());
                        bankShorts->updateUShortData();
                        builder.addChild(bankBanks, bankShorts);

                        // bank of longs
                        auto bankLongs = EvioBank::getInstance(tag + 40, DataType::ULONG64, num + 40);
                        auto &lData = bankLongs->getULongData();
                        lData.insert(lData.begin(), longVec.begin(), longVec.end());
                        bankLongs->updateULongData();
                        builder.addChild(bankBanks, bankLongs);

                        // bank of floats
                        auto bankFloats = EvioBank::getInstance(tag + 5, DataType::FLOAT32, num + 5);
                        auto &fData = bankFloats->getFloatData();
                        fData.insert(fData.begin(), floatVec.begin(), floatVec.end());
                        bankFloats->updateFloatData();
                        builder.addChild(bankBanks, bankFloats);

                        // bank of doubles
                        auto bankDoubles = EvioBank::getInstance(tag + 6, DataType::DOUBLE64, num + 6);
                        auto &dData = bankDoubles->getDoubleData();
                        dData.insert(dData.begin(), doubleVec.begin(), doubleVec.end());
                        bankDoubles->updateDoubleData();
                        builder.addChild(bankBanks, bankDoubles);

                        // bank of strings
                        auto bankStrings = EvioBank::getInstance(tag + 7, DataType::CHARSTAR8, num + 7);
                        auto &stData = bankStrings->getStringData();
                        stData.insert(stData.begin(), stringsVec.begin(), stringsVec.end());
                        bankStrings->updateStringData();
                        builder.addChild(bankBanks, bankStrings);



                        // bank of segments
                        auto bankSegs = EvioBank::getInstance(tag + 14, DataType::SEGMENT, num + 14);
                        builder.addChild(event, bankSegs);

                        // seg of ints
                        auto segInts = EvioSegment::getInstance(tag + 8, DataType::INT32);
                        auto &siData = segInts->getIntData();
                        siData.insert(siData.begin(), intVec.begin(), intVec.end());
                        segInts->updateIntData();
                        builder.addChild(bankSegs, segInts);

                        // seg of bytes
                        auto segBytes = EvioSegment::getInstance(tag + 9, DataType::CHAR8);
                        auto &scData = segBytes->getCharData();
                        scData.insert(scData.begin(), byteVec.begin(), byteVec.end());
                        segBytes->updateCharData();
                        builder.addChild(bankSegs, segBytes);

                        // seg of shorts
                        auto segShorts = EvioSegment::getInstance(tag + 10, DataType::SHORT16);
                        auto &ssData = segShorts->getShortData();
                        ssData.insert(ssData.begin(), shortVec.begin(), shortVec.end());
                        segShorts->updateShortData();
                        builder.addChild(bankSegs, segShorts);

                        // seg of longs
                        auto segLongs = EvioSegment::getInstance(tag + 40, DataType::LONG64);
                        auto &slData = segLongs->getLongData();
                        slData.insert(slData.begin(), longVec.begin(), longVec.end());
                        segLongs->updateLongData();
                        builder.addChild(bankSegs, segLongs);

                        // seg of floats
                        auto segFloats = EvioSegment::getInstance(tag + 11, DataType::FLOAT32);
                        auto &sfData = segFloats->getFloatData();
                        sfData.insert(sfData.begin(), floatVec.begin(), floatVec.end());
                        segFloats->updateFloatData();
                        builder.addChild(bankSegs, segFloats);

                        // seg of doubles
                        auto segDoubles = EvioSegment::getInstance(tag + 12, DataType::DOUBLE64);
                        auto &sdData = segDoubles->getDoubleData();
                        sdData.insert(sdData.begin(), doubleVec.begin(), doubleVec.end());
                        segDoubles->updateDoubleData();
                        builder.addChild(bankSegs, segDoubles);

                        // seg of strings
                        auto segStrings = EvioSegment::getInstance(tag + 13, DataType::CHARSTAR8);
                        auto &sstData = segStrings->getStringData();
                        sstData.insert(sstData.begin(), stringsVec.begin(), stringsVec.end());
                        segStrings->updateStringData();
                        builder.addChild(bankSegs, segStrings);



                        // bank of tagsegments
                        auto bankTsegs = EvioBank::getInstance(tag + 15, DataType::TAGSEGMENT, num + 15);
                        builder.addChild(event, bankTsegs);

                        // tagsegments of ints
                        auto tsegInts = EvioTagSegment::getInstance(tag + 16, DataType::UINT32);
                        auto &tiData = tsegInts->getUIntData();
                        tiData.insert(tiData.begin(), intVec.begin(), intVec.end());
                        tsegInts->updateUIntData();
                        builder.addChild(bankTsegs, tsegInts);

                        // tagsegments of bytes
                        auto tsegBytes = EvioTagSegment::getInstance(tag + 17, DataType::UCHAR8);
                        auto &tcData = tsegBytes->getUCharData();
                        tcData.insert(tcData.begin(), byteVec.begin(), byteVec.end());
                        tsegBytes->updateUCharData();
                        builder.addChild(bankTsegs, tsegBytes);

                        // tagsegments of shorts
                        auto tsegShorts = EvioTagSegment::getInstance(tag + 18, DataType::USHORT16);
                        auto &tsData = tsegShorts->getUShortData();
                        tsData.insert(tsData.begin(), shortVec.begin(), shortVec.end());
                        tsegShorts->updateUShortData();
                        builder.addChild(bankTsegs, tsegShorts);

                        // tagsegments of longs
                        auto tsegLongs = EvioTagSegment::getInstance(tag + 40, DataType::ULONG64);
                        auto &tlData = tsegLongs->getULongData();
                        tlData.insert(tlData.begin(), longVec.begin(), longVec.end());
                        tsegLongs->updateULongData();
                        builder.addChild(bankTsegs, tsegLongs);

                        // tagsegments of floats
                        auto tsegFloats = EvioTagSegment::getInstance(tag + 19, DataType::FLOAT32);
                        auto &tfData = tsegFloats->getFloatData();
                        tfData.insert(tfData.begin(), floatVec.begin(), floatVec.end());
                        tsegFloats->updateFloatData();
                        builder.addChild(bankTsegs, tsegFloats);

                        // tagsegments of doubles
                        auto tsegDoubles = EvioTagSegment::getInstance(tag + 20, DataType::DOUBLE64);
                        auto &tdData = tsegDoubles->getDoubleData();
                        tdData.insert(tdData.begin(), doubleVec.begin(), doubleVec.end());
                        tsegDoubles->updateDoubleData();
                        builder.addChild(bankTsegs, tsegDoubles);

                        // tagsegments of strings
                        auto tsegStrings = EvioTagSegment::getInstance(tag + 21, DataType::CHARSTAR8);
                        auto &tstData = tsegStrings->getStringData();
                        std::cout << "# strings in tagseg = " << tstData.size() << std::endl;
                        tstData.insert(tstData.begin(), stringsVec.begin(), stringsVec.end());
                        std::cout << "# strings in tagseg after insert = " << tstData.size() << std::endl;
                        tsegStrings->updateStringData();
                        builder.addChild(bankTsegs, tsegStrings);

                        std::cout << "Event:\n" << event->treeToString("") << std::endl;

                        // Take event & write it into buffer
                        event->write(*(buffer.get()));
                        buffer->flip();
                    }

                    std::cout << "createObjectEvents: buffer -> \n" << buffer->toString() << std::endl;

                    auto t2 = chrono::high_resolution_clock::now();
                    auto duration = chrono::duration_cast<chrono::milliseconds>(t2 - t1);
                    cout << duration.count() << endl;
                    std::cout << "Time = " << duration.count() << " milliseconds" << std::endl;
                }
            }
            catch (EvioException &e) {
                std::cout << e.what() << std::endl;
            }
        }


        // Test CompactEventBuilder class
        void CompactEBTest() {

            //---------------------------
            // Test CompactEventBuilder:
            //---------------------------

            size_t bufSize = 1000;
            CompactEventBuilder ceb(bufSize, ByteOrder::ENDIAN_LOCAL, true);
            //explicit CompactEventBuilder(std::shared_ptr<ByteBuffer> buffer, bool generateNodes = false);
            ceb.openBank(4, DataType::SEGMENT, 4);
            ceb.openSegment(5, DataType::DOUBLE64);
            double dd[3] = {1.11, 2.22, 3.33};
            ceb.addDoubleData(dd, 3);
            ceb.closeAll();
            auto cebEvbuf = ceb.getBuffer();

            //Util::printBytes(cebEvbuf, 0 , 200, "From CompactEventBuilder");

            // Write into a buffer
            auto newBuf = std::make_shared<ByteBuffer>(1000);
            EventWriter writer(newBuf);
            writer.writeEvent(cebEvbuf);
            writer.close();
            auto writerBuf = writer.getByteBuffer();

            //Util::printBytes(newBuf, 0 , 200, "From EventWriter");

            // Read event back out of buffer
            EvioReader reader(writerBuf);
            auto cebEv = reader.getEvent(1);

            std::cout << "CompactEventBuilder's cebEv:\n" << cebEv->toString() << std::endl;
        }


    };


    /**
     * Print the data from a CompositeData object in a user-friendly form.
     * @param cData CompositeData object
     */
    static void printCompositeDataObject(CompositeData &cData) {

        std::cout << "\n************************\nFormat = " << cData.getFormat() << std::endl << std::endl;
        // Get vectors of data items & their types from composite data object
        auto items = cData.getItems();
        auto types = cData.getTypes();

        // Use these list to print out data of unknown format
        size_t len = items.size();

        for (size_t i = 0; i < len; i++) {
            auto type = types[i];
            std::cout << "type = " << std::setw(9) << type.toString() << std::endl;

            if ((type == DataType::NVALUE || type == DataType::UNKNOWN32 ||
                 type == DataType::UINT32 || type == DataType::INT32)) {
                uint32_t j = items[i].item.ui32;
                std::cout << hex << showbase << j << std::endl;
            }
            else if (type == DataType::LONG64 || type == DataType::ULONG64) {
                uint64_t l = items[i].item.ul64;
                std::cout << hex << showbase << l << std::endl;
            }
            else if (type == DataType::SHORT16 || type == DataType::USHORT16) {
                uint16_t s = items[i].item.us16;
                std::cout << hex << showbase << s << std::endl;
            }
            else if (type == DataType::CHAR8 || type == DataType::UCHAR8) {
                uint8_t b = items[i].item.ub8;
                std::cout << hex << showbase << (char) b << std::endl;
            }
            else if (type == DataType::FLOAT32) {
                float ff = items[i].item.flt;
                std::cout << ff << std::endl;
            }
            else if (type == DataType::DOUBLE64) {
                double dd = items[i].item.dbl;
                std::cout << dd << std::endl;
            }
            else if (type == DataType::CHARSTAR8) {
                auto strs = items[i].strVec;
                for (std::string const &ss : strs) {
                    std::cout << ss << ", ";
                }
                std::cout << std::endl;
            }
        }

    }


    /**
    * Print the data from a CompositeData object in a user-friendly form.
    * @param cData CompositeData object
    */
    static void printCompositeDataObject(std::shared_ptr<CompositeData> cData) {
        printCompositeDataObject(*(cData.get()));
    }

}




int main(int argc, char **argv) {
    evio::CompactBuilderTest tester;
    tester.createCompactEvents(1,1);
    // This call will also write a file which can then be used in the readFileIntoBuffer call following
    //tester.createObjectEvents(1,1);
    auto node = tester.searchBuffer(3, 3);

        //evio::EventBuilderTest();
    return 0;
}


