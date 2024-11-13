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


#include "TestBase.h"


using namespace std;


namespace evio {


    class Tester : public TestBase {

    public:

        /** Writing to a buffer using new interface. */
        void createCompactEvents(uint16_t tag, uint8_t num) {

            try {

                auto builder = std::make_shared<CompactEventBuilder>(buffer);
                auto buf = createCompactEventBuffer(tag, num, ByteOrder::ENDIAN_LOCAL, 200000, builder);


                if (!writeFileName1.empty()) {
                    EventWriter writer(writeFileName1, dictionary, ByteOrder::ENDIAN_LOCAL, false);
                    writer.setFirstEvent(buf);
                    writer.writeEvent(buf);
                    writer.close();

                    // Read event back out of file
                    EvioReader reader(writeFileName1);

                    std::cout << "createCompactEvents: have dictionary? " << reader.hasDictionaryXML() << std::endl;
                    std::string xmlDict = reader.getDictionaryXML();
                    std::cout << "createCompactEvents: read dictionary ->\n\n" << xmlDict << std::endl << std::endl;

                    std::cout << "createCompactEvents: have first event? " << reader.hasFirstEvent() << std::endl;
                    auto fe = reader.getFirstEvent();
                    std::cout << "createCompactEvents: read first event ->\n\n" << fe->toString() << std::endl << std::endl;

                    std::cout << "createCompactEvents: try getting ev from file" << std::endl;
                    auto ev = reader.parseEvent(1);
                    std::cout << "createCompactEvents: event ->\n" << ev->treeToString("") << std::endl;
                    // This sets the proper pos and lim in buf
                    auto bb = builder->getBuffer();
                    std::cout << "createCompactEvents: buf = \n" << bb->toString() << std::endl;


                    std::cout << "createCompactEvents: create dictionary object\n" << std::endl;
                    std::vector<std::shared_ptr<BaseStructure>> vec;
                    EvioXMLDictionary dict(dictionary, 0);

                    std::cout << "\n\ncreateCompactEvents: search, using dictionary for struct = My Event\n" << std::endl;
                   StructureFinder::getMatchingStructures(ev, "My Event", dict, vec);
                    for (std::shared_ptr<BaseStructure> bs : vec) {
                        std::cout << "createCompactEvents: found, thru dict -> " << bs->toString() << std::endl;
                    }

                    std::cout << "\n\n";
                    vec.clear();

                    StructureFinder::getMatchingStructures(ev, "Tag6Num6", dict, vec);
                    std::cout << "createCompactEvents: find Tag6Num6 -> " << std::endl;
                    for (std::shared_ptr<BaseStructure> bs : vec) {
                        std::cout << "createCompactEvents: found, thru dict -> " << bs->toString() << std::endl;
                    }
                    std::cout << "\n\n";
                    vec.clear();

                    StructureFinder::getMatchingStructures(ev, "Tag 5", dict, vec);
                    std::cout << "createCompactEvents: find Tag 5 -> " << std::endl;
                    for (std::shared_ptr<BaseStructure> bs : vec) {
                        std::cout << "createCompactEvents: found, thru dict -> " << bs->toString() << std::endl;
                    }
                    std::cout << "\n\n";
                    vec.clear();


                    //"  <dictEntry name=\"Tag_%t_A\"      tag=\"13\" />" <<
                    std::cout << "createCompactEvents: find Tag_13_A -> " << std::endl;
                    StructureFinder::getMatchingStructures(ev, "Tag_13_A", dict, vec);
                    for (std::shared_ptr<BaseStructure> bs : vec) {
                        std::cout << "createCompactEvents: found, thru dict -> " << bs->toString() << std::endl;
                    }
                    std::cout << "\n\n";
                    vec.clear();

                    //" <dictEntry name=\"Tag_%t_B\"      tag=\"41\"    num=\"41\" />" <<
                    std::cout << "createCompactEvents: find Tag_41_B -> " << std::endl;
                    StructureFinder::getMatchingStructures(ev, "Tag_41_B", dict, vec);
                    for (std::shared_ptr<BaseStructure> bs : vec) {
                        std::cout << "createCompactEvents: found, thru dict -> " << bs->toString() << std::endl;
                    }
                    std::cout << "\n\n";
                    vec.clear();

                    //"  <dictEntry name=\"TagNumA_%n\"     tag=\"7\"    num=\"7\" />" <<
                    std::cout << "createCompactEvents: find TagNumA_7 -> " << std::endl;
                    StructureFinder::getMatchingStructures(ev, "TagNumA_7", dict, vec);
                    for (std::shared_ptr<BaseStructure> bs : vec) {
                        std::cout << "createCompactEvents: found, thru dict -> " << bs->toString() << std::endl;
                    }
                    std::cout << "\n\n";
                    vec.clear();

                    // "  <dictEntry name=\"TagNumB_%n\"     tag=\"8\"    num=\"4-8\" />" <<
                    std::cout << "createCompactEvents: find TagNumB_8 -> " << std::endl;
                    StructureFinder::getMatchingStructures(ev, "TagNumB_8", dict, vec);
                    for (std::shared_ptr<BaseStructure> bs : vec) {
                        std::cout << "createCompactEvents: found, thru dict -> " << bs->toString() << std::endl;
                    }
                    std::cout << "\n\n";
                    vec.clear();

                    // "  <dictEntry name=\"Tag_%t_Num_%n\"  tag=\"4\"    num=\"4\" />" <<
                    std::cout << "createCompactEvents: find Tag_4_Num_4 -> " << std::endl;
                    StructureFinder::getMatchingStructures(ev, "Tag_4_Num_4", dict, vec);
                    for (std::shared_ptr<BaseStructure> bs : vec) {
                        std::cout << "createCompactEvents: found, thru dict -> " << bs->toString() << std::endl;
                    }
                    std::cout << "\n\n";
                    vec.clear();


                    DataType type = dict.getType(1, 2);
                    std::cout << "createCompactEvents: type of tag=1, num=2 -> " << type.toString() << std::endl;

                    bool good = dict.getTag("2nd Level Bank of Segments", &tag);
                    if (good) {
                        std::cout << "createCompactEvents: tag of  2nd Level Bank of Segments = " << tag << std::endl;
                    }
                    else {
                        std::cout << "createCompactEvents: not tag for  2nd Level Bank of Segments"  << std::endl;
                    }

                    good = dict.getTag("Tag 5", &tag);
                    if (good) {
                        std::cout << "createCompactEvents: tag of  Tag 5 = " << tag << std::endl;
                    }
                    else {
                        std::cout << "createCompactEvents: not tag for  Tag 5"  << std::endl;
                    }

                    good = dict.getTag("Blah", &tag);
                    if (good) {
                        std::cout << "createCompactEvents: tag of  Blah = " << tag << std::endl;
                    }
                    else {
                        std::cout << "createCompactEvents: no tag for  Blah"  << std::endl;
                    }

                    // Retrieve & print info from dictionary
                    std::cout << "Getting stuff for name = \"me\":" << std::endl;
                    dict.getTag("me", &tag);
                    std::cout << "    tag         = " << tag << std::endl;
                    dict.getNum("me",  &num);
                    std::cout << "    num         = " << +num << std::endl;
                    std::cout << "    type        = " << dict.getType("me").toString() << std::endl;
                    std::cout << "    format      = " << dict.getFormat("me") << std::endl;
                    std::cout << "    description = " << dict.getDescription("me") << std::endl;

                    std::cout << "Getting stuff for tag = 100, num = 0:" << std::endl;
                    std::cout << "    type        = " << dict.getType(100, 0).toString() << std::endl;
                    std::cout << "    name        = " << dict.getName(100,0) << std::endl;
                    std::cout << "    format      = " << dict.getFormat(100,0) << std::endl;
                    std::cout << "    description = " << dict.getDescription(100,0) << std::endl;

                    // first -> name, second -> sharedptr to entry
                    std::cout << "\n\nPrint out contents of dictionary:" << std::endl;
                    auto & map = dict.getMap();
                    for (auto & entry : map) {
                        std::cout << "   " << entry.first << " :   " << entry.second->toString() << std::endl;
                    }
                }

            }
            catch (EvioException &e) {
                std::cout << e.what() << std::endl;
            }
        }


        /** Writing to a buffer using original evio interface. */
        void createObjectEvents(uint16_t tag, uint8_t num) {

            try {

                std::shared_ptr<EvioEvent> event = createEventBuilderEvent(tag, num);

                std::cout << "Event:\n" << event->treeToString("") << std::endl;
                std::cout << "Event Header:\n" << event->getHeader()->toString() << std::endl;

                // Take event & write it into buffer
                EventWriter writer(writeFileName1, dictionary, ByteOrder::ENDIAN_LOCAL, false);
                std::cout << "    createObjectEvents: set First Event, size = ? " << event->getTotalBytes() << std::endl;

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
    evio::Tester tester;
    tester.createCompactEvents(1,1);
    //tester.createObjectEvents(1,1);
    return 0;
}


