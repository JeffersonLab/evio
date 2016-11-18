/**
 * @file
 *
 *
 *
 * 12-Oct-2016
 *
 *  @author  Carl Timmer
 */



#ifndef _evioDictEntry_hxx
#define _evioDictEntry_hxx

#include <stdint.h>
#include <string>
#include <iostream>
#include "evioException.hxx"
#include "evioDictionary.hxx"

namespace evio {

    using namespace std;


    /** An entry in the dictionary can be either a tag/num pair, a tag only, or a range of tags. */
    enum DictEntryType {TAG_NUM, TAG_ONLY, TAG_RANGE};

    /**
     * Data types supported by evio. The prefix of EVIO_ is added to differentiate
     * between these values and those of enum ContainerType.
     */
    enum DataType {
        EVIO_UNKNOWN32    =  (0x0),
        EVIO_UINT32       =  (0x1),
        EVIO_FLOAT32      =  (0x2),
        EVIO_CHARSTAR8    =  (0x3),
        EVIO_SHORT16      =  (0x4),
        EVIO_USHORT16     =  (0x5),
        EVIO_CHAR8        =  (0x6),
        EVIO_UCHAR8       =  (0x7),
        EVIO_DOUBLE64     =  (0x8),
        EVIO_LONG64       =  (0x9),
        EVIO_ULONG64      =  (0xa),
        EVIO_INT32        =  (0xb),
        EVIO_TAGSEGMENT   =  (0xc),
        EVIO_ALSOSEGMENT  =  (0xd),
        EVIO_ALSOBANK     =  (0xe),
        EVIO_COMPOSITE    =  (0xf),
        EVIO_BANK         =  (0x10),
        EVIO_SEGMENT      =  (0x20) };

    /** Put DataType enums into an array. */
    static const DataType DataTypes[18] = { EVIO_UNKNOWN32,  EVIO_UINT32,      EVIO_FLOAT32,  EVIO_CHARSTAR8,
                                            EVIO_SHORT16,    EVIO_USHORT16,    EVIO_CHAR8,    EVIO_UCHAR8,
                                            EVIO_DOUBLE64,   EVIO_LONG64,      EVIO_ULONG64,  EVIO_INT32,
                                            EVIO_TAGSEGMENT, EVIO_ALSOSEGMENT, EVIO_ALSOBANK,
                                            EVIO_COMPOSITE,  EVIO_BANK,        EVIO_SEGMENT };

    /** Put the string associated with each DataType enums into an array. */
    static const char * DataTypeNames[18] = { "unknown32",  "uint32",      "float32",  "charstar8",
                                              "short16",    "ushort16",    "char8",    "uchar8",
                                              "double64",   "long64",      "ulong64",  "int32",
                                              "tagsegment", "alsosegment", "alsobank",
                                              "composite",  "bank",        "segment" };

    /** This class defines an entry in the XML dictionary. */
    class evioDictEntry {

    public:
        // These methods need to call setFormat and setDescription which should be private
        friend  void evioDictionary::startElementHandler(void *userData, const char *xmlname, const char **atts);
        friend  void evioDictionary::charDataHandler(void *userData, const char *s, int len);

        evioDictEntry();
        evioDictEntry(uint16_t tag);
        evioDictEntry(uint16_t tag, uint8_t num, uint16_t tagEnd);
        evioDictEntry(uint16_t tag, uint8_t num);
        evioDictEntry(uint16_t tag, uint8_t num, uint16_t tagEnd, DataType type,
                      bool numIsUndefined=false, string format="", string description="");
        evioDictEntry(uint16_t tag, uint8_t num, uint16_t tagEnd,
                      bool hasParent, uint16_t parentTag, uint8_t parentNum, uint16_t parentTagEnd, DataType type,
                      bool numIsUndefined=false, string format="", string description="");

        evioDictEntry(const evioDictEntry & entry);

        virtual ~evioDictEntry();

    private:
        void initEntry(uint16_t tag, uint8_t num, uint16_t tagEnd,
                       DataType type, bool numIsUndefined, string format, string description,
                       bool hasParent, uint16_t parentTag, uint8_t parentNum, uint16_t parentTagEnd);

    public:
        uint16_t getTag(void) const;
        uint16_t getTagEnd(void) const;
        uint8_t getNum(void) const;
        bool isNumUndefined(void) const;
        DataType getType(void) const;
        DictEntryType getEntryType(void) const;
        string getFormat(void) const;
        string getDescription(void) const;

        // Parent entry methods

        uint16_t getParentTag(void) const;
        uint16_t getParentTagEnd(void) const;
        uint8_t  getParentNum(void) const;
        bool     hasParent(void) const;

        // General

        static DataType getDataType(const char *type);
        string toString(void) const throw(evioException);
        bool inRange(uint16_t tagArg);

    private:
        void setFormat(const string &format);
        void setFormat(const char *format);

        void setDescription(const string &description);
        void setDescription(const char *description);

    private:
        /** Tag value or low end of a tag range of an evio container. */
        uint16_t tag;

        /** If > 0 && != tag, this is the high end of a tag range.
         *  @since 5.2 */
        uint16_t tagEnd;

        /** Num value of evio container which is 0 if not given in xml entry. */
        uint8_t num;

        /** Track whether num is even defined for this entry.
        *   @since 5.2 */
        bool numIsUndefined;

        /** Type of data in evio container.
         *   @since 5.2 */
        DataType type;

        /** String used to identify format (currently only used for composite data type).
         *   @since 5.2 */
        string format;

        /** String used to describe data (currently only used for composite data type).
         *   @since 5.2 */
        string description;

        /** Does this entry specify a tag & num, only a tag, or a tag range?
         *   @since 5.2 */
        DictEntryType entryType;

        // HIERARCHICAL ENTRY'S PARENT INFO

        /** For hierarchical entries (bank & leaf), does its parent exist?
         *   @since 5.2 */
        bool gotParent;

        /** Parent's tag value or low end of a tag range of an evio container. */
        uint16_t parentTag;

        /** If > 0 && != tag, this is the high end of a parent's' tag range.
         *  @since 5.2 */
        uint16_t parentTagEnd;

        /** Num value of parent evio container which is 0 if not given in xml entry.
         *  @since 5.2 */
        uint8_t parentNum;


    public:
        // Redefine == for this class
        inline bool operator == (const evioDictEntry & tn) const
        {
            bool result =  (tag == tn.tag) &&
                           (num == tn.num) &&
                           (tagEnd == tn.tagEnd) &&
                           (entryType == tn.entryType);

            // If both parent containers are defined, use them as well
            if (gotParent && tn.gotParent) {
                result = result &&
                        (parentTag == tn.parentTag) &&
                        (parentNum == tn.parentNum) &&
                        (parentTagEnd == tn.parentTagEnd);
            }

            return result;
        }


        // Map is ordered and does sorting so, if !(a < b) && !(b < a) then (a == b)
        inline bool operator < (const evioDictEntry & tn) const
        {
            bool result = tag < tn.tag;
            if (tag != tn.tag) return result;

            result = num < tn.num;
            if (num != tn.num) return result;

            result = tagEnd < tn.tagEnd;
            if (tagEnd != tn.tagEnd) return result;

            result = entryType < tn.entryType;
            if (entryType != tn.entryType) return result;

            // If both parent containers are defined, use them as well
            if (gotParent && tn.gotParent) {
                result = parentTag < tn.parentTag;
                if (parentTag != tn.parentTag) return result;

                result = parentNum < tn.parentNum;
                if (parentNum != tn.parentNum) return result;

                result = parentTagEnd < tn.parentTagEnd;
                if (parentTagEnd != tn.parentTagEnd) return result;
            }

            return result;
        }

    };
    

};

#endif
  
