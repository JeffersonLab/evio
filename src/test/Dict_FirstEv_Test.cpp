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
                    std::cout << "createCompactEvents: dictionary ->\n\n" << dict.toString() << std::endl << std::endl;

                    std::cout << "\n\ncreateCompactEvents: search, using dictionary for struct = JUNK" << std::endl;
                    StructureFinder::getMatchingStructures(ev, "JUNK", dict, vec);
                    for (std::shared_ptr<BaseStructure> bs : vec) {
                        std::cout << "createCompactEvents: found, thru dict -> " << bs->toString() << std::endl;
                    }

                    std::cout << "\n\n";
                    vec.clear();

                    StructureFinder::getMatchingStructures(ev, "SEG5", dict, vec);
                    std::cout << "createCompactEvents: find SEG5 -> " << std::endl;
                    for (std::shared_ptr<BaseStructure> bs : vec) {
                        std::cout << "createCompactEvents: found, thru dict -> " << bs->toString() << std::endl;
                    }
                    std::cout << "\n\n";
                    vec.clear();

                    //<bank name="TopLevel"   tag="1"  num="1" type="bank" >
                    //  <bank name="2Level"   tag="201-203"    type="bank" >
                    StructureFinder::getMatchingStructures(ev, "Top.2ndLevel", dict, vec);
                    std::cout << "createCompactEvents: find Top.2ndLevel -> " << std::endl;
                    for (std::shared_ptr<BaseStructure> bs : vec) {
                        std::cout << "createCompactEvents: found, thru dict -> " << bs->toString() << std::endl;
                    }
                    std::cout << "\n\n";
                    vec.clear();

                    //  <leaf name="TagSegUints"   tag="17" /> <<
                    std::cout << "createCompactEvents: find Top.2ndLevel.TagSegUints -> " << std::endl;
                    StructureFinder::getMatchingStructures(ev, "Top.2ndLevel.TagSegUints", dict, vec);
                    for (std::shared_ptr<BaseStructure> bs : vec) {
                        std::cout << "createCompactEvents: found, thru dict -> " << bs->toString() << std::endl;
                    }
                    std::cout << "\n\n";
                    vec.clear();


                    DataType type = dict.getType(101, 101);
                    if (type == DataType::NOT_A_VALID_TYPE) {
                        std::cout << "createCompactEvents: type of tag=101, num=101 has no type" << std::endl;
                    }
                    else {
                        std::cout << "createCompactEvents: type of tag=101, num=101 -> " << type.toString() << std::endl;
                    }


                    //std::string entry = "Top.2ndLevel";
                    std::string entry = "Top.2ndLevel.BankUints";
                    //std::string entry = "Top.2ndLevel.SegInts";

                    if (dict.exists(entry)) {
                        uint16_t tag1, tagEnd1; uint8_t num1;
                        // This is a tag range
                        if (dict.isTagRange(entry)) {
                            dict.getTagNum(entry, &tag1, nullptr, &tagEnd1);
                            std::cout << "createCompactEvents: tag range of 2nd Level Banks = " <<
                                      tag1 << " - " << tagEnd1 << std::endl;
                        }
                        else if (dict.isTagNum(entry)) {
                            dict.getTagNum(entry, &tag1, &num1, nullptr);
                            std::cout << "createCompactEvents: tag & num of 2nd Level Banks = tag " <<
                                      tag1 << ", num " << +num1 << std::endl;
                        }
                        else if (dict.isTagOnly(entry)) {
                            dict.getTagNum(entry, &tag1, nullptr, nullptr);
                            std::cout << "createCompactEvents: tag of 2nd Level Banks = " << tag1  << std::endl;
                        }
                        else {
                            std::cout << "createCompactEvents: internal error finding tag/tagEnd/num" << std::endl;
                        }
                    }
                    else {
                        std::cout << "createCompactEvents: no tag for 2nd Level Banks"  << std::endl;
                    }


                    bool good = dict.getTag("Tag 5", &tag);
                    if (good) {
                        std::cout << "createCompactEvents: tag for dict entry \"Tag 5\" = " << tag << std::endl;
                    }
                    else {
                        std::cout << "createCompactEvents: no dict entry for \"Tag 5\""  << std::endl;
                    }


                    // Retrieve & print info from dictionary
                    entry = "CompositeData";
                    std::cout << "Getting stuff for name = \"CompositeData\":" << std::endl;
                    dict.getTag("CompositeData", &tag);
                    std::cout << "    tag         = " << tag << std::endl;
                    dict.getNum("CompositeData",  &num);
                    std::cout << "    num         = " << +num << std::endl;
                    std::cout << "    type        = " << dict.getType(entry).toString() << std::endl;
                    std::cout << "    format      = " << dict.getFormat(entry) << std::endl;
                    std::cout << "    description = " << dict.getDescription(entry) << std::endl;

                    std::cout << "Getting stuff for tag = 8, num = 8:" << std::endl;
                    std::cout << "    type        = " << dict.getType(8, 8).toString() << std::endl;
                    std::cout << "    name        = " << dict.getName(8,8) << std::endl;
                    std::cout << "    format      = " << dict.getFormat(8,8) << std::endl;
                    std::cout << "    description = " << dict.getDescription(8,8) << std::endl;

                    // first -> name, second -> sharedptr to entry
                    std::cout << "\n\nPrint out contents of dictionary:" << std::endl;
                    auto & map = dict.getMap();
                    for (auto & entry1 : map) {
                        std::cout << "   " << entry1.first << " :   " << entry1.second->toString() << std::endl;
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
                std::cout << "    createObjectEvents: set First Event, size bytes = " << event->getTotalBytes() << std::endl;

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
    tester.createObjectEvents(1,1);
    return 0;
}


