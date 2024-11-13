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



#include <regex>
#include <iterator>
#include <fstream>


#include "TestBase.h"


using namespace std;


namespace evio {


    class ReadWriteTest : public TestBase {


    public:


        // Create a fake Evio Event
        static std::shared_ptr<ByteBuffer> generateEvioBuffer(ByteOrder & order, int dataWords) {

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


//            EventWriterV4(std::string & baseName,
//                          const std::string & directory, const std::string & runType,
//                          uint32_t runNumber = 1, uint64_t split = 0,
//                          uint32_t maxBlockSize = DEFAULT_BLOCK_SIZE,
//                          uint32_t maxEventCount = DEFAULT_BLOCK_COUNT,
//                          const ByteOrder & byteOrder = ByteOrder::nativeOrder(),
//                          const std::string & xmlDictionary = "",
//                          bool overWriteOK = true, bool append = false,
//                          std::shared_ptr<EvioBank> firstEvent = nullptr,
//                          uint32_t streamId = 0, uint32_t splitNumber = 0,
//                          uint32_t splitIncrement = 1, uint32_t streamCount = 1,
//                          uint32_t bufferSize = DEFAULT_BUFFER_SIZE,
//                          std::bitset<24> *bitInfo = nullptr);


            // Create an event with lots of stuff in it
            auto evioDataBuf = createCompactEventBuffer(3, 4, outputOrder);

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
            ByteOrder & order = reader.getByteOrder();

            cout << "Read in file " << finalFilename  << " of byte order " << order.getName() << endl;
            int32_t evCount = reader.getEventCount();
            cout << "Got " << evCount << " events" << endl;

            string dict = reader.getDictionaryXML();
            if (dict.empty()) {
                cout << "\nNo dictionary" << endl;
            }
            else {
                cout << "\nGot dictionary:\n" << dict << endl;
            }

            auto pFE = reader.getFirstEvent();
            if (pFE != nullptr) {
                cout << "\nGot first Event:\n" << pFE->toString() << endl << endl;
            }
            else {
                cout << "\nNo first event" << endl;
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
                std::shared_ptr<ByteBuffer> evioDataBuf = generateEvioBuffer(order, 4);
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

                uint32_t evCount2 = reader1.getEventCount();
                std::cout << "Read in buffer, got " << evCount2 << " events" << std::endl;

                std::string dict2 = reader1.getDictionaryXML();
                std::cout << "   Got dictionary = " << dict2 << std::endl;

                // Compact reader does not deal with first events, so skip over it

                std::cout << "Print out regular events:" << std::endl;

                for (uint32_t i = 0; i < evCount2; i++) {
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

                uint32_t evCount3 = reader2.getEventCount();
                std::cout << "Read in buffer, got " << evCount3 << " events" << std::endl;

                std::string dict3 = reader2.getDictionaryXML();
                std::cout << "   Got dictionary = " << dict3 << std::endl;

                auto pFE3 = reader2.getFirstEvent();
                if (pFE3 != nullptr) {
                    std::cout << "   First Event bytes = " << pFE3->getTotalBytes() << std::endl;
                    std::cout << "   First Event values = \n   " << std::endl;
                    for (size_t i = 0; i < pFE3->getRawBytes().size(); i++) {
                        std::cout << (int) (pFE3->getRawBytes()[i]) << ",  " << std::endl;
                    }
                    std::cout << std::endl;
                }

                std::cout << "Print out regular events:" << std::endl;

                for (uint32_t i = 0; i < evCount3; i++) {
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
            for (size_t i = 0; i < dataVec0.size(); i++) {
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

