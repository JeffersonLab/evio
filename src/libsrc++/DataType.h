//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_DATATYPE_H
#define EVIO_6_0_DATATYPE_H


#include <string>


namespace evio {


    /**
     * Numerical values associated with evio data types.
     * This class approximates the Java enum it was copied from.
     * ALSOTAGSEGMENT (0x40) value was removed from this class because
     * the upper 2 bits of a byte containing the datatype are now used
     * to store padding data.
     *
     * @version 6.0
     * @since 6.0 7/22/2019
     * @author timmer
     */
    class DataType {

    public:

        static const DataType UNKNOWN32;
        static const DataType UINT32;
        static const DataType FLOAT32;
        static const DataType CHARSTAR8;
        static const DataType SHORT16;
        static const DataType USHORT16;
        static const DataType CHAR8;
        static const DataType UCHAR8;
        static const DataType DOUBLE64;
        static const DataType LONG64;
        static const DataType ULONG64;
        static const DataType INT32;
        static const DataType TAGSEGMENT;
        static const DataType ALSOSEGMENT;
        static const DataType ALSOBANK;
        static const DataType COMPOSITE;
        static const DataType BANK;
        static const DataType SEGMENT;

        // These types are only used when dealing with COMPOSITE data.
        // They are never transported independently and are stored in integers.
        static const DataType HOLLERIT;
        static const DataType NVALUE;
        static const DataType nVALUE;
        static const DataType mVALUE;

        // In C++ this seems to be useful on occasion
        static const DataType NOT_A_VALID_TYPE;


    private:

        /** Value of this data type. */
        uint32_t value;

        /** Name of this data type. */
        std::string name;

        /** Number of bytes this data type consumes (if relevant, else = -1). */
        int bytes;

    private:

        /** Fast way to convert integer values into DataType objects. */
        static DataType intToType[37];  // min size -> 37 = 0x24 + 1

        /** Store a name for each DataType object. */
        static std::string names[37];

        /**
         * Constructor.
         * @param val   int value of this DataType object.
         * @param name  name (string representation) of this DataType object.
         * @param byteLen number of bytes this type takes (if relevant).
         */
        DataType(uint32_t val, std::string name, int byteLen = -1) : value(val), name(std::move(name)), bytes(byteLen) {}

    public:

        /**
         * Get the object from the integer value.
         * @param val the value to match.
         * @return the matching DataType object.
         */
        static const DataType & getDataType(uint32_t val) {
            if (val > 0x24 || (val > 0x10 && val < 0x20)) return UNKNOWN32;
            return intToType[val];
        }

        /**
         * Get the name from the integer value.
         * @param val the value to match.
         * @return the name, or <code>null</code>.
         */
        static std::string getName(uint32_t val) {
            if (val > 0x24 || (val > 0x10 && val < 0x20)) return "UNKNOWN32";
            return getDataType(val).names[val];
        }

        /**
         * Get the enum constant from a string.
         * @param typeName the name of the DataType to obtain.
         * @return the DataType object associated with the given type name,
         *         or DataType::UNKNOWN32 if there's no match.
         */
        static DataType valueOf(std::string const & typeName) {
            int index = 0;
            for (std::string const & name : names) {
                if (name == typeName) {
                    return intToType[index];
                }
                index++;
            }
            return DataType::UNKNOWN32;
        }

        /**
         * Convenience method to see if the given integer arg represents a data type which
         * is a structure (a container).
         * @param dataType the int value to match.
         * @return <code>true</code> if the data type corresponds to one of the structure
         * types: BANK, SEGMENT, TAGSEGMENT, ALSOBANK, or ALSOSEGMENT.
         */
        static bool isStructure(uint32_t dataType) {
            return  dataType == BANK.value    || dataType == ALSOBANK.value    ||
                    dataType == SEGMENT.value || dataType == ALSOSEGMENT.value ||
                    dataType == TAGSEGMENT.value;
        }

        /**
         * Convenience method to see if the given integer arg represents a BANK.
         * @param dataType the int value to match.
         * @return <code>true</code> if the data type corresponds to a BANK.
         */
        static bool isBank(uint32_t dataType) {
            return (BANK.value == dataType || ALSOBANK.value == dataType);
        }

        /**
         * Convenience method to see if the given integer arg represents a SEGMENT.
         * @param dataType the int value to match.
         * @return <code>true</code> if the data type corresponds to a SEGMENT.
         */
        static bool isSegment(uint32_t dataType) {
            return (SEGMENT.value == dataType || ALSOSEGMENT.value == dataType);
        }

        /**
         * Convenience method to see if the given integer arg represents a TAGSEGMENT.
         * @param dataType the int value to match.
         * @return <code>true</code> if the data type corresponds to a TAGSEGMENT.
         */
        static bool isTagSegment(uint32_t dataType) {
            return (TAGSEGMENT.value == dataType);
        }



        /**
         * Get the name associated with this data type.
         * @return name associated with this data type.
         */
        const std::string & getName() const {return name;}

        /**
         * Get the integer value associated with this data type.
         * @return integer value associated with this data type.
         */
        uint32_t getValue() const {return value;}

        /**
         * Return a string which is usually the same as the name of the
         * enumerated value, except in the cases of ALSOSEGMENT and
         * ALSOBANK which return SEGMENT and BANK respectively.
         *
         * @return name of the enumerated type
         */
        std::string toString() const {
            if      (*this == ALSOBANK)    return "BANK";
            else if (*this == ALSOSEGMENT) return "SEGMENT";
            return name;
        }


        /**
         * Convenience routine to see if "this" data type is a structure (a container.)
         * @return <code>true</code> if the data type corresponds to one of the structure
         * types: BANK, SEGMENT, TAGSEGMENT, ALSOBANK, or ALSOSEGMENT.
         */
        bool isStructure() const {
            return ((*this == BANK)       ||
                    (*this == SEGMENT)    ||
                    (*this == TAGSEGMENT) ||
                    (*this == ALSOBANK)   ||
                    (*this == ALSOSEGMENT));
        }

        /**
         * Convenience routine to see if "this" data type is a bank structure.
         * @return <code>true</code> if this data type corresponds to a bank structure.
         */
        bool isBank() const {return (*this == BANK || *this == ALSOBANK);}

        /**
         * Convenience routine to see if "this" data type is a segment structure.
         * @return <code>true</code> if this data type corresponds to a bank structure.
         */
        bool isSegment() const {return (*this == SEGMENT || *this == ALSOSEGMENT);}

        /**
         * Convenience routine to see if "this" data type is a tagsegment structure.
         * @return <code>true</code> if this data type corresponds to a tagsegment structure.
         */
        bool isTagSegment() const {return (*this == TAGSEGMENT);}

        /**
         * Convenience method to see if "this" data type is an integer of some kind -
         * either 8, 16, 32, or 64 bits worth.
         * @return <code>true</code> if the data type corresponds to an integer type
         */
        bool isInteger() const {
            return ((*this == UCHAR8)   ||
                    (*this == CHAR8)    ||
                    (*this == USHORT16) ||
                    (*this == SHORT16)  ||
                    (*this == UINT32)   ||
                    (*this == INT32)    ||
                    (*this == ULONG64)  ||
                    (*this == LONG64));
        }

        /**
         *  Return the number of bytes this data type takes (if relevant).
         * @return the number of bytes this data type takes (if relevant).
         */
        int getBytes() const {return bytes;}

        bool operator==(const DataType &rhs) const;

        bool operator!=(const DataType &rhs) const;
    };

}


#endif //EVIO_6_0_DATATYPE_H
