// evioDictionary.hxx
//
// parses XML and creates a pair of name<-->evioDictEntry maps
//
// probably not thread-safe due to static expat handlers
//
//
// basic dictionary element:
//    <dictEntry name="" tag="" num="">
//
// This simply makes entries in the two maps. It can occur anywhere in the XML.
//
//
//
// hierarchical dictionary elements:
//    <bank name="" tag="" num="">
//      <bank name="" tag="" num="">
//         <leaf name="" tag="" num=""/>
//         <leaf name="" tag="" num=""/>
//         <leaf name="" tag="" num=""/>
//      </bank>
//    </bank>
//
// Here the name entered in the map reflects the position of the bank or leaf in
//   the full hierarchy.  The full name is a concatenation of the hierarchy of names
//   with a separator character between them (e.g. '.' or '/')
//
//
//
//  Author:  Elliott Wolin, JLab, 13-apr-2012
//           Carl Timmer, JLab, 27-oct-2016  extend dictionary capabilities



#ifndef _evioDictionary_hxx
#define _evioDictionary_hxx


//#include "evioDictEntry.hxx"
#include "evioTypedefs.hxx"
#include "evioException.hxx"

#include <stack>
#include <expat.h>



namespace evio {

using namespace std;
using namespace evio;


// xml tags holding straight dictionary entries
const string dictEntryTag = "dictentry";
const string oldDictEntryTag = "xmldumpdictdntry";

class evioDictEntry;

/**
 * This class parses XML dictionary string and contains maps for looking up dictionary information.
 */
class evioDictionary {

public:
    evioDictionary();
    evioDictionary(const string &dictXML, const string &sep=".");
    evioDictionary(ifstream &dictIFS, const string &sep=".");
    virtual ~evioDictionary();


public:
    bool parseDictionary(const string &dictionaryXML);
    evioDictEntry getEntry(const string &name) const throw(evioException);
    string getName(evioDictEntry &entry) const throw(evioException);
    string getName(uint16_t tag, uint8_t num, uint16_t tagEnd=0, bool haveParent=false,
                   uint16_t parentTag=0, uint8_t parentNum=0, uint16_t parentTagEnd=0) const throw(evioException);

    string getDictionaryXML(void) const;
    void setSeparator(const string &sep);
    string getSeparator(void) const;
    string toString(void) const throw(evioException);


public:
    // It's necessary to make these public so they can be friend methods to evioDictEntry,
    // which in turn allows them access to private methods that set format and description.
    static void startElementHandler(void *userData, const char *xmlname, const char **atts);
    static void endElementHandler(void *userData, const char *xmlname);
    static void charDataHandler(void *userData, const char *s, int len);


private:
    /** String containing the xml dictionary. */
    string dictionaryXML;
    /** Separator to use between elements of hierarchical names. Currently a period. */
    string separator;
    /** Temporary storage when creating hierarchical names of dictionary entries. */
    string parentPrefix;

    /** Flag, if true, indicates currently reading an XML element named "description". */
    bool readingDescription;
    /** Flag, if true, indicates currently reading an XML element named "leaf".
     *  Used to catch error condition in which leaf is parent of a container.*/
    bool parentIsLeaf;


public:
    // Unfortunately these maps were made public ...

    /**
     * This is the heart of the dictionary in which a key is composed of a tag/num
     * pair and other entry data and its corresponding value is a name.
     * This map contains all entries whether tag/num, tag-only, or tag-range.
     */
    map<evioDictEntry, string> getNameMap;     /**<Gets node name given tag/num.*/

    /**
     * This is a map in which the key is a name and the value is its
     * corresponding dictionary entry. It's the reverse of the getNameMap map.
     * This map contains all entries whether tag/num, tag-only, or tag-range.
     */
    map<string, evioDictEntry> getTagNumMap;   /**<Gets tag/num given node name.*/

private:

    /**
     * This stack is a place to store entry & name when going thru xml hierarchy.
     * Used to deal with description xml element.
     * @since 5.2
     */
    stack< pair<evioDictEntry, string> > entryStack;

    /**
     * This is a map in which the key is a name and the value is the entry
     * of a corresponding tag/num pair. It's the reverse of the tagNumMap map.
     * @since 5.2
     */
    map<string, evioDictEntry> tagNumReverseMap;   /**<Gets tag/num given node name.*/


    /**
     * This is a map in which the key is composed of a tag/num
     * pair and its corresponding value is a name. Contains only tag/num pair entries.
     * @since 5.2
     */
    map<evioDictEntry, string> tagNumMap;

    /**
     * Contains dictionary entries which have only a tag and no num.
     * It matches a tag/num pair if there is no exact match in tagNumMap,
     * but does match a tag in this map.
     * @since 5.2
     */
    map<evioDictEntry, string> tagOnlyMap;

    /**
     * Contains dictionary entries which have only a tag range and no num.
     * It matches a tag/num pair if there is no exact match in tagNumMap
     * or in the tagOnlyMap but the tag is within the specified range of an entry.
     * @since 5.2
     */
    map<evioDictEntry, string> tagRangeMap;

};


}

#endif
  
