/**
 * Copyright (c) 2019, Jefferson Science Associates
 *
 * Thomas Jefferson National Accelerator Facility
 * Data Acquisition Group
 *
 * 12000, Jefferson Ave, Newport News, VA 23606
 * Phone : (757)-269-7100
 *
 * @date 04/29/2019
 * @author timmer
 */

#include "Compressor.h"


namespace evio {


#ifdef USE_GZIP
    z_stream Compressor::strmDeflate;
    z_stream Compressor::strmInflate;
#endif


Compressor::Compressor() {
    setUpCompressionHardware();
    setUpZlib();
}


uint32_t Compressor::getYear(ByteBuffer & buf) {
    uint32_t rv = 0;
    rv |= (uint32_t)buf.get(6);
    rv &= 0x000000ff;
    rv |= (uint32_t)buf.get(7) << 8;
    rv &= 0x0000ffff;
    return rv;
}

uint32_t Compressor::getRevisionId(ByteBuffer & buf, uint32_t board_id) {
    uint32_t rv = 0;
    rv |= buf.get((9 + board_id));
    rv &= 0x000000ff;
    return rv;
}

uint32_t Compressor::getSubsystemId(ByteBuffer & buf, uint32_t board_id) {
    uint32_t rv = 0;
    uint32_t offset = (26 + (board_id * 2));
    rv |= buf.get(offset);
    rv &= 0x000000ff;
    rv |= buf.get((offset + 1)) << 8;
    rv &= 0x0000ffff;
    return rv;
}

uint32_t Compressor::getDeviceId(ByteBuffer & buf, uint32_t board_id) {
    uint32_t rv = 0;
    uint32_t offset = (58 + (board_id * 2));
    rv |= buf.get(offset);
    rv &= 0x000000ff;
    rv |= buf.get((offset + 1)) << 8;
    rv &= 0x0000ffff;
    return rv;
}

Compressor::CompressionType Compressor::toCompressionType(uint32_t type) {
    switch (type) {
        case GZIP:
            return GZIP;
        case LZ4_BEST:
            return LZ4_BEST;
        case LZ4:
            return LZ4;
        case UNCOMPRESSED:
        default:
            return UNCOMPRESSED;
    }
}

void Compressor::setUpZlib() {

#ifdef USE_GZIP

    /* init deflate state */
    strmDeflate.next_in = Z_NULL;
    strmDeflate.zalloc  = Z_NULL;
    strmDeflate.zfree   = Z_NULL;
    strmDeflate.opaque  = Z_NULL;

    int level = Z_DEFAULT_COMPRESSION; // =6, 1 gives top speed, 9 top compression
    int windowBits = 15 + 16; // 15 is default and adding 16 makes it gzip and not zlib format
    int memLevel = 9;         // Highest mem usage for best speed

    int ret = deflateInit2(&strmDeflate, level, Z_DEFLATED, windowBits,
                           memLevel, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK)
        throw HipoException("error initializing gzip deflate stream");

    //-----------------------------------------------------------------

    /* init deflate state */
    strmInflate.next_in  = Z_NULL;
    strmInflate.avail_in = Z_NULL;
    strmInflate.zalloc   = Z_NULL;
    strmInflate.zfree    = Z_NULL;
    strmInflate.opaque   = Z_NULL;

    ret = inflateInit2(&strmInflate, windowBits);
    if (ret != Z_OK)
        throw HipoException("error initializing gzip inflate stream");

#endif

}

/**
 * CHeck for existence of AHA3641/2 board for gzip hardware compression.
 * This will most likely not be used so comment it out.
 */
void Compressor::setUpCompressionHardware() {

//    // Check for an AHA374 FPGA data compression board for hardware (de)compression.
//    // Only want to do this once. Need the jar which uses native methods,
//    // the C lib which it wraps using jni, and the C lib which does the compression.
//    // Both C libs must have their directory in LD_LIBRARY_PATH for this to work!
//
//    /** Do we have an AHA374 data compression board on the local host? */
//    static bool haveHardwareGzipCompression = false;
//
//    try {
//
//        AHACompressionAPI apiObj = new AHACompressionAPI();
//        ByteBuffer as(8);
//        as.order(ByteOrder::ENDIAN_LOCAL);
//        int rv = AHACompressionAPI.Open(apiObj, as, 0, 0);
//        cout << "rv = " << rv << endl;
//
//        rv = 0;
//        string betaString;
//        int versionArgSize = 89;
//        ByteBuffer versionArg(versionArgSize);
//        versionArg.order(ByteOrder::ENDIAN_LOCAL);
//
//        long longrv = AHACompressionAPI.Version(apiObj, as, versionArg);
//        if(longrv != 0){
//            rv = -1;
//        }
//
//        int loopMax = versionArg.get(8);
//        if(versionArg.get(0) != 0){
//            betaString = " Beta " + to_string(versionArg.get(0));
//        }else{
//            betaString = "";
//        }
//
//        string receivedDriverVersion = to_string(versionArg.get(3)) + "." +
//                                       to_string(versionArg.get(2)) + "." +
//                                       to_string(versionArg.get(1)) + betaString;
//        string receivedReleaseDate = to_string(versionArg.get(5)) + "/" +
//                                     to_string(versionArg.get(4)) + "/" +
//                                     to_string(getYear(versionArg));
//        string receivedNumBoards = "" + to_string(versionArg.get(8));
//
//        cout << "driver version  = "  << receivedDriverVersion << endl;
//        cout << "release date    = " << receivedReleaseDate << endl;
//        cout << "number of chips = " << receivedNumBoards << endl;
//
//        for(int j = 0; j < loopMax; j++) {
//            stringstream ss;
//
//            // show 0x for hex
//            ss << showbase << hex;
//
//            ss << setw(2) << getRevisionId(versionArg, j) << endl;
//            string receivedHwRevisionId = ss.str();
//            ss.str("");
//            ss.clear();
//
//            ss << setw(4) << getSubsystemId(versionArg, j) << endl;
//            string receivedHwSubsystemId = ss.str();
//            ss.str("");
//            ss.clear();
//
//            ss << setw(4) << getDeviceId(versionArg, j) << endl;
//            string receivedHwDeviceId = ss.str();
//            ss.str("");
//            ss.clear();
//
//            cout << "revision  id (" << j << ")= " << receivedHwRevisionId << endl;
//            cout << "subsystem id (" << j << ")= " << receivedHwSubsystemId << endl;
//            cout << "device    id (" << j << ")= " << receivedHwDeviceId << endl;
//        }
//
//        haveHardwareGzipCompression = true;
//        cout << endl << "Successfully loaded aha3xx jni shared lib for gzip hardware compression available" << endl << endl;
//    }
//    catch (Error & e) {
//        // If the library cannot be found, we can still do compression -
//        // just not with the AHA374 compression board.
//        cout << endl << "Cannot load aha3xx jni shared lib so no gzip hardware compression available" << endl << endl;
//    }
}



/**
 * Returns the maximum number of bytes needed to compress the given length
 * of uncompressed data. Depends on compression type. Unknown for gzip.
 *
 * @param compressionType type of data compression to do
 *                        (0=none, 1=lz4 fast, 2=lz4 best, 3=gzip).
 *                        Default to none.
 * @param uncompressedLength uncompressed data length in bytes.
 * @return maximum compressed length in bytes or -1 if unknown.
 */
int Compressor::getMaxCompressedLength(CompressionType compressionType, uint32_t uncompressedLength) {
    switch(compressionType) {
        case GZIP:
#ifdef USE_GZIP
            return deflateBound(&strmDeflate, uncompressedLength);
#else
            return -1;
#endif
        case LZ4_BEST:
        case LZ4:
            return LZ4_compressBound(uncompressedLength);
        case UNCOMPRESSED:
        default:
            return uncompressedLength;
    }
}

//---------------------------
// GZIP Compression
//---------------------------

#ifdef USE_GZIP
/**
 * GZIP compression. Returns locally allocated compressed byte array.
 * Caller must delete[] it.
 *
 * @param ungzipped  uncompressed data.
 * @param offset     offset into ungzipped array
 * @param length     length of valid data in bytes
 * @param compLen    number of bytes in returned array.
 * @return compressed data.
 * @throws HipoException if arg(s) are null.
 */
uint8_t* Compressor::compressGZIP(uint8_t* ungzipped, uint32_t offset,
                                  uint32_t length, uint32_t *compLen) {

    if (ungzipped == nullptr || compLen == nullptr) {
        throw HipoException("ungzipped and/or compLen arg is null");
    }

    uint32_t dstLen = deflateBound(&strmDeflate, length);
    auto *dst = new uint8_t[dstLen];

    // This should not generate an error
    compressGZIP(dst, &dstLen, (ungzipped + offset), length);

    // Number of valid return bytes
    *compLen = dstLen;

    return dst;
}


/**
 * GZIP decompression. Returns locally allocated uncompressed byte array.
 * Caller must delete[] it.
 *
 * @param gzipped compressed data.
 * @param off     offset into gzipped array.
 * @param length  number of bytes to read from gzipped.
 * @param uncompLen upon entry, *uncompLen is the size of destination buffer
 *                  to be locally allocated. Upon exit, *uncompLen is the size
 *                  of the uncompressed data in that buffer.
 * @param origUncompLen size of originally uncompressed data in bytes.
 *                      (The size of the uncompressed data must have been saved previously
 *                      by the compressor and transmitted to the decompressor by some
 *                      mechanism outside the scope of this compression library.)
 * @return uncompressed data.
 * @throws HipoException if gzipped or uncompLen pointer(s) are null,
 *                       if there was not enough room in the output buffer, or
 *                       if the input data was corrupted, including if the input data is
 *                       an incomplete stream.
 */
uint8_t* Compressor::uncompressGZIP(uint8_t* gzipped, uint32_t off,
                                    uint32_t length, uint32_t *uncompLen,
                                    uint32_t origUncompLen) {

    if (gzipped == nullptr || uncompLen == nullptr) {
        throw HipoException("gzipped and/or uncompLen arg is null");
    }

    uint32_t destLen = *uncompLen;
    auto ungzipped = new uint8_t[destLen];

    int err = uncompressGZIP(ungzipped, &destLen, gzipped + off, &length, origUncompLen);
    if (err != Z_OK) {
        delete [] ungzipped;
        throw HipoException("error uncompressing data");
    }

    // size of uncompressed data
    *uncompLen = destLen;

    return ungzipped;
}


/**
 * This method is based on zlib's compress2() function.
 * It compresses the source buffer into the destination buffer in the gzip format.
 *
 * @param dest buffer for holding compressed data.
 * @param destLen upon entry, *destLen is the total size of the destination buffer,
 *                which must be at least 0.1% larger than sourceLen plus 12 bytes.
 *                Upon exit, *destLen is the actual size of the compressed buffer.
 * @param source buffer holding uncompressed data.
 * @param sourceLen byte length of the source buffer.
 * @return  Z_OK if success,
 *          Z_MEM_ERROR if there was not enough memory,
 *          Z_BUF_ERROR if there was not enough room in the output buffer,
 *          Z_STREAM_ERROR if the level parameter is invalid.
 * @throws HipoException if the value at destLen is too small, destLen is null,
 *                       dest is null, or source in null.
 */
int Compressor::compressGZIP(uint8_t* dest, uint32_t *destLen,
                             const uint8_t* source, uint32_t sourceLen) {

    if (destLen == nullptr) {
        throw HipoException("null pointer for destLen arg");
    }

    if (dest == nullptr || source == nullptr) {
        throw HipoException("null pointer for one or both buffer args");
    }

    if ((deflateBound(&strmDeflate, sourceLen)) < *destLen) {
        throw HipoException("destination buffer is too small");
    }

    strmDeflate.next_out  = dest;
    strmDeflate.avail_out = *destLen;

    strmDeflate.next_in   = (z_const Bytef *)source;
    strmDeflate.avail_in  = sourceLen;

    int err = deflate(&strmDeflate, Z_FINISH);

    *destLen = strmDeflate.total_out;
    deflateReset(&strmDeflate);

    return err == Z_STREAM_END ? Z_OK : err;
}


/**
 *    Decompresses the source buffer into the destination buffer.
 *
 *    It returns Z_OK if success, Z_MEM_ERROR if there was not enough
 *    memory, Z_BUF_ERROR if there was not enough room in the output buffer, or
 *    Z_DATA_ERROR if the input data was corrupted, including if the input data is
 *    an incomplete stream.
 *
 *
 * @param dest buffer holding uncompressed data.
 * @param destLen upon entry, *destLen is the total size of the destination buffer,
 *                which must be large enough to hold the entire uncompressed data.
 *                Upon exit, *destLen is the size of the uncompressed data.
 * @param source buffer holding compressed data.
 * @param sourceLen  upon entry, *sourceLen is is the byte length of the source buffer.
 *                   Upon exit, *sourceLen is the number of source bytes consumed.
 *                   Upon return, source + *sourceLen points to the first unused input byte.
 * @param uncompLen size of originally uncompressed data in bytes.
 *                  (The size of the uncompressed data must have been saved previously
 *                  by the compressor and transmitted to the decompressor by some
 *                  mechanism outside the scope of this compression library.)
 * @return Z_OK if success,
 *         Z_MEM_ERROR if there was not enough memory,
 *         Z_BUF_ERROR if there was not enough room in the output buffer, or
 *         Z_DATA_ERROR if the input data was corrupted, including if the input data is
 *                      an incomplete stream.
 * @throws HipoException if the value at destLen is too small or destLen is null.
 */
int Compressor::uncompressGZIP(uint8_t* dest, uint32_t *destLen,
                               const uint8_t* source, uint32_t *sourceLen,
                               uint32_t uncompLen) {

    if (destLen == nullptr || sourceLen == nullptr) {
        throw HipoException("null pointer for destLen and/or sourceLen arg(s)");
    }

    if (dest == nullptr || source == nullptr) {
        throw HipoException("null pointer for one or both buffer args");
    }

    if (*destLen < uncompLen) {
        throw HipoException("destination buffer is too small");
    }

    uint32_t len  = *sourceLen;
    uint32_t left = *destLen;

    strmInflate.next_in   = (z_const Bytef *)source;
    strmInflate.avail_in  = len;

    strmInflate.next_out  = dest;
    strmInflate.avail_out = left;

    int err = inflate(&strmInflate, Z_FINISH);

    *sourceLen = len - strmInflate.avail_in;
    *destLen   = strmInflate.total_out;

    uint32_t avail_out = strmInflate.avail_out;

    inflateReset(&strmInflate);

    return err == Z_STREAM_END ? Z_OK :
           err == Z_NEED_DICT ? Z_DATA_ERROR  :
           err == Z_BUF_ERROR && left + avail_out ? Z_DATA_ERROR :
           err;
}

/**
 * GZIP decompression. Returns locally allocated uncompressed byte array.
 * Caller must delete[] it.
 *
 * @param gzipped compressed data.
 * @param uncompLen pointer to be filled with uncompressed data size in bytes.
 * @return uncompressed data. Number of valid bytes returned in uncompLen.
 * @throws HipoException if error in uncompressing gzipped data.
 */
uint8_t* Compressor::uncompressGZIP(ByteBuffer & gzipped, uint32_t * uncompLen) {

    // Length of compressed data
    uint32_t srcLen = gzipped.remaining();

    // Max length of uncompressed data.
    // As a rough overestimate, create array double the compressed data size.
    uint32_t dstLen = 2*srcLen;
    auto ungzipped = new uint8_t[dstLen];

    int err = uncompressGZIP(ungzipped, &dstLen, gzipped.array(), &srcLen, dstLen);
    if (err != Z_OK) {
        delete[] ungzipped;
        throw HipoException("error in uncompressing gzipped data");
    }

    *uncompLen = dstLen;

    return ungzipped;
}
#endif


//---------------------------
// LZ4 Fast Compression
//---------------------------

/**
 * Fastest LZ4 compression. Returns length of compressed data in bytes.
 *
 * @param src      source of uncompressed data from position.
 * @param srcSize  number of bytes to compress.
 * @param dst      destination buffer from position.
 * @param maxSize  maximum number of bytes to write in dst.
 * @return length of compressed data in bytes.
 * @throws HipoException if maxSize < max # of compressed bytes or compression failed.
 */
int Compressor::compressLZ4(ByteBuffer & src, int srcSize, ByteBuffer & dst, int maxSize) {

    if (LZ4_compressBound(srcSize) > maxSize) {
        throw HipoException("maxSize (" + to_string(maxSize) +
                            ") is < max # of compressed bytes (" +
                            to_string(LZ4_compressBound(srcSize)) + ")");
    }

    int size = LZ4_compress_fast((const char*)(src.array() + src.position()),
                                 (char*)(dst.array() + dst.position()),
                                 srcSize, maxSize, lz4Acceleration);
    if (size < 1) {
        throw HipoException("compression failed");
    }

    return size;
}

/**
 * Fastest LZ4 compression. Returns length of compressed data in bytes.
 *
 * @param src      source of uncompressed data.
 * @param srcOff   start offset in src.
 * @param srcSize  number of bytes to compress.
 * @param dst      destination array.
 * @param dstOff   start offset in dst.
 * @param maxSize  maximum number of bytes to write in dst.
 * @return length of compressed data in bytes.
 * @throws HipoException if maxSize < max # of compressed bytes or compression failed.
 */
int Compressor::compressLZ4(uint8_t* src, int srcOff, int srcSize,
                            uint8_t* dst, int dstOff, int maxSize) {

    if (LZ4_compressBound(srcSize) > maxSize) {
        throw HipoException("maxSize (" + to_string(maxSize) +
                            ") is < max # of compressed bytes (" +
                            to_string(LZ4_compressBound(srcSize)) + ")");
    }

    int size = LZ4_compress_fast((const char*)(src + srcOff), (char*)(dst + dstOff),
                                 srcSize, maxSize, lz4Acceleration);
    if (size < 1) {
        throw HipoException("compression failed");
    }

    return size;
}

/**
 * Fastest LZ4 compression. Returns length of compressed data in bytes.
 *
 * @param src      source of uncompressed data.
 * @param srcOff   start offset in src regardless of position.
 * @param srcSize  number of bytes to compress.
 * @param dst      destination array.
 * @param dstOff   start offset in dst regardless of position.
 * @param maxSize  maximum number of bytes to write in dst.
 * @return length of compressed data in bytes.
 * @throws HipoException if maxSize < max # of compressed bytes or compression failed.
 */
int Compressor::compressLZ4(ByteBuffer & src, int srcOff, int srcSize,
                            ByteBuffer & dst, int dstOff, int maxSize) {

    if (LZ4_compressBound(srcSize) > maxSize) {
        throw HipoException("maxSize (" + to_string(maxSize) +
                            ") is < max # of compressed bytes (" +
                            to_string(LZ4_compressBound(srcSize)) + ")");
    }

    int size = LZ4_compress_fast((const char*)(src.array() + srcOff),
                                 (char*)(dst.array() + dstOff),
                                 srcSize, maxSize, lz4Acceleration);
    if (size < 1) {
        throw HipoException("compression failed");
    }

    return size;
}

//---------------------------
// LZ4 Best Compression
//---------------------------

/**
 * Highest LZ4 compression. Returns length of compressed data in bytes.
 *
 * @param src      source of uncompressed data starting at position.
 * @param srcSize  number of bytes to compress.
 * @param dst      destination buffer starting at position.
 * @param maxSize  maximum number of bytes to write in dst.
 * @return length of compressed data in bytes.
 * @throws HipoException if maxSize < max # of compressed bytes or compression failed.
 */
int Compressor::compressLZ4Best(ByteBuffer & src, int srcSize, ByteBuffer & dst, int maxSize) {

    if (LZ4_compressBound(srcSize) > maxSize) {
        throw HipoException("maxSize (" + to_string(maxSize) +
                            ") is < max # of compressed bytes (" +
                            to_string(LZ4_compressBound(srcSize)) + ")");
    }

    int size = LZ4_compress_HC((const char*)(src.array() + src.position()),
                               (char*)(dst.array() + dst.position()),
                               srcSize, maxSize, 1);
    if (size < 1) {
        throw HipoException("compression failed");
    }

    return size;
}

/**
 * Highest LZ4 compression. Returns length of compressed data in bytes.
 *
 * @param src      source of uncompressed data.
 * @param srcOff   start offset in src.
 * @param srcSize  number of bytes to compress.
 * @param dst      destination array.
 * @param dstOff   start offset in dst.
 * @param maxSize  maximum number of bytes to write in dst.
 * @return length of compressed data in bytes.
 * @throws HipoException if maxSize < max # of compressed bytes or compression failed.
 */
int Compressor::compressLZ4Best(uint8_t src[], int srcOff, int srcSize,
                                uint8_t dst[], int dstOff, int maxSize) {
    if (LZ4_compressBound(srcSize) > maxSize) {
        throw HipoException("maxSize (" + to_string(maxSize) +
                            ") is < max # of compressed bytes (" +
                            to_string(LZ4_compressBound(srcSize)) + ")");
    }

    int size = LZ4_compress_HC((const char*)(src + srcOff), (char*)(dst + dstOff),
                               srcSize, maxSize, 1);
    if (size < 1) {
        throw HipoException("compression failed");
    }

    return size;
}

/**
 * Highest LZ4 compression. Returns length of compressed data in bytes.
 *
 * @param src      source of uncompressed data.
 * @param srcOff   start offset in src.
 * @param srcSize  number of bytes to compress.
 * @param dst      destination array.
 * @param dstOff   start offset in dst.
 * @param maxSize  maximum number of bytes to write in dst.
 * @return length of compressed data in bytes.
 * @throws HipoException if maxSize < max # of compressed bytes or compression failed.
 */
int Compressor::compressLZ4Best(ByteBuffer & src, int srcOff, int srcSize,
                                ByteBuffer & dst, int dstOff, int maxSize) {
    if (LZ4_compressBound(srcSize) > maxSize) {
        throw HipoException("maxSize (" + to_string(maxSize) +
                            ") is < max # of compressed bytes (" +
                            to_string(LZ4_compressBound(srcSize)) + ")");
    }

    int size = LZ4_compress_HC((const char*)(src.array() + srcOff),
                               (char*)(dst.array() + dstOff),
                               srcSize, maxSize, 1);
    if (size < 1) {
        throw HipoException("compression failed");
    }

    return size;
}

//---------------------------
// LZ4 Decompression
//---------------------------

/**
 * LZ4 decompression. Returns original length of decompressed data in bytes.
 *
 * @param src      source of compressed data.
 * @param srcSize  number of compressed bytes.
 * @param dst      destination array.
 * @return original (uncompressed) input size.
 * @throws HipoException if destination buffer is too small to hold uncompressed data or
 *                       source data is malformed.
 */
int Compressor::uncompressLZ4(ByteBuffer & src, int srcSize, ByteBuffer & dst) {
    return uncompressLZ4(src, src.position(), srcSize, dst);
}


/**
 * LZ4 decompression. Returns original length of decompressed data in bytes.
 *
 * @param src      source of compressed data.
 * @param srcOff   start offset in src.
 * @param srcSize  number of compressed bytes.
 * @param dst      destination array.
 * @return original (uncompressed) input size.
 * @throws HipoException if destination buffer is too small to hold uncompressed data or
 *                       source data is malformed.
 */
int Compressor::uncompressLZ4(ByteBuffer & src, int srcOff, int srcSize, ByteBuffer & dst) {

    int dstOff = dst.position();

    int size = LZ4_decompress_safe((const char*)(src.array() + srcOff),
                                   (char*)(dst.array() + dstOff),
                                   srcSize, dst.remaining());

    if (size < 0) {
        throw HipoException("destination buffer too small or data malformed");
    }

    // Prepare buffer for reading
    dst.limit(dstOff + size).position(dstOff);
    return size;
}



/**
 * LZ4 decompression. Returns original length of decompressed data in bytes.
 *
 * @param src      source of compressed data.
 * @param srcOff   start offset in src.
 * @param srcSize  number of compressed bytes.
 * @param dst      destination array.
 * @param dstOff   start offset in dst.
 * @return original (uncompressed) input size.
 * @throws HipoException if destination buffer is too small to hold uncompressed data or
 *                       source data is malformed.
 */
int Compressor::uncompressLZ4(ByteBuffer & src, int srcOff, int srcSize, ByteBuffer & dst, int dstOff) {

    int size = LZ4_decompress_safe((const char*)(src.array() + srcOff),
                                   (char*)(dst.array() + dstOff),
                                   srcSize, dst.remaining());

    if (size < 0) {
        throw HipoException("destination buffer too small or data malformed");
    }

    // Prepare buffer for reading
    dst.limit(dstOff + size).position(dstOff);
    return size;
}


/**
 * LZ4 decompression. Returns original length of decompressed data in bytes.
 *
 * @param src      source of compressed data.
 * @param srcOff   start offset in src.
 * @param srcSize  number of compressed bytes.
 * @param dst      destination array.
 * @param dstOff   start offset in dst.
 * @param dstCapacity size of destination buffer in bytes, which must be already allocated.
 * @return original (uncompressed) input size.
 * @throws HipoException if uncompressed data bytes &gt; dstCapacity or
 *                       source data is malformed.
 */
int Compressor::uncompressLZ4(uint8_t src[], int srcOff, int srcSize, uint8_t dst[],
                              int dstOff, int dstCapacity) {

    int size = LZ4_decompress_safe((const char*)(src + srcOff),
                                   (char*)(dst + dstOff),
                                   srcSize, dstCapacity);
    if (size < 0) {
        throw HipoException("destination buffer too small or data malformed");
    }

    return size;
}

}
