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


#include <cstdint>
#include <chrono>
#include <memory>
#include <limits>

#include "TestBase.h"

#include "eviocc.h"


using namespace std;


namespace evio {


    class CompactBuilderTest : public TestBase {

    public:

        CompactBuilderTest() = default;


        // Search the buffer created by createObjectEvents
        std::shared_ptr<EvioNode> searchBuffer(uint16_t tag, uint8_t num) {

            std::vector<std::shared_ptr<EvioNode>> returnList;
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


        /**
         * Method to compare 2 ByteBuffers.
         * @param buf1 first buffer.
         * @param buf2 second buffer.
         * @return true if identical, else false.
         */
        static bool compareByteBuffers(std::shared_ptr<ByteBuffer> buf1, std::shared_ptr<ByteBuffer> buf2) {
            if (buf1->remaining() != buf2->remaining()) {
                std::cout << "compareByteBuffers: buffer data lengths differ, " << buf1->remaining() <<
                             " vs " << buf2->remaining() << std::endl;
                return false;
            }

            size_t count = buf1->remaining();
            size_t pos1 = buf1->position(), pos2 = buf2->position();
            bool identical = true;

            for (int i = 0; i < count; i++) {
                if (buf1->getByte(pos1 + i) != buf2->getByte(pos2 + i)) {
                    std::cout << "compareByteBuffers: buffer data differs at relative pos, " << i << std::endl;
                    identical = false;
                }
            }
            return identical;
        }



        /** Writing to a buffer & file using CompactEventBuilder interface. */
        void createCompactEvents(uint16_t tag, uint8_t num) {

            try {
                auto builder = std::make_shared<CompactEventBuilder>(buffer);
                auto evBuf = createCompactEventBuffer(tag, num, ByteOrder::ENDIAN_LOCAL, 200000, builder);

                builder->toFile(writeFileName1);
                // buffer is left in a readable state after above method

                // Read event back out of file
                EvioReader reader(writeFileName1);
                std::cout << "createCompactEvents: try getting first ev from file" << std::endl;
                auto ev = reader.parseEvent(1);
                std::cout << "createCompactEvents: event ->\n" << ev->treeToString("") << std::endl;
                // This sets the proper pos and lim in buffer
                auto bb = builder->getBuffer();
                std::cout << "createCompactEvents: buffer = \n" << bb->toString() << std::endl;
            }
            catch (EvioException &e) {
                std::cout << e.what() << std::endl;
            }
        }



        // TCompare different methods of creating events - for accuracy
        void CompactEBTest() {

            uint16_t tag = 1;
            uint8_t  num = 1;

            std::shared_ptr<ByteBuffer> compactBuf = createCompactEventBuffer(tag, num);

            std::shared_ptr<ByteBuffer> ebBuf = createEventBuilderBuffer(tag, num);

            std::shared_ptr<ByteBuffer> treeBuf = createTreeBuffer(tag, num);

            // First compare events created with CompactEventBuilder to those created by EventBuilder
            if (!compareByteBuffers(compactBuf, ebBuf)) {
                std::cout << "\nCompactEBTest: compactBuf is different than ebBuf\n" << std::endl;

                Util::printBytes(compactBuf, 0, 572, "compact buf");
                Util::printBytes(ebBuf, 0, 572, "EB buf");
            }
            else {
                std::cout << "\nCompactEBTest: compactBuf & ebBuf ARE THE SAME!!!\n" << std::endl;
            }

            // Next compare events created with CompactEventBuilder to those
            // created by EvioEvent tree structure methods.
            if (!compareByteBuffers(compactBuf, treeBuf)) {
                std::cout << "CompactEBTest: compactBuf is different than treeBuf\n" << std::endl;

                Util::printBytes(compactBuf, 0, 572, "compact buf");
                Util::printBytes(treeBuf, 0, 572, "EB buf");
            }
            else {
                std::cout << "\nCompactEBTest: compactBuf & treeBuf ARE THE SAME!!!\n" << std::endl;
            }
        }


    };



}




int main(int argc, char **argv) {
    evio::CompactBuilderTest tester;
    tester.CompactEBTest();
    auto node = tester.searchBuffer(3, 3);

    return 0;
}


