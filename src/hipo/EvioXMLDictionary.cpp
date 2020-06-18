//
// Created by Carl Timmer on 6/18/20.
//

#include "EvioXMLDictionary.h"

namespace evio {

    /** Element containing entire dictionary. */
    std::string const EvioXMLDictionary::DICT_TOP_LEVEL = "xmlDict";

    /** There is only one type of element which directly defines an entry (strange name). */
    std::string const EvioXMLDictionary::ENTRY = "xmldumpDictEntry";

    /** New, alternate, shortened form of ENTRY. */
    std::string const EvioXMLDictionary::ENTRY_ALT = "dictEntry";

    /** Hierarchical container element. */
    std::string const EvioXMLDictionary::ENTRY_BANK = "bank";

    /** Hierarchical leaf element. */
    std::string const EvioXMLDictionary::ENTRY_LEAF = "leaf";

    /** Description element. */
    std::string const EvioXMLDictionary::DESCRIPTION = "description";

    /** The "format" attribute string. */
    std::string const EvioXMLDictionary::FORMAT = "format";

    /** The "type" attribute string. */
    std::string const EvioXMLDictionary::TYPE = "type";

    /** The "name" attribute string. */
    std::string const EvioXMLDictionary::NAME = "name";

    /** The "tag" attribute string. */
    std::string const EvioXMLDictionary::TAG = "tag";

    /** The "num" attribute string. */
    std::string const EvioXMLDictionary::NUM = "num";





}