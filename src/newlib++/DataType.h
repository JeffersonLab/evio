/*
 * Copyright (c) 2019, Jefferson Science Associates, all rights reserved.
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000 Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 */

//
// Created by timmer on 4/10/19.
//

#ifndef EVIO_6_0_DATATYPE_H
#define EVIO_6_0_DATATYPE_H

#include <string>

using std::string;

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
    static DataType intToType[0x24 + 1];

    /** Store a name for each DataType object. */
    static string names[];

private:

    /**
     * Constructor.
     * @param value int value of this DataType object.
     * @param name  name (string representation) of this DataType object.
     */
    DataType(uint32_t value, string name) {
        this->value = value;
        this->name  = name;
        intToType[value] = *this;
        names[value] = name;
    }

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
        return  (((dataType > 0xb && dataType < 0x11) && dataType != 0xf) || dataType == 0x20);
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

    const DataType & operator=(const DataType &rhs);

};

// Enum value DEFINITIONS
// The initialization occurs in the scope of the class,
// so the private DataType constructor can be used.

/** Unknown data type. */
const DataType DataType::UNKNOWN32 = DataType(0x0, "UNKNOWN32");
/** Unsigned 32 bit int. */
const DataType DataType::UINT32 = DataType(0x1, "UINT32");
/** 32 bit float. */
const DataType DataType::FLOAT32 = DataType(0x2, "FLOAT32");
/** ASCII characters. */
const DataType DataType::CHARSTAR8 = DataType(0x3, "CHARSTAR8");
/** 16 bit int. */
const DataType DataType::SHORT16 = DataType(0x4, "SHORT16");
/** Unsigned 16 bit int. */
const DataType DataType::USHORT16 = DataType(0x5, "USHORT16");
/** 8 bit int. */
const DataType DataType::CHAR8 = DataType(0x6, "CHAR8");
/** Unsigned 8 bit int. */
const DataType DataType::UCHAR8 = DataType(0x7, "UCHAR8");
/** 64 bit double.. */
const DataType DataType::DOUBLE64 = DataType(0x8, "DOUBLE64");
/** 64 bit int. */
const DataType DataType::LONG64 = DataType(0x9, "LONG64");
/** Unsigned 64 bit int. */
const DataType DataType::ULONG64 = DataType(0xa, "ULONG64");
/** 32 bit int. */
const DataType DataType::INT32 = DataType(0xb, "INT32");


/** Tag segment. */
const DataType DataType::TAGSEGMENT = DataType(0xc, "TAGSEGMENT");
/** Segment alternate value. */
const DataType DataType::ALSOSEGMENT = DataType(0xd, "ALSOSEGMENT");
/** Bank alternate value. */
const DataType DataType::ALSOBANK = DataType(0xe, "ALSOBANK");
/** Composite data type. */
const DataType DataType::COMPOSITE = DataType(0xf, "COMPOSITE");
/** Bank. */
const DataType DataType::BANK = DataType(0x10, "BANK");
/** Segment. */
const DataType DataType::SEGMENT = DataType(0x20, "SEGMENT");


/** In composite data, Hollerit type. */
const DataType DataType::HOLLERIT = DataType(0x21, "HOLLERIT");
/** In composite data, N value. */
const DataType DataType::NVALUE = DataType(0x22, "NVALUE");
/** In composite data, n value. */
const DataType DataType::nVALUE = DataType(0x23, "nVALUE");
/** In composite data, m value. */
const DataType DataType::mVALUE = DataType(0x24, "mVALUE");



bool DataType::operator==(const DataType &rhs) const {
    return value == rhs.value;
}

bool DataType::operator!=(const DataType &rhs) const {
    return value != rhs.value;
}

const DataType & DataType::operator=(const DataType &rhs) {
    return rhs;
}


#endif //EVIO_6_0_DATATYPE_H
