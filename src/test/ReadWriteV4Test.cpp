/**
 * Copyright (c) 2024, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 07/25/2024
 * @author timmer
 */


#include <string>
#include <cstdint>
#include <chrono>
#include <memory>
#include <regex>
#include <iterator>
#include <fstream>


#include "eviocc.h"


using namespace std;


namespace evio {


    class ReadWriteTest {


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

        int dataElementCount = 3;
        int skip = 0;
        int bufSize = 200000;


        ByteOrder order{ByteOrder::ENDIAN_LOCAL};

        std::string dictionary;


        ReadWriteTest() {

            setDataSize(dataElementCount);
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
            //std::cout << "Dictionary len = " << dictionary.size() << std::endl;
            //std::cout << "Dictionary = \n" << dictionary << std::endl;
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
                byte1[i] = (uint8_t) ((i + 1) % std::numeric_limits<uint8_t>::max());
                short1[i] = (uint16_t) ((i + 1) % std::numeric_limits<uint16_t>::max());
                long1[i] = i + 1;
                float1[i] = (float) (i + 1);
                double1[i] = (double) (i + 1);

                intVec.push_back(i + 1);
                byteVec.push_back((uint8_t) ((i + 1) % std::numeric_limits<uint8_t>::max()));
                shortVec.push_back((uint16_t) ((i + 1) % std::numeric_limits<uint16_t>::max()));
                longVec.push_back(i + 1);
                floatVec.push_back(static_cast<float>(i + 1));
                doubleVec.push_back(static_cast<double>(i + 1));

                stringsVec.push_back("0x" + std::to_string(i + 1));
            }
        }


        // Create a fake Evio Event
        static std::shared_ptr<ByteBuffer> generateEvioBuffer(ByteOrder order, int dataWords) {

            // Create an evio bank of banks, containing a bank of ints
            std::shared_ptr<ByteBuffer> buf = std::make_shared<ByteBuffer>(16 + 4*dataWords);
            buf->order(order);
            buf->putInt(3+dataWords);  // event length in words

            int tag  = 0x1234;
            int type = 0x10;  // contains evio banks
            int num  = 0x12;
            int secondWord = tag << 16 | type << 8 | num;

            buf->putInt(secondWord);  // 2nd evio header word

            // now put in a bank of ints
            buf->putInt(1+dataWords);  // bank of ints length in words
            tag = 0x5678; type = 0x1; num = 0x56;
            secondWord = tag << 16 | type << 8 | num;
            buf->putInt(secondWord);  // 2nd evio header word

            // Int data
            for (int i=0; i < dataWords; i++) {
                buf->putInt(i);
            }

            buf->flip();
            return buf;
        }



        std::shared_ptr<EvioBank> generateEvioBank(ByteOrder & order, uint16_t tag, uint8_t num) {
            // Event, traditional bank of banks
            EventBuilder builder(tag, DataType::BANK, num);
            auto ev = builder.getEvent();

            // add a bank of doubles
            auto bank1 = EvioBank::getInstance(22, DataType::DOUBLE64, 0);
            double dData[1000];
            for (int i = 0; i < 1000; i++) {
                dData[i] = i + 1.;
            }
            builder.appendDoubleData(bank1, dData, 1000);
            cout << "  generate Evio Bank, bank1 len = " << bank1->getTotalBytes() << endl;
            builder.addChild(ev, bank1);
            cout << "  generate Evio Bank, ev len = " << ev->getTotalBytes() << endl;

            return static_cast<std::shared_ptr<EvioBank>>(ev);
        }



        // Create a fake Evio Event
        std::shared_ptr<ByteBuffer> generateEvioBuffer(ByteOrder & order, uint16_t tag, uint8_t num) {

            std::shared_ptr<ByteBuffer> buf = std::make_shared<ByteBuffer>(200000);
            buf->order(order);
            CompactEventBuilder builder(buf);

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

            // add bank of banks
            builder.openBank(tag + 1000, DataType::BANK, num + 1000);

            // add bank of shorts
            builder.openBank(tag + 1200, DataType::USHORT16, num + 1200);
            builder.addShortData(short1, dataElementCount);
            builder.closeStructure();

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

            // add bank of composite data
            builder.openBank(tag + 100, DataType::COMPOSITE, num + 100);

            // Now create some COMPOSITE data
            // Format to write 1 int and 1 float a total of N times
            std::string format = "N(I,F)";

            CompositeData::Data myData;
            myData.addN(2);
            myData.addInt(1);
            myData.addFloat(1.0F);
            myData.addInt(2);
            myData.addFloat(2.0F);

            // Create CompositeData object
            // Purposely create it in the opposite byte order so that when it's added below, it will be swapped
            auto cData = CompositeData::getInstance(format, myData, 1, 1, 1, order);
            std::vector<std::shared_ptr<CompositeData>> cDataVec;
            cDataVec.push_back(cData);

            // Add to bank
            builder.addCompositeData(cDataVec);
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
            return builder.getBuffer();
        }


        void writeFile(string finalFilename) {

            ByteOrder outputOrder = ByteOrder::ENDIAN_LITTLE;

            // Create a "first event"
            uint32_t firstEventData[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
            uint32_t firstEventDataLen = 10;
            EventBuilder builder(1, DataType::UINT32, 2);
            std::shared_ptr<EvioEvent> firstEvent = builder.getEvent();
            builder.appendUIntData(firstEvent, firstEventData, firstEventDataLen);
            //auto firstBank = static_cast<std::shared_ptr<EvioBank>>(firstEvent);

            std::string directory = "";
            std::string runType = "";

            // Create files
            EventWriterV4 writer(finalFilename, directory, runType, 1, 0,
                                 EventWriterV4::DEFAULT_BLOCK_SIZE,
                                 EventWriterV4::DEFAULT_BLOCK_COUNT,
                                 outputOrder, dictionary,
                                 true, false,
                                 firstEvent);


//            EventWriterV4(std::string & baseName, const std::string & directory, const std::string & runType,
//            uint32_t runNumber = 1, uint64_t split = 0,
//                    uint32_t maxBlockSize = DEFAULT_BLOCK_SIZE,
//                    uint32_t maxEventCount = DEFAULT_BLOCK_COUNT,
//            const ByteOrder & byteOrder = ByteOrder::nativeOrder(),
//            const std::string & xmlDictionary = "",
//            bool overWriteOK = true, bool append = false,
//            std::shared_ptr<EvioBank> firstEvent = nullptr,
//            uint32_t streamId = 0, uint32_t splitNumber = 0,
//                    uint32_t splitIncrement = 1, uint32_t streamCount = 1,
//                    uint32_t bufferSize = DEFAULT_BUFFER_SIZE,
//                    std::bitset<24> *bitInfo = nullptr);


            // Create an event with lots of stuff in it
            std::shared_ptr<ByteBuffer> evioDataBuf = generateEvioBuffer(outputOrder, 3, 4);
            // Create node from this buffer
            std::shared_ptr<EvioNode> node = EvioNode::extractEventNode(evioDataBuf,0,0,0);

            // Create EvioBank
            uint16_t tag = 4567;
            uint8_t num = 123;
            std::shared_ptr<EvioBank> bank = generateEvioBank(outputOrder, tag, num);

            // write as buffer
            writer.writeEvent(evioDataBuf, false);
            cout << "  Wrote evio buffer, len = " << evioDataBuf->limit() << endl;
            // write as node
            writer.writeEvent(node, false);
            cout << "  Wrote evio node, total bytes = " << node->getTotalBytes() << endl;
            // write as EvioBank
            writer.writeEvent(bank);
            cout << "  Wrote evio bank, total bytes = " << bank->getTotalBytes() << endl;
            cout << "  Wrote evio bank, header len in bytes = " << 4*(bank->getHeader()->getLength() + 1) << endl;

            writer.close();
            cout << "Finished writing file " << finalFilename << " now read it" << endl;

            //Util::printBytes(finalFilename, 0, 2000, "File bytes");
        }


        void readFile(string finalFilename) {
            EvioReader reader(finalFilename);
            ByteOrder order = reader.getByteOrder();

            int32_t evCount = reader.getEventCount();
            cout << "Read in file " << finalFilename << ", got " << evCount << " events" << endl;

            string dict = reader.getDictionaryXML();
            if (dict.empty()) {
                cout << "\nNo dictionary" << endl;
            }
            else {
                cout << "\nGot dictionary:\n" << dict << endl;
            }

            auto pFE = reader.getFirstEvent();
            if (pFE != nullptr) {
                cout << "\nFirst Event:\n" << pFE->toString() << endl << endl;
            }

            cout << "Print out regular events:" << endl;

            for (int i=0; i < evCount; i++) {
                auto ev = reader.getEvent(i+1);
                cout << "\nEvent" << (i+1) << ":\n" << ev->toString() << endl;
            }
        }



        void writeAndReadBuffer() {

            ByteOrder order = ByteOrder::ENDIAN_LITTLE;
            std::shared_ptr<ByteBuffer> buffer = std::make_shared<ByteBuffer>(200000);
            buffer->order(order);

            // Create a "first event"
            int firstEventData[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
            EventBuilder builder(1, DataType::INT32, 2);
            std::shared_ptr<EvioEvent> firstEv = builder.getEvent();
            try {
                builder.setIntData(firstEv, firstEventData, 10);
            }
            catch (EvioException &e) {/* never happen */}

            bool append = false;

            try {
                EventWriterV4 writer(
                        buffer,
                        EventWriterV4::DEFAULT_BLOCK_SIZE,
                        EventWriterV4::DEFAULT_BLOCK_COUNT,
                        dictionary, nullptr, 0,
                        1, append, firstEv);


                //                    EventWriterV4(std::shared_ptr<ByteBuffer> buf,
                //                            int maxBlockSize, int maxEventCount,
                //                            const std::string & xmlDictionary = "", std::bitset < 24 > *bitInfo = nullptr,
                //                            int reserved1 = 0, int blockNumber = 1, bool append = false,
                //                            std::shared_ptr<EvioBank> firstEvent = nullptr);

                // Create an event with lots of stuff in it
                std::shared_ptr<ByteBuffer> evioDataBuf = generateEvioBuffer(order, 3, 4);
                // Create node from this buffer
                std::shared_ptr<EvioNode> node = EvioNode::extractEventNode(evioDataBuf, 0, 0, 0);

                // Create EvioBank
                uint16_t tag = 4567;
                uint8_t num = 123;
                std::shared_ptr<EvioBank> bank = generateEvioBank(order, tag, num);

                // write as buffer
                writer.writeEvent(evioDataBuf, false);
                // write as node
                writer.writeEvent(node, false);
                // write as EvioBank
                writer.writeEvent(bank);

                writer.close();
            }
            catch (EvioException &e) {
                std::cout << "PROBLEM: " << e.what();
            }


            std::shared_ptr<ByteBuffer> copy = ByteBuffer::copyBuffer(buffer);
            std::shared_ptr<ByteBuffer> copy2 = ByteBuffer::copyBuffer(buffer);


            // Compare original with copy


            std::cout << "--------------------------------------------" << std::endl;
            std::cout << "----------      READER1       --------------" << std::endl;
            std::cout << "--------------------------------------------" << std::endl;

            std::shared_ptr<ByteBuffer> dataBuf0 = nullptr;

            try {
                EvioCompactReader reader1(copy);

                int evCount2 = reader1.getEventCount();
                std::cout << "Read in buffer, got " << evCount2 << " events" << std::endl;

                std::string dict2 = reader1.getDictionaryXML();
                std::cout << "   Got dictionary = " << dict2 << std::endl;

                // Compact reader does not deal with first events, so skip over it

                std::cout << "Print out regular events:" << std::endl;

                for (int i = 0; i < reader1.getEventCount(); i++) {
                    std::cout << "scanned event #" << i << " :" << std::endl;
                    std::shared_ptr<EvioNode> compactNode = reader1.getScannedEvent(i + 1);
                    std::cout << "node ->\n" << compactNode->toString() << std::endl;

                    std::shared_ptr<ByteBuffer> dataBuf = std::make_shared<ByteBuffer>(
                            compactNode->getTotalBytes() + 1000);
                    compactNode->getStructureBuffer(dataBuf, true);
                    //                        ByteBuffer buffie(4*compactNode->getDataLength());
                    //                        auto dataBuf = compactNode->getByteData(buffie,true);

                    if (i == 0) {
                        dataBuf0 = dataBuf;
                    }

                    Util::printBytes(dataBuf, dataBuf->position(), dataBuf->remaining(),
                                     "  Event #" + std::to_string(i));
                }
            }
            catch (EvioException &e) {
                std::cout << "PROBLEM: " << e.what();
            }


            std::cout << "--------------------------------------------" << std::endl;
            std::cout << "----------      READER2       --------------" << std::endl;
            std::cout << "--------------------------------------------" << std::endl;


            bool unchanged = true;
            int index = 0;
            std::vector<uint8_t> dataVec;
            std::vector<uint8_t> dataVec0;

            try {
                EvioReader reader2(copy2);

                ///////////////////////////////////
                // Do a parsing listener test here
                std::shared_ptr<EventParser> parser = reader2.getParser();


                class myListener : public IEvioListener {
                public:
                    void startEventParse(std::shared_ptr<BaseStructure> structure) override {
                        std::cout << "  START parsing event = " << structure->toString() << std::endl;
                    }

                    void endEventParse(std::shared_ptr<BaseStructure> structure) override {
                        std::cout << "  END parsing event = " << structure->toString() << std::endl;
                    }

                    void gotStructure(std::shared_ptr<BaseStructure> topStructure,
                                      std::shared_ptr<BaseStructure> structure) override {
                        std::cout << "  GOT struct = " << structure->toString() << std::endl;
                    }
                };

                class myListener2 : public IEvioListener {
                public:
                    void startEventParse(std::shared_ptr<BaseStructure> structure) override {
                        std::cout << "  START parsing event 2 = " << structure->toString() << std::endl;
                    }

                    void endEventParse(std::shared_ptr<BaseStructure> structure) override {
                        std::cout << "  END parsing event 2 = " << structure->toString() << std::endl;
                    }

                    void gotStructure(std::shared_ptr<BaseStructure> topStructure,
                                      std::shared_ptr<BaseStructure> structure) override {
                        std::cout << "  GOT struct 2 = " << structure->toString() << std::endl;
                    }
                };

                // Add the listener to the parser
                auto listener = std::make_shared<myListener>();
                parser->addEvioListener(listener);

                auto listener2 = std::make_shared<myListener2>();
                parser->addEvioListener(listener2);

                // Define a filter to select everything (not much of a filter!)
                class myFilter : public IEvioFilter {
                public:
                    bool accept(StructureType const &type, std::shared_ptr<BaseStructure> struc) override {
                        return (true);
                    }
                };

                // Add the filter to the parser
                auto filter = std::make_shared<myFilter>();
                parser->setEvioFilter(filter);

                // Now parse some event
                std::cout << "Run custom filter and listener, placed in reader's parser, on first event:" << std::endl;
                reader2.parseEvent(1);

                ///////////////////////////////////

                int evCount3 = reader2.getEventCount();
                std::cout << "Read in buffer, got " << evCount3 << " events" << std::endl;

                std::string dict3 = reader2.getDictionaryXML();
                std::cout << "   Got dictionary = " << dict3 << std::endl;

                auto pFE3 = reader2.getFirstEvent();
                if (pFE3 != nullptr) {
                    std::cout << "   First Event bytes = " << pFE3->getTotalBytes() << std::endl;
                    std::cout << "   First Event values = \n   " << std::endl;
                    for (int i = 0; i < pFE3->getRawBytes().size(); i++) {
                        std::cout << (int) (pFE3->getRawBytes()[i]) << ",  " << std::endl;
                    }
                    std::cout << std::endl;
                }

                std::cout << "Print out regular events:" << std::endl;

                for (int i = 0; i < reader2.getEventCount(); i++) {
                    auto ev = reader2.getEvent(i + 1);
                    std::cout << "ev ->\n" << ev->toString() << std::endl;

                    dataVec = ev->getRawBytes();
                    if (i == 0) {
                        dataVec0 = dataVec;
                    }
                    Util::printBytes(dataVec.data(), dataVec.size(), "  Event #" + std::to_string(i));
                }
            }
            catch (EvioException &e) {
                std::cout << "PROBLEM: " << e.what();
            }

            std::cout << "Comparing buffer data (lim = " << dataBuf0->limit() << ") with vector data (len = " << dataVec0.size()
                      << ")" << std::endl;
            for (int i = 0; i < dataVec0.size(); i++) {
                if ((/*data[i+8]*/ dataBuf0->array()[i + 8] != dataVec0[i]) && (i > 3)) {
                    unchanged = false;
                    index = i;
                    std::cout << "Reader different than EvioReader at byte #" << index << std::endl;
                    std::cout << showbase << hex << dataBuf0->array()[i + 8] << " changed to " <<
                              dataVec0[i] << dec << std::endl;
                    break;
                }
            }
            if (unchanged) {
                std::cout << "First data EVENT same whether using EvioCompactReader or EvioReader!" << std::endl;
            }
        }


    };


}




int main(int argc, char **argv) {


    string filename     = "./evioTest.c.evio";
    //string filename_j   = "./evioTest.java.evio";

    evio::ReadWriteTest tester;

    tester.writeFile(filename);
    tester.readFile(filename);

    // Buffers ...
    cout << endl << endl << "----------------------------------------" << endl << endl;

    return 0;
}

