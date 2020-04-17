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
    

/**
 * Templated method to swap data in place.
 * @tparam T type of data.
 * @param var reference to data to be swapped.
 */
template <typename T>
void ByteOrder::byteSwapInPlace(T& var)
{
    char* varArray = reinterpret_cast<char*>(&var);
    for (size_t i = 0;  i < sizeof(T)/2;  i++)
        std::swap(varArray[sizeof(var) - 1 - i],varArray[i]);
}

/**
 * Convenience method to return swapped float.
 * @param var float to swap
 */
float ByteOrder::byteSwap(float var)
{
    char* varArray = reinterpret_cast<char*>(&var);
    for (size_t i = 0;  i < 2;  i++)
        std::swap(varArray[3 - i],varArray[i]);
    return var;
}

/**
 * Convenience method to return swapped double.
 * @param var double to swap
 */
double ByteOrder::byteSwap(double var)
{
    char* varArray = reinterpret_cast<char*>(&var);
    for (size_t i = 0;  i < 4;  i++)
        std::swap(varArray[7 - i],varArray[i]);
    return var;
}

/**
 * Templated method to swap array data in place.
 * @tparam T data type.
 * @param var reference to data to be swapped.
 * @param elements number of data elements to be swapped.
 */
template <typename T>
void ByteOrder::byteSwapInPlace(T& var, size_t elements)
{
    char *c = reinterpret_cast<char *>(&var);
    size_t varSize = sizeof(T);

    for (size_t j = 0;  j < elements;  j++) {
        for (size_t i = 0; i < varSize/2; i++) {
            std::swap(c[varSize - 1 - i], c[i]);
        }
        c += varSize;
    }
}


/**
 * Templated method to swap array data in place.
 * @tparam T data type.
 * @param var pointer to data to be swapped.
 * @param elements number of data elements to be swapped.
 */
    template <typename T>
    void ByteOrder::byteSwapInPlace(T* var, size_t elements)
    {
        char *c = reinterpret_cast<char *>(var);
        size_t varSize = sizeof(T);

        for (size_t j = 0;  j < elements;  j++) {
            for (size_t i = 0; i < varSize/2; i++) {
                std::swap(c[varSize - 1 - i], c[i]);
            }
            c += varSize;
        }
    }

    /**
     * This method swaps an array of 2-byte data.
     * @param src pointer to data source.
     * @param elements number of 2-byte elements to swap.
     * @param dst pointer to destination.
     */
    void ByteOrder::byteSwap16(void* src, size_t elements, void* dst)
    {
        auto *s = reinterpret_cast<uint16_t *>(src);
        auto *d = reinterpret_cast<uint16_t *>(dst);

        for (size_t j = 0;  j < elements;  j++) {
            *d = SWAP_16(*s);
            ++s, ++d;
        }
    }

    /**
     * This method swaps an array of 4-byte data.
     * @param src pointer to data source.
     * @param elements number of 4-byte elements to swap.
     * @param dst pointer to destination.
     */
    void ByteOrder::byteSwap32(void* src, size_t elements, void* dst)
    {
        auto *s = reinterpret_cast<uint32_t *>(src);
        auto *d = reinterpret_cast<uint32_t *>(dst);

        for (size_t j = 0;  j < elements;  j++) {
            *d = SWAP_32(*s);
            ++s, ++d;
        }
    }

    /**
     * This method swaps an array of 8-byte data.
     * @param src pointer to data source.
     * @param elements number of 8-byte elements to swap.
     * @param dst pointer to destination.
     */
    void ByteOrder::byteSwap64(void* src, size_t elements, void* dst)
    {
        auto *s = reinterpret_cast<uint64_t *>(src);
        auto *d = reinterpret_cast<uint64_t *>(dst);

        for (size_t j = 0;  j < elements;  j++) {
            *d = SWAP_64(*s);
            ++s, ++d;
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

