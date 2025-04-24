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
#include <iostream>


#include "TestBase.h"


using namespace std;


namespace evio {


    class ReadWriteTest : public TestBase {


    public:



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



        void writeFile(string finalFilename, uint16_t tag, uint8_t num) {

            std::cout << std::endl;
            std::cout << "--------------------------------------------" << std::endl;
            std::cout << "----------    Write to file   --------------" << std::endl;
            std::cout << "--------------------------------------------" << std::endl;

            ByteOrder outputOrder = ByteOrder::ENDIAN_LITTLE;

            // Create a "first event"
            uint32_t firstEventData[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
            uint32_t firstEventDataLen = 10;
            EventBuilder builder(1, DataType::UINT32, 2);
            std::shared_ptr<EvioEvent> firstEvent = builder.getEvent();
            builder.appendUIntData(firstEvent, firstEventData, firstEventDataLen);
            //auto firstBank = static_cast<std::shared_ptr<EvioBank>>(firstEvent);

            std::string directory;
            std::string runType;

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
            auto evioDataBuf = createCompactEventBuffer(tag, num, outputOrder);

            // Create node from this buffer
            std::shared_ptr<EvioNode> node = EvioNode::extractEventNode(evioDataBuf,0,0,0);

            // Create EvioBank
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

            std::cout << std::endl;
            std::cout << "--------------------------------------------" << std::endl;
            std::cout << "----------   Read from file   --------------" << std::endl;
            std::cout << "--------------------------------------------" << std::endl;

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



        void writeAndReadBuffer(uint16_t tag, uint8_t num) {

            std::cout << std::endl;
            std::cout << "--------------------------------------------" << std::endl;
            std::cout << "----------    Write to buf    --------------" << std::endl;
            std::cout << "--------------------------------------------" << std::endl;

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
            // Allow only 2 events per block to test using multiple blocks
            int blockCount = 20000;

            try {
                EventWriterV4 writer(
                        buffer,
                        EventWriterV4::DEFAULT_BLOCK_SIZE,
                        blockCount,
                        dictionary, nullptr, 0,
                        1, append, firstEv);

                // Create an event in buffer form with lots of stuff in it
                std::shared_ptr<ByteBuffer> evioDataBuf = createCompactEventBuffer(tag, num, order);
                // Create same event as EvioEvent object
                std::shared_ptr<EvioEvent> evioEv = createEventBuilderEvent(tag, num);
                // Create node from this buffer
                std::shared_ptr<EvioNode> node = EvioNode::extractEventNode(evioDataBuf, 0, 0, 0);


                // write as buffer
                writer.writeEvent(evioDataBuf, false);
                // write as EvioEvent
                writer.writeEvent(evioEv, false);
                // write as node
                writer.writeEvent(node, false);

                writer.close();
                // Get ready-to-read buffer
                buffer = writer.getByteBuffer();

            }
            catch (EvioException &e) {
                std::cout << "PROBLEM: " << e.what();
            }

            std::shared_ptr<ByteBuffer> copy = ByteBuffer::copyBuffer(buffer);
            std::shared_ptr<ByteBuffer> copy2 = ByteBuffer::copyBuffer(buffer);


            // Reader cannot be used, it only works on evio version 6 files and buffers.
           // Reader reader(buffer);



            std::cout << "--------------------------------------------" << std::endl;
            std::cout << "----------   EvioCompactReader   -----------" << std::endl;
            std::cout << "--------------------------------------------" << std::endl;

            std::shared_ptr<ByteBuffer> dataBuf0 = nullptr;

            try {
                EvioCompactReader reader1(copy);

                uint32_t evCount2 = reader1.getEventCount();
                std::cout << "   Got " << evCount2 << " events" << std::endl;

                std::string dict2 = reader1.getDictionaryXML();
                std::cout << "   Got dictionary = \n" << dict2 << std::endl;

                // Compact reader does not deal with first events, so skip over it

                std::cout << "\n   Print out events (includes first event if evio version 4) :" << std::endl;

/*                 for (uint32_t i = 0; i < evCount2; i++) {
                    // The "first event" is just the first event in the list (not treated specially)
                    std::cout << "      scanned event #" << (i+1) << " :" << std::endl;
                    std::shared_ptr<EvioNode> compactNode = reader1.getScannedEvent(i + 1);
                    std::cout << "      node ->\n         " << compactNode->toString() << std::endl;

                    std::shared_ptr<ByteBuffer> dataBuf = std::make_shared<ByteBuffer>(compactNode->getTotalBytes());
                    std::cout << "GOT THIS FAR 1" << std::endl;

                    compactNode->getStructureBuffer(dataBuf, true);
                    std::cout << "GOT THIS FAR 2" << std::endl;

                    if (i == 0) {
                        dataBuf0 = dataBuf;
                        Util::printBytes(dataBuf, dataBuf->position(), dataBuf->remaining(),
                                         "      Event #" + std::to_string(i+1));
                    }
                }
 */            }
            catch (EvioException &e) {
                std::cout << "PROBLEM: " << e.what();
            }


            std::cout << std::endl;
            std::cout << "--------------------------------------------" << std::endl;
            std::cout << "----------     EvioReader     --------------" << std::endl;
            std::cout << "--------------------------------------------" << std::endl;


            bool unchanged = true;
            size_t index = 0;
            std::vector<uint8_t> dataVec;
            std::vector<uint8_t> dataVec0;

            try {

                // TODO: Something WRONG with the parsing!!!
                EvioReader reader1(copy2);

                uint32_t evCount2 = reader1.getEventCount();
                std::cout << "   Got " << evCount2 << " events" << std::endl;

                std::string dict2 = reader1.getDictionaryXML();
                std::cout << "   Got dictionary = \n" << dict2 << std::endl;

                std::cout << "\n   Got first event = " << reader1.hasFirstEvent() << std::endl;

                std::cout << "\n   Print out events (includes first event if evio version 4) :" << std::endl;

                for (uint32_t i = 0; i < evCount2; i++) {
                    std::shared_ptr<EvioEvent> ev = reader1.parseEvent(i + 1);
                    std::cout << "      got & parsed ev " << (i+1) << std::endl;
                    std::cout << "      event ->\n" << ev->toString() << std::endl;
                    if (i == 0) {
                        dataVec0 = ev->getRawBytes();
                    }
                    //std::shared_ptr<EvioEvent> evt = reader1.parseNextEvent();
                }
// This has the same output as above
//                int j=0;
//                std::shared_ptr<EvioEvent> evt;
//                while ((evt = reader1.parseNextEvent()) != nullptr) {
//                    std::cout << "      got & parsed ev " << (j+1) << std::endl;
//                    std::cout << "      event ->\n" << evt->toString() << std::endl;
//                    if (j == 0) {
//                        dataVec0 = evt->getRawBytes();
//                    }
//                    j++;
//                }

                std::cout << "   Comparing buffer data (lim = " << dataBuf0->limit() << ") with vector data (len = " << dataVec0.size()
                          << ")" << std::endl;
                for (size_t i = 0; i < dataVec0.size(); i++) {
                    if ((/*data[i+8]*/ dataBuf0->array()[i + 8] != dataVec0[i]) && (i > 3)) {
                        unchanged = false;
                        index = i;
                        std::cout << "       Compact reader different than EvioReader at byte #" << index << std::endl;
                        std::cout << showbase << hex << dataBuf0->array()[i + 8] << " changed to " <<
                                  dataVec0[i] << dec << std::endl;
                        break;
                    }
                }
                if (unchanged) {
                    std::cout << "First data EVENT same whether using EvioCompactReader or EvioReader!" << std::endl;
                }

            }
            catch (EvioException &e) {
                std::cout << "PROBLEM: " << e.what();
            }



        }


    };


}




int main(int argc, char **argv) {


    string filename_c = "./evioTest.c.evio";
    string filename_j = "./evioTest.java.evio";

    evio::ReadWriteTest tester;

    tester.writeFile(filename_c, 1,1);
    tester.readFile(filename_c);

//    std::ifstream file(filename_j);
//    if (file) {
//        tester.readFile(filename_j);
//    }

    tester.writeAndReadBuffer(1,1);

    // Buffers ...
    cout << endl << endl << "----------------------------------------" << endl << endl;

    return 0;
}

