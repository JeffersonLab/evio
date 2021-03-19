//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_HEADERTYPE_H
#define EVIO_6_0_HEADERTYPE_H


#include <string>


namespace evio {


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
    class HeaderType {

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

        /** Fast way to convert integer values into HeaderType objects. */
        static HeaderType intToType[16];

        /** Store a name for each HeaderType object. */
        static std::string names[16];

        /** Value of this header type. */
        uint32_t value;

        /** Store a name for each ByteOrder object. */
        std::string name;


    private:

        /**
         * Constructor.
         * @param val int value of this HeaderType object.
         * @param name name of this HeaderType object.
         */
        HeaderType(uint32_t val, std::string name) : value(val), name(std::move(name)) {}

    public:

        /**
        * Get the object name.
        * @return the object name.
        */
        std::string getName() const {return name;}

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
         * Is this a trailer?
         * @return <code>true</code> if is a trailer, else <code>false</code>
         */
        bool isTrailer() const {return (*this == EVIO_TRAILER || *this == HIPO_TRAILER);}

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
        static std::string getName(uint32_t val) {
            if (val > 7) return "UNKNOWN";
            return getHeaderType(val).names[val];
        }

        bool operator==(const HeaderType &rhs) const;

        bool operator!=(const HeaderType &rhs) const;
    };

}


#endif //EVIO_6_0_HEADERTYPE_H
