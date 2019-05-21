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

#ifndef EVIO_6_0_HEADERTYPE_H
#define EVIO_6_0_HEADERTYPE_H

#include <string>

using std::string;

/**
 * Numerical values associated with types of a file or record header.
 * The value associated with each member is stored in the
 * header's bit-info word in the top 4 bits. Thus the lowest value is 0
 * and the highest (UNKNOWN) is 15.
 * This class approximates the Java enum it was copied from.
 *
 * @version 6.0
 * @since 6.0 4/10/2019
 * @author timmer
 */
class HeaderType final {

public:

    static const HeaderType EVIO_RECORD;
    static const HeaderType EVIO_FILE;
    static const HeaderType EVIO_FILE_EXTENDED;
    static const HeaderType EVIO_TRAILER;
    static const HeaderType HIPO_RECORD;
    static const HeaderType HIPO_FILE;
    static const HeaderType HIPO_FILE_EXTENDED;
    static const HeaderType HIPO_TRAILER;
    static const HeaderType UNKNOWN;

private:

    /** Value of this header type. */
    uint32_t value;

    /** Fast way to convert integer values into HeaderType objects. */
    static HeaderType intToType[16];

    /** Store a name for each HeaderType object. */
    static string names[];

private:

    /**
     * Constructor.
     * @param value int value of this headerType object.
     * @param name  name (string representation) of this headerType object.
     */
    HeaderType(uint32_t value, string name) {
        this->value = value;
        intToType[value] = *this;
        names[value] = name;
    }

public:

    /**
     * Get the integer value associated with this header type.
     * @return integer value associated with this header type.
     */
    uint32_t getValue() const {return value;}

    /**
      * Is this an evio file header?
      * @return <code>true</code> if is an evio file header, else <code>false</code>
      */
    bool isEvioFileHeader() const {return (*this == EVIO_FILE || *this == EVIO_FILE_EXTENDED);}

    /**
     * Is this a HIPO file header?
     * @return <code>true</code> if is an HIPO file header, else <code>false</code>
     */
    bool isHipoFileHeader() const {return (*this == HIPO_FILE || *this == HIPO_FILE_EXTENDED);}

    /**
     * Is this a file header?
     * @return <code>true</code> if is a file header, else <code>false</code>
     */
    bool isFileHeader() const {return (isEvioFileHeader() | isHipoFileHeader());}

    /**
     * Get the object from the integer value.
     * @param val the value to match.
     * @return the matching enum, or <code>null</code>.
     */
    static const HeaderType & getHeaderType(uint32_t val) {
        if (val > 7) return UNKNOWN;
        return intToType[val];
    }

    /**
     * Get the name from the integer value.
     * @param val the value to match.
     * @return the name, or <code>null</code>.
     */
    static string getName(uint32_t val) {
        if (val > 7) return "UNKNOWN";
        return getHeaderType(val).names[val];
    }

    bool operator==(const HeaderType &rhs) const;

    bool operator!=(const HeaderType &rhs) const;

    const HeaderType & operator=(const HeaderType &rhs);

};

// Enum value DEFINITIONS
// The initialization occurs in the scope of the class,
// so the private HeaderType constructor can be used.

/** Header for a general evio record. */
const HeaderType HeaderType::EVIO_RECORD = HeaderType(0, "EVIO_RECORD");
/** Header for an evio file. */
const HeaderType HeaderType::EVIO_FILE = HeaderType(1, "EVIO_FILE");
/** Header for an extended evio file. Currently not used. */
const HeaderType HeaderType::EVIO_FILE_EXTENDED = HeaderType(2, "EVIO_FILE_EXTENDED");
/** Header for an evio trailer record. */
const HeaderType HeaderType::EVIO_TRAILER = HeaderType(3, "EVIO_TRAILER");

/** Header for a general hipo record. */
const HeaderType HeaderType::HIPO_RECORD = HeaderType(4, "HIPO_RECORD");
/** Header for an hipo file. */
const HeaderType HeaderType::HIPO_FILE = HeaderType(5, "HIPO_FILE");
/** Header for an extended hipo file. Currently not used. */
const HeaderType HeaderType::HIPO_FILE_EXTENDED = HeaderType(6, "HIPO_FILE_EXTENDED");
/** Header for an hipo trailer record. */
const HeaderType HeaderType::HIPO_TRAILER = HeaderType(7, "HIPO_TRAILER");

/** Unknown header. */
const HeaderType HeaderType::UNKNOWN = HeaderType(15, "UNKNOWN");

bool HeaderType::operator==(const HeaderType &rhs) const {
    return value == rhs.value;
}

bool HeaderType::operator!=(const HeaderType &rhs) const {
    return value != rhs.value;
}

const HeaderType & HeaderType::operator=(const HeaderType &rhs) {
    return rhs;
}

#endif //EVIO_6_0_HEADERTYPE_H
