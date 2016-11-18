/*
 *  evioDictionary.cc
 *
 *   Author:  Elliott Wolin, JLab, 13-Apr-2012
 *   Author:  Carl Timmer, JLab, 17-Oct-2016
*/


#include "evioDictEntry.hxx"
#include "evioDictionary.hxx"
#include <fstream>
#include <cstring>



using namespace std;
using namespace evio;


//-----------------------------------------------------------------------

/**
 * Function to take a string and replace each occurrence of %n with the given string value.
 * Used to take a value of num and place it into a dictionary entry name.
 *
 * @param name  string containing %n - generally name of a dictionary entry.
 * @param n     num value to substitute for all occurrences of %n.
 * @return string resulting from making substitutions.
 */
static string insertNumVal(string name, int n) {
    size_t found = name.find("%n");

    // All this in C++ just to add an int to a string!
    string numStr = "";
    stringstream Stream(numStr);
    Stream << n;
    numStr = Stream.str();

    while (found != string::npos) {
        name.replace(found, 2, numStr);
        found = name.find("%n", found + 1);
    }
    return name;
}

/**
 * Function to take a string and replace each occurrence of %n with the given string value.
 * Used to take a value of num and place it into a dictionary entry name.
 *
 * @param name  reference to string containing %n - generally name of a dictionary entry.
 * @param val   string to substitute for all occurrences of %n.
 */
static void insertNumVal(string &name, string val) {
    size_t found = name.find("%n");
    while (found != string::npos) {
        name.replace(found, 2, val);
        found = name.find("%n", found + 1);
    }
}

/**
 * Function to take a string and replace each occurrence of %t with the given string value.
 * Used to take a value of tag and place it into a dictionary entry name.
 *
 * @param name  reference to string containing %t - generally name of a dictionary entry.
 * @param val   string to substitute for all occurrences of %t.
 */
static void insertTagVal(string &name, string val) {
    size_t found = name.find("%t");

    while (found != string::npos) {
        name.replace(found, 2, val);
        found = name.find("%t", found + 1);
    }
}

//-----------------------------------------------------------------------


/**
 * No-arg constructor contains empty maps.
 */
evioDictionary::evioDictionary() {
}


//-----------------------------------------------------------------------


/**
 * Constructor fills dictionary maps from string.
 * @param dictionaryXML XML string parsed to create dictionary maps
 */
evioDictionary::evioDictionary(const string &dictXML, const string &sep) 
  : dictionaryXML(dictXML), separator(sep), readingDescription(false), parentIsLeaf(false) {
  parseDictionary(dictionaryXML);
}


//-----------------------------------------------------------------------

/**
 * Constructor fills dictionary maps from ifstream.
 * @param dictionaryXML XML string parsed to create dictionary maps
 */
evioDictionary::evioDictionary(ifstream &dictIFS, const string &sep) :
        separator(sep), readingDescription(false), parentIsLeaf(false) {

    if(dictIFS.is_open()) {
      string s;
      while((dictIFS.good())&&(!dictIFS.eof())) {
        getline(dictIFS,s);
        if(s.size()>0)dictionaryXML += s + "\n";
      }
      dictIFS.close();
    } else {
      throw(evioException(0,"?evioDictionary::evioDictionary...unable to read from ifstream",__FILE__,__FUNCTION__,__LINE__));
    }
    
    // now parse XML
    parseDictionary(dictionaryXML);
}


//-----------------------------------------------------------------------

/**
 * Destructor.
 */
evioDictionary::~evioDictionary() {
}


//-----------------------------------------------------------------------


/**
 * Gets dictionary XML
 * @return dictionary XML
 */
string evioDictionary::getDictionaryXML(void) const {
  return(dictionaryXML);
}


//-----------------------------------------------------------------------


/**
 * Uses Expat to parse XML dictionary string and fill maps.
 * @param dictionaryXML XML string
 * @return True if parsing succeeded
 */
bool evioDictionary::parseDictionary(const string &dictionaryXML) {

    // init string parser and start element handler
    XML_Parser xmlParser = XML_ParserCreate(NULL);
    XML_SetElementHandler(xmlParser,startElementHandler,endElementHandler);
    XML_SetUserData(xmlParser,reinterpret_cast<void*>(this));
    XML_SetCharacterDataHandler(xmlParser, charDataHandler);


    // parse XML dictionary
    bool ok = XML_Parse(xmlParser,dictionaryXML.c_str(),(int)dictionaryXML.size(),true)!=0;
    if(!ok) {
        cerr << endl << "  ?evioDictionary::parseDictionary...parse error"
        << endl << endl << XML_ErrorString(XML_GetErrorCode(xmlParser));
    }
    XML_ParserFree(xmlParser);

    return(ok);
}


//-----------------------------------------------------------------------


/**
 * Expat start character data handler, must be static.
 *
 * @param userData void* pointer to evioDictionary instance
 * @param s character data string which is NOT zero terminated!
 * @param len number of valid characters returned at pointer
 */
void evioDictionary::charDataHandler(void *userData, const char *s, int len) {

    // userData points to dictionary
    if (userData == NULL) {
        cerr << "?evioDictionary::startElementHandler...NULL userData" << endl;
        return;
    }

    // Get dictionary from user data
    evioDictionary *d = reinterpret_cast<evioDictionary*>(userData);

    // Only look at data if we're processing a "description" element ...
    if (d->readingDescription) {
        // arg s is not terminated so take care of that
        char txt[256] = {};
        if (len > 255) len = 255;

        for (int i=0; i < len; i++) {
            txt[i] = s[i];
        }
        txt[len] = 0;

        // At this point we want to modify the evioDictEntry
        // entry in all containers (2 maps and a stack).

        // Get entry & associated name from stack
        pair<evioDictEntry, string> & p = d->entryStack.top();
        string name = p.second;
        evioDictEntry & entry = p.first;

        // Update stack entry
        entry.setDescription(txt);

        map<evioDictEntry, string>::iterator iter;

        // Depending on the type of entry, determine which maps to update.
        // Rewrite updated entry back into those maps.
        switch (entry.getEntryType()) {
            case TAG_NUM:
                // Put in maps. To change a key, it must be removed then added again
                iter = d->getNameMap.find(entry);
                if (iter != d->getNameMap.end()) {
                    d->getNameMap.erase(iter);
                    d->getNameMap[entry] = name;
                }

                iter = d->tagNumMap.find(entry);
                if (iter != d->tagNumMap.end()) {
                    d->tagNumMap.erase(iter);
                    d->tagNumMap[entry] = name;
                }

                d->getTagNumMap[name]     = entry;
                d->tagNumReverseMap[name] = entry;
                break;

            case TAG_ONLY:
                iter = d->getNameMap.find(entry);
                if (iter != d->getNameMap.end()) {
                    d->getNameMap.erase(iter);
                    d->getNameMap[entry] = name;
                }

                iter = d->tagOnlyMap.find(entry);
                if (iter != d->tagOnlyMap.end()) {
                    d->tagOnlyMap.erase(iter);
                    d->tagOnlyMap[entry] = name;
                }

                d->getTagNumMap[name] = entry;
                break;

            case TAG_RANGE:
                iter = d->getNameMap.find(entry);
                if (iter != d->getNameMap.end()) {
                    d->getNameMap.erase(iter);
                    d->getNameMap[entry] = name;
                }

                iter = d->tagRangeMap.find(entry);
                if (iter != d->tagRangeMap.end()) {
                    d->tagRangeMap.erase(iter);
                    d->tagRangeMap[entry] = name;
                }

                d->getTagNumMap[name] = entry;
                break;

            default:
                break;
        }

        // Done reading description element
        d->readingDescription = false;
    }
}



//-----------------------------------------------------------------------


/**
 * Expat start element handler, must be static.
 * @param userData void* pointer to evioDictionary instance
 * @param xmlname Name of current element
 * @param atts Array of attributes for this element
 */
void evioDictionary::startElementHandler(void *userData, const char *xmlname, const char **atts) {

    //cout << "Start element " << xmlname << endl;

    // userData points to dictionary
    if (userData == NULL) {
        cerr << "?evioDictionary::startElementHandler...NULL userData" << endl;
        return;
    }

    // get dictionary from user data
    evioDictionary *d = reinterpret_cast<evioDictionary*>(userData);

    // only process dictionary entries, make xml name lower case
    string xmlnameLC = xmlname;
    std::transform(xmlnameLC.begin(), xmlnameLC.end(), xmlnameLC.begin(), (int(*)(int)) tolower);  // magic

    // Look at child description element if any
    if (xmlnameLC == "description") {
        d->readingDescription = true;

        // Get any format attribute if any
        for (int i=0; atts[i]; i+=2) {
            if (strcasecmp(atts[i], "format")==0) {
                // At this point we want to modify the evioDictEntry
                // entry in all containers (2 maps and a stack).

                // Get entry & associated name from stack
                pair<evioDictEntry, string> & p = d->entryStack.top();
                string name = p.second;
                evioDictEntry & entry = p.first;

                // Update stack entry
                entry.setFormat(atts[i+1]);

                map<evioDictEntry, string>::iterator iter;

                // Depending on the type of entry, determine which maps to update.
                // Rewrite updated entry back into those maps.
                switch (entry.getEntryType()) {
                    case TAG_NUM:
                        // Put in maps. To change a key, it must be removed then added again
                        iter = d->getNameMap.find(entry);
                        if (iter != d->getNameMap.end()) {
                            d->getNameMap.erase(iter);
                            d->getNameMap[entry] = name;
                        }

                        iter = d->tagNumMap.find(entry);
                        if (iter != d->tagNumMap.end()) {
                            d->tagNumMap.erase(iter);
                            d->tagNumMap[entry] = name;
                        }

                        d->getTagNumMap[name]     = entry;
                        d->tagNumReverseMap[name] = entry;
                        break;

                    case TAG_ONLY:
                        iter = d->getNameMap.find(entry);
                        if (iter != d->getNameMap.end()) {
                            d->getNameMap.erase(iter);
                            d->getNameMap[entry] = name;
                        }

                        iter = d->tagOnlyMap.find(entry);
                        if (iter != d->tagOnlyMap.end()) {
                            d->tagOnlyMap.erase(iter);
                            d->tagOnlyMap[entry] = name;
                        }

                        d->getTagNumMap[name] = entry;
                        break;

                    case TAG_RANGE:
                        iter = d->getNameMap.find(entry);
                        if (iter != d->getNameMap.end()) {
                            d->getNameMap.erase(iter);
                            d->getNameMap[entry] = name;
                        }

                        iter = d->tagRangeMap.find(entry);
                        if (iter != d->tagRangeMap.end()) {
                            d->tagRangeMap.erase(iter);
                            d->tagRangeMap[entry] = name;
                        }

                        d->getTagNumMap[name] = entry;
                        break;

                    default:
                        break;
                };
                break;
            }
        }

        // The description text is handled in charDataHandler()
        return;
    }

    // Is this a hierarchical entry (bank or leaf)?
    bool hierarchicalEntry = false;

    // Look at bank, leaf elements
    if ((xmlnameLC == "bank") || (xmlnameLC == "leaf")) {
        hierarchicalEntry = true;
    }
    // Also look at dictentry, and xmldumpdictdntry elements
    else if ((xmlnameLC == dictEntryTag) || (xmlnameLC == oldDictEntryTag)) {
        // Good
    }
    // Ignore everything else
    else {
        return;
    }

    if (d->parentIsLeaf) {
        throw(evioException(0,"?evioDictionary::startElementHandler...parent bank is leaf!",__FILE__,__FUNCTION__,__LINE__));
    }

    // init variables
    string name = "";

    int tag = 0, num = 0, tagEnd = 0, numEnd = 0, isTagRange = 0, isNumRange = 0;
    bool numIsDefined = false, nameIsDefined = false, tagIsDefined = false, typeIsDefined = false;
    DataType type = EVIO_UNKNOWN32;

    // Look at name, tag, num, and type  attributes
    for (int i=0; atts[i]; i+=2) {
        if (strcasecmp(atts[i], "name")==0) {
            name = string(atts[i+1]);
            nameIsDefined = true;
        }
        else if (strcasecmp(atts[i], "tag")==0) {
            // Tag may be a range (tag="1 - 4").
            // First get leading integer (tag or first int of range)
            tag = atoi(atts[i+1]);
            // Go past any "-"
            const char *minus = strstr(atts[i+1], "-");
            if (minus != NULL) {
                // Get last integer of range
                tagEnd = atoi(minus + 1);
                isTagRange = 1;
            }
            tagIsDefined = true;
        }
        else if (strcasecmp(atts[i], "num")==0) {
            // Num may also be a range (num="1 - 4").
            num = atoi(atts[i+1]);
            // Go past any "-"
            const char *minus = strstr(atts[i+1], "-");
            if (minus != NULL) {
                // Get last integer of range
                numEnd = atoi(minus + 1);
                isNumRange = 1;
            }
            numIsDefined = true;
        }
        else if (strcasecmp(atts[i], "type")==0) {
            type = evioDictEntry::getDataType(atts[i+1]);
            typeIsDefined = true;
            cout << "type = " << type << endl;
        }
    }


    // Catch meaningless entries
    if (!nameIsDefined || !tagIsDefined) {
        throw(evioException(0,"?evioDictionary::startElementHandler...name and/or tag not defined"
                ,__FILE__,__FUNCTION__,__LINE__));
    }

    // Ignore the type if num not defined
    if (!numIsDefined && typeIsDefined) {
        typeIsDefined = false;
        type = EVIO_UNKNOWN32;
    }


    // Check to see if name attribute conflicts with strings set aside to
    // describe evio as xml. There can be substitutions in the name
    // such as %n for num and %t for tag which will be done later,
    // but these will not affect the following check for conflicts
    // with reserved names.
    for (int i=0; i < 18; i++) {
        if (strcasecmp(DataTypeNames[i], name.c_str()) == 0) {
            throw(evioException(0,"?evioDictionary::startElementHandler...xml using reserved name (" + name + ")"
                    ,__FILE__,__FUNCTION__,__LINE__));
        }
    }
    if (strcasecmp("event", name.c_str()) == 0 || strcasecmp("evio-data", name.c_str()) == 0) {
        throw(evioException(0,"?evioDictionary::startElementHandler...xml using reserved name = event or evio-data"
                ,__FILE__,__FUNCTION__,__LINE__));
    }

    // If no num defined, substitute "" for each %n
    if (!numIsDefined) {
        insertNumVal(name, "");
    }

    // Scan name for the string "%t".
    // If the tag exists, substitute it for the %t.
    // If tag range defined, substitute "".
    if (isTagRange) {
        if (numIsDefined) {
            throw(evioException(0,"?evioDictionary::startElementHandler...cannot define both tag range and num value"
                    ,__FILE__,__FUNCTION__,__LINE__));
        }
        else {
            insertTagVal(name, "");
        }
    }
    else {
        // All this in C++ just to add an int to a string!
        string tagStr = "";
        stringstream Stream(tagStr);
        Stream << tag;
        tagStr = Stream.str();

        insertTagVal(name, tagStr);
    }

    // create full name using parent prefix for hierarchical dictionary tags
    string fullName = name;
    if (xmlnameLC!=dictEntryTag && xmlnameLC!=oldDictEntryTag) {
        if(d->parentPrefix.empty()) {
            d->parentPrefix = name;
        }
        else {
            fullName = d->parentPrefix + d->separator + name;
            d->parentPrefix += d->separator + name;
        }
        d->parentIsLeaf = xmlnameLC=="leaf";
    }
    name = fullName;

    // For hierarchical entries, we need parent tag/tagEnd/num values since
    // we allow for identical tag/tagEnd/num if parents are different.
    uint8_t parentNum = 0;
    uint16_t parentTag = 0, parentTagEnd = 0;
    bool hasParent = false;

    if (hierarchicalEntry && d->entryStack.size() > 0) {
        // Find parent's tag/tagEnd/num values
        pair<evioDictEntry, string> &p = d->entryStack.top();
        hasParent = true;
        parentNum = p.first.getNum();
        parentTag = p.first.getTag();
        parentTagEnd = p.first.getTagEnd();
    }

    // If the num or num range is defined ...
    if (numIsDefined) {
        // Make sure num < numEnd if range defined
        if (isNumRange) {
            if (num > numEnd) {
                int tmp = num;
                num = numEnd;
                numEnd = tmp;
            }
        }
        else {
            numEnd = num;
        }

        string nameOrig = name;

        // Range of nums (num == numEnd for no range)
        for (int n = num; n <= numEnd; n++) {
            // Scan name for the string "%n" and substitute num for it
            name = insertNumVal(nameOrig, n);

            // add tag/num pair and full (hierarchical) name to maps, also check for duplicates
            evioDictEntry entry = evioDictEntry((uint16_t)tag, (uint8_t)n, (uint16_t)tagEnd,
                                                hasParent, parentTag, parentNum, parentTagEnd, type);

//            cerr << "Entry: name = " << name << ", tag = " << entry.getTag() <<
//            ", tagEnd = " << entry.getTagEnd() << ", num = " << (int)entry.getNum() <<
//            ", hasParent = " << hasParent <<", pTag = " << parentTag <<
//            ", pTagEnd = " << parentTagEnd << ", pNum = " << (int)parentNum << endl;

            if (d->getTagNumMap.find(name) != d->getTagNumMap.end()) {
                //cerr << "Duplicate entry" << endl;
                throw(evioException(0,"?evioDictionary::startElementHandler...duplicate entry in dictionary!",
                                    __FILE__,__FUNCTION__,__LINE__));
            }
            else {
                // Only add to dictionary if both name and tag/num pair are unique
                if ( (d->getNameMap.find(entry) == d->getNameMap.end()) &&
                     (d->tagNumReverseMap.find(name) == d->tagNumReverseMap.end()) ) {
                    //cerr << "Add tag/num entry: name = " << name << ", tag = " << entry.getTag() << ", tagEnd = " << tagEnd <<
                    //        ", num = " << (int)entry.getNum() << endl;

                    // Put in maps
                    d->getNameMap[entry] = name;
                    d->tagNumMap[entry]  = name;

                    d->getTagNumMap[name]     = entry;
                    d->tagNumReverseMap[name] = entry;

                    // Store this entry on a stack so we can add a description and format
                    // from elements lower down if necessary.
                    d->entryStack.push(make_pair(entry, name));
                    //cerr << "Adding normal entry" << endl;
                }
                else {
                    //cerr << "Duplicate entry" << endl;
                    throw(evioException(0,"?evioDictionary::startElementHandler...duplicate entry in dictionary!",
                                        __FILE__,__FUNCTION__,__LINE__));
                }
            }
        }
    }
    // If no num defined ...
    else {
        evioDictEntry entry = evioDictEntry((uint16_t)tag, (uint8_t)0, (uint16_t)tagEnd,
                                            hasParent, parentTag, parentNum, parentTagEnd, type, true);

//        cerr << "Entry: name = " << name << ", tag = " << entry.getTag() <<
//        ", tagEnd = " << entry.getTagEnd() << ", num = " << (int)entry.getNum() <<
//        ", hasParent = " << hasParent <<", pTag = " << parentTag <<
//        ", pTagEnd = " << parentTagEnd << ", pNum = " << (int)parentNum << endl;

        if (d->getTagNumMap.find(name) != d->getTagNumMap.end()) {
            //cerr << "Duplicate entry" << endl;

            throw(evioException(0,"?evioDictionary::startElementHandler...duplicate entry in dictionary!",
                                __FILE__,__FUNCTION__,__LINE__));
        }
        else {
            if (isTagRange) {
                if (d->tagRangeMap.find(entry) == d->tagRangeMap.end()) {
                    //cerr << "Add range entry: name = " << name << ", tag = " << entry.getTag()  << ", tagEnd = "
                    //<< tagEnd << ", num undefined" << endl;

                    // Put in maps
                    d->tagRangeMap[entry] = name;
                    d->getNameMap[entry]  = name;

                    d->getTagNumMap[name] = entry;

                    d->entryStack.push(make_pair(entry, name));
                    //cerr << "Adding range entry" << endl;
                }
                else {
                    //cerr << "Duplicate entry" << endl;
                    throw(evioException(0,"?evioDictionary::startElementHandler...duplicate entry in dictionary!",
                                        __FILE__,__FUNCTION__,__LINE__));
                }
            }
            else {
                if (d->tagOnlyMap.find(entry) == d->tagOnlyMap.end()) {
                    //cerr << "Add tag only entry: name = " << name << ", tag = " << entry.getTag() << endl;

                    // Put in maps
                    d->tagOnlyMap[entry] = name;
                    d->getNameMap[entry] = name;

                    d->getTagNumMap[name] = entry;

                    d->entryStack.push(make_pair(entry, name));
                    //cerr << "Adding tag entry" << endl;
                }
                else {
                    //cerr << "Duplicate entry" << endl;
                    throw(evioException(0,"?evioDictionary::startElementHandler...duplicate entry in dictionary!",
                                        __FILE__,__FUNCTION__,__LINE__));
                }
            }
        }
    }

}


//-----------------------------------------------------------------------


/**
 * Expat end element handler, must be static.
 * @param userData void* pointer to evioDictionary instance
 * @param xmlname Name of current element
 */
void evioDictionary::endElementHandler(void *userData, const char *xmlname) {
    //cout << "End element " << xmlname << endl;

    evioDictionary *d = reinterpret_cast<evioDictionary*>(userData);

    string xmlnameLC = xmlname;
    std::transform(xmlnameLC.begin(), xmlnameLC.end(), xmlnameLC.begin(), (int(*)(int)) tolower);  // magic

    if ((xmlnameLC == "bank") ||
        (xmlnameLC == "leaf") ||
        (xmlnameLC == dictEntryTag) ||
        (xmlnameLC == oldDictEntryTag)) {

        d->parentIsLeaf = false;

        // Set name back to previous level's name
        unsigned long spos = d->parentPrefix.rfind(d->separator);
        if(spos == d->parentPrefix.npos) {
            d->parentPrefix.clear();
        } else {
            d->parentPrefix.erase(spos);
        }

        // Done with this xml element so remove from stack
        d->entryStack.pop();
    }
}


//-----------------------------------------------------------------------


/**
 * Gets the dictionary entry (evioDictEntry) for a given name.
 *
 * @param name name of dictionary entry
 * @return dictionary entry
 * @throws evioException if entry not found
 */
evioDictEntry evioDictionary::getEntry(const string &name) const throw(evioException) {
    map<string, evioDictEntry>::const_iterator iter = getTagNumMap.find(name);
    if (iter != getTagNumMap.end()) {
        return iter->second;
    }
    else {
        throw(evioException(0,"?evioDictionary::getEntry...no entry named "+name,__FILE__,__FUNCTION__,__LINE__));
    }
}


//-----------------------------------------------------------------------


/**
 * Gets the name associated with an evioDictEntry.
 *
 * @since 5.2
 * @param entry dictionary entry
 * @return name associated with entry
 * @throws evioException if entry not found
 */
string evioDictionary::getName(evioDictEntry &entry) const throw(evioException) {
    // First, see if there is an exact match in map which contains all entries
    map<evioDictEntry, string>::const_iterator iter = getNameMap.find(entry);
    if (iter != getNameMap.end()) {
        return iter->second;
    }

    // If we're here, there's no exact match but there can still be a match
    // with a tag-only or tag-range entry.
    return getName(entry.getTag(), entry.getNum(), entry.getTagEnd());
}


//-----------------------------------------------------------------------


/**
 * Gets the name associated with an evioDictEntry which has the given tag, num, tagEnd,
 * and parent tag, num, and tagEnd values.
 *
 * @since 5.2
 * @param tag dictionary entry's tag value.
 * @param num dictionary entry's num value.
 * @param tagEnd dictionary entry's tagEnd value (ending value of a range of tags and > tag).
 *               Defaults to 0.
 * @param haveParent true if entry has a parent entry, else false. Defaults to false.
 * @param parentTag parent entry's tag value. Defaults to 0.
 * @param parentNum parent entry's num value. Defaults to 0.
 * @param parentTagEnd parent entry's tagEnd value. Defaults to 0.
 *
 * @return name associated with arguments
 * @throws evioException if entry not found
 */
string evioDictionary::getName(uint16_t tag, uint8_t num, uint16_t tagEnd, bool haveParent,
                               uint16_t parentTag, uint8_t parentNum, uint16_t parentTagEnd)
                                                                        const throw(evioException) {

    // The generated key below is equivalent to the key existing in the map. Use it to find the value.
    evioDictEntry key, key1, key2;
    key = key1 = key2 = evioDictEntry(tag, num, tagEnd, haveParent,
                                      parentTag, parentNum, parentTagEnd, EVIO_UNKNOWN32);
    DictEntryType entryType = key.getEntryType();

    string name = "";
    map<evioDictEntry, string>::const_iterator pos;

    switch (entryType) {
        case TAG_NUM:
            // Look first for a tag/num pair ...
            pos = tagNumMap.find(key);
            if (pos != tagNumMap.end()) {
                name = pos->second;
                //cout << "getName: found key in tagNumMap, val = " << name << endl;
                break;
            }
            // Create tag-only key and try to find tag-only match
            key1 = evioDictEntry((uint16_t) tag);

        case TAG_ONLY:
            // If only a tag was specified or a tag/num pair was specified
            // but there was no exact match for the pair ...
            pos = tagOnlyMap.find(key1);
            if (pos != tagOnlyMap.end()) {
                name = pos->second;
                //cout << "getName: found key in tagOnlyMap, val = " << name << endl;
                break;
            }
            // Create tag-range key and try to find tag-range match
            key2 = evioDictEntry((uint16_t)tag, num, (uint16_t)tagEnd);

        case TAG_RANGE:
            // If a range was specified in the args, check to see if
            // there's an exact match first ...
            pos = tagRangeMap.find(key2);
            if (pos != tagRangeMap.end()) {
                name = pos->second;
                //cout << "getName: found key in tagRangeMap, val = " << name << endl;
                break;
            }
                // If a tag/num pair or only a tag was specified in the args,
                // see if either falls in a range of tags.
            else if (entryType != TAG_RANGE) {
                // Additional check to see if tag fits in a range
                bool gotMatch=false;

                map<evioDictEntry, string>::const_iterator pos;
                for (pos = tagRangeMap.begin(); pos != tagRangeMap.end(); pos++) {
                    evioDictEntry entry = pos->first;
                    if (entry.inRange(tag)) {
                        name = pos->second;
                        gotMatch = true;
                        //cout << "getName: found key in range in tagRangeMap, val = " << name << endl;
                        break;
                    }
                }
                if (gotMatch) break;
            }

        default:
            ostringstream ss;

            if (haveParent) {
                ss << "?evioDictionary::getName...no dictionary entry for tag = " << tag << ", num = " << num <<
                ", tagEnd = " << tagEnd <<  ", PARENT: tag = " << parentTag << ", num = " << parentNum <<
                ", tagEnd = " << parentTagEnd << ends;
            }
            else {
                ss << "?evioDictionary::getName...no dictionary entry for tag = " << tag << ", num = " << num <<
                ", tagEnd = " << tagEnd << ends;
            }

            throw(evioException(0,ss.str(),__FILE__,__FUNCTION__,__LINE__));
    }

    return name;
}


//-----------------------------------------------------------------------


/**
 * Sets separator character.
 * @param sep Separator character
 */
void evioDictionary::setSeparator(const string &sep) {
  separator=sep;
}


//-----------------------------------------------------------------------


/**
 * Gets separator character.
 * @return Separator character
 */
string evioDictionary::getSeparator(void) const {
  return(separator);
}


//-----------------------------------------------------------------------
//-----------------------------------------------------------------------


/**
 * Converts dictionary into string.
 * @return String containing dictionary keys and values
 */
string evioDictionary::toString(void) const throw(evioException) {

  stringstream ss;

  map<string, evioDictEntry>::const_iterator pos;
  ss << "key                                    " << "value     " << "Desc/Format" << endl;
  ss << "---                                    " << "-----     " << "-----------" <<endl;
  for (pos = getTagNumMap.begin(); pos != getTagNumMap.end(); pos++) {
      evioDictEntry entry = pos->second;
    ss << left << setw(35) << pos->first << "    " << (int)entry.getTag() << "," << (int)entry.getNum() <<
    "     " << entry.getDescription() << "     " << entry.getFormat() << endl;
  }

  return(ss.str());
}


//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

