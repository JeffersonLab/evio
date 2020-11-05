//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_EVIODICTIONARYENTRY_H
#define EVIO_6_0_EVIODICTIONARYENTRY_H


#include <sstream>
#include <memory>


#include "DataType.h"


namespace evio {


    /**
     * Class to facilitate use of Evio XML dictionary entry data as a key or value in a hash table.
     * (8/17/15).
     * @author timmer.
     */
    class EvioDictionaryEntry {

        friend class EvioXMLDictionary;
        
    public:

        /**
         * Type of dictionary entry. Is it just a tag? a tag and tag range but no num? or
         * a tag & num with a possible tag range?
         */
        enum EvioDictionaryEntryType {
            /** Valid tag & num, with or without a tagEnd. */
            TAG_NUM = 0,
            /** Valid tag, but no num or tagEnd. */
            TAG_ONLY = 1,
            /** Valid tag and tagEnd, but no num. */
            TAG_RANGE = 2
        };

    private:
        
        /** Tag value or low end of a tag range of an evio container. */
        uint16_t tag= 0;

        /** If &gt; 0 && != tag, this is the high end of a tag range. Never null, always &gt;= 0.
         *  @since 5.2 */
        uint16_t tagEnd = 0;

        /** Num value of evio container which may be null if not given in xml entry. */
        uint8_t num = 0;

        /** Is the num value of evio container being used? */
        bool numValid = false;

        /** Type of data in evio container. */
        DataType type {DataType::UNKNOWN32};

        /** String used to identify format of data if CompositeData type. */
        std::string format = "";

        /** String used to describe data if CompositeData type. */
        std::string description = "";

        /** Does this entry specify a tag & num, only a tag, or a tag range? */
        EvioDictionaryEntryType entryType = TAG_NUM;

        /** Track parent so identical tag/num/tagEnd can be used in another entry
            if and only if the parent tag/num/tagEnd is different. For simplicity limit this
            to one parent and not the stack/tree. */
        std::shared_ptr<EvioDictionaryEntry> parentEntry = nullptr;




        /** Zero-arg constructor.  */
        EvioDictionaryEntry() = default;


        /**
         * Constructor containing actual implementation.
         * Caller assumes responsibility of supplying correct arg values.
         * If tag &gt; tagEnd, these values are switched so tag &lt; tagEnd.
         *
         * @param tag    tag value or low end of a tag range of an evio container.
         * @param num    num value of evio container.
         * @param tagEnd if &gt; 0, this is the high end of a tag range.
         * @param numValid true, if num value is to be used, else false.
         * @param type   type of data in evio container which may be:
         *      {@link DataType#UNKNOWN32},
         *      {@link DataType#INT32},
         *      {@link DataType#UINT32},
         *      {@link DataType#FLOAT32},
         *      {@link DataType#DOUBLE64},
         *      {@link DataType#CHARSTAR8},
         *      {@link DataType#CHAR8},
         *      {@link DataType#UCHAR8},
         *      {@link DataType#SHORT16},
         *      {@link DataType#USHORT16},
         *      {@link DataType#LONG64},
         *      {@link DataType#ULONG64},
         *      {@link DataType#TAGSEGMENT},
         *      {@link DataType#SEGMENT}.
         *      {@link DataType#ALSOSEGMENT},
         *      {@link DataType#BANK},
         *      {@link DataType#ALSOBANK}, or
         *      {@link DataType#COMPOSITE},
         * @param description   description of CompositeData
         * @param format        format of CompositeData
         * @param parent        parent dictionary entry object
         */
        EvioDictionaryEntry(uint16_t tag, uint8_t num, uint16_t tagEnd, bool numValid,
                            DataType const & type, std::string const & description, std::string const & format,
                            std::shared_ptr<EvioDictionaryEntry> parent) {

            bool isRange = true;

            if (tagEnd == tag || tagEnd == 0) {
                // If both values equal each other or tagEnd == 0, there is no range.
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

            this->num = num;
            this->numValid = numValid;
            this->format = format;
            this->description = description;
            this->type = type;

            if (!isRange) {
                if (numValid) {
                    entryType = EvioDictionaryEntryType::TAG_NUM;
                }
                else {
                    entryType = EvioDictionaryEntryType::TAG_ONLY;
                }
            }
            else {
                entryType = EvioDictionaryEntryType::TAG_RANGE;
            }

            parentEntry = parent;
        }


    public:


        /**
         * Constructor.
         * @param tag  tag value of evio container.
         * @param num  num value of evio container.
         * @param type type of data in evio container which may be (case-independent):
         *             {@link DataType#UNKNOWN32} ...
         *             {@link DataType#COMPOSITE}.
         */
        EvioDictionaryEntry(uint16_t tag, uint8_t num, DataType const & type) :
            EvioDictionaryEntry(tag, num, 0, true, type,
                    "", "", nullptr) {
        }


        /**
         * Constructor containing actual implementation.
         * Caller assumes responsibility of supplying correct arg values.
         * If tag &gt; tagEnd, these values are switched so tag &lt; tagEnd.
         * Num is ignored.
         *
         * @param tag    tag value or low end of a tag range of an evio container.
         * @param tagEnd if &gt; 0, this is the high end of a tag range.
         * @param type   type of data in evio container which may be:
         *               {@link DataType#UNKNOWN32} ...
         *               {@link DataType#COMPOSITE}.
         * @param description   description of CompositeData
         * @param format        format of CompositeData
         * @param parent        parent dictionary entry object
         */
        explicit EvioDictionaryEntry(uint16_t tag, uint16_t tagEnd = 0,
                DataType const & type = DataType::UNKNOWN32,
                std::string const & description = "", std::string const & format = "",
                std::shared_ptr<EvioDictionaryEntry> parent = nullptr) :

            EvioDictionaryEntry(tag, 0, tagEnd, false, type, description, format, parent) {
        }

        /**
         * Constructor containing actual implementation.
         * Caller assumes responsibility of supplying correct arg values.
         * If tag &gt; tagEnd, these values are switched so tag &lt; tagEnd.
         *
         * @param tag    tag value or low end of a tag range of an evio container.
         * @param num    num value of evio container.
         * @param tagEnd if &gt; 0, this is the high end of a tag range.
         * @param type   type of data in evio container which may be:
         *               {@link DataType#UNKNOWN32} ...
         *               {@link DataType#COMPOSITE}.
         * @param description   description of CompositeData
         * @param format        format of CompositeData
         * @param parent        parent dictionary entry object
         */
        explicit EvioDictionaryEntry(uint16_t tag, uint8_t num, uint16_t tagEnd = 0,
                                     DataType const & type = DataType::UNKNOWN32,
                                     std::string const & description = "", std::string const & format = "",
                                     std::shared_ptr<EvioDictionaryEntry> parent = nullptr) :

                EvioDictionaryEntry(tag, num, tagEnd, true, type, description, format, parent) {
        }


        /**
         * Is the given tag within the specified range (inclusive) of this dictionary entry?
         * @since 5.2
         * @param tagArg  tag to compare with range
         * @return {@code false} if tag not in range, else {@code true}.
         */
        bool inRange(uint16_t tagArg) const {
            return tagEnd != 0 && tagArg >= tag && tagArg <= tagEnd;
        }


        /**
         * Is the given dictionary entry's tag within the specified range
         * (inclusive) of this dictionary entry?
         * @since 5.2
         * @param entry  dictionary entry to compare with range
         * @return {@code false} if tag not in range, else {@code true}.
         */
        bool inRange(EvioDictionaryEntry & entry) const {
            return  tagEnd != 0 && entry.tag >= tag && entry.tag <= tagEnd;
        }


        bool operator==(const EvioDictionaryEntry &other) const {

            if (&other == this) return true;

            // Objects equal each other if tag & num & tagEnd are the same
            auto otherParent = other.getParentEntry();

            bool match = (tag == other.tag);
            match = match && (numValid == other.numValid);

            if (numValid) {
                match = match && (num == other.num);
            }

            // Now check tag range if any
            match = match && (tagEnd == other.tagEnd);

            // Now check if same entry type
            match = match && (entryType == other.entryType);

            // If both parent containers are defined, use them as well
            if (parentEntry != nullptr && otherParent != nullptr) {
                match = match && (parentEntry->getTag() == otherParent->getTag());
                match = match && (parentEntry->numValid == otherParent->numValid);
                if (parentEntry->numValid) {
                     match = match && (parentEntry->getNum() == otherParent->getNum());
                }
                match = match && (parentEntry->getTagEnd() == otherParent->getTagEnd());
                if (!match) std::cout << "  parents don't match" << std::endl;
            }

            return match;
        }

        bool operator!=(const EvioDictionaryEntry &rhs) const {
            return !(rhs == *this);
        }


        /**
         * Get a string representation of this object.
         * @return a string representation of this object.
         */
        std::string toString() const {
            std::stringstream ss;

            switch (entryType) {
                case TAG_NUM:
                    ss << "(tag=" << tag << ",num =" << +num << ")" ;
                    break;
                case TAG_ONLY:
                    ss << "(tag=" << tag << ")" ;
                    break;
                case TAG_RANGE:
                    ss << "(tag=" << tag << "-" << tagEnd << ")" ;
            }

            return ss.str();
        }


        /**
         * Get the tag value.
         * This is the low end of a tag range if tagEnd &gt; 0.
         * @return tag value.
         */
        uint16_t getTag() const {return tag;}

        /**
         * Get the tagEnd value (upper end of a tag range).
         * A value of 0 means there is no range.
         * @return tagEnd value.
         */
        uint16_t getTagEnd() const {return tagEnd;}

        /**
         * Get the num value.
         * @return num value.
         */
        uint8_t getNum() const {return num;}

        /**
         * Get the data's type.
         * @return data type object, null if nonexistent.
         */
         DataType getType() const {return type;}

        /**
         * Get the CompositeData's format.
         * @return CompositeData's format, empty if nonexistent.
         */
        std::string getFormat() const {return format;}

        /**
         * Get the CompositeData's description.
         * @return CompositeData's description, empty if nonexistent.
         */
         std::string getDescription() const {return description;}

        /**
         * Get this entry's type.
         * @return this entry's type.
         */
        EvioDictionaryEntryType getEntryType() const {return entryType;}

        /**
         * Get the parent container's dictionary entry.
         * @return the parent container's dictionary entry, null if nonexistent.
         */
        std::shared_ptr<EvioDictionaryEntry> getParentEntry() const {return parentEntry;}

        /**
         * Get the string representation of this object.
         * @return string representation of this object.
         */
        std::string toString() {

            std::stringstream ss;

            ss << std::boolalpha;

            ss << "tag = " << tag << ", tagEnd = " << tagEnd << ", num = " << +num << ", numValid = " << numValid <<
                  ", data type = " << type.toString();

            if (entryType == TAG_NUM)
                ss << ", entry type = TAG/NUM";
            else if (entryType == TAG_ONLY)
                ss << ", entry type = TAG_ONLY";
            else if (entryType == TAG_RANGE)
                ss << ", entry type = TAG_RANGE";

            ss << std::endl;

            if (!format.empty()) {
                ss << "    format = " << format << std::endl;
            }

            if (!description.empty()) {
                ss << "    description = " << description << std::endl;
            }

            if (parentEntry != nullptr) {
                ss << "    parent = " << parentEntry->toString() << std::endl;
            }

            return ss.str();
        }

    };


}

#endif //EVIO_6_0_EVIODICTIONARYENTRY_H
