/**
 * @file
 *
 *
 *
 * 12-Oct-2016
 *
 *  @author  Carl Timmer
 */


#include <iostream>
#include "evioDictEntry.hxx"


using namespace std;
using namespace evio;



/** Zero arg constructor. */
evioDictEntry::evioDictEntry() {
    initEntry(0, 0, 0, EVIO_UNKNOWN32, false, "", "", false, 0, 0, 0);
}

/**
 * Constructor for tag-only entry.
 * @param tag tag
 */
evioDictEntry::evioDictEntry(uint16_t tag) {
    initEntry(tag, 0, 0, EVIO_UNKNOWN32, true, "", "", false, 0, 0, 0);
}

/**
 * Constructor for tag and num entry.
 *
 * @param tag   tag
 * @param num   num
 */
evioDictEntry::evioDictEntry(uint16_t tag, uint8_t num) {
    initEntry(tag, num, 0, EVIO_UNKNOWN32, false, "", "", false, 0, 0, 0);
}

/**
 * Constructor for tag-range entry. If tag > tagEnd, these values are switched.
 *
 * @param tag   beginning tag of range
 * @param num   num is used only to differentiate this from the tag-num constructor.
 * @param tagEnd ending tag of range
 */
evioDictEntry::evioDictEntry(uint16_t tag, uint8_t num, uint16_t tagEnd) {
    initEntry(tag, num, tagEnd, EVIO_UNKNOWN32, true, "", "", false, 0, 0, 0);
}

/** Copy constructor. */
evioDictEntry::evioDictEntry(const evioDictEntry & entry) {
    tag = entry.tag;
    num = entry.num;
    tagEnd = entry.tagEnd;
    gotParent = entry.gotParent;
    parentTag = entry.parentTag;
    parentNum = entry.parentNum;
    parentTagEnd = entry.parentTagEnd;
    type = entry.type;
    entryType = entry.entryType;
    format = entry.format;
    description = entry.description;
    numIsUndefined = entry.numIsUndefined;
}


/**
 * General constructor. If tag > tagEnd, these values are switched.
 *
 * @param tag             tag or the beginning tag of a range.
 * @param num             num.
 * @param tagEnd          ending tag of a range. Set to 0 or same value as tag if no range desired.
 * @param type            type of evio data contained in evio structure this entry represents.
 * @param numIsUndefined  if desiring a tag-range or tag-only entry, set this to {@code true}, else {@code false}.
 *                        Defaults to false.
 * @param format          string describing data format. Defaults to "".
 * @param description     string describing data. Defaults to "".
 */
evioDictEntry::evioDictEntry(uint16_t tag, uint8_t num, uint16_t tagEnd,
                             DataType type, bool numIsUndefined, string format, string description) {
    initEntry(tag, num, tagEnd, type, numIsUndefined, format, description, false, 0, 0, 0);
}

/**
 * General constructor. If tag > tagEnd, these values are switched.
 *
 * @param tag             tag or the beginning tag of a range.
 * @param num             num.
 * @param tagEnd          ending tag of a range. Set to 0 or same value as tag if no range desired.
 * @param type            type of evio data contained in evio structure this entry represents.
 * @param numIsUndefined  if desiring a tag-range or tag-only entry, set this to {@code true}, else {@code false}.
 *                        Defaults to false.
 * @param format          string describing data format. Defaults to "".
 * @param description     string describing data. Defaults to "".
 */
evioDictEntry::evioDictEntry(uint16_t tag, uint8_t num, uint16_t tagEnd,
                             bool hasParent, uint16_t parentTag, uint8_t parentNum, uint16_t parentTagEnd,
                             DataType type, bool numIsUndefined, string format, string description) {
    initEntry(tag, num, tagEnd, type, numIsUndefined, format, description, hasParent, parentTag, parentNum, parentTagEnd);
}


/**
 * Method to initialize objects of this type in constructors.
 * If tag > tagEnd, these values are switched.
 *
 * @param tag             tag or the beginning tag of a range.
 * @param num             num.
 * @param tagEnd          ending tag of a range. Set to 0 or same value as tag if no range desired.
 * @param type            type of evio data contained in evio structure this entry represents.
 * @param numIsUndefined  if desiring a tag-range or tag-only entry, set this to {@code true}, else {@code false}.
 * @param format          string describing data format.
 * @param description     string describing data.
 * @param hasParent       is this a hierarchical entry with a parent?
 * @param tag             parent entry's tag.
 * @param num             parent entry's num.
 * @param tagEnd          parent entry's ending tag of a range.
 */
void evioDictEntry::initEntry(uint16_t tag, uint8_t num, uint16_t tagEnd,
                              DataType type, bool numIsUndefined, string format, string description,
                              bool hasParent, uint16_t parentTag, uint8_t parentNum, uint16_t parentTagEnd) {

    this->num = num;
    this->type = type;
    this->format = format;
    this->description = description;
    this->numIsUndefined = numIsUndefined;

    this->gotParent = hasParent;
    this->parentNum = parentNum;
    this->parentTag = parentTag;
    this->parentTagEnd = parentTagEnd;

    bool isRange = true;

    if (tagEnd == tag || tagEnd == 0) {
        // If both values equal each other or tagEnd = 0, there is no range.
        this->tag    = tag;
        this->tagEnd = 0;
        isRange = false;
    }
    else if (tagEnd < tag) {
        // Switch things so tag < tagEnd for simplicity
        this->tag = tagEnd;
        this->tagEnd = tag;
    }
    else {
        this->tag    = tag;
        this->tagEnd = tagEnd;
    }

    if (!isRange) {
        if (numIsUndefined) {
            entryType = TAG_ONLY;
        }
        else {
            entryType = TAG_NUM;
        }
    }
    else {
        entryType = TAG_RANGE;
    }
}


/** Destructor. */
evioDictEntry::~evioDictEntry() {}

// Getters

/** Get the tag value of this entry. @return tag. */
uint16_t      evioDictEntry::getTag(void)         const {return tag;}
/** Get the ending value of a tag range defined by this entry. @since 5.2 @return tagEnd. */
uint16_t      evioDictEntry::getTagEnd(void)      const {return tagEnd;}
/** Get the num value of this entry. @return num. */
uint8_t       evioDictEntry::getNum(void)         const {return num;}
/** Get the string describing the data format of this entry. @since 5.2 @return format. */
string        evioDictEntry::getFormat(void)      const {return format;}
/** Get the string describing the data of this entry. @since 5.2 @return description. */
string        evioDictEntry::getDescription(void) const {return description;}
/** Get whether num is undefined.  @since 5.2 @return true if tag-only or tag-range, else false. */
bool          evioDictEntry::isNumUndefined(void) const {return numIsUndefined;}
/** Get the data type of this entry. @since 5.2 @return DataType enum describing type of data. */
DataType      evioDictEntry::getType(void)        const {return type;}
/** Get the type of this entry. @since 5.2 @return DictEntryType enum specify a tag & num, only a tag, or a tag range. */
DictEntryType evioDictEntry::getEntryType(void)   const {return entryType;}

/** Get the tag of parent entry if any. @return parent tag. */
uint16_t evioDictEntry::getParentTag(void) const {return parentTag;}
/** Get the tagEnd of parent entry if any. @return parent tagEnd. */
uint16_t evioDictEntry::getParentTagEnd(void) const {return parentTagEnd;}
/** Get the num of parent entry if any. @return parent num. */
uint8_t  evioDictEntry::getParentNum(void) const {return parentNum;}
/** Does this entry have a valid parent entry? @return true if valid parent entry, else false. */
bool     evioDictEntry::hasParent(void) const {return gotParent;}

// Setters

/** Set the string describing the data format of this entry.@since 5.2  @param f reference to format.*/
void evioDictEntry::setFormat(const string & f) {format = string(f);}
/** Set the string describing the data format of this entry. @since 5.2 @param f format. */
void evioDictEntry::setFormat(const char *f)    {format = string(f);}

/** Set the string describing the data of this entry. @since 5.2 @param d reference to description. */
void evioDictEntry::setDescription(const string &d) {description = string(d);}
/** Set the string describing the data of this entry. @since 5.2 @param d description. */
void evioDictEntry::setDescription(const char *d)   {description = string(d);}



/** Create a string representation of this object.
 *  @return string representation of this object. */
string evioDictEntry::toString(void) const throw(evioException) {
    string eType[] = {"TAG_NUM", "TAG_ONLY", "TAG_RANGE"};

    ostringstream ss;
    ss << "tag = " << tag << ", num = " << num << ", tagEnd = " << tagEnd << ", entryType = " << eType[entryType]  << ends;
    return ss.str();
}

/**
 * Is the given tag within the specified range (inclusive) of this dictionary entry?
 * @since 5.2
 * @param tagArg  tag to compare with range
 * @return {@code false} if tag not in range or this entry does not define a range, else {@code true}.
 */
bool evioDictEntry::inRange(uint16_t tagArg) {
    return (entryType == TAG_RANGE) && tagArg >= tag && tagArg <= tagEnd;
}



//-----------------------------------------------------------------------


/**
 * Given a string data type, return the equivalent DataType enum.
 *
 * @parm type data type string of "unknown32", "uint32", ... "bank", "segment".
 * @return DataType enum equivalent of arg.
 */
DataType evioDictEntry::getDataType(const char *type) {
    for (int i=0; i < 18; i++) {
        if (strcasecmp(type, DataTypeNames[i]) == 0) {
            return DataTypes[i];
        }
    }
    return EVIO_UNKNOWN32;
}




