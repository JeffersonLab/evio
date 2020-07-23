//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#include "HeaderType.h"


namespace evio {


    bool HeaderType::operator==(const HeaderType &rhs) const {
        return value == rhs.value;
    }

    bool HeaderType::operator!=(const HeaderType &rhs) const {
        return value != rhs.value;
    }


    // "Enum" value DEFINITIONS

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


    // Initialize static members
    std::string HeaderType::names[16] =
            {"EVIO_RECORD",
             "EVIO_FILE",
             "EVIO_FILE_EXTENDED",
             "EVIO_TRAILER",
             "HIPO_RECORD",
             "HIPO_FILE",
             "HIPO_FILE_EXTENDED",
             "HIPO_TRAILER",
                    // Line of unused elements, just fill with unknown
             "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN", "UNKNOWN",
             "UNKNOWN"};

    HeaderType HeaderType::intToType[16] =
            {HeaderType::EVIO_RECORD,
             HeaderType::EVIO_FILE,
             HeaderType::EVIO_FILE_EXTENDED,
             HeaderType::EVIO_TRAILER,
             HeaderType::HIPO_RECORD,
             HeaderType::HIPO_FILE,
             HeaderType::HIPO_FILE_EXTENDED,
             HeaderType::HIPO_TRAILER,
                    // 2 Lines of unused elements, just fill with unknown
             HeaderType::UNKNOWN, HeaderType::UNKNOWN, HeaderType::UNKNOWN, HeaderType::UNKNOWN,
             HeaderType::UNKNOWN, HeaderType::UNKNOWN, HeaderType::UNKNOWN,
             HeaderType::UNKNOWN};

}
