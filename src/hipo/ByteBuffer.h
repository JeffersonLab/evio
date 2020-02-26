/**
 * Copyright (c) 2018, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 04/18/2018
 * @author timmer
 */

#ifndef EVIO_6_0_BYTEBUFFER_H
#define EVIO_6_0_BYTEBUFFER_H


#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <memory>
#include <iostream>
#include <cstdio>

#include "ByteOrder.h"
#include "EvioException.h"


using namespace std;


namespace evio {


/**
 * This class is copied from one of the same name in the Java programming language.
 * It wraps an array or buffer of data and is extremely useful in reading and writing
 * data. It's particularly useful when converting Java code to C++.
 * There is only one function that is not implemented, the slice() method.
 * For more info, read the Java ByteBuffer documentation.
 */
class ByteBuffer {

private:

    /** This is the current position in data buffer. Making it mutable means
     * the value can be altered even if an object of this class is const. */
    mutable size_t pos = 0;

    /** Limit is the position just past the last valid data byte. */
    mutable size_t lim = 0;

    /** Mark is set to mark a position in the buffer. */
    mutable size_t mrk = 0;

    /** Capacity is the total size of this buffer in bytes. */
    size_t cap = 0;

    /** This buffer is implemented with an array. Has shared pointer access
     * in order to implement the duplicate() method. Note that a shared pointer
     * to vector is not used since in one constructor, the underlying array is
     * passed in as an arg. */
    shared_ptr<uint8_t> buf = nullptr;

    /** Byte order of data. In java, default is big endian. */
    ByteOrder byteOrder {ByteOrder::ENDIAN_LITTLE};

    /** Is the data the same endian as the host? Convenience variable. */
    bool isHostEndian = false;

    /** Is the data little endian? Convenience variable. */
    bool isLittleEndian = false;

public:

    ByteBuffer();
    explicit ByteBuffer(size_t size);
    ByteBuffer(const ByteBuffer & srcBuf);
    ByteBuffer(ByteBuffer && srcBuf) noexcept;
    ByteBuffer(char* byteArray, size_t len);
    ByteBuffer(uint8_t* byteArray, size_t len);
    ~ByteBuffer() = default;

    ByteBuffer & operator=(ByteBuffer&& other) noexcept;
    ByteBuffer & operator=(const ByteBuffer& other);
    uint8_t    & operator[] (size_t index);
    uint8_t      operator[] (size_t index) const;
    ByteBuffer & compact();
    ByteBuffer & zero();

    static std::shared_ptr<ByteBuffer> copyBuffer(const std::shared_ptr<const ByteBuffer> & srcBuf);
    void copy(const ByteBuffer & srcBuf);
    void copy(const std::shared_ptr<const ByteBuffer> & srcBuf);
    bool equals(const ByteBuffer & other);
    void expand(size_t newSize);

    bool isDirect()     const;
    bool hasArray()     const;
    bool hasRemaining() const;
    bool isReadOnly()   const;
    const ByteOrder & order() const;
    uint8_t * array()   const;
    shared_ptr<uint8_t> getData() const;

    size_t arrayOffset() const;
    size_t remaining()   const;
    size_t capacity()    const;
    size_t limit()       const;
    size_t position()    const;

    ByteBuffer & mark();
    ByteBuffer & clear();
    ByteBuffer & flip();
    ByteBuffer & reset();
    ByteBuffer & rewind();
    ByteBuffer & position(size_t p);
    ByteBuffer & limit(size_t l);

    ByteBuffer & order(ByteOrder const & order);
    ByteBuffer & duplicate(ByteBuffer &destBuf);
    std::shared_ptr<ByteBuffer> duplicate();
    // ByteBuffer & slice();

    // Read

    const ByteBuffer & getBytes(uint8_t * dst, size_t offset, size_t length) const;

    uint8_t  peek() const;
    uint8_t  getByte()  const;
    uint8_t  getByte(size_t index) const;

    wchar_t  getChar() const; // Relative
    wchar_t  getChar(size_t index) const; // Absolute

    int16_t  getShort() const;
    int16_t  getShort(size_t index) const;
    uint16_t getUShort() const;
    uint16_t getUShort(size_t index) const;

    int32_t  getInt() const;
    int32_t  getInt(size_t index) const;
    uint32_t getUInt() const;
    uint32_t getUInt(size_t index) const;

    int64_t  getLong() const;
    int64_t  getLong(size_t index) const;
    uint64_t getULong() const;
    uint64_t getULong(size_t index) const;

    float    getFloat() const;
    float    getFloat(size_t index) const;
    double   getDouble() const;
    double   getDouble(size_t index) const;

    // Write

    ByteBuffer & put(const ByteBuffer & src);
    ByteBuffer & put(const std::shared_ptr<ByteBuffer> & src);
    ByteBuffer & put(const uint8_t* src, size_t offset, size_t length);

    ByteBuffer & put(uint8_t val); // Relative write
    ByteBuffer & put(size_t index, uint8_t val); // Absolute write at index

    ByteBuffer & putChar(wchar_t val);
    ByteBuffer & putChar(size_t index, wchar_t val);

    ByteBuffer & putShort(uint16_t val);
    ByteBuffer & putShort(size_t index, uint16_t val);

    ByteBuffer & putInt(uint32_t val);
    ByteBuffer & putInt(size_t index, uint32_t val);

    ByteBuffer & putLong(uint64_t val);
    ByteBuffer & putLong(size_t index, uint64_t val);

    ByteBuffer & putFloat(float val);
    ByteBuffer & putFloat(size_t index, float val);

    ByteBuffer & putDouble(double val);
    ByteBuffer & putDouble(size_t index, double val);

    // Utility Method
    void printBytes(size_t offset, size_t bytes, string const & label);

private:

    /** Template for relative read methods. */
    template<typename T> T read() const {
        T data = read<T>(pos);
        pos += sizeof(T);
        return data;
    }

    /** Template for absolute read methods. */
    template<typename T> T read(size_t index) const {
        if (index + sizeof(T) <= lim) {
            return *((T *) &(buf.get())[index]);
        }
        // Read would exceed limit
        throw EvioException("buffer underflow");
    }

    /** Template for relative write methods. */
    template<typename T> void write(T & data) {
        size_t s = sizeof(data);

        if (lim < (pos + s)) {
            // Write would exceeded limit
            throw EvioException("buffer overflow");
        }
        memcpy((void *) (&(buf.get())[pos]), (void *) (&data), s);

        pos += s;
    }

    /** Template for absolute write methods. */
    template<typename T> void write(T & data, size_t index) {
        size_t s = sizeof(data);
        if ((index + s) > lim) {
            throw EvioException("buffer overflow");
        }

        memcpy((void *) (&(buf.get())[index]), (void *) (&data), s);
    }

    // Follwing methods are a little more efficient for 1 byte transfers.

    /**
     * Write 1 byte unsigned int at current position then increment position.
     * More efficient than the templated method of this class.
     * @param data reference to byte to write.
     */
    void write(uint8_t & data) {
        if (lim < (pos + 1)) {
            throw EvioException("buffer overflow");
        }
        buf.get()[pos++] = data;
    }

    /**
     * Write 1 byte char at current position then increment position.
     * More efficient than the templated method of this class.
     * @param data reference to char to write.
     */
    void write(char & data) {
        if (lim < (pos + 1)) {
            throw EvioException("buffer overflow");
        }
        buf.get()[pos++] = data;
    }

    void write(uint8_t & data, size_t index) {
        if ((index + 1) > lim) {
            throw EvioException("buffer overflow");
        }
        buf.get()[index] = data;
    }

    void write(char & data, size_t index) {
        if ((index + 1) > lim) {
            throw EvioException("buffer overflow");
        }
        buf.get()[index] = data;
    }


    uint8_t read() const {
        if (pos + 1 <= lim) {
            return buf.get()[pos];
        }
        throw EvioException("buffer underflow");
    }

    uint8_t read(size_t index) const {
        if (index + 1 <= lim) {
            return buf.get()[index];
        }
        throw EvioException("buffer underflow");
    }

};


} // namespace evio



#endif //EVIO_6_0_BYTEBUFFER_H
