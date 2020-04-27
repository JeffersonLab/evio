/**
 * Copyright (c) 2020, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 11/02/2020
 * @author timmer
 */

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

using namespace std;


namespace evio {


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
     * Write int into byte array.
     *
     * @param data        int to convert.
     * @param byteOrder   byte order of array.
     * @param dest        array in which to write int.
     * @param off         offset into array where int is to be written.
     * @param destMaxSize max size in bytes of dest array.
     * @throws EvioException if dest is null or too small.
     */
    static void toBytes(uint32_t data, const ByteOrder & byteOrder,
                        uint8_t* dest, uint32_t off, uint32_t destMaxSize) {

        if (dest == nullptr || destMaxSize < 4+off) {
            throw EvioException("bad arg(s)");
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
     * Write int into byte vector.
     *
     * @param data        int to convert.
     * @param byteOrder   byte order of vector.
     * @param dest        vector in which to write int.
     * @param off         offset into vector where int is to be written.
     */
    static void toBytes(uint32_t data, const ByteOrder & byteOrder,
                        std::vector<uint8_t> & dest, uint32_t off) {

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
     * @param off         offset into array where short is to be written.
     * @param destMaxSize max size in bytes of dest array.
     * @throws EvioException if dest is null or too small.
     */
    static void toBytes(uint16_t data, const ByteOrder & byteOrder,
                        uint8_t* dest, uint32_t off, uint32_t destMaxSize) {

        if (dest == nullptr || destMaxSize < 2+off) {
            throw EvioException("bad arg(s)");
        }

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
     * Write short into byte vector.
     *
     * @param data        short to convert.
     * @param byteOrder   byte order of vector.
     * @param dest        vector in which to write short.
     * @param off         offset into vector where short is to be written.
     */
    static void toBytes(uint16_t data, const ByteOrder & byteOrder,
                        std::vector<uint8_t> & dest, uint32_t off) {

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
     * This method reads and swaps an evio bank header.
     * It can also return information about the bank.
     * Position and limit of neither buffer argument is changed.<p></p>
     * <b>This only swaps data if buffer arguments have opposite byte order!</b>
     *
     * @param node       object in which to store data about the bank
     *                   in destBuffer after swap.
     * @param srcBuffer  buffer containing bank header to be swapped.
     * @param destBuffer buffer in which to place swapped bank header.
     * @param srcPos     position in srcBuffer to start reading bank header.
     * @param destPos    position in destBuffer to start writing swapped bank header.
     *
     * @throws EvioException if srcBuffer data underflow;
     *                       if destBuffer is too small to contain swapped data;
     *                       srcBuffer and destBuffer have same byte order.
     */
    static void swapBankHeader(EvioNode & node, ByteBuffer & srcBuffer, ByteBuffer & destBuffer,
                               uint32_t srcPos, uint32_t destPos) {

        // Check endianness
        if (srcBuffer.order() == destBuffer.order()) {
            throw EvioException("src & dest buffers need different byte order for swapping");
        }

        // Read & swap first bank header word
        uint32_t length = srcBuffer.getInt(srcPos);
        destBuffer.putInt(destPos, length);
        srcPos  += 4;
        destPos += 4;

        // Read & swap second bank header word
        uint32_t word = srcBuffer.getInt(srcPos);
        destBuffer.putInt(destPos, word);

        node.tag      = (word >> 16) & 0xffff;
        uint32_t dt   = (word >> 8) & 0xff;
        node.dataType = dt & 0x3f;
        node.pad      = dt >> 6;
        node.num      = word & 0xff;
        node.len      = length;
        node.pos      = destPos - 4;
        node.dataPos  = destPos + 4;
        node.dataLen  = length - 1;
    }


    /**
     * This method reads and swaps an evio segment header.
     * It can also return information about the segment.
     * Position and limit of neither buffer argument is changed.<p></p>
     * <b>This only swaps data if buffer arguments have opposite byte order!</b>
     *
     * @param node       object in which to store data about the segment
     *                   in destBuffer after swap; may be null
     * @param srcBuffer  buffer containing segment header to be swapped
     * @param destBuffer buffer in which to place swapped segment header
     * @param srcPos     position in srcBuffer to start reading segment header
     * @param destPos    position in destBuffer to start writing swapped segment header
     *
     * @throws EvioException if srcBuffer data underflow;
     *                       if destBuffer is too small to contain swapped data;
     *                       srcBuffer and destBuffer have same byte order.
     */
    static void swapSegmentHeader(EvioNode & node, ByteBuffer & srcBuffer, ByteBuffer & destBuffer,
                                  uint32_t srcPos, uint32_t destPos) {

        if (srcBuffer.order() == destBuffer.order()) {
            throw EvioException("src & dest buffers need different byte order for swapping");
        }

        // Read & swap segment header word
        uint32_t word = srcBuffer.getInt(srcPos);
        destBuffer.putInt(destPos, word);

        node.tag      = (word >> 24) & 0xff;
        uint32_t dt   = (word >> 16) & 0xff;
        node.dataType = dt & 0x3f;
        node.pad      = dt >> 6;
        node.len      = word & 0xffff;
        node.num      = 0;
        node.pos      = destPos;
        node.dataPos  = destPos + 4;
        node.dataLen  = node.len;
    }


    /**
     * This method reads and swaps an evio tagsegment header.
     * It can also return information about the tagsegment.
     * Position and limit of neither buffer argument is changed.<p></p>
     * <b>This only swaps data if buffer arguments have opposite byte order!</b>
     *
     * @param node       object in which to store data about the tagsegment
     *                   in destBuffer after swap; may be null
     * @param srcBuffer  buffer containing tagsegment header to be swapped
     * @param destBuffer buffer in which to place swapped tagsegment header
     * @param srcPos     position in srcBuffer to start reading tagsegment header
     * @param destPos    position in destBuffer to start writing swapped tagsegment header
     *
     * @throws EvioException if srcBuffer is not properly formatted;
     *                       if destBuffer is too small to contain swapped data
     */
    static void swapTagSegmentHeader(EvioNode & node, ByteBuffer & srcBuffer, ByteBuffer & destBuffer,
                                     uint32_t srcPos, uint32_t destPos) {

        if (srcBuffer.order() == destBuffer.order()) {
            throw EvioException("src & dest buffers need different byte order for swapping");
        }

        // Read & swap tagsegment header word
        uint32_t word = srcBuffer.getInt(srcPos);
        destBuffer.putInt(destPos, word);

        node.tag      = (word >> 20) & 0xfff;
        node.dataType = (word >> 16) & 0xf;
        node.len      = word & 0xffff;
        node.num      = 0;
        node.pad      = 0;
        node.pos      = destPos;
        node.dataPos  = destPos + 4;
        node.dataLen  = node.len;
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
    static void printBytes(const ByteBuffer & buf, uint32_t position, uint32_t bytes, const string & label) {

        // Make sure we stay in bounds
        bytes = bytes + position > buf.capacity() ? (buf.capacity() - position) : bytes;

        if (!label.empty()) cout << label << ":" << endl;

        if (bytes < 1) {
            cout << "  no data in buf from position = " << position << endl;
            return;
        }

        for (int i=0; i < bytes; i++) {
            if (i%20 == 0) {
                cout << endl << dec << "  Buf(" << (i + 1) << "-" << (i + 20) << ") =  ";
            }
            else if (i%4 == 0) {
                cout << "  ";
            }
            // Accessing buf in this way does not change position or limit of buffer
            cout << hex << setfill('0') << setw(2) << (int)(buf[i]) << " ";
        }
        cout << dec << endl << endl << setfill(' ');
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
    static void printBytes(const string & fileName, uint64_t offset,
                           uint32_t bytes, const string & label) {

        if (fileName.empty()) {
            cout << "Util::printBytes: fileName arg is invalid" << endl;
            return;
        }

        try {
            ifstream inStreamRandom;

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
            cout << "Util::printBytes: " << strerror(errno) << endl;
        }
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

//    /**
//     * Return an input string as ASCII in which each character is one byte.
//     * @param input     input string.
//     * @param array     array in which to place ASCII.
//     * @param arraySize size of arry in bytes.
//     */
//    static void stringToASCII(const string & input, uint8_t *array, uint32_t arraySize) {
//        size_t inputSize = input.size();
//        if (arraySize < inputSize) {
//            throw EvioException("array too small to hold string as ASCII");
//        }
//
//        for (int i=0; i < inputSize; i++) {
//            array[i] = (uint8_t) input[i];
//        }
//    }

    /**
     * Return an input string as ASCII in which each character is one byte.
     * @param input input string.
     * @param array vector in which to place ASCII.
     */
    static void stringToASCII(const string & input, std::vector<uint8_t> & array) {
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
    static void stringToASCII(const string & input, ByteBuffer & buf) {
        size_t inputSize = input.size();
        buf.clear();
        buf.expand(inputSize);

        for (int i=0; i < inputSize; i++) {
            buf.put(i, input[i]);
        }
    }

    /**
     * Substitute environmental variables in a given string
     * when they come in the form, $(env).
     * @param text string to analyze.
     */
    static void expandEnvironmentalVariables(string & text) {
        static std::regex env("\\$\\(([^)]+)\\)");
        std::smatch match;
        while ( std::regex_search(text, match, env) ) {
            const char * s = getenv(match[1].str().c_str());
            const std::string var(s == nullptr ? "" : s);
            text.replace(match[0].first, match[0].second, var);
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
    static uint32_t countAndFixIntSpecifiers(string & text) {
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
                text.replace(match[1].first, match[1].second, "0" + specWidth);
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
     * In the {@link #generateFileName(String, int, int, long, int, int, int)} method,
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
    static int generateBaseFileName(const string & baseName, const string & runType,
                                    string & modifiedBaseName) {

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
    static string generateFileName(string fileName, uint32_t specifierCount,
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
                    string temp(tempChar);
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
                        fileName.replace(match[0].first, match[0].second, "%d." + match.str());

                        char tempChar[fileName.length() + 1024];
                        int err = std::sprintf(tempChar, fileName.c_str(), runNumber, streamId, splitNumber);
                        if (err < 0) throw EvioException("badly formatted file name");
                        string temp(tempChar);
                        fileName = temp;
                    }
                    else {
                        char tempChar[fileName.length() + 1024];
                        int err = std::sprintf(tempChar, fileName.c_str(), runNumber, splitNumber);
                        if (err < 0) throw EvioException("badly formatted file name");
                        string temp(tempChar);
                        fileName = temp;
                    }
                }
                // For 3 specifiers: insert run #, stream id, and split # at specified locations
                else if (specifierCount == 3) {
                    char tempChar[fileName.length() + 1024];
                    int err = std::sprintf(tempChar, fileName.c_str(), runNumber, streamId, splitNumber);
                    if (err < 0) throw EvioException("badly formatted file name");
                    string temp(tempChar);
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
                    string temp(tempChar);
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
                    fileName.replace(match[0].first, match[0].second, "");

                    // Insert runNumber into first specifier
                    char tempChar[fileName.length() + 1024];
                    int err = std::sprintf(tempChar, fileName.c_str(), runNumber);
                    if (err < 0) throw EvioException("badly formatted file name");
                    string temp(tempChar);
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
                    fileName.replace(match[0].first, match[0].second, "");

                    // Insert runNumber into first specifier, stream id into 2nd
                    char tempChar[fileName.length() + 1024];
                    int err = std::sprintf(tempChar, fileName.c_str(), runNumber, streamId);
                    if (err < 0) throw EvioException("badly formatted file name");
                    string temp(tempChar);
                    fileName = temp;
                }
            }
//cout << "generateFileName: end with    " << fileName << endl;

            return fileName;
    }



};

}


#endif //EVIO_6_0_UTIL_H
