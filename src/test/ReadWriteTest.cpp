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



#include <regex>
#include <iterator>
#include <fstream>


#include "TestBase.h"


using namespace std;


namespace evio {


    class ReadWriteTest : public TestBase {


    public:


        void writeAndReadBuffer() {

            // Create Buffer
            size_t bufSize = 3000;
            auto buffer = std::make_shared<ByteBuffer>(bufSize);
            buffer->order(order);

            // Possible user header data
            uint8_t userHdr[10];
            for (uint8_t i = 0; i < 10; i++) {
                userHdr[i] = i + 16;
            }

            Writer writer(buffer, userHdr, 10);

            // Calling the following method makes a shared pointer out of dataArray, so don't delete
            ByteBuffer dataBuffer(20);
            for (int i = 0; i < 10; i++) {
                dataBuffer.putShort(i);
            }
            dataBuffer.flip();

            //cout << "Data buffer ->\n" << dataBuffer.toString() << endl;

            // Create an evio bank of ints
            auto evioDataBuf = createEventBuilderBuffer(0, 0, order);
            // Create node from this buffer
            std::shared_ptr<EvioNode> node = EvioNode::extractEventNode(evioDataBuf, 0, 0, 0);

            writer.addEvent(*node);

            cout << "Wrote Buffer w/ user header" << endl;

            writer.close();

            // Get ready-to-read buffer
            buffer = writer.getBuffer();

            auto copy = std::make_shared<ByteBuffer>(*buffer);
            auto copy2 = std::make_shared<ByteBuffer>(*buffer);

            cout << "Finished buffer ->\n" << buffer->toString() << endl;
            cout << "COPY1 ->\n" << copy->toString() << endl;
            cout << "COPY2 ->\n" << copy2->toString() << endl;

            Util::printBytes(buffer, 0, bufSize, "Buffer Bytes");

            //------------------------------
            //----  READER  ---------------
            //------------------------------

            Reader reader(buffer);
            cout << "Past Reader's constructor" << endl;
            bool unchanged = true;
            int index = 0;

            // Compare original with copy
            for (size_t i = 0; i < buffer->capacity(); i++) {
                if (buffer->array()[i] != copy->array()[i]) {
                    unchanged = false;
                    index = i;
                    std::cout << "Orig buffer CHANGED at byte #" << index << std::endl;
                    std::cout << showbase << hex << ", " << +copy->array()[i] << " changed to "
                              << +buffer->array()[i] << std::endl;
                    Util::printBytes(buffer, 0, 200, "Buffer Bytes");
                    break;
                }
            }
            if (unchanged) {
                std::cout << "ORIGINAL buffer Unchanged!\n";
            }

            std::shared_ptr<ByteBuffer> usrHeadBuf = reader.readUserHeader();
            if (usrHeadBuf->remaining() < 1) {
                cout << "NO user header in buffer" << endl;
            }
            else {
                Util::printBytes(usrHeadBuf, 0, usrHeadBuf->limit(), "User Header exists! ");
            }

            int32_t evCount = reader.getEventCount();
            cout << "\nRead in buffer, got " << evCount << " events" << endl;

            cout << "   Got dictionary = " << reader.hasDictionary() << endl;
            cout << "   Got first evt  = " << reader.hasFirstEvent() << endl;

            cout << "Print out regular events:" << endl;
            shared_ptr<uint8_t> data;
            uint32_t byteLen;

            for (int i = 0; i < evCount; i++) {
                data = reader.getEvent(i, &byteLen);
                Util::printBytes(data.get(), byteLen, "  Event #" + std::to_string(i+1));
            }


            std::cout << "--------------------------------------------\n";
            std::cout << "--------------  Reader 2 -------------------\n";
            std::cout << "--------------------------------------------\n";

            std::shared_ptr<ByteBuffer> dataBuf = nullptr;

            try {
                EvioCompactReader reader2(copy);

                int32_t evCount2 = reader2.getEventCount();
                cout << "Read in buffer, got " << evCount2 << " events" << endl;

                cout << "   Got dictionary = " << reader2.hasDictionary() << endl;

                // Compact reader does not deal with first events, so skip over it

                std::cout << "Print out regular events:" << std::endl;

                for (int i = 0; i < evCount2; i++) {
                    std::cout << "scanned event #" << (i+1) << std::endl;
                    std::shared_ptr<EvioNode> compactNode = reader2.getScannedEvent(i + 1);
                    std::cout << "   node ->\n" << compactNode->toString() << std::endl;

                    dataBuf = std::make_shared<ByteBuffer>(compactNode->getTotalBytes());
                    dataBuf->order(order);
                    compactNode->getStructureBuffer(dataBuf, true);

                    Util::printBytes(dataBuf, dataBuf->position(), dataBuf->remaining(),
                                     "  Event #" + std::to_string(i+1) + " at pos " + std::to_string(dataBuf->position()));
                }


                for (size_t i=0; i < dataBuf->limit(); i++) {
                    if ((data.get()[i] != dataBuf->array()[i])) {
                        unchanged = false;
                        index = i;
                        std::cout << "Reader different than EvioCompactReader at byte #" << index << std::endl;
                        std::cout << showbase << hex << +data.get()[i] << " changed to " << +dataBuf->array()[i] << dec << std::endl;
                        break;
                    }
                }
                if (unchanged) {
                    std::cout << "EVENT same whether using Reader or EvioCompactReader!\n";
                }

            }
            catch (EvioException &e) {
                std::cout << "PROBLEM: " << e.what() << std::endl;
            }


            std::cout << "--------------------------------------------\n";
            std::cout << "--------------  Reader 3 -------------------\n";
            std::cout << "--------------------------------------------\n";

            // Contains only the data of a structure (after evio structure header, ie bank,seg,tagseg)
            std::vector<uint8_t> dataVec;

            try {
                EvioReader reader3(copy2);

                ///////////////////////////////////
                // Do a parsing listener test here
                auto parser = reader3.getParser();

                // Define a listener to be used with an event parser IEvioListener listener = new IEvioListener() { 
                class myListener : public IEvioListener {
                public:
                    void gotStructure(std::shared_ptr<BaseStructure> topStructure,
                                      std::shared_ptr<BaseStructure> structure) override {
                        std::cout << "  GOT struct = " << structure->toString() << std::endl;
                    }

                    // We're not parsing so these are not used ...
                    void startEventParse(std::shared_ptr<BaseStructure> structure) override {
                        std::cout << "  START parsing event = " << structure->toString() << std::endl << std::endl;
                    }
                    void endEventParse(std::shared_ptr<BaseStructure> structure) override {
                        std::cout << "  END parsing event = " << structure->toString() << std::endl << std::endl;
                    }
                };

                auto listener = std::make_shared<myListener>();

                // Define a listener to be used with an event parser IEvioListener listener = new IEvioListener() { 
                class myListener2 : public IEvioListener {
                public:
                    void gotStructure(std::shared_ptr<BaseStructure> topStructure,
                                      std::shared_ptr<BaseStructure> structure) override {
                        std::cout << "  GOT struct 2 = " << structure->toString() << std::endl;
                    }

                    // We're not parsing so these are not used ...
                    void startEventParse(std::shared_ptr<BaseStructure> structure) override {
                        std::cout << "  START parsing event 2 = " << structure->toString() << std::endl << std::endl;
                    }
                    void endEventParse(std::shared_ptr<BaseStructure> structure) override {
                        std::cout << "  END parsing event 2 = " << structure->toString() << std::endl << std::endl;
                    }
                };

                auto listener2 = std::make_shared<myListener2>();


                // Add the listener to the parser
                parser->addEvioListener(listener);
                parser->addEvioListener(listener2);

                // Define a filter to select everything (not much of a filter!)
                class myFilter : public IEvioFilter {
                public:
                    bool accept(StructureType const & type, std::shared_ptr<BaseStructure> struc) override {
                        return (true);
                    }
                };

                auto filter = std::make_shared<myFilter>();

                // Add the filter to the parser
                parser->setEvioFilter(filter);

                // Now parse some event
                cout << "Run custom filter and listener, placed in reader's parser, on first event:" << endl;
                auto ev = reader3.parseEvent(1);

                ///////////////////////////////////

                int32_t evCount3 = reader3.getEventCount();
                cout << "Read in buffer, got " << evCount3 << " events" << endl;

                cout << "   Got dictionary = " << reader3.hasDictionaryXML() << endl;
                cout << "   Got first evt  = " << reader3.hasFirstEvent() << endl;

                cout << "Print out regular events:" << endl;

                for (int i = 0; i < evCount3; i++) {
                    std::shared_ptr<EvioEvent> ev = reader3.getEvent(i + 1);
                    std::cout << "ev ->\n" << ev->toString() << std::endl;

                    /* auto & */ dataVec = ev->getRawBytes();
                    Util::printBytes(dataVec.data(), dataVec.size(),
                                     "  Event #" + std::to_string(i+1));
                }


                std::cout << "Comparing data with dataVec\n";
                // data has whole event, dataVec only data, so have event bank skip over header
                for (size_t i=0; i < dataVec.size(); i++) {
                    if ((data.get()[i+8] != dataVec[i])) {
                        unchanged = false;
                        index = i;
                        std::cout << "Reader different than EvioReader at byte #" << index << std::endl;
                        std::cout << showbase << hex << +data.get()[i+8] << " changed to " << +dataVec[i] << dec << std::endl;
                        break;
                    }
                }
                if (unchanged) {
                    std::cout << "EVENT same whether using Reader or EvioReader!\n";
                }

            }
            catch (EvioException &e) {
                std::cout << "PROBLEM: " << e.what() << std::endl;
            }


        }



    };


}




int main(int argc, char **argv) {

    evio::ReadWriteTest tester;

    // Buffers ...
    tester.writeAndReadBuffer();
    cout << endl << endl << "----------------------------------------" << endl << endl;

    return 0;
}

