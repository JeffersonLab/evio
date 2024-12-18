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



        std::streamsize getFileSize(const std::string& filePath) {
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
                                   ByteOrder::ENDIAN_LOCAL, dictionary, true, false, nullptr, 1, 1, 1, 1,
                                   Compressor::LZ4, 2, 16, 0);

                writer.setFirstEvent(event);
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


    };




}



int main(int argc, char **argv) {
    auto tester = evio::HipoTester();
    tester.testCompactEventCreation(1,1);
    //tester.testTreeEventCreation(1,1);
    return 0;
}


