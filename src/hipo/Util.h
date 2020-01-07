//
// Created by timmer on 1/2/20.
//

#ifndef EVIO_6_0_UTIL_H
#define EVIO_6_0_UTIL_H

#include "HipoException.h"
#include "ByteOrder.h"
#include <iostream>
//#include <sstream>
//#include <stdexcept>
//#include <string>


/**
 * Class to write Evio-6.0/HIPO files.
 *
 * @version 6.0
 * @since 6.0 8/10/17
 * @author timmer
 */
class Util {

public:

    /**
     * Turn int into byte array.
     *
     * @param data        int to convert.
     * @param byteOrder   byte order of returned bytes.
     * @param dest        array in which to store returned bytes.
     * @param off         offset into dest array where returned bytes are placed.
     * @param destMaxSize max size in bytes of dest array.
     * @throws HipoException if dest is null or too small.
     */
    static void toBytes(uint32_t data, const ByteOrder & byteOrder,
                           uint8_t* dest, uint32_t off, uint32_t destMaxSize) {

        if (dest == nullptr || destMaxSize < 4+off) {
            throw HipoException("bad arg(s)");
        }

        if (byteOrder == ByteOrder::ENDIAN_BIG) {
            dest[off  ] = (uint8_t)(data >> 24);
            dest[off+1] = (uint8_t)(data >> 16);
            dest[off+2] = (uint8_t)(data >>  8);
            dest[off+3] = (uint8_t)(data      );
        }
        else {
            dest[off  ] = (uint8_t)(data      );
            dest[off+1] = (uint8_t)(data >>  8);
            dest[off+2] = (uint8_t)(data >> 16);
            dest[off+3] = (uint8_t)(data >> 24);
        }
    }


};


#endif //EVIO_6_0_UTIL_H
