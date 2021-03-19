//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_EVIOXMLDICTIONARY_H
#define EVIO_6_0_EVIOXMLDICTIONARY_H


#include <stdexcept>
#include <string>
#include <regex>
#include <iostream>
#include <iterator>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <algorithm>

#include "EvioDictionaryEntry.h"
#include "EvioException.h"
#include "Util.h"
#include "BaseStructure.h"
#include "pugixml.hpp"


namespace evio {


    /**
     * This was developed to read the xml dictionary that Maurizio uses for GEMC.<p>
     *
     * <b>An assumption in the following class is that each unique tag/num/tagEnd group
     * corresponds to an equally unique name. In other words, 2 different
     * groups cannot have the same name. And 2 different names cannot map to the
     * same group.</b><p>
     *
     * An entry with only a tag value and no num is allowed. It will match
     * a tag/num pair if no exact match exists but the tag matches. For such an
     * entry, no additional existence of type, format, or description is allowed.<p>
     *
     * Similarly, an entry with a range of tags is also allowed. In this case,
     * no num &amp; type is allowed. It will match
     * a tag/num pair if no exact match exists but the tag is in the range
     * (inclusive).
     *
     * @author heddle
     * @author timmer
     */
    class EvioXMLDictionary {

    private:

        /** Element containing entire dictionary. */
        static const std::string DICT_TOP_LEVEL;

        /** There is only one type of element which directly defines an entry (strange name). */
        static const std::string ENTRY;

        /** New, alternate, shortened form of ENTRY.  */
        static const std::string ENTRY_ALT;

        /** Hierarchical container element. */
        static const std::string ENTRY_BANK;

        /** Hierarchical leaf element. */
        static const std::string ENTRY_LEAF;

        /** Description element. */
        static const std::string DESCRIPTION;

        /** The "format" attribute string. */
        static const std::string FORMAT;

        /** The "type" attribute string. */
        static const std::string TYPE;

        /** The "name" attribute string. */
        static const std::string NAME;

        /** The "tag" attribute string. */
        static const std::string TAG;

        /** The "num" attribute string. */
        static const std::string NUM;

        /**
         * Use regular expressions to parse a tag since it may be of the form:
         * tag="num" or tag="num1 - num2". Allow spaces on either side of minus.
         * @since 5.2
         */
        static std::regex pattern_regex;

        /**
         * The character used to separate hierarchical parts of names.
         * @since 4.0
         */
        std::string const delimiter = ".";

        pugi::xml_document doc;

    public:

        /**
         * This is the heart of the dictionary in which a key is composed of a tag/num
         * pair & other entry data and its corresponding value is a name.
         * Using a map ensures entries are unique.
         * @since 4.0
         */
        std::unordered_map<std::shared_ptr<EvioDictionaryEntry>, std::string> tagNumMap;

        /**
         * Some dictionary entries have only a tag and no num.
         * It matches a tag/num pair if there is no exact match in tagNumMap,
         * but does match a tag in this map.
         * @since 4.1
         */
        std::unordered_map<std::shared_ptr<EvioDictionaryEntry>, std::string> tagOnlyMap;

        /**
         * Some dictionary entries have only a tag range and no num.
         * It matches a tag/num pair if there is no exact match in tagNumMap
         * or in the tagOnlyMap but the tag is within the specified range of an entry.
         * @since 5.2
         */
        std::unordered_map<std::shared_ptr<EvioDictionaryEntry>, std::string> tagRangeMap;


    private:

        /**
         * This is a map in which the key is a name and the value is its
         * corresponding dictionary entry. This map contains all entries whether
         * tag/num, tag-only, or tag-range.
         * @since 5.2
         */
        std::unordered_map<std::string, std::shared_ptr<EvioDictionaryEntry>> reverseMap;

        /**
         * This is a map in which the key is a name and the value is the entry
         * of a corresponding tag/num pair. It's the reverse of the tagNumMap map.
         * @since 4.0
         */
        std::unordered_map<std::string, std::shared_ptr<EvioDictionaryEntry>> tagNumReverseMap;

        /**
         * Top level xml Node object of xml DOM representation of dictionary.
         * @since 4.0
         */
        pugi::xml_node topLevelDoc;

        /**
         * Keep a copy of the string representation around so toString() only does hard work once.
         * @since 4.1
         */
        std::string stringRepresentation;


    public:


        static const std::string &NO_NAME_STRING();

        explicit EvioXMLDictionary(std::string const &path);
        EvioXMLDictionary(std::string const &xml, int dummy);


        void parseXML(pugi::xml_parse_result &domDocument);

        size_t size() const;

        const std::unordered_map<std::string, std::shared_ptr<EvioDictionaryEntry>> &getMap() const;


    private:


        void addHierarchicalDictEntries(std::vector<pugi::xml_node> &kidList,
                                        std::string const &parentName);

    public:

        std::string getName(std::shared_ptr<BaseStructure> &structure);
        std::string getName(uint16_t tag);
        std::string getName(uint16_t tag, uint8_t num);
        std::string getName(uint16_t tag, uint8_t num, uint16_t tagEnd);
        std::string getName(uint16_t tag, uint8_t num, uint16_t tagEnd,
                            uint16_t pTag, uint8_t pNum, uint16_t pTagEnd);
        std::string getName(uint16_t tag, uint8_t num, uint16_t tagEnd,
                            uint16_t pTag, uint8_t pNum, uint16_t pTagEnd,
                            bool numValid = true, bool parentValid = false,
                            bool parentNumValid = false);


    private:


        std::string getName(std::shared_ptr<EvioDictionaryEntry> key);
        std::shared_ptr<EvioDictionaryEntry> entryLookupByData(uint16_t tag, uint8_t num, uint16_t tagEnd);
        std::shared_ptr<EvioDictionaryEntry> entryLookupByName(std::string const &name);


    public:


        std::string getDescription(uint16_t tag, uint8_t num);
        std::string getDescription(uint16_t tag, uint8_t num, uint16_t tagEnd);
        std::string getDescription(std::string const &name);

        std::string getFormat(uint16_t tag, uint8_t num);
        std::string getFormat(uint16_t tag, uint8_t num, uint16_t tagEnd);
        std::string getFormat(std::string const &name);

        DataType getType(uint16_t tag, uint8_t num);
        DataType getType(uint16_t tag, uint8_t num, uint16_t tagEnd);
        DataType getType(std::string const &name);

        bool getTagNum(std::string const &name, uint16_t *tag, uint8_t *num, uint16_t *tagEnd);
        bool getTag(std::string const &name, uint16_t *tag);
        bool getTagEnd(std::string const &name, uint16_t *tagEnd);

        bool getNum(std::string const &name, uint8_t *num);


        std::string toString();
    };

}


#endif //EVIO_6_0_EVIOXMLDICTIONARY_H
