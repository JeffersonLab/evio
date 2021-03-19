//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_STRUCTUREYPE_H
#define EVIO_6_0_STRUCTUREYPE_H


#include <string>


namespace evio {


    /**
     * Numerical values associated with evio structure types.
     * This class approximates the Java enum it was copied from.
     * This class converts structure type numerical values to a more meaningful name.
     * For example, the structure type with value 0xe corresponds to a BANK.
     * This is mostly used for printing.
     *
     * @author heddle
     * @author timmer
     *
     * @version 6.0
     * @since 6.0 4/13/2020
     */
    class StructureType {

    public:

        static const StructureType STRUCT_UNKNOWN32;
        static const StructureType STRUCT_TAGSEGMENT;
        static const StructureType STRUCT_SEGMENT;
        static const StructureType STRUCT_BANK;

    private:

        /** Value of this structure type. */
        uint32_t value;

        /** The bank and segment have 2 values associated with them. */
        uint32_t value2 = 0;

        /** Name of this structure type. */
        std::string name;

        /** Fast way to convert integer values into StructureType objects. */
        static StructureType intToType[33]; // 0x20 + 1 = 33

        /** Store a name for each StructureType object. */
        static std::string names[33];

        /**
         * Constructor.
         * @param name  name (string representation) of this StructureType object.
         * @param val int value of this StructureType object.
         * @param val2 possible second int value of this StructureType object.
         */
        StructureType(std::string name, uint32_t val, uint32_t val2=0) : name(std::move(name)), value(val), value2(val2) {}

    public:

        /**
         * Get the object from the integer value.
         * @param val the value to match.
         * @return the matching StructureType object.
         */
        static const StructureType & getStructureType(uint32_t val) {
            if (val > 0x20) return STRUCT_UNKNOWN32;
            return intToType[val];
        }

        /**
         * Get the name from the integer value.
         * @param val the value to match.
         * @return the name, or <code>null</code>.
         */
        static std::string getName(uint32_t val) {
            if (val > 0x20) return "UNKNOWN32";
            return getStructureType(val).names[val];
        }

        /**
         * Convenience method to see if the given integer arg represents a BANK.
         * @param type the int value to match.
         * @return <code>true</code> if the structure type corresponds to a BANK.
         */
        static bool isBank(uint32_t type) {return (STRUCT_BANK.value == type || STRUCT_BANK.value2 == type);}

        /**
         * Convenience method to see if the given integer arg represents a SEGMENT.
         * @param type the int value to match.
         * @return <code>true</code> if the structure type corresponds to a SEGMENT.
         */
        static bool isSegment(uint32_t type) {return (STRUCT_SEGMENT.value == type || STRUCT_SEGMENT.value2 == type);}

        /**
         * Convenience method to see if the given integer arg represents a TAGSEGMENT.
         * @param type the int value to match.
         * @return <code>true</code> if the structure type corresponds to a TAGSEGMENT.
         */
        static bool isTagSegment(uint32_t type) {return (STRUCT_TAGSEGMENT.value == type);}



        /**
         * Get the name associated with this structure type.
         * @return name associated with this structure type.
         */
        const std::string & getName() const {return name;}

        /**
         * Get the integer value associated with this structure type.
         * @return integer value associated with this structure type.
         */
        uint32_t getValue() const {return value;}

        /**
         * Return a string which is usually the same as the name of the
         * enumerated value, except in the cases of ALSOSEGMENT and
         * ALSOBANK which return SEGMENT and BANK respectively.
         *
         * @return name of the enumerated type
         */
        std::string toString() {return name;}

        /**
         * Convenience routine to see if "this" structure type is a bank structure.
         * @return <code>true</code> if this structure type corresponds to a bank structure.
         */
        bool isBank() {return (*this == STRUCT_BANK);}

        /**
         * Convenience routine to see if "this" structure type is a segment structure.
         * @return <code>true</code> if this structure type corresponds to a segment structure.
         */
        bool isSegment() {return (*this == STRUCT_SEGMENT);}

        /**
         * Convenience routine to see if "this" structure type is a tagsegment structure.
         * @return <code>true</code> if this structure type corresponds to a tagsegment structure.
         */
        bool isTagSegment() {return (*this == STRUCT_TAGSEGMENT);}


        bool operator==(const StructureType &rhs) const;

        bool operator!=(const StructureType &rhs) const;
    };

}


#endif //EVIO_6_0_STRUCTUREYPE_H
