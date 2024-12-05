//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100



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


        /** Writing to a buffer using CompactEventBuilder interface. */
        void testCompactEventCreation(uint16_t tag, uint8_t num) {

            try {
                // Create ByteBuffer with EvioEvent in it
                buffer = createCompactEventBuffer(tag, num);


                Util::printBytes(buffer, 0, 100, "BUFFER BYTES");

                std::cout << "\nBuffer -> \n" << buffer->toString() << "\n";


                //
                // Write file
                //
                Writer writer(HeaderType::EVIO_FILE, ByteOrder::ENDIAN_LOCAL,
                                0, 0, "", nullptr, 0,
                              Compressor::UNCOMPRESSED, false);

                writer.open(writeFileName1);
                writer.addEvent(buffer);
                writer.close();


                Util::printBytes(writeFileName1, 0, 100, "WRITTEN FILE BYTES");

                std::cout << "\n\nAfter writer closed ... \n";

                // Read event back out of file
                Reader reader(writeFileName1);

                std::cout << "createCompactEvents: have dictionary? " << reader.hasDictionary() << std::endl;
                std::string xmlDict = reader.getDictionary();
                std::cout << "createCompactEvents: read dictionary ->\n\n" << xmlDict << std::endl << std::endl;

                std::cout << "createCompactEvents: try getting getNextEvent" << std::endl;

                if (reader.getEventCount() < 1) {
                    std::cout << "createCompactEvents: no data events in file" << std::endl;
                    return;
                }

                uint32_t len;
                std::shared_ptr<uint8_t> bytes = reader.getNextEvent(&len);
                auto ev = EvioReader::getEvent(bytes.get(), len, reader.getByteOrder());
                std::cout << "createCompactEvents: next evio event ->\n" << ev->treeToString("") << std::endl;

                std::shared_ptr<uint8_t> bytes2 = reader.getEvent(0, &len);
                std::cout << "createCompactEvents: get event(0), size = " << std::to_string(len) << std::endl << std::endl;

                ByteBuffer bb1(20000);
                reader.getEvent(bb1, 0);
                std::cout << "createCompactEvents: event 1,  bb1 limit ->\n" << bb1.limit() << std::endl;


                auto bb2 = std::make_shared<ByteBuffer>(20000);
                reader.getEvent(bb2, 0);
                std::cout << "createCompactEvents: event 1,  bb2 limit ->\n" << bb2->limit() << std::endl;
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

                std::cout << "Event:\n" << event->treeToString("") << std::endl;
                std::cout << "Event Header:\n" << event->getHeader()->toString() << std::endl;

                // Take event & write it into buffer
                std::cout << "Write event to " << writeFileName1 << " as compressed LZ4" << std::endl;
                EventWriter writer(writeFileName1, "", "runType", 1, 0L, 0, 0,
                                   ByteOrder::ENDIAN_LOCAL, dictionary, true, false, nullptr, 1, 1, 1, 1,
                                   Compressor::LZ4, 2, 16, 0);

                writer.setFirstEvent(event);
                writer.writeEvent(event);
                std::cout << "    createObjectEvents: call writer.close()" << std::endl;
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
    auto tester = evio::HipoTester();
    //tester.testCompactEventCreation(1,1);
    tester.testTreeEventCreation(1,1);
    return 0;
}


