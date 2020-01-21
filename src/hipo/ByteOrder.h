/**
 * Copyright (c) 2019, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 09/20/2019
 * @author timmer
 */


#ifndef EVIO_6_0_BYTEORDER_H
#define EVIO_6_0_BYTEORDER_H

#include <string>
#include <iostream>

using std::string;


namespace evio {


/* Macros for swapping constant values in the preprocessing stage.
 * Note, there's no sign extension for unsigned types. */
#define SWAP_16(x) \
    ((uint16_t)((((uint16_t)(x)) >> 8) | \
                (((uint16_t)(x)) << 8)))

#define SWAP_32(x) \
    ((uint32_t)((((uint32_t)(x)) >> 24) | \
                (((uint32_t)(x) & 0x00ff0000) >>  8) | \
                (((uint32_t)(x) & 0x0000ff00) <<  8) | \
                (((uint32_t)(x)) << 24)))

#define SWAP_64(x) \
    ((uint64_t)((((uint64_t)(x)) >> 56) | \
                (((uint64_t)(x) & 0x00ff000000000000ULL) >> 40) | \
                (((uint64_t)(x) & 0x0000ff0000000000ULL) >> 24) | \
                (((uint64_t)(x) & 0x000000ff00000000ULL) >>  8) | \
                (((uint64_t)(x) & 0x00000000ff000000ULL) <<  8) | \
                (((uint64_t)(x) & 0x0000000000ff0000ULL) << 24) | \
                (((uint64_t)(x) & 0x000000000000ff00ULL) << 40) | \
                (((uint64_t)(x)) << 56)))





///* Read little-endian 16 bit unsigned integer */
//#define READ_16LE(a) ( ((((unsigned char *) (a))[1]) <<  8) | \
//                       ((((unsigned char *) (a))[0])) )
//
///* Read big-endian 16 bit unsigned integer */
//#define READ_16BE(a) ( ((((unsigned char *) (a))[0]) <<  8) | \
//                       ((((unsigned char *) (a)[1])) )
//
///* Read little-endian 32 bit unsigned integer */
//#define READ_32LE(a) ( ((((unsigned char *) a)[3]) << 24) | \
//                       ((((unsigned char *) a)[2]) << 16) | \
//                       ((((unsigned char *) a)[1]) <<  8) | \
//                       ((((unsigned char *) a)[0])) )
///* Read big-endian 32 bit unsigned integer */
//#define READ_32BE(a) ( ((((unsigned char *) a)[0]) << 24) | \
//                       ((((unsigned char *) a)[1]) << 16) | \
//                       ((((unsigned char *) a)[2]) <<  8) | \
//                       ((((unsigned char *) a)[3])) )

/**
 * Numerical values associated with endian byte order.
 *
 * @version 6.0
 * @since 6.0 4/16/2019
 * @author timmer
 */
class ByteOrder {

public:

    static const ByteOrder ENDIAN_LITTLE;
    static const ByteOrder ENDIAN_BIG;

    static const ByteOrder ENDIAN_UNKNOWN;
    static const ByteOrder ENDIAN_LOCAL;

private:

    /** Value of this endian type. */
    int value;

    /** Store a name for each ByteOrder object. */
    string name;

    /**
     * Constructor.
     * @param value int value of this headerType object.
     * @param name  name (string representation) of this headerType object.
     */
    ByteOrder(int val, string name) : value(val), name(std::move(name)) {}

public:

    /**
     * Get the object name.
     * @return the object name.
     */
    string getName() const {return name;}


    bool isBigEndian()    const {return (value == ENDIAN_BIG.value);}

    bool isLittleEndian() const {return (value == ENDIAN_LITTLE.value);}

    bool isLocalEndian()  const {return (value == ENDIAN_LOCAL.value);}


    bool operator==(const ByteOrder &rhs) const;

    bool operator!=(const ByteOrder &rhs) const;

    static ByteOrder const & getLocalByteOrder() {
        if (isLocalHostBigEndian()) {
            return ENDIAN_BIG;
        }
        return ENDIAN_LITTLE;
    }

    static bool isLocalHostBigEndian() {
        int i = 1;
        return !*((char *) &i);
    }

    static bool needToSwap(ByteOrder & order) {
        return !(order == getLocalByteOrder());
    }

    // Template to swap stuff in place
    template <typename T>
    static void byteSwapInPlace(T& var);

    // Methods to swap and return floats and doubles
    static float byteSwap(float var);

    static double byteSwap(double var);

    static void byteSwap(uint32_t *array, size_t elements);
};
}


#endif //EVIO_6_0_BYTEORDER_H
