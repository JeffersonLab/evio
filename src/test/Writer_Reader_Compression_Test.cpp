//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100



#include "TestBase.h"


namespace evio {


    /**
     * Test the compression algorithms.
     *
     * @date 08/27/2020
     * @author timmer
     */
    class CompressionTester : public TestBase {

    public:

        /** Writing compressed data to a file using CompactEventBuilder interface. */
        void createCompactEvents(uint16_t tag, uint8_t num) {

            try {
                auto builder = std::make_shared<CompactEventBuilder>(buffer);
                buffer = createCompactEventBuffer(tag, num, ByteOrder::ENDIAN_LOCAL, 200000, builder);

                //
                // Write file
                //

//                EventWriter::EventWriter(std::string baseName, const std::string & directory,
//                          const std::string & runType,
//                          uint32_t runNumber, uint64_t split,
//                          uint32_t maxRecordSize, uint32_t maxEventCount,
//                          const ByteOrder & byteOrder, const std::string & xmlDictionary,
//                          bool overWriteOK, bool append,
//                          std::shared_ptr<EvioBank> firstEvent, uint32_t streamId,
//                          uint32_t splitNumber, uint32_t splitIncrement, uint32_t streamCount,
//                          Compressor::CompressionType compressionType, uint32_t compressionThreads,
//                          uint32_t ringSize, uint32_t bufferSize) {


                /**
                 * compression = LZ4
                 */
                std::cout << "Write event to " << writeFileName1 << " as compressed LZ4" << std::endl;
                EventWriter writer(writeFileName1, "", "runType", 1, 0L, 0, 0,
                                   ByteOrder::ENDIAN_LOCAL, dictionary, true, false, nullptr, 1, 1, 1, 1,
                                   Compressor::LZ4, 2, 16, 0);

                writer.writeEvent(buffer);
                writer.close();

                // Read event back out of file
                EvioReader reader(writeFileName1);

                std::cout << "createCompactEvents: have dictionary? " << reader.hasDictionaryXML() << std::endl;
                std::string xmlDict = reader.getDictionaryXML();
                std::cout << "createCompactEvents: read dictionary ->\n\n" << xmlDict << std::endl << std::endl;

                std::cout << "createCompactEvents: have first event? " << reader.hasFirstEvent() << std::endl;

                std::cout << "createCompactEvents: try getting ev from file" << std::endl;
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


        /** Writing to a buffer using EventBuilder interface. */
        void createEventBuilderEvents(uint16_t tag, uint8_t num) {

            try {
                auto event = createEventBuilderEvent(tag, num);

                std::cout << "Event:\n" << event->treeToString("") << std::endl;
                std::cout << "Event Header:\n" << event->getHeader()->toString() << std::endl;

                // Take event & write it into file
                std::cout << "Write event to " << writeFileName1 << " as compressed LZ4" << std::endl;
                EventWriter writer(writeFileName1, "", "runType", 1, 0L, 0, 0,
                                   ByteOrder::ENDIAN_LOCAL, dictionary, true, false, nullptr, 1, 1, 1, 1,
                                   Compressor::LZ4, 2, 16, 0);

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
    auto tester = evio::CompressionTester();
    //tester.createCompactEvents(1,1);
    tester.createEventBuilderEvents(1,1);
    return 0;
}


