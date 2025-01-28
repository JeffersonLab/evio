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


    class DictTest : public TestBase {

    public:

        std::string xmlDict3;
        std::string xmlDict4;
        std::string xmlDict5;
        std::string xmlDict7;

        DictTest() {
            std::stringstream ss;

            ss << "<JUNK>\n" <<
               "<moreJunk/>\n" <<

               "<xmlDict attr='junk'>" <<
               "<leaf name='leaf21' tag= '2.1' num = '2.1' />\n" <<
               "<leaf name='leaf2'  tag= '2'   num = '2' />\n" <<
               "<leaf name='leaf3'  tag= '2'   num = '2' />\n" <<

               "<dictEntry name='pretty-print'  tag= '456' />\n" <<
               "<dictEntry name='first'  tag= '123'   num = '123' />\n" <<
               "<dictEntry name='second'  tag= '123'   num = '123' />\n" <<
               "<dictEntry name='a' tag= '1.7'   num = '1.8' />\n" <<

               "<bank name='b1' tag= '10' num='0' attr ='gobbledy gook' >\n" <<
               "<bank name='b2' tag= '20' num='20' >\n" <<
               "<leaf name='l1' tag= '30' num='31'>\n" <<
               "<bank name='lowest' tag= '111' num='222' />\n" <<
               "</leaf>\n" <<
               "<leaf name='l2' tag= '31' num='32' />\n" <<
               "</bank>\n" <<
               "</bank>\n" <<

               "</xmlDict>\n" <<

               "<xmlDict>\n" <<
               "<leaf name='leaf21' tag= '3.1' num = '3.1' />\n" <<
               "<leaf name='a'  tag= '33'   num = '44' />\n" <<
               "</xmlDict>\n" <<

               "</JUNK>\n";

            xmlDict4 = ss.str();


            std::string description = "\n     i  TDC some comment\n     F  ADC blah min=5\n     N  multiplier\n";

            std::stringstream ss1;
            ss1 << "<xmlDict>" <<
                "<dictEntry name='first'  tag='123'   num ='456' type='ComPosiTe' >" <<
                "<description format='FD2i' >" <<
                description <<
                "</description>" <<
                "</dictEntry>" <<
                "<bank name='b1'   tag='10'   num='0' type='inT32' >" <<
                "<description format='2(N3F)' >" <<
                "this is a bank of signed 32 bit integers" <<
                "</description>" <<
                "</bank>" <<
                "</xmlDict>";

            xmlDict5 = ss1.str();



            // There are several format errors in the dictionary below
            // They'll show up in the printout
            // "description" is a node not an attribute!!

            std::stringstream ss2;
            ss2 << "<xmlDict>\n" <<
                "  <bank name=\"HallD\"          tag=\"6-8\"  >\n" <<
                "      <description format=\"blah\" >" <<
                "          hall_d_tag_range" <<
                "      </description>" <<
                "      <bank name=\"TAG7\"       tag=\"7\"  />\n" <<
                "      <bank name=\"DC(%t)\"     tag=\"6\" num=\"0\" >\n" <<
                "          <description format=\"DC Format\" >tag 6, num 0 bank</description>" <<
                "          <leaf name=\"xpos(%n)\"   tag=\"6\" num=\"1\" type=\"BLAH_BLAH\" />\n" <<
                "          <bank name=\"ypos(%n)\"   tag=\"6\" num=\"2\" />\n" <<
                "          <bank name=\"zpos(%n)\"   tag=\"6\" num=\"3\" />\n" <<
                "          <bank name=\"zpos(%t-%n)\"   tag=\"6\" num=\"4-5\" />\n" <<
                "          <bank name=\"tag72only\"          tag=\"72\" type=\"TAG_72_ONLY\" />\n" <<
                "          <bank name=\"tag72only_dup\"      tag=\"72\" />\n" <<
                "          <bank name=\"tagrange73-74\"      tag=\"73-74\" type=\"BaNk\" />\n" <<
                "          <bank name=\"tagrange73-74_dup\"  tag=\"73-74\" />\n" <<
                "      </bank >\n" <<
                "      <bank name=\"TOF\"        tag=\"8\" num=\"0\"  type=\"bank\" >\n" <<
                "          <bank name=\"ypos(%n)\"   tag=\"6\" num=\"2\" />\n" <<
                "          <bank name=\"duplicate_ypos(2)\"   tag=\"6\" num=\"2\" />\n" <<
                "          <leaf name=\"xpos\"   tag=\"8\" num=\"1\" >\n" <<
                "               <leaf name=\"xpos_leaf\"   tag=\"9\" num=\"1\" />\n" <<
                "          </leaf >\n" <<
                "          <bank name=\"ypos\"   tag=\"8\" num=\"2\" />\n" <<
                "      </bank >\n" <<
                "  </bank >\n" <<
                "  <dictEntry name=\"BadType\" tag=\"55\" type=\"JunkType\" />\n" <<
                "  <dictEntry name=\"BadType??\" tag=\"66\" num=\"6\" type=\"ActualJunk\" />\n" <<
                "  <dictEntry name=\"TaggiesOnly\" tag=\"5\" num=\"3\" type=\"bANk\" >\n" <<
                "       <description format=\"My Format\" >tag 5 description</description>" <<
                "  </dictEntry>\n" <<
                "  <dictEntry name=\"Rangy_Small\" tag=\"75 - 76\"  />\n" <<
                "  <dictEntry name=\"Rangy\"       tag=\"75 - 78\"  />\n" <<
                "  <dictEntry name=\"TAG11\"       tag=\"11\" num=\"ZAP\" />\n" <<
                "  <dictEntry name=\"TAG12\"       tag=\"12\" type=\"bank\" description=\"desc_is_not_attribute\" />\n" <<
                "  <dictEntry name=\"TAG1\"        tag=\"1\" />\n" <<
                "  <dictEntry name=\"TAG1\"        tag=\"2\" />\n" <<
                "  <dictEntry name=\"num=(%t-%n)\"    tag=\"123\" num=\"1-7\" />\n" <<
                "  <dictEntry name=\"num=(7)\"     tag=\"123\" num=\"7\" />\n" <<
                "  <dictEntry name=\"num=(%n)\"     tag=\"123\" />\n" <<
                "</xmlDict>";

            xmlDict7 = ss2.str();
        }




    void testDict7() {

        EvioXMLDictionary dict(xmlDict7, true);

        std::cout << "\nValid dictionary entries:\n" << std::endl;

        std::unordered_map<std::string, std::shared_ptr<EvioDictionaryEntry>> map = dict.getMap();

        // Iterate using range-based for loop
        for (const auto &pair : map) {

            std::shared_ptr<EvioDictionaryEntry> val = pair.second;
            bool numValid = val->isNumValid();
            DataType type = val->getType();
            EvioDictionaryEntry::EvioDictionaryEntryType eType = val->getEntryType();

            std::string key = pair.first;
            uint16_t tag, tagEnd;
            uint8_t num;
            dict.getTagNum(key, &tag, &num, &tagEnd);
            std::string entryType[] = {"TAG_NUM", "TAG_ONLY", "TAG_RANGE"};

            std::cout << "key = " << pair.first <<
                      ", tag=" << tag <<
                      ", tagEnd=" << tagEnd;

            if (numValid) {
                std::cout << ", num=" << +num;
            }
            else {
                std::cout << ", num=<undefined>";
            }

            std::cout << ", type=" << type.toString() << ", entryType=" << entryType[eType] << std::endl;

        }
        std::cout << std::endl;

        std::cout << "\ntagNumMap entries:\n" << std::endl;

        for (const auto &pair : dict.tagNumMap) {
            std::shared_ptr<EvioDictionaryEntry> entry = pair.first;
            std::string val = pair.second;

            std::cout << "VAL = " << val << ": KEY = " << entry->toString() << std::endl;
        }
        std::cout << std::endl;

        uint16_t tag = 6;
        uint8_t num = 2;
        uint16_t tagEnd = 0;

        std::string n = dict.getName(tag, num, tagEnd);
        std::cout << "getName(tag = " << tag << ", num = " << +num <<
                  ", tagEnd = " << tagEnd << ") = " << n << std::endl;

        auto ent = std::make_shared<EvioDictionaryEntry>(tag, num, tagEnd);
        std::string name = dict.tagNumMap.at(ent);

        std::cout << "1 name = " << name << ", for tag = " << tag << ", num = " << +num <<
                  ", tagEnd = " << tagEnd << std::endl;

        auto parentEntry = std::make_shared<EvioDictionaryEntry>(6, 0, 0);
        auto newEntry = std::make_shared<EvioDictionaryEntry>(tag, num, tagEnd, DataType::UNKNOWN32, "", "", parentEntry);
        name = dict.tagNumMap.at(newEntry);

        std::cout << "2 name = " << name << ", for tag = " << tag << ", num = " << +num <<
                  ", tagEnd = " << tagEnd << " and parent = 6/0/0" << std::endl;


        uint16_t pTag = 8;
        uint8_t pNum  = 0;
        uint16_t pTagEnd = 0;

        std::string nm = dict.getName(tag, num, tagEnd, pTag, pNum, pTagEnd);
        std::cout << "getName(tag = " << tag << ", num = " << +num <<
                  ", tagEnd = " << tagEnd << ", pTag = " << pTag <<
                  ", pNum = " << pNum << ", pTagEnd = " << pTagEnd <<
                  ") = " << nm << std::endl;

        std::cout << std::endl;

        std::cout << "description for tag = 5, num = 3, tagEnd = 0 => \"" <<
                  dict.getDescription(5, 3, 0) << "\"" << std::endl;

        std::cout << "description for tag = 6, num = 0, tagEnd = 0 => \"" <<
                  dict.getDescription(6, 0, 0) << "\"" << std::endl;

        std::cout << "description for tag = 6, num = 6, tagEnd = 0 => \"" <<
                  dict.getDescription(6, 6, 0) << "\"" << std::endl;

        std::cout << "format for tag = " << tag << ", num = " << +num <<
                  ", tagEnd = " << tagEnd << " => \"" <<
                  dict.getFormat(tag, num, tagEnd) << "\"" << std::endl;

        std::cout << std::endl;

        std::cout << "Dictionary.toString() gives:\n" << dict.toString() << std::endl;
    }

    void testDict5() {

        EvioXMLDictionary dict(xmlDict5, true);

        std::cout << "Getting stuff for tag = 123, num = 456:"<< std::endl;
        std::cout << "    type        = " << dict.getType(123, (uint8_t)456).toString() << std::endl;
        std::cout << "    name        = " << dict.getName(123, (uint8_t)456) << std::endl;
        std::cout << "    format      = " << dict.getFormat(123, (uint8_t)456) << std::endl;
        std::cout << "    description = " << dict.getDescription(123, (uint8_t)456) << std::endl;

        uint16_t tag;
        uint8_t  num;
        bool found = dict.getTag("first", &tag);
        found = dict.getNum("first", &num);

        std::cout << "Getting stuff for name = \"first\":" << std::endl;
        std::cout << "    tag         = " << tag << std::endl;
        std::cout << "    num         = " << num << std::endl;
        std::cout << "    type        = " << dict.getType("first").toString() << std::endl;
        std::cout << "    format      = " << dict.getFormat("first") << std::endl;
        std::cout << "    description = " << dict.getDescription("first") << std::endl;

        std::cout << "Getting stuff for tag = 10, num = 0:" << std::endl;
        std::cout << "    type        = " << dict.getType(10,0).toString() << std::endl;
        std::cout << "    name        = " << dict.getName(10,0) << std::endl;
        std::cout << "    format      = " << dict.getFormat(10,0) << std::endl;
        std::cout << "    description = " << dict.getDescription(10,0) << std::endl;

        found = dict.getTag("b1", &tag);
        found = dict.getNum("b1", &num);

        std::cout << "Getting stuff for name = \"b1\":" << std::endl;
        std::cout << "    tag         = " << tag << std::endl;
        std::cout << "    num         = " << num << std::endl;
        std::cout << "    type        = " << dict.getType("b1").toString() << std::endl;
        std::cout << "    format      = " << dict.getFormat("b1") << std::endl;
        std::cout << "    description = " << dict.getDescription("b1") << std::endl;
    }


        void testDict4() {

            EvioXMLDictionary dict(xmlDict4, true);

            std::unordered_map<std::string, std::shared_ptr<EvioDictionaryEntry>> map = dict.getMap();

            // Iterate using range-based for loop
            for (const auto &pair : map) {
                //std::shared_ptr<EvioDictionaryEntry> val = pair.second;
                std::string key = pair.first;
                uint16_t tag, tagEnd;
                dict.getTag(key, &tag);
                dict.getTagEnd(key, &tagEnd);
                uint8_t num;
                dict.getNum(key, &num);

                std::cout << "key = " << pair.first <<
                          ", tag = " << tag <<
                          ", tagEnd = " << tagEnd <<
                          ", num = " << num << std::endl;
            }
            std::cout << std::endl;


            std::shared_ptr<EvioEvent> bank20 = EvioEvent::getInstance(456, DataType::BANK, 20);

            std::string dictName = dict.getName(bank20);
            std::cout << "Bank w/ tag=456 / num=20 corresponds to dictionary entry, \"" << dictName << "\"" << std::endl;


            auto bank11 = EvioEvent::getInstance(10, DataType::BANK, 10);

            dictName = dict.getName(bank11);
            std::cout << "Bank w/ tag=10 / num=10 corresponds to dictionary entry, \"" << dictName << "\"" << std::endl;

            EventBuilder builder(bank11);
            auto bank12 = EvioBank::getInstance(20, DataType::SEGMENT, 20);
            try {
                builder.addChild(bank11, bank12);
            }
            catch (EvioException e) {}

            dictName = dict.getName(bank12);
            std::cout << "Bank with tag=20 / num=20 corresponds to dictionary entry, \"" << dictName << "\"" << std::endl;

            auto seg30 = EvioSegment::getInstance(1, DataType::INT32);
            try {
                builder.addChild(bank12, seg30);
            }
            catch (EvioException & e) {}



            dictName = dict.getName(seg30);
            std::cout << "Segment w/ tag 1 corresponds to dictionary entry, \"" << dictName << "\"" << std::endl;

            auto ev = EvioEvent::getInstance(10, DataType::INT32, 10);
            dictName = dict.getName(ev);
            std::cout << "Event w/ tag=10 / num=10 corresponds to dictionary entry, \"" << dictName << "\"" << std::endl;

            auto seg = EvioSegment::getInstance(10, DataType::INT32);
            dictName = dict.getName(seg);
            std::cout << "Segment w/ tag=10 corresponds to dictionary entry, \"" << dictName << "\"" << std::endl;



            auto bk2 = EvioBank::getInstance(2, DataType::INT32, 2);
            dictName = dict.getName(bk2);
            std::cout << "Bank w/ tag=2 / num=2 corresponds to dictionary entry, \"" << dictName << "\"" << std::endl;

            auto bk22 = EvioBank::getInstance(2, DataType::INT32, 2);
            dictName = dict.getName(bk22);
            std::cout << "Another Bank w/ tag=2 / num=2 corresponds to dictionary entry, \"" << dictName << "\"" << std::endl;


            uint16_t tag, tagEnd;
            uint8_t  num;

            std::cout << "TEST NEW FEATURE:" << std::endl;
            bool found = dict.getTagNum("b1.b2.l1", &tag, &num, &tagEnd);
            if (found) {
                std::cout << "Dict entry of b1.b2.l1 has tag = " << tag <<
                                   " and num = " << num << std::endl;
            }

            found = dict.getTagNum("a", &tag, &num, &tagEnd);
            if (found) {
                std::cout << "Dict entry of \"a\" has tag = " << tag <<
                                   " and num = " << num << std::endl;
            }
            else {
                std::cout << "Dict NO entry for \"a\"" << std::endl;
            }

            found = dict.getTagNum("b1.b2.l1.lowest", &tag, &num, &tagEnd);
            if (found) {
                std::cout << "Dict entry of b1.b2.l1.lowest has tag = " << tag <<
                                   " and num = " << num << std::endl;
            }
            else {
                std::cout << "Dict NO entry for b1.b2.l1.lowest" << std::endl;
            }

            std::cout << "\nNo toXml() method in C++" << std::endl;
            std::cout << "\nTEST NEW toString() method:" << std::endl;
            std::cout << dict.toString() << std::endl;
        }



        void testDict3() {

            try {
                uint16_t tag = 1;
                uint8_t  num = 1;

                auto builder = std::make_shared<CompactEventBuilder>(buffer);
                auto buf = createCompactEventBuffer(tag, num, ByteOrder::ENDIAN_LOCAL, 200000, builder);


                EventWriter writer(buffer, dictionary);
                writer.writeEvent(buf);
                writer.close();

                // Read event back out of buffer
                EvioReader reader(writer.getByteBuffer());

                auto ev = reader.parseEvent(1);
                std::cout << "    event ->\n" << ev->treeToString("") << std::endl;
                // This sets the proper pos and lim in buf
                auto bb = builder->getBuffer();
                std::cout << "    buf = \n" << bb->toString() << std::endl;

                EvioXMLDictionary dict(dictionary, 0);
                std::cout << "    dictionary ->\n" << dict.toString() << std::endl << std::endl;

                std::cout << "\n    search, using dictionary for struct = JUNK" << std::endl;
                std::vector<std::shared_ptr<BaseStructure>> vec;
                StructureFinder::getMatchingStructures(ev, "JUNK", dict, vec);
                for (std::shared_ptr<BaseStructure> bs : vec) {
                    std::cout << "      found, thru dict -> " << bs->toString() << std::endl;
                }

                std::cout << "\n";
                vec.clear();

                StructureFinder::getMatchingStructures(ev, "SEG5", dict, vec);
                std::cout << "    find SEG5 -> " << std::endl;
                for (std::shared_ptr<BaseStructure> bs : vec) {
                    std::cout << "      found, thru dict -> " << bs->toString() << std::endl;
                }
                std::cout << "\n";
                vec.clear();

                //<bank name="TopLevel"   tag="1"  num="1" type="bank" >
                //  <bank name="2Level"   tag="201-203"    type="bank" >
                StructureFinder::getMatchingStructures(ev, "Top.2ndLevel", dict, vec);
                std::cout << "    find Top.2ndLevel -> " << std::endl;
                for (std::shared_ptr<BaseStructure> bs : vec) {
                    std::cout << "      found, thru dict -> " << bs->toString() << std::endl;
                }
                std::cout << "\n";
                vec.clear();

                //  <leaf name="TagSegUints"   tag="17" /> <<
                std::cout << "    find Top.2ndLevel.TagSegUints -> " << std::endl;
                StructureFinder::getMatchingStructures(ev, "Top.2ndLevel.TagSegUints", dict, vec);
                for (std::shared_ptr<BaseStructure> bs : vec) {
                    std::cout << "      found, thru dict -> " << bs->toString() << std::endl;
                }
                std::cout << "\n\n";
                vec.clear();


                std::cout << "    find tag & num = 101:" << std::endl;
                DataType type = dict.getType(101, 101);
                if (type == DataType::NOT_A_VALID_TYPE) {
                    std::cout << "      has no type" << std::endl;
                }
                else {
                    std::cout << "      data type = " << type.toString() << std::endl;
                }


                std::cout << "    find Top.2ndLevel.BankUints" << std::endl;
                //std::std::string entry = "Top.2ndLevel";
                std::string entry = "Top.2ndLevel.BankUints";
                //std::std::string entry = "Top.2ndLevel.SegInts";

                if (dict.exists(entry)) {
                    uint16_t tag1, tagEnd1; uint8_t num1;
                    // This is a tag range
                    if (dict.isTagRange(entry)) {
                        dict.getTagNum(entry, &tag1, nullptr, &tagEnd1);
                        std::cout << "      tag range of 2nd Level Banks = " <<
                                  tag1 << " - " << tagEnd1 << std::endl;
                    }
                    else if (dict.isTagNum(entry)) {
                        dict.getTagNum(entry, &tag1, &num1, nullptr);
                        std::cout << "      tag & num of 2nd Level Banks = tag " <<
                                  tag1 << ", num " << +num1 << std::endl;
                    }
                    else if (dict.isTagOnly(entry)) {
                        dict.getTagNum(entry, &tag1, nullptr, nullptr);
                        std::cout << "      tag of 2nd Level Banks = " << tag1  << std::endl;
                    }
                    else {
                        std::cout << "      internal error finding tag/tagEnd/num" << std::endl;
                    }
                }
                else {
                    std::cout << "      no tag for " << entry << std::endl;
                }


                std::cout << "    find Tag 5:" << std::endl;
                bool good = dict.getTag("Tag 5", &tag);
                if (good) {
                    std::cout << "      tag = " << tag << std::endl;
                }
                else {
                    std::cout << "      no dict entry"  << std::endl;
                }


                // Retrieve & print info from dictionary
                entry = "CompositeData";
                std::cout << "\n    Getting stuff for name = \"CompositeData\":" << std::endl;
                dict.getTag(entry, &tag);
                std::cout << "      tag  = " << tag << std::endl;
                dict.getNum("CompositeData",  &num);
                std::cout << "      num = " << num << std::endl;
                std::cout << "      type = " << dict.getType(entry).toString() << std::endl;
                std::cout << "      format = " << dict.getFormat(entry) << std::endl;
                std::cout << "      description = " << dict.getDescription(entry) << std::endl;

                std::cout << "\n    Getting stuff for tag = 8, num = 8:" << std::endl;
                std::cout << "      type = " << dict.getType(8, 8).toString() << std::endl;
                std::cout << "      name = " << dict.getName(8,8) << std::endl;
                std::cout << "      format = " << dict.getFormat(8,8) << std::endl;
                std::cout << "      description = " << dict.getDescription(8,8) << std::endl;

                // first -> name, second -> sharedptr to entry
                std::cout << "\n\n    Print out contents of dictionary:" << std::endl;
                auto & map = dict.getMap();
                for (auto & entry1 : map) {
                    std::cout << "      " << entry1.first << " :   " << entry1.second->toString() << std::endl;
                }

            }
            catch (EvioException &e) {
                std::cout << e.what() << std::endl;
            }
        }


    };
}

int main2() {

    std::unordered_map<std::shared_ptr<evio::EvioDictionaryEntry>, std::string, evio::EvioDictionaryEntry::Hash> map;

    uint16_t tag = 1;
    uint16_t tagEnd = 0;
    uint8_t num = 2;
    evio::DataType type = evio::DataType::UNKNOWN32;
    std::string description = "";
    std::string format = "";


    auto key1 = std::make_shared<evio::EvioDictionaryEntry>(tag, num, tagEnd, type);
    // Using the custom hash function
    evio::EvioDictionaryEntry::Hash hasher;
    std::size_t hash1 = hasher(key1);
    // Print the hash value
    std::cout << "    Hash of key1: " << hash1 << std::endl;

    auto key2 = std::make_shared<evio::EvioDictionaryEntry>(tag, num, tagEnd, type);
    std::size_t hash2 = hasher(key2);
    std::cout << "    Hash of key2: " << hash2 << std::endl;

    auto key3 = std::make_shared<evio::EvioDictionaryEntry>(tag, num, tagEnd, type);
    std::size_t hash3 = hasher(key3);
    std::cout << "    Hash of key3: " << hash3 << std::endl;

    // Insert keys
    map.insert({key1, "First"});
    map.insert({key2, "Second"});;  // Should trigger operator==
    map.insert({key3, "Third"});;


    for (size_t i = 0; i < map.bucket_count(); ++i) {
        std::cout << "Bucket " << i << " contains:";
        for (auto it = map.begin(i); it != map.end(i); ++it) {
            std::cout << " (tag = " << it->first->getTag() << ", num = " << +(it->first->getNum()) << ")";
        }
        std::cout << std::endl;
    }

    return 0;
}

int main(int argc, char **argv) {
    evio::DictTest tester;
    tester.testDict7();
    tester.testDict5();
    tester.testDict4();
    tester.testDict3();
    return 0;
}


