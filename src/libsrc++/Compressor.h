//
// Copyright 2020, Jefferson Science Associates, LLC.
// Subject to the terms in the LICENSE file found in the top-level directory.
//
// EPSCI Group
// Thomas Jefferson National Accelerator Facility
// 12000, Jefferson Ave, Newport News, VA 23606
// (757)-269-7100


#ifndef EVIO_6_0_COMPRESSOR_H
#define EVIO_6_0_COMPRESSOR_H


#include <string>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <sstream>


#include "EvioException.h"
#include "ByteBuffer.h"
#include "lz4.h"
#include "lz4hc.h"

#ifdef USE_GZIP
    #include "zlib.h"
#endif


namespace evio {


    /**
     * Singleton class used to provide data compression and decompression in a variety of formats.
     * This class in NOT thread safe when using the gzip deflating and inflating routines.
     * @date 04/29/2019
     * @author timmer
     */
    class Compressor {


    public:

        /**
         * Get an instance of this singleton class.
         * @return an instance of this singleton class.
         */
        static Compressor& getInstance() {
            static Compressor theCompressor;   // Instantiated when this function is called
            return theCompressor;
        }

    private:

        Compressor();                              // constructor is private
        Compressor(const Compressor &);            // copy constructor is private
        Compressor& operator=(Compressor const&);  // assignment operator is private
        ~Compressor() = default;                   // destructor is private

    public:

        /** Enum of supported data compression types. */
        enum CompressionType {
            UNCOMPRESSED = 0,
            LZ4,
            LZ4_BEST,
            GZIP
        };

        static CompressionType toCompressionType(uint32_t type);

    private:

#ifdef USE_GZIP
        static z_stream strmDeflate;
        static z_stream strmInflate;
#endif

        /** Number of bytes to read in a single call while doing gzip decompression. */
        static const uint32_t MTU = 1024*1024;

        /* Makes regular lz4 compression to be lz4Acceleration * 3% speed up. */
        static const int lz4Acceleration = 1;

        static uint32_t getYear(       ByteBuffer & buf);
        static uint32_t getRevisionId( ByteBuffer & buf, uint32_t board_id);
        static uint32_t getSubsystemId(ByteBuffer & buf, uint32_t board_id);
        static uint32_t getDeviceId(   ByteBuffer & buf, uint32_t board_id);

        static void setUpCompressionHardware();
        static void setUpZlib();

    public:

        static int getMaxCompressedLength(CompressionType compressionType, uint32_t uncompressedLength);

        //---------------
        // GZIP
        //---------------
#ifdef USE_GZIP
        static uint8_t* compressGZIP(uint8_t* ungzipped, uint32_t offset,
                                     uint32_t length, uint32_t *compLen);

        static uint8_t* uncompressGZIP(uint8_t* gzipped, uint32_t off,
                                       uint32_t length, uint32_t *uncompLen, uint32_t origUncompLen);

        static int compressGZIP(uint8_t* dest, uint32_t *destLen,
                                const uint8_t* source, uint32_t sourceLen);

        static int uncompressGZIP(uint8_t* dest, uint32_t *destLen,
                                  const uint8_t* source, uint32_t *sourceLen,
                                  uint32_t uncompLen);

        static uint8_t* uncompressGZIP(ByteBuffer & gzipped, uint32_t *uncompLen);
#endif

        //---------------
        // LZ4
        //---------------
        static int compressLZ4(ByteBuffer & src, int srcSize, ByteBuffer & dst, int maxSize);
        static int compressLZ4(uint8_t *src, int srcOff, int srcSize,
                               uint8_t *dst, int dstOff, int maxSize);
        static int compressLZ4(ByteBuffer & src, int srcOff, int srcSize,
                               ByteBuffer & dst, int dstOff, int maxSize);
        static int compressLZ4Best(ByteBuffer & src, int srcSize, ByteBuffer & dst, int maxSize);
        static int compressLZ4Best(uint8_t *src, int srcOff, int srcSize,
                                   uint8_t *dst, int dstOff, int maxSize);
        static int compressLZ4Best(ByteBuffer & src, int srcOff, int srcSize,
                                   ByteBuffer & dst, int dstOff, int maxSize);

        static int uncompressLZ4(ByteBuffer & src, int srcSize, ByteBuffer & dst);
        static int uncompressLZ4(ByteBuffer & src, int srcOff, int srcSize, ByteBuffer & dst);
        static int uncompressLZ4(ByteBuffer & src, int srcOff, int srcSize, ByteBuffer & dst, int dstOff);
        static int uncompressLZ4(uint8_t *src, int srcOff, int srcSize, uint8_t *dst,
                                 int dstOff, int dstCapacity);


    };

}

#endif //EVIO_6_0_COMPRESSOR_H
