/**
 * Copyright (c) 2025, Jefferson Science Associates
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



#include <iterator>
#include <fstream>


#include "TestBase.h"
#include "eviocc.h"

using namespace std;


namespace evio {


    class CompactReaderAddSubtractTest : public TestBase {


    public:

        static std::shared_ptr<EvioEvent> createSingleEvent(uint16_t tag) {

            // Top level event
            std::shared_ptr<EvioEvent> event = nullptr;

            // data
            std::vector<int32_t> intVec;
            intVec.reserve(2);
            intVec.push_back((int)tag);
            intVec.push_back((int)tag);

            try {
                // Build event (bank of banks) with EventBuilder object
                EventBuilder builder(tag, DataType::BANK, 1);
                event = builder.getEvent();

                    // bank of banks
                    auto bankBanks = EvioBank::getInstance(tag + 1, DataType::BANK, 2);
                    builder.addChild(event, bankBanks);

                        // bank of ints
                        auto bankInts = EvioBank::getInstance(tag + 2, DataType::INT32, 3);
                        builder.setIntData(bankInts, intVec.data(), 2);
                        builder.addChild(bankBanks, bankInts);
            }
            catch (EvioException & e) {
                std::cout << "PROBLEM: " << e.what() << std::endl;
            }

            return event;
        }


        static std::shared_ptr<EvioEvent> createComplexEvent(uint16_t tag) {

            // Top level event
            std::shared_ptr<EvioEvent> event = nullptr;

            // data
            std::vector<int32_t> intVec;
            intVec.reserve(2);
            intVec.push_back((int)tag);
            intVec.push_back((int)tag);

            std::vector<int32_t> intVec2;
            intVec2.reserve(2);
            intVec2.push_back(10*(int32_t)tag);
            intVec2.push_back(10*(int32_t)tag);


            try {
                // Build event (bank of banks) with EventBuilder object
                EventBuilder builder(tag, DataType::BANK, 1);
                event = builder.getEvent();

                // bank of banks
                auto bankBanks = EvioBank::getInstance(tag + 1, DataType::BANK, 2);
                builder.addChild(event, bankBanks);

                // bank of ints
                auto bankInts = EvioBank::getInstance(tag + 2, DataType::INT32, 3);
                builder.setIntData(bankInts, intVec.data(), 2);
                builder.addChild(bankBanks, bankInts);

                // bank of banks
                auto bankBanks2 = EvioBank::getInstance(tag + 3, DataType::BANK, 4);
                builder.addChild(event, bankBanks2);

                // bank of ints
                auto bankInts2 = EvioBank::getInstance(tag + 4, DataType::INT32, 5);
                builder.setIntData(bankInts2, intVec2.data(), 2);
                builder.addChild(bankBanks2, bankInts2);
            }
            catch (EvioException & e) {
                std::cout << "PROBLEM: " << e.what() << std::endl;
            }

            return event;
        }



        static std::shared_ptr<ByteBuffer> createComplexBuffer() {

            // Create a buffer
            std::shared_ptr<ByteBuffer> myBuf = std::make_shared<ByteBuffer>(4100);
            myBuf->order(ByteOrder::ENDIAN_BIG);

            try {
                // Create an event writer to write into "myBuf"
                // When writing a buffer, only 1 (ONE) record is used,
                // so maxEventCount (3rd arg) must be set to accommodate all events to be written!
                EventWriter writer(myBuf);
                EventWriter writer22(myBuf, 4000, 2);

                auto ev1 = createComplexEvent(1);
//std::cout << "\ncreateComplexBuffer: complex event = " << ev1->toString() << std::endl;
                auto ev2 = createSingleEvent(100);
//std::cout << "\ncreateComplexBuffer: simple event = " << ev2->toString() << std::endl;

                // Write events to buffer
                bool added = writer.writeEvent(ev1);
//std::cout << "\ncreateComplexBuffer: added complex event = " << added << std::endl;
                added = writer.writeEvent(ev2);
//std::cout << "\ncreateComplexBuffer: added simple event = " << added << std::endl;

                // All done writing
                writer.close();

                uint32_t eventsWritten = writer.getEventsWritten();
//std::cout << "\ncreateComplexBuffer: events written = " << eventsWritten << "\n\n" << std::endl;

                myBuf = writer.getByteBuffer();
            }
            catch (exception & e) {
                std::cout << "PROBLEM: " << e.what() << std::endl;
            }

            // Get ready to read

            return myBuf;
        }


        /** Add bank w/ addStructure() & remove it w/ removeStructure(). Compare. */
        static void test() {

            try {
                // ready-to-read buf with 2 events
                auto buf = createComplexBuffer();
                EvioCompactReader reader(buf);

                std::cout << "# of events = " << reader.getEventCount() << std::endl;

                auto node1 = reader.getScannedEvent(1);
                auto node2 = reader.getScannedEvent(2);

                int i=0;
                std::cout << "1st event all:" << std::endl;
                for (auto & n : node1->getAllNodes()) {
                    i++;
                    std::cout << "  node " << i << ": " << n->toString() << std::endl;
                }

                std::cout << "\n1st event children:" << std::endl;
                i=0;
                auto kids = node1->getChildNodes();
                if (!kids.empty()) {
                    for (auto & n : kids) {
                        i++;
                        std::cout << "  child node " << i << ": " << n->toString() << std::endl;
                    }
                }

                i=0;
                std::cout << "\n2nd event all:" << std::endl;
                for (auto & n : node2->getAllNodes()) {
                    i++;
                    std::cout << "  node " << i << ": " << n->toString() << std::endl;
                }

                std::cout << "\nnode 1 has all-node-count = " << node1->getAllNodes().size() << std::endl;

                // Add bank of ints to node 1
                uint32_t data1[] = {
                        0x00000002,
                        0x00060b06,
                        0x1,
                };

                // This Only works if dest created with "new" since it becomes part of a shared pointer!!
                auto dest = new uint8_t[12];
                Util::toByteArray(data1, 3, ByteOrder::ENDIAN_BIG, dest);
                auto addBuf = std::make_shared<ByteBuffer>(dest, 12);
                addBuf->order(ByteOrder::ENDIAN_BIG);

                auto origBuf = reader.getByteBuffer();
                std::cout << "  origBuf = " << origBuf->toString() << std::endl;
                Util::printBytes(origBuf, 0, origBuf->limit(), "ORIG  BEFORE");

                //--------------------------------------------------
                // Take a look at what "should be" a modified node
                //--------------------------------------------------
                auto newBuf = reader.addStructure(1, addBuf);
                Util::printBytes(newBuf, 0, newBuf->limit(), "AFTER ADDED BUF");

                reader.setBuffer(newBuf);
                node1 = reader.getScannedEvent(1);
                node2 = reader.getScannedEvent(2);

                std::cout << "1st event after:" << std::endl;
                i = 0;
                for (auto &n : node1->getAllNodes()) {
                    i++;
                    std::cout << "node " << i << ": " << n->toString() << std::endl;
                }

                std::cout << "reader.byteBuffer = " << origBuf->toString() << std::endl;

                if (reader.getByteBuffer() == node1->getBuffer()) {
                    std::cout << "reader and node have same buffer" << std::endl;
                }
                else {
                    std::cout << "reader and node have DIFFERENT buffer" << std::endl;
                }

                std::cout << "\n\nTry removing 2nd event" << std::endl;



                uint32_t nodeKidCount = node1->getChildCount();
                std::cout << "node1 has " << nodeKidCount << " kids" << std::endl;
                auto kidToRemove = node1->getChildAt(nodeKidCount-1);
                std::cout << "node to remove = " << kidToRemove->toString() << std::endl;


                // Remove last child node of first event (the one we just added above)
                auto removedBuf = reader.removeStructure(kidToRemove);
                Util::printBytes(removedBuf, 0, removedBuf->limit(), "REMOVED BUFFER");
                // Reread new buffer which should be same as original
                EvioCompactReader reader2(removedBuf);

                node1 = reader2.getScannedEvent(1);
                node2 = reader2.getScannedEvent(2);

                i=0;
                std::cout << "1st event all:" << std::endl;
                for (auto & n : node1->getAllNodes()) {
                    i++;
                    std::cout << "  node " << i << ": " << n->toString() << std::endl;
                }

                std::cout << "\n1st event children:" << std::endl;
                i=0;
                kids = node1->getChildNodes();
                if (!kids.empty()) {
                    for (auto & n : kids) {
                        i++;
                        std::cout << "  child node " << i << ": " << n->toString() << std::endl;
                    }
                }

                i=0;
                std::cout << "\n2nd event all:" << std::endl;
                for (auto & n : node2->getAllNodes()) {
                    i++;
                    std::cout << "  node " << i << ": " << n->toString() << std::endl;
                }




                // Remove 2nd event
                std::cout << "Node2 is obsolete? " << node2->isObsolete() << std::endl;
                auto removedBuf2 = reader2.removeStructure(node2);
                Util::printBytes(removedBuf2, 0, removedBuf2->limit(), "REMOVED BUFFER");


                // Reread new buffer which should be same as original
                EvioCompactReader reader3(removedBuf2);
                std::cout << "New reader shows " << reader3.getEventCount() << " events" << std::endl;

                node1 = reader3.getScannedEvent(1);
                //   node2 = reader3.getScannedEvent(2);

                i=0;
                std::cout << "1st event all:" << std::endl;
                for (auto & n : node1->getAllNodes()) {
                    i++;
                    std::cout << "  node " << i << ": " << n->toString() << std::endl;
                }

                std::cout << "\n1st event children:" << std::endl;
                i=0;
                kids = node1->getChildNodes();
                if (!kids.empty()) {
                    for (auto & n : kids) {
                        i++;
                        std::cout << "  child node " << i << ": " << n->toString() << std::endl;
                    }
                }


            }
            catch (runtime_error & e) {
                std::cout << "PROBLEM: " << e.what() << std::endl;
            }
        }


    };
}



int main(int argc, char **argv) {

    // Buffers ...
    evio::CompactReaderAddSubtractTest::test();
    cout << endl << endl << "----------------------------------------" << endl << endl;

    return 0;
}
