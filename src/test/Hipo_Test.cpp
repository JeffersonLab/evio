//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include <iostream>
#include <fstream>

#include "TestBase.h"
#include "eviocc.h"


namespace evio {


    /**
     * Test the HIPO API.
     *
     * @date 08/27/2020
     * @author timmer
     */
    class HipoTester : public TestBase {

    public:



        static std::streamsize getFileSize(const std::string& filePath) {
            std::ifstream file(filePath, std::ios::binary | std::ios::ate); // Open file in binary mode at the end
            if (!file) {
                std::cerr << "Error opening file: " << filePath << std::endl;
                return -1;
            }
            return file.tellg(); // Get the position, which is the file size
        }



        /** Writing to a buffer using CompactEventBuilder interface. */
        void testCompactEventCreation(uint16_t tag, uint8_t num) {

            try {
                bool addTrailerIndex = true;

                // Create ByteBuffer with EvioEvent in it
                buffer = createCompactEventBuffer(tag, num);

                Util::printBytes(buffer, 0, buffer->limit(), "BUFFER BYTES");
                std::cout << "\nBuffer -> \n" << buffer->toString() << "\n";

                //------------------------------
                // Create record to test writer.writeRecord(recOut);
                // This will not change position of buffer.
                //------------------------------
                RecordOutput recOut(order);
                recOut.addEvent(buffer, 0);
                //------------------------------

                // Create node from this buffer
                std::shared_ptr<EvioNode> node = EvioNode::extractEventNode(buffer, 0, 0, 0);


                //
                // Write file.
                // Dictionary and first event end up as user header in file header.
                //
//                Writer writer(HeaderType::EVIO_FILE, ByteOrder::ENDIAN_LOCAL,
//                              0, 0, "", nullptr, 0,
//                              Compressor::UNCOMPRESSED, false);

                WriterMT writer(HeaderType::EVIO_FILE, ByteOrder::ENDIAN_LOCAL,
                                0, 0, dictionary, buffer->array(), buffer->limit(),
                                Compressor::UNCOMPRESSED, 1, addTrailerIndex);

                WriterMT writer2(HeaderType::EVIO_FILE, ByteOrder::ENDIAN_LOCAL,
                                0, 0, dictionary, buffer->array(), buffer->limit(),
                                Compressor::LZ4_BEST, 3, addTrailerIndex);

                writer.open(writeFileName1, nullptr, true);
                writer.addEvent(buffer);
                writer.close();
                std::cout << "File size of " << writeFileName1 << " is " << getFileSize(writeFileName1) << std::endl;
                Util::printBytes(writeFileName1, 0, 200, "WRITTEN FILE BYTES");

                writer.open(writeFileName1, nullptr, true);
                std::cout << std::endl<< "Call open again, rewrite 3 events to file" << std::endl;
                writer.addEvent(buffer);
                writer.addEvent(buffer);
                writer.addEvent(buffer);
                std::cout << "add entire record" << std::endl;
                writer.writeRecord(recOut);

                writer.addEvent(node);

                writer.close();
                std::cout << "File size of " << writeFileName1 << " is now " << getFileSize(writeFileName1) << std::endl;

                Util::printBytes(writeFileName1, 0, 200, "WRITTEN FILE BYTES 2");

                std::cout << "\n\nRead file ...\n";

                // Read event back out of file
                Reader reader(writeFileName1);

                std::cout << "have dictionary? " << reader.hasDictionary() << std::endl;
                std::string xmlDict = reader.getDictionary();
                if (reader.hasDictionary()) {
                    std::cout << "dictionary ->\n\n" << xmlDict << std::endl << std::endl;
                }

                std::cout << "have first event? " << reader.hasFirstEvent() << std::endl;
                if (reader.hasFirstEvent()) {
                    uint32_t feSize = 0;
                    std::shared_ptr<uint8_t> fe = reader.getFirstEvent(&feSize);
                    std::cout << "first event len = " << feSize << std::endl;
                }

                std::cout << "\ntry getting getNextEvent" << std::endl;
                if (reader.getEventCount() < 1) {
                    std::cout << "no data events in file" << std::endl;
                    return;
                }

                std::cout << "event count = " << reader.getEventCount() << std::endl;

                uint32_t len;
                std::shared_ptr<uint8_t> bytes = reader.getNextEvent(&len);
                auto ev = EvioReader::getEvent(bytes.get(), len, reader.getByteOrder());
                if (bytes != nullptr) {
                    std::cout << "next evio event ->\n" << ev->treeToString("") << std::endl;
                }

                bytes = reader.getEvent(0, &len);
                if (bytes != nullptr) {
                    std::cout << "getEvent(0), size = " << std::to_string(len) << std::endl;
                }

                bytes = reader.getEvent(1, &len);
                if (bytes != nullptr) {
                    std::cout << "getEvent(1), size = " << std::to_string(len) << std::endl;
                }

                bytes = reader.getEvent(2, &len);
                if (bytes != nullptr) {
                    std::cout << "getEvent(2), size = " << std::to_string(len) << std::endl;
                }

                bytes = reader.getEvent(3, &len);
                if (bytes != nullptr) {
                    std::cout << "getEvent(3), size = " << std::to_string(len) << std::endl;
                }

                // This event was added with reader.recordWrite()
                bytes = reader.getEvent(4, &len);
                if (bytes != nullptr) {
                    std::cout << "getEvent(4), size = " << std::to_string(len) << std::endl;
                }

                bytes = reader.getEvent(20, &len);
                if (bytes != nullptr) {
                    std::cout << "getEvent(20), size = " << std::to_string(len) << std::endl;
                }
                else {
                    std::cout << "getEvent(20), no such event!" << std::endl;
                }

                ByteBuffer bb1(20000);
                reader.getEvent(bb1, 0);
                std::cout << "event 1,  ByteBuffer limit = " << bb1.limit() << std::endl;

                auto bb2 = std::make_shared<ByteBuffer>(20000);
                reader.getEvent(bb2, 0);
                std::cout << "event 1, std::shared_ptr<ByteBuffer> limit = " << bb2->limit() << std::endl;
            }
            catch (EvioException &e) {
                std::cout << e.what() << std::endl;
            }
        }


        /** Writing to a buffer using original evio tree interface. */
        void testTreeEventCreation(uint16_t tag, uint8_t num) {

            try {
                // Build event (bank of banks) with EventBuilder object
                std::shared_ptr<EvioEvent> event = createTreeEvent(tag, num);

                std::cout << "\n\nEvent (created by tree methods):\n" << event->treeToString("") << std::endl;
                std::cout << "Event Header:\n" << event->getHeader()->toString() << std::endl;

                // Take event & write it into buffer
                std::cout << "Write event to " << writeFileName2 << " as compressed LZ4" << std::endl;
                EventWriter writer(writeFileName2, "", "runType", 1, 0L, 0, 0,
                                   ByteOrder::ENDIAN_LOCAL, dictionary, true, false, event, 1, 1, 1, 1,
                                   Compressor::LZ4, 2, 16, 0);

                //writer.setFirstEvent(event);
                writer.writeEvent(event);
                std::cout << "    call writer.close()" << std::endl;
                writer.close();

                // Read event back out of file
                std::cout << "    create EvioReader" << std::endl;
                EvioReader reader(writeFileName2);

                std::cout << "    have dictionary? " << reader.hasDictionaryXML() << std::endl;
                if (reader.hasDictionaryXML()) {
                    std::string xmlDict = reader.getDictionaryXML();
                    std::cout << "    read dictionary ->\n\n" << xmlDict << std::endl << std::endl;
                }

                std::cout << "    have first event? " << reader.hasFirstEvent() << std::endl;
                if (reader.hasFirstEvent()) {
                    auto fe = reader.getFirstEvent();
                    std::cout << "    read first event ->\n\n" << fe->treeToString("") << std::endl << std::endl;
                }

                std::cout << "    try getting ev #1" << std::endl;
                auto ev = reader.parseEvent(1);
                std::cout << "    event ->\n" << ev->treeToString("") << std::endl;
            }
            catch (EvioException &e) {
                std::cout << e.what() << std::endl;
            }
        }


        void writeAndReadBuffer(uint16_t tag, uint8_t num) {

            std::cout << std::endl << std::endl;
            std::cout << "--------------------------------------------\n";
            std::cout << "--------------------------------------------\n";
            std::cout << "------------- NOW BUFFERS ------------------\n";
            std::cout << "--------------------------------------------\n";
            std::cout << "--------------------------------------------\n";
            std::cout << std::endl << std::endl;

            // Create Buffer
            ByteOrder order = ByteOrder::ENDIAN_LOCAL;
            size_t bufSize = 3000;
            auto buffer = std::make_shared<ByteBuffer>(bufSize);
            buffer->order(order);

            // user header data
            uint8_t userHdr[10];
            for (uint8_t i = 0; i < 10; i++) {
                userHdr[i] = i + 16;
            }

            // No compression allowed with buffers

            //Writer writer(buffer, 0, 0, dictionary, userHdr, 10);
            Writer writer(buffer, userHdr, 10);

            // Create an evio bank of ints
            auto evioDataBuf = createEventBuilderBuffer(tag, num, order);
            // Create node from this buffer
            std::shared_ptr<EvioNode> node = EvioNode::extractEventNode(evioDataBuf, 0, 0, 0);

            writer.addEvent(*node);
            writer.close();

            // Get ready-to-read buffer
            buffer = writer.getBuffer();

            // 2 ways to copy
            auto copy  = std::make_shared<ByteBuffer>(*buffer);
            auto copy2 = ByteBuffer::copyBuffer(buffer);

            std::cout << "Finished buffer ->\n" << buffer->toString() << std::endl;
            std::cout << "COPY1 ->\n" << copy->toString() << std::endl;
            std::cout << "COPY2 ->\n" << copy2->toString() << std::endl;

            // Compare original with copy
            bool unchanged = true;
            size_t index = 0;
            for (size_t i = 0; i < buffer->capacity(); i++) {
                if (buffer->array()[i] != copy->array()[i]) {
                    unchanged = false;
                    index = i;
                    std::cout << "Orig buffer CHANGED at byte #" << index << std::endl;
                    std::cout << std::showbase << std::hex << ", " << +copy->array()[i] << " changed to "
                              << +buffer->array()[i] << std::endl;
                    Util::printBytes(buffer, 0, 200, "Buffer Bytes");
                    break;
                }
            }
            if (unchanged) {
                std::cout << "ORIGINAL buffer Unchanged!\n";
            }

            Util::printBytes(buffer, 0, buffer->limit(), "Buffer Bytes");

            std::cout << "--------------------------------------------\n";
            std::cout << "------------------ Reader ------------------\n";
            std::cout << "--------------------------------------------\n";

            Reader reader(buffer);

            uint32_t evCount = reader.getEventCount();
            std::cout << "   Got " << evCount << " events" << std::endl;

            std::string dict = reader.getDictionary();
            std::cout << "   Have dictionary = " << reader.hasDictionary() << std::endl;

            uint32_t feBytes;
            std::shared_ptr<uint8_t> pFE = reader.getFirstEvent(&feBytes);
            if (pFE != nullptr) {
                std::cout << "   First Event bytes = " << feBytes << std::endl;
                std::cout << "   First Event values = " << std::endl << "   ";
                for (uint32_t i = 0; i < feBytes; i++) {
                    std::cout << (uint32_t) ((pFE.get())[i]) << ",  ";
                }
                std::cout << std::endl << std::endl;
            }

            std::cout << "   Print out regular events:" << std::endl;
            uint32_t byteLen;
            std::shared_ptr<uint8_t> data;
            for (uint32_t i = 0; i < evCount; i++) {
                // Because this is a Reader object, it does not parse evio, it only gets a bunch of bytes.
                // For parsing evio, use EvioCompactReader or EvioReader.
                data = reader.getEvent(i, &byteLen);
                std::cout << "      Event " << (i+1) << " len = " << byteLen << " bytes" << std::endl;
            }


            std::cout << "--------------------------------------------\n";
            std::cout << "------------ EvioCompactReader -------------\n";
            std::cout << "--------------------------------------------\n";

            std::shared_ptr<ByteBuffer> dataBuf = nullptr;

            try {
                EvioCompactReader reader2(copy);

                uint32_t evCount2 = reader2.getEventCount();
                std::cout << "   Got " << evCount2 << " events" << std::endl;

                std::string dict2 = reader2.getDictionaryXML();
                std::cout << "   Have dictionary = " << reader2.hasDictionary() << std::endl;

                // Compact reader does not deal with first events, so skip over it

                std::cout << "   Print out regular events:" << std::endl;

                for (uint32_t i = 0; i < evCount2; i++) {
                    std::shared_ptr<EvioNode> compactNode = reader2.getScannedEvent(i + 1);

                    // This node and possibly other nodes have the same underlying buffer.
                    // Next copy out this node's portion of the underlying buffer into its own buffer.
                    // Make it the right size.
                    dataBuf = std::make_shared<ByteBuffer>(compactNode->getTotalBytes());
                    // Get the byte order right.
                    dataBuf->order(order);
                    // Do the copy
                    compactNode->getByteData(dataBuf, true);

                    std::cout << "      Event " << (i+1) << " len = " << compactNode->getTotalBytes() << " bytes" << std::endl;
                    //                    Util::printBytes(dataBuf, dataBuf->position(), dataBuf->remaining(),
                    //                                     "      Event #" + std::to_string(i+1) + " at pos " + std::to_string(dataBuf->position()));
                }

                // Comparing last events together, one from reader, the other from reader2
                unchanged = true;
                for (size_t i=0; i < dataBuf->limit(); i++) {
                    if ((data.get()[i+8] != dataBuf->array()[i])) {
                        unchanged = false;
                        index = i;
                        std::cout << "Reader different than EvioCompactReader at byte #" << index << std::endl;
                        std::cout << std::showbase << std::hex << +data.get()[i] << " changed to " << +dataBuf->array()[i] << std::dec << std::endl;
                        break;
                    }
                }
                if (unchanged) {
                    std::cout << std::endl << "Last event same whether using Reader or EvioCompactReader!" << std::endl;
                }
            }
            catch (EvioException &e) {
                std::cout << "PROBLEM: " << e.what() << std::endl;
            }


            std::cout << "--------------------------------------------\n";
            std::cout << "-------------- EvioReader ------------------\n";
            std::cout << "--------------------------------------------\n";

            try {
                EvioReader reader3(copy2);

                ///////////////////////////////////
                // Do a parsing listener test here
                auto parser = reader3.getParser();


                // Define a listener to be used with an event parser
                class myListener : public IEvioListener {
                public:
                    void gotStructure(std::shared_ptr<BaseStructure> topStructure,
                                      std::shared_ptr<BaseStructure> structure) override {
                        std::cout << "  GOT: struct 1 = " << structure->toString() << std::endl;
                    }

                    // We're not parsing so these are not used ...
                    void startEventParse(std::shared_ptr<BaseStructure> structure) override {
                        std::cout << "  START: parsing event 1 = " << structure->toString() << std::endl;
                    }
                    void endEventParse(std::shared_ptr<BaseStructure> structure) override {
                        std::cout << "  END: parsing event 1 = " << structure->toString() << std::endl;
                    }
                };

                auto listener1 = std::make_shared<myListener>();



                // Define another listener to be used with an event parser
                class myListener2 : public IEvioListener {
                public:
                    void gotStructure(std::shared_ptr<BaseStructure> topStructure,
                                      std::shared_ptr<BaseStructure> structure) override {
                        std::cout << "  GOT: struct 2 = " << structure->toString() << std::endl << std::endl;
                    }

                    // We're not parsing so these are not used ...
                    void startEventParse(std::shared_ptr<BaseStructure> structure) override {
                        std::cout << "  START: parsing event 2 = " << structure->toString() << std::endl << std::endl;
                    }
                    void endEventParse(std::shared_ptr<BaseStructure> structure) override {
                        std::cout << "  END: parsing event 2 = " << structure->toString() << std::endl << std::endl;
                    }
                };

                auto listener2 = std::make_shared<myListener2>();


                // Add the 2 listeners to the parser
                parser->addEvioListener(listener2);
                parser->addEvioListener(listener1);

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
                std::cout << "Run custom filter and listener, placed in reader's parser, on first event:" << std::endl << std::endl;
                auto ev = reader3.parseEvent(1);

                ///////////////////////////////////

                int32_t evCount3 = reader3.getEventCount();
                std::cout << "   Got " << evCount3 << " events" << std::endl;

                std::string dict3 = reader3.getDictionaryXML();
                std::cout << "   Got dictionary = " << reader3.hasDictionaryXML() << std::endl;

                std::shared_ptr<EvioEvent> fe = reader3.getFirstEvent();
                if (fe != nullptr) {
                    std::cout << "   First Event bytes = " << fe->getTotalBytes() << std::endl;
                    std::cout << "   First Event values = " << std::endl << "   ";
                    std::vector<uint8_t> & rawByteVector = fe->getRawBytes();
                    for (size_t i = 0; i < rawByteVector.size(); i++) {
                        std::cout << (uint32_t) (rawByteVector[i]) << ",  ";
                    }
                    std::cout << std::endl << std::endl;
                }

                // Remove all listeners & filter
                parser->removeEvioListener(listener2);
                parser->removeEvioListener(listener1);
                parser->setEvioFilter(nullptr);

                std::vector<uint8_t> dataVec;

                std::cout << "   Print out regular events:" << std::endl;
                for (int i = 0; i < evCount3; i++) {
                    std::shared_ptr<EvioEvent> evt = reader3.parseEvent(i + 1);
                    std::cout << "      Event " << (i+1) << " len = " << evt->getTotalBytes() << " bytes" << std::endl;

                    dataVec = evt->getRawBytes();
//                    Util::printBytes(dataVec.data(), dataVec.size(), "  Event #" + std::to_string(i));
                }

                std::cout << std::endl << "Comparing data with dataVec\n";
                unchanged = true;
                for (size_t i=0; i < dataVec.size(); i++) {
                    if ((data.get()[i+8] != dataVec[i]) && (i > 3)) {
                        unchanged = false;
                        index = i;
                        std::cout << "Reader different than EvioReader at byte #" << index << std::endl;
                        std::cout << std::showbase << std::hex << +data.get()[i] << " changed to " << +dataVec[i] << std::dec << std::endl;
                        break;
                    }
                }
                if (unchanged) {
                    std::cout << "EVENT same whether using Reader or EvioReader!\n\n";
                }

            }
            catch (EvioException &e) {
                std::cout << "PROBLEM: " << e.what() << std::endl;
            }
        }


    };


}



int main(int argc, char **argv) {
    auto tester = evio::HipoTester();

    // FILES
    tester.testCompactEventCreation(1,1);
    tester.testTreeEventCreation(1,1);

    // BUFFERS
    tester.writeAndReadBuffer(1,1);

    return 0;
}


