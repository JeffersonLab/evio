//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_UTIL_H
#define EVIO_6_0_UTIL_H

#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <regex>
#include <vector>
#include <fstream>

#include "EvioException.h"
#include "ByteOrder.h"
#include "ByteBuffer.h"
#include "EvioNode.h"
#include "IBlockHeader.h"
#include "DataType.h"


namespace evio {


    /**
     * Class containing static methods of general purpose.
     *
     * @version 6.0
     * @since 6.0 11/2/19
     * @author timmer
     */
    class Util {

    public:

        // Some methods to help deal with padding data to 4-byte boundaries

        /**
         * Returns length padded to 4-byte boundary for given length in bytes.
         * @param length length in bytes.
         * @return length in bytes padded to 4-byte boundary.
         */
        static uint32_t getWords(uint32_t length) {
            uint32_t words = length/4;
            if (getPadding(length) > 0) words++;
            return words;
        }


        /**
         * Returns number of bytes needed to pad to 4-byte boundary for the given length.
         * @param length length in bytes.
         * @return number of bytes needed to pad to 4-byte boundary.
         */
        static uint32_t getPadding(uint32_t length) {
            /** Array to help find number of bytes to pad data. */
            static uint32_t padValue[4] = {0,3,2,1};
            return padValue[length%4];
        }


        //-----------------------------------------------------------------------


        /**
         * Case insensitive compare for 2 strings.
         * @param a first string.
         * @param b second string.
         * @return true if equal, else false.
         */
        static bool iStrEquals(const std::string& a, const std::string& b) {
            unsigned int sz = a.size();
            if (b.size() != sz)
                return false;
            for (unsigned int i = 0; i < sz; ++i)
                if (tolower(a[i]) != tolower(b[i]))
                    return false;
            return true;
        }


        /**
         * This method returns an XML element name given an evio data type.
         * @param type evio data type
         * @return XML element name used in evio event xml output
         */
        static const DataType & getDataType(const std::string & type) {

            if (iStrEquals(type,"int8"))       return DataType::CHAR8;
            else if (iStrEquals(type,"uint8"))      return DataType::UCHAR8;
            else if (iStrEquals(type,"int16"))      return DataType::SHORT16;
            else if (iStrEquals(type,"uint16"))     return DataType::USHORT16;
            else if (iStrEquals(type,"int32"))      return DataType::INT32;
            else if (iStrEquals(type,"uint32"))     return DataType::UINT32;
            else if (iStrEquals(type,"int64"))      return DataType::LONG64;
            else if (iStrEquals(type,"uint64"))     return DataType::ULONG64;
            else if (iStrEquals(type,"long64"))     return DataType::LONG64;
            else if (iStrEquals(type,"ulong64"))    return DataType::ULONG64;
            else if (iStrEquals(type,"float32"))    return DataType::FLOAT32;
            else if (iStrEquals(type,"float64"))    return DataType::DOUBLE64;
            else if (iStrEquals(type,"double64"))   return DataType::DOUBLE64;
            else if (iStrEquals(type,"string"))     return DataType::CHARSTAR8;
            else if (iStrEquals(type,"composite"))  return DataType::COMPOSITE;
            else if (iStrEquals(type,"unknown32"))  return DataType::UNKNOWN32;
            else if (iStrEquals(type,"tagsegment")) return DataType::TAGSEGMENT;
            else if (iStrEquals(type,"segment"))    return DataType::ALSOSEGMENT;
            else if (iStrEquals(type,"bank"))       return DataType::ALSOBANK;

            return DataType::NOT_A_VALID_TYPE;
        }


        /**
         * Turn byte array into an int array.
         * Number of int array elements = number of bytes / 4.
         *
         * @param data       char array to convert.
         * @param dataLen    number of bytes to convert.
         * @param byteOrder  byte order of supplied bytes.
         * @param dest       array in which to write converted bytes.
         *
         * @throws EvioException if data or dest is null
         */
        static void toIntArray(char const *data,  uint32_t dataLen,
                               const ByteOrder & byteOrder, uint32_t *dest) {

            if (data == nullptr || dest == nullptr) {
                throw EvioException("bad arg");
            }

            for (int i = 0; i < dataLen-3; i+=4) {
                dest[i/4] = toInt(data[i ], data[i+1], data[i+2], data[i+3], byteOrder);
            }
        }


        /**
         * Turn 4 bytes into an unsigned 32 bit int.
         *
         * @param b1 1st byte
         * @param b2 2nd byte
         * @param b3 3rd byte
         * @param b4 4th byte
         * @param byteOrder if big endian, 1st byte is most significant & 4th is least
         * @return int converted from byte array
         */
        static uint32_t toInt(char b1, char b2, char b3, char b4, const ByteOrder & byteOrder) {

            if (byteOrder == ByteOrder::ENDIAN_BIG) {
                return (
                        (0xff & b1) << 24 |
                        (0xff & b2) << 16 |
                        (0xff & b3) <<  8 |
                        (0xff & b4)
                );
            }
            else {
                return (
                        (0xff & b1)       |
                        (0xff & b2) <<  8 |
                        (0xff & b3) << 16 |
                        (0xff & b4) << 24
                );
            }
        }


        /**
         * Turn 4 bytes into an unsigned 32 bit int.
         *
         * @param data pointer to bytes to convert.
         * @param byteOrder byte order of bytes.
         * @return int converted from byte array.
         * @throws EvioException if data is null.
         */
        static uint32_t toInt(uint8_t const * data, ByteOrder const & byteOrder) {
            if (data == nullptr) {
                throw EvioException("null arg");
            }

            if (byteOrder == ByteOrder::ENDIAN_BIG) {
                return (
                        (0xff & data[0]) << 24 |
                        (0xff & data[1]) << 16 |
                        (0xff & data[2]) <<  8 |
                        (0xff & data[3])
                );
            }
            else {
                return (
                        (0xff & data[0])       |
                        (0xff & data[1]) <<  8 |
                        (0xff & data[2]) << 16 |
                        (0xff & data[3]) << 24
                );
            }
        }


        /**
         * Turn 4 bytes into an unsigned 32 bit int.
         *
         * @param data pointer to bytes to convert
         * @param byteOrder byte order of bytes.
         * @return int converted from byte array.
         * @throws EvioException if data is null.
         */
        static uint64_t toLong(uint8_t const * data, ByteOrder const & byteOrder) {
            if (data == nullptr) {
                throw EvioException("null arg");
            }

            if (byteOrder == ByteOrder::ENDIAN_BIG) {
                return (
                        (uint64_t)(0xff & data[0]) << 56 |
                        (uint64_t)(0xff & data[1]) << 48 |
                        (uint64_t)(0xff & data[2]) << 40 |
                        (uint64_t)(0xff & data[3]) << 32 |
                        (uint64_t)(0xff & data[4]) << 24 |
                        (uint64_t)(0xff & data[5]) << 16 |
                        (uint64_t)(0xff & data[6]) <<  8 |
                        (uint64_t)(0xff & data[7])
                );
            }
            else {
                return (
                        (uint64_t)(0xff & data[0])       |
                        (uint64_t)(0xff & data[1]) <<  8 |
                        (uint64_t)(0xff & data[2]) << 16 |
                        (uint64_t)(0xff & data[3]) << 24 |
                        (uint64_t)(0xff & data[4]) << 32 |
                        (uint64_t)(0xff & data[5]) << 40 |
                        (uint64_t)(0xff & data[6]) << 48 |
                        (uint64_t)(0xff & data[7]) << 56
                );
            }
        }


        /**
         * Write int into byte array.
         *
         * @param data        int to convert.
         * @param byteOrder   byte order of written bytes.
         * @param dest        array in which to write int.
         * @throws EvioException if dest is null or too small.
         */
        static void toBytes(uint32_t data, const ByteOrder & byteOrder, uint8_t* dest) {

            if (dest == nullptr) {
                throw EvioException("bad arg(s)");
            }

            if (byteOrder == ByteOrder::ENDIAN_BIG) {
                dest[0] = (uint8_t)(data >> 24);
                dest[1] = (uint8_t)(data >> 16);
                dest[2] = (uint8_t)(data >>  8);
                dest[3] = (uint8_t)(data      );
            }
            else {
                dest[0] = (uint8_t)(data      );
                dest[1] = (uint8_t)(data >>  8);
                dest[2] = (uint8_t)(data >> 16);
                dest[3] = (uint8_t)(data >> 24);
            }
        }


        /**
         * Turn long into byte array.
         *
         * @param data       long to convert.
         * @param byteOrder  byte order of written bytes.
         * @param dest       array in which to write long.
         * @throws EvioException if dest is null or too small
         */
        static void toBytes(uint64_t data, const ByteOrder & byteOrder, uint8_t* dest) {

            if (dest == nullptr) {
                throw EvioException("bad arg(s)");
            }

            if (byteOrder == ByteOrder::ENDIAN_BIG) {
                dest[0] = (uint8_t)(data >> 56);
                dest[1] = (uint8_t)(data >> 48);
                dest[2] = (uint8_t)(data >> 40);
                dest[3] = (uint8_t)(data >> 32);
                dest[4] = (uint8_t)(data >> 24);
                dest[5] = (uint8_t)(data >> 16);
                dest[6] = (uint8_t)(data >>  8);
                dest[7] = (uint8_t)(data      );
            }
            else {
                dest[0] = (uint8_t)(data      );
                dest[1] = (uint8_t)(data >>  8);
                dest[2] = (uint8_t)(data >> 16);
                dest[3] = (uint8_t)(data >> 24);
                dest[4] = (uint8_t)(data >> 32);
                dest[5] = (uint8_t)(data >> 40);
                dest[6] = (uint8_t)(data >> 48);
                dest[7] = (uint8_t)(data >> 56);
            }
        }


        /**
         * Write int into byte vector.
         *
         * @param data        int to convert.
         * @param byteOrder   byte order of vector.
         * @param dest        vector in which to write int.
         * @param off         offset into vector where int is to be written.
         */
        static void toBytes(uint32_t data, const ByteOrder & byteOrder,
                            std::vector<uint8_t> & dest, size_t off) {

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
         * Write short into byte array.
         *
         * @param data        short to convert.
         * @param byteOrder   byte order of array.
         * @param dest        array in which to write short.
         * @throws EvioException if dest is null or too small.
         */
        static void toBytes(uint16_t data, const ByteOrder & byteOrder, uint8_t* dest) {

            if (dest == nullptr) {
                throw EvioException("bad arg(s)");
            }

            if (byteOrder == ByteOrder::ENDIAN_BIG) {
                dest[0] = (uint8_t)(data >>  8);
                dest[1] = (uint8_t)(data      );
            }
            else {
                dest[0] = (uint8_t)(data      );
                dest[1] = (uint8_t)(data >>  8);
            }
        }


        /**
         * Write short into byte vector.
         *
         * @param data        short to convert.
         * @param byteOrder   byte order of vector.
         * @param dest        vector in which to write short.
         * @param off         offset into vector where short is to be written.
         */
        static void toBytes(uint16_t data, const ByteOrder & byteOrder,
                            std::vector<uint8_t> & dest, size_t off) {

            if (byteOrder == ByteOrder::ENDIAN_BIG) {
                dest[off  ] = (uint8_t)(data >>  8);
                dest[off+1] = (uint8_t)(data      );
            }
            else {
                dest[off  ] = (uint8_t)(data      );
                dest[off+1] = (uint8_t)(data >>  8);
            }
        }


        /**
         * Get a string used to indicate that no name can be determined.
         * @return string used to indicate that no name can be determined.
         */
        static const std::string& NO_NAME_STRING() {
            // Initialize the static variable
            static std::string s("???");
            return s;
        }


        /**
         * Reads a couple things in a block/record header
         * in order to determine the evio version and endianness of a buffer/file.
         * The endianness can be read from the given ByteBuffer by calling,
         * bb.order(). This does <b>not</b> change any parameters of the given
         * buffer.
         *
         * @param bb ByteBuffer to read from.
         * @param initialPos position in bb to start reading.
         * @return evio version.
         * @throws underflow_error if not enough data in buffer.
         * @throws EvioException bad magic number in header.
         */
        static uint32_t findEvioVersion(ByteBuffer & bb, size_t initialPos) {
            // Look at first record header

            // Have enough remaining bytes to read 8 words of header?
            if (bb.limit() - initialPos < 32) {
                throw std::underflow_error("not enough data to read in header");
            }

            // Set the byte order to match the file's ordering.

            // Check the magic number for endianness (buffer defaults to big endian)
            ByteOrder byteOrder = bb.order();

            // Offset to magic # is in the SAME LOCATION FOR ALL EVIO VERSIONS
            uint32_t magicNumber = bb.getUInt(initialPos + IBlockHeader::MAGIC_OFFSET);
            if (magicNumber != IBlockHeader::MAGIC_NUMBER) {
                if (byteOrder == ByteOrder::ENDIAN_BIG) {
                    byteOrder = ByteOrder::ENDIAN_LITTLE;
                }
                else {
                    byteOrder = ByteOrder::ENDIAN_BIG;
                }
                bb.order(byteOrder);

                // Reread magic number to make sure things are OK
                magicNumber = bb.getInt(initialPos + IBlockHeader::MAGIC_OFFSET);
                if (magicNumber != IBlockHeader::MAGIC_NUMBER) {
                    throw EvioException("magic number is bad, " + std::to_string(magicNumber));
                }
            }

            // Find the version number, again, SAME LOCATION FOR ALL EVIO VERSIONS
            uint32_t bitInfo = bb.getUInt(initialPos + IBlockHeader::BIT_INFO_OFFSET);
            return bitInfo & IBlockHeader::VERSION_MASK;
        }


        /**
         * This method takes a byte buffer and prints out the desired number of bytes
         * from the given position, in hex. Prints all bytes.
         *
         * @param buf       buffer to print out
         * @param position  position of data (bytes) in buffer to start printing
         * @param bytes     number of bytes to print in hex
         * @param label     a label to print as header
         */
        static void printBytes(const std::shared_ptr<ByteBuffer> buf, uint32_t position, uint32_t bytes,
                               const std::string & label) {
            printBytes(*(buf.get()), position, bytes, label);
        }

        /**
         * This method takes a byte buffer and prints out the desired number of bytes
         * from the given position, in hex. Prints all bytes.
         *
         * @param buf       buffer to print out
         * @param position  position of data (bytes) in buffer to start printing
         * @param bytes     number of bytes to print in hex
         * @param label     a label to print as header
         */
        static void printBytes(const ByteBuffer & buf, uint32_t position, uint32_t bytes, const std::string & label) {

            // Make sure we stay in bounds
            bytes = bytes + position > buf.capacity() ? (buf.capacity() - position) : bytes;

            if (!label.empty()) std::cout << label << ":" << std::endl;

            if (bytes < 1) {
                std::cout << "  no data in buf from position = " << position << std::endl;
                return;
            }

            for (int i=0; i < bytes; i++) {
                if (i%20 == 0) {
                    std::cout << std::endl << std::dec << std::right << std::setfill(' ') <<
                    "  Buf(" << std::setw(3) << (i + 1) <<
                    " - "<< std::setw(3) << (i + 20) << ") =  ";
                }
                else if (i%4 == 0) {
                    std::cout << "  ";
                }
                // Accessing buf in this way does not change position or limit of buffer
                std::cout << std::hex << std::noshowbase << std::internal << std::setfill('0') <<
                std::setw(2) << (int)(buf[i + position]) << " ";
            }

            std::cout << std::dec << std::endl << std::endl << std::setfill(' ');
        }


        /**
          * This method takes a pointer and prints out the desired number of bytes
          * from the given position, in hex.
          *
          * @param data      data to print out
          * @param bytes     number of bytes to print in hex
          * @param label     a label to print as header
          */
        static void printBytes(uint8_t const * data, uint32_t bytes, const std::string & label) {

            if (!label.empty()) std::cout << label << ":" << std::endl;

            if (bytes < 1) {
                return;
            }

            for (int i=0; i < bytes; i++) {
                if (i%20 == 0) {
                    std::cout << std::endl << std::dec << std::right << std::setfill(' ') <<
                              "  Buf(" << std::setw(3) << (i + 1) <<
                              " - "<< std::setw(3) << (i + 20) << ") =  ";
                }
                else if (i%4 == 0) {
                    std::cout << "  ";
                }
                // Accessing buf in this way does not change position or limit of buffer
                std::cout << std::hex << std::noshowbase << std::internal << std::setfill('0') <<
                          std::setw(2) << (int)(*((data + i))) << " ";
            }

            std::cout << std::dec << std::endl << std::endl << std::setfill(' ');
        }


        /**
         * This method takes a file and prints out the desired number of bytes
         * from the given offset, in hex.
         *
         * @param fileName file to print out
         * @param offset   offset into file to start printing
         * @param bytes    number of bytes to print in hex
         * @param label    a label to print as header
         */
        static void printBytes(const std::string & fileName, uint64_t offset,
                               uint32_t bytes, const std::string & label) {

            if (fileName.empty()) {
                std::cout << "Util::printBytes: fileName arg is invalid" << std::endl;
                return;
            }

            try {
                std::ifstream inStreamRandom;

                // "ate" mode flag will go immediately to file's end (do this to get its size)
                inStreamRandom.open(fileName, std::ios::in | std::ios::ate);
                size_t fileSize = inStreamRandom.tellg();
                // Go back to beginning of file
                inStreamRandom.seekg(0);

                // read data
                uint64_t limit = bytes + offset > fileSize ? fileSize : bytes + offset;
                auto dataLen = (uint32_t)(limit - offset);
                ByteBuffer buf(dataLen);
                uint8_t * array = buf.array();
                inStreamRandom.read(reinterpret_cast<char *>(array), dataLen);

                printBytes(buf, 0, dataLen, label);
            }
            catch (std::exception & e) {
                // e.what() does not give any useful information...
                std::cout << "Util::printBytes: " << strerror(errno) << std::endl;
            }
        }


        /**
         * This method takes a ByteBuffer and writes its data to a file.
         * This will overwrite any existing file of the same name.
         * @param fileName file to write to.
         * @param buf      buffer to write into the file.
         * @throws EvioException bad file name or unable to write.
         */
        static void writeBytes(const std::string & fileName, ByteBuffer & buf) {

            if (fileName.empty()) {
                std::cout << "Util::writeBytes: fileName arg is invalid" << std::endl;
                throw EvioException("fileName arg is invalid");
            }

            std::fstream file;
            file.open(fileName, std::ios::binary | std::ios::out);
            if (file.fail()) {
                std::cout << "error opening file " << fileName << std::endl;
                throw EvioException("error opening file " + fileName);
            }

            // Write this into a file
            file.write(reinterpret_cast<char *>(buf.array() + buf.arrayOffset() + buf.position()),
                       buf.remaining());

            if (file.fail()) {
                std::cout << "error writing to file " << fileName << std::endl;
                throw EvioException("error writing to file " + fileName);
            }
            file.close();
        }


        /**
         * This method reads part of a file into a ByteBuffer.
         * The buffer's position will be at the end of the data that is read.
         * The caller will have to flip the buffer to read it.
         *
         * @param fileName file to read from.
         * @param buf      buffer to write into.
         * @throws EvioException bad file name or unable to do I/O.
         */
        static void readBytes(const std::string & fileName, ByteBuffer & buf) {

            if (fileName.empty()) {
                std::cout << "Util::writeBytes: fileName arg is invalid" << std::endl;
                throw EvioException("fileName arg is invalid");
            }

            std::fstream file;
            file.open(fileName, std::ios::binary | std::ios::in);
            if (file.fail()) {
                std::cout << "error opening file " << fileName << std::endl;
                throw EvioException("error opening file " + fileName);
            }

            // Write this into a file
            file.read(reinterpret_cast<char *>(buf.array() + buf.arrayOffset() + buf.position()),
                       buf.remaining());

            if (file.fail()) {
                std::cout << "error reading from file " << fileName << std::endl;
                throw EvioException("error reading from file " + fileName);
            }
            file.close();
            buf.position(buf.limit());
        }


        /**
         * Return the power of 2 closest to the given argument.
         *
         * @param x value to get the power of 2 closest to.
         * @param roundUp if true, round up, else down
         * @return -1 if x is negative or the closest power of 2 to value
         */
        static int powerOfTwo(int x, bool roundUp) {
            if (x < 0) return -1;

            // The following algorithm is found in
            // "Hacker's Delight" by Henry Warren Jr.

            if (roundUp) {
                x = x - 1;
                x |= (x>>1);
                x |= (x>>2);
                x |= (x>>4);
                x |= (x>>8);
                x |= (x>>16);
                return x + 1;
            }

            int y;
            do {
                y = x;
                x &= (x - 1);
            } while (x != 0);
            return y;
        }


        /**
         * Return an input string as ASCII in which each character is one byte.
         * @param input input string.
         * @param array vector in which to place ASCII.
         */
        static void stringToASCII(const std::string & input, std::vector<uint8_t> & array) {
            size_t inputSize = input.size();
            array.clear();
            array.reserve(inputSize);

            for (int i=0; i < inputSize; i++) {
                array.push_back((uint8_t) input[i]);
            }
        }


        /**
         * Return an input string as ASCII in which each character is one byte.
         * @param input input string.
         * @param buf   ByteBuffer in which to place ASCII. Clears existing data
         *              and may expand internal storage.
         */
        static void stringToASCII(const std::string & input, ByteBuffer & buf) {
            size_t inputSize = input.size();
            buf.clear();
            buf.expand(inputSize);

            for (int i=0; i < inputSize; i++) {
                buf.put(i, input[i]);
            }
        }


        //////////////////////////////////////////////////////////////////////////
        //
        //  Methods for parsing strings in evio format.
        //  These are placed here to break the circular dependency between
        //  BaseStructure and CompositeData.
        //
        //////////////////////////////////////////////////////////////////////////


        /**
         * This method returns the number of bytes in a raw
         * evio format of the given string array, not including header.
         *
         * @param strings vector of strings to size
         * @return the number of bytes in a raw evio format of the given strings
         * @return 0 if vector empty.
         */
        static uint32_t stringsToRawSize(std::vector<std::string> const & strings) {

            if (strings.empty()) {
                return 0;
            }

            uint32_t dataLen = 0;
            for (std::string const & s : strings) {
                dataLen += s.length() + 1; // don't forget the null char after each string
            }

            // Add any necessary padding to 4 byte boundaries.
            // IMPORTANT: There must be at least one '\004'
            // character at the end. This distinguishes evio
            // string array version from earlier version.
            int pads[] = {4,3,2,1};
            dataLen += pads[dataLen%4];

            return dataLen;
        }


        /**
         * This method returns the number of bytes in a raw
         * evio format of the given string array (with a single string), not including header.
         *
         * @param str single string to size
         * @return the number of bytes in a raw evio format of the given strings or 0 if vector empty.
         */
        static uint32_t stringToRawSize(const std::string & str) {

            if (str.empty()) {
                return 0;
            }

            uint32_t dataLen = str.length() + 1; // don't forget the null char after each string

            // Add any necessary padding to 4 byte boundaries.
            // IMPORTANT: There must be at least one '\004'
            // character at the end. This distinguishes evio
            // string array version from earlier version.
            int pads[] = {4,3,2,1};
            dataLen += pads[dataLen%4];

            return dataLen;
        }


        /**
         * This method transforms an array/vector of strings into raw evio format data,
         * not including header.
         *
         * @param strings vector of strings to transform.
         * @param bytes   vector of bytes to contain evio formatted strings.
         */
        static void stringsToRawBytes(std::vector<std::string> & strings,
                                      std::vector<uint8_t> & bytes) {

            if (strings.empty()) {
                bytes.clear();
                return;
            }

            // create some storage
            int dataLen = stringsToRawSize(strings);
            std::string strData;
            strData.reserve(dataLen);

            for (std::string const & s : strings) {
                // add string
                strData.append(s);
                // add ending null
                strData.append(1, '\000');
            }

            // Add any necessary padding to 4 byte boundaries.
            // IMPORTANT: There must be at least one '\004'
            // character at the end. This distinguishes evio
            // string array version from earlier version.
            int pads[] = {4,3,2,1};
            switch (pads[strData.length()%4]) {
                case 4:
                    strData.append(4, '\004');
                    break;
                case 3:
                    strData.append(3, '\004');
                    break;
                case 2:
                    strData.append(2, '\004');
                    break;
                case 1:
                    strData.append(1, '\004');
            }

            // Transform to ASCII
            bytes.resize(dataLen);
            for (int i=0; i < strData.length(); i++) {
                bytes[i] = strData[i];
            }
        }


        /**
         * This method extracts an array of strings from byte array of raw evio string data.
         *
         * @param bytes raw evio string data.
         * @param offset offset into raw data array.
         * @param strData vector in which to place extracted strings.
         */
        static void unpackRawBytesToStrings(std::vector<uint8_t> & bytes, size_t offset,
                                            std::vector<std::string> & strData) {
            unpackRawBytesToStrings(bytes, offset, bytes.size(), strData);
        }


        /**
         * This method extracts an array of strings from byte array of raw evio string data.
         * Don't go beyond the specified max character limit and stop at the first
         * non-character value.
         *
         * @param bytes       raw evio string data
         * @param offset      offset into raw data vector
         * @param maxLength   max length in bytes of valid data in bytes vector
         * @param strData     vector in which to place extracted strings.
         */
        static void unpackRawBytesToStrings(std::vector<uint8_t> & bytes,
                                            size_t offset, size_t maxLength,
                                            std::vector<std::string> & strData) {
            int length = bytes.size() - offset;
            if (bytes.empty() || (length < 4)) return;

            // Don't read read more than maxLength ASCII characters
            length = length > maxLength ? maxLength : length;

            std::string sData(reinterpret_cast<const char *>(bytes.data()) + offset, length);
            return stringBuilderToStrings(sData, true, strData);
        }


        /**
         * This method extracts an array of strings from byte array of raw evio string data.
         * Don't go beyond the specified max character limit and stop at the first
         * non-character value.
         *
         * @param bytes       raw evio string data
         * @param length      length in bytes of valid data in bytes vector
         * @param strData     vector in which to place extracted strings.
         */
        static void unpackRawBytesToStrings(uint8_t *bytes, size_t length,
                                            std::vector<std::string> & strData) {
            if (bytes == nullptr) return;

            std::string sData(reinterpret_cast<const char *>(bytes), length);
           // std::cout << "unpackRawBytesToStrings: string = " << sData << std::endl;
            return stringBuilderToStrings(sData, true, strData);
        }


        /**
         * This method extracts an array of strings from buffer containing raw evio string data.
         *
         * @param buffer  buffer containing evio string data
         * @param pos     position of string data in buffer
         * @param length  length of string data in buffer in bytes
         * @param strData vector in which to place extracted strings.
         */
        static void unpackRawBytesToStrings(ByteBuffer & buffer,
                                            size_t pos, size_t length,
                                            std::vector<std::string> & strData) {

            if (length < 4) return;

            std::string sData(reinterpret_cast<const char *>(buffer.array() + buffer.arrayOffset()) + pos, length);
            return stringBuilderToStrings(sData, false, strData);
        }


        /**
         * This method extracts an array of strings from a string containing evio string data.
         * If non-printable chars are found (besides those used to terminate strings),
         * then 1 string with all characters will be returned. However, if the "onlyGoodChars"
         * flag is true, 1 string is returned in truncated form without
         * the bad characters at the end.<p>
         * The name of this method is taken from the java and has little to do with C++.
         * That's done for ease of code maintenance.
         *
         * @param strData        containing string data
         * @param onlyGoodChars  if true and non-printable chars found,
         *                       only 1 string with printable ASCII chars will be returned.
         * @param strings        vector in which to place extracted strings.
         */
        static void stringBuilderToStrings(std::string const & strData, bool onlyGoodChars,
                                           std::vector<std::string> & strings) {

            // Each string is terminated with a null (char val = 0)
            // and in addition, the end is padded by ASCII 4's (char val = 4).
            // However, in the legacy versions of evio, there is only one
            // null-terminated string and anything as padding. To accommodate legacy evio, if
            // there is not an ending ASCII value 4, anything past the first null is ignored.
            // After doing so, split at the nulls. Do not use the String
            // method "split" as any empty trailing strings are unfortunately discarded.

            char c;
            std::vector<int> nullIndexList;
            nullIndexList.reserve(10);
            uint32_t nullCount = 0, goodChars = 0;
            bool badFormat = true;

            size_t length = strData.length();
            bool noEnding4 = false;
            if (strData[length - 1] != '\004') {
                noEnding4 = true;
            }

            for (int i=0; i < length; i++) {
                c = strData[i];

                // If char is a null
                if (c == 0) {
                    nullCount++;
                    nullIndexList.push_back(i);
                    // If evio v2 or 3, only 1 null terminated string exists
                    // and padding is just junk or nonexistent.
                    if (noEnding4) {
                        badFormat = false;
                        break;
                    }
                }
                // Look for any non-printing/control characters (not including null)
                // and end the string there. Allow tab & newline.
                else if ((c < 32 || c > 126) && c != 9 && c != 10) {
                    if (nullCount < 1) {
                        badFormat = true;
                        // Getting garbage before first null.
                        break;
                    }

                    // Already have at least one null & therefore a String.
                    // Now we have junk or non-printing ascii which is
                    // possibly the ending 4.

                    // If we have a 4, investigate further to see if format
                    // is entirely valid.
                    if (c == '\004') {
                        // How many more chars are there?
                        int charsLeft = length - (i+1);

                        // Should be no more than 3 additional 4's before the end
                        if (charsLeft > 3) {
                            badFormat = true;
                            break;
                        }
                        else {
                            // Check to see if remaining chars are all 4's. If not, bad.
                            for (int j=1; j <= charsLeft; j++) {
                                c = strData[i+j];
                                if (c != '\004') {
                                    badFormat = true;
                                    goto pastOuterLoop;
                                }
                            }
                            badFormat = false;
                            break;
                        }
                    }
                    else {
                        badFormat = true;
                        break;
                    }
                }

                pastOuterLoop:

                // Number of good ASCII chars we have
                goodChars++;
            }

            strings.clear();

            if (badFormat) {
                if (onlyGoodChars) {
                    // Return everything in one String WITHOUT garbage
                    std::string goodStr(strData.data(), goodChars);
                    strings.push_back(goodStr);
                    return;
                }
                // Return everything in one String including possible garbage
                strings.push_back(strData);
                return;
            }

            // If here, raw bytes are in the proper format

            int firstIndex = 0;
            for (int nullIndex : nullIndexList) {
                std::string str(strData.data() + firstIndex, (nullIndex - firstIndex));
                strings.push_back(str);
                firstIndex = nullIndex + 1;
            }
        }


        //////////////////////////////////////////////////////////////////////////
        //////////////////////////////////////////////////////////////////////////


        /**
         * Substitute environmental variables in a given string
         * when they come in the form, $(env).
         * @see https://stackoverflow.com/questions/1902681/expand-file-names-that-have-environment-variables-in-their-path
         * @param text string to analyze.
         */
        static void expandEnvironmentalVariables(std::string & text) {
            static std::regex env("\\$\\(([^)]+)\\)");
            std::smatch match;
            while ( std::regex_search(text, match, env) ) {
                char * s = getenv(match[1].str().c_str());
                std::string var(s == nullptr ? "" : s);
                // Next line doesn't compile on linux gcc
                //text.replace(match[0].first, match[0].second, var);
                text.replace( match.position(0), match.length(0), var );
            }
        }


        /**
         * Count the number of integer specifiers (e.g. %34d, %07x) in the
         * given string, making sure any number preceding "x" or "d" starts with a 0.
         * This is done so there will be no empty spaces in the resulting string
         * (i.e. file name) when final substitutions are made for these specifiers.
         *
         * @param text string to analyze.
         * @return number of integer specifiers.
         */
        static uint32_t countAndFixIntSpecifiers(std::string & text) {
            static std::regex specifier("%(\\d*)([xd])");

            auto begin = std::sregex_iterator(text.begin(), text.end(), specifier);
            auto end   = std::sregex_iterator();
            uint32_t specifierCount = std::distance(begin, end);

            std::sregex_iterator i = begin;

            // Go thru all specifiers in text, only once
            for (int j = 0; j < specifierCount; j++) {
                if (j > 0) {
                    // skip over specifiers previously dealt with (text can change each loop)
                    i = std::sregex_iterator(text.begin(), text.end(), specifier);
                    int k=j;
                    while (k-- > 0) i++;
                }

                std::smatch match = *i;
                std::string specWidth = match[1].str();

                // Make sure any number preceding "x" or "d" starts with a 0 or else
                // there will be empty spaces in the resulting string (i.e. file name).
                if (specWidth.length() > 0 && specWidth[0] != '0') {
                    // Does not compile on linux gcc
                    // text.replace(match[1].first, match[1].second, "0" + specWidth);

                    // So try this:
                    // This first sub match occurs at the beginning of the match
                    auto pos = match.position();
                    auto len = specWidth.length();

                    text.replace(pos, len, "0" + specWidth);
                }
            }

            return specifierCount;
        }


        /**
         * This method generates part of a file name given a base file name as an argument.<p>
         *
         * The base file name may contain up to 3, C-style integer format specifiers
         * (such as <b>%03d</b>, or <b>%x</b>). If more than 3 are found, an exception
         * will be thrown.
         * If no "0" precedes any integer between the "%" and the "d" or "x" of the format specifier,
         * it will be added automatically in order to avoid spaces in the returned string.
         * In the {@link #generateFileName(std::string, uint32_t, uint32_t, uint64_t, uint32_t, uint32_t, uint32_t)} method,
         * the first occurrence will be substituted with the given runNumber value.
         * If the file is being split, the second will be substituted with the split number.
         * If there are multiple streams, the third will be substituted with the stream id.<p>
         *
         * The base file name may contain characters of the form <b>$(ENV_VAR)</b>
         * which will be substituted with the value of the associated environmental
         * variable or a blank string if none is found.<p>
         *
         * Finally, the base file name may contain occurrences of the string "%s"
         * which will be substituted with the value of the runType arg or nothing if
         * the runType is null.
         *
         * @param baseName          file name to start with
         * @param runType           run type/configuration name
         * @param modifiedBaseName  final generated base file name
         * @return                  number of C-style int format specifiers found in baseName arg.
         * @throws EvioException    if baseName arg is improperly formatted or blank
         */
        static int generateBaseFileName(const std::string & baseName, const std::string & runType,
                                        std::string & modifiedBaseName) {

            int* returnInts = new int[1];

            // Return the modified base file name
            modifiedBaseName = baseName;

            if (modifiedBaseName.length() < 1) {
                throw EvioException("empty string arg");
            }

            // Replace all %s occurrences with runType
            std::string::size_type pos;
            while ((pos = modifiedBaseName.find("%s")) != std::string::npos) {
                modifiedBaseName = (runType.length() < 1) ?  modifiedBaseName.replace(pos, 2, "") :
                                   modifiedBaseName.replace(pos, 2, runType);
            }

            // Scan for environmental variables of the form $(xxx)
            // and substitute the values for them (blank string if not found)
            expandEnvironmentalVariables(modifiedBaseName);

            // Count # of int specifiers, making sure any number preceding
            // "x" or "d" starts with a 0 or else there will be empty spaces
            // in the file name (%3x --> %03x).
            uint32_t specifierCount = countAndFixIntSpecifiers(modifiedBaseName);

            if (specifierCount > 3) {
                throw EvioException("baseName arg is improperly formatted");
            }

            // Return # of C-style int format specifiers
            return specifierCount;
        }


        /**
         * This method does NOT work on its own. It generates a complete file name from the
         * previously determined baseFileName obtained from calling {@link #generateBaseFileName}.
         * If evio data is to be split up into multiple files (split &gt; 0), numbers are used to
         * distinguish between the split files with splitNumber.
         * If baseFileName contains C-style int format specifiers (specifierCount &gt; 0), then
         * the first occurrence will be substituted with the given runNumber value.
         * If the file is being split, the second will be substituted with the splitNumber.
         * If there are multiple streams, the third will be substituted with the stream id.<p>
         *
         * If no specifier for the splitNumber exists, it is tacked onto the end of the file name.
         * If no specifier for the stream id exists, it is tacked onto the end of the file name,
         * after the splitNumber. No run numbers are ever tacked on without a specifier.<p>
         *
         * For splitting: if there is only 1 stream, no stream ids are used and any
         * third specifier is removed.
         * For non-splitting: if there is only 1 stream, no stream ids are used and any
         * second and third specifiers are removed. For multiple streams, the second specifier is
         * removed and the 3rd substituted with the stream id.
         * For all cases: if there are more than 3 specifiers, <b>NO SUBSTITUTIONS
         * ARE DONE.</b><p>
         *
         * @param fileName       file name to use as a basis for the generated file name
         * @param specifierCount number of C-style int format specifiers in baseFileName arg
         * @param runNumber      CODA run number
         * @param split          number of bytes at which to split off evio file
         * @param splitNumber    number of the split file
         * @param streamId       number of the stream id
         * @param streamCount    total number of streams
         *
         * @return generated file name
         *
         * @throws EvioException if the baseFileName arg contains printing format
         *                                specifiers which are not compatible with integers
         *                                and interfere with formatting.
         */
        static std::string generateFileName(std::string fileName, uint32_t specifierCount,
                                            uint32_t runNumber, uint64_t split, uint32_t splitNumber,
                                            uint32_t streamId, uint32_t streamCount) {

            if (streamCount < 1) streamCount = 1;
            if (splitNumber < 1) splitNumber = 0;
            if (runNumber < 0)   runNumber = 0;
            if (streamId < 0)    streamId = 0;
            bool oneStream = streamCount < 2;

            if (fileName.length() < 1) {
                fileName = "file";
            }

            //cout << "generateFileName: split# = " << splitNumber << ", start with    " << fileName <<
            //",    streamId = " << streamId << ", stream count = " << streamCount << ", one stream = " <<
            //oneStream << endl;
            // NOTE: no run #s are tacked on the end!

            // If we're splitting files which is always the case of CODA users ...
            if (split > 0L) {
                // For no specifiers:  tack stream id and split # onto end of file name
                if (specifierCount < 1) {
                    if (oneStream) {
                        fileName += "." + std::to_string(splitNumber);
                    }
                    else {
                        fileName += "." + std::to_string(streamId) +
                                    "." + std::to_string(splitNumber);
                    }
                }
                // For 1 specifier: insert run # at specified location,
                // then tack stream id and split # onto end of file name
                else if (specifierCount == 1) {
                    char tempChar[fileName.length() + 1024];
                    int err = std::sprintf(tempChar, fileName.c_str(), runNumber);
                    if (err < 0) throw EvioException("badly formatted file name");
                    std::string temp(tempChar);
                    fileName = temp;

                    if (oneStream) {
                        fileName += "." + std::to_string(splitNumber);
                    }
                    else {
                        fileName += "." + std::to_string(streamId) +
                                    "." + std::to_string(splitNumber);
                    }
                }
                // For 2 specifiers: insert run # and split # at specified locations
                // and place stream id immediately before split #.
                else if (specifierCount == 2) {
                    if (!oneStream) {
                        // In order to place streamId before split#, place a %d in the filename
                        // immediately before 2nd specifier.
                        static std::regex specifier("(%\\d*[xd])");
                        auto it = std::sregex_iterator(fileName.begin(), fileName.end(), specifier);

                        // Go to 2nd match
                        it++;
                        std::smatch match = *it;
                        auto pos = match.position();
                        auto len = match.length();
                        fileName.replace(pos, len, "%d." + match.str());
                        // won't compile in linux gcc
                        //fileName.replace(match[0].first, match[0].second, "%d." + match.str());

                        char tempChar[fileName.length() + 1024];
                        int err = std::sprintf(tempChar, fileName.c_str(), runNumber, streamId, splitNumber);
                        if (err < 0) throw EvioException("badly formatted file name");
                        std::string temp(tempChar);
                        fileName = temp;
                    }
                    else {
                        char tempChar[fileName.length() + 1024];
                        int err = std::sprintf(tempChar, fileName.c_str(), runNumber, splitNumber);
                        if (err < 0) throw EvioException("badly formatted file name");
                        std::string temp(tempChar);
                        fileName = temp;
                    }
                }
                // For 3 specifiers: insert run #, stream id, and split # at specified locations
                else if (specifierCount == 3) {
                    char tempChar[fileName.length() + 1024];
                    int err = std::sprintf(tempChar, fileName.c_str(), runNumber, streamId, splitNumber);
                    if (err < 0) throw EvioException("badly formatted file name");
                    std::string temp(tempChar);
                    fileName = temp;
                }

            }
            // If we're not splitting files, then CODA isn't being used and stream id is
            // probably meaningless.
            else {
                // For no specifiers:  tack stream id onto end of file name
                if (specifierCount < 1) {
                    if (!oneStream) {
                        fileName += "." + std::to_string(streamId);
                    }
                }
                else if (specifierCount == 1) {
                    // Insert runNumber
                    char tempChar[fileName.length() + 1024];
                    int err = std::sprintf(tempChar, fileName.c_str(), runNumber);
                    if (err < 0) throw EvioException("badly formatted file name");
                    std::string temp(tempChar);
                    fileName = temp;

                    if (!oneStream) {
                        fileName += "." + std::to_string(streamId);
                    }
                }
                else if (specifierCount == 2) {
                    // First get rid of the extra (2nd) int format specifier as no split # exists
                    static std::regex specifier("(%\\d*[xd])");
                    auto it = std::sregex_iterator(fileName.begin(), fileName.end(), specifier);
                    // Go to 2nd match
                    it++;
                    std::smatch match = *it;
                    auto pos = match.position();
                    auto len = match.length();
                    fileName.replace(pos, len, "");
                    // won't compile in linux gcc
                    // fileName.replace(match[0].first, match[0].second, "");

                    // Insert runNumber into first specifier
                    char tempChar[fileName.length() + 1024];
                    int err = std::sprintf(tempChar, fileName.c_str(), runNumber);
                    if (err < 0) throw EvioException("badly formatted file name");
                    std::string temp(tempChar);
                    fileName = temp;

                    if (!oneStream) {
                        fileName += "." + std::to_string(streamId);
                    }
                }
                else if (specifierCount == 3) {
                    // Get rid of extra (3rd) int format specifier as no split # exists
                    static std::regex specifier("(%\\d*[xd])");
                    auto it = std::sregex_iterator(fileName.begin(), fileName.end(), specifier);
                    // Go to 3rd match
                    it++; it++;
                    std::smatch match = *it;
                    auto pos = match.position();
                    auto len = match.length();
                    fileName.replace(pos, len, "");
                    // won't compile in linux gcc
                    // fileName.replace(match[0].first, match[0].second, "");

                    // Insert runNumber into first specifier, stream id into 2nd
                    char tempChar[fileName.length() + 1024];
                    int err = std::sprintf(tempChar, fileName.c_str(), runNumber, streamId);
                    if (err < 0) throw EvioException("badly formatted file name");
                    std::string temp(tempChar);
                    fileName = temp;
                }
            }
            //cout << "generateFileName: end with    " << fileName << endl;

            return fileName;
        }

    };

}


#endif //EVIO_6_0_UTIL_H
