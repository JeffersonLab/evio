//
// Created by timmer on 1/2/20.
//

#ifndef EVIO_6_0_UTIL_H
#define EVIO_6_0_UTIL_H

#include "HipoException.h"
#include "ByteOrder.h"
#include <iostream>
#include <iomanip>


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

    /**
     * This method takes a byte buffer and prints out the desired number of bytes
     * from the given position. Prints all bytes.
     *
     * @param buf       buffer to print out
     * @param position  position of data (bytes) in buffer to start printing
     * @param bytes     number of bytes to print in hex
     * @param label     a label to print as header
     */
    static void printBytes(ByteBuffer & buf, uint32_t position, uint32_t bytes, string label) {

        cout << "printBytes: cap = " << buf.capacity() << endl;
        int origPos = buf.position();
        int origLim = buf.limit();
        // set pos = 0, lim = cap
        buf.clear();
        buf.position(position);
        cout << "pos = " << position << ", lim = " << buf.limit() << ", cap = " << buf.capacity() << ", bytes = " << bytes << endl;

        bytes = bytes + position > buf.capacity() ? (buf.capacity() - position) : bytes;
cout << "printing out " << bytes << " number of bytes" << endl;
        if (label.size() > 0) cout << label << ":" << endl;

        for (int i=0; i < bytes; i++) {
            if (i%20 == 0) {
                cout << endl << "  Buf(" << (i + 1) << "-" << (i + 20) << ") =  ";
            }
            else if (i%4 == 0) {
                cout << "  ";
            }

            cout << hex << setfill('0') << setw(2) << (int)(buf.get(i)) << " ";
        }
        cout << dec << endl << endl << setfill(' ');

        buf.limit(origLim).position(origPos);
    }

};


#endif //EVIO_6_0_UTIL_H
