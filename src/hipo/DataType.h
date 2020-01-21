/**
 * Copyright (c) 2019, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 04/10/2019
 * @author timmer
 */

#ifndef EVIO_6_0_DATATYPE_H
#define EVIO_6_0_DATATYPE_H

#include <string>

using std::string;


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
class DataType final {

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


private:

    /** Value of this data type. */
    uint32_t value;

    /** Name of this data type. */
    string name;

    /** Fast way to convert integer values into DataType objects. */
    static DataType intToType[37];  // min size -> 37 = 0x24 + 1

    /** Store a name for each DataType object. */
    static string names[37];

private:

    /**
     * Constructor.
     * @param value int value of this DataType object.
     * @param name  name (string representation) of this DataType object.
     */
    DataType(uint32_t val, string name) : value(val), name(std::move(name)) {}

public:

    /**
     * Get the object from the integer value.
     * @param val the value to match.
     * @return the matching enum, or <code>null</code>.
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
    static string getName(uint32_t val) {
        if (val > 0x24 || (val > 0x10 && val < 0x20)) return "UNKNOWN32";
        return getDataType(val).names[val];
    }

    /**
     * Convenience method to see if the given integer arg represents a data type which
     * is a structure (a container).
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
     * @return <code>true</code> if the data type corresponds to a BANK.
     */
    static bool isBank(uint32_t dataType) {
        return (BANK.value == dataType || ALSOBANK.value == dataType);
    }

    /**
     * Convenience method to see if the given integer arg represents a SEGMENT.
     * @return <code>true</code> if the data type corresponds to a SEGMENT.
     */
    static bool isSegment(uint32_t dataType) {
        return (SEGMENT.value == dataType || ALSOSEGMENT.value == dataType);
    }

    /**
     * Convenience method to see if the given integer arg represents a TAGSEGMENT.
     * @return <code>true</code> if the data type corresponds to a TAGSEGMENT.
     */
    static bool isTagSegment(uint32_t dataType) {
        return (TAGSEGMENT.value == dataType);
    }



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
    string toString() {
        if      (*this == ALSOBANK)    return "BANK";
        else if (*this == ALSOSEGMENT) return "SEGMENT";
        return name;
    }


    /**
     * Convenience routine to see if "this" data type is a structure (a container.)
     * @return <code>true</code> if the data type corresponds to one of the structure
     * types: BANK, SEGMENT, TAGSEGMENT, ALSOBANK, or ALSOSEGMENT.
     */
    bool isStructure() {
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
    bool isBank() {return (*this == BANK || *this == ALSOBANK);}

    /**
     * Convenience method to see if "this" data type is an integer of some kind -
     * either 8, 16, 32, or 64 bits worth.
     * @return <code>true</code> if the data type corresponds to an integer type
     */
    bool isInteger() {
        return ((*this == UCHAR8)   ||
                (*this == CHAR8)    ||
                (*this == USHORT16) ||
                (*this == SHORT16)  ||
                (*this == UINT32)   ||
                (*this == INT32)    ||
                (*this == ULONG64)  ||
                (*this == LONG64));
    }


    bool operator==(const DataType &rhs) const;

    bool operator!=(const DataType &rhs) const;
};

}


#endif //EVIO_6_0_DATATYPE_H
