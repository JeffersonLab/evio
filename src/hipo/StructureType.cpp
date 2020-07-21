//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "StructureType.h"


namespace evio {


    bool StructureType::operator==(const StructureType &rhs) const {
        return value == rhs.value;
    }

    bool StructureType::operator!=(const StructureType &rhs) const {
        return value != rhs.value;
    }


    // "Enum" value DEFINITIONS

    /** Unknown data type. */
    const StructureType StructureType::STRUCT_UNKNOWN32 = StructureType("UNKNOWN32", 0);
    /** Tag segment. */
    const StructureType StructureType::STRUCT_TAGSEGMENT = StructureType("TAGSEGMENT", 0xc);
    /** Bank. */
    const StructureType StructureType::STRUCT_BANK = StructureType("BANK", 0x10, 0xe);
    /** Segment. */
    const StructureType StructureType::STRUCT_SEGMENT = StructureType("SEGMENT", 0x20, 0xd);

    // Initialize static members
    std::string StructureType::names[33] = {
            "UNKNOWN32", "UNKNOWN32",

            // Unused, just fill with unknown
            "UNKNOWN32", "UNKNOWN32", "UNKNOWN32", "UNKNOWN32", "UNKNOWN32",
            "UNKNOWN32", "UNKNOWN32", "UNKNOWN32", "UNKNOWN32", "UNKNOWN32",

            "TAGSEGMENT",
            "SEGMENT",
            "BANK",
            "UNKNOWN32",
            "BANK",

            "UNKNOWN32", "UNKNOWN32", "UNKNOWN32", "UNKNOWN32", "UNKNOWN32",
            "UNKNOWN32", "UNKNOWN32", "UNKNOWN32", "UNKNOWN32", "UNKNOWN32",
            "UNKNOWN32", "UNKNOWN32", "UNKNOWN32", "UNKNOWN32", "UNKNOWN32",

            "SEGMENT"
    };

    StructureType StructureType::intToType[33] = {
            StructureType::STRUCT_UNKNOWN32, StructureType::STRUCT_UNKNOWN32, StructureType::STRUCT_UNKNOWN32,
            StructureType::STRUCT_UNKNOWN32, StructureType::STRUCT_UNKNOWN32, StructureType::STRUCT_UNKNOWN32,
            StructureType::STRUCT_UNKNOWN32, StructureType::STRUCT_UNKNOWN32, StructureType::STRUCT_UNKNOWN32,
            StructureType::STRUCT_UNKNOWN32, StructureType::STRUCT_UNKNOWN32, StructureType::STRUCT_UNKNOWN32,

            StructureType::STRUCT_TAGSEGMENT,
            StructureType::STRUCT_SEGMENT,
            StructureType::STRUCT_BANK,
            StructureType::STRUCT_UNKNOWN32,
            StructureType::STRUCT_BANK,

            StructureType::STRUCT_UNKNOWN32, StructureType::STRUCT_UNKNOWN32, StructureType::STRUCT_UNKNOWN32,
            StructureType::STRUCT_UNKNOWN32, StructureType::STRUCT_UNKNOWN32, StructureType::STRUCT_UNKNOWN32,
            StructureType::STRUCT_UNKNOWN32, StructureType::STRUCT_UNKNOWN32, StructureType::STRUCT_UNKNOWN32,
            StructureType::STRUCT_UNKNOWN32, StructureType::STRUCT_UNKNOWN32, StructureType::STRUCT_UNKNOWN32,
            StructureType::STRUCT_UNKNOWN32, StructureType::STRUCT_UNKNOWN32, StructureType::STRUCT_UNKNOWN32,

            StructureType::STRUCT_SEGMENT
    };

}
