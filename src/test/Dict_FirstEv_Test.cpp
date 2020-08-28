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


    class Tester {

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

        std::string dictionary;


        Tester() {

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

            std::stringstream ss;

            ss << "<xmlDict>\n" <<
               "  <bank name=\"HallD\"             tag=\"6-8\"  type=\"bank\" >\n" <<
               "      <description format=\"New Format\" >hall_d_tag_range</description>\n" <<
               "      <bank name=\"DC(%t)\"        tag=\"6\" num=\"4\" >\n" <<
               "          <leaf name=\"xpos(%n)\"  tag=\"6\" num=\"5\" />\n" <<
               "          <bank name=\"ypos(%n)\"  tag=\"6\" num=\"6\" />\n" <<
               "      </bank >\n" <<
               "      <bank name=\"TOF\"     tag=\"8\" num=\"0\" >\n" <<
               "          <leaf name=\"x\"   tag=\"8\" num=\"1\" />\n" <<
               "          <bank name=\"y\"   tag=\"8\" num=\"2\" />\n" <<
               "      </bank >\n" <<
               "      <bank name=\"BCAL\"      tag=\"7\" >\n" <<
               "          <leaf name=\"x(%n)\" tag=\"7\" num=\"1-3\" />\n" <<
               "      </bank >\n" <<
               "  </bank >\n" <<
               "  <dictEntry name=\"JUNK\" tag=\"5\" num=\"0\" />\n" <<
               "  <dictEntry name=\"SEG5\" tag=\"5\" >\n" <<
               "       <description format=\"Old Format\" >tag 5 description</description>\n" <<
               "  </dictEntry>\n" <<
               "  <bank name=\"Rangy\" tag=\"75 - 78\" >\n" <<
               "      <leaf name=\"BigTag\" tag=\"76\" />\n" <<
               "  </bank >\n" <<
               "</xmlDict>";

            dictionary = ss.str();

            std::cout << "Const: dictionary = " << dictionary << std::endl;


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


        /** Writing to a buffer using new interface. */
        void createCompactEvents(uint16_t tag, uint8_t num) {

            try {

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

                // Make this call to set proper pos & lim
                buffer = builder.getBuffer();

                if (!writeFileName1.empty()) {
                    EventWriter writer(writeFileName1, dictionary, ByteOrder::ENDIAN_LOCAL, false);
                    writer.setFirstEvent(buffer);
                        writer.writeEvent(buffer);
                    writer.close();

                    // Read event back out of file
                    EvioReader reader(writeFileName1);

                    std::cout << "createCompactEvents: have dictionary? " << reader.hasDictionaryXML() << std::endl;
                    std::string xmlDict = reader.getDictionaryXML();
                    std::cout << "createCompactEvents: read dictionary ->\n\n" << xmlDict << std::endl << std::endl;

                    std::cout << "createCompactEvents: have first event? " << reader.hasFirstEvent() << std::endl;
                    auto fe = reader.getFirstEvent();
                    std::cout << "createCompactEvents: read first event ->\n\n" << fe->toString() << std::endl << std::endl;

                    std::cout << "createCompactEvents: try getting ev from file" << std::endl;
                    auto ev = reader.parseEvent(1);
                    std::cout << "createCompactEvents: event ->\n" << ev->treeToString("") << std::endl;
                    // This sets the proper pos and lim in buffer
                    auto bb = builder.getBuffer();
                    std::cout << "createCompactEvents: buffer = \n" << bb->toString() << std::endl;
                }

            }
            catch (EvioException &e) {
                std::cout << e.what() << std::endl;
            }
        }


        /** Writing to a buffer using original evio interface. */
        void createObjectEvents(uint16_t tag, uint8_t num) {

            try {

                // Build event (bank of banks) with EventBuilder object
                EventBuilder builder(tag, DataType::BANK, num);
                std::shared_ptr<EvioEvent> event = builder.getEvent();

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


                // bank of banks
                auto bankBanks2 = EvioBank::getInstance(tag + 100, DataType::BANK, num + 100);
                builder.addChild(bankBanks, bankBanks2);

                // bank of shorts
                auto bankShorts2 = EvioBank::getInstance(tag + 104, DataType::USHORT16, num + 104);
                auto &sData2 = bankShorts2->getUShortData();
                sData2.insert(sData2.begin(), shortVec.begin(), shortVec.end());
                bankShorts2->updateUShortData();
                builder.addChild(bankBanks2, bankShorts2);


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
                tstData.insert(tstData.begin(), stringsVec.begin(), stringsVec.end());
                tsegStrings->updateStringData();
                builder.addChild(bankTsegs, tsegStrings);

                // Remove middle bank
                if (true) {
                    std::cout << "    createObjectEvents:removing banks of segs" << std::endl;
                    builder.remove(bankSegs);
                }

                std::cout << "Event:\n" << event->treeToString("") << std::endl;
                std::cout << "Event Header:\n" << event->getHeader()->toString() << std::endl;

                // Take event & write it into buffer
                EventWriter writer(writeFileName1, dictionary, ByteOrder::ENDIAN_LOCAL, false);
                std::cout << "    createObjectEvents: set First Event, size = ? " << event->getTotalBytes() << std::endl;

                writer.setFirstEvent(event);
                writer.writeEvent(event);
                writer.close();

                // Read event back out of file
                EvioReader reader(writeFileName1);

                std::cout << "    createObjectEvents: have dictionary? " << reader.hasDictionaryXML() << std::endl;
                std::string xmlDict = reader.getDictionaryXML();
                std::cout << "    createObjectEvents: read dictionary ->\n\n" << xmlDict << std::endl << std::endl;

                std::cout << "    createObjectEvents: have first event? " << reader.hasFirstEvent() << std::endl;
                auto fe = reader.getFirstEvent();
                std::cout << "    createObjectEvents: read first event ->\n\n" << fe->treeToString("") << std::endl << std::endl;

                std::cout << "    createObjectEvents: try getting ev #1" << std::endl;
                auto ev = reader.parseEvent(1);
                std::cout << "    createObjectEvents: event ->\n" << ev->treeToString("") << std::endl;


            }
            catch (EvioException &e) {
                std::cout << e.what() << std::endl;
            }

        }


    };
}




int main(int argc, char **argv) {
    evio::Tester tester;
    //tester.createCompactEvents(1,1);
    tester.createObjectEvents(1,1);
    return 0;
}


