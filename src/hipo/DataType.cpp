//
// Created by timmer on 9/23/19.
//

#include "DataType.h"


bool DataType::operator==(const DataType &rhs) const {
    return value == rhs.value;
}

bool DataType::operator!=(const DataType &rhs) const {
    return value != rhs.value;
}


// "Enum" value DEFINITIONS

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


// Initialize static members
std::string DataType::names[37] =
        {"UNKNOWN32",
         "UINT32",
         "FLOAT32",
         "CHARSTAR8",
         "SHORT16",
         "USHORT16",
         "CHAR8",
         "UCHAR8",
         "DOUBLE64",
         "LONG64",
         "ULONG64",
         "INT32",
         "TAGSEGMENT",
         "ALSOSEGMENT",
         "ALSOBANK",
         "COMPOSITE",
         "BANK",

         // Unused, just fill with unknown
         "UNKNOWN32", "UNKNOWN32", "UNKNOWN32", "UNKNOWN32", "UNKNOWN32",
         "UNKNOWN32", "UNKNOWN32", "UNKNOWN32", "UNKNOWN32", "UNKNOWN32",
         "UNKNOWN32", "UNKNOWN32", "UNKNOWN32", "UNKNOWN32", "UNKNOWN32",

         "SEGMENT",
         "HOLLERIT",
         "NVALUE",
         "nVALUE",
         "mVALUE"
         };

DataType DataType::intToType[37] =
        {DataType::UNKNOWN32,
         DataType::UINT32,
         DataType::FLOAT32,
         DataType::CHARSTAR8,
         DataType::SHORT16,
         DataType::USHORT16,
         DataType::CHAR8,
         DataType::UCHAR8,
         DataType::DOUBLE64,
         DataType::LONG64,
         DataType::ULONG64,
         DataType::INT32,
         DataType::TAGSEGMENT,
         DataType::ALSOSEGMENT,
         DataType::ALSOBANK,
         DataType::COMPOSITE,
         DataType::BANK,

         DataType::UNKNOWN32, DataType::UNKNOWN32, DataType::UNKNOWN32, DataType::UNKNOWN32, DataType::UNKNOWN32,
         DataType::UNKNOWN32, DataType::UNKNOWN32, DataType::UNKNOWN32, DataType::UNKNOWN32, DataType::UNKNOWN32,
         DataType::UNKNOWN32, DataType::UNKNOWN32, DataType::UNKNOWN32, DataType::UNKNOWN32, DataType::UNKNOWN32,

         DataType::SEGMENT,
         DataType::HOLLERIT,
         DataType::NVALUE,
         DataType::nVALUE,
         DataType::mVALUE};
