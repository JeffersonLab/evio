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

#include "ByteOrder.h"


namespace evio {
    

// Template to swap stuff in place
template <typename T>
void ByteOrder::byteSwapInPlace(T& var)
{
    char* varArray = reinterpret_cast<char*>(&var);
    for (size_t i = 0;  i < sizeof(T)/2;  i++)
        std::swap(varArray[sizeof(var) - 1 - i],varArray[i]);
}

// Methods to swap and return floats and doubles
float ByteOrder::byteSwap(float var)
{
    char* varArray = reinterpret_cast<char*>(&var);
    for (size_t i = 0;  i < 2;  i++)
        std::swap(varArray[3 - i],varArray[i]);
    return var;
}

double ByteOrder::byteSwap(double var)
{
    char* varArray = reinterpret_cast<char*>(&var);
    for (size_t i = 0;  i < 4;  i++)
        std::swap(varArray[7 - i],varArray[i]);
    return var;
}

void ByteOrder::byteSwap(uint32_t *array, size_t elements) {
    for (size_t i=0; i < elements; i++) {
        array[i] = SWAP_32(array[i]);
    }
}


// Enum value DEFINITIONS
/** Little endian byte order. */
const ByteOrder ByteOrder::ENDIAN_LITTLE(0, "ENDIAN_LITTLE");
/** Big endian byte order. */
const ByteOrder ByteOrder::ENDIAN_BIG(1, "ENDIAN_BIG");
/** Unknown endian byte order. */
const ByteOrder ByteOrder::ENDIAN_UNKNOWN(2, "ENDIAN_UNKNOWN");
/** Local host's byte order. */
const ByteOrder ByteOrder::ENDIAN_LOCAL = ByteOrder::getLocalByteOrder();


bool ByteOrder::operator==(const ByteOrder &rhs) const {
    return value == rhs.value;
}

bool ByteOrder::operator!=(const ByteOrder &rhs) const {
    return value != rhs.value;
}
}

