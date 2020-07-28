//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "EvioXMLDictionary.h"


namespace evio {

    // Element containing entire dictionary
    std::string const EvioXMLDictionary::DICT_TOP_LEVEL = "xmlDict";

    // There is only one type of element which directly defines an entry (strange name)
    std::string const EvioXMLDictionary::ENTRY = "xmldumpDictEntry";

    // New, alternate, shortened form of ENTRY
    std::string const EvioXMLDictionary::ENTRY_ALT = "dictEntry";

    // Hierarchical container element
    std::string const EvioXMLDictionary::ENTRY_BANK = "bank";

    // Hierarchical leaf element
    std::string const EvioXMLDictionary::ENTRY_LEAF = "leaf";

    // Description element
    std::string const EvioXMLDictionary::DESCRIPTION = "description";

    // The "format" attribute string
    std::string const EvioXMLDictionary::FORMAT = "format";

    // The "type" attribute string
    std::string const EvioXMLDictionary::TYPE = "type";

    // The "name" attribute string
    std::string const EvioXMLDictionary::NAME = "name";

    // The "tag" attribute string
    std::string const EvioXMLDictionary::TAG = "tag";

    // The "num" attribute string
    std::string const EvioXMLDictionary::NUM = "num";


    // Regular expression to parse tag & num
    std::regex EvioXMLDictionary::pattern_regex("(\\d+)([ ]*-[ ]*(\\d+))?");


    /////////////////////////////////////////////////////


    /**
     * Get a string used to indicate that no name can be determined.
     * @return string used to indicate that no name can be determined.
     */
    const std::string& EvioXMLDictionary::NO_NAME_STRING() {
        return Util::NO_NAME_STRING();
    }


    /**
     * Create an EvioXMLDictionary from an xml file.
     * @param file file containing xml.
     */
    EvioXMLDictionary::EvioXMLDictionary(std::string const & path) {
        pugi::xml_parse_result result = doc.load_file(path.c_str());
        if (!result) {
            std::cout << "XML file parsed with error: \n" << result.description() << std::endl;
            throw EvioException("error parsing xml dictionary file");
        }

        tagNumMap.reserve(100);
        tagOnlyMap.reserve(20);
        tagRangeMap.reserve(20);
        reverseMap.reserve(100);
        tagNumReverseMap.reserve(100);

        parseXML(result);
    };


    /**
     * Create an EvioXMLDictionary from an xml string.
     * @param xmlString string containing xml.
     * @param dummy here only to differentiate from other constructor, value unused.
     */
    EvioXMLDictionary::EvioXMLDictionary(std::string const & xml, int dummy) {
        pugi::xml_parse_result result = doc.load_string(xml.c_str());
        if (!result) {
            std::cout << "XML string parsed with error: \n" << result.description() << std::endl;
            throw EvioException("error parsing xml dictionary string");
        }

        tagNumMap.reserve(100);
        tagOnlyMap.reserve(20);
        tagRangeMap.reserve(20);
        reverseMap.reserve(100);
        tagNumReverseMap.reserve(100);

        parseXML(result);
    };


    /**
     * Create an EvioXMLDictionary from an xml Document object.
     * @param domDocument DOM object representing xml dictionary.
     * @throws EvioException if parsing error.
     */
    void EvioXMLDictionary::parseXML(pugi::xml_parse_result & domDocument) {

        pugi::xml_node topNode = topLevelDoc = doc.child(DICT_TOP_LEVEL.c_str());

        // If no dictionary (child nodes), just return
        if (!topNode.first_child()) {
            return;
        }

        uint16_t tag, tagEnd;
        uint8_t num, numEnd;
        bool badEntry, isTagRange, isNumRange;
        std::string name, tagStr, tagEndStr, numStr, numEndStr, typeStr, format, description;

        // Pick out elements that are both old & new direct entry elements

        uint32_t kidCount = 0;
        uint32_t index = 0;

        // Look at all the children (and creating a list of them)
        std::vector<pugi::xml_node> children;
        for (pugi::xml_node node : topNode.children()) {

            if (node.empty()) continue;

            // Only looking for "xmldumpDictEntry" and "dictEntry" nodes (case insensitive)
            std::string nodeName = node.name();
            if (!Util::iStrEquals(nodeName, ENTRY) &&
                !Util::iStrEquals(nodeName, ENTRY_ALT)) {
                continue;
            }

            tag = tagEnd = num = numEnd = 0;
            badEntry = isTagRange = isNumRange = false;
            name = numStr = tagStr = typeStr = format = description = "";
            DataType type = DataType::UNKNOWN32;

            // Get the NAME attribute
            pugi::xml_attribute attrNode = node.attribute(NAME.c_str());
            if (attrNode.name() == NAME) {
                name = attrNode.value();
            }

            // Check to see if name conflicts with strings set aside to
            // describe evio as xml. There can be substitutions in the name
            // such as %n for num and %t for tag which will be done later,
            // but these will not affect the following check for conflicts
            // with reserved names.
            if (Util::getDataType(name) != DataType::NOT_A_VALID_TYPE ||
                Util::iStrEquals(name, "event") ||
                Util::iStrEquals(name, "evio-data")) {
                std::cout << "IGNORING entry whose name conflicts with reserved strings: " << name << std::endl;
                continue;
            }

            // Get the num or num range as the case may be
            attrNode = node.attribute(NUM.c_str());
            if (attrNode.name() == NUM) {
                // Use regular expressions to parse num
                std::smatch sm;

                // regex_search searches for pattern regexp in the string nodeVal
                std::string const & nodeVal = attrNode.value();
                regex_search(nodeVal, sm, pattern_regex);

                //                cout<<"String that matches the pattern:"<<endl;
                //                for (auto str : sm)
                //                    cout << str << " ";

                if (sm.size() > 1) {
                    // First num is always >= 0
                    numStr = sm[1];
                    try {
                        num = std::stoi(numStr);
                    }
                    catch (std::invalid_argument & e) {
                        badEntry = true;
                    }
                    catch (std::out_of_range & e) {
                        badEntry = true;
                    }

                    // Ending num
                    if (sm.size() > 3) {
                        numEndStr = sm[3];
                        try {
                            numEnd = std::stoi(numEndStr);
                            // The regexp matching only allows values >= 0 for tagEnd.
                            // When tagEnd == 0 or tag == tagEnd, no range is defined.
                            if (numEnd > 0 && (numEnd != num)) {
                                isNumRange = true;
                            }
                        }
                        catch (std::invalid_argument & e) {
                            badEntry = true;
                        }
                        catch (std::out_of_range & e) {
                            badEntry = true;
                        }
                    }
                    else {
                        // Set for later convenience in for loop
                        numEnd = num;
                    }
                }
                else {
                    badEntry = true;
                }
            }

            // If no num defined, substitute "" for each %n
            if (numStr.empty()) {
                name = std::regex_replace(name, std::regex("%n"), "");
                //name = name.replaceAll("%n", "");
            }

            // Get the tag or tag range as the case may be
            attrNode = node.attribute(TAG.c_str());
            if (attrNode.name() == TAG) {

                // Use regular expressions to parse tag
                std::smatch sm;

                // regex_search searches pattern regexp in the string nodeVal
                std::string const & nodeVal = attrNode.value();
                regex_search(nodeVal, sm, pattern_regex);

                if (sm.size() > 1) {
                    // First tag, never null, always >= 0, or no match occurs
                    tagStr = sm[1];
                    try {
                        tag = std::stoi(tagStr);
                    }
                    catch (std::invalid_argument & e) {
                        badEntry = true;
                    }
                    catch (std::out_of_range & e) {
                        badEntry = true;
                    }

                    // Ending tag
                    if (sm.size() > 3) {
                        tagEndStr = sm[3];
                        try {
                            tagEnd = std::stoi(tagEndStr);
                            // The regexp matching only allows values >= 0 for tagEnd.
                            // When tagEnd == 0 or tag == tagEnd, no range is defined.
                            if (tagEnd > 0 && (tagEnd != tag)) {
                                isTagRange = true;
                            }
                        }
                        catch (std::invalid_argument & e) {
                            badEntry = true;
                        }
                        catch (std::out_of_range & e) {
                            badEntry = true;
                        }
                    }
                }
                else {
                    badEntry = true;
                }
            }

            // Scan name for the string "%t".
            // If the tag exists, substitute it for the %t.
            // If tag range defined, substitute "".
            if (isTagRange) {
                if (!numStr.empty()) {
                    // Cannot define num (or num range) and tag range at the same time ...
                    badEntry = true;
                }
                else {
                    name = std::regex_replace(name, std::regex("%t"), "");
                }
            }
            else {
                name = std::regex_replace(name, std::regex("%t"), tagStr);
            }

            // Get the type, if any
            attrNode = node.attribute(TYPE.c_str());
            if (attrNode.name() == TYPE) {
                typeStr = attrNode.value();
                type = DataType::valueOf(typeStr);
            }

            // Look for description node (xml element) as child of entry node
            for (pugi::xml_node const & childNode : node.children()) {

                // Pick out first description element only
                if (childNode.empty()) continue;

                // Only looking for "description" node
                if (!Util::iStrEquals(childNode.name(), DESCRIPTION)) {
                    continue;
                }

                description = childNode.value();
                //System.out.println("FOUND DESCRIPTION H: = " + description);

                // See if there's a format attribute
                pugi::xml_attribute attr = childNode.attribute(FORMAT.c_str());
                if (!attr.empty()) {
                    format = attr.value();
                    //System.out.println("FOUND FORMAT H: = " + format);
                }

                break;
            }

            // Skip meaningless entries
            if (name.empty() || tagStr.empty() || badEntry) {
                std::cout << "IGNORING badly formatted dictionary entry 1: name = " << name << std::endl;
                continue;
            }

            if (numStr.empty() && !typeStr.empty()) {
                std::cout << "IGNORING bad type for this dictionary entry: type = " << typeStr << std::endl;
                typeStr = "";
            }

            // If the num or num range is defined ...
            if (!numStr.empty()) {
                // Make sure num < numEnd
                if (isNumRange && (num > numEnd)) {
                    uint8_t tmp = num;
                    num = numEnd;
                    numEnd = tmp;
                }

                std::string nameOrig = name;

                // Range of nums (num == numEnd for no range)
                for (uint8_t n = num; n <= numEnd; n++) {
                    // Scan name for the string "%n" and substitute num for it
                    name = std::regex_replace(name, std::regex("%n"), std::to_string(n));

                    std::shared_ptr<EvioDictionaryEntry> key =
                            std::make_shared<EvioDictionaryEntry>(tag, n, tagEnd, type, description, format);

                    bool entryAlreadyExists = true;

                    auto it = reverseMap.find(name);
                    if (it != reverseMap.end()) {
                        std::cout << "IGNORING duplicate dictionary entry: name = " <<  name << std::endl;
                    }
                    else {
                        // Only add to dictionary if both name and tag/num pair are unique
                        auto itnm  = tagNumMap.find(key);
                        auto itnrm = tagNumReverseMap.find(name);

                        if ((itnm == tagNumMap.end()) && (itnrm == tagNumReverseMap.end())) {
                            tagNumMap.insert({key, name});
                            tagNumReverseMap.insert({name, key});
                            entryAlreadyExists = false;
                        }
                        else {
                            std::cout << "IGNORING duplicate dictionary entry: name = " <<  name << std::endl;
                        }

                        if (!entryAlreadyExists) {
                            reverseMap.insert({name, key});
                        }
                    }
                }
            }
                // If no num defined ...
            else {
                auto key = std::make_shared<EvioDictionaryEntry>(tag, tagEnd, type, description, format);
                bool entryAlreadyExists = true;

                auto it = reverseMap.find(name);
                if (it != reverseMap.end()) {
                    std::cout << "IGNORING duplicate dictionary entry: name = " <<  name << std::endl;
                }
                else {
                    if (isTagRange) {
                        auto itrm = tagRangeMap.find(key);
                        if (itrm == tagRangeMap.end()) {
                            tagRangeMap.insert({key, name});
                            entryAlreadyExists = false;
                        }
                        else {
                            std::cout << "IGNORING duplicate dictionary entry: name = " <<  name << std::endl;
                        }
                    }
                    else {
                        auto itom  = tagOnlyMap.find(key);
                        if (itom == tagOnlyMap.end()) {
                            tagOnlyMap.insert({key, name});
                            entryAlreadyExists = false;
                        }
                        else {
                            std::cout << "IGNORING duplicate dictionary entry: name = " <<  name << std::endl;
                        }
                    }

                    if (!entryAlreadyExists) {
                        reverseMap.insert({name, key});
                    }
                }
            }

            children.push_back(node);
            kidCount++;
        }

        if (kidCount < 1) return;

        // Look at the (new) hierarchical entry elements,
        // recursively, and add all existing entries.
        addHierarchicalDictEntries(children, "");

    } // end method


    /**
     * Get the number of entries in this dictionary.
     * @return number of entries in this dictionary.
     */
    size_t EvioXMLDictionary::size() const {return tagNumMap.size();}


    /**
     * Get the map in which the key is the entry name and the value is an object
     * containing its data (tag, num, type, etc.).
     * @return  map in which the key is the entry name and the value is an object
     *          containing its data (tag, num, type, etc.).
     */
    const std::unordered_map<std::string, std::shared_ptr<EvioDictionaryEntry>> & EvioXMLDictionary::getMap() const {
        return reverseMap;
    }


    /**
     * Takes a list of the children of an xml node, selects the new
     * hierarchical elements and converts them into a number of dictionary
     * entries which are added to this object.
     * This method acts recursively since any node may contain children.
     *
     * @param kidList a list of the children of an xml node.
     * @param parentName name of the parent xml node.
     */
    void EvioXMLDictionary::addHierarchicalDictEntries(std::vector<pugi::xml_node> & kidList,
                                                       std::string const & parentName) {

        if (kidList.empty()) return;

        uint16_t tag, tagEnd;
        uint8_t  num, numEnd;
        bool badEntry, isTagRange, isNumRange, isLeaf;
        std::string name, tagStr, tagEndStr, numStr, numEndStr, typeStr, format, description;

        // Look at all the children
        for (pugi::xml_node node : kidList) {

            if (node.empty()) continue;
            std::string nodeName = node.name();

            isLeaf = Util::iStrEquals(nodeName, ENTRY_LEAF);

            // Only looking for "bank" and "leaf" nodes
            if (!Util::iStrEquals(nodeName, ENTRY_BANK) && !isLeaf) {
                continue;
            }

            tag = tagEnd = num = numEnd = 0;
            badEntry = isTagRange = isNumRange = false;
            name = numStr = tagStr = typeStr = format = description = "";
            DataType type = DataType::UNKNOWN32;

            // Get the NAME attribute
            pugi::xml_attribute attrNode = node.attribute(NAME.c_str());
            if (attrNode.name() == NAME) {
                name = attrNode.value();
            }

            // Get the num or num range as the case may be
            attrNode = node.attribute(NUM.c_str());
            if (attrNode.name() == NUM) {
                // Use regular expressions to parse num
                std::smatch sm;

                // regex_search searches for pattern regexp in the string nodeVal
                std::string const &nodeVal = attrNode.value();
                regex_search(nodeVal, sm, pattern_regex);

                //                cout<<"String that matches the pattern:"<<endl;
                //                for (auto str : sm)
                //                    cout << str << " ";

                if (sm.size() > 1) {
                    // First num is always >= 0
                    numStr = sm[1];
                    try {
                        num = std::stoi(numStr);
                    }
                    catch (std::invalid_argument &e) {
                        badEntry = true;
                    }
                    catch (std::out_of_range &e) {
                        badEntry = true;
                    }

                    // Ending num
                    if (sm.size() > 3) {
                        numEndStr = sm[3];
                        try {
                            numEnd = std::stoi(numEndStr);
                            // The regexp matching only allows values >= 0 for tagEnd.
                            // When tagEnd == 0 or tag == tagEnd, no range is defined.
                            if (numEnd > 0 && (numEnd != num)) {
                                isNumRange = true;
                            }
                        }
                        catch (std::invalid_argument &e) {
                            badEntry = true;
                        }
                        catch (std::out_of_range &e) {
                            badEntry = true;
                        }
                    }
                    else {
                        // Set for later convenience in for loop
                        numEnd = num;
                    }
                }
                else {
                    badEntry = true;
                }
            }

            // If no num defined, substitute "" for each %n
            if (numStr.empty()) {
                name = std::regex_replace(name, std::regex("%n"), "");
            }

            // Get the tag or tag range as the case may be
            attrNode = node.attribute(TAG.c_str());
            if (attrNode.name() == TAG) {

                // Use regular expressions to parse tag
                std::smatch sm;

                // regex_search searches pattern regexp in the string nodeVal
                std::string const &nodeVal = attrNode.value();
                regex_search(nodeVal, sm, pattern_regex);

                if (sm.size() > 1) {
                    // First tag, never null, always >= 0, or no match occurs
                    tagStr = sm[1];
                    try {
                        tag = std::stoi(tagStr);
                    }
                    catch (std::invalid_argument &e) {
                        badEntry = true;
                    }
                    catch (std::out_of_range &e) {
                        badEntry = true;
                    }

                    // Ending tag
                    if (sm.size() > 3) {
                        tagEndStr = sm[3];
                        try {
                            tagEnd = std::stoi(tagEndStr);
                            // The regexp matching only allows values >= 0 for tagEnd.
                            // When tagEnd == 0 or tag == tagEnd, no range is defined.
                            if (tagEnd > 0 && (tagEnd != tag)) {
                                isTagRange = true;
                            }
                        }
                        catch (std::invalid_argument &e) {
                            badEntry = true;
                        }
                        catch (std::out_of_range &e) {
                            badEntry = true;
                        }
                    }
                }
                else {
                    badEntry = true;
                }
            }

            // Scan name for the string "%t".
            // If the tag exists, substitute it for the %t.
            // If tag range defined, substitute "".
            if (isTagRange) {
                if (!numStr.empty()) {
                    // Cannot define num (or num range) and tag range at the same time ...
                    badEntry = true;
                }
                else {
                    name = std::regex_replace(name, std::regex("%t"), "");
                }
            }
            else {
                name = std::regex_replace(name, std::regex("%t"), tagStr);
            }

            // Get the type, if any
            attrNode = node.attribute(TYPE.c_str());
            if (attrNode.name() == TYPE) {
                typeStr = attrNode.value();
                type = DataType::valueOf(typeStr);
            }

            // Look for description node (xml element) as child of entry node
            for (pugi::xml_node const &childNode : node.children()) {

                // Pick out first description element only
                if (childNode.empty()) continue;

                // Only looking for "description" node
                if (!Util::iStrEquals(childNode.name(), DESCRIPTION)) {
                    continue;
                }

                description = childNode.value();
                //System.out.println("FOUND DESCRIPTION H: = " + description);

                // See if there's a format attribute
                pugi::xml_attribute attr = childNode.attribute(FORMAT.c_str());
                if (!attr.empty()) {
                    format = attr.value();
                    //System.out.println("FOUND FORMAT H: = " + format);
                }

                break;
            }

            // Skip meaningless entries
            if (name.empty() || tagStr.empty() || badEntry) {
                std::cout << "IGNORING badly formatted dictionary entry 1: name = " << name << std::endl;
                continue;
            }

            if (numStr.empty() && !typeStr.empty()) {
                std::cout << "IGNORING bad type for this dictionary entry: type = " << typeStr << std::endl;
                typeStr = "";
            }

            // If the num or num range is defined ...
            if (!numStr.empty()) {
                // Make sure num < numEnd
                if (isNumRange && (num > numEnd)) {
                    uint8_t tmp = num;
                    num = numEnd;
                    numEnd = tmp;
                }

                std::string nameOrig = name;

                // Range of nums (num == numEnd for no range)
                for (uint8_t n = num; n <= numEnd; n++) {
                    // Scan name for the string "%n" and substitute num for it
                    name = std::regex_replace(name, std::regex("%n"), std::to_string(n));

                    // Create hierarchical name
                    if (!parentName.empty()) {
                        name.insert(0, delimiter);
                        name.insert(0, parentName);
                    }

                    // Find the parent entry if any
                    std::shared_ptr<EvioDictionaryEntry> parent = nullptr;
                    auto pName = node.parent().name();
                    if (pName != nullptr) {
                        auto it = reverseMap.find(name);
                        if (it != reverseMap.end()) {
                            parent = it->second;
                        }
                    }

                    auto key = std::make_shared<EvioDictionaryEntry>(tag, n, tagEnd, type, description, format, parent);

                    bool entryAlreadyExists = true;

                    auto it = reverseMap.find(name);
                    if (it != reverseMap.end()) {
                        std::cout << "IGNORING duplicate dictionary entry: name = " << name << std::endl;
                    }
                    else {
                        // Only add to dictionary if both name and tag/num pair are unique
                        auto itnm = tagNumMap.find(key);
                        auto itnrm = tagNumReverseMap.find(name);

                        if ((itnm == tagNumMap.end()) && (itnrm == tagNumReverseMap.end())) {
                            tagNumMap.insert({key, name});
                            tagNumReverseMap.insert({name, key});
                            entryAlreadyExists = false;
                        }
                        else {
                            std::cout << "IGNORING duplicate dictionary entry: name = " << name << std::endl;
                        }

                        if (!entryAlreadyExists) {
                            reverseMap.insert({name, key});
                        }
                    }
                }
            }
                // If no num defined ...
            else {
                if (!parentName.empty()) {
                    name.insert(0, delimiter);
                    name.insert(0, parentName);
                }

                // Find the parent entry if any
                std::shared_ptr<EvioDictionaryEntry> parent = nullptr;
                auto pName = node.parent().name();
                if (pName != nullptr) {
                    auto it = reverseMap.find(name);
                    if (it != reverseMap.end()) {
                        parent = it->second;
                    }
                }

                auto key = std::make_shared<EvioDictionaryEntry>(tag, tagEnd, type, description, format, parent);
                bool entryAlreadyExists = true;

                auto it = reverseMap.find(name);
                if (it != reverseMap.end()) {
                    std::cout << "IGNORING duplicate dictionary entry: name = " << name << std::endl;
                }
                else {
                    if (isTagRange) {
                        auto itrm = tagRangeMap.find(key);
                        if (itrm == tagRangeMap.end()) {
                            tagRangeMap.insert({key, name});
                            entryAlreadyExists = false;
                        }
                        else {
                            std::cout << "IGNORING duplicate dictionary entry: name = " << name << std::endl;
                        }
                    }
                    else {
                        auto itom = tagOnlyMap.find(key);
                        if (itom == tagOnlyMap.end()) {
                            tagOnlyMap.insert({key, name});
                            entryAlreadyExists = false;
                        }
                        else {
                            std::cout << "IGNORING duplicate dictionary entry: name = " << name << std::endl;
                        }
                    }

                    if (!entryAlreadyExists) {
                        reverseMap.insert({name, key});
                    }
                }
            }

            // Create a list of this node's children for recursion
            std::vector<pugi::xml_node> children;
            for (pugi::xml_node n : node.children()) {
                if (n.empty()) continue;
                children.push_back(n);
            }

            // Look at this node's children recursively but skip a leaf's kids
            if (!isLeaf) {
                addHierarchicalDictEntries(children, name);
            }
            else if (!children.empty()) {
                std::cout << "IGNORING children of \"leaf\" element " << name << std::endl;
            }
        }
    };


    /**
     * Returns the name of a given evio structure.
     * This is the method used in BaseStructure.toString()
     * to assign a dictionary entry to a particular evio structure.
     *
     * @param structure the structure to find the name of.
     * @param numValid  is num being used (ie is this an EvioBank / EvioEvent (true) or
     * and EvioSegment / EvioTagSegment (false))?
     * @return a descriptive name or ??? if none found
     */
    std::string EvioXMLDictionary::getName(std::shared_ptr<BaseStructure> & structure) {
        if (structure == nullptr) {
            NO_NAME_STRING();
        }

        auto const & header = structure->getHeader();
        DataType const & type = header->getDataType();
        uint16_t tag = header->getTag();

        if (type.isBank()) {
            uint8_t  num = header->getNumber();
            return getName(tag, num);
        }
        else {
            return getName(tag);
        }
    }


    /**
     * Returns the name associated with the given tag.
     * A search is made for an entry of a tag only.
     * If nothing found, ??? is returned.
     *
     * @param tag  tag of dictionary entry to find
     * @param num  num of dictionary entry to find
     * @return descriptive name or ??? if none found
     */
    std::string EvioXMLDictionary::getName(uint16_t tag) {
        return getName(tag, 0, tag, 0, 0, 0, false);
    }


    /**
     * Returns the name associated with the given tag and num.
     * A search is made for:
     * <ol>
     * <li>an entry of a tag/num pair. If that fails,
     * <li>an entry of a tag only.
     * </ol><p>
     *
     * Argument values which have no match result in "???" being returned.<p>
     *
     * @param tag  tag of dictionary entry to find
     * @param num  num of dictionary entry to find
     * @return descriptive name or ??? if none found
     */
    std::string EvioXMLDictionary::getName(uint16_t tag, uint8_t num) {
        return getName(tag, num, tag);
    }


    /**
     * Returns the name associated with the given tag, num, and tagEnd.
     * A search is made for:
     * <ol>
     * <li>an entry of a tag/num pair. If that fails,
     * <li>an entry of a tag only. If that fails,
     * <li>an entry of a tag range.
     * </ol><p>
     *
     * If a valid tag range is given (different valid tag and tagEnd with no num),
     * a search is made for an entry of a tag range. Note: tag and tagEnd being the
     * same value or tagEnd being 0 mean that no range is defined - it's equivalent
     * to only specifying a tag.<p>
     *
     * Argument values which have no match result in "???" being returned.<p>
     *
     * @param tag       tag of dictionary entry to find
     * @param num       num of dictionary entry to find
     * @param tagEnd tagEnd of dictionary entry to find
     * @return descriptive name or ??? if none found
     */
    std::string EvioXMLDictionary::getName(uint16_t tag, uint8_t num, uint16_t tagEnd) {
        // The generated key below is equivalent (equals() overridden)
        // to the key existing in the map. Use it to find the value.
        auto key = std::make_shared<EvioDictionaryEntry>(tag, num, tagEnd);
        return getName(key);
    }


    /**
     * Returns the name associated with the given tag, num, and tagEnd.
     * If a valid tag and num are given (&gt;= 0) a search is made for:
     * <ol>
     * <li>an entry of a tag/num pair. If that fails,
     * <li>an entry of a tag only. If that fails,
     * <li>an entry of a tag range.
     * </ol><p>
     *
     *
     * If a valid tag range is given (different valid tag and tagEnd with no num),
     * a search is made for an entry of a tag range. Note: tag and tagEnd being the
     * same value or tagEnd being 0 mean that no range is defined - it's equivalent
     * to only specifying a tag.<p>
     *
     * Argument values which have no match result in "???" being returned.<p>
     *
     * Things are actually more complicated due to parent structures. Duplicate
     * entries (same tag, num, and tagEnd) are permitted only as long their
     * parent entries are different. Say, for example, that this dictionary is
     * defined as follows:
     * <pre>
     *
     *      &lt;bank name="B1" tag="1" num="1" &gt;
     *           &lt;bank name="sub1" tag="5" num="5" /&gt;
     *           &lt;bank name="sub2" tag="5" num="5" /&gt;
     *           &lt;leaf name="tagNum"   tag="10" num="10" /&gt;
     *           &lt;leaf name="tagOnly"  tag="20" /&gt;
     *           &lt;leaf name="tagRange" tag="30-40" /&gt;
     *      &lt;/bank&gt;
     *      &lt;bank name="B2" tag="2" num="2" &gt;
     *           &lt;leaf name="tagNum"   tag="10" num="10" /&gt;
     *           &lt;leaf name="tagOnly"  tag="20" /&gt;
     *           &lt;leaf name="tagRange" tag="30-40" /&gt;
     *      &lt;/bank&gt;
     *
     * </pre>
     *
     * You can see that the leaf entries under bank "B1" are identical to those under "B2".
     * This is permitted since B1 and B2 have different tag &amp; num values so there
     * is a way to tell the difference between the 2 instances of tagNum, tagOnly and
     * tagRange.<p>
     *
     * It is not possible to specify parents using the "dictEntry" XML element and
     * consequently duplicates are not allowed if using this form of dictionary
     * definition. Think of things like this: no parents = no duplicates.
     *
     *
     * @param tag       tag of dictionary entry to find
     * @param num       num of dictionary entry to find
     * @param tagEnd tagEnd of dictionary entry to find
     *
     * @param pTag       tag of dictionary entry's parent
     * @param pNum       num of dictionary entry's parent
     * @param pTagEnd tagEnd of dictionary entry's parent
     *
     * @return descriptive name or "???" if none found
     */
    std::string EvioXMLDictionary::getName(uint16_t tag,  uint8_t num,  uint16_t tagEnd,
                                           uint16_t pTag, uint8_t pNum, uint16_t pTagEnd) {
        return getName(tag, num, tagEnd, pTag, pNum, pTagEnd, true, true, true);
    }


    /**
      * Returns the name associated with the given tag, num, and tagEnd.
      * If a valid tag and num are given (&gt;= 0) a search is made for:
      * <ol>
      * <li>an entry of a tag/num pair. If that fails,
      * <li>an entry of a tag only. If that fails,
      * <li>an entry of a tag range.
      * </ol><p>
      *
      * If only a valid tag is given, a search is made for:
      * <ol>
      * <li>an entry of a tag only. If that fails,
      * <li>an entry of a tag range.
      * </ol><p>
      *
      * If a valid tag range is given (different valid tag and tagEnd with no num),
      * a search is made for an entry of a tag range. Note: tag and tagEnd being the
      * same value or tagEnd being 0 mean that no range is defined - it's equivalent
      * to only specifying a tag.<p>
      *
      * Argument values which have no match result in "???" being returned.<p>
      *
      * Things are actually more complicated due to parent structures. Duplicate
      * entries (same tag, num, and tagEnd) are permitted only as long their
      * parent entries are different. Say, for example, that this dictionary is
      * defined as follows:
      * <pre>
      *
      *      &lt;bank name="B1" tag="1" num="1" &gt;
      *           &lt;bank name="sub1" tag="5" num="5" /&gt;
      *           &lt;bank name="sub2" tag="5" num="5" /&gt;
      *           &lt;leaf name="tagNum"   tag="10" num="10" /&gt;
      *           &lt;leaf name="tagOnly"  tag="20" /&gt;
      *           &lt;leaf name="tagRange" tag="30-40" /&gt;
      *      &lt;/bank&gt;
      *      &lt;bank name="B2" tag="2" num="2" &gt;
      *           &lt;leaf name="tagNum"   tag="10" num="10" /&gt;
      *           &lt;leaf name="tagOnly"  tag="20" /&gt;
      *           &lt;leaf name="tagRange" tag="30-40" /&gt;
      *      &lt;/bank&gt;
      *
      * </pre>
      *
      * You can see that the leaf entries under bank "B1" are identical to those under "B2".
      * This is permitted since B1 and B2 have different tag &amp; num values so there
      * is a way to tell the difference between the 2 instances of tagNum, tagOnly and
      * tagRange.<p>
      *
      * It is not possible to specify parents using the "dictEntry" XML element and
      * consequently duplicates are not allowed if using this form of dictionary
      * definition. Think of things like this: no parents = no duplicates.
      *
      *
      * @param tag       tag of dictionary entry to find
      * @param num       num of dictionary entry to find
      * @param tagEnd tagEnd of dictionary entry to find
      *
      * @param pTag       tag of dictionary entry's parent
      * @param pNum       num of dictionary entry's parent
      * @param pTagEnd tagEnd of dictionary entry's parent
      *
      * @param numValid  is num being used?
      * @param parentValid is parent being used?
      * @param parentNumValid if parent is being used, is its num also being used?
      *
      * @return descriptive name or "???" if none found
      */
    std::string EvioXMLDictionary::getName(uint16_t tag,  uint8_t num,  uint16_t tagEnd,
                                           uint16_t pTag, uint8_t pNum, uint16_t pTagEnd,
                                           bool numValid, bool parentValid,
                                           bool parentNumValid) {

        // Do we use parent info?
        if (!parentValid) {
            if (numValid) {
                return getName(tag, num, tagEnd);
            }
            else {
                return getName(tag, tagEnd);
            }
        }

        // The generated key below is equivalent (equals() overridden)
        // to the key existing in the map. Use it to find the value.
        std::shared_ptr<EvioDictionaryEntry> parentEntry;
        if (parentNumValid) {
            parentEntry = std::make_shared<EvioDictionaryEntry>(pTag, pNum, pTagEnd);
        }
        else {
            parentEntry = std::make_shared<EvioDictionaryEntry>(pTag, pTagEnd);
        }

        if (numValid) {
            auto key = std::make_shared<EvioDictionaryEntry>(tag, num, tagEnd,
                                                             DataType::UNKNOWN32, "", "", parentEntry);
            return getName(key);
        }

        auto key = std::make_shared<EvioDictionaryEntry>(tag, tagEnd,
                                                         DataType::UNKNOWN32, "", "", parentEntry);
        return getName(key);
    }


    /**
     * Implementation of getName().
     * @param key dictionary entry to look up name for.
     * @return name associated with key or "???" if none.
     */
    std::string EvioXMLDictionary::getName(std::shared_ptr<EvioDictionaryEntry> key) {

        uint16_t tag = key->getTag();
        EvioDictionaryEntry::EvioDictionaryEntryType entryType = key->getEntryType();

        // name = ???
        std::string name = NO_NAME_STRING();

        switch (entryType) {
            case EvioDictionaryEntry::EvioDictionaryEntryType::TAG_NUM: {
                // If a tag/num pair was specified ...
                // There may be multiple entries with the same tag/tagEnd/num values
                // but having parents with differing values. Since we don't specify
                // the parent info, we just get the first match found in the map.
                auto it = tagNumMap.find(key);
                if (it != tagNumMap.end()) {
                    // name is the "value" of the entry
                    name = it->second;
                    break;
                }

                // Create tag-only key and try to find tag-only match (fall thru case)
                key = std::make_shared<EvioDictionaryEntry>(tag);
            }

            case EvioDictionaryEntry::EvioDictionaryEntryType::TAG_ONLY: {
                // If only a tag was specified or a tag/num pair was specified
                // but there was no exact match for the pair ...
                auto it = tagOnlyMap.find(key);
                if (it != tagOnlyMap.end()) {
                    name = it->second;
                    break;
                }
                // Create tag-range key and try to find tag-range match
                key = std::make_shared<EvioDictionaryEntry>(tag, key->getTagEnd());
            }

            case EvioDictionaryEntry::EvioDictionaryEntryType::TAG_RANGE: {
                // If a range was specified in the args, check to see if
                // there's an exact match first ...
                auto it = tagRangeMap.find(key);
                if (it != tagRangeMap.end()) {
                    name = it->second;
                    break;
                }
                    // If a tag/num pair or only a tag was specified in the args,
                    // see if either falls in a range of tags.
                else if (entryType != EvioDictionaryEntry::EvioDictionaryEntryType::TAG_RANGE) {

                    // Additional check to see if tag fits in a range
                    for (auto & iter : tagRangeMap) {
                        auto entry = iter.first;
                        if (entry->inRange(tag)) {
                            name = iter.second;
                            goto out;
                        }
                    }
                }
            }
            out:

            default:
                std::cout << "no dictionary entry for tag = " << tag << ", tagEnd = " <<
                          key->getTagEnd() << ", num = " << key->getNum() << std::endl;
        }

        return name;
    }


    /**
     * Returns a copy of the dictionary entry, if any, associated with the given
     * tag, num, and tagEnd.
     *
     * @param tag       tag of dictionary entry to find
     * @param num       num of dictionary entry to find
     * @param tagEnd tagEnd of dictionary entry to find
     * @return entry or null if none found
     */
    std::shared_ptr<EvioDictionaryEntry> EvioXMLDictionary::entryLookupByData(uint16_t tag, uint8_t num, uint16_t tagEnd) {

        // Given data, find the entry in dictionary that corresponds to it.
        //
        // The generated key below is equivalent (equals() overridden) to the key existing
        // in the map. Use it to find the value, then use the value to find the
        // original key which contains other data besides tag, tagEnd, and num.
        auto key = std::make_shared<EvioDictionaryEntry>(tag, num, tagEnd);
        EvioDictionaryEntry::EvioDictionaryEntryType entryType = key->getEntryType();

        std::string name;
        std::shared_ptr<EvioDictionaryEntry> entry;

        switch (entryType) {
            case EvioDictionaryEntry::EvioDictionaryEntryType::TAG_NUM : {
                auto it = tagNumMap.find(key);
                if (it != tagNumMap.end()) {
                    // name is the "value" of the entry
                    name = it->second;

                    auto it2 = tagNumReverseMap.find(name);
                    if (it2 != tagNumReverseMap.end()) {
                        entry = it2->second;
                    }
                    break;
                }

                // Create tag-only key and try to find tag-only match
                key = std::make_shared<EvioDictionaryEntry>(tag);
            }
            case EvioDictionaryEntry::EvioDictionaryEntryType::TAG_ONLY : {

                auto it = tagOnlyMap.find(key);
                if (it != tagOnlyMap.end()) {
                    name = it->second;

                    for (auto & iter : tagOnlyMap) {
                        std::string n = iter.second;
                        if (n == name) {
                            entry = iter.first;
                            goto out;
                        }
                    }
                }

                // Create tag-range key and try to find tag-range match
                key = std::make_shared<EvioDictionaryEntry>(tag, tagEnd);
            }
            case EvioDictionaryEntry::EvioDictionaryEntryType::TAG_RANGE : {

                auto it = tagRangeMap.find(key);
                if (it != tagRangeMap.end()) {
                    name  = it->second;
                    entry = it->first;
                }
                    // If a tag/num pair or only a tag was specified in the args,
                    // see if either falls in a range of tags.
                else if (entryType != EvioDictionaryEntry::EvioDictionaryEntryType::TAG_RANGE) {
                    // See if tag fits in a range
                    for (auto & iter : tagRangeMap) {
                        auto entry2 = iter.first;
                        if (entry2->inRange(tag)) {
                            entry = entry2;
                            goto out;
                        }
                    }
                }
            }

            default:
                std::cout << "no dictionary entry for tag = " << tag << ", tagEnd = " <<
                          key->getTagEnd() << ", num = " << key->getNum() << std::endl;
        }

        out:

        return entry;
    }


    /**
     * Returns the dictionary entry, if any, associated with the given name.
     *
     * @param name name associated with entry
     * @return entry or null if none found
     */
    std::shared_ptr<EvioDictionaryEntry> EvioXMLDictionary::entryLookupByName(std::string const & name) {
        // Check all entries
        auto it2 = tagNumReverseMap.find(name);
        if (it2 != tagNumReverseMap.end()) {
            return it2->second;
        }

        std::cout << "entryLookup: no entry for name = " << name << std::endl;
        return nullptr;
    }


    /**
     * Returns the description, if any, associated with the given tag and num.
     *
     * @param tag    to find the description of
     * @param num    to find the description of
     * @return description or null if none found
     */
    std::string EvioXMLDictionary::getDescription(uint16_t tag, uint8_t num) {
        return getDescription(tag, num, tag);
    }


    /**
     * Returns the description, if any, associated with the given tag, num, and tagEnd.
     *
     * @param tag    to find the description of
     * @param num    to find the description of
     * @param tagEnd to find the description of
     * @return description or empty string if none found
     */
    std::string EvioXMLDictionary::getDescription(uint16_t tag, uint8_t num, uint16_t tagEnd) {
        auto entry = entryLookupByData(tag, num, tagEnd);
        if (entry == nullptr) {
            return "";
        }

        return entry->getDescription();
    }


    /**
     * Returns the description, if any, associated with the name of a dictionary entry.
     *
     * @param name dictionary name
     * @return description; empty string if name or is unknown or no description is associated with it
     */
    std::string EvioXMLDictionary::getDescription(std::string const & name) {
        auto entry = entryLookupByName(name);
        if (entry == nullptr) {
            return "";
        }

        return entry->getDescription();
    }


    /**
     * Returns the format, if any, associated with the given tag and num.
     *
     * @param tag to find the format of
     * @param num to find the format of
     * @return the format or null if none found
     */
    std::string EvioXMLDictionary::getFormat(uint16_t tag, uint8_t num) {
        return getFormat(tag, num, tag);
    }


    /**
     * Returns the format, if any, associated with the given tag, num, and tagEnd.
     *
     * @param tag    to find the format of
     * @param num    to find the format of
     * @param tagEnd to find the format of
     * @return  format or null if none found
     */
    std::string EvioXMLDictionary::getFormat(uint16_t tag, uint8_t num, uint16_t tagEnd) {
        auto entry = entryLookupByData(tag, num, tagEnd);
        if (entry == nullptr) {
            return "";
        }

        return entry->getFormat();
    }


    /**
     * Returns the format, if any, associated with the name of a dictionary entry.
     *
     * @param name dictionary name
     * @return format; null if name or is unknown or no format is associated with it
     */
    std::string EvioXMLDictionary::getFormat(std::string const & name) {
        auto entry = entryLookupByName(name);
        if (entry == nullptr) {
            return "";
        }

        return entry->getFormat();
    }


    /**
     * Returns the type, if any, associated with the given tag and num.
     *
     * @param tag to find the type of
     * @param num to find the type of
     * @return type or null if none found
     */
    DataType EvioXMLDictionary::getType(uint16_t tag, uint8_t num) {
        return getType(tag, num, tag);
    }


    /**
     * Returns the type, if any, associated with the given tag, num, and tagEnd.
     *
     * @param tag    to find the type of.
     * @param num    to find the type of.
     * @param tagEnd to find the type of.
     * @return type or DataType::NOT_A_VALID_TYPE if none found.
     */
    DataType EvioXMLDictionary::getType(uint16_t tag, uint8_t num, uint16_t tagEnd) {
        auto entry = entryLookupByData(tag, num, tagEnd);
        if (entry == nullptr) {
            return DataType::NOT_A_VALID_TYPE;
        }

        return entry->getType();
    }


    /**
     * Returns the type, if any, associated with the name of a dictionary entry.
     *
     * @param name dictionary name.
     * @return type; DataType::NOT_A_VALID_TYPE if name or is unknown or no type is associated with it.
     */
    DataType EvioXMLDictionary::getType(std::string const & name) {
        auto entry = entryLookupByName(name);
        if (entry == nullptr) {
            return DataType::NOT_A_VALID_TYPE;
        }

        return entry->getType();
    }


    /**
     * Returns the tag/num/tagEnd values, in an Integer object array,
     * corresponding to the name of a dictionary entry.<p>
     *
     * If there is an exact match with a tag and num pair, it is returned
     * (last element is null).
     * If not, but there is a match with a tag-only entry, that is returned
     * (last 2 elements are null).
     * If no tag-only match exits, but there is a match with a tag range,
     * that is returned (i.e. second element, the num, is null).
     *
     * @since 5.2 now returns 3 Integer objects instead of
     *            2 ints (tag, num) as in previous versions.
     * @param name dictionary name
     * @param tag pointer which gets filled with tag value.
     * @param num pointer which gets filled with num value.
     * @param tagEnd pointer which gets filled with tagEnd value.
     * @return true if entry found, else false.
     */
    bool EvioXMLDictionary::getTagNum(std::string const & name, uint16_t *tag, uint8_t *num, uint16_t *tagEnd) {
        auto entry = entryLookupByName(name);
        if (entry != nullptr) {
            if (tag != nullptr)    {*tag = entry->getTag();}
            if (num != nullptr)    {*num = entry->getNum();}
            if (tagEnd != nullptr) {*tagEnd = entry->getTagEnd();}
            return true;
        }

        return false;
    }


    /**
     * Returns the tag corresponding to the name of a dictionary entry.
     * @param name dictionary name
     * @param tag pointer which gets filled with tag value.
     * @return true if entry found, else false.
     */
    bool EvioXMLDictionary::getTag(std::string const & name, uint16_t *tag) {
        auto entry = entryLookupByName(name);
        if (tag != nullptr) {
            *tag = entry->getTag();
            return true;
        }

        return false;
    }


    /**
     * Returns the tagEnd corresponding to the name of a dictionary entry.
     * @param name dictionary name
     * @param tagEnd pointer which gets filled with tagEnd value.
     * @return true if entry found, else false.
     */
    bool EvioXMLDictionary::getTagEnd(std::string const & name, uint16_t *tagEnd) {
        auto entry = entryLookupByName(name);
        if (entry == nullptr) {
            *tagEnd = entry->getTagEnd();
            return true;
        }

        return false;
    }


    /**
     * Returns the num corresponding to the name of a dictionary entry.
     *
     * @param name dictionary name
     * @return num or null if name unknown
     */
    bool EvioXMLDictionary::getNum(std::string const & name, uint8_t *num) {
        auto entry = entryLookupByName(name);
        if (entry == nullptr) {
            if (num != nullptr) {*num = entry->getNum();}
            return true;
        }

        return false;
    }


    /**
     * Get a string representation of the dictionary.
     * @return a string representation of the dictionary.
     */
    std::string EvioXMLDictionary::toString() {
        if (!stringRepresentation.empty()) return stringRepresentation;

        int row=1;
        std::shared_ptr<EvioDictionaryEntry> entry;
        std::string sb, name;
        sb.reserve(4096);
        sb.append("-- Dictionary --\n\n");

        for (auto const & mapItem : reverseMap) {
            // Get the entry
            name  = mapItem.first;
            entry = mapItem.second;

            uint8_t num = entry->getNum();
            uint16_t tag = entry->getTag();
            uint16_t tagEnd = entry->getTagEnd();

            switch (entry->getEntryType()) {
                case EvioDictionaryEntry::EvioDictionaryEntryType::TAG_RANGE : {
                    std::stringstream ss;
                    ss << std::setw(30) << name << ": tag range " << tag << "-" << tagEnd << std::endl;
                    sb.append(ss.str());
                    break;
                }
                case EvioDictionaryEntry::EvioDictionaryEntryType::TAG_ONLY : {
                    std::stringstream ss;
                    ss << std::setw(30) << name << ": tag " << tag << std::endl;
                    sb.append(ss.str());
                    break;
                }
                case EvioDictionaryEntry::EvioDictionaryEntryType::TAG_NUM : {
                    std::stringstream ss;
                    ss << std::setw(30) << name << ": tag " << tag << ", num " << num << std::endl;
                    sb.append(ss.str());
                    break;
                }
                default: {}
            }

            if (row++ % 4 == 0) {
                sb.append("\n");
            }

        }

        stringRepresentation = sb;
        return stringRepresentation;
    }

}

