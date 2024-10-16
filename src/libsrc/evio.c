/*-----------------------------------------------------------------------------
 * Copyright (c) 1991,1992 Southeastern Universities Research Association,
 *                         Continuous Electron Beam Accelerator Facility
 *
 * This software was developed under a United States Government license
 * described in the NOTICE file included as part of this distribution.
 *
 * CEBAF Data Acquisition Group, 12000 Jefferson Ave., Newport News, VA 23606
 * Email: coda@cebaf.gov  Tel: (804) 249-7101  Fax: (804) 249-7363
 *-----------------------------------------------------------------------------
 * 
 * Description:
 *	Event I/O routines
 *	
 * Author:  Chip Watson, CEBAF Data Acquisition Group
 *
 * Modified: Stephen A. Wood, TJNAF Hall C
 *      Works on ALPHA 64 bit machines if _LP64 is defined
 *      Will read input from standard input if filename is "-"
 *      If input filename is "|command" will take data from standard output
 *                        of command.
 *      If input file is compressed and uncompressible with gunzip, it will
 *                        decompress the data on the fly.
 *
 * Modified: Carl Timmer, DAQ group, Jan 2012
 *      Get rid of gzip compression stuff
 *      Get rid of ALPHA stuff
 *      Everything is now _LP64
 *      Updates for evio version 4 include:
 *          1) Enable reading from / writing to buffers and sockets
 *          2) Same format for writing to file as for writing to buffer or over socket
 *          3) Allow seemless storing of dictionary as very first bank
 *          4) Keep information about padding for 8 byte int & short data.
 *          5) Each data block is variable size with an integral number of events
 *          6) Data block size is suggestable
 *          7) Added documentation - How about that!
 *          8) Move 2 routines from evio_util.c to here (evIsContainer, evGetTypename)
 *          9) Add read that allocates memory instead of using supplied buffer
 *         10) Add random access reads using memory mapped files
 *         11) Add routine for getting event pointers in random access mode
 *         12) Add no-copy reads for streams
 *         13) Add routine for finding how many bytes written to buffer
 *         14) Additional options for evIoctl including getting # of events in file/buffer, etc.
 *
 * Modified: Carl Timmer, DAQ group, Sep 2012
 *      Make threadsafe by using read/write lock when getting/closing handle
 *      and using mutex to prevent simultaneous reads/writes per handle.
 * 
 * Modified: Carl Timmer, DAQ group, Aug 2013
 *      Build in file splitting & auto-naming capabilities.
 *      Fix bug in mutex handling.
 *      Allow mutexes to be turned off.
 *      Allow setting buffer size.
 * 
 * Routines:
 * ---------
 * 
 * int  evOpen                 (char *filename, char *flags, int *handle)
 * int  evOpenBuffer           (char *buffer, int bufLen, char *flags, int *handle)
 * int  evOpenSocket           (int sockFd, char *flags, int *handle)
 * int  evRead                 (int handle, uint32_t *buffer, uint32_t buflen)
 * int  evReadAlloc            (int handle, uint32_t **buffer, uint32_t *buflen)
 * int  evReadNoCopy           (int handle, const uint32_t **buffer, uint32_t *buflen)
 * int  evReadRandom           (int handle, const uint32_t **pEvent, uint32_t *buflen, uint32_t eventNumber)
 * int  evWrite                (int handle, const uint32_t *buffer)
 * int  evIoctl                (int handle, char *request, void *argp)
 * int  evClose                (int handle)
 * int  evFlush                (int handle)
 * int  evGetBufferLength      (int handle, uint32_t *length)
 * int  evGetRandomAccessTable (int handle, const uint32_t *** const table, uint32_t *len)
 * int  evGetDictionary        (int handle, char **dictionary, uint32_t *len)
 * int  evWriteDictionary      (int handle, char *xmlDictionary)
 * int  evWriteFirstEvent      (int handle, const uint32_t *firstEvent)
 * int  evCreateFirstEventBlock(const uint32_t *firstEvent, int localEndian, void **block, uint32_t *words)
 * int  evStringsToBuf         (uint32_t *buffer, int bufLen, char **strings, int stringCount, int *dataLen)
 * int  evBufToStrings         (char *buffer, int bufLen, char ***pStrArray, int *strCount)
 * int  evIsContainer          (int type)
 * char *evPerror              (int error)
 * const char *evGetTypename   (int type)
 * int  evIsLastBLock          (uint32_t sixthWord)
 * void evPrintBuffer          (uint32_t *p, uint32_t len, int swap)
 */

/* This is necessary to use an error check version of the pthread mutex */
#ifndef __APPLE__
    /** Enable GNU extensions to C lib such as strdup, strndup, asprintf, pid_t. */
    #define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <pthread.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>
#include "evio.h"


/* A few items to make the code more readable */

/** Read from a file. */
#define EV_READFILE     0
/** Read from a pipe. */
#define EV_READPIPE     1
/** Read from a socket. */
#define EV_READSOCK     2
/** Read from a buffer. */
#define EV_READBUF      3

/** Write to a file. */
#define EV_WRITEFILE    4
/** Write to a pipe. */
#define EV_WRITEPIPE    5
/** Write to a socket. */
#define EV_WRITESOCK    6
/** Write to a buffer. */
#define EV_WRITEBUF     7


/** Number used to determine data endian */
#define EV_MAGIC 0xc0da0100

/** Version 3's fixed block size in 32 bit words */
#define EV_BLOCKSIZE_V3 8192

/** Version 4&6's target block size in 32 bit words (16MB).
 * It is a soft limit since a single event larger than
 * this limit may need to be written. */
//#define EV_BLOCKSIZE 4000000
#define EV_BLOCKSIZE 150

/** Minimum block size in 32 bit words allowed if size reset (4MB) */
#define EV_BLOCKSIZE_MIN 1000000
/**
 * The upper limit of maximum size for a single block used for writing
 * is 2^25 ints or words. This gives block sizes of about 134MB.
 * It is a soft limit since a single event larger than this limit
 * may need to be written.
 */
#define EV_BLOCKSIZE_MAX 33554432

/** In version 4 & 6, lowest 8 bits are version, rest is bit info */
#define EV_VERSION_MASK 0xFF

/** In version 4 & 6, dictionary presence is 9th bit in version/info word */
#define EV_DICTIONARY_MASK 0x100

/** In version 4 & 6, "last block" is 10th bit in version/info word. */
#define EV_LASTBLOCK_MASK 0x200

/** In version 4 & 6, "first event" is 15th bit in version/info word */
#define EV_FIRSTEVENT_MASK 0x4000

/** In version 6, number of bits to shift compression word right to get type of compression. */
#define EV_COMPRESSED_SHIFT 28
/** In version 6, # of bits in record's compression word that specifies type of compression after shift right. */
#define EV_COMPRESSED_MASK 0xf

/** In versions 4 & 6, upper limit on maximum max number of events per block */
#define EV_EVENTS_MAX 100000

/** In versions 4 & 6, default max number of events per block */
#define EV_EVENTS_MAX_DEF 10000

/** In versions 4 & 6, if splitting file, default split size in bytes (2GB) */
#define EV_SPLIT_SIZE 2000000000L

/** In versions 1-3, default size for a single file read in bytes.
 *  Equivalent to 500, 32,768 byte blocks.
 *  This constant <b>MUST BE</b> an integer multiple of 32768.*/
#define EV_READ_BYTES_V3 16384000
/* #define EV_READ_BYTES_V3 (32768) */

/** In version 6, the file header's file type. */
#define EV_FILE_TYPE 0x4556494F


/**
 * @file
 * <pre><code>
 * Let's take a look at an evio block header also
 * known as a physical record header.
 * 
 * In versions 1, 2 & 3, evio files impose an anachronistic
 * block structure. The complication that arises is that logical records
 * (events) will sometimes cross physical record boundaries.
 *
 * ####################################
 * Evio block header, versions 1,2 & 3:
 * ####################################
 *
 * MSB(31)                          LSB(0)
 * <---  32 bits ------------------------>
 * +-------------------------------------+
 * +             Block Size              +
 * +-------------------------------------+
 * +            Block Number             +
 * +-------------------------------------+
 * +           Header Size = 8           +
 * +-------------------------------------+
 * +               Start                 +
 * +-------------------------------------+
 * +                Used                 +
 * +-------------------------------------+
 * +              Version                +
 * +-------------------------------------+
 * +              Reserved               +
 * +-------------------------------------+
 * +              Magic #                +
 * +-------------------------------------+
 *
 *
 *      Block Size    = number of 32 bit ints in block (including this one).
 *                      This is fixed for versions 1-3, generally at 8192 (32768 bytes)
 *      Block Number  = id number (starting at 1)
 *      Header Size   = number of 32 bit nts in this header (always 8)
 *      Start         = offset to first event header in block relative to start of block
 *      Used          = # of used/valid words (header + data) in block,
 *                      (normally = block size, but in last block may be smaller)
 *      Version       = evio format version
 *      Reserved      = reserved
 *      Magic #       = magic number (0xc0da0100) used to check endianness
 *
 *
 * 
 * In version 4, only an integral number of complete
 * events will be contained in a single block.
 *
 * ################################
 * Evio block header, version 4:
 * ################################
 *
 * MSB(31)                          LSB(0)
 * <---  32 bits ------------------------>
 * +-------------------------------------+
 * +             Block Size              +
 * +-------------------------------------+
 * +            Block Number             +
 * +-------------------------------------+
 * +          Header Length = 8          +
 * +-------------------------------------+
 * +             Event Count             +
 * +-------------------------------------+
 * +              Reserved               +
 * +-------------------------------------+
 * +          Bit info         + Version +
 * +-------------------------------------+
 * +              Reserved               +
 * +-------------------------------------+
 * +            Magic Number             +
 * +-------------------------------------+
 *
 *
 *      Block Size         = number of ints in block (including this one).
 *      Block Number       = id number (starting at 1)
 *      Header Length      = number of ints in this header (EV_HDSIZ which is currently 8)
 *      Event Count        = number of events in this block (always an integral #).
 *                           NOTE: this value should not be used to parse the following
 *                           events since the first block may have a dictionary whose
 *                           presence is not included in this count.
 *      Bit info & Version = Lowest 8 bits are the version number (4).
 *                           Upper 24 bits contain bit info.
 *                           If a dictionary is included as the first event, bit #9 is set (=1)
 *      Magic #            = magic number (0xc0da0100) used to check endianness
 *
 *
 *      Bit info (24 bits) has the following bits defined (starting at 1):
 * 
 *         Bit  9     = true if dictionary is included (relevant for first block only)
 *         Bit  10    = true if this block is the last block in file or network transmission
 *         Bits 11-14 = type of events following (ROC Raw = 0, Physics = 1, PartialPhysics = 2,
 *                      DisentangledPhysics = 3, User = 4, Control = 5, Prestart = 6, Go = 7,
 *                      Pause = 8, End = 9, Other = 15)
 *         Bit  15    = true if block contains "first" event which gets written in each file split
 *
 *         Bits 11-15 are ONLY for the CODA online use of evio.
 *         That's because only a single CODA event TYPE is placed into
 *         a single ET or cMsg buffer. Each user or control event has its own
 *         buffer. Thus all events parsed from a single buffer will be of a single CODA type.
 *
 * ################################
 * COMPOSITE DATA:
 * ################################
 *   This is a new type of data (value = 0xf) which originated with Hall B.
 *   It is a composite type and allows for possible expansion in the future
 *   if there is a demand. Basically it allows the user to specify a custom
 *   format by means of a string - stored in a tagsegment. The data in that
 *   format follows in a bank. The routine to swap this data must be provided
 *   by the definer of the composite type - in this case Hall B. The swapping
 *   function is plugged into this evio library's swapping routine.
 *   Here's what it looks like.
 *
 * MSB(31)                          LSB(0)
 * <---  32 bits ------------------------>
 * +---------+------+--------------------+
 * +  tag    + type +    length          + --> tagsegment header
 * +---------+------+--------------------+
 * +        Data Format String           +
 * +                                     +
 * +-------------------------------------+
 * +              length                 + \
 * +----------------+---------+----------+  \  bank header
 * +       tag      +  type   +   num    +  /
 * +----------------+---------+----------+ /
 * +               Data                  +
 * +                                     +
 * +-------------------------------------+
 *
 *   The beginning tagsegment is a normal evio tagsegment containing a string
 *   (type = 0x3). Currently its type and tag are not used - at least not for
 *   data formatting.
 *   The bank is a normal evio bank header with data following.
 *   The format string is used to read/write this data so that takes care of any
 *   padding that may exist. As with the tagsegment, the tags and type are ignored.
 *
 * ########################################
 * Evio block or record header, version 6+:
 * ########################################
 *
 *  GENERAL RECORD HEADER STRUCTURE ( 56 bytes, 14 integers (32 bit) )
 *
 *    +----------------------------------+
 *  1 +         Record Length            + // 32bit words, inclusive
 *    +----------------------------------+
 *  2 +         Record Number            +
 *    +----------------------------------+
 *  3 +         Header Length            + // 14 (words)
 *    +----------------------------------+
 *  4 +       Event (Index) Count        +
 *    +----------------------------------+
 *  5 +      Index Array Length          + // bytes
 *    +-----------------------+----------+
 *  6 +       Bit Info        + Version  + // version (8 bits)
 *    +-----------------------+----------+
 *  7 +      User Header Length          + // bytes
 *    +----------------------------------+
 *  8 +          Magic Number            + // 0xc0da0100
 *    +----------------------------------+
 *  9 +     Uncompressed Data Length     + // bytes
 *    +------+---------------------------+
 * 10 +  CT  +  Data Length Compressed   + // CT = compression type (4 bits); compressed len in words
 *    +------+---------------------------+
 * 11 +          User Register 1         + // UID 1st (64 bits)
 *    +--                              --+
 * 12 +                                  +
 *    +----------------------------------+
 * 13 +          User Register 2         + // UID 2nd (64 bits)
 *    +--                              --+
 * 14 +                                  +
 *    +----------------------------------+
 *
 * -------------------
 *   Compression Type
 * -------------------
 *     0  = none
 *     1  = LZ4 fastest
 *     2  = LZ4 best
 *     3  = gzip
 *
 * -------------------
 *   Bit Info Word
 * -------------------
 *     0-7  = version
 *     8    = true if dictionary is included (relevant for first record only)
 *     9    = true if this record has "first" event (to be in every split file)
 *    10    = true if this record is the last in file or stream
 *    11-14 = type of events contained: 0 = ROC Raw,
 *                                      1 = Physics
 *                                      2 = PartialPhysics
 *                                      3 = DisentangledPhysics
 *                                      4 = User
 *                                      5 = Control
 *                                     15 = Other
 *    15-19 = reserved
 *    20-21 = pad 1
 *    22-23 = pad 2
 *    24-25 = pad 3
 *    26-27 = reserved
 *    28-31 = general header type: 0 = Evio record,
 *                                 3 = Evio file trailer
 *                                 4 = HIPO record,
 *                                 7 = HIPO file trailer
 *
 * ------------------------------------------------------------
 * ------------------------------------------------------------
 *
 *   TRAILER HEADER STRUCTURE ( 56 bytes, 14 integers (32 bit) )
 *
 *    +----------------------------------+
 *  1 +         Record Length            + // 32bit words, inclusive
 *    +----------------------------------+
 *  2 +         Record Number            +
 *    +----------------------------------+
 *  3 +               14                 +
 *    +----------------------------------+
 *  4 +                0                 +
 *    +----------------------------------+
 *  5 +      Index Array Length          + // bytes
 *    +-----------------------+----------+
 *  6 +       Bit Info        + Version  +
 *    +-----------------------+----------+
 *  7 +                0                 +
 *    +----------------------------------+
 *  8 +           0xc0da0100             +
 *    +----------------------------------+
 *  9 +     Uncompressed Data Length     + // bytes
 *    +----------------------------------+
 * 10 +                0                 +
 *    +----------------------------------+
 * 11 +                0                 +
 *    +--                              --+
 * 12 +                0                 +
 *    +----------------------------------+
 * 13 +                0                 +
 *    +--                              --+
 * 14 +                0                 +
 *    +----------------------------------+
 *
 * ----------------------------------
 *   Bit Info Word (bit num = value)
 * ----------------------------------
 *     0-7  = 6
 *     8    = 0
 *     9    = 0
 *    10    = 1
 *    11-14 = 0
 *    15-19 = 0
 *    20-21 = 0
 *    22-23 = 0
 *    24-25 = 0
 *    26-27 = 0
 *    28-31 = 3
 *
 *
 *         THE FULL TRAILER FORMAT IS:
 *
 *    +----------------------------------+
 *    +         Trailer Header           +
 *    +          (14 words)              +
 *    +----------------------------------+
 *
 *    +----------------------------------+
 *    +            Optional              +
 *    +      Uncompressed Array of       +
 *    +     a record length in bytes,    +
 *    +           followed by            +
 *    +  an event count for that record  +
 *    +       (2 words / record)         +
 *    +          (all records)           +
 *    +----------------------------------+
 *
 *   HOWEVER, in this library, the optional index of lengths and counts is NOT written.
 *
 * ------------------------------------------------------------
 * ------------------------------------------------------------
 *
 *         THE FULL RECORD FORMAT IS:
 *
 *    +----------------------------------+
 *    +         Record Header            +
 *    +          (14 words)              +
 *    +----------------------------------+
 *
 *    +----------------------------------+
 *    +           Index Array            +
 *    +     (required index of all       +
 *    +      event lengths in bytes,     +
 *    +       one word / length )        +
 *    +----------------------------------+
 *
 *    +----------------------------------+
 *    +          User Header             +
 *    +    (any user data)    +----------+
 *    +                       +  Pad 1   +
 *    +-----------------------+----------+
 *
 *    +----------------------------------+
 *    +             Events               +
 *    +                       +----------+
 *    +                       +  Pad 2   +
 *    +-----------------------+----------+
 *
 *
 * Records may be compressed, but that is only handled in the Java and C++ libs.
 * The record header is never compressed and so is always readable.
 * If events are in the evio format, pad_2 will be 0.
 *
 *
 * ################################
 * Evio FILE header, version 6+:
 * ################################
 *
 * FILE HEADER STRUCTURE ( 56 bytes, 14 integers (32 bit) )
 *
 *    +----------------------------------+
 *  1 +              ID                  + // HIPO: 0x43455248, Evio: 0x4556494F
 *    +----------------------------------+
 *  2 +          File Number             + // split file #
 *    +----------------------------------+
 *  3 +         Header Length            + // 14 (words)
 *    +----------------------------------+
 *  4 +      Record (Index) Count        +
 *    +----------------------------------+
 *  5 +      Index Array Length          + // bytes
 *    +-----------------------+----------+
 *  6 +       Bit Info        + Version  + // version (8 bits)
 *    +-----------------------+----------+
 *  7 +      User Header Length          + // bytes
 *    +----------------------------------+
 *  8 +          Magic Number            + // 0xc0da0100
 *    +----------------------------------+
 *  9 +          User Register           +
 *    +--                              --+
 * 10 +                                  +
 *    +----------------------------------+
 * 11 +         Trailer Position         + // File offset to trailer head (64 bits).
 *    +--                              --+ // 0 = no offset available or no trailer exists.
 * 12 +                                  +
 *    +----------------------------------+
 * 13 +          User Integer 1          +
 *    +----------------------------------+
 * 14 +          User Integer 2          +
 *    +----------------------------------+
 *
 * -------------------
 *   Bit Info Word
 * -------------------
 *     0-7  = version
 *     8    = true if dictionary is included (relevant for first record only)
 *     9    = true if this file has "first" event (in every split file)
 *    10    = File trailer with index array of record lengths exists
 *    11-19 = reserved
 *    20-21 = pad 1
 *    22-23 = pad 2
 *    24-25 = pad 3 (always 0)
 *    26-27 = reserved
 *    28-31 = general header type: 1 = Evio file
 *                                 2 = Evio extended file
 *                                 5 = HIPO file
 *                                 6 = HIPO extended file
 *
 * In this library, the Trailer Position is never written and therefore is always 0.
 * It's unneeded since the trailer's index is never written.
 *
 * ---------------------------------------------------------------
 * ---------------------------------------------------------------
 *
 * The file header occurs once at the beginning of the file.
 * The full file format looks like:
 *
 *         THE FULL FILE FORMAT IS:
 *
 *    +----------------------------------+
 *    +          File Header             +
 *    +          (14 words)              +
 *    +----------------------------------+
 *
 *    +----------------------------------+
 *    +           Index Array            +
 *    +   (optional index, same format   +
 *    +      as file trailer index:      +
 *    +   1 word of record len in bytes, +
 *    +           followed by            +
 *    +      1 word of event count       +
 *    +----------------------------------+
 *
 *    +----------------------------------+
 *    +          User Header             +
 *    +    (any user data)    +----------+
 *    +                       +  Pad 1   +
 *    +-----------------------+----------+
 *
 *    +----------------------------------+
 *    +             Record 1             +
 *    +----------------------------------+
 *                   ___
 *    +----------------------------------+
 *    +             Record N             +
 *    +----------------------------------+
 *
 *    The last record may be a trailer.
 *
 * </code></pre>
 */

#define EV_HD_BLKSIZ 0	/**< Position of blk hdr word for size of block in 32-bit words (v 1-6). */
#define EV_HD_BLKNUM 1	/**< Position of blk hdr word for block number, starting at 0 (v 1-6). */
#define EV_HD_HDSIZ  2	/**< Position of blk hdr word for size of header in 32-bit words (v 1-4, =8) (v 6 =14). */
#define EV_HD_COUNT  3  /**< Position of blk hdr word for number of events in block (version 4,6). */
#define EV_HD_START  3  /**< Position of blk hdr word for first start of event in this block (ver 1-3). */
#define EV_HD_USED   4	/**< Position of blk hdr word for number of words used in block (<= BLKSIZ) (ver 1-3). */
#define EV_HD_RESVD1 4  /**< Position of blk hdr word for reserved (v 1-4). */
#define EV_HD_VER    5	/**< Position of blk hdr word for version of file format (+ bit info in ver 4,6). */
#define EV_HD_RESVD2 6	/**< Position of blk hdr word for reserved (v 1-4). */
#define EV_HD_MAGIC  7	/**< Position of blk hdr word for magic number for endianness tracking (v 4,6). */

#define EV_HD_INDEXARRAYLEN  4   /**< Position of file & blk hdr word for index array length (v 6). */
#define EV_HD_USERHDRLEN     6   /**< Position of file & blk hdr word for user header length (v 6). */
#define EV_HD_UNCOMPDATALEN  8   /**< Position of blk hdr word for uncompressed data length (v 6). */
#define EV_HD_COMPDATALEN    9   /**< Position of blk hdr word for compressed data length (v 6). */
#define EV_HD_TRAILERPOS     10  /**< Position of file hdr word for trailer position (v 6). */
#define EV_HD_USERREG1       10  /**< Position of blk hdr word for user register 1 (v 6). */
#define EV_HD_USERREG2       12  /**< Position of blk hdr word for user register 2 (v 6). */
#define EV_HD_USERREGFILE    8   /**< Position of file hdr word for user register (v 6). */


/** Turn on 9th bit to indicate dictionary included in block */
#define setDictionaryBit(a)     ((a)[EV_HD_VER] |= EV_DICTIONARY_MASK)
/** Turn off 9th bit to indicate dictionary included in block */
#define clearDictionaryBit(a)   ((a)[EV_HD_VER] &= ~EV_DICTIONARY_MASK)
/** Is there a dictionary in this block? */
#define hasDictionary(a)        (((a)[EV_HD_VER] & EV_DICTIONARY_MASK) > 0 ? 1 : 0)
/** Is there a dictionary in this block? */
#define hasDictionaryInt(i)     ((i & EV_DICTIONARY_MASK) > 0 ? 1 : 0)

/** Turn on bit to indicate last block of file/transmission */
#define setLastBlockBit(a)       ((a)[EV_HD_VER] |= EV_LASTBLOCK_MASK)

/** Turn off bit to indicate last block of file/transmission */
#define clearLastBlockBit(a)     ((a)[EV_HD_VER] &= ~EV_LASTBLOCK_MASK)

/** Turn off bit to indicate last block of file/transmission */
#define clearLastBlockBitInt(i)    (i &= ~EV_LASTBLOCK_MASK)

/** Is this the last block of file/transmission? */
#define isLastBlock(a)          (((a)[EV_HD_VER] & EV_LASTBLOCK_MASK) > 0 ? 1 : 0)

/** Is this the last block of file/transmission? */
#define isLastBlockInt(i)       ((i & EV_LASTBLOCK_MASK) > 0 ? 1 : 0)

/** Is the record data compressed (version 6, 10th header word)? */
#define isCompressed(i) (((i >> 28) && 0xf) == 0 ? 0 : 1)

/** Get padding #1 from bitinfo word (v6). */
#define getPad1(i) (((int)i >> 20) & 0x3)
/** Get padding #2 from bitinfo word (v6). */
#define getPad2(i) (((int)i >> 22) & 0x3)
/** Get padding #3 from bitinfo word (v6). */
#define getPad3(i) (((int)i >> 24) & 0x3)

///** Initialize a record header */
#define initBlockHeader(a) { \
    (a)[EV_HD_BLKSIZ]        = EV_HDSIZ_V6; \
    (a)[EV_HD_BLKNUM]        = 1; \
    (a)[EV_HD_HDSIZ]         = EV_HDSIZ_V6; \
    (a)[EV_HD_COUNT]         = 0; \
    (a)[EV_HD_INDEXARRAYLEN] = 0; \
    (a)[EV_HD_VER]           = EV_VERSION; \
    (a)[EV_HD_USERHDRLEN]    = 0; \
    (a)[EV_HD_MAGIC]         = EV_MAGIC; \
    (a)[EV_HD_UNCOMPDATALEN] = 0; \
    (a)[EV_HD_COMPDATALEN]   = 0; \
    (a)[EV_HD_USERREG1]      = 0; \
    (a)[EV_HD_USERREG1 + 1]  = 0; \
    (a)[EV_HD_USERREG2]      = 0; \
    (a)[EV_HD_USERREG2 + 1]  = 0; \
}

/** Initialize a record header */
#define initBlockHeader2(a,b) { \
    (a)[EV_HD_BLKSIZ]        = EV_HDSIZ_V6; \
    (a)[EV_HD_BLKNUM]        = b; \
    (a)[EV_HD_HDSIZ]         = EV_HDSIZ_V6; \
    (a)[EV_HD_COUNT]         = 0; \
    (a)[EV_HD_INDEXARRAYLEN] = 0; \
    (a)[EV_HD_VER]           = EV_VERSION; \
    (a)[EV_HD_USERHDRLEN]    = 0; \
    (a)[EV_HD_MAGIC]         = EV_MAGIC; \
    (a)[EV_HD_UNCOMPDATALEN] = 0; \
    (a)[EV_HD_COMPDATALEN]   = 0; \
    (a)[EV_HD_USERREG1]      = 0; \
    (a)[EV_HD_USERREG1 + 1]  = 0; \
    (a)[EV_HD_USERREG2]      = 0; \
    (a)[EV_HD_USERREG2 + 1]  = 0; \
}

/** Initialize a file header, with split# = 1, record count = 0 (4th word). */
#define initFileHeader(a) { \
    (a)[0]   = EV_FILE_TYPE; \
    (a)[1]   = 1; \
    (a)[2]   = EV_HDSIZ_V6; \
    (a)[3]   = 0; \
    (a)[4]   = 0; \
    (a)[5]   = (0x10000000 | EV_VERSION); \
    (a)[6]   = 0; \
    (a)[7]   = EV_MAGIC; \
    (a)[8]   = 0; \
    (a)[9]   = 0; \
    (a)[10]  = 0; \
    (a)[11]  = 0; \
    (a)[12]  = 0; \
    (a)[13]  = 0; \
}

/* Prototypes for static routines */
static  int      fileExists(char *filename);
static  int      evOpenImpl(char *srcDest, uint32_t bufLen, int sockFd, char *flags, int *handle);
static  int      evGetNewBuffer(EVFILE *a);
static  char *   evTrim(char *s, int skip);
static  int      tcpWrite(int fd, const void *vptr, int n);
static  int      tcpRead(int fd, void *vptr, int n);
static  int      evReadAllocImpl(EVFILE *a, uint32_t **buffer, uint32_t *buflen);
static  void     localClose(EVFILE *a);
static  int      getEventCount(EVFILE *a, uint32_t *count);
static  int      evWriteImpl(int handle, const uint32_t *buffer, int useMutex);
static  int      flushToDestination(EVFILE *a, int force, int *wroteData);

static  int      splitFile(EVFILE *a);
static  int      generateSixthWord(int version, int hasDictionary, int isEnd, int eventType);
static  void     resetBuffer(EVFILE *a, int beforeDictionary);
static  int      expandBuffer(EVFILE *a, uint32_t newSize);
static  int      writeEventToBufferV6(EVFILE *a, const uint32_t *buffer, uint32_t wordsToWrite);
static  int      writeEventToBuffer(EVFILE *a, const uint32_t *buffer,
                                    uint32_t wordsToWrite, int isDictionary);
//static int       writeNewHeaderV6(EVFILE *a,
//                                  uint32_t eventCount, uint32_t blockNumber,
//                                  int hasDictionary, int isLast);
static  int      writeNewHeader(EVFILE *a,
                                uint32_t eventCount, uint32_t blockNumber,
                                int hasDictionary, int isLast);
static int       evGetNewBufferFileV3(EVFILE *a);
static int       evReadAllocImplFileV3(EVFILE *a, uint32_t **buffer, uint32_t *buflen);
static int       evReadFileV3(EVFILE *a, uint32_t *buffer, uint32_t buflen);
static void      resetBufferV6(EVFILE *a);


/* Dealing with EVFILE struct */
static void      handleLock(int handle);
static void      handleUnlock(int handle);
static void      handleUnlockUnconditional(int handle);
static void      getHandleLock(void);
static void      getHandleUnlock(void);

/* Append Mode */
static  int      toAppendPosition(EVFILE *a);

/* Random Access Mode */
static  int      memoryMapFile(EVFILE *a, const char *fileName);
static  int      generatePointerTable(EVFILE *a);
static  int      generatePointerTableV6(EVFILE *a);

/** Array that holds all pointers to structures created with evOpen().
 * Space in the array is allocated as needed, beginning with 100
 * and adding 50% every time more are needed. */
EVFILE **handleList = NULL;
/** The number of handles available for use. */
static size_t handleCount = 0;

/** Pthread mutex for serializing calls to get and free handles. */
#ifdef __APPLE__
static pthread_mutex_t getHandleMutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER;
#else
static pthread_mutex_t getHandleMutex = PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;
#endif

/** Array of pthread lock pointers for preventing simultaneous calls
 * to evClose, read/write routines, etc. Need 1 for each evOpen() call. */
static pthread_mutex_t **handleLocks = NULL;

/*-----------------*
 *     FORTRAN     *
 *-----------------*/

/**
 * @defgroup fortran FORTRAN routines
 * These routines handle limited evio operations for FORTRAN programs.
 * @{
 */

/** Fortran interface to {@link #evOpen}. */
#ifdef AbsoftUNIXFortran
int evopen
#else
int evopen_
#endif
        (char *filename, char *flags, int *handle, int fnlen, int flen)
{
    char *fn, *fl;
    int status;
    fn = (char *) malloc((size_t )fnlen+1);
    strncpy(fn,filename, (size_t )fnlen);
    fn[fnlen] = 0;      /* insure filename is null terminated */
    fl = (char *) malloc((size_t )flen+1);
    strncpy(fl,flags,(size_t )flen);
    fl[flen] = 0;           /* insure flags is null terminated */
    status = evOpen(fn,fl,handle);
    free(fn);
    free(fl);
    return(status);
}


/** Fortran interface to {@link #evRead}. */
#ifdef AbsoftUNIXFortran
int evread
#else
int evread_
#endif
        (int *handle, uint32_t *buffer, uint32_t *buflen)
{
    return(evRead(*handle, buffer, *buflen));
}


/** Fortran interface to {@link #evWrite}. */
#ifdef AbsoftUNIXFortran
int evwrite
#else
int evwrite_
#endif
        (int *handle, const uint32_t *buffer)
{
    return(evWrite(*handle, buffer));
}


/** Fortran interface to {@link #evClose}. */
#ifdef AbsoftUNIXFortran
int evclose
#else
int evclose_
#endif
        (int *handle)
{
    return(evClose(*handle));
}


/** Fortran interface to {@link #evIoctl}. */
#ifdef AbsoftUNIXFortran
int evioctl
#else
int evioctl_
#endif
        (int *handle, char *request, void *argp, int reqlen)
{
    char *req;
    int32_t status;
    req = (char *)malloc((size_t)reqlen+1);
    strncpy(req,request,(size_t)reqlen);
    req[reqlen]=0;		/* insure request is null terminated */
    status = evIoctl(*handle,req,argp);
    free(req);
    return(status);
}

/** @} */

/*-----------------------------------------------------------------------------*/
/*-----------------------------------------------------------------------------*/

/**
 * Routine to take the 6th word of a block header and tell whether
 * it's the last block or not.
 *
 * @param sixthWord  6th word of an evio block header
 * @return 1 of this is the last block, else 0
 */
int evIsLastBlock(uint32_t sixthWord) {
    return (sixthWord & EV_LASTBLOCK_MASK) > 0 ? 1 : 0;
}


/**
 * Routine to print the contents of a buffer.
 * 
 * @param p pointer to buffer
 * @param len number of 32-bit words to print out
 * @param swap swap if true
 */
void evPrintBuffer(uint32_t *p, uint32_t len, int swap) {
    uint32_t i, width=5;
    printf("\nBUFFER:\n");
    for (i=0; i < len; i++) {
        if (i%width == 0) printf("%3u   ", i);
        if (swap) {
            uint32_t p_tmp = (uint32_t) EVIO_SWAP32(*p);
            printf("0x%08x  ", p_tmp);
            p++;
        }
        else {
            printf("0x%08x  ", *(p++));
        }
        if ((i+1)%width == 0) printf("\n");
    }
    printf("\n");
}


/**
 * Routine to intialize an EVFILE structure for writing.
 * If reading, relevant stuff gets overwritten anyway.
 * 
 * @param a   pointer to structure being inititalized
 */
void evFileStructInit(EVFILE *a) {
    a->file         = NULL;
    a->handle       = 0;
    a->rw           = 0;
    a->magic        = EV_MAGIC;
    a->bigEndian    = evioIsLocalHostBigEndian();
    a->byte_swapped = 0;
    a->version      = EV_VERSION;
    a->append       = 0;
    a->eventCount   = 0;

    /* block stuff */
    a->buf           = NULL;
    a->pBuf          = NULL;
    a->next          = NULL;
    /* Space in number of words, not in header, left for writing */
    if (EV_VERSION >= 6) {
        a->left = EV_BLOCKSIZE - EV_HDSIZ_V6;
    }
    else {
        a->left = EV_BLOCKSIZE - EV_HDSIZ;
    }
    a->blocksToParse = 0; /* for reading version 1-3 files */
    /* Total data written = block header size so far */
    if (EV_VERSION >= 6) {
        a->blksiz = EV_HDSIZ_V6;
    }
    else {
        a->blksiz = EV_HDSIZ;
    }
    a->blknum        = 1;
    a->blkNumDiff    = 0;
    /* Target block size (final size may be larger or smaller) */
    a->blkSizeTarget = EV_BLOCKSIZE;
    /* Start with this size block buffer */
    a->bufSize       = EV_BLOCKSIZE;
    a->bufRealSize   = EV_BLOCKSIZE;
    /* Max # of events/block */
    a->blkEvMax      = EV_EVENTS_MAX_DEF;
    a->blkEvCount    = 0;
    a->isLastBlock   = 0;

    /* file stuff */
    a->baseFileName   = NULL;
    a->fileName       = NULL;
    a->runType        = NULL;
    a->runNumber      = 1;
    a->specifierCount = 0;
    a->splitting      = 0;
    a->lastEmptyBlockHeaderExists = 0;
    a->streamCount    = 1;
    a->streamId       = 0;
    a->splitNumber    = 0;
    a->split          = EV_SPLIT_SIZE;
    a->fileSize       = 0L;
    a->bytesToFile    = 0L;
    if (EV_VERSION > 4) {
        a->bytesToBuf = EV_HDSIZ_BYTES_V6; /* start off with 1 block header */
    }
    else {
        a->bytesToBuf = EV_HDSIZ_BYTES; /* start off with 1 block header */
    }
    a->eventsToBuf    = 0;
    a->eventsToFile   = 0;
    a->currentHeader  = NULL;

    a->fileSize       = 0L;
    a->filePosition   = 0L;

    /* buffer stuff */
    a->rwBuf        = NULL;
    a->rwBufSize    = 0;
    /* Total data read/written so far */
    a->rwBytesOut   = 0;
    a->rwBytesIn    = 0;
    a->rwFirstWrite = 1;

    /* socket stuff */
    a->sockFd = 0;

    /* randomAcess stuff */
    a->randomAccess = 0;
    a->mmapFileSize = 0;
    a->mmapFile = NULL;
    a->pTable   = NULL;

    /* dictionary */
    a->hasAppendDictionary = 0;
    a->wroteDictionary = 0;
    a->dictLength = 0;
    a->dictBuf    = NULL;
    a->dictionary = NULL;

    /* first event */
    a->firstEventLength = 0;
    a->firstEventBuf    = NULL;

    /* common block */
    a->commonBlkCount   = 0;

    /* sync */
    a->lockingOn = 1;

    /* version 6 */
    a->fileIndexArrayLen = 0;
    a->fileUserHeaderLen = 0;

    a->curRecordIndexArrayLen = 0;
    a->curRecordUserHeaderLen = 0;

    a->trailerPosition = 0;

    a->eventLengths = NULL;
    a->eventLengthsLen = 0;
    a->dataBuf = NULL;
    a->dataNext = NULL;
    a->dataLeft = EV_BLOCKSIZE;
    a->bytesToDataBuf = 0;
}


/**
 * Routine to free an EVFILE structure.
 * @param a pointer to structure being freed.
 */
static void freeEVFILE(EVFILE *a) {

    if (a->buf != NULL && a->rw != EV_WRITEBUF) {
        if (a->pBuf != NULL) {
            free((void *)(a->pBuf));
        }
        else {
            free((void *) (a->buf));
        }
    }

    if (a->pTable        != NULL) free((void *)(a->pTable));
    if (a->dictBuf       != NULL) free((void *)(a->dictBuf));
    if (a->dictionary    != NULL) free((void *)(a->dictionary));
    if (a->fileName      != NULL) free((void *)(a->fileName));
    if (a->baseFileName  != NULL) free((void *)(a->baseFileName));
    if (a->runType       != NULL) free((void *)(a->runType));
    if (a->eventLengths  != NULL) free((void *)(a->eventLengths));
    if (a->dataBuf       != NULL) free((void *)(a->dataBuf));

    free((void *)a);
}


/**
 * Routine to lock the pthread mutex used for acquiring and releasing handles
 * and used only in evOpenImpl and evClose.
 */
static void getHandleLock(void) {
    int status;
    status = pthread_mutex_lock(&getHandleMutex);
    if (status != 0) {
        evio_err_abort(status, "Failed get handle lock");
    }
}


/**
 * Routine to unlock the pthread mutex used for acquiring and releasing handles
 * and used only in evOpenImpl and evClose.
 */
static void getHandleUnlock(void) {
    int status;
    status = pthread_mutex_unlock(&getHandleMutex);
    if (status != 0) {
        evio_err_abort(status, "Failed get handle unlock");
    }
}


/** Routine to grab read lock used to prevent simultaneous
 *  calls to evClose and read/write routines. */
static void handleLock(int handle) {
    EVFILE *a = handleList[handle-1];
    pthread_mutex_t *lock;
    int status;

    if (a == NULL || !a->lockingOn) return;

    lock = handleLocks[handle-1];
    status = pthread_mutex_lock(lock);
    if (status != 0) {
        evio_err_abort(status, "Failed handle lock");
    }
}


/** Routine to release read lock used to prevent simultaneous
 *  calls to evClose and read/write routines. */
static void handleUnlock(int handle) {
    EVFILE *a = handleList[handle-1];
    pthread_mutex_t *lock;
    int status;

    if (a == NULL || !a->lockingOn) return;

    lock = handleLocks[handle-1];
    status = pthread_mutex_unlock(lock);
    if (status != 0) {
        evio_err_abort(status, "Failed handle unlock");
    }
}

/** Routine to release read lock used to prevent simultaneous
 *  calls to evClose and read/write routines. Called only by evClose
 *  when handleList array member has already been cleared. */
static void handleUnlockUnconditional(int handle) {
    pthread_mutex_t *lock = handleLocks[handle-1];
    int status = pthread_mutex_unlock(lock);
    if (status != 0) {
        evio_err_abort(status, "Failed handle unlock");
    }
}


/**
 * Routine to expand existing storage space for EVFILE structures
 * (one for each evOpen() call).
 * 
 * @return S_SUCCESS if success
 * @return S_EVFILE_ALLOCFAIL if memory cannot be allocated
 */
static int expandHandles() {
    /* If this is the first initialization, add 100 places for 100 evOpen()'s */
    if (handleCount < 1 || handleList == NULL) {
        size_t i;
        
        handleCount = 100;

        handleList = (EVFILE **) calloc(handleCount, sizeof(EVFILE *));
        if (handleList == NULL) {
            return S_EVFILE_ALLOCFAIL;
        }

        handleLocks = (pthread_mutex_t **) calloc(handleCount, sizeof(pthread_mutex_t *));
        if (handleLocks == NULL) {
            return S_EVFILE_ALLOCFAIL;
        }
        /* Initialize new array of mutexes */
        for (i=0; i < handleCount; i++) {
            pthread_mutex_t *plock = (pthread_mutex_t *) calloc(1, sizeof(pthread_mutex_t));
            pthread_mutex_init(plock, NULL);
            handleLocks[i] = plock;
        }
    }
    /* We're expanding the exiting arrays */
    else {
        /* Create new, 50% larger arrays */
        size_t i;
        size_t newCount = handleCount * 3 / 2;

        EVFILE **newHandleList;
        pthread_mutex_t **newHandleLocks;

        newHandleLocks = (pthread_mutex_t **) calloc(newCount, sizeof(pthread_mutex_t *));
        if (newHandleLocks == NULL) {
            return S_EVFILE_ALLOCFAIL;
        }

        newHandleList = (EVFILE **) calloc(newCount, sizeof(EVFILE *));
        if (newHandleList == NULL) {
            return S_EVFILE_ALLOCFAIL;
        }

        /* Copy old into new */
        for (i = 0; i < handleCount; i++) {
            newHandleList[i]  = handleList[i];
            newHandleLocks[i] = handleLocks[i];
        }

        /* Initialize the rest */
        for (i = handleCount; i < newCount; i++) {
            pthread_mutex_t *plock = (pthread_mutex_t *) calloc(1, sizeof(pthread_mutex_t));
            pthread_mutex_init(plock, NULL);
            newHandleLocks[i] = plock;
        }
        
        /* Free the unused arrays */
        free((void *)handleLocks);
        free((void *)handleList);

        /* Update variables */
        handleCount = newCount;
        handleList  = newHandleList;
        handleLocks = newHandleLocks;
    }

    return S_SUCCESS;
}



/**
 * Routine to write a specified number of bytes to a TCP socket.
 *
 * @param fd   socket's file descriptor
 * @param vptr pointer to buffer from which bytes are supplied
 * @param n    number of bytes to write
 *
 * @return number of bytes written if successful
 * @return -1 if error and errno is set
 */
static int tcpWrite(int fd, const void *vptr, int n)
{
    int         nleft;
    int         nwritten;
    const char  *ptr;

    
    ptr = (char *) vptr;
    nleft = n;
  
    while (nleft > 0) {
        if ( (nwritten = (int) write(fd, (char*)ptr, nleft)) <= 0) {
            if (errno == EINTR) {
                nwritten = 0;       /* and call write() again */
            }
            else {
                return(nwritten);   /* error */
            }
        }

        nleft -= nwritten;
        ptr   += nwritten;
    }
    return(n);
}


/**
 * Routine to read a specified number of bytes from a TCP socket.
 * If no error, this routine could block until the full number of
 * bytes is read;
 * 
 * @param fd   socket's file descriptor
 * @param vptr pointer to buffer into which bytes go
 * @param n    number of bytes to read
 * 
 * @return number of bytes read if successful
 * @return -1 if error and errno is set
 */
static int tcpRead(int fd, void *vptr, int n)
{
    int   nleft;
    int   nread;
    char  *ptr;

    
    ptr = (char *) vptr;
    nleft = n;
  
    while (nleft > 0) {
        if ( (nread = (int) read(fd, ptr, nleft)) < 0) {
            /*
            if (errno == EINTR)            fprintf(stderr, "call interrupted\n");
            else if (errno == EAGAIN)      fprintf(stderr, "non-blocking return, or socket timeout\n");
            else if (errno == EWOULDBLOCK) fprintf(stderr, "nonblocking return\n");
            else if (errno == EIO)         fprintf(stderr, "I/O error\n");
            else if (errno == EISDIR)      fprintf(stderr, "fd refers to directory\n");
            else if (errno == EBADF)       fprintf(stderr, "fd not valid or not open for reading\n");
            else if (errno == EINVAL)      fprintf(stderr, "fd not suitable for reading\n");
            else if (errno == EFAULT)      fprintf(stderr, "buffer is outside address space\n");
            else {perror("cMsgTcpRead");}
            */
            if (errno == EINTR) {
                nread = 0;      /* and call read() again */
            }
            else {
                return(nread);
            }
        }
        else if (nread == 0) {
            break;            /* EOF */
        }
    
        nleft -= nread;
        ptr   += nread;
    }
    return(n - nleft);        /* return >= 0 */
}


/**
 * This routine trims white space and nonprintable characters
 * from front and back of the given string. Resulting string is
 * placed at the beginning of the argument. If resulting string
 * is of no length or error, return NULL.
 *
 * @param s    NULL-terminated string to be trimmed
 * @param skip number of bytes (or ASCII characters) to skip over or
 *             remove from string's front end before beginning the trimming
 * @return     argument pointer (s) if successful
 * @return     NULL if no meaningful string can be returned
 */
static char *evTrim(char *s, int skip) {
    size_t i;
    size_t len, frontCount=0;
    char *firstChar, *lastChar;

    
    if (s == NULL) return NULL;
    if (skip < 0) skip = 0;

    len = strlen(s + skip);

    if (len < 1) return NULL;
    
    firstChar = s + skip;
    lastChar  = s + skip + len - 1;

    /* Find first front end nonwhite, printable char */
    while (*firstChar != '\0' && (isspace(*firstChar) || !isprint(*firstChar))) {
        firstChar++;
        /* Check to see if string all white space or nonprinting */
        if (firstChar > lastChar) {
            return NULL;
        }
    }
    frontCount = firstChar - s;

    /* Find first back end nonwhite, printable char */
    while (isspace(*lastChar) || !isprint(*lastChar)) {
        lastChar--;
    }

    /* Number of nonwhite chars */
    len = lastChar - firstChar + 1;

    /* Move chars to front of string */
    for (i=0; i < len; i++) {
        s[i] = s[frontCount + i];
    }
    /* Add null at end */
    s[len] = '\0';

    return s;
}


/**
 * Does the file exist or not?
 * @param filename the file's name
 * @return 1 if it exists, else 0
 */
static int fileExists(char *filename) {
    FILE *fp = fopen(filename,"r");
    if (fp) {
        /* exists */
        fclose(fp);
        return 1;
    }
    return 0;
}


/**
 * This routine substitutes a given string for a specified substring.
 * Free the result if non-NULL.
 *
 * @param  orig    string to modify
 * @param  replace substring in orig arg to replace
 * @param  with    string to substitute for replace arg
 * @return resulting string which is NULL if there is a problem
 *         and needs to be freed if not NULL.
 */
char *evStrReplace(char *orig, const char *replace, const char *with) {
    
    char *result;  /* return string */
    char *ins;     /* next insert point */
    char *tmp;
    
    size_t len_rep;   /* length of rep  */
    size_t len_with;  /* length of with */
    size_t len_front; /* distance between rep and end of last rep */
    int count;        /* number of replacements */

    
    /* Check string we're changing */
    if (!orig) return NULL;
    ins = orig;
  
    /* What are we replacing? */
    if (!replace) replace = "";
    len_rep = strlen(replace);
    
    /* What are we replacing it with? */
    if (!with) with = "";
    len_with = strlen(with);

    for (count = 0; (tmp = strstr(ins, replace)); ++count) {
        ins = tmp + len_rep;
    }

    /* First time through the loop, all the variables are set correctly.
     * From here on,
     *    tmp  points to the end of the result string
     *    ins  points to the next occurrence of rep in orig
     *    orig points to the remainder of orig after "end of rep" */
    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result) return NULL;

    while (count--) {
        ins = strstr(orig, replace);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; /* move to next "end of rep" */
    }
    
    strcpy(tmp, orig);
    return result;
}


/**
 * This routine finds constructs of the form $(ENV) and replaces it with
 * the value of the ENV environmental variable if it exists or with nothing
 * if it doesn't. Simple enough to do without regular expressions.
 * Free the result if non-NULL.
 *
 * @param orig string to modify
 * @return resulting string which is NULL if there is a problem
 *         and needs to be freed if not NULL.
 */
char *evStrReplaceEnvVar(const char *orig) {
    
    char *start;   /* place of $( */
    char *end;     /* place of  ) */
    char *env;     /* value of env variable */
    char *envVar;  /* env variable name */
    char *result, *tmp;
    char replace[256]; /* string that needs replacing */
    
    size_t len;   /* length of env variable name  */

    
    /* Check string we're changing */
    if (!orig) return NULL;
    
    /* Copy it */
    result = strdup(orig);
    if (!result) return NULL;

    /* Look for all "$(" occurrences */
    while ( (start = strstr(result, "$(")) ) {
        /* Is there an ending ")" ? */
        end = strstr(start, ")");
     
        /* If so ... */
        if (end) {
            /* Length of the env variable name */
            len = end - (start + 2);
            
            /* Memory for name */
            envVar = (char *)calloc(1, len+1);
            if (!envVar) {
                free(result);
                return NULL;
            }
            
            /* Create env variable name */
            strncpy(envVar, start+2, len);
            
            /* Look up value of env variable (NULL if none) */
            env = getenv(envVar);

/*printf("found env variable = %s, value = %s\n", envVar, env);*/
  
            /* Create string to be replaced */
            memset(replace, 0, 256);
            sprintf(replace, "%s%s%s", "$(", envVar, ")");

            /* Do a straight substitution */
            tmp = evStrReplace(result, replace, env);
            free(result);
            free(envVar);
            result = tmp;
        }
        else {
            /* No substitutions need to be made since no closing ")" */
            break;
        }
    }
  
    return result;
}


/**
 * This routine checks a string for C-style printing integer format specifiers.
 * More specifically, it checks for %nd and %nx where n can be multiple digits.
 * It makes sure that there is at least one digit between % and x/d and that the
 * first digit is a "0" so that generated filenames contain no white space.
 * It returns the modified string or NULL if error. Free the result if non-NULL.
 * It also returns the number of valid specifiers found in the orig argument.
 *
 * @param orig           string to be checked/modified
 * @param specifierCount pointer to int which gets filled with the
 *                       number of valid format specifiers in the orig arg
 *
 * @return resulting string which is NULL if there is a problem
 *         and needs to be freed if not NULL.
 */
char *evStrFindSpecifiers(const char *orig, int *specifierCount) {
    
    char *start;            /* place of % */
    char digits[20];        /* digits of format specifier after % */
    char oldSpecifier[25];  /* format specifier as it appears in orig */
    char newSpecifier[25];  /* format specifier desired */
    char c, *result;
    const int debug=0;
    int count=0;            /* number of valid specifiers in orig */
    int digitCount;         /* number of digits in a specifier between % and x/d */
    

    /* Check args */
    if (!orig || specifierCount == NULL) return NULL;
    
    /* Duplicate orig for convenience */
    result = strdup(orig);
    if (!result) return NULL;
    start = result;
        
    /* Look for C integer printing specifiers (starting with % and using x & d) */
    while ( (start = strstr(start, "%")) ) {
        
        c = *(++start);
        memset(digits, 0, 20);
        digits[0] = c;
        if (debug) printf("Found %%, first char = %c\n", c);
        
        /* Read all digits between the % and the first non-digit char */
        digitCount = 0;
        while (isdigit(c)) {
            digits[digitCount++] = c;
            c = *(++start);
            if (debug) printf("         next char = %c\n", c);
        }
        
        /* What is the ending char? */

        /* - skip over any %s specifiers */
        if (c == 's' && digitCount == 0) {
            start++;
            if (debug) printf("         skip over %%s\n");
            continue;
        }

        /* - any thing besides x & d are forbidden */
        if (c != 'x' && c != 'd') {
            /* This is not an acceptable C integer format specifier and
             * may generate a seg fault later, so just return an error. */
            if (debug) printf("         bad ending char = %c, return\n", c);
            free(result);
            return NULL;            
        }
        
        /* Store the x or d */
        digits[digitCount] = c;
        start++;
        
        /* If we're here, we have a valid specifier */
        count++;
        
        /* Is there a "0" as the first digit between the % and the x/d?
         * If not, make it so to avoid white space in generated file names. */
        if (debug) printf("         digit count = %d, first digit = %c\n", digitCount, digits[0]);
        if (digitCount < 1 || digits[0] != '0') {
            /* Recreate original specifier */
            memset(oldSpecifier, 0, 25);
            sprintf(oldSpecifier, "%%%s", digits);
           
            /* Create replacement specifier */
            memset(newSpecifier, 0, 25);
            sprintf(newSpecifier, "%%%c%s", '0', digits);
            if (debug) printf("         old specifier = %s, new specifier = %s\n",
                               oldSpecifier, newSpecifier);
       
            /* Replace old with new & start over */
            start = evStrReplace(result, oldSpecifier, newSpecifier);
            free(result);
            result = start;
            count = 0;
        }
    }
    
    *specifierCount = count;
    
    return result;    
}


/**
 * This routine checks a string for C-style printing integer format specifiers.
 * More specifically, it checks for %nd and %nx where n can be multiple digits.
 * It removes all such specifiers and returns the modified string or NULL
 * if error. Skips over given # of int specifiers. Free the result if non-NULL.
 *
 * @param orig string to be checked/modified
 * @param skip # of int specifiers to skip
 *
 * @return resulting string which is NULL if there is a problem
 *         and needs to be freed if not NULL.
 */
char *evStrRemoveSpecifiers(const char *orig, int skip) {
    
    char *pStart;           /* pointer to start of string */
    char *pChar;            /* pointer to char somewhere in string */
    char c, *result, *pSpec;
    int i, startIndex, endIndex;
    const int debug=0;
    int specLen;           /* number of chars in valid specifier in orig */
    int digitCount;        /* number of digits in a specifier between % and x/d */

    /** How many int specifiers skipped so far. */
    int skipCount = 0;

    /* Check args */
    if (!orig) return NULL;
    
    /* Duplicate orig for convenience */
    pChar = pStart = result = strdup(orig);
    if (!result) return NULL;

    /* Look for C integer printing specifiers (starting with % and using x & d) */
    while ( (pChar = strstr(pChar, "%")) ) {
        /* remember where specifier starts */
        pSpec = pChar;
        
        c = *(++pChar);
        if (debug) printf("evStrRemoveSpecifiers found %%, first char = %c\n", c);
        
        /* Read all digits between the % and the first non-digit char */
        digitCount = 0;
        while (isdigit(c)) {
            digitCount++;
            c = *(++pChar);
            if (debug) printf("         next char = %c\n", c);
        }
        
        /* What is the ending char? */

        /* - skip over any %s specifiers */
        if (c == 's' && digitCount == 0) {
            pChar++;
            if (debug) printf("         skip over %%s\n");
            continue;
        }

        /* - any thing besides x & d are forbidden */
        if (c != 'x' && c != 'd') {
            /* This is not an acceptable C integer format specifier and
            * may generate a seg fault later, so just return an error. */
            if (debug) printf("         bad ending char = %c, return\n", c);
            free(result);
            return NULL;
        }
        
        pChar++;

        if (skipCount++ < skip) {
            /* Skip over this specifier */
            continue;
        }

        /* # of chars in specifier */
        specLen = (int) (pChar - pSpec);
        if (debug) printf("         spec len = %d\n", specLen);

        /* shift chars to eliminate specifier */
        startIndex = (int) (pSpec-pStart);
        endIndex = (int) strlen(result) + 1 - specLen - startIndex; /* include NULL in move */
        for (i=0; i < endIndex; i++) {
            result[i+startIndex] = result[i+startIndex+specLen];
        }
        if (debug) printf("         resulting string = %s\n", result);
    }
    
    return result;
}



/**
 * <p>This routine generates a (base) file name from a name containing format specifiers
 * and enviromental variables.</p>
 *
 * <p>The file name may contain characters of the form <b>\$(ENV_VAR)</b>
 * which will be substituted with the value of the associated environmental
 * variable or a blank string if none is found.</p>
 *
 * <p>The given name may contain up to 3, C-style integer format specifiers
 * (such as <b>%03d</b>, or <b>%x</b>). If more than 3 are found, an error is returned.
 * If no "0" precedes any integer between the "%" and the "d" or "x" of the format specifier,
 * it will be added automatically in order to avoid spaces in the final, generated
 * file name.</p>
 *
 * See the doc for {@link #evGenerateFileName}.
 * The details of what is substituted for the int specifiers is given there.<p>
 *
 *
 * @param origName  file name to modify
 * @param baseName  pointer which gets filled with resulting file name
 * @param count     pointer to int filled with number of format specifiers found
 *
 * @return S_SUCCESS          if successful
 * @return S_FAILURE          if bad format specifiers or more that 3 specifiers found
 * @return S_EVFILE_BADARG    if args are null or origName is invalid
 * @return S_EVFILE_ALLOCFAIL if out-of-memory
 */
int evGenerateBaseFileName(const char *origName, char **baseName, int *count) {

    char *name, *tmp;
    int   specifierCount=0;

    
    /* Check args */
    if (count == NULL    || baseName == NULL ||
        origName == NULL || strlen(origName) < 1) {
        return(S_EVFILE_BADARG);
    }

    /* Scan for environmental variables of the form $(env)
     * and substitute the values for them (blank string if not found) */
    tmp = evStrReplaceEnvVar(origName);
    if (tmp == NULL) {
        /* out-of-mem */
        return(S_EVFILE_ALLOCFAIL);
    }

    /* Check/fix C-style int specifiers in baseFileName.
     * How many specifiers are there? */
    name = evStrFindSpecifiers(tmp, &specifierCount);
    if (name == NULL) {
        free(tmp);
        /* out-of-mem or bad specifier */ 
        return(S_EVFILE_ALLOCFAIL);
    }
    free(tmp);

    if (specifierCount > 3) {
        return(S_FAILURE);
    }

    /* Return the base file name */
    *baseName = name;

    /* Return the number of C-style int format specifiers */
    *count = specifierCount;
    
    return(S_SUCCESS);
}


/**
 * <p>This method generates a complete file name from the previously determined baseFileName
 * obtained from calling {@link #evGenerateBaseFileName(char *, char **, int *)} and
 * stored in the evOpen handle.</p>
 *
 * <p>All occurrences of the string "%s" in the baseFileName will be substituted with the
 * value of the runType arg or nothing if the runType is null.</p>
 * 
 * <p>If evio data is to be split up into multiple files (split > 0), numbers are used to
 * distinguish between the split files with splitNumber.
 * If baseFileName contains C-style int format specifiers (specifierCount > 0), then
 * the first occurrence will be substituted with the given runNumber value.
 * If the file is being split, the second will be substituted with the splitNumber.
 * If 2 specifiers exist and the file is not being split, no substitutions are made.
 * If no specifier for the splitNumber exists, it is tacked onto the end of the file name.
 * It returns the final file name or NULL if error. Free the result if non-NULL.</p>
 *
 * <p>If multiple streams of data, each writing a file, end up with the same file name,
 * they can be differentiated by a stream id number. If the id is > 0, the string, ".strm"
 * is appended to the very end of the file followed by the id number (e.g. filename.strm1).
 * This is done after the run type, run number, split numbers, and env vars have been inserted
 * into the file name.</p>
 *
 * @param a              evio handle (contains file name to use as a basis for the
 *                       generated file name)
 * @param specifierCount number of C-style int format specifiers in file name arg
 * @param runNumber      CODA run number
 * @param splitting      is file being split (split > 0)? 1 - yes, 0 - no
 * @param splitNumber    number of the split file
 * @param runType        run type name
 * @param streamId       streamId number (100 > id > -1)
 *
 * @return NULL if error
 * @return generated file name (free if non-NULL)
 */
char *evGenerateFileNameOld(EVFILE *a, int specifierCount, int runNumber,
                           int splitting, int splitNumber, char *runType,
                           uint32_t streamId) {

    char   *fileName, *name, *specifier;

    
    /* Check args */
    if ( (splitting && splitNumber < 0) || (runNumber < 1) ||
         (streamId > 99) ||
         (specifierCount < 0) || (specifierCount > 2)) {
        return(NULL);
    }

    /* Check handle arg */
    if (a == NULL) {
        return(NULL);
    }

    /* Need to be writing to a file */
    if (a->rw != EV_WRITEFILE) {
        return(NULL);
    }

    /* Internal programming bug */
    if (a->baseFileName == NULL) {
        return(NULL);
    }
    
    /* Replace all %s occurrences with run type ("" if NULL).
     * This needs to be done before the run # & split # substitutions. */
    name = evStrReplace(a->baseFileName, "%s", runType);
    if (name == NULL) {
        /* out-of-mem */
        return(NULL);
    }
    free(a->baseFileName);
    a->baseFileName = name;

   /* As far as memory goes, allow 10 digits for the run number and 10 for the split.
     * That will cover 32 bit ints. */

    /* If we're splitting files ... */
    if (splitting) {
        
        /* For no specifiers: tack split # on end of base file name */
        if (specifierCount < 1) {
            /* Create space for filename */
            fileName = (char *)calloc(1, strlen(a->baseFileName) + 12);
            if (!fileName) return(NULL);
            
            /* Create a printing format specifier for split # */
            specifier = (char *)calloc(1, strlen(a->baseFileName) + 6);
            if (!specifier) {
                free(fileName);
                return(NULL);
            }
            
            sprintf(specifier, "%s.%s", a->baseFileName, "%d");

            /* Create the filename */
            sprintf(fileName, specifier, splitNumber);
            free(specifier);
        }
        
        /* For 1 specifier: insert run # at specified location, then tack split # on end */
        else if (specifierCount == 1) {
            /* Create space for filename */
            fileName = (char *)calloc(1, strlen(a->baseFileName) + 22);
            if (!fileName) return(NULL);
            
            /* The first printfing format specifier is in a->filename already */
            
            /* Create a printing format specifier for split # */
            specifier = (char *)calloc(1, strlen(a->baseFileName) + 6);
            if (!specifier) {
                free(fileName);
                return(NULL);
            }

            sprintf(specifier, "%s.%s", a->baseFileName, "%d");

            /* Create the filename */
            sprintf(fileName, specifier, runNumber, splitNumber);
            free(specifier);
        }
        
        /* For 2 specifiers: insert run # and split # at specified locations */
        else {
            /* Create space for filename */
            fileName = (char *)calloc(1, strlen(a->baseFileName) + 21);
            if (!fileName) return(NULL);
            
            /* Both printfing format specifiers are in a->filename already */
                        
            /* Create the filename */
            sprintf(fileName, a->baseFileName, runNumber, splitNumber);
        }
    }
    /* If we're not splitting files ... */
    else {
        /* Still insert runNumber if requested */
        if (specifierCount == 1) {
            /* Create space for filename */
            fileName = (char *)calloc(1, strlen(a->baseFileName) + 11);
            if (!fileName) return(NULL);
            
            /* The printfing format specifier is in a->filename already */
                        
            /* Create the filename */
            sprintf(fileName, a->baseFileName, runNumber);
        }
        /* For 2 specifiers: insert run # and remove split # specifier */
        else if (specifierCount == 2) {
            fileName = (char *)calloc(1, strlen(a->baseFileName) + 11);
            if (!fileName) return(NULL);
            sprintf(fileName, a->baseFileName, runNumber);

            /* Get rid of remaining int specifiers */
            name = evStrRemoveSpecifiers(fileName, 0);
            free(fileName);
            fileName = name;
        }
        else {
            fileName = strdup(a->baseFileName);
        }
    }

    /* If we have a valid stream id number, append ".strm#" to end of file name */
    if (streamId > 0) {
        char *fName;
        size_t idDigits, len = strlen(fileName);

        if (streamId < 10) {
            idDigits = 1 + 5;
        }
        else {
            idDigits = 2 + 5;
        }

        /* Create space for filename */
        fName = (char *)calloc(1, len + 1 + idDigits);
        if (fName == NULL) {
            free(fileName);
            return(NULL);
        }

        sprintf(fName, "%s.strm%u", fileName, streamId);

        free(fileName);
        fileName = fName;
    }

    return fileName;
}


/**
 * Inserts a string just before the first occurrence of a format specifier that starts with '%' and
 * ends with a conversion specifier of 'd' or 'x'.
 * The existing format specifier can include flags made of ints indicating width (ie %03x).
 * This function partly courtesy of ChatGPT. Remember, when this is called, the substitution for
 * %s with runType will have already been made.
 *
 * @param str string to scan
 * @param n number of "%"s to skip over before making the insert
 * @param insert string to insert before the int specifier
 * @param result resulting string
 * @param result_size size of result string
 *
 * @return 1 if specifier successfully inserted, 0 if nothing done, -1 if result is too small.
 *           If 1 is not returned, result is not written into.
 */
int evStrInsertBeforeSpecifier(char *str, uint32_t n, const char *insert, char *result, size_t result_size) {

    int count = 0;    // Counter for % specifiers
    int debug = 0;

    char *start = str; // Pointer to traverse the string
    char *end = NULL;
    size_t len_before, len_insert, len_after;

    // Step 1: Find the starting position of the format specifier (%)
    // Traverse the string using strchr to find the nth %
    while ((start = strchr(start, '%')) != NULL) {

        if (count++ == n) {
            // Step 2: Traverse to find the end position of the specifier
            end = start + 1;  // Start checking right after '%'

            // Loop through valid format specifier characters until we find a conversion character
            while (*end && isdigit(*end)) {
                end++;  // Continue parsing flags, width, and precision
                if (debug) printf("evStrInsertBeforeSpecifier: skipping over %c\n", *end);
            }

            // Check if the current character is a valid format specifier ending (d, x, f, etc.)
            if (*end == 'd' || *end == 'x') {
                if (debug) printf("evStrInsertBeforeSpecifier: found d or x\n");
                // Step 3: Insert the given string just before the format specifier
                len_before = start - str;               // Length of the string before the specifier
                len_insert = strlen(insert);            // Length of the string to insert
                len_after = strlen(start);              // Length of the remaining string after the prefix

                // Check if the result buffer is large enough
                if (len_before + len_insert + len_after >= result_size) {
                    //fprintf(stderr, "Error: result buffer too small!\n");
                    return -1;
                }

                // Construct the new string in the result buffer
                strncpy(result, str, len_before);                        // Copy the prefix part
                strncpy(result + len_before, insert, len_insert);        // Insert the given string
                strncpy(result + len_before + len_insert, start,
                        len_after);  // Copy the suffix (format specifier included)
                result[len_before + len_insert + len_after] = '\0';      // Null-terminate the result
                return 1;
            }
        }

        start++;  // Move past the current % to search for the next
    }
    return 0;
}


/**
 * <p>This method generates a complete file name from the previously determined baseFileName
 * obtained from calling {@link #evGenerateBaseFileName(char *, char **, int *)} and
 * stored in the evOpen handle.</p>
 *
 * <p>All occurrences of the string "%s" in the baseFileName will be substituted with the
 * value of the runType arg or nothing if the runType is null.</p>
 *
 * <p>The given fileName may contain up to 3, C-style int format specifiers which will be substituted
 * with runNumber, splitNumber and streamId in the manner described below.</p>
 *
 * <ul>
 *     <li>If file is to be split:</li>
 *          <ul>
 *          <li>If no specifiers:</li>
 *              <ul>
 *                  <li>for one stream, splitNumber is tacked onto the end of the file name as <b>.&lt;splitNumber&gt;</b></li>
 *                  <li>for multiple streams, streamId and splitNumber are tacked onto the end of the file name
 *                      as <b>.&lt;streamId&gt;.&lt;splitNumber&gt;</b></li>
 *                  <li>No run numbers are ever tacked on without a specifier </li>
 *              </ul>
 *          <li>If 1 specifier:</li>
 *              <ul>
 *                  <li>add runNumber according to specifier</li>
 *                  <li>for one stream, splitNumber is tacked onto the end of the file name as <b>.&lt;splitNumber&gt;</b></li>
 *                  <li>for multiple streams, streamId and splitNumber are tacked onto the end of the file name
 *                      as <b>.&lt;streamId&gt;.&lt;splitNumber&gt;</b></li>
 *              </ul>
 *          <li>If 2 specifiers:</li>
 *              <ul>
 *                  <li>add runNumber according to first specifier</li>
 *                  <li>for one stream, add splitNumber according to second specifier</li>
 *                  <li>for multiple streams, add splitNumber according to second specifier, but place
 *                      <b>&lt;streamId&gt;.</b> just before the splitNumber</li>
 *              </ul>
 *          <li>If 3 specifiers:</li>
 *              <ul>
 *                  <li>add runNumber according to first specifier, streamId according to second specifier,
 *                  and splitNumber according to third specifier</li>
 *              </ul>
 *          </ul>
 *
 *      <li>If file is NOT split:</li>
 *          <ul>
 *          <li>If no specifiers:</li>
 *              <ul>
 *                  <li>streamId is tacked onto the end of the file name as <b>.&lt;streamId&gt;</b></li>
 *                  <li>No run numbers are ever tacked on without a specifier.</li>
 *              </ul>
 *          <li>If 1 specifier:</li>
 *              <ul>
 *                  <li>add runNumber according to specifier</li>
 *                  <li>for multiple streams, streamId is tacked onto the end of the file name as .<b>.&lt;streamId&gt;</b></li>
 *              </ul>
 *          <li>If 2 specifiers:</li>
 *              <ul>
 *                  <li>add runNumber according to first specifier</li>
 *                  <li>remove second specifier</li>
 *                  <li>for multiple streams, streamId is tacked onto the end of the file name as <b>.&lt;streamId&gt;</b></li>
 *              </ul>
 *          <li>If 3 specifiers:</li>
 *              <ul>
 *                  <li>add runNumber according to first specifier</li>
 *                  <li>add streamId according to second specifier</li>
 *                  <li>remove third specifier</li>
 *              </ul>
 *          </ul>
 *  </ul>
 *
 * If there are more than 3 specifiers, <b>NO SUBSTITUTIONS ARE DONE on the extra specifiers</b>.
 *
 * @param a              evio handle (contains file name to use as a basis for the
 *                       generated file name)
 * @param specifierCount number of C-style int format specifiers in file name arg
 * @param runNumber      CODA run number
 * @param splitting      is file being split (split > 0)? 1 - yes, 0 - no
 * @param splitNumber    number of the split file
 * @param runType        run type name
 * @param streamId       streamId number (100 > id > -1)
 * @param streamCount    total number of streams in DAQ (100 > id > 0)
 * @param debug          if = 0, no debug output
 *
 * @return NULL if error
 * @return generated file name (free if non-NULL)
 */
char *evGenerateFileName(EVFILE *a, int specifierCount, uint32_t runNumber,
                         int splitting, uint32_t splitNumber, char *runType,
                         uint32_t streamId, uint32_t streamCount, int debug) {

    char   *fileName = NULL, *name;


    /* Check args */
    if ( (runNumber < 1) ||
         (streamId > 99) || (streamCount > 99) || (streamCount < 1) ||
         (specifierCount < 0) || (specifierCount > 3)) {
        return(NULL);
    }

    int oneStream = streamCount < 2;

    /* Check handle arg */
    if (a == NULL) {
        return(NULL);
    }

    /* Need to be writing to a file */
    if (a->rw != EV_WRITEFILE) {
        return(NULL);
    }

    /* Internal programming bug */
    if (a->baseFileName == NULL) {
        return(NULL);
    }

    /* Replace all %s occurrences with run type ("" if NULL).
     * This needs to be done before the run # & split # substitutions. */
    name = evStrReplace(a->baseFileName, "%s", runType);
    if (name == NULL) {
        /* out-of-mem */
        return(NULL);
    }
    free(a->baseFileName);
    a->baseFileName = name;

    /* As far as memory goes, allow 10 digits for the run number, 10 for the split,
     * and 10 for the stream id. That will cover 32 bit ints. However, to play it
     * safe, allocate much more. */

    size_t memSize = strlen(a->baseFileName) + 256;

    /* If we're splitting files ... */
    if (splitting) {

        /* For no specifiers: tack split # on end of base file name */
        if (specifierCount < 1) {
            /* Create space for filename + "." + stream id + "." + split# */
            fileName = (char *) calloc(1, memSize);
            if (!fileName) return (NULL);

            if (oneStream) {
                snprintf(fileName, memSize, "%s.%d", a->baseFileName, splitNumber);
                if (debug) printf("Split, 0 spec, 1 stream: fileName = %s, split# = %d\n",
                                  fileName, splitNumber);
            }
            else {
                snprintf(fileName, memSize, "%s.%u.%d", a->baseFileName, streamId, splitNumber);
                if (debug) printf("Split, 0 spec, multistream: fileName = %s, streamId = %u, split# = %d\n",
                                  fileName, streamId, splitNumber);
            }
        }

        /* For 1 specifier: insert run # at specified location,
         * then tack stream id and split # onto end of file name. */
        else if (specifierCount == 1) {
            /* Create space for filename + run# + "." + stream id + "." + split# */
            fileName = (char *) calloc(1, memSize);
            if (!fileName) return (NULL);

            if (oneStream) {
                /* Create 1 string with all print specifiers. */
                char temp[memSize];
                snprintf(temp, memSize, "%s.%s", a->baseFileName, "%d");

                snprintf(fileName, memSize, temp, runNumber, splitNumber);
                if (debug) printf("Split, 1 spec, 1 stream: fileName = %s, run# = %d, split# = %d\n",
                                  fileName, runNumber, splitNumber);
            }
            else {
                char temp[memSize];
                snprintf(temp, memSize, "%s.%s", a->baseFileName, "%u.%d");

                snprintf(fileName, memSize, temp, runNumber, streamId, splitNumber);
                if (debug) printf("Split, 1 spec, multistream: fileName = %s, run# = %d, streamId = %u, split# = %d\n",
                                  fileName, runNumber, streamId, splitNumber);
            }
        }

        /* For 2 specifiers: insert run # and split # at specified locations
         * and place stream id immediately before split #. */
        else if (specifierCount == 2) {
            fileName = (char *)calloc(1, memSize);
            if (!fileName) return(NULL);

            if (!oneStream) {
                /* Insert "%u." before the 2nd specifier */
                char result[memSize];
                int err = evStrInsertBeforeSpecifier(a->baseFileName, 1, "%u.", result, memSize);
                if (err == 1) {
                    /* Place streamId and split# into specifiers in baseFileName. */
                    snprintf(fileName, memSize, result, runNumber, streamId, splitNumber);
                    if (debug) printf("Split, 2 spec, multistream: fileName = %s, run# = %d, streamId = %u, split# = %d\n",
                                      fileName, runNumber, streamId, splitNumber);
                }
                else {
                    // error
                    if (debug) printf("Error in evStrInsertBeforeSpecifier\n");
                    return NULL;
                }
            }
            else {
                /* Place run# & split# into specifiers in baseFileName. */
                snprintf(fileName, memSize, a->baseFileName, runNumber, splitNumber);
                if (debug) printf("Split, 2 spec, 1 stream: fileName = %s, run# = %d, split# = %d\n",
                                  fileName, runNumber, splitNumber);
            }
        }

        /* For 3 specifiers: insert run #, stream id, and split # at specified locations */
        else if (specifierCount == 3) {
            fileName = (char *)calloc(1, memSize);
            if (!fileName) return(NULL);
            snprintf(fileName, memSize, a->baseFileName, runNumber, (int)streamId, splitNumber);
            if (debug) printf("Split, 3 spec: fileName = %s, run# = %d, streamId = %u, split# = %d\n",
                              fileName, runNumber, streamId, splitNumber);
        }
    }

    /** If we're not splitting files, then CODA isn't being used and stream id is
     * probably meaningless. */
    else {
        /* For no specifiers:  tack stream id onto end of file name */
        if (specifierCount < 1) {
            fileName = (char *) calloc(1, memSize);
            if (!fileName) return (NULL);

            if (!oneStream) {
                snprintf(fileName, memSize, "%s.%u", a->baseFileName, streamId);
                if (debug) printf("No-split, 0 spec, multistream: fileName = %s, streamId = %u\n",
                                  fileName, streamId);
            }
            else {
                strncpy(fileName, a->baseFileName, strlen(a->baseFileName));
                if (debug) printf("No-split, 0 spec, 1 stream: fileName = %s\n", fileName);
            }
        }
        /* Still insert runNumber if requested */
        if (specifierCount == 1) {
            char temp[memSize];
            snprintf(temp, memSize, a->baseFileName, runNumber);

            fileName = (char *) calloc(1, memSize);
            if (!fileName) return (NULL);

            if (!oneStream) {
                 snprintf(fileName, memSize, "%s.%u", temp, streamId);
                 if (debug) printf("No-split, 1 spec, multistream: fileName = %s, run# = %d, streamId = %u\n",
                                  fileName, runNumber, streamId);
            }
            else {
                 strncpy(fileName, temp, strlen(temp));
                 if (debug) printf("No-split, 1 spec, 1 stream: fileName = %s, run# = %d\n",
                                  fileName, runNumber);
            }
        }
        /* For 2 specifiers: insert run # and remove split # specifier as no split # exists */
        else if (specifierCount == 2) {
            /* Get rid of 2nd int specifier */
            name = evStrRemoveSpecifiers(a->baseFileName, 1);
            if (!name) return (NULL);

            /* Insert runNumber into first specifier */
            char temp[memSize];
            snprintf(temp, memSize, name, runNumber);

            fileName = (char *) calloc(1, memSize);
            if (!fileName) return (NULL);

            if (!oneStream) {
                snprintf(fileName, memSize, "%s.%u", temp, streamId);
                if (debug) printf("No-split, 2 spec, multistream: fileName = %s, run# = %d, streamId = %u\n",
                                  fileName, runNumber, streamId);
            }
            else {
                if (debug) printf("No-split, 2 spec, 1 stream: fileName = %s, run# = %d\n",
                                  name, runNumber);
                strncpy(fileName, temp, memSize);
            }

            free(name);
        }
        /* Get rid of extra (3rd) int format specifier as no split # exists */
        else if (specifierCount == 3) {
            /* Get rid of last int specifier */
            name = evStrRemoveSpecifiers(a->baseFileName, 2);
            if (!name) return (NULL);
            fileName = name;

            /* Insert runNumber into first specifier, stream id into 2nd */
            fileName = (char *) calloc(1, memSize);
            if (!fileName) return (NULL);
            snprintf(fileName, memSize, name, runNumber, (int)streamId);
            if (debug) printf("No-split, 3 spec: fileName = %s, run# = %d, streamId = %u\n",
                              fileName, runNumber, streamId);

            free(name);
        }
        /* This shouldn't be necessary */
        else {
            fileName = strdup(a->baseFileName);
        }
    }

    return fileName;
}


/**
 * @defgroup open open & close routines
 * These routines handle opening & closing the ev lib for reading or writing to a file, buffer, or socket.
 * @{
 */

/**
 * This function opens a file for either reading or writing evio format data.
 * Works with all versions of evio for reading, but only writes version 6
 * format. A handle is returned for use with calling other evio routines.
 *
 * @param filename  name of file. Constructs of the form $(env) will be substituted
 *                  with the given environmental variable or removed if nonexistent.
 *                  Constructs of the form %s will be substituted with the run type
 *                  if specified in {@link #evIoctl} or removed if nonexistent.
 *                  Up to 2, C-style int format specifiers are allowed. The first is
 *                  replaced with the run number (set in {@link #evIoctl}). If splitting,
 *                  the second is replaced by the split number, otherwise it's removed.
 *                  If splitting and no second int specifier exists, a "." and split
 *                  number are automatically appended to the end of the file name.
 * @param flags     pointer to case-independent string of "w" for writing,
 *                  "r" for reading, "a" for appending, "ra" for random
 *                  access reading of a file which means memory mapping it,
 *                  or "s" for splitting a file while writing.
 *                  The w, r, a, s, or ra, may be followed by an x which means
 *                  do not do any mutex locking (not thread safe).
 * @param handle    pointer to int which gets filled with handle
 *
 * @return S_SUCCESS          if successful
 * @return S_FAILURE          when splitting, if bad format specifiers or more that 2 specifiers found
 * @return S_EVFILE_BADARG    if filename, flags or handle arg is NULL; unrecognizable flags;
 * @return S_EVFILE_ALLOCFAIL if memory allocation failed
 * @return S_EVFILE_BADFILE   if error reading file, unsupported version,
 *                            or contradictory data in file
 * @return errno              if file could not be opened (handle = 0)
 */
int evOpen(char *filename, char *flags, int *handle)
{
    if (strcasecmp(flags, "w")   != 0 &&
        strcasecmp(flags, "s")   != 0 &&
        strcasecmp(flags, "r")   != 0 &&
        strcasecmp(flags, "a")   != 0 &&
        strcasecmp(flags, "ra")  != 0 &&
        strcasecmp(flags, "wx")  != 0 &&
        strcasecmp(flags, "sx")  != 0 &&
        strcasecmp(flags, "rx")  != 0 &&
        strcasecmp(flags, "ax")  != 0 &&
        strcasecmp(flags, "rax") != 0)  {
        return(S_EVFILE_BADARG);
    }

    return(evOpenImpl(filename, 0, 0, flags, handle));
}



/**
 * This function allows for either reading or writing evio format data from a buffer.
 * Works with all versions of evio for reading, but only writes version 6
 * format. A handle is returned for use with calling other evio routines.
 *
 * @param buffer  pointer to buffer
 * @param bufLen  length of buffer in 32 bit ints
 * @param flags   pointer to case-independent string of "w", "r", "a", or "ra"
 *                for writing/reading/appending/random-access-reading to/from a buffer.
 *                The w, r, a, or ra, may be followed by an x which means
 *                do not do any mutex locking (not thread safe).
 * @param handle  pointer to int which gets filled with handle
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADARG    if buffer, flags or handle arg is NULL; unrecognizable flags;
 *                            buffer size too small
 * @return S_EVFILE_ALLOCFAIL if memory allocation failed
 * @return S_EVFILE_BADFILE   if error reading buffer, unsupported version,
 *                            or contradictory data
 */
int evOpenBuffer(char *buffer, uint32_t bufLen, char *flags, int *handle)
{
    char *flag;
    
    
    /* Check flags & translate them */
    if (strcasecmp(flags, "w") == 0) {
        flag = "wb";
    }
    else if (strcasecmp(flags, "r") == 0) {
        flag = "rb";
    }
    else if (strcasecmp(flags, "a") == 0) {
        flag = "ab";
    }
    else if (strcasecmp(flags, "ra") == 0) {
        flag = "rab";
    }
    else if (strcasecmp(flags, "wx") == 0) {
        flag = "wbx";
    }
    else if (strcasecmp(flags, "rx") == 0) {
        flag = "rbx";
    }
    else if (strcasecmp(flags, "ax") == 0) {
        flag = "abx";
    }
    else if (strcasecmp(flags, "rax") == 0) {
        flag = "rabx";
    }
    else {
        return(S_EVFILE_BADARG);
    }
    
    return(evOpenImpl(buffer, bufLen, 0, flag, handle));
}


/**
 * This function allows for either reading or writing evio format data from a TCP socket.
 * Works with all versions of evio for reading, but only writes version 6
 * format. A handle is returned for use with calling other evio routines.
 *
 * @param sockFd  TCP socket file descriptor
 * @param flags   pointer to case-independent string of "w" & "r" for
 *                writing/reading to/from a socket
 *                The w or r, may be followed by an x which means
 *                do not do any mutex locking (not thread safe).
 * @param handle  pointer to int which gets filled with handle
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADARG    if flags or handle arg is NULL; bad file descriptor arg;
 *                            unrecognizable flags
 * @return S_EVFILE_ALLOCFAIL if memory allocation failed
 * @return S_EVFILE_BADFILE   if error reading socket, unsupported version,
 *                            or contradictory data
 * @return errno              if socket read error
 */
int evOpenSocket(int sockFd, char *flags, int *handle)
{
    char *flag;

    
    /* Check flags & translate them */
    if (strcasecmp(flags, "w") == 0) {
        flag = "ws";
    }
    else if (strcasecmp(flags, "r") == 0) {
        flag = "rs";
    }
    else if (strcasecmp(flags, "wx") == 0) {
        flag = "wsx";
    }
    else if (strcasecmp(flags, "rx") == 0) {
        flag = "rsx";
    }
    else {
        return(S_EVFILE_BADARG);
    }

    return(evOpenImpl((char *)NULL, 0, sockFd, flag, handle));
}


/** @} */


/** For test purposes only ... */
int evOpenFake(char *filename, char *flags, int *handle, char **evf)
{
    EVFILE *a;
    size_t ihandle;
    
    
    a = (EVFILE *)calloc(1, sizeof(EVFILE));
    evFileStructInit(a);

     for (ihandle=0; ihandle < handleCount; ihandle++) {
        if (handleList[ihandle] == 0) {
            handleList[ihandle] = a;
            *handle = ihandle+1;
            break;
        }
    }
    
    a->rw = EV_WRITEFILE;
    a->baseFileName = filename;
    *evf = (char *)a;
    
    return(S_SUCCESS);
}

    
/**
 * This function opens a file, socket, or buffer for either reading or writing
 * evio format data.
 * Works with all versions of evio for reading, but only writes version 4
 * format. A handle is returned for use with calling other evio routines.
 *
 * @param srcDest name of file if flags = "w", "s" or "r";
 *                pointer to buffer if flags = "wb" or "rb"
 * @param bufLen  length of buffer (srcDest) if flags = "wb" or "rb"
 * @param sockFd  socket file descriptor if flags = "ws" or "rs"
 * @param flags   pointer to case-independent string:
 *                "w", "s, "r", "a", & "ra"
 *                for writing/splitting/reading/append/random-access-reading to/from a file;
 *                "ws" & "rs"
 *                for writing/reading to/from a socket;
 *                "wb", "rb", "ab", & "rab"
 *                for writing/reading/append/random-access-reading to/from a buffer.
 *                The above mentioned flags, may be followed by an x which means
 *                do not do any mutex locking (not thread safe).
 * @param handle  pointer to int which gets filled with handle
 * 
 * @return S_SUCCESS          if successful
 * @return S_FAILURE          if code compiled so that block header < 8, 32bit words long;
 *                            when splitting, if bad format specifiers or more that 2 specifiers found
 * @return S_EVFILE_BADARG    if flags or handle arg is NULL;
 *                            if srcDest arg is NULL when using file or buffer;
 *                            if unrecognizable flags;
 *                            if buffer size too small when using buffer;
 *                            if specifying random access on Windows
 * @return S_EVFILE_ALLOCFAIL if memory allocation failed
 * @return S_EVFILE_BADFILE   if error reading file, unsupported version,
 *                            or contradictory data in file
 * @return S_EVFILE_UNXPTDEOF if reading buffer which is too small
 * @return errno              for file opening or socket reading problems
 */
static int evOpenImpl(char *srcDest, uint32_t bufLen, int sockFd, char *flags, int *handle)
{
    EVFILE *a;
    char *filename=NULL, *buffer=NULL, *baseName;

    uint32_t blk_size, temp, headerInfo, blkHdrSize, header[EV_HDSIZ_V6], fileHeader[EV_HDSIZ_V6];
    uint32_t rwBufSize=0, bytesToRead;

    int err, version=6;
    size_t ihandle;
    int64_t nBytes=0;
    const int debug=0;
    int useFile=0, useBuffer=0, useSocket=0;
    int reading=0, randomAccess=0, append=0, splitting=0, specifierCount=0, lockingOn=1;

    
    /* Check args */
    if (flags == NULL || handle == NULL) {
        return(S_EVFILE_BADARG);
    }
    *handle = 0;  /* in case of error, give handle = 0 back to the caller */

    /* Check to see if someone set the length of the block header to be too small. */
    if (EV_HDSIZ < 8) {
if (debug) printf("EV_HDSIZ in evio.h set to be too small (%d). Must be >= 8.\n", EV_HDSIZ);
        return(S_FAILURE);
    }

    /* Are we removing mutex locking? */
    if (strcasecmp(flags, "wx")   == 0  ||
        strcasecmp(flags, "sx")   == 0  ||
        strcasecmp(flags, "rx")   == 0  ||
        strcasecmp(flags, "ax")   == 0  ||
        strcasecmp(flags, "rax")  == 0  ||

        strcasecmp(flags, "wbx")  == 0  ||
        strcasecmp(flags, "rbx")  == 0  ||
        strcasecmp(flags, "abx")  == 0  ||
        strcasecmp(flags, "rabx") == 0  ||

        strcasecmp(flags, "wsx")  == 0  ||
        strcasecmp(flags, "rsx")  == 0)   {

        lockingOn = 0;
    }

    /* Are we dealing with a file, buffer, or socket? */
    if (strcasecmp(flags, "w")    == 0  ||
        strcasecmp(flags, "s")    == 0  ||
        strcasecmp(flags, "r")    == 0  ||
        strcasecmp(flags, "a")    == 0  ||
        strcasecmp(flags, "ra")   == 0  ||

        strcasecmp(flags, "wx")   == 0  ||
        strcasecmp(flags, "sx")   == 0  ||
        strcasecmp(flags, "rx")   == 0  ||
        strcasecmp(flags, "ax")   == 0  ||
        strcasecmp(flags, "rax")  == 0)   {

        useFile = 1;

        if      (strncasecmp(flags,  "a", 1) == 0) append = 1;
        else if (strncasecmp(flags,  "s", 1) == 0) splitting = 1;
        else if (strncasecmp(flags, "ra", 2) == 0) randomAccess = 1;

#if defined _MSC_VER
        /* No random access support in Windows */
        if (randomAccess) {
            return(S_EVFILE_BADARG);
        }
#endif
        if (srcDest == NULL) {
            return(S_EVFILE_BADARG);
        }

        filename = strdup(srcDest);
        if (filename == NULL) {
            return(S_EVFILE_ALLOCFAIL);
        }
        
        /* Trim whitespace from filename front & back */
        evTrim(filename, 0);
    }
    else if (strcasecmp(flags, "wb")   == 0  ||
             strcasecmp(flags, "rb")   == 0  ||
             strcasecmp(flags, "ab")   == 0  ||
             strcasecmp(flags, "rab")  == 0  ||

             strcasecmp(flags, "wbx")  == 0  ||
             strcasecmp(flags, "rbx")  == 0  ||
             strcasecmp(flags, "abx")  == 0  ||
             strcasecmp(flags, "rabx") == 0)   {
        
        useBuffer = 1;
        if      (strcasecmp(flags, "ab")  == 0) append = 1;
        else if (strcasecmp(flags, "rab") == 0) randomAccess = 1;

        if (srcDest == NULL) {
            return(S_EVFILE_BADARG);
        }

        buffer    = srcDest;
        rwBufSize = 4*bufLen;
        /* Smallest possible evio V4 buffer with data =
         * block header (4*8) + evio bank (4*3) */
        if (rwBufSize < 4*11) {
            return(S_EVFILE_BADARG);
        }
    }
    else if (strcasecmp(flags, "ws")  == 0 ||
             strcasecmp(flags, "rs")  == 0 ||
             strcasecmp(flags, "wsx") == 0 ||
             strcasecmp(flags, "rsx") == 0)  {
        
        useSocket = 1;
        if (sockFd < 0) {
            return(S_EVFILE_BADARG);
        }
    }
    else {
        return(S_EVFILE_BADARG);
    }

    if (debug) {
        printf("evOpen: append = %d\n", append);
        printf("evOpen: randomAccess = %d\n", randomAccess);
    }

    /* Are we reading or writing? */
    if (strncasecmp(flags, "r", 1) == 0) {
        reading = 1;
    }

    /* Allocate control structure (mem zeroed) or quit */
    a = (EVFILE *)calloc(1, sizeof(EVFILE));
    if (!a) {
        if (useFile) {
            free(filename);
        }
        return(S_EVFILE_ALLOCFAIL);
    }
    /* Initialize newly allocated structure */
    evFileStructInit(a);

    /* Set the mutex locking */
    a->lockingOn = lockingOn;

    /*********************************************************/
    /* If we're reading a version 1-6 file/socket/buffer,
     * first read a smaller size header compatible with versions 1-4.
     * If the data proves to be version 6, read the full, larger header
     * again or if that's not possible, the rest of the header. */
    /*********************************************************/
    if (reading) {
        
        if (useFile) {
#if defined _MSC_VER
            /* No pipe or zip/unzip support in Windows */
            a->file = fopen(filename,"r");
            a->rw = EV_READFILE;
            a->randomAccess = randomAccess = 0;
#else
            a->rw = EV_READFILE;
            a->randomAccess = randomAccess;

            if (strcmp(filename,"-") == 0) {
                a->file = stdin;
            }
            /* If input filename is standard output of command ... */
            else if (filename[0] == '|') {
                /* Open a process by creating a unidirectional pipe, forking, and
                   invoking the shell. The "filename" is a shell command line.
                   This command is passed to /bin/sh using the -c flag.
                   Make sure we know to use pclose instead of fclose. */
                a->file = popen(filename+1,"r");
                a->rw = EV_READPIPE;
if (debug) printf("evOpen: reading from pipe %s\n", filename + 1);
            }
            else if (randomAccess) {
if (debug) printf("evOpen: MEMORY MAP THE FILE, %s\n", filename);
                err = memoryMapFile(a, filename);
                if (err != S_SUCCESS) {
                    freeEVFILE(a);
                    free(filename);
                    return(errno);
                }
            }
            else {
                a->file = fopen(filename,"r");
            }
#endif
            if (randomAccess) {
                /* Read (copy) in header */
                nBytes = EV_HDSIZ_BYTES;
                memcpy((void *)header, (const void *)a->mmapFile, (size_t)nBytes);
            }
            else {
                int bytesRead = 0, headerSize = EV_HDSIZ_BYTES;
                char *pHead = (char *)header;

                if (a->file == NULL) {
                    freeEVFILE(a);
                    free(filename);
                    return(errno);
                }

                /* Read in header */
                while (bytesRead < headerSize) {

                    nBytes = (int64_t)fread((void *)(pHead + bytesRead), 1,
                                            (size_t)(headerSize - bytesRead), a->file);
                    if (nBytes < 1) {
                        if (feof(a->file)) {
                            localClose(a);
                            freeEVFILE(a);
                            free(filename);
                            return(EOF);
                        }
                        else if (ferror(a->file)) {
                            /* errno is not set for ferror */
                            localClose(a);
                            freeEVFILE(a);
                            free(filename);
                            return(S_FAILURE);
                        }
                    }
                    bytesRead += nBytes;
                }
            }
            a->filePosition = EV_HDSIZ_BYTES;

            if (!a->randomAccess) {
                /* Find the size of the file just opened for reading */
                int fd = fileno(a->file);
                struct stat fstatBuf;
                err = fstat(fd, &fstatBuf);
                if (err != 0) {
                    localClose(a);
                    freeEVFILE(a);
                    free(filename);
                    return(errno);
                }
                a->fileSize = (uint64_t) fstatBuf.st_size;
            }
        }
        else if (useSocket) {
            a->sockFd = sockFd;
            a->rw = EV_READSOCK;
            
            /* Read in header */
            nBytes = (int64_t)tcpRead(sockFd, header, EV_HDSIZ_BYTES);
            if (nBytes < 0) {
                freeEVFILE(a);
                return(errno);
            }
        }
        else if (useBuffer) {
            a->randomAccess = randomAccess;
            a->rwBuf = buffer;
//            a->buf = (uint32_t *) buffer;
            a->rw = EV_READBUF;
            a->rwBufSize = rwBufSize;
            a->bufSize = rwBufSize/4;
            
            /* Read (copy) in header */
            nBytes = EV_HDSIZ_BYTES;
//TODO: get rid of this copy!
            memcpy((void *)header, (const void *)(a->rwBuf), (size_t)nBytes);
            a->rwBytesIn += nBytes;
        }

        /**********************************/
        /* Run header through some checks */
        /**********************************/
if (debug) {
    printf("evOpen: swapped = %d\n", a->byte_swapped);
    int j;
    for (j=0; j < EV_HDSIZ; j++) {
        printf("header[%d] = 0x%x\n", j, header[j]);
    }
    /*
    str = (char *)header;
    printf("header as string = %s\n", str);
    */
}

        /* Check to see if all bytes are there */
        if (nBytes != EV_HDSIZ_BYTES) {
            /* Close file and free memory */
            if (useFile) {
                localClose(a);
                free(filename);
            }
            freeEVFILE(a);
            return(S_EVFILE_BADFILE);
        }

        /* Check endianness */
        if (header[EV_HD_MAGIC] != EV_MAGIC) {
            temp = EVIO_SWAP32(header[EV_HD_MAGIC]);
            if (temp == EV_MAGIC) {
                a->byte_swapped = 1;
            }
            else {
if (debug) printf("Magic # is a bad value\n");
                if (useFile) {
                    localClose(a);
                    free(filename);
                }
                freeEVFILE(a);
                return(S_EVFILE_BADFILE);
            }
        }
        else {
            a->byte_swapped = 0;
        }

        if (a->byte_swapped) {
            a->bigEndian = a->bigEndian ? 0 : 1;
        }

        /* Check VERSION */
        headerInfo = header[EV_HD_VER];
        if (a->byte_swapped) {
            headerInfo = EVIO_SWAP32(headerInfo);
        }

        /* Only lowest 8 bits count in version 4's header word */
        version = headerInfo & EV_VERSION_MASK;
        if (version < 1 || version > 6 || version == 5) {
if (debug) printf("Header has unsupported evio version (%d), quit\n", version);
            if (useFile) {
                localClose(a);
                free(filename);
            }
            freeEVFILE(a);
            return(S_EVFILE_BADFILE);
        }
        a->version = version;

        /* Check the header's value for header size against our assumption. */
        blkHdrSize = header[EV_HD_HDSIZ];
        if (a->byte_swapped) {
            blkHdrSize = EVIO_SWAP32(blkHdrSize);
        }

        /******************************************************************************
         * Version 6 departs radically from the others as there is, for a file,
         * an additional file header before the expected records/blocks.
         * Also the file and record headers are larger, 14 instead of 8 words.
         * And finally, there are the index array and user header for each that must
         * be accounted for.
        ******************************************************************************/

        // Track the bytes from beginning of first record to beginning of first record's
        // first event.
        int recordToEventBytes = 0;

        if (version < 6) {
            /* If actual header size not what we're expecting ... */
            if (blkHdrSize != EV_HDSIZ) {
                int restOfHeader = blkHdrSize - EV_HDSIZ;
                if (debug) printf("Header size was assumed to be %d but it was actually %u\n", EV_HDSIZ, blkHdrSize);
                if (restOfHeader < 0) {
                    if (debug) printf("Header size is too small (%u), return error\n", blkHdrSize);
                    if (useFile) {
                        localClose(a);
                        free(filename);
                    }
                    freeEVFILE(a);
                    return (S_EVFILE_BADFILE);
                }
            }
        }
        // VERSION 6
        else {
            if (debug) printf("Reading from evio version %d source\n", version);

            /* If actual (file) header size not what we're expecting ... */
            if (blkHdrSize != EV_HDSIZ_V6) {
                int restOfHeader = blkHdrSize - EV_HDSIZ_V6;
                if (debug) printf("Header size was assumed to be %d but it was actually %u\n", EV_HDSIZ_V6, blkHdrSize);
                if (restOfHeader < 0) {
                    if (debug) printf("Header size is too small (%u), return error\n", blkHdrSize);
                    if (useFile) {
                        localClose(a);
                        free(filename);
                    }
                    freeEVFILE(a);
                    return (S_EVFILE_BADFILE);
                }
            }

            //------------------------------------------------------------
            //  For file, socket, or buffer, read in first record header
            //------------------------------------------------------------

            if (useFile) {
                //--------------------------------------------------
                // Read in file header first, just re-read.
                // This header will not exist for buffer or socket.
                //--------------------------------------------------

                int bytesRead = 0;

                if (randomAccess) {
                    /* Read in v6 file header */
                    nBytes = EV_HDSIZ_BYTES_V6;
                    memcpy((void *) fileHeader, (const void *) a->mmapFile, (size_t) nBytes);
                }
                else {
                    int headerSize = EV_HDSIZ_BYTES_V6;
                    char *pHead = (char *) fileHeader;

                    /* Read in v6 file header */
                    while (bytesRead < headerSize) {
                        // Back up to file beginning
                        fseek(a->file, 0, 0);
                        nBytes = (int64_t) fread((void *) (pHead + bytesRead), 1,
                                                 (size_t) (headerSize - bytesRead), a->file);
                        if (nBytes < 1) {
                            if (feof(a->file)) {
                                localClose(a);
                                freeEVFILE(a);
                                free(filename);
                                return (EOF);
                            }
                            else if (ferror(a->file)) {
                                localClose(a);
                                freeEVFILE(a);
                                free(filename);
                                return (S_FAILURE);
                            }
                        }
                        bytesRead += nBytes;
                    }

                    /* Check to see if all bytes are there */
                    if (nBytes != headerSize) {
                        localClose(a);
                        free(filename);
                        freeEVFILE(a);
                        return (S_EVFILE_BADFILE);
                    }
                }

                // Swap if necessary
                if (a->byte_swapped) {
                    evioSwapFileHeaderV6(fileHeader);
                }

                if (debug) {
                    int j;
                    for (j=0; j < EV_HDSIZ_V6; j++) {
                        printf("fileHeader[%d] = 0x%x\n", j, fileHeader[j]);
                    }
                }

                // Store some additional info from file header
                a->fileIndexArrayLen = fileHeader[EV_HD_INDEXARRAYLEN];
                a->fileUserHeaderLen = fileHeader[EV_HD_USERHDRLEN];

                // Calculate the 64 bit trailer position from 2, 32 bit words
                uint32_t word1 = fileHeader[EV_HD_TRAILERPOS];
                uint32_t word2 = fileHeader[EV_HD_TRAILERPOS + 1];
                a->trailerPosition = evioToLongWord(word1, word2, 0);

                // Skip over file's header (including those of unusual size)
                uint32_t actualHeaderBytes = 4*fileHeader[EV_HD_HDSIZ];
                // Skip over file's index array since it's not used in coda.
                // Skip over file's user header and its padding since they're not used either.
                int padding = getPad1(fileHeader[EV_HD_VER]);
                a->filePosition = actualHeaderBytes + a->fileIndexArrayLen + a->fileUserHeaderLen + padding;
                a->firstRecordPosition = a->filePosition;

                if (debug) {
printf("evOpenImpl: index array len = %u,  user header len = %u, version word = 0x%x, padding = %d, actual hdr = %u\n",
       a->fileIndexArrayLen, a->fileUserHeaderLen, fileHeader[EV_HD_VER], padding, actualHeaderBytes);
                }

                //---------------------------------------
                // Now read in first record (block) header
                //---------------------------------------
                bytesRead = 0;
                if (randomAccess) {
                    /* Read in v6 file header */
                    nBytes = EV_HDSIZ_BYTES_V6;
                    memcpy((void *) header, (const void *) ((char *) (a->mmapFile) + a->filePosition), (size_t) nBytes);
                }
                else {
                    int headerSize = EV_HDSIZ_BYTES_V6;
                    char *pHead = (char *) header;

                    // Do the actual skipping over index array and user header here
                    if (a->fileIndexArrayLen + a->fileUserHeaderLen + padding > 0) {
                        fseek(a->file, a->filePosition, 0);
                    }

                    /* Read in v6 file header */
                    while (bytesRead < headerSize) {

                        nBytes = (int64_t) fread((void *) (pHead + bytesRead), 1, (size_t) headerSize, a->file);
                        if (nBytes < 1) {
                            if (feof(a->file)) {
                                localClose(a);
                                freeEVFILE(a);
                                free(filename);
                                return (EOF);
                            }
                            else if (ferror(a->file)) {
                                localClose(a);
                                freeEVFILE(a);
                                free(filename);
                                return (S_FAILURE);
                            }
                        }
                        bytesRead += nBytes;
                    }

                    /* Check to see if all bytes are there */
                    if (nBytes != headerSize) {
                        localClose(a);
                        free(filename);
                        freeEVFILE(a);
                        return (S_EVFILE_BADFILE);
                    }
                }

                a->filePosition += EV_HDSIZ_BYTES_V6;
            }
            else if (useSocket) {
if (debug) printf("evOpen: read in rest of header, bytes = %d\n", (int)(EV_HDSIZ_BYTES_V6 - EV_HDSIZ_BYTES));
                /* Read in rest of RECORD header, no file header in this case */
                nBytes += (int64_t) tcpRead(a->sockFd, (void *) (header + EV_HDSIZ),
                                           (EV_HDSIZ_BYTES_V6 - EV_HDSIZ_BYTES));
                if (nBytes != EV_HDSIZ_BYTES_V6) {
                    freeEVFILE(a);
                    return (errno);
                }
            }
            else if (useBuffer) {
                /* Read in rest of RECORD header, no file header in this case */
                nBytes = EV_HDSIZ_BYTES_V6 - EV_HDSIZ_BYTES;
                // TODO: get rid of this copy!
                memcpy((void *) (header + EV_HDSIZ), (const void *) (a->rwBuf + a->rwBytesIn), (size_t) nBytes);
                a->rwBytesIn += nBytes;
            }

            //------------------------------------------------------
            // At this point we've read in the first record header
            //------------------------------------------------------

            // Swap if header necessary
            if (a->byte_swapped) {
                evioSwapRecordHeaderV6(header);
            }

            // But what do we do if header's not a normal size?
            blkHdrSize = header[EV_HD_HDSIZ];
            if (blkHdrSize != EV_HDSIZ_V6) {
                uint32_t restOfHeader = 4*(blkHdrSize - EV_HDSIZ_V6);

                // If too small, quit
                if (restOfHeader < 0) {
                    if (debug) printf("Record header size is too small (%u bytes), return error\n", blkHdrSize);
                    if (useFile) {
                        localClose(a);
                        free(filename);
                    }
                    freeEVFILE(a);
                    return (S_EVFILE_BADFILE);
                }

                // If extra big, skip over file and buffer bytes, but we must read in extra socket bytes
                if (useFile) {
                    a->filePosition += restOfHeader;
                    fseek(a->file, a->filePosition, 0);
                }
                else if (useBuffer) {
                    //printf("evOpen: read in rest of extra big header, bytes = %u\n", restOfHeader);
                    a->rwBytesIn += restOfHeader;
                }
                else if (useSocket) {
                    char junk[restOfHeader];
                    nBytes = (int64_t) tcpRead(a->sockFd, (void *) junk, restOfHeader);
                    if (nBytes != restOfHeader) {
                        freeEVFILE(a);
                        return (errno);
                    }
                }
            }

            if (debug) {
                printf("\n");
                int j;
                for (j=0; j < EV_HDSIZ_V6; j++) {
                    printf("firstHdr[%d] = 0x%x\n", j, header[j]);
                }
            }

            // C library is not equipped to (un)compress data
            uint32_t compWord = header[EV_HD_COMPDATALEN];
            if (isCompressed(compWord)) {
                printf("evOpen: compressed data cannot be read by C evio lib");
                if (useFile) {
                    localClose(a);
                    free(filename);
                }
                freeEVFILE(a);
                return (S_EVFILE_BADFILE);
            }

            // bytes in index array
            uint32_t indexLen = a->curRecordIndexArrayLen = header[EV_HD_INDEXARRAYLEN];
if (debug) printf("evOpen: index array len = %u bytes\n", indexLen);

            // Read event lengths if there are any
            if (indexLen > 0) {

                if (indexLen % 4 != 0) {
printf("evOpen: index array has bad size\n");
                    if (useFile) {
                        localClose(a);
                        free(filename);
                    }
                    freeEVFILE(a);
                    return (S_EVFILE_BADFILE);
                }

                // alloc memory for the event lengths
                a->eventLengths = (uint32_t *) malloc(indexLen);
                a->eventLengthsLen = indexLen/4;
                if (a->eventLengths == NULL) {
                    if (useFile) {
                        localClose(a);
                        free(filename);
                    }
                    freeEVFILE(a);
                    return (S_EVFILE_ALLOCFAIL);
                }

                // read in event lengths
                if (useFile) {
                    int bytesRead = 0;
                    if (randomAccess) {
                        memcpy((void *) a->eventLengths,
                               (const void *) ((char *) (a->mmapFile) + a->filePosition + EV_HDSIZ_V6),
                               (size_t) indexLen);
                    }
                    else {
                        while (bytesRead < indexLen) {
                            nBytes = (int64_t) fread((void *) ((char *) (a->eventLengths) + bytesRead), 1,
                                                     (size_t) (indexLen), a->file);
                            if (nBytes < 1) {
                                if (feof(a->file)) {
                                    localClose(a);
                                    freeEVFILE(a);
                                    free(filename);
                                    return (EOF);
                                }
                                else if (ferror(a->file)) {
                                    localClose(a);
                                    freeEVFILE(a);
                                    free(filename);
                                    return (S_FAILURE);
                                }
                            }
                            bytesRead += nBytes;
                        }

                        /* Check to see if all bytes are there */
                        if (nBytes != indexLen) {
                            localClose(a);
                            free(filename);
                            freeEVFILE(a);
                            return (S_EVFILE_BADFILE);
                        }
                    }

                    a->filePosition += indexLen;
                }
                else if (useSocket) {
                    nBytes = (int64_t) tcpRead(a->sockFd, (void *) (a->eventLengths), indexLen);
                    if (nBytes != indexLen) {
                        freeEVFILE(a);
                        return (errno);
                    }
                }
                else if (useBuffer) {
if (debug) printf("evOpen: read in index array, len = %u bytes\n", indexLen);
                    memcpy((void *) (a->eventLengths), (const void *) (a->rwBuf + a->rwBytesIn), (size_t) indexLen);
                    a->rwBytesIn += indexLen;
                }

                // Swap array in place if necessary
                if (a->byte_swapped) {
                    swap_int32_t(a->eventLengths, indexLen / 4, NULL);
                }
            }

            //---------------------------------------------------------------------
            // Skip over user header and its padding since they're not used
            //---------------------------------------------------------------------

            // bytes in user header
            a->curRecordUserHeaderLen = header[EV_HD_USERHDRLEN];
            // padding bytes in user header
            int padding = getPad1(header[EV_HD_VER]);
            uint32_t bytesToSkip = a->curRecordUserHeaderLen + padding;

            if (bytesToSkip > 0) {
                if (useFile) {
                    a->filePosition += bytesToSkip;
                    fseek(a->file, a->filePosition, 0);
                }
                else if (useBuffer) {
                    a->rwBytesIn += bytesToSkip;
                }
                else if (useSocket) {
                    // For sockets, we still have to put bytes somewhere when reading
                    char storage[bytesToSkip];
                    nBytes = (int64_t) tcpRead(a->sockFd, (void *) (storage), bytesToSkip);
                    if (nBytes != bytesToSkip) {
                        freeEVFILE(a);
                        return (errno);
                    }
                }
            }

            recordToEventBytes = 4*blkHdrSize + indexLen + bytesToSkip;
//printf("evOpen: a->recordToEventBytes = %d, bytesToSkip = %u\n", recordToEventBytes, bytesToSkip);
        }

        if (randomAccess) {
            /* Random access only available for version 4+ */
            if (version < 4) {
                if (useFile) {
                    localClose(a);
                    free(filename);
                }
                freeEVFILE(a);
                return(S_EVFILE_BADFILE);
            }

            /* Find pointers to all the events (skips over any dictionary) */
            err = generatePointerTable(a);
            if (err != S_SUCCESS) {
                if (useFile) {
                    localClose(a);
                    free(filename);
                }
                freeEVFILE(a);
                return(err);
            }
        }
        else {
            /**********************************/
            /* Allocate buffer to store block */
            /**********************************/
    
            /* Size of block/record we're reading, version 6's header is already swapped */
            blk_size = header[EV_HD_BLKSIZ];
            if (a->byte_swapped && version < 6) {
                blk_size = EVIO_SWAP32(blk_size);
            }
            a->blksiz = blk_size;

            if (useFile && version < 4) {
                // Read early version files in big chunks, integral multiples of a block
                a->bufSize = EV_READ_BYTES_V3;
                a->pBuf = a->buf = (uint32_t *) malloc(EV_READ_BYTES_V3);
            }
            else {
                /* How big do we make this buffer? Use a minimum size. */
                a->bufSize = blk_size < EV_BLOCKSIZE_MIN ? EV_BLOCKSIZE_MIN : blk_size;
                a->buf = (uint32_t *) malloc(4 * a->bufSize);
//printf("evOpen: Allocating a->buf = %p, bytes = %u\n", a->buf, (4 * a->bufSize));
            }
//printf("evOpen: a->bufSize = %u\n", a->bufSize);

            /* Error if can't allocate buffer memory */
            if (a->buf == NULL) {
                if (useFile) {
                    localClose(a);
                    free(filename);
                }
                freeEVFILE(a);
                return(S_EVFILE_ALLOCFAIL);
            }
    
            /* Copy header (the part we read in) into block (swapping if necessary) */
            if (version > 4) {
                // Version 6 header, already swapped
                memcpy(a->buf, header, EV_HDSIZ_BYTES_V6);
//printf("evOpen: copied header size %u bytes into a->buf\n", EV_HDSIZ_BYTES_V6);
                // Event lengths, already swapped
                memcpy(a->buf + EV_HDSIZ_V6, a->eventLengths, 4*(a->eventLengthsLen));
                // Do NOT copy in usr header data, since that's ignored in this library
//printf("evOpen: copied event lengths, size %u bytes into a->buf\n\n", 4*(a->eventLengthsLen));
                // Rest of record
                bytesToRead = 4*blk_size - recordToEventBytes;
            }
            else if (a->byte_swapped) {
                swap_int32_t((uint32_t *)header, EV_HDSIZ, a->buf);
                // Rest of block
                bytesToRead = 4*(blk_size - EV_HDSIZ);
            }
            else {
                memcpy(a->buf, header, EV_HDSIZ_BYTES);
                bytesToRead = 4*(blk_size - EV_HDSIZ);
            }
//printf("evOpen: bytesToRead = %u\n", bytesToRead);

            /***********************************************************************/
            /* Read rest of block/record, including extra. over-sized header words */
            /***********************************************************************/

            if (useFile) {
                if (version > 4) {
                    uint32_t offset = EV_HDSIZ_V6 + a->eventLengthsLen;
                    nBytes = (int64_t) fread(a->buf + offset, 1, bytesToRead, a->file);
                }
                else if (version > 3) {
                    nBytes = (int64_t) fread(a->buf + EV_HDSIZ, 1, bytesToRead, a->file);
                }
                else {
                    // We already read in the header. Take that into account when
                    // reading in next blocks.
                    uint64_t bytesLeftInFile = a->fileSize - a->filePosition;
                    uint32_t bytesRead = 0;
                    char *pBuf = (char *)a->buf;

                    bytesToRead = (EV_READ_BYTES_V3 - 32) < bytesLeftInFile ?
                                  (EV_READ_BYTES_V3 - 32) : (uint32_t) bytesLeftInFile;

                    if ( (a->fileSize % 32768) != 0) {
fprintf(stderr,"evOpen: file is NOT integral # of 32K blocks!\n");
                        localClose(a);
                        free(filename);
                        freeEVFILE(a);
                        return (S_FAILURE);
                    }

                    while (bytesRead < bytesToRead) {
                        nBytes = (int64_t) fread((void *)(pBuf + 32 + bytesRead), 1,
                                                 (size_t) (bytesToRead - bytesRead), a->file);
                        if (nBytes < 1) {
                            if (feof(a->file)) {
                                localClose(a);
                                free(filename);
                                freeEVFILE(a);
                                return (EOF);
                            }
                            else if (ferror(a->file)) {
                                /* errno is not set for ferror */
                                localClose(a);
                                free(filename);
                                freeEVFILE(a);
                                return (S_FAILURE);
                            }
                        }
                        bytesRead += nBytes;
                    }

                    a->filePosition += bytesRead;

                    // Set blocks just read in that are not being parsed right now.
                    // We're parsing the very first hence the "- 1".
                    a->blocksToParse = (bytesRead + 32)/32768 - 1;
                }
            }
            else if (useSocket) {
                if (version > 4) {
                    nBytes = (int64_t)tcpRead(sockFd, a->buf + EV_HDSIZ_V6 + a->eventLengthsLen, bytesToRead);
                }
                else {
                    nBytes = (int64_t)tcpRead(sockFd, a->buf+EV_HDSIZ, bytesToRead);
                }
                if (nBytes < 0) {
                    freeEVFILE(a);
                    return(errno);
                }
            }
            else if (useBuffer) {
                if (version > 4) {
                    memcpy(a->buf + EV_HDSIZ_V6 + a->eventLengthsLen, (a->rwBuf + a->rwBytesIn), (size_t) bytesToRead);
                }
                else {
                    memcpy(a->buf + EV_HDSIZ, (a->rwBuf + a->rwBytesIn), (size_t) bytesToRead);
                }
                nBytes = bytesToRead;
                a->rwBytesIn += bytesToRead;
            }
    
            /* Check to see if all bytes were read in */
            if (nBytes != bytesToRead) {
                if (useFile) {
                    localClose(a);
                    free(filename);
                }            
                freeEVFILE(a);
                return(S_EVFILE_BADFILE);
            }

            if (version < 4) {
                /* Pointer to where start of first event header occurs. */
                a->next = a->buf + (a->buf)[EV_HD_START];
                /* Number of valid 32 bit words from start of first event to end of block */
                a->left = (a->buf)[EV_HD_USED] - (a->buf)[EV_HD_START];
            }
            else if (version < 6) {
                /* Pointer to where start of first event header occurs =
                 * right after header for version 4. */
                a->next = a->buf + EV_HDSIZ;
                
                /* Number of valid 32 bit words = (full block size - header size) in v4+ */
                a->left = (a->buf)[EV_HD_BLKSIZ] - EV_HDSIZ;
            
                /* Is this the last block? */
                a->isLastBlock = isLastBlock(a->buf);
                
                /* Pull out dictionary if there is one (works only after header is swapped). */
                if (hasDictionary(a->buf)) {
                    int status;
                    uint32_t buflen;
                    uint32_t *buf;

                    /* Read in first bank which will be dictionary */
                    status = evReadAllocImpl(a, &buf, &buflen);
                    if (status == S_SUCCESS) {
                        /* Trim off any whitespace/padding, skipping over event header (8 bytes) */
                        a->dictionary = evTrim((char *)buf, 8);
                    }
                    else if (debug) {
printf("ERROR retrieving DICTIONARY, status = %#.8x\n", status);
                    }
                }
            }
            else {
                a->next = a->buf + EV_HDSIZ_V6 + a->eventLengthsLen;
//printf("evOpen: a->next = a->buf + 0x%x, a->eventLengthsLen = %u\n", (EV_HDSIZ_V6 + a->eventLengthsLen), a->eventLengthsLen);
//printf("evOpen: value at a->next = 0x%x\n", *(a->next));
                a->left = (a->buf)[EV_HD_BLKSIZ] - EV_HDSIZ_V6 - a->eventLengthsLen;
                a->isLastBlock = isLastBlock(a->buf);
                // Ignore dictionary
            }

            /* Store general info in handle structure */
            a->blknum = a->buf[EV_HD_BLKNUM];
        }
    }

    /*************************/
    /* If we're writing  ... */
    /*************************/
    else {

        a->append = append;
if (debug) printf("evOpen: append while writing to %s\n", filename);

        if (useFile) {
#if defined _MSC_VER
            a->fileName = strdup(filename);
            a->file = fopen(filename,"w");
            a->rw = EV_WRITEFILE;
#else
            a->rw = EV_WRITEFILE;
            if (strcmp(filename,"-") == 0) {
                /* Cannot append to stdout */
                if (append) {
                    freeEVFILE(a);
                    free(filename);
                    return (S_EVFILE_BADARG);
                }
                a->file = stdout;
            }
            else if (filename[0] == '|') {
if (debug) printf("evOpen: writing to pipe %s\n", filename + 1);
                /* Cannot append to pipe */
                if (append) {
                    freeEVFILE(a);
                    free(filename);
                    return (S_EVFILE_BADARG);
                }
                fflush(NULL); /* recommended for writing to pipe */
                a->file = popen(filename+1,"w");
                a->rw = EV_WRITEPIPE;	/* Make sure we know to use pclose */
            }
            else if (append) {
                /* Must be able to read & write since we may
                 * need to write over last block header. Do NOT
                 * truncate (erase) the file here! */
                a->file = fopen(filename,"r+");
                if (a->file == NULL) {
                    freeEVFILE(a);
                    free(filename);
                    return(errno);
                }
if (debug) printf("evOpen: append, opened file %s\n", filename);

                /* Read in header */
                nBytes = (int64_t)(sizeof(header)*fread(header, EV_HDSIZ_BYTES, 1, a->file));
                
                /* Check to see if read the whole header */
                if (nBytes != sizeof(header)) {
                    /* Close file and free memory */
                    fclose(a->file);
                    free(filename);
                    freeEVFILE(a);
                    return(S_EVFILE_BADFILE);
                }
if (debug) printf("evOpen: append, read in %" PRId64 " bytes\n", nBytes);
            }
            else {
                err = evGenerateBaseFileName(filename, &baseName, &specifierCount);
                if (err != S_SUCCESS) {
                    freeEVFILE(a);
                    free(filename);
                    return(err);
                }
                if (splitting) a->splitting = 1;
                a->baseFileName   = baseName;
                a->specifierCount = specifierCount;
            }
#endif
        }
        else if (useSocket) {
            a->sockFd = sockFd;
            a->rw = EV_WRITESOCK;
        }
        else if (useBuffer) {
            a->rwBuf = buffer;
            a->rw = EV_WRITEBUF;
            a->rwBufSize = rwBufSize;
            a->bufSize = rwBufSize/4;

            /* if appending, read in first header */
            if (append) {
                nBytes = EV_HDSIZ_BYTES;
                /* unexpected EOF or end-of-buffer in this case */
                if (rwBufSize < nBytes) {
                    freeEVFILE(a);
                    return(S_EVFILE_UNXPTDEOF);
                }
                /* Read (copy) in header */
                memcpy(header, a->rwBuf, (size_t)nBytes);
            }
        }

        /*********************************************************************/
        /* If we're appending, we already read in (part of) the first header */
        /* header so check a few things like version number and endianness.  */
        /*********************************************************************/
        if (append) {
            /* Check endianness */
            if (header[EV_HD_MAGIC] != EV_MAGIC) {
                temp = EVIO_SWAP32(header[EV_HD_MAGIC]);
                if (temp == EV_MAGIC) {
                    a->byte_swapped = 1;
                }
                else {
if (debug) printf("Magic # is a bad value\n");
                    if (useFile) {
                        localClose(a);
                        free(filename);
                    }
                    freeEVFILE(a);
                    return(S_EVFILE_BADFILE);
                }
            }
            else {
                a->byte_swapped = 0;
            }

            /* Check VERSION */
            headerInfo = header[EV_HD_VER];
            if (a->byte_swapped) {
                headerInfo = EVIO_SWAP32(headerInfo);
            }
            /* Only lowest 8 bits count in version 4's header word */
            version = headerInfo & EV_VERSION_MASK;
            if (version != EV_VERSION) {
if (debug) printf("File must be evio version %d (not %d) for append mode, quit\n", EV_VERSION, version);
                if (useFile) {
                    localClose(a);
                    free(filename);
                }
                freeEVFILE(a);
                return(S_EVFILE_BADFILE);
            }
            a->version = version;

            /* Is there a dictionary? */
            a->hasAppendDictionary = hasDictionaryInt(headerInfo);
        }

        /* Allocate memory only if we are not writing to a buffer */
        if (!useBuffer) {
            /* bufRealSize is EV_BLOCKSIZE by default, see evFileStructInit() */
            a->buf = (uint32_t *) calloc(1,4*a->bufRealSize);
            if (a->buf == NULL) {
                if (useFile) {
                    localClose(a);
                    free(filename);
                }
                freeEVFILE(a);
                return(S_EVFILE_ALLOCFAIL);
            }

            // But before we start writing events, when writing to a file
            // we need to write a file header.
            if (useFile && !append) {
                // Initialize file header
                initFileHeader(a->buf);
                a->bytesToBuf += EV_HDSIZ_BYTES_V6;

                // Now initialize following record header
                initBlockHeader2(a->buf + EV_HDSIZ_V6, 1);
            }
            else {
                // Now initialize record header
                initBlockHeader2(a->buf, 1);
            }
        }
        /* If writing to buffer, skip step of writing to separate block buffer first. */
        else {
            /* If not appending, get the block header in the buffer set up.
             * The equivalent is done in toAppendPosition() when appending. */
            if (!append) {
                /* Block header is at beginning of buffer */
                a->buf = (uint32_t*) a->rwBuf;

                /* Initialize block header (a->left was already initialized to
                 * EV_BLOCKSIZE - EV_HDSIZ_V6) */
                initBlockHeader2(a->buf, 1);

                a->left = a->rwBufSize - EV_HDSIZ_BYTES_V6;

                /* # of bytes "written" - just the block header so far */
                a->rwBytesOut = EV_HDSIZ_BYTES_V6;
            }
        }

        if (version > 4) {
            // Allocate an extra buffer for data if evio 6. We do this since if we're writing,
            // there's an index, dependent on the number of events that goes after the header
            // and before the data. So keep data separate for later easy of writing.
            // Make same size as buf
            a->dataNext = a->dataBuf = (uint32_t *) calloc(1,4*a->bufRealSize);
            if (a->dataBuf == NULL) {
                if (useFile) {
                    localClose(a);
                    free(filename);
                }
                freeEVFILE(a);
                return(S_EVFILE_ALLOCFAIL);
            }

            a->dataLeft = a->bufRealSize;

            // Also keep space for event lengths for writing the index array
            a->eventLengths = (uint32_t *) calloc(a->blkEvMax,4);
            if (a->eventLengths == NULL) {
                if (useFile) {
                    localClose(a);
                    free(filename);
                }
                freeEVFILE(a);
                return(S_EVFILE_ALLOCFAIL);
            }
        }

        /* Set position in file stream / buffer for next write.
         * If not appending this is does nothing. */
if (debug) printf("evOpen: append, call toAppendPosition\n");
        err = toAppendPosition(a);
        if (err != S_SUCCESS) {
            if (useFile) {
                localClose(a);
                free(filename);
            }
            freeEVFILE(a);
            return(err);
        }

        /* Pointer to where next to write. In this case, the start of the
         * first event header will be right after first block header. */
        if (useFile && !append) {
            // Account for file header
            a->currentHeader = a->buf + EV_HDSIZ_V6;
            a->next = a->buf + 2*EV_HDSIZ_V6;
            a->left = a->blksiz - 2*EV_HDSIZ_V6;
        }
        else {
            a->currentHeader = a->buf;
            a->next = a->buf + EV_HDSIZ_V6;
            // a->left already set
        }

        /* Get ready to write the next block */
        a->blknum++;

    } /* if writing */


    /* Don't let no one else get no "a" while we're openin' somethin' */
    getHandleLock();

    /* Do the first-time initialization */
    if (handleCount < 1 || handleList == NULL) {
        expandHandles();
    }

    {
        int gotHandle = 0;
        
        for (ihandle=0; ihandle < handleCount; ihandle++) {
            /* If a slot is available ... */
            if (handleList[ihandle] == NULL) {
                handleList[ihandle] = a;
                *handle = ihandle + 1;
                /* store handle in structure for later convenience */
                a->handle = ihandle + 1;
                gotHandle = 1;
                break;
            }
        }
    
        /* If no available handles left ... */
        if (!gotHandle) {
            /* Remember old handle count */
            int oldHandleLimit = (int) handleCount;
            /* Create 50% more handles (and increase handleCount) */
            expandHandles();

            /* Use a newly created handle */
            ihandle = oldHandleLimit;
            handleList[ihandle] = a;
            *handle = ihandle + 1;
            a->handle = ihandle + 1;
        }
    }
    
    getHandleUnlock();
    
    if (useFile) free(filename);
    return(S_SUCCESS);
}


/**
 * This routine closes any open files and unmaps any mapped memory.
 * @param handle evio handle
 */
static void localClose(EVFILE *a) {
    /* Close file */
    if (a->rw == EV_WRITEFILE) {
        if (a->file != NULL) fclose(a->file);
    }
    else if (a->rw == EV_READFILE) {
        if (a->randomAccess) {
            munmap(a->mmapFile, a->mmapFileSize);
        }
        else {
            fclose(a->file);
        }
    }
    /* Pipes requires special close */
    else if (a->rw == EV_READPIPE || a->rw == EV_WRITEPIPE) {
        pclose(a->file);
    }
}


/**
 * This function memory maps the given file as read only.
 * 
 * @param a         handle structure
 * @param fileName  name of file to map
 *
 * @return S_SUCCESS   if successful
 * @return S_FAILURE   if failure to open or memory map file (errno is set)
 */
static int memoryMapFile(EVFILE *a, const char *fileName) {
    int        fd;
    uint32_t   *pmem;
    size_t      fileSize;
    mode_t      mode;
    struct stat fileInfo;

    
    /* user & user's group have read & write permission */
    mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
    if ((fd = open(fileName, O_RDWR, mode)) < 0) {
        /* errno is set */
        return(S_FAILURE);
    }
    
    /* Find out how big the file is in bytes */
    if (fstat(fd, &fileInfo) != 0) {
        close(fd);
        return(S_FAILURE);
    }
    fileSize = (size_t)fileInfo.st_size;

    /* Map file to local memory in read-only mode. */
    if ((pmem = (uint32_t *)mmap((void *)0, fileSize, PROT_READ,
                                   MAP_PRIVATE, fd, (off_t)0)) == MAP_FAILED) {
        /* errno is set */
        close(fd);
        return(S_FAILURE);
    }

    /* close fd for mapped mem since no longer needed */
    close(fd);
  
    a->mmapFile = pmem;
    a->mmapFileSize = fileSize;
    a->fileSize = fileSize;

    return(S_SUCCESS);
}


/**
 * This function returns a count of the number of events in a file or buffer.
 * If reading with random access, it returns the count taken when initially
 * generating the table of event pointers. If regular reading, the count is
 * generated when asked for in evIoctl. If writing, the count gets incremented
 * by 1 for each evWrite. If appending, the count is set when moving to the
 * correct file position during evOpen and is thereafter incremented with each
 * evWrite.
 *
 * @param a     handle structure
 * @param count pointer to int which gets filled with number of events
 *
 * @return S_SUCCESS          if successful
 * @return S_FAILURE          if using sockets 
 * @return S_EVFILE_BADFILE   if file too small or problem reading file
 * @return S_EVFILE_UNXPTDEOF if buffer too small
 * @return errno              if error in fseek, ftell
 */
static int getEventCount(EVFILE *a, uint32_t *count) {
    int        i, usingBuffer = 0, nBytes;
    ssize_t    startingPosition=0;
    uint32_t   bytesUsed=0, blockEventCount, blockSize, header[EV_HDSIZ];

    /* Already protected with read lock since it's called only by evIoctl */

    /* Reject if using sockets/pipes */
    if (a->rw == EV_WRITESOCK || a->rw == EV_READSOCK ||
        a->rw == EV_WRITEPIPE || a->rw == EV_READPIPE) {
        return(S_FAILURE);
    }

    /* If using random access, counting is already done. */
    if (a->randomAccess) {
        *count = a->eventCount;
        return(S_SUCCESS);
    }

    /* If we have a non-zero event count that means
     * it has already been found and is up-to-date. */
    if (a->eventCount > 0) {
        *count = a->eventCount;
        return(S_SUCCESS);
    }

    /* If we have a zero event count and we're writing (NOT in append mode), 
     * it means nothing has been written yet so nothing to read. */
    if (!a->append && (a->rw == EV_WRITEBUF || a->rw == EV_WRITEFILE)) {
        *count = a->eventCount;
        return(S_SUCCESS);
    }

    /* A zero event count may, in fact, be up-to-date. If it is, recounting is
     * not a big deal since there are no events. If it isn't, we need to count
     * the events. So go ahead and count the events now. */

    /* Buffer or file? */
    if (a->rw == EV_READBUF) {
        usingBuffer = 1;
    }

    /* Go to starting position */
    if (usingBuffer) {
        bytesUsed = 0;
    }
    else {
        /* Record starting position, return here when finished */
        if ((startingPosition = ftell(a->file)) < 0) return(errno);

        /* Go to back to first record of file */
        if (fseek(a->file, a->firstRecordPosition, SEEK_SET) < 0) return(errno);
    }

    while (1) {
        /* Read in EV_HDSIZ (8) ints of header */
        if (usingBuffer) {
            /* Is there enough data to read in header? */
            if (a->rwBufSize - bytesUsed < sizeof(header)) {
                /* unexpected EOF or end-of-buffer in this case */
                return(S_EVFILE_UNXPTDEOF);
            }
            /* Read (copy) in header */
            memcpy(header, (a->rwBuf + bytesUsed), 4*EV_HDSIZ);
        }
        else {
            nBytes = (int) (sizeof(header)*fread(header, sizeof(header), 1, a->file));
                
            /* Check to see if we read the whole header */
            if (nBytes != sizeof(header)) {
                return(S_EVFILE_BADFILE);
            }
        }

        /* Swap header if necessary */
        if (a->byte_swapped) {
            swap_int32_t(header, EV_HDSIZ, NULL);
        }

        /* Look at block header to get info */
        i               = header[EV_HD_VER];
        blockSize       = header[EV_HD_BLKSIZ];
        blockEventCount = header[EV_HD_COUNT];
//printf("getEventCount: ver = 0x%x, blk size = 0x%x, blk ev cnt = %u\n", i, blockSize, blockEventCount);
        
        /* Add to the number of events. Dictionary is NOT
         * included in the header's event count. */
        a->eventCount += blockEventCount;

        /* Stop at the last block */
        if (a->version > 5 && isLastBlockInt(i)) {
            break;
        }
        else if (isLastBlockInt(i)) {
            break;
        }

        /* Hop to next block header */
        if (usingBuffer) {
            /* Is there enough buffer space to hop over block? */
            if (a->rwBufSize - bytesUsed < 4*blockSize) {
                /* unexpected EOF or end-of-buffer in this case */
                return(S_EVFILE_UNXPTDEOF);
            }
            bytesUsed += 4*blockSize;
        }
        else {
//printf("getEventCount: hop to next record = %u words\n", (blockSize-EV_HDSIZ));
            if (fseek(a->file, 4*(blockSize-EV_HDSIZ), SEEK_CUR) < 0) return(errno);
        }
    }

    /* Reset file to original position (buffer needs no resetting) */
    if (!usingBuffer) {
        if (fseek(a->file, startingPosition, SEEK_SET) < 0) return(errno);
    }
    if (count != NULL) *count = a->eventCount;

    return(S_SUCCESS);
}


/**
 * This function steps through a memory mapped file or buffer and creates
 * a table containing pointers to the beginning of all events.
 *
 * @param a  handle structure
 *
 * @return S_SUCCESS          if successful or not random access handle (does nothing).
 * @return S_EVFILE_ALLOCFAIL if memory cannot be allocated.
 * @return S_EVFILE_BADFILE   if bad data format.
 * @return S_EVFILE_UNXPTDEOF if unexpected EOF or end-of-valid-data
 *                            while reading data (perhaps bad block header).
 */
static int generatePointerTable(EVFILE *a) {

    int        i, usingBuffer=0, lastBlock=0, firstBlock=1;
    size_t     bytesLeft;
    uint32_t  *pmem, len, numPointers, blockEventCount, blockHdrSize, evIndex = 0L;

    if (a->version > 4) {
        return generatePointerTableV6(a);
    }

    /* Only random access handles need apply */
    if (!a->randomAccess) {
        return(S_SUCCESS);
    }

    if (a->rw == EV_READBUF) {
        usingBuffer = 1;
    }

    /* Start with space for 10,000 event pointers */
    numPointers = 10000;
    a->pTable = (uint32_t **) malloc(numPointers * sizeof(uint32_t *));
    if (!a->pTable) {
        return (S_EVFILE_ALLOCFAIL);
    }

    if (usingBuffer) {
        pmem = (uint32_t *)a->rwBuf;
        bytesLeft = a->rwBufSize; /* limit on size only */
    }
    else {
        pmem = a->mmapFile;
        bytesLeft = a->mmapFileSize;
    }

    while (!lastBlock) {
        
        /* Look at block header to get info */
        i               = pmem[EV_HD_VER];
        blockHdrSize    = pmem[EV_HD_HDSIZ];
        blockEventCount = pmem[EV_HD_COUNT];
        
        /* Swap if necessary */
        if (a->byte_swapped) {
            i               = EVIO_SWAP32(i);
            blockHdrSize    = EVIO_SWAP32(blockHdrSize);
            blockEventCount = EVIO_SWAP32(blockEventCount);
        }
        lastBlock = isLastBlockInt(i);

        /* Hop over block header to data */
        pmem += blockHdrSize;
        bytesLeft -= 4*blockHdrSize;

        /* Check for a dictionary - the first event in the first block.
         * It's not included in the header block count, but we must take
         * it into account by skipping over it. */
        if (hasDictionaryInt(i) && firstBlock) {
            firstBlock = 0;
            
            /* Get its length */
            len = *pmem;
            if (a->byte_swapped) {
                len = EVIO_SWAP32(len);
            }
            /* bank's len does not include itself */
            len++;

            /* Skip over it */
            pmem += len;
            bytesLeft -= 4*len;
        }

        /* For each event in block, store its location */
        for (i=0; i < (int)blockEventCount; i++) {
            /* Sanity check - must have at least 2 ints left */
            if (bytesLeft < 8) {
                free(a->pTable);
                return(S_EVFILE_UNXPTDEOF);
            }

            len = *pmem;
            if (a->byte_swapped) {
                len = EVIO_SWAP32(len);
            }
            /* bank's len does not include itself */
            len++;
            a->pTable[evIndex++] = pmem;

            /* Need more space for the table, increase by 10,000 pointers each time */
            if (evIndex >= numPointers) {
                uint32_t** old_pTable = a->pTable;
                numPointers += 10000;
                a->pTable = realloc(a->pTable, numPointers*sizeof(uint32_t *));
                if (a->pTable == NULL) {
                    free(old_pTable);
                    return(S_EVFILE_ALLOCFAIL);
                }
            }
        
            pmem += len;
            bytesLeft -= 4*len;
        }
    }
    
    a->eventCount = evIndex;

    return(S_SUCCESS);
}


/**
 * This function steps through a memory mapped file or buffer and creates
 * a table containing pointers to the beginning of all events. For evio version 6 only.
 *
 * @param a  handle structure
 *
 * @return S_SUCCESS          if successful or not random access handle (does nothing).
 * @return S_EVFILE_ALLOCFAIL if memory cannot be allocated.
 * @return S_EVFILE_BADFILE   if bad data format.
 * @return S_EVFILE_UNXPTDEOF if unexpected EOF or end-of-valid-data
 *                            while reading data (perhaps bad block header).
 */
static int generatePointerTableV6(EVFILE *a) {

    int        usingBuffer=0, lastRecord=0;
    size_t     bytesLeft;
    uint32_t  *pmem, numPointers, evIndex = 0L;


    /* Only random access */
    if (!a->randomAccess) {
        return(S_SUCCESS);
    }

    if (a->rw == EV_READBUF) {
        usingBuffer = 1;
    }

    /* Start with space for 10,000 event pointers */
    numPointers = 10000;
    a->pTable = (uint32_t **) malloc(numPointers * sizeof(uint32_t *));
    if (!a->pTable) {
        return (S_EVFILE_ALLOCFAIL);
    }

    if (usingBuffer) {
        pmem = (uint32_t *)a->rwBuf;
        bytesLeft = a->rwBufSize; /* limit on size only */
    }
    else {
        pmem = a->mmapFile + (a->firstRecordPosition)/4;
        bytesLeft = a->mmapFileSize - a->firstRecordPosition;
    }

    while (!lastRecord) {

        /* Look at block header to get info */
        int i                     = pmem[EV_HD_VER];
        uint32_t recordHdrSize    = pmem[EV_HD_HDSIZ];
        uint32_t recordEventCount = pmem[EV_HD_COUNT];
        uint32_t compWord         = pmem[EV_HD_COMPDATALEN];
        uint32_t indexLen         = pmem[EV_HD_INDEXARRAYLEN];
        uint32_t usrHdrLen        = pmem[EV_HD_USERHDRLEN];

        /* Swap if necessary */
        if (a->byte_swapped) {
            i                = EVIO_SWAP32(i);
            recordHdrSize    = EVIO_SWAP32(recordHdrSize);
            recordEventCount = EVIO_SWAP32(recordEventCount);
            compWord         = EVIO_SWAP32(compWord);
            indexLen         = EVIO_SWAP32(indexLen);
            usrHdrLen        = EVIO_SWAP32(usrHdrLen);
        }
        lastRecord = isLastBlockInt(i);

//        printf("generatePointerTable:\n");
//        printf("  ver word = 0x%x\n", i);
//        printf("  hdr size = 0x%x\n", recordHdrSize);
//        printf("  ev count = %u\n", recordEventCount);
//        printf("  indexLen = %u\n", indexLen);
//        printf("  hdr size = %u\n", recordHdrSize);
//        printf("  usr hdr size = %u\n", usrHdrLen);

        // C library is not equipped to (un)compress data
        if (isCompressed(compWord)) {
            printf("generatePointerTableV6: compressed data cannot be read by C evio lib");
            return (S_EVFILE_BADFILE);
        }

        if (indexLen % 4 != 0 || (indexLen != 4*recordEventCount)) {
            printf("generatePointerTableV6: index array has bad size");
            return (S_EVFILE_BADFILE);
        }

        // Need more space for the table, increase by 10,000 pointers each time
        if (evIndex + recordEventCount >= numPointers) {
            uint32_t** old_pTable = a->pTable;
            numPointers += 10000 + recordEventCount;
            a->pTable = realloc(a->pTable, numPointers*sizeof(uint32_t *));
            if (a->pTable == NULL) {
                free(old_pTable);
                return(S_EVFILE_ALLOCFAIL);
            }
        }

        // Hop over record header
        pmem += recordHdrSize;
        bytesLeft -= 4 * recordHdrSize;

        // If there's an index of event lengths, use that.
        // There should always be one, but in case there isn't.
        if (indexLen > 0) {
            // Create pointer to start of first event
            uint32_t  *pevent = pmem + (indexLen + usrHdrLen)/4;
            int j;

            // For each event in block, store its location
            for (j=0; j < (int)recordEventCount; j++) {
                uint32_t eventByteLen = pmem[j];
                if (a->byte_swapped) {
                    eventByteLen = EVIO_SWAP32(eventByteLen);
                }

                a->pTable[evIndex++] = pevent;
                pevent += eventByteLen/4;
            }

            // Hop over index and user header
            pmem = pevent;
            bytesLeft -= indexLen + usrHdrLen;
        }
        else {
            // Else hop through record event by event.
            // Start by hopping over user header
            pmem += usrHdrLen/4;

            for (i=0; i < (int)recordEventCount; i++) {
                // Sanity check - must have at least 2 ints left
                if (bytesLeft < 8) {
                    free(a->pTable);
                    return(S_EVFILE_UNXPTDEOF);
                }

                // Bank's length is first word of bank
                uint32_t eventWordLen = *pmem;
                if (a->byte_swapped) {
                    eventWordLen = EVIO_SWAP32(eventWordLen);
                }

                // Bank's len does not include itself
                eventWordLen++;
                a->pTable[evIndex++] = pmem;

                pmem += eventWordLen;
                bytesLeft -= 4*eventWordLen;
            }
        }
    }

    a->eventCount = evIndex;

    return(S_SUCCESS);
}


/**
 * This function positions a file or buffer for the first {@link #evWrite}
 * in append mode. It makes sure that the last record header is an empty one
 * with its "last record" bit set. Evio version 6.
 *
 * @param a  handle structure
 *
 * @return S_SUCCESS          if successful or not random access handle (does nothing)
 * @return S_EVFILE_TRUNC     if not enough space in buffer to write ending header
 * @return S_EVFILE_BADFILE   if bad file format - cannot read
 * @return S_EVFILE_UNXPTDEOF if unexpected EOF or end-of-valid-data
 *                            while reading data (perhaps bad record header)
 * @return errno              if any file seeking/writing errors
 */
static int toAppendPosition(EVFILE *a) {
    const int debug = 0;
    int usingBuffer=0, readEOF=0;
    uint32_t nBytes, *pmem, sixthWord, header[EV_HDSIZ], *pHeader;
    uint32_t recordBitInfo, recordEventCount, recordSize, recordHeaderSize, recordNumber = 1;


    /* Only for append mode */
    if (!a->append) {
        return (S_SUCCESS);
    }

    if (a->rw == EV_WRITEBUF) {
        usingBuffer = 1;
    }

    if (usingBuffer) {
        /* Go to back to beginning of buffer */
        a->rwBytesOut = 0;  /* # bytes written to rwBuf so far (after each evWrite) */
    }
    else {
        /* Go to back to beginning of file and find our way to the first record,
         * past the file header. */
        if (fseek(a->file, a->firstRecordPosition, SEEK_SET) < 0) {
            return (errno);
        }

        /* Read in 8 words of file header */
        nBytes = (int64_t)(fread(header, 1, EV_HDSIZ_BYTES, a->file));
        if (nBytes != EV_HDSIZ_BYTES) {
            return(S_EVFILE_BADFILE);
        }

        // We already read in part of this before so we know version and endianness
        if (a->byte_swapped) {
            swap_int32_t(header, EV_HDSIZ_BYTES / 4, NULL);
        }

        // Size info from file header
        uint32_t indexLen = header[EV_HD_INDEXARRAYLEN];
        uint32_t userHeaderLen = header[EV_HD_USERHDRLEN];

        // Skip over file's header (including those of unusual size)
        uint32_t actualHeaderBytes = 4*header[EV_HD_HDSIZ];
        // Skip over file's index array, user header and user header's padding
        int padding = getPad1(header[EV_HD_VER]);
        long skipBytes = actualHeaderBytes + indexLen + userHeaderLen + padding;

        if (fseek(a->file, skipBytes, SEEK_SET) < 0) {
            return (errno);
        }
    }

    while (1) {
        /* Read in EV_HDSIZ (8) ints of header. Even though the version 6 header is 14 words,
         * all the data we need is in the first 8. */
        if (usingBuffer) {
            /* Is there enough data to read in header? */
            if (a->rwBufSize - a->rwBytesOut < EV_HDSIZ_BYTES) {
                /* unexpected EOF or end-of-buffer in this case */
                return (S_EVFILE_UNXPTDEOF);
            }

            /* Look for record header info here */
            a->buf = pHeader = (uint32_t *) (a->rwBuf + a->rwBytesOut);

            recordBitInfo    = pHeader[EV_HD_VER];
            recordSize       = pHeader[EV_HD_BLKSIZ];
            recordHeaderSize = pHeader[EV_HD_HDSIZ];
            recordEventCount = pHeader[EV_HD_COUNT];

            /* Swap header if necessary */
            if (a->byte_swapped) {
                recordBitInfo    = EVIO_SWAP32(recordBitInfo);
                recordSize       = EVIO_SWAP32(recordSize);
                recordHeaderSize = EVIO_SWAP32(recordHeaderSize);
                recordEventCount = EVIO_SWAP32(recordEventCount);
            }
        }
        else {
            size_t bytesToRead = EV_HDSIZ_BYTES;
            nBytes = 0;

            /* It may take more than one fread to read all data */
            while (nBytes < bytesToRead) {
                char *pBuf = (char *)(header) + nBytes;
                /* Read from file */
                nBytes += fread((void *)pBuf, 1, bytesToRead - nBytes, a->file);

                /* If End-Of-File ... */
                if (feof(a->file)) {
                    /* clears EOF */
                    clearerr(a->file);

                    /* If we can only read a partial header, evio format error */
                    if (nBytes > 0) {
                        return(S_EVFILE_BADFILE);
                    }

                    /* If here, there's no header to read, file must have ended
                     * just after a regular record, That's OK, we can continue. */
                    readEOF = 1;
                }
                /* Return for error condition of file stream */
                else if (ferror(a->file)) {
                    return(S_EVFILE_BADFILE);
                }
            }

            /* Swap header if necessary */
            if (a->byte_swapped) {
                swap_int32_t(header, EV_HDSIZ, NULL);
            }

            /* Look at record header to get info */
            recordBitInfo    = header[EV_HD_VER];
            recordSize       = header[EV_HD_BLKSIZ];
            recordHeaderSize = header[EV_HD_HDSIZ];
            recordEventCount = header[EV_HD_COUNT];
        }

        /* Add to the number of events */
        a->eventCount += recordEventCount;

        /* Next record has this number. */
        recordNumber++;

        // Stop at the last record. The file may not have a last record if
        // improperly terminated. Running into an End-Of-File will flag
        // this condition.
        if (isLastBlockInt(recordBitInfo) || readEOF) {
            break;
        }

        /* Hop to next record header */
        if (usingBuffer) {
            /* Is there enough buffer space to hop over record? */
            if (a->rwBufSize - a->rwBytesOut < 4 * recordSize) {
                /* unexpected EOF or end-of-buffer in this case */
                return (S_EVFILE_UNXPTDEOF);
            }
            a->rwBytesOut += 4 * recordSize;
        }
        else {
            if (fseek(a->file, 4 * (recordSize - EV_HDSIZ), SEEK_CUR) < 0) return (errno);
        }
    }

    a->eventsToFile = a->eventsToBuf = a->eventCount;

    /*-------------------------------------------------------------------------------
     * If we're here, we've just read the last record header (at least 14 words of it).
     * File position is just past regular-sized header, but buffer position is just before it.
     * Either that or we ran into end of file (last record header missing).
     *
     * If EOF, last record header missing, we're good.
     *
     * Else check to see if the last record contains data. If it does,
     * change a single bit so it's not labeled as the last record,
     * then jump past all data.
     *
     * Else if there is no data, position file before it as preparation for writing
     * the next record.
     *-------------------------------------------------------------------------------*/
    /* If no last, empty record header in file ... */
    if (readEOF) {
        /* It turns out we need to do nothing. The function that
         * calls this method will write out the next record header. */
        recordNumber--;
    }
    // If there's some data (index len > 0 and user header len > 0 or both)
    else if (recordSize > recordHeaderSize) {
        /* Clear last record bit in 6th header word */
        sixthWord = clearLastBlockBitInt(recordBitInfo);

        /* Rewrite header word with bit info & hop over record */
        if (usingBuffer) {
            /*if (debug) printf("toAppendPosition: writing over last record's 6th word for buffer\n");*/
            /* Write over 6th record header word */
            pmem = (uint32_t *) (a->rwBuf + a->rwBytesOut + 4*EV_HD_VER);
            *pmem = sixthWord;

            /* Hop over the entire record */
            a->rwBytesOut += 4*recordSize;

            /* If there's not enough space in the user-given buffer to
             * contain another (empty ending) record header, return an error. */
            if (a->rwBufSize < a->rwBytesOut + EV_HDSIZ_BYTES_V6) {
                return(S_EVFILE_TRUNC);
            }

            /* Initialize bytes in a->rwBuf for a new record header */
            a->buf = (uint32_t *) (a->rwBuf + a->rwBytesOut);
            initBlockHeader(a->buf);
        }
        else {
            /* Back up to before 6th record header word */
            if (debug) printf("toAppendPosition: writing over last record's 6th word, back up %d words\n",
                              (EV_HDSIZ - EV_HD_VER));
            if (fseek(a->file, -4*(EV_HDSIZ - EV_HD_VER), SEEK_CUR) < 0) return(errno);

            /* Write over 6th record header word */
            if (fwrite((const void *)&sixthWord, 1, sizeof(uint32_t), a->file) != sizeof(uint32_t)) {
                return(errno);
            }

            /* Hop over the entire record */
            if (debug) printf("toAppendPosition: wrote over last record's 6th word, hop over %d words\n",
                              (recordSize - (EV_HD_VER + 1)));
            if (fseek(a->file, 4*(recordSize - (EV_HD_VER + 1)), SEEK_CUR) < 0) return(errno);
        }
    }
    // no data in record
    else {
        /* We already read in the record header, now back up so we can overwrite it.
         * If using buffer, we never incremented the position, so we're OK position wise. */
        recordNumber--;
        if (usingBuffer) {
            initBlockHeader(a->buf);
        }
        else {
            long ppos;
            if (fseek(a->file, -1*EV_HDSIZ_BYTES, SEEK_CUR) < 0) return(errno);
            ppos = ftell(a->file);
            if (debug) printf("toAppendPosition: last record had no data, back up 1 header to pos = %ld (%ld words)\n", ppos, ppos/4);
        }
    }

    /* This function gets called right after
     * the handle's record header memory is initialized and other members of
     * the handle structure are also initialized. Some of the values need to
     * be set properly here - like the record number - since we've skipped over
     * all existing records. */
    a->buf[EV_HD_BLKNUM] = recordNumber;
    a->blknum = recordNumber;

    /* We should now be in a state identical to that if we had
     * just now written everything currently in the file/buffer. */

    return(S_SUCCESS);
}


/**
 * This routine reads an evio bank from an evio format file
 * opened with routine {@link #evOpen}, allocates a buffer and fills it with the bank.
 * Works with versions 1-3 of evio. A status is returned. Caller will need
 * to free buffer to avoid a memory leak.
 *
 * @param a      pointer to handle structure
 * @param buffer pointer to pointer to buffer gets filled with
 *               pointer to allocated buffer (caller must free)
 * @param buflen pointer to int gets filled with length of buffer in 32 bit words
 *               including the full (8 byte) header
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADMODE   if opened for writing or random-access reading
 * @return S_EVFILE_BADARG    if buffer or buflen is NULL
 * @return S_EVFILE_ALLOCFAIL if memory cannot be allocated
 * @return S_EVFILE_UNXPTDEOF if unexpected EOF or end-of-valid-data
 *                            while reading data (perhaps bad     block header)
 * @return EOF                if end-of-file or end-of-valid-data reached
 * @return errno              if file/socket read error
 * @return stream error       if file stream error
 */
static int evReadAllocImplFileV3(EVFILE *a, uint32_t **buffer, uint32_t *buflen)
{
    uint32_t *buf, *pBuf;
    int       status;
    uint32_t  nleft, ncopy, len;


    /* If no more data left to read from current block, get a new block */
    if (a->left < 1) {
        status = evGetNewBufferFileV3(a);
        if (status != S_SUCCESS) {
            return(status);
        }
    }

    /* Find number of words to read in next event (including header) */
    if (a->byte_swapped) {
        /* Value at pointer to next event (bank) header = length of bank - 1 */
        nleft = EVIO_SWAP32(*(a->next)) + 1;
    }
    else {
        /* Length of next bank, including header, in 32 bit words */
        nleft = *(a->next) + 1;
    }

    /* Allocate buffer for event */
    len = nleft;
    buf = pBuf = (uint32_t *)malloc(4*len);
    if (buf == NULL) {
        return(S_EVFILE_ALLOCFAIL);
    }

    /* While there is more event data left to read ... */
    while (nleft > 0) {

        /* If no more data left to read from current block, get a new block */
        if (a->left < 1) {
            status = evGetNewBufferFileV3(a);
            if (status != S_SUCCESS) {
                if (buf != NULL) free(buf);
                return(status);
            }
        }

        /* If # words left to read in event <= # words left in block,
         * copy # words left to read in event to buffer, else
         * copy # left in block to buffer.*/
        ncopy = (nleft <= a->left) ? nleft : a->left;

        memcpy(pBuf, a->next, ncopy*4);

        pBuf    += ncopy;
        nleft   -= ncopy;
        a->next += ncopy;
        a->left -= ncopy;
    }

    /* Swap event in place if necessary */
    if (a->byte_swapped) {
        evioswap(buf, 1, NULL);
    }

    /* Return allocated buffer with event inside and its inclusive len (with full header) */
    *buffer = buf;
    *buflen = len;

    return(S_SUCCESS);
}


/**
 * This routine reads an evio bank from an evio format file/socket/buffer
 * opened with routines {@link #evOpen}, {@link #evOpenBuffer}, or
 * {@link #evOpenSocket}, allocates a buffer and fills it with the bank.
 * Works with all versions of evio. A status is returned. Caller will need
 * to free buffer to avoid a memory leak.
 *
 * @param a      pointer to handle structure
 * @param buffer pointer to pointer to buffer gets filled with
 *               pointer to allocated buffer (caller must free)
 * @param buflen pointer to int gets filled with length of buffer in 32 bit words
 *               including the full (8 byte) header
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADMODE   if opened for writing or random-access reading
 * @return S_EVFILE_BADARG    if buffer or buflen is NULL
 * @return S_EVFILE_ALLOCFAIL if memory cannot be allocated
 * @return S_EVFILE_UNXPTDEOF if unexpected EOF or end-of-valid-data
 *                            while reading data (perhaps bad     block header)
 * @return EOF                if end-of-file or end-of-valid-data reached
 * @return errno              if file/socket read error
 * @return stream error       if file stream error
 */
static int evReadAllocImpl(EVFILE *a, uint32_t **buffer, uint32_t *buflen)
{
    uint32_t *buf, *pBuf;
    int       status;
    uint32_t  nleft, ncopy, len;


    /* Check args */
    if (buffer == NULL || buflen == NULL) {
        return(S_EVFILE_BADARG);
    }

    /* Need to be reading not writing */
    if (a->rw != EV_READFILE && a->rw != EV_READPIPE &&
        a->rw != EV_READBUF  && a->rw != EV_READSOCK) {
        return(S_EVFILE_BADMODE);
    }

    /* Cannot be random access reading */
    if (a->randomAccess) {
        return(S_EVFILE_BADMODE);
    }

    if (a->version < 4 && a->rw == EV_READFILE) {
        return evReadAllocImplFileV3(a, buffer, buflen);
    }

    /* If no more data left to read from current block, get a new block */
    if (a->left < 1) {
        status = evGetNewBuffer(a);
        if (status != S_SUCCESS) {
            return(status);
        }
    }

    /* Find number of words to read in next event (including header) */
    if (a->byte_swapped) {
        /* Value at pointer to next event (bank) header = length of bank - 1 */
        nleft = EVIO_SWAP32(*(a->next)) + 1;
    }
    else {
        /* Length of next bank, including header, in 32 bit words */
        nleft = *(a->next) + 1;
    }

    /* Allocate buffer for event */
    len = nleft;
    buf = pBuf = (uint32_t *)malloc(4*len);
    if (buf == NULL) {
        return(S_EVFILE_ALLOCFAIL);
    }

    /* While there is more event data left to read ... */
    while (nleft > 0) {

        /* If no more data left to read from current block, get a new block */
        if (a->left < 1) {
            status = evGetNewBuffer(a);
            if (status != S_SUCCESS) {
                if (buf != NULL) free(buf);
                return(status);
            }
        }

        /* If # words left to read in event <= # words left in block,
         * copy # words left to read in event to buffer, else
         * copy # left in block to buffer.*/
        ncopy = (nleft <= a->left) ? nleft : a->left;
        
        memcpy(pBuf, a->next, ncopy*4);
        
        pBuf    += ncopy;
        nleft   -= ncopy;
        a->next += ncopy;
        a->left -= ncopy;
    }

    /* Swap event in place if necessary */
    if (a->byte_swapped) {
        evioswap(buf, 1, NULL);
    }

    /* Return allocated buffer with event inside and its inclusive len (with full header) */
    *buffer = buf;
    *buflen = len;
    
    return(S_SUCCESS);
}





/**
 * @defgroup read read routines
 * These routines handle opening the ev lib for reading from a file, buffer, or socket.
 * @{
 */


/**
 * Routine to get the next block if reading version 1-3 files.
 *
 * @param a pointer to file structure
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_UNXPTDEOF if unexpected EOF or end-of-valid-data
 *                            while reading data (perhaps bad block header
 *                            or reading from a too-small buffer)
 * @return EOF                if end-of-file or end-of-valid-data reached
 * @return errno              if file/socket read error
 * @return S_FAILURE          if file reading error
 */
static int evGetNewBufferFileV3(EVFILE *a)
{
    uint32_t  blkHdrSize;
    size_t    nBytes=0;
    int       status = S_SUCCESS;


    /* If no data left in the internal buffer ... */
    if (a->blocksToParse < 1) {

        /* Bytes left to read in file */
        uint64_t bytesLeftInFile = a->fileSize - a->filePosition;

        /* The block size is a fixed 32kB which is on the small side.
         * We want to read in 16MB (EV_READ_BYTES_V3) or so at once
         * for efficiency. */
        uint32_t fileBytesToRead = EV_READ_BYTES_V3 < bytesLeftInFile ?
                                   EV_READ_BYTES_V3 : (int) bytesLeftInFile;

        /* Read data */
        uint32_t bytesRead = 0;
        char *pBuf = (char *)a->pBuf;

        if (bytesLeftInFile < 32L) {
            return(EOF);
        }
        while (bytesRead < fileBytesToRead) {
            nBytes = fread((void *)(pBuf + bytesRead), 1,
                           (size_t)(fileBytesToRead - bytesRead), a->file);
            if (nBytes < 1) {
                if (feof(a->file)) {
                    return(EOF);
                }
                else if (ferror(a->file)) {
                    /* errno is not set for ferror */
                    return(S_FAILURE);
                }
            }
            bytesRead += nBytes;
        }

        /* How many blocks beyond the one we're doing right now? */
        a->blocksToParse = fileBytesToRead / 32768 - 1;

        /* Keep track of where were are in internal buffer */
        a->buf = a->pBuf;

        /* Keep track of where were are in reading the file */
        a->filePosition += bytesRead;
    }
    /* We have more data (whole blocks) in the internal buffer */
    else {
        /* Move to next block */
        a->buf += 8192;
        a->blocksToParse--;
    }

    /* Swap header in place if necessary */
    if (a->byte_swapped) {
        swap_int32_t(a->buf, EV_HDSIZ, NULL);
    }

    /* For ver 1-3 all block headers are same size - 8 words. */
    blkHdrSize = a->buf[EV_HD_HDSIZ];
    if (blkHdrSize != 8) {
#ifdef DEBUG
        fprintf(stderr,"evGetNewBufferSeqV3: block header != 8 words\n");
#endif
        /* Although technically OK to have larger block headers, they were always 8 words */
        return(S_FAILURE);
    }

    /* Each block is same size. */
    a->blksiz = a->buf[EV_HD_BLKSIZ];
    if (a->blksiz != 8192) {
#ifdef DEBUG
        fprintf(stderr,"evGetNewBufferSeqV3: block size != 8192 words\n");
#endif
        /* Although technically OK to have a different block size, they were always 8192 words */
        return(S_FAILURE);
    }

    /* Keep track of the # of blocks read */
    a->blknum++;

    /* Is our block # consistent with block header's? */
    if (a->buf[EV_HD_BLKNUM] != a->blknum + a->blkNumDiff) {
        /* Record the difference so we don't print out a message
        * every single time if things get out of sync. */
        a->blkNumDiff = a->buf[EV_HD_BLKNUM] - a->blknum;
#ifdef DEBUG
        fprintf(stderr,"evGetNewBuffer: block # read(%d) is different than expected(%d)\n",
                a->buf[EV_HD_BLKNUM], a->blknum);
#endif
    }

    /* Start out pointing to the data right after the block header.
     * If we're in the middle of reading an event, this will allow
     * us to continue reading it. If we've looking to read a new
     * event, this should point to the next one. */
    a->next = a->buf + blkHdrSize;

    /* Number of valid words left to read in block */
    a->left = (a->buf)[EV_HD_USED] - blkHdrSize;

    /* If there are no valid data left in block ... */
    if (a->left < 1) {
       return(S_EVFILE_UNXPTDEOF);
    }

    return(status);
}


/**
 * This routine reads from an evio format file opened with  {@link #evOpen}
 * and returns the next event in the buffer arg. Works with all versions 1-3
 * evio format. A status is returned.
 *
 * @param handle evio handle
 * @param buffer pointer to buffer provided by user to hold data.
 * @param buflen length of buffer in 32 bit words.
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADMODE   if opened for writing or random-access reading
 * @return S_EVFILE_TRUNC     if buffer provided by caller is too small for event read
 * @return S_EVFILE_BADARG    if buffer is NULL or buflen < 3
 * @return S_EVFILE_BADHANDLE if bad handle arg
 * @return S_EVFILE_ALLOCFAIL if memory cannot be allocated
 * @return S_EVFILE_UNXPTDEOF if unexpected EOF or end-of-valid-data
 *                            while reading data (perhaps bad block header)
 * @return EOF                if end-of-file or end-of-valid-data reached
 * @return errno              if file/socket read error
 * @return stream error       if file stream error
 */
static int evReadFileV3(EVFILE *a, uint32_t *buffer, uint32_t buflen)
{
    int       status,  swap;
    uint32_t  nleft, ncopy;

    /* If no more data left to read from current BLOCK, get a new block */
    if (a->left < 1) {
        status = evGetNewBufferFileV3(a);
        if (status != S_SUCCESS) {
            return(status);
        }
    }

    /* Find number of words to read in next event (including header) */
    if (a->byte_swapped) {
        /* Value at pointer to next event (bank) header = length of bank - 1 */
        nleft = EVIO_SWAP32(*(a->next)) + 1;
    }
    else {
        /* Length of next bank, including header, in 32 bit words */
        nleft = *(a->next) + 1;
    }

    /* Is there NOT enough room in buffer to store whole event? */
    if (nleft > buflen) {
        /* Buffer too small, just return error.
         * Previous evio lib tried to swap truncated event!? */
        return(S_EVFILE_TRUNC);
    }

    /* While there is more event data left to read ... */
    while (nleft > 0) {

        /* If no more data left to read from current block, get a new block */
        if (a->left < 1) {
            status = evGetNewBufferFileV3(a);
            if (status != S_SUCCESS) {
                return(status);
            }
        }

        /* If # words left to read in event <= # words left in block,
         * copy # words left to read in event to buffer, else
         * copy # left in block to buffer.*/
        ncopy = (nleft <= a->left) ? nleft : a->left;

        memcpy(buffer, a->next, ncopy*4);
        buffer += ncopy;

        nleft   -= ncopy;
        a->next += ncopy;
        a->left -= ncopy;
    }

    /* Store value locally so we can release lock before swapping. */
    swap = a->byte_swapped;
    
    /* Swap event if necessary */
    if (swap) {
        evioswap(buffer, 1, NULL);
    }

    return(S_SUCCESS);
}


/**
 * This routine reads from an evio format file/socket/buffer opened with routines
 * {@link #evOpen}, {@link #evOpenBuffer}, or {@link #evOpenSocket} and returns the
 * next event in the buffer arg. Works with all versions of evio. A status is
 * returned.
 *
 * @param handle evio handle
 * @param buffer pointer to buffer
 * @param buflen length of buffer in 32 bit words
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADMODE   if opened for writing or random-access reading
 * @return S_EVFILE_TRUNC     if buffer provided by caller is too small for event read
 * @return S_EVFILE_BADARG    if buffer is NULL or buflen < 3
 * @return S_EVFILE_BADHANDLE if bad handle arg
 * @return S_EVFILE_ALLOCFAIL if memory cannot be allocated
 * @return S_EVFILE_UNXPTDEOF if unexpected EOF or end-of-valid-data
 *                            while reading data (perhaps bad block header)
 * @return EOF                if end-of-file or end-of-valid-data reached
 * @return errno              if file/socket read error
 * @return stream error       if file stream error
 */
int evRead(int handle, uint32_t *buffer, uint32_t buflen)
{
    EVFILE   *a;
    int       status,  swap;
    uint32_t  nleft, ncopy;

    if (handle < 1 || (size_t)handle > handleCount) {
        return(S_EVFILE_BADHANDLE);
    }

    if (buffer == NULL || buflen < 3) {
        return(S_EVFILE_BADARG);
    }

    /* For thread-safe function calls */
    handleLock(handle);

    /* Look up file struct (which contains block buffer) from handle */
    a = handleList[handle-1];

    /* Check args */
    if (a == NULL) {
        handleUnlock(handle);
        return(S_EVFILE_BADHANDLE);
    }

    /* Need to be reading not writing */
    if (a->rw != EV_READFILE && a->rw != EV_READPIPE &&
        a->rw != EV_READBUF  && a->rw != EV_READSOCK) {
        handleUnlock(handle);
        return(S_EVFILE_BADMODE);
    }

    /* Cannot be random access reading */
    if (a->randomAccess) {
        handleUnlock(handle);
        return(S_EVFILE_BADMODE);
    }

    if (a->rw == EV_READFILE && a->version < 4) {
        int err = evReadFileV3(a, buffer, buflen);
        handleUnlock(handle);
        return (err);
    }

//    printf("evRead: a->left = %u\n", a->left);
//    printf("evRead: a->buf - a->next = %ld\n", (a->next - a->buf));
//    printf("evRead: *(a->buf) = 0x%x\n", *(a->buf));

    /* If no more data left to read from current block, get a new block */
    if (a->left < 1) {
        status = evGetNewBuffer(a);
        if (status != S_SUCCESS) {
            handleUnlock(handle);
            return(status);
        }
    }

    /* Find number of words to read in next event (including header) */
    if (a->byte_swapped) {
        /* Value at pointer to next event (bank) header = length of bank - 1 */
        nleft = EVIO_SWAP32(*(a->next)) + 1;
    }
    else {
        /* Length of next bank, including header, in 32 bit words */
        nleft = *(a->next) + 1;
    }

//    int evLen = nleft;
//    uint32_t *nextOrig = a->next;
//    uint32_t *bufferOrig = buffer;
//printf("evRead: nleft = 0x%x, a->left = %u, val at a->next = 0x%x\n", nleft, a->left, *(a->next));

    /* Is there NOT enough room in buffer to store whole event? */
    if (nleft > buflen) {
        /* Buffer too small, just return error.
         * Previous evio lib tried to swap truncated event!? */
        handleUnlock(handle);
        return(S_EVFILE_TRUNC);
    }

    /* While there is more event data left to read ... */
    while (nleft > 0) {

        /* If no more data left to read from current block, get a new block */
        if (a->left < 1) {
            status = evGetNewBuffer(a);
            if (status != S_SUCCESS) {
                handleUnlock(handle);
                return(status);
            }
        }

        /* If # words left to read in event <= # words left in block,
         * copy # words left to read in event to buffer, else
         * copy # left in block to buffer.*/
        ncopy = (nleft <= a->left) ? nleft : a->left;

        memcpy(buffer, a->next, ncopy*4);
        buffer += ncopy;

        nleft   -= ncopy;
        a->next += ncopy;
        a->left -= ncopy;
    }

    /* Store value locally so we can release lock before swapping. */
    swap = a->byte_swapped;

    /* Unlock mutex for multithreaded reads/writes/access */
    handleUnlock(handle);

    /* Swap event if necessary */
    if (swap) {
        evioswap(buffer, 1, NULL);
    }

//    {
//        int j;
//        printf("evRead: data length = %u\n", evLen);
//        for (j=0; j < evLen; j++) {
//            printf("data[%d] = 0x%x\n", j, bufferOrig[j]);
//        }
//        printf("Length of ev = %ld\n", (a->next - nextOrig));
//   }

    return(S_SUCCESS);
}


/**
 * This routine reads an evio bank from an evio format file/socket/buffer
 * opened with routines {@link #evOpen}, {@link #evOpenBuffer}, or
 * {@link #evOpenSocket}, allocates a buffer and fills it with the bank.
 * Works with all versions of evio. A status is returned.
 * Cannot use with random access.
 *
 * @param handle evio handle
 * @param buffer pointer to pointer to buffer gets filled with
 *               pointer to allocated buffer (caller must free)
 * @param buflen pointer to int gets filled with length of buffer in 32 bit words
 *               including the full (8 byte) bank header
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADMODE   if opened for writing or random-access reading
 * @return S_EVFILE_BADARG    if buffer or buflen is NULL
 * @return S_EVFILE_BADHANDLE if bad handle arg
 * @return S_EVFILE_ALLOCFAIL if memory cannot be allocated
 * @return S_EVFILE_UNXPTDEOF if unexpected EOF or end-of-valid-data
 *                            while reading data (perhaps bad block header)
 * @return EOF                if end-of-file or end-of-valid-data reached
 * @return errno              if file/socket read error
 * @return stream error       if file stream error
 */
int evReadAlloc(int handle, uint32_t **buffer, uint32_t *buflen)
{
    EVFILE *a;
    int status;


    if (handle < 1 || (size_t)handle > handleCount) {
        return(S_EVFILE_BADHANDLE);
    }

    /* For thread-safe function calls */
    handleLock(handle);

    /* Look up file struct (which contains block buffer) from handle */
    a = handleList[handle-1];

    if (a == NULL) {
        handleUnlock(handle);
        return(S_EVFILE_BADHANDLE);
    }

    status = evReadAllocImpl(a, buffer, buflen);

    handleUnlock(handle);

    return status;
}


/**
 * This routine reads from an evio format file/buffer/socket opened with routines
 * {@link #evOpen}, {@link #evOpenBuffer}, or {@link #evOpenSocket} and returns a
 * pointer to the next event residing in an internal buffer.
 * If the data needs to be swapped, it is swapped in place. Any other
 * calls to read routines may cause the data to be overwritten.
 * No writing to the returned pointer is allowed.
 * Works only with evio version 4 and up. A status is returned.
 * Caller must free the buffer when finished with it.
 * Cannot use this routine for random access reading.
 *
 * @param handle evio handle
 * @param buffer pointer to pointer to buffer gets filled with pointer to location in
 *               internal buffer which is guaranteed to be valid only until the next
 *               {@link #evRead}, {@link #evReadAlloc}, or {@link #evReadNoCopy} call.
 * @param buflen pointer to int gets filled with length of buffer in 32 bit words
 *               including the full (8 byte) bank header
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADMODE   if opened for writing or random-access reading
 * @return S_EVFILE_BADARG    if buffer or buflen is NULL
 * @return S_EVFILE_BADFILE   if version < 4, unsupported or bad format
 * @return S_EVFILE_BADHANDLE if bad handle arg
 * @return S_EVFILE_ALLOCFAIL if memory cannot be allocated
 * @return S_EVFILE_UNXPTDEOF if unexpected EOF or end-of-valid-data
 *                               while reading data (perhaps bad block header)
 * @return EOF                if end-of-file or end-of-valid-data reached
 * @return errno              if file/socket read error
 * @return stream error       if file stream error
 */
int evReadNoCopy(int handle, const uint32_t **buffer, uint32_t *buflen)
{
    EVFILE    *a;
    int       status;
    uint32_t  nleft;


    if (handle < 1 || (size_t)handle > handleCount) {
        return(S_EVFILE_BADHANDLE);
    }
    
    if (buffer == NULL || buflen == NULL) {
        return(S_EVFILE_BADARG);
    }

    /* For thread-safe function calls */
    handleLock(handle);

    /* Look up file struct (which contains block buffer) from handle */
    a = handleList[handle-1];

    /* Check args */
    if (a == NULL) {
        handleUnlock(handle);
        return(S_EVFILE_BADHANDLE);
    }

    /* Returning a pointer into a block only works in evio version 4 and
     * up since in earlier versions events may be split between blocks. */
    if (a->version < 4) {
        handleUnlock(handle);
        return(S_EVFILE_BADFILE);
    }

    /* Need to be reading and not writing */
    if (a->rw != EV_READFILE && a->rw != EV_READPIPE &&
        a->rw != EV_READBUF  && a->rw != EV_READSOCK) {
        handleUnlock(handle);
        return(S_EVFILE_BADMODE);
    }
    
    /* Cannot be random access reading */
    if (a->randomAccess) {
        handleUnlock(handle);
        return(S_EVFILE_BADMODE);
    }
    
    /* If no more data left to read from current block, get a new block */
    if (a->left < 1) {
        status = evGetNewBuffer(a);
        if (status != S_SUCCESS) {
            handleUnlock(handle);
            return(status);
        }
    }

    /* Find number of words to read in next event (including header) */
    if (a->byte_swapped) {
        /* Length of next bank, including header, in 32 bit words */
        nleft = EVIO_SWAP32(*(a->next)) + 1;
                        
        /* swap data in block buffer */
        evioswap(a->next, 1, NULL);
    }
    else {
        /* Length of next bank, including header, in 32 bit words */
        nleft = *(a->next) + 1;
    }

    /* return location of place of event in block buffer */
    *buffer = a->next;
    *buflen = nleft;

    a->next += nleft;
    a->left -= nleft;
    
    handleUnlock(handle);

    return(S_SUCCESS);
}


/**
 * This routine does a random access read from an evio format file/buffer opened
 * with routines {@link #evOpen} or {@link #evOpenBuffer}. It returns a
 * pointer to the desired event residing in either a
 * memory mapped file or buffer when opened in random access mode.<p>
 *
 * If reading a file from a remote machine, it is not wise to use
 * the "ra" flag in evOpen. This memory maps the file which is not
 * ideal over the network.<p>
 * 
 * If the data needs to be swapped, it is swapped in place.
 * No writing to the returned pointer is allowed.
 * Works only with evio version 4 and up. A status is returned.
 *
 * @param handle evio handle
 * @param pEvent pointer which gets filled with pointer to event in buffer or
 *               memory mapped file
 * @param buflen pointer to int gets filled with length of buffer in 32 bit words
 *               including the full (8 byte) bank header
 * @param eventNumber the number of the event to be read (returned) starting at 1.
 *
 * @return S_SUCCESS          if successful
 * @return S_FAILURE          if no events found in file or failure to make random access map
 * @return S_EVFILE_BADMODE   if not opened for random access reading
 * @return S_EVFILE_BADARG    if pEvent arg is NULL
 * @return S_EVFILE_BADFILE   if version < 4, unsupported or bad format
 * @return S_EVFILE_BADHANDLE if bad handle arg
 */
int evReadRandom(int handle, const uint32_t **pEvent, uint32_t *buflen, uint32_t eventNumber)
{
    EVFILE   *a;
    uint32_t *pev;

    if (pEvent == NULL) {
        return(S_EVFILE_BADARG);
    }

    if (handle < 1 || (size_t)handle > handleCount) {
        return(S_EVFILE_BADHANDLE);
    }

    /* For thread-safe function calls */
    handleLock(handle);

    /* Look up file struct (which contains block buffer) from handle */
    a = handleList[handle-1];

    /* Check args */
    if (a == NULL) {
        handleUnlock(handle);
        return(S_EVFILE_BADHANDLE);
    }

    /* Returning a pointer into a block only works in evio version 4 and
    * up since in earlier versions events may be split between blocks. */
    if (a->version < 4) {
        handleUnlock(handle);
        return(S_EVFILE_BADFILE);
    }

    /* Need to be *** random access *** reading (not from socket or pipe) and not writing */
    if ((a->rw != EV_READFILE && a->rw != EV_READBUF) || !a->randomAccess) {
        handleUnlock(handle);
        return(S_EVFILE_BADMODE);
    }

    /* event not in file/buf */
    if (eventNumber > a->eventCount || a->pTable == NULL) {
        handleUnlock(handle);
        return(S_FAILURE);
    }

    pev = a->pTable[eventNumber - 1];

    /* event not in file/buf */
    if (pev == NULL) {
        handleUnlock(handle);
        return(S_FAILURE);
    }

    /* Find number of words to read in next event (including header) */
    /* and swap data in buf/mem-map if necessary */
    if (a->byte_swapped) {
        /* Length of bank, including header, in 32 bit words */
        *buflen = EVIO_SWAP32(*pev) + 1;
                        
        /* swap data in buf/mem-map buffer */
        evioswap(pev, 1, NULL);
    }
    else {
        /* Length of bank, including header, in 32 bit words */
        *buflen = *pev + 1;
    }

    /* return pointer to event in memory map / buffer */
    *pEvent = pev;

    handleUnlock(handle);

    return(S_SUCCESS);
}


/** @} */


/**
 * Routine to get the next block.
 * This is not used for random access reading.
 * @param a pointer to file structure
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_ALLOCFAIL if memory cannot be allocated
 * @return S_EVFILE_UNXPTDEOF if unexpected EOF or end-of-valid-data
 *                            while reading data (perhaps bad block header
 *                            or reading from a too-small buffer)
 * @return EOF                if end-of-file or end-of-valid-data reached
 * @return errno              if file/socket read error
 * @return stream error       if file stream error
 */
static int evGetNewBuffer(EVFILE *a)
{
    uint32_t *newBuf, blkHdrSize, headerBytes, headerWords;
    size_t    nBytes=0, bytesToRead;
    const int debug=0;
    int status = S_SUCCESS;

    assert(a != NULL && a->buf != NULL);       /* else internal error */

    /* See if we read in the last block the last time this was called (v4) */
    if (a->version > 3 && a->isLastBlock) {
        return(EOF);
    }

    /* First read block header from file/sock/buf */
    if (a->version > 4) {
        // Bigger header in evio 6
        headerWords = EV_HDSIZ_V6;
        bytesToRead = headerBytes = EV_HDSIZ_BYTES_V6;
    }
    else {
        headerWords = EV_HDSIZ;
        bytesToRead = headerBytes = EV_HDSIZ_BYTES;
    }

    if (a->rw == EV_READFILE) {
        assert(a->file != NULL);  /* else internal error */

        /* If end-of-file, return EOF as status */
        if (feof(a->file)) {
            return(EOF);
        }

        /* Clear EOF and error indicators for file stream */
        clearerr(a->file);

        /* Read block header */
        nBytes = fread(a->buf, 1, bytesToRead, a->file);
        
        /* Return end-of-file if so */
        if (feof(a->file)) {
            return(EOF);
        }
    
        /* Return any error condition of file stream */
        if (ferror(a->file)) {
            return(ferror(a->file));
        }
    }
    else if (a->rw == EV_READSOCK) {
        nBytes = (size_t) tcpRead(a->sockFd, a->buf, (int)bytesToRead);
    }
    else if (a->rw == EV_READPIPE) {
        nBytes = fread(a->buf, 1, bytesToRead, a->file);
    }
    else if (a->rw == EV_READBUF) {
        if (a->rwBufSize < a->rwBytesIn + bytesToRead) {
            return(S_EVFILE_UNXPTDEOF);
        }
        memcpy(a->buf, (a->rwBuf + a->rwBytesIn), bytesToRead);
        nBytes = bytesToRead;
        a->rwBytesIn += bytesToRead;
    }

    /* Return any read error */
    if (nBytes != bytesToRead) {
        return(errno);
    }

    /* Swap header in place if necessary */
    if (a->byte_swapped) {
        swap_int32_t(a->buf, headerWords, NULL);
    }
    
    /* It is possible that the block header size is > EV_HDSIZ(_V6).
     * I think that the only way this could happen is if someone wrote
     * out an evio file "by hand", that is, not writing using the evio libs
     * but writing the bits directly to file. Be sure to check for it
     * and read any extra words in the header. They may need to be swapped. */
    blkHdrSize = a->buf[EV_HD_HDSIZ];
    if (blkHdrSize > headerWords) {
        /* Read rest of block header from file/sock/buf ... */
        bytesToRead = 4*(blkHdrSize - headerWords);
if (debug) printf("HEADER IS TOO BIG, reading an extra %lu bytes\n", bytesToRead);
        if (a->rw == EV_READFILE) {
            nBytes = fread(a->buf + headerWords, 1, bytesToRead, a->file);
        
            if (feof(a->file)) return(EOF);
            if (ferror(a->file)) return(ferror(a->file));
        }
        else if (a->rw == EV_READSOCK) {
            nBytes = (size_t) tcpRead(a->sockFd, a->buf + headerWords, (int) bytesToRead);
        }
        else if (a->rw == EV_READPIPE) {
            nBytes = fread(a->buf + headerWords, 1, bytesToRead, a->file);
        }
        else if (a->rw == EV_READBUF) {
            if (a->rwBufSize < a->rwBytesIn + bytesToRead) return(S_EVFILE_UNXPTDEOF);
            memcpy(a->buf + headerWords, a->rwBuf + a->rwBytesIn, bytesToRead);
            nBytes = bytesToRead;
            a->rwBytesIn += bytesToRead;
        }

        /* Return any read error */
        if (nBytes != bytesToRead) return(errno);
                
        /* Swap in place if necessary */
        if (a->byte_swapped) {
            swap_int32_t(a->buf + headerWords, (unsigned int) bytesToRead/4, NULL);
        } 
    }
    
    /* Each block may be different size so find it. */
    a->blksiz = a->buf[EV_HD_BLKSIZ];

    /* Do we have room to read the rest of the block data?
     * If not, allocate a bigger block buffer. */
    if (a->bufSize < a->blksiz) {
#ifdef DEBUG
        fprintf(stderr,"evGetNewBuffer: increase internal buffer size to %d bytes\n", 4*a->blksiz);
#endif
        newBuf = (uint32_t *)malloc(4*a->blksiz);
        if (newBuf == NULL) {
            return(S_EVFILE_ALLOCFAIL);
        }
        
        /* copy header into new buf */
        memcpy((void *)newBuf, (void *)a->buf, 4*blkHdrSize);
        
        /* bookkeeping */
        a->bufSize = a->blksiz;
        free(a->buf);
        a->buf = newBuf;
    }

    /* Read rest of block */
    bytesToRead = 4*(a->blksiz - blkHdrSize);
    if (a->rw == EV_READFILE) {
        nBytes = fread((a->buf + blkHdrSize), 1, bytesToRead, a->file);
        if (feof(a->file))   {return(EOF);}
        if (ferror(a->file)) {return(ferror(a->file));}
    }
    else if (a->rw == EV_READSOCK) {
        nBytes = (size_t) tcpRead(a->sockFd, (a->buf + blkHdrSize), (int) bytesToRead);
    }
    else if (a->rw == EV_READSOCK) {
        nBytes = fread((a->buf + blkHdrSize), 1, bytesToRead, a->file);
    }
    else if (a->rw == EV_READBUF) {
        if (a->rwBufSize < a->rwBytesIn + bytesToRead) {
            return(S_EVFILE_UNXPTDEOF);
        }
        memcpy(a->buf + blkHdrSize, a->rwBuf + a->rwBytesIn, bytesToRead);
        nBytes = bytesToRead;
        a->rwBytesIn += bytesToRead;
    }

    /* Return any read error */
    if (nBytes != bytesToRead) {
        return(errno);
    }

    /* Keep track of the # of blocks read */
    a->blknum++;
    
    /* Is our block # consistent with block header's? */
    if (a->buf[EV_HD_BLKNUM] != a->blknum + a->blkNumDiff) {
        /* Record the difference so we don't print out a message
        * every single time if things get out of sync. */
        a->blkNumDiff = a->buf[EV_HD_BLKNUM] - a->blknum;
#ifdef DEBUG
        fprintf(stderr,"evGetNewBuffer: block # read(%d) is different than expected(%d)\n",
                a->buf[EV_HD_BLKNUM], a->blknum);
#endif
    }

    /* Check to see if we just read in the last block (v4,6) */
    if ( (a->version == 4 && isLastBlock(a->buf)) ||
         (a->version > 4 && isLastBlock(a->buf))) {
        a->isLastBlock = 1;
    }

    /* Start out pointing to the data right after the block header.
     * If we're in the middle of reading an event, this will allow
     * us to continue reading it. If we've looking to read a new
     * event, this should point to the next one. */

    // Additional words after header, to get to data, evio 6
    uint32_t additionalHeaderWordsV6 = 0;

    if (a->version > 4) {
        // Need to hop over index and user header and user header's padding besides the header
        uint32_t indexLen = a->buf[EV_HD_INDEXARRAYLEN];
        uint32_t userHeaderLen = a->buf[EV_HD_USERHDRLEN];
        int padding = getPad1(a->buf[EV_HD_VER]);
        additionalHeaderWordsV6 = (indexLen + userHeaderLen + padding)/4;
    }
    a->next = a->buf + blkHdrSize + additionalHeaderWordsV6;

    /* Find number of valid words left to read (w/ evRead) in block */
    if (a->version < 4) {
        a->left = (a->buf)[EV_HD_USED] - blkHdrSize;
    }
    else {
        a->left = a->blksiz - blkHdrSize - additionalHeaderWordsV6;
    }

    /* If there are no valid data left in block ... */
    if (a->left < 1) {
        /* Hit end of valid data in file/sock/buf in v4 */
        if (a->isLastBlock) {
            return(EOF);
        }
        return(S_EVFILE_UNXPTDEOF);
    }

    return(status);
}


/**
 * Calculates the sixth word of the block header which has the version number
 * in the lowest 8 bits (1-8). The arg hasDictionary is set in the 9th bit and
 * isEnd is set in the 10th bit.
 * Four bits of an int (event type) are set in bits 11-14.
 *
 * @param version evio version number
 * @param hasDictionary does this block include an evio xml dictionary as the first event?
 * @param isEnd is this the last block of a file or a buffer?
 * @param eventType 4 bit type of events header is containing
 * @return generated sixth word of this header.
 */
static int generateSixthWord(int version, int hasDictionary, int isEnd, int eventType) {
    version = hasDictionary ? (version | EV_DICTIONARY_MASK) : version;
    version = isEnd ? (version | EV_LASTBLOCK_MASK) : version;
    version |= ((eventType & 0xf) << 10);
    return version;
}


/**
 * Write a block header into the given buffer for evio version 6.
 * This routine assumes data has already been written into a->buf.
 *
 * @param a             pointer to file structure
 * @param wordSize      size in number of 32 bit words
 * @param eventCount    number of events in block
 * @param blockNumber   number of block
 * @param hasDictionary does this block have a dictionary?
 * @param isLast        is this the last block?
 *
 * @return S_SUCCESS    if successful
 * @return S_FAILURE    if not enough space in buffer for header
 */
static int writeNewHeader(EVFILE *a,
                          uint32_t eventCount, uint32_t blockNumber,
                          int hasDictionary, int isLast) {
    uint32_t *pos;
    const int debug=0;

    // In evio 6, we need to write the index and then the data into a->buf.
    // They are kept in a->eventLengths and a->dataBuf. Write these for the
    // previous record before writing the new record's header.

    // If no room left for rest of current record ...
    if ((a->bufSize - a->bytesToBuf/4) < (a->blkEvCount + a->bytesToDataBuf/4)) {
        if (debug) printf("  writeNewHeaderV6: no room in buffer, return, buf size = %u - to buf (words) = %u <? %u\n",
                          a->bufSize, a->bytesToBuf/4, (a->blkEvCount + a->bytesToDataBuf/4));
        return (S_FAILURE);
    }

    // Then write the index of events lengths
    memcpy(a->buf + a->bytesToBuf/4, a->eventLengths, 4*(a->blkEvCount));
if (debug) printf("  writeNewHeaderV6: write index of byte len = %u, bytes past buf = %u\n",
                  4*(a->blkEvCount), a->bytesToBuf);
    a->bytesToBuf += 4*(a->blkEvCount);
if (debug) printf("  writeNewHeaderV6: reset bytes past buf = %u\n", a->bytesToBuf);

    // Finally write the data
    memcpy(a->buf + a->bytesToBuf/4, a->dataBuf, a->bytesToDataBuf);
    a->bytesToBuf += a->bytesToDataBuf;
if (debug) printf("  writeNewHeaderV6: copied data bytes = %u\n", a->bytesToDataBuf);

    a->next += a->blkEvCount + a->bytesToDataBuf/4;
    a->left -= a->blkEvCount + a->bytesToDataBuf/4;

    /* If no room left for a header to be written in buffer ... */
    if ((a->bufSize - a->bytesToBuf/4) < EV_HDSIZ_V6) {
        if (debug) printf("  writeNewHeaderV6: no room in buffer, return, buf size = %u, bytes to buf = %u\n",
                          a->bufSize, a->bytesToBuf/4);
        return (S_FAILURE);
    }

    /* Record where beginning of header is so we can
     * go back and update block size and event count. */
    a->currentHeader = a->next;

if (debug) printf("  writeNewHeaderV6: block# = %d, ev Cnt = %d, 6th wd = 0x%x\n",
    blockNumber, eventCount, generateSixthWord(6, hasDictionary, isLast, 0));

    /* Write header words, some of which will be
     * overwritten later when the values are determined. */
    pos    = a->next;
    pos[0] = EV_HDSIZ_V6;  /* record's actual size in 32-bit words (Ints) */
    pos[1] = blockNumber;  /* incremental count of blocks */
    pos[2] = EV_HDSIZ_V6;  /* header size always 14 */
    pos[3] = eventCount;   /* number of events in block */
    pos[4] = 0;            /* index array len */
    pos[5] = (uint32_t) generateSixthWord(EV_VERSION, hasDictionary, isLast, 0);
    pos[6] = 0;            /* user header length */
    pos[7] = EV_MAGIC;     /* MAGIC_NUMBER */
    pos[8] = 0;            /* uncompressed byte length of record */
    pos[9] = 0;            /* compression type and compressed data byte length */
    pos[10] = 0;           /* User register 1 */
    pos[11] = 0;           /* User register 1 */
    pos[12] = 0;           /* User register 2 */
    pos[13] = 0;           /* User register 2 */

    // HIPO/Evio_V6 format is NOT ideal for online and writing.
    // An index will need to be inserted between header and data
    // somewhere down the road, which means a full copy of the data :(
    // Blame upper physics management. I had nothing to do with this.

    a->next += EV_HDSIZ_V6;
    a->left -= EV_HDSIZ_V6;

    // Start all over with new data for this new record
    a->dataNext = a->dataBuf;
    a->dataLeft = a->bufRealSize;
    a->bytesToDataBuf = 0;

    a->blksiz  = EV_HDSIZ_V6;
    a->blkEvCount = 0;
    a->bytesToBuf += EV_HDSIZ_BYTES_V6;

    if (isLast) {
if (debug) printf("  writeNewHeaderV6: last empty header added\n");
        /* Last item in internal buffer is last empty block header */
        a->lastEmptyBlockHeaderExists = 1;
    }

    if (debug) printf("  writeNewHeaderV6: add hdr to bytesToBuf = %u\n", a->bytesToBuf);
    return (S_SUCCESS);
}


/**
 * This routine expands the size of the internal buffer used when
 * writing to files/sockets/pipes. Some variables are updated.
 * Assumes 1 block header of space has been (or shortly will be) used.
 * In usage, {@link #resetBuffer(EVFILE *, int)} always follows this routine.
 *
 * @param a pointer to data structure
 * @param newSize size in bytes to make the new buffer
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_ALLOCFAIL if cannot allocate memory
 */
static int expandBuffer(EVFILE *a, uint32_t newSize)
{
    const int debug = 0;
    uint32_t *biggerBuf = NULL;
    
    /* No need to increase it. */
    if (newSize <= 4*a->bufSize) {
if (debug) printf("    expandBuffer: buffer is big enough\n");
        return(S_SUCCESS);
    }
    /* The memory is already there, just not currently utilized */
    else if (newSize <= 4*a->bufRealSize) {
if (debug) printf("    expandBuffer: expand, but memory already there\n");
        a->bufSize = newSize/4;
        return(S_SUCCESS);
    }
    
    biggerBuf = (uint32_t *) malloc(newSize);
    if (!biggerBuf) {
        return(S_EVFILE_ALLOCFAIL);
    }
    
if (debug) printf("    expandBuffer: increased buffer size to %u bytes\n", newSize);
    
    /* Use the new buffer from here on */
    free(a->buf);
    a->buf = biggerBuf;
    a->currentHeader = biggerBuf;

    /* Update free space size, pointer to writing space, & buffer size */
    a->left = newSize/4;
    a->next = a->buf;
    a->bufRealSize = a->bufSize = newSize/4;

    if (a->version > 4) {
        // Also increase the buffer that holds data
        uint32_t *biggerDataBuf = (uint32_t *) malloc(newSize);
        if (!biggerDataBuf) {
            return(S_EVFILE_ALLOCFAIL);
        }

        /* Use the new data buffer from here on */
        free(a->dataBuf);
        a->dataBuf = biggerDataBuf;

        /* Update variables */
        a->dataLeft = newSize/4;
        a->dataNext = a->dataBuf;
        a->bytesToDataBuf = 0;
    }
    
    return(S_SUCCESS);
}


/**
 * This routine writes an event into the internal buffer
 * and does much of the bookkeeping associated with it.
 * 
 * @param a             pointer to data structure
 * @param buffer        buffer containing event to be written
 * @param wordsToWrite  number of 32-bit words to write from buffer
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_ALLOCFAIL if cannot allocate memory
 */
static int writeEventToBufferV6(EVFILE *a, const uint32_t *buffer, uint32_t wordsToWrite) {
    const int debug = 0;

    if (debug) {
        printf("    writeEventToBufferV6: before write, bytes already in Buf = %u, bytes to write = %u\n",
               a->bytesToBuf, 4 * wordsToWrite);

        printf("    writeEventToBufferV6: dataNext - dataBuf = %d, next - buf = %d\n",
               (int) (a->dataNext - a->dataBuf), (int) (a->next - a->buf));
    }

    /* Write event to internal data buffer */
    memcpy((void *)a->dataNext, (const void *)buffer, 4*wordsToWrite);

    /* Update the current block header's size, event count, ... */
    a->blksiz         +=   wordsToWrite + 1;  // don't forget the word in index of event lengths
    a->bytesToDataBuf += 4*wordsToWrite;
    a->eventLengths[a->blkEvCount] = 4*wordsToWrite;

    // Note: we didn't update a->bytesToBuf, a->next, a->left since we haven't actually
    // written data into buf, only into a->dataBuf.

    if (debug) printf("    writeEventToBufferV6: add %u bytes, bytesToBuf = %u\n", (4*wordsToWrite), a->bytesToBuf);

    a->dataNext += wordsToWrite;
    a->dataLeft -= wordsToWrite;
    a->blkEvCount++;
    a->eventsToBuf++;
    // record size
    a->currentHeader[EV_HD_BLKSIZ] = a->blksiz;
    a->eventCount++;
    // # of events in record
    a->currentHeader[EV_HD_COUNT] = a->blkEvCount;
    // index (event lengths) array length
    a->currentHeader[EV_HD_INDEXARRAYLEN] = 4*(a->blkEvCount);
    // uncompressed data length in bytes
    a->currentHeader[EV_HD_UNCOMPDATALEN] = 4*(a->blksiz - a->blkEvCount - EV_HDSIZ_V6);

    /* Signifies that we wrote an event. Used in evIoctl
     * when determined whether an event was appended already. */
    if (a->append) a->append = 2;

    /* If we're writing over the last empty block header for the
     * first time (first write after opening file or flush), clear last block bit */
    if (isLastBlock(a->currentHeader)) {
        /* Always end up here if writing a dictionary */
if (debug) printf("    writeEventToBufferV6: IS LAST BLOCK\n");
        clearLastBlockBit(a->currentHeader);
        ///* Here is where blknum goes from 1 to 2 */
        //a->blknum++;
    }

//    if (debug) {
//        printf("    writeEventToBufferV6: after last header written, Events written to:\n");
//        printf("                          cnt total (no dict) = %u\n", a->eventCount);
//        printf("                          file cnt total (dict) = %u\n", a->eventsToFile);
//        printf("                          internal buffer cnt (dict) = %u\n", a->eventsToBuf);
//        printf("                          block cnt (dict) = %u\n", a->blkEvCount);
//        printf("                          bytes-to-buf = %u\n", a->bytesToBuf);
//        printf("                          blksiz  = %u\n", a->blksiz);
//        printf("                          block # = %u\n", a->blknum);
//    }

    return (S_SUCCESS);
}


/**
 * This routine writes an event into the internal buffer
 * and does much of the bookkeeping associated with it.
 *
 * @param a             pointer to data structure
 * @param buffer        buffer containing event to be written
 * @param wordsToWrite  number of 32-bit words to write from buffer
 * @param isDictionary  true if is this a dictionary being written
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_ALLOCFAIL if cannot allocate memory
 */
static int writeEventToBuffer(EVFILE *a, const uint32_t *buffer,
                               uint32_t wordsToWrite, int isDictionary)
{
    const int debug = 0;

    if (a->version > 4) {
        return writeEventToBufferV6(a, buffer, wordsToWrite);
    }

    if (debug) printf("    writeEventToBuffer: before write, bytesToBuf = %u\n", a->bytesToBuf);

    /* Write event to internal buffer */
    memcpy((void *)a->next, (const void *)buffer, 4*wordsToWrite);

    /* Update the current block header's size, event count, ... */
    a->blksiz     +=   wordsToWrite;
    a->bytesToBuf += 4*wordsToWrite;
    if (debug) printf("    writeEventToBuffer: add %u bytes, bytesToBuf = %u\n", (4*wordsToWrite), a->bytesToBuf);
    a->next       +=   wordsToWrite;
    a->left       -=   wordsToWrite;
    a->blkEvCount++;
    a->eventsToBuf++;
    a->currentHeader[EV_HD_BLKSIZ] = a->blksiz;

    if (isDictionary) {
        /* We are writing a dictionary in this (single) file */
        a->wroteDictionary = 1;
        /* Set bit in block header that there is a dictionary */
        setDictionaryBit(a->buf);
        /* Do not include dictionary in header event count.
         * Dictionaries are written in their own block. */
        a->currentHeader[EV_HD_COUNT] = 0;
        if (debug) printf("    writeEventToBuffer: writing dict, set block cnt = 0, a->blkEvCount = %u\n", a->blkEvCount);
    }
    else {
        a->eventCount++;
        a->currentHeader[EV_HD_COUNT] = a->blkEvCount;
        /* If we wrote a dictionary and it's the first block, don't count dictionary ... */
        if (a->wroteDictionary && a->blknum == 2 && (a->blkEvCount - 1 > 0)) {
            a->currentHeader[EV_HD_COUNT]--;
            if (debug) printf("    writeEventToBuffer: substract ev cnt since in dictionary's block, cur header block cnt = %u, a->blkEvCount = %u\n",
                              a->currentHeader[EV_HD_COUNT],  a->blkEvCount);
        }

        /* Signifies that we wrote an event. Used in evIoctl
         * when determined whether an event was appended already. */
        if (a->append) a->append = 2;
    }

    /* If we're writing over the last empty block header for the
     * first time (first write after opening file or flush), clear last block bit */
    if (isLastBlock(a->currentHeader)) {
        /* Always end up here if writing a dictionary */
        if (debug) printf("  writeEventToBuffer: IS LAST BLOCK\n");
        clearLastBlockBit(a->currentHeader);
        ///* Here is where blknum goes from 1 to 2 */
        //a->blknum++;
    }

//    if (debug) {
//        printf("    writeEventToBuffer: after last header written, Events written to:\n");
//        printf("                        cnt total (no dict) = %u\n", a->eventCount);
//        printf("                        file cnt total (dict) = %u\n", a->eventsToFile);
//        printf("                        internal buffer cnt (dict) = %u\n", a->eventsToBuf);
//        printf("                        block cnt (dict) = %u\n", a->blkEvCount);
//        printf("                        bytes-to-buf = %u\n", a->bytesToBuf);
//        printf("                        block # = %u\n", a->blknum);
//        printf("                        blksiz  = %u\n", a->blksiz);
//    }

    return (S_SUCCESS);
}


/**
 * This routine initializes the internal buffer
 * as if evOpen was just called and resets some
 * evio handle structure variables.
 *
 * @param a  pointer to data structure
 */
static void resetBufferV6(EVFILE *a) {

    /* Go back to the beginning of the buffers */
    a->next = a->buf;
    a->dataNext = a->dataBuf;

    /* Reset buffer values */
    a->bytesToBuf = 0;
    a->bytesToDataBuf = 0;
    a->eventsToBuf = 0;
    a->dataLeft = 4*(a->bufRealSize);

    /* By default, last item in internal buffer is NOT last empty block header */
    a->lastEmptyBlockHeaderExists = 0;

    /* Space in number of words, not in header, left for writing in block buffer */
    a->left = a->bufSize;

    /* Initialize block header as empty block and start writing after it.
     * No support for dictionaries in version 6 - too much hassle, use C++ lib. */
    writeNewHeader(a, 0, a->blknum++, 0, 0);
}


/**
 * This routine initializes the internal buffer
 * as if evOpen was just called and resets some
 * evio handle structure variables.
 *
 * @param a                pointer to data structure
 * @param beforeDictionary is this to reset buffer as it was before the
 *                         writing of the dictionary?
 */
void resetBuffer(EVFILE *a, int beforeDictionary) {

    if (a->version > 4) {
        return resetBufferV6(a);
    }
printf("Reset BUffer \n");
    /* Go back to the beginning of the buffer */
    a->next = a->buf;

    /* Reset buffer values */
    a->bytesToBuf = 0;
    a->eventsToBuf = 0;

    /* By default, last item in internal buffer is NOT last empty block header */
    a->lastEmptyBlockHeaderExists = 0;

    /* Space in number of words, not in header, left for writing in block buffer */
    a->left = a->bufSize;

    /* Initialize block header as empty block and start writing after it */
    if (beforeDictionary) {
        /*printf("      resetBuffer: as in evOpen\n");*/
        a->blknum = 1;
        writeNewHeader(a, 0, a->blknum++, ((a->dictionary != NULL) ? 1 : 0), 0);
    }
    else {
        /*printf("      resetBuffer: NOTTTT as in evOpen\n");*/
        writeNewHeader(a, 0, a->blknum++, 0, 0);
    }
}


/**
 * This routine writes an evio event to an internal buffer containing evio data.
 * If the internal buffer is full, it is flushed to the final destination
 * file/socket/buffer/pipe opened with routines {@link #evOpen}, {@link #evOpenBuffer},
 * or {@link #evOpenSocket}. The file will possibly be split into multiple files if
 * a split size was given by calling evIoctl. Note that the split file size may be
 * <b>bigger</b> than the given limit by 54 bytes using the algorithm below.
 * It writes data in evio version 6 format and returns a status.
 *
 * @param handle   evio handle
 * @param buffer   pointer to buffer containing event to write
 * @param useMutex if != 0, use mutex locking, else no locking
 *
 * @return S_SUCCESS          if successful
 * @return S_FAILURE          if internal programming error writing block header, or
 *                            if file/socket write error
 * @return S_EVFILE_BADMODE   if opened for reading or appending to opposite endian file/buffer.
 * @return S_EVFILE_TRUNC     if not enough room writing to a user-supplied buffer
 * @return S_EVFILE_BADARG    if buffer is NULL
 * @return S_EVFILE_BADHANDLE if bad handle arg
 * @return S_EVFILE_ALLOCFAIL if cannot allocate memory
 */
static int evWriteImpl(int handle, const uint32_t *buffer, int useMutex)
{
    EVFILE   *a;
    uint32_t size=0;
    int status, headerBytes = EV_HDSIZ_BYTES_V6, splittingFile=0;
    int doFlush = 0, roomInBuffer = 1, needBiggerBuffer = 0;
    int writeNewBlockHeader = 1, fileActuallySplit = 0;
    const int debug=0;

    if (handle < 1 || (size_t)handle > handleCount) {
        return(S_EVFILE_BADHANDLE);
    }
    
    if (buffer == NULL) {
        return(S_EVFILE_BADARG);
    }

    /* For thread-safe function calls */
    if (useMutex) handleLock(handle);

    /* Look up file struct (which contains block buffer) from handle */
    a = handleList[handle-1];

    /* Check args */
    if (a == NULL) {
        if (useMutex) handleUnlock(handle);
        return(S_EVFILE_BADHANDLE);
    }

    /* If appending and existing file/buffer is opposite endian, return error */
    if (a->append && a->byte_swapped) {
        if (useMutex) handleUnlock(handle);
        return(S_EVFILE_BADMODE);
    }

    /* Need to be open for writing not reading */
    if (a->rw != EV_WRITEFILE && a->rw != EV_WRITEBUF &&
        a->rw != EV_WRITESOCK && a->rw != EV_WRITEPIPE) {
        if (useMutex) handleUnlock(handle);
        return(S_EVFILE_BADMODE);
    }

    /* Number of words/bytes to write = full event size + bank header */
    uint32_t wordsToWrite = buffer[0] + 1;
    uint32_t bytesToWrite = 4 * wordsToWrite;

    // Amount of data and index not written yet into buf, but need to account for it
    uint32_t bytesCommittedToWrite = a->bytesToDataBuf + 4*(a->blkEvCount);

    if (debug && a->splitting) {
printf("evWrite: splitting, bytesToFile = %" PRIu64 ", event bytes = %u, bytesToBuf = %u, split = %" PRIu64 "\n",
               a->bytesToFile, bytesToWrite, a->bytesToBuf, a->split);
printf("evWrite: blockNum = %u, (blkNum == 2) = %d, eventsToBuf (%u)  <=? common blk cnt (%u)\n",
       a->blknum, (a->blknum == 2),  a->eventsToBuf,  a->commonBlkCount);
    }

    /* If we have enough room in the current block and have not exceeded
     * the number of allowed events, then write it in the current block.
     * Worry about memory later. */

    if ( ((wordsToWrite + a->blksiz + bytesCommittedToWrite/4) <= a->blkSizeTarget) &&
          (a->blkEvCount < a->blkEvMax)) {
if (debug) printf("evWrite: do NOT need a new blk header: blk size target = %u >= %u bytes, blk count = %u, max = %u\n",
                   4*(a->blkSizeTarget), 4*(wordsToWrite + a->blksiz + bytesCommittedToWrite), a->blkEvCount, a->blkEvMax);
        writeNewBlockHeader = 0;
    }
    else if (debug) {
printf("evWrite: DO need a new blk header: blk size target = %u < %u bytes,   blk count = %u, max = %u\n",
                 4*(a->blkSizeTarget), 4*(wordsToWrite + a->blksiz + bytesCommittedToWrite + EV_HDSIZ_V6), a->blkEvCount, a->blkEvMax);
        if (a->blkEvCount >= a->blkEvMax) {
printf("evWrite: too many events in block, already have %u\n", a->blkEvCount );
        }
    }

    /* Are we splitting files in general? */
    while (a->splitting) {
        /* Is this event (together with the current buffer, current file,
         *  and ending block header) large enough to split the file? */
        uint64_t totalSize = a->bytesToFile + bytesToWrite + a->bytesToBuf +
                             bytesCommittedToWrite + headerBytes;

        /* If we have to add another record header before this event, account for it. */
        if (writeNewBlockHeader) {
            totalSize += headerBytes;
        }

if (debug) {
    printf("evWrite: splitting = %s: total size = %" PRIu64 " >? split = %" PRIu64 "\n",
           (totalSize > a->split ? "True" : "False"), totalSize, a->split);

    printf("evWrite: total size components: bytesToFile = %" PRIu64 ", bytesToBuf = %u, ev bytes = %u, data bytes = %u\n",
           a->bytesToFile, a->bytesToBuf, bytesToWrite, a->bytesToDataBuf);
}

        /* If we're going to split the file ... */
        if (totalSize > a->split) {
            /* Yep, we're gonna to do it */
            splittingFile = 1;

            /* Flush the current buffer if any events contained and prepare
             * for a new file (split) to hold the current event. */
            if (a->eventsToBuf > 0) {
if (debug) printf("evWrite: eventsToBuf > 0 so doFlush = 1\n");
                doFlush = 1;
            }
        }
        
        break;
    }

if (debug) printf("evWrite: bufSize = %u <? bytesToWrite = %u + 2hdrs + data + index = %u, events in buf = %u\n",
                        (4*a->bufSize), (a->bytesToBuf + bytesToWrite),
                        (a->bytesToBuf + bytesToWrite + 2*headerBytes + bytesCommittedToWrite),
                        a->eventsToBuf);

    /* Is this event (by itself) too big for the current internal buffer?
     * Internal buffer needs room for first block header, event, and ending empty block. */
    if (4*a->bufSize < bytesToWrite + 4 + 2*headerBytes) {
        /* Not enough room in user-supplied buffer for this event */
        if (a->rw == EV_WRITEBUF) {
            if (useMutex) handleUnlock(handle);
if (debug) printf("evWrite: error, bufSize = %u <? current event bytes = %u + 2hdrs + data + index, total = %u, room = %u\n",
                  (4*a->bufSize),
                   bytesToWrite,
                  (bytesToWrite + 2*headerBytes + bytesCommittedToWrite),
                  (4*a->bufSize - a->bytesToBuf - headerBytes - bytesCommittedToWrite));
            return(S_EVFILE_TRUNC);
        }
        
        roomInBuffer = 0;
        needBiggerBuffer = 1;
if (debug) printf("evWrite: NEED another buffer & block for 1 big event, bufferSize = %d bytes\n",
               (4*a->bufSize));
    }
    /* Is this event plus ending block header, in combination with events previously written
     * to the current internal buffer, too big for it? */
    else if ((!writeNewBlockHeader && ((4*a->bufSize - a->bytesToBuf - bytesCommittedToWrite) < bytesToWrite + headerBytes)) ||
             ( writeNewBlockHeader && ((4*a->bufSize - a->bytesToBuf - bytesCommittedToWrite) < bytesToWrite + 2*headerBytes)))  {

        /* Not enough room in user-supplied buffer for this event */
        if (a->rw == EV_WRITEBUF) {
printf("evWrite: NOT ENOUGH ROOM in user-supplied buffer\n");
            if (useMutex) handleUnlock(handle);
            return(S_EVFILE_TRUNC);
        }
        
        if (debug) {
printf("evWrite: NEED to flush buffer and re-use, ");
            if (writeNewBlockHeader) {
printf(" buf room = %d, needed = %d\n", (4*a->bufSize - a->bytesToBuf - bytesCommittedToWrite), (bytesToWrite + 2*headerBytes));
            }
            else {
printf(" buf room = %d, needed = %d\n", (4*a->bufSize - a->bytesToBuf - bytesCommittedToWrite), (bytesToWrite + headerBytes));
            }
        }
        roomInBuffer = 0;
    }

    /* If there is no room in the buffer for this event ... */
    if (!roomInBuffer) {
        /* If we need more room for a single event ... */
        if (needBiggerBuffer) {
            /* We're here because there is not enough room in the internal buffer
             * to write this single large event. Increase buffer to match. */
            size = 4*(wordsToWrite + 2*headerBytes + 1);
if (debug) printf("         must expand, bytes needed for 1 big ev + 2 hdrs = %d\n", size);
        }
        
        /* Flush what we have to file (if anything) */
if (debug) printf("evWrite: no room in Buf so doFlush = 1\n");
        doFlush = 1;
    }

    /* Do we flush? */
    if (doFlush) {
        status = flushToDestination(a, 0, NULL);
        if (status != S_SUCCESS) {
            if (useMutex) handleUnlock(handle);
            return status;
        }
    }

    /* Do we split the file? */
    if (splittingFile) {
        fileActuallySplit = splitFile(a);
        if (fileActuallySplit == S_FAILURE) {
            if (useMutex) handleUnlock(handle);
            return S_FAILURE;
        }
    }

    /* Do we expand buffer? */
    if (needBiggerBuffer) {
        /* If here, we just flushed. */
        status = expandBuffer(a, size);
        if (status != S_SUCCESS) {
            if (useMutex) handleUnlock(handle);
            return status;
        }
    }

    /* If we either flushed events or split the file, reset the
     * internal buffer to prepare it for writing another event. */
    if (doFlush || splittingFile) {
        resetBuffer(a, 0);
        /* We have a newly initialized buffer ready to write
         * to, so we don't need a new block header. */
        writeNewBlockHeader = 0;
    }

    /*********************************************************************/
    /* Now we have enough room for the event in the buffer, record & file */
    /*********************************************************************/
if (debug) printf("evWrite: writeNewBlockHeader = %d\n", writeNewBlockHeader);

    /* Write new record header if required */
    if (writeNewBlockHeader) {
        status = writeNewHeader(a, 1, a->blknum++, 0, 0);
         if (status != S_SUCCESS) {
             if (useMutex)  handleUnlock(handle);
             return(status);
         }
if (debug) printf("evWrite: wrote new block header, bytesToBuf = %u\n", a->bytesToBuf);
    }

    /******************************************/
    /* Write the event to the internal buffer */
    /******************************************/
    writeEventToBuffer(a, buffer, wordsToWrite, 0);

if (debug) {
        printf("evWrite: after last header written, Events written to:\n");
        printf("         cnt total (no dict) = %u\n", a->eventCount);
        printf("         file cnt total = %u\n", a->eventsToFile);
        printf("         internal buffer cnt = %u\n", a->eventsToBuf);
        printf("         current block cnt (dict) = %u\n", a->blkEvCount);
        printf("         bytes-to-buf  = %u\n", a->bytesToBuf);
        printf("         bytes-to-file = %" PRIu64 "\n", a->bytesToFile);
        printf("         bytes-to-databuf = %u\n", a->bytesToDataBuf);
        printf("         block # = %u\n", a->blknum);
}

    if (useMutex) {
        handleUnlock(handle);
    }

    return(S_SUCCESS);
}


/**
 * @defgroup write write routines
 * These routines handle opening the ev lib for writing to a file, buffer, or socket.
 * @{
 */


/**
 * This routine writes an evio event to an internal buffer containing evio data.
 * If that internal buffer is full, it is flushed to the final destination
 * file/socket/buffer/pipe opened with routines {@link #evOpen}, {@link #evOpenBuffer},
 * or {@link #evOpenSocket}.
 * It writes data in evio version 4 format and returns a status.
 *
 * @param handle evio handle
 * @param buffer pointer to buffer containing event to write
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADMODE   if opened for reading or appending to opposite endian file/buffer.
 * @return S_EVFILE_TRUNC     if not enough room writing to a user-supplied buffer
 * @return S_EVFILE_BADARG    if buffer is NULL
 * @return S_EVFILE_BADHANDLE if bad handle arg
 * @return S_EVFILE_ALLOCFAIL if cannot allocate memory
 * @return errno              if file/socket write error
 * @return stream error       if file stream error
 */
int evWrite(int handle, const uint32_t *buffer) {
    return evWriteImpl(handle, buffer, 1);
}


/**
 * This function flushes any remaining internally buffered data to file/socket.
 * Calling {@link #evClose} automatically does this so it isn't necessary
 * to call before closing. This method should only be used when writing
 * events at such a low rate that it takes an inordinate amount of time
 * for internally buffered data to be written to the file.<p>
 *
 * Calling this can kill performance if writing to a hard disk!
 *
 * @param handle evio handle
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADHANDLE if bad handle arg
 * @return S_FAILURE          error occurred during writing
 */
int evFlush(int handle) {

    EVFILE *a;
    int wroteData=0, err;
    const int debug=0;


    if (handle < 1 || (size_t)handle > handleCount) {
        return(S_EVFILE_BADHANDLE);
    }

    /* For thread-safe function calls */
    handleLock(handle);

    /* Look up file struct from handle */
    a = handleList[handle-1];

    /* If already closed, ignore */
    if (a == NULL) {
        handleUnlock(handle);
        return(S_SUCCESS);
    }

    /*
     * Only for writing. Also, if lastEmptyBlockHeaderExists is true, then resetBuffer
     * has been called and no events have been written into buffer yet.
     * In other words, no need to flush an empty, last block header.
     * That is only done in evClose().
     */
if (debug) printf("evFlush: call lastEmptyBlockHeaderExists = %d\n", a->lastEmptyBlockHeaderExists);
    if (((a->rw != EV_WRITEFILE) && (a->rw != EV_WRITEPIPE) && (a->rw != EV_WRITESOCK)) ||
          a->lastEmptyBlockHeaderExists) {
        handleUnlock(handle);
        return(S_SUCCESS);
    }

    // Make sure everything (index and data) is placed in a->buf


    /* Flush everything then clear & write the last empty block into internal buffer.
     * This will kill performance when writing to hard disk! */
    err = flushToDestination(a, 1, &wroteData);
    if (err != S_SUCCESS) {
        handleUnlock(handle);
        return (err);
    }
    if (wroteData) {
        /* If we actually wrote some data, start a new block */
        resetBuffer(a, 0);
    }

    handleUnlock(handle);

    return(S_SUCCESS);
}


/** @} */


/**
 * This routine writes any existing evio format data in an internal buffer
 * (written to with {@link #evWrite}) to the final destination
 * file/socket opened with routines {@link #evOpen} or {@link #evOpenSocket}.
 * It writes data in evio version 6 format.<p>
 * This will not overwrite an existing file if splitting is enabled.
 * The calls to this routine are either in evClose or followed by resetBuffer.
 *
 * @param a         pointer to data structure
 * @param force     if non-zero, force it to write event to the disk.
 * @param wroteData pointer to int which gets filled with 0 if no data written, else 1.
 *                  If an error is returned, int's value is not changed.
 *
 * @return S_SUCCESS if successful
 * @return S_FAILURE  if file could not be opened for writing;
 *                    if file exists already and splitting;
 *                    if file name could not be generated;
 *                    if error writing data;
 */
static int flushToDestination(EVFILE *a, int force, int *wroteData) {

    size_t nBytes=0;
    uint32_t bytesToWrite=0;
    const int debug=0;

    // Find out if we have data not yet written into a->buf. If so, write it all
    if (a->bytesToDataBuf > 0) {
if (debug) printf("    flushToDestination: no write events lengths, blk count = %u\n", a->blkEvCount);
        // Write index to internal data buffer
        memcpy((void *)a->next, (const void *)a->eventLengths, 4*(a->blkEvCount));
        a->next += a->blkEvCount;
        a->left -= a->blkEvCount;

        // Write data to internal data buffer
if (debug) printf("    flushToDestination: write data, bytes = %u\n", a->bytesToDataBuf);
        memcpy((void *)a->next, (const void *)a->dataBuf, a->bytesToDataBuf);
        a->left -= a->bytesToDataBuf/4;

        // Previous calls to writeEventToBuffer have already
        // set the header and a->blksiz properly
        a->bytesToBuf += 4*(a->blkEvCount) + a->bytesToDataBuf;
    }

    /* How much data do we write? */
    bytesToWrite = a->bytesToBuf;

    /* If nothing to write ... */
    if (bytesToWrite < 1) {
        if (debug) printf("    flushToDestination: no events to write\n");
        if (wroteData != NULL) *wroteData = 0;
        return(S_SUCCESS);
    }

    /* Write internal buffer out to socket, file, or pipe */
    if (a->rw == EV_WRITESOCK) {
        if (debug) printf("    flushToDestination: write %u events to SOCKET\n", a->eventsToBuf);
        nBytes = (size_t) tcpWrite(a->sockFd, a->buf, bytesToWrite);
        /* Return any error condition of write attempt */
        if (nBytes != bytesToWrite) {
            if (debug) printf("    flushToDestination: did NOT write correct number of bytes\n");
            /* It's possible some bytes were written over socket before error */
            return(S_FAILURE);
        }
    }
    else if (a->rw == EV_WRITEPIPE) {
        if (debug) printf("    flushToDestination: write %u events to PIPE\n", a->eventsToBuf);

        /* It may take more than one fwrite to write all data */
        while (nBytes < bytesToWrite) {
            char *pBuf = (char *)(a->buf) + nBytes;
            /* Write block to file */
            nBytes += fwrite((const void *)pBuf, 1, bytesToWrite - nBytes, a->file);

            /* Return for error condition of file stream */
            if (ferror(a->file)) return(S_FAILURE);
        }
    }
    else if (a->rw == EV_WRITEFILE) {
        if (debug) printf("    flushToDestination: write %u events to FILE\n", a->eventsToBuf);
        /* Clear EOF and error indicators for file stream */
        if (a->file != NULL)
            clearerr(a->file);

        else {
            /* a->file == NULL: create the file now */

            assert (a->bytesToFile < 1);  /* else EVFILE incorrectly initialized */
            a->bytesToFile = 0;

            /* Generate the file name if not done yet (very first file) */
            if (a->fileName == NULL) {
                /* Generate actual file name from base name */
                char *fname = evGenerateFileName(a, a->specifierCount, a->runNumber,
                                                 a->splitting, a->splitNumber++,
                                                 a->runType, a->streamId, a->streamCount, debug);
                if (fname == NULL) {
                    return(S_FAILURE);
                }
                a->fileName = fname;
                /*if (debug) printf("    flushToDestination: generate first file name = %s\n", a->fileName);*/
            }

            if (debug) printf("    flushToDestination: create file = %s\n", a->fileName);

            /* If splitting, don't overwrite a file ... */
            if (a->splitting) {
                if (fileExists(a->fileName)) {
                    printf("    flushToDestination: will not overwrite file = %s\n", a->fileName);
                    return(S_FAILURE);
                }
            }

            a->file = fopen(a->fileName,"w");
            if (a->file == NULL) {
                return(S_FAILURE);
            }
        }
        /*if (debug) printf("    flushToDestination: write %d bytes\n", bytesToWrite);*/

        /* It may take more than one fwrite to write all data */
        while (nBytes < bytesToWrite) {
            char *pBuf = (char *)(a->buf) + nBytes;
            /* Write block to file */
            nBytes += fwrite((const void *)pBuf, 1, bytesToWrite - nBytes, a->file);

            /* Return for error condition of file stream */
            if (ferror(a->file)) return(S_FAILURE);
        }

        // Now we need to update the file header to set # of records in file.
        // Don't bother updating the trailer position since we don't write the trailer's index
        fseek(a->file, 12, 0);
        uint32_t numBlocks = a->blknum;
        fwrite((const void *)(&numBlocks), 4, 1, a->file);
        if (ferror(a->file)) return(S_FAILURE);
if (debug) printf("    flushToDestination: write %u as record count to file header\n", numBlocks);

        // Go back to where we were
        fseek(a->file, a->bytesToFile + bytesToWrite, 0);

        if (force) {
            fflush(a->file);
        }
    }

    a->bytesToFile  += bytesToWrite;
    a->eventsToFile += a->eventsToBuf;

    if (debug) {
        printf("    flushToDestination: after last header written, Events written to:\n");
        printf("         cnt total (no dict) = %u\n", a->eventCount);
        printf("         file cnt total (dict) = %u\n", a->eventsToFile);
        printf("         internal buffer cnt (dict) = %u\n", a->eventsToBuf);
        printf("         current block cnt (dict) = %u\n", a->blkEvCount);
        printf("         bytes-written = %u\n", bytesToWrite);
        printf("         bytes-to-file = %" PRIu64 "\n", a->bytesToFile);
        printf("         block # = %u\n", a->blknum);
    }

    /* Everything flushed, nothing left in internal buffer, go back to top of buf */
    a->next = a->buf;
    a->left = a->bufSize;
    a->bytesToBuf = 0;
    a->eventsToBuf = 0;

    a->dataNext = a->dataBuf;
    a->dataLeft = a->bufRealSize;
    a->bytesToDataBuf = 0;
    a->blkEvCount = 0;

    if (wroteData != NULL) *wroteData = 1;

    return(S_SUCCESS);
}


/**
 * This routine splits the file being written to.
 * Does nothing when output destination is not a file.
 * It resets file variables, closes the old file, and opens the new.
 *
 * @param a  pointer to data structure
 *
 * @return  1  if file actually split
 * @return  0  if no error but file not split
 * @return -1  if mapped memory does not unmap;
 *             if failure to generate file name;
 *             if failure to close file;
 */
static int splitFile(EVFILE *a) {
    char *fname;
    int err=0, status=1;
    const int debug=0;

    
    /* Only makes sense when writing to files */
    if (a->rw != EV_WRITEFILE) {
        return(0);
    }

    // We need to end the file with an empty block header.
    // However, if resetBuffer (or flush) was just called,
    // a last block header will already exist.
    if (a->eventsToBuf > 0 || a->bytesToBuf < 1) {
if(debug) printf("    splitFile: write last empty header\n");
        err = writeNewHeader(a, 0, a->blknum, 0, 1);
        if (err != S_SUCCESS) {
            return (-1);
        }
    }
    //printf("    splitFile: call flushToDestination for file being closed\n");
    err = flushToDestination(a, 1, NULL);
    if (err) {
        return (-1);
    }

    /* Reset first-block & file values for reuse */
    a->blknum = 1;
    a->bytesToFile  = 0;
    a->eventsToFile = 0;
    a->wroteDictionary = 0;

    /* Close file */
    if (a->randomAccess) {
        err = munmap(a->mmapFile, a->mmapFileSize);
        if (err < 0) {
if (debug) printf("    splitFile: error unmapping memory, %s\n", strerror(errno));
            status = -1;
        }

        if (a->pTable != NULL) {
            free(a->pTable);
        }
    }
    else {
        if (a->file != NULL) {
            err = fclose(a->file);
        }
        if (err == EOF) {
if (debug) printf("    splitFile: error closing file, %s\n", strerror(errno));
            status = -1;
        }
    }

    /* Right now no file is open for writing */
    a->file = NULL;

    /* Create the next file's name */
    fname = evGenerateFileName(a, a->specifierCount, a->runNumber,
                               a->splitting, a->splitNumber++,
                               a->runType, a->streamId, a->streamCount, debug);
    if (fname == NULL) {
        return(-1);
    }

    if (a->fileName != NULL) {
        free(a->fileName);
    }
    
    a->fileName = fname;
    
if (debug) printf("    splitFile: generate next file name = %s\n", a->fileName);
    
    return(status);
}


/**
 * @addtogroup open
 * @{
 */


/**
 * This routine flushes any existing evio format data in an internal buffer
 * (written to with {@link #evWrite}) to the final destination
 * file/socket/buffer opened with routines {@link #evOpen}, {@link #evOpenBuffer},
 * or {@link #evOpenSocket}.
 * It also frees up the handle so it cannot be used any more without calling
 * {@link #evOpen} again.
 * Any data written is in evio version 4 format and any opened file is closed.
 * If reading, nothing is done.
 *
 * @param handle evio handle
 *
 * @return S_SUCCESS          if successful
 * @return S_FAILURE          if mapped memory does not unmap or write/close failed
 * @return S_EVFILE_TRUNC     if not enough room writing to a user-given buffer in {@link #evOpen}
 * @return S_EVFILE_BADHANDLE if bad handle arg or this function already called.
 * @return fclose error       if fclose error
 */
int evClose(int handle) {
    EVFILE *a;
    const int debug=0;
    int status = S_SUCCESS;


    if (handle < 1 || (size_t)handle > handleCount) {
        return(S_EVFILE_BADHANDLE);
    }

    /* For thread-safe function calls */
    handleLock(handle);

    /* Look up file struct from handle */
    a = handleList[handle-1];

    /* Check arg */
    if (a == NULL) {
        handleUnlock(handle);
        return(S_EVFILE_BADHANDLE);
    }

    int lockOn = a->lockingOn;
if(debug) printf("evClose: eventsToBuf = %u, bytesToBuf = %u\n", a->eventsToBuf, a->bytesToBuf);

    /* If file writing ... */
    if (a->rw == EV_WRITEFILE || a->rw == EV_WRITEPIPE || a->rw == EV_WRITESOCK) {
        // We need to end the file with an empty block header.
        // However, if resetBuffer (or flush) was just called,
        // a last block header will already exist.
        if (a->eventsToBuf > 0 || a->bytesToBuf < 1) {
if(debug) printf("evClose: write header, free bytes In Buffer = %d\n", (int)(a->bufSize - a->bytesToBuf));
            writeNewHeader(a, 0, a->blknum, 0, 1);
        }
        flushToDestination(a, 1, NULL);
    }
    else if ( a->rw == EV_WRITEBUF) {
        writeNewHeader(a, 0, a->blknum, 0, 1);
    }

    /* Close file */
    if (a->rw == EV_WRITEFILE || a->rw == EV_READFILE) {
        if (a->randomAccess) {
            status = munmap(a->mmapFile, a->mmapFileSize);
            if (status < 0) status = S_FAILURE;
            else status = S_SUCCESS;
        }
        else {
            if (a->file != NULL) status = fclose(a->file);
            if (status == EOF) status = S_FAILURE;
            else status = S_SUCCESS;
        }
    }
    /* Pipes requires special close */
    else if (a->rw == EV_READPIPE || a->rw == EV_WRITEPIPE) {
        if (a->file != NULL) {
            status = pclose(a->file);
            if (status < 0) status = S_FAILURE;
            else status = S_SUCCESS;
        }
    }

    /* Free up resources */
    freeEVFILE(a);

    /* Remove this handle from the list */
    getHandleLock();
    handleList[handle-1] = 0;
    getHandleUnlock();

    if (lockOn) handleUnlockUnconditional(handle);

    return(status);
}


/** @} */


/**
 * @defgroup getAndSet get and set routines
 * These routines handle getting and setting evio parameters.
 * @{
 */


/**
 * This routine gets the name of the file currently being written to and opened with {@link #evOpen},
 * Returned string may <b>NOT</b> be written into.
 *
 * @param handle evio handle
 * @param name caller's char array which gets filled with file name or NULL if there is no name.
 * @param maxLength length of array being passed in.
 *
 * @return S_SUCCESS          if successful.
 * @return S_FAILURE          if file name is NULL.
 * @return S_EVFILE_TRUNC     if char array too small to file filename and ending NULL.
 * @return S_EVFILE_BADMODE   if not opened for writing to file.
 * @return S_EVFILE_BADHANDLE if bad handle arg, NULL name arg, or maxLength arg &lt; 1.
 */
int evGetFileName(int handle, char *name, size_t maxLength) {

    EVFILE *a;
    int err = S_SUCCESS;

    if (handle < 1 || (size_t) handle > handleCount) {
        return (S_EVFILE_BADHANDLE);
    }

    /* For thread-safe function calls */
    handleLock(handle);

    /* Look up file struct (which contains block buffer) from handle */
    a = handleList[handle - 1];

    /* Check args */
    if (a == NULL || name == NULL || maxLength < 1) {
        handleUnlock(handle);
        return (S_EVFILE_BADHANDLE);
    }

    /* Need to be open for writing to file */
    if (a->rw != EV_WRITEFILE) {
        handleUnlock(handle);
        return (S_EVFILE_BADMODE);
    }

    if (a->fileName == NULL) {
        err = S_FAILURE;
    }
    else {
        /* If there's not enough room for filename + ending NULL, copy whatever there's room for */
        if (strlen(a->fileName) + 1 > maxLength) {
            strncpy(name, a->fileName, maxLength);
            err = S_EVFILE_TRUNC;
        }
        else {
            strcpy(name, a->fileName);
        }
    }

    handleUnlock(handle);
    return(err);
}


/**
 * This routine returns the number of bytes written into a buffer so
 * far when given a handle provided by calling {@link #evOpenBuffer}.
 * After the handle is closed, this no longer returns anything valid.
 * In this evio version, this routine doesn't tell you much since
 * all data isn't written out to buffer until {@link #evClose}.
 *
 * @param handle evio handle
 * @param length pointer to int which gets filled with number of bytes
 *               written to buffer so far
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADHANDLE if bad handle arg
 */
int evGetBufferLength(int handle, uint32_t *length)
{
    EVFILE *a;

    if (length == NULL)
        return (S_SUCCESS);

    if (handle < 1 || (size_t)handle > handleCount) {
        return(S_EVFILE_BADHANDLE);
    }

    /* For thread-safe function calls */
    handleLock(handle);

    /* Look up file struct (which contains block buffer) from handle */
    a = handleList[handle-1];

    /* Check arg */
    if (a == NULL) {
        handleUnlock(handle);
        return(S_EVFILE_BADHANDLE);
    }
    
    *length = a->rwBytesOut;

    handleUnlock(handle);
 
    return(S_SUCCESS);
}


/**
 * This routine changes various evio parameters used in reading and writing.<p>
 *
 * It changes the target block size (in 32-bit words) for writes if request = "B".
 * If setting block size fails, writes can still continue with original
 * block size. Minimum size = EV_BLOCKSIZE_MIN (1024) words.
 * Max size = EV_BLOCKSIZE_MAX (33554432) words.<p>
 *
 * It changes size of buffer (in 32-bit words) for writing to file/socket/pipe
 * if request = "W". Must be >= target block size (see above) + header (EV_HDSIZ).
 * Max size = EV_BLOCKSIZE_MAX + EV_HDSIZ (33554444) words.<p>
 *
 * It changes the maximum number of events/block if request = "N".
 * It only goes up to EV_EVENTS_MAX (100,000).
 * Used only in version 4.<p>
 *
 * It sets the run number used when auto naming while splitting files
 * being written to if request = "R".
 * Used only in version 4.<p>
 *
 * It sets the run type used when auto naming while splitting files
 * being written to if request = "T".
 * Used only in version 4.<p>
 *
 * It changes the number of bytes at which to split a file
 * being written to if request = "S". If unset with this function,
 * it defaults to EV_SPLIT_SIZE (2GB). NOTE: argp must point to
 * 64 bit integer (not 32 bit)!
 * Used only in version 4.<p>
 *
 * It sets the stream id used when auto naming files being written to if request = "M".
 * Used only in version 4.<p>
 *
 * It sets the total # of streams used in DAQ when auto naming files being written to
 * if request = "D". Used only in version 6.<p>
 *
 * It returns the version number if request = "V".<p>
 *
 * It returns a pointer to the EV_HDSIZ_V6 block header ints if request = "H".
 * This pointer must be freed by the caller to avoid a memory leak.<p>
 *
 * It returns the total number of events in a file/buffer
 * opened for reading or writing if request = "E". Includes any
 * event added with {@link #evWrite} call. Used only in version 4.<p>
 *
 * NOTE: all request strings are case insensitive. All version 4 commands to
 * version 3 files are ignored.
 *
 * @param handle  evio handle
 * @param request case independent string value of:
 * <OL type=1>
 * <LI>  "B"  for setting target block size for writing in words
 * <LI>  "W"  for setting writing (to file) internal buffer size in words
 * <LI>  "N"  for setting max # of events/block
 * <LI>  "R"  for setting run number (used in file splitting)
 * <LI>  "T"  for setting run type   (used in file splitting)
 * <LI>  "S"  for setting file split size in bytes
 * <LI>  "M"  for setting stream id  (used in auto file naming)
 * <LI>  "D"  for setting total # of streams in DAQ (used in auto file naming)
 * <LI>  "V"  for getting evio version #
 * <LI>  "H"  for getting 14 ints of block header info (only 8 valid for version < 6)
 * <LI>  "E"  for getting # of events in file/buffer
 * </OL>
 *
 * @param argp
 * <OL type=1>
 * <LI> pointer to uin32_t containing new block size in 32-bit words if request = B, or
 * <LI> pointer to uin32_t containing new buffer size in 32-bit words if request = W, or
 * <LI> pointer to uin32_t containing new max # of events/block if request = N, or
 * <LI> pointer to uin32_t containing run number if request = R, or
 * <LI> pointer to character containing run type if request = T, or
 * <LI> pointer to <b>uint64_t</b> containing max size in bytes of split file if request = S, or
 * <LI> pointer to uin32_t containing stream id if request = M, or
 * <LI> pointer to uin32_t containing stream count if request = D, or
 * <LI> pointer to int32_t returning version # if request = V, or
 * <LI> address of pointer to uint32_t returning pointer to 14
 *              uint32_t's of block header if request = H (only 8 valid for version < 6).
 *              This pointer must be freed by caller since it points to allocated memory.
 * <LI> pointer to uin32_t returning total # of original events in existing
 *              file/buffer when reading or appending if request = E
 * </OL>
 *
 * @return S_SUCCESS           if successful
 * @return S_FAILURE           if using sockets when request = E
 * @return S_EVFILE_BADARG     if request is NULL or argp is NULL
 * @return S_EVFILE_BADFILE    if file too small or problem reading file when request = E
 * @return S_EVFILE_BADHANDLE  if bad handle arg
 * @return S_EVFILE_ALLOCFAIL  if cannot allocate memory
 * @return S_EVFILE_UNXPTDEOF  if buffer too small when request = E
 * @return S_EVFILE_UNKOPTION  if unknown option specified in request arg
 * @return S_EVFILE_BADSIZEREQ  when setting block/buffer size - if currently reading,
 *                              have already written events with different block size,
 *                              or is smaller than min allowed size (header size + 1K),
 *                              or is larger than the max allowed size (5120000 words);
 *                              when setting max # events/blk - if val < 1
 * @return errno                if error in fseek, ftell when request = E
 */
int evIoctl(int handle, char *request, void *argp)
{
    EVFILE   *a;
    uint32_t *newBuf, *pHeader;
    int       err;
    const int debug=0;
    char     *runType;
    uint32_t  eventsMax, blockSize, bufferSize, runNumber;
    uint64_t  splitSize;

    if (handle < 1 || (size_t)handle > handleCount) {
        return(S_EVFILE_BADHANDLE);
    }

    /* For thread-safe function calls */
    handleLock(handle);

    /* Look up file struct from handle */
    a = handleList[handle-1];

    /* Check args */
    if (a == NULL) {
        handleUnlock(handle);
        return(S_EVFILE_BADHANDLE);
    }
    
    if (request == NULL) {
        handleUnlock(handle);
        return(S_EVFILE_BADARG);
    }

    switch (*request) {
        /*******************************/
        /* Specifing target block size */
        /*******************************/
        case 'b':
        case 'B':
if (debug) printf("evIoctl: trying to set block target size\n");
            /* Need to specify block size */
            if (argp == NULL) {
                handleUnlock(handle);
                return(S_EVFILE_BADARG);
            }

            /* Need to be writing not reading */
            if (a->rw != EV_WRITEFILE &&
                a->rw != EV_WRITEPIPE &&
                a->rw != EV_WRITESOCK &&
                a->rw != EV_WRITEBUF) {
                handleUnlock(handle);
                return(S_EVFILE_BADSIZEREQ);
            }

            /* If not appending AND events already written ... */
            if (a->append == 0 && (a->blknum != 2 || a->blkEvCount != 0)) {
if (debug) printf("evIoctl: error setting block target size, not appending and events already written\n");
                handleUnlock(handle);
                return(S_EVFILE_BADSIZEREQ);
            }
            /* Else appending AND events already appended ... */
            else if (a->append > 1) {
                handleUnlock(handle);
                return(S_EVFILE_BADSIZEREQ);
            }

            /* Read in requested target block size */
            blockSize = *(uint32_t *) argp;

            /* If there is no change, return success */
            if (blockSize == a->blkSizeTarget) {
                handleUnlock(handle);
                return(S_SUCCESS);
            }
            
            /* If it's too small, return error */
            if (blockSize < EV_BLOCKSIZE_MIN) {
if (debug) printf("evIoctl: error setting block target size, too small, must be >= %d\n", EV_BLOCKSIZE_MIN);
                handleUnlock(handle);
                return(S_EVFILE_BADSIZEREQ);
            }

            /* If it's too big, return error */
            if (blockSize > EV_BLOCKSIZE_MAX) {
if (debug) printf("evIoctl: error setting block target size, too big, must be <= %d\n", EV_BLOCKSIZE_MAX);
                handleUnlock(handle);
                return(S_EVFILE_BADSIZEREQ);
            }

            /* If we need a bigger buffer ... */
            if (blockSize + EV_HDSIZ > a->bufRealSize && a->rw != EV_WRITEBUF) {
                /* Allocate buffers' memory for increased block size. If this fails
                 * we can still (theoretically) continue with writing. */
if (debug) printf("evIoctl: increasing buffer size to %u words\n", (blockSize + EV_HDSIZ));
                newBuf = (uint32_t *) malloc(4*(blockSize + EV_HDSIZ_V6));
                uint32_t *newDataBuf = (uint32_t *) malloc(4*(blockSize + EV_HDSIZ));
                if (newBuf == NULL || newDataBuf == NULL) {
                    handleUnlock(handle);
                    return(S_EVFILE_ALLOCFAIL);
                }

                /* Free allocated memory for current buffers */
                free(a->buf);
                free(a->dataBuf);

                /* New buffers stored here */
                a->buf = newBuf;
                a->dataBuf = newDataBuf;
                /* Current header is at top of new buffer */
                a->currentHeader = a->buf;

                /* Initialize block header */
                initBlockHeader2(a->buf, 1);
                
                /* Remember size of new buffer */
                a->bufRealSize = a->bufSize = (blockSize + EV_HDSIZ_V6);
            }
            else if (blockSize + EV_HDSIZ_V6 > a->bufSize && a->rw != EV_WRITEBUF) {
                /* Remember how much of buffer is actually being used */
                a->bufSize = blockSize + EV_HDSIZ_V6;
            }

if (debug) printf("evIoctl: block size = %u words\n", blockSize);

            /* Reset some file struct members */

            /* Recalculate how many words are left to write in block */
            a->left = blockSize - EV_HDSIZ_V6;
            /* Store new target block size (final size,
             * a->blksiz, may be larger or smaller) */
            a->blkSizeTarget = blockSize;
            /* Next word to write is right after header */
            a->next = a->buf + EV_HDSIZ_V6;

            a->dataNext = a->dataBuf;
            a->dataLeft = blockSize + EV_HDSIZ_V6;

            break;

            /****************************************************/
            /* Specifing buffer size for writing file/sock/pipe */
            /****************************************************/
        case 'w':
        case 'W':
            /* Need to specify buffer size */
            if (argp == NULL) {
                handleUnlock(handle);
                return(S_EVFILE_BADARG);
            }

            /* Need to be writing file/sock/pipe, not reading */
            if (a->rw != EV_WRITEFILE &&
                a->rw != EV_WRITEPIPE &&
                a->rw != EV_WRITESOCK) {
                handleUnlock(handle);
                return(S_EVFILE_BADSIZEREQ);
            }

            /* If not appending AND events already written ... */
            if (a->append == 0 && (a->blknum != 2 || a->blkEvCount != 0)) {
if (debug) printf("evIoctl: error setting buffer size, not appending and events already written\n");
                handleUnlock(handle);
                return(S_EVFILE_BADSIZEREQ);
            }
            /* Else appending AND events already appended ... */
            else if (a->append > 1) {
                handleUnlock(handle);
                return(S_EVFILE_BADSIZEREQ);
            }

            /* Read in requested buffer size */
            bufferSize = *(uint32_t *) argp;

            /* If there is no change, return success */
            if (bufferSize == a->bufSize) {
                handleUnlock(handle);
                return(S_SUCCESS);
            }

            /* If it's too small, return error */
            if (bufferSize < a->blkSizeTarget + EV_HDSIZ_V6) {
if (debug) printf("evIoctl: error setting buffer size, too small, must be >= %u\n", (a->blkSizeTarget + EV_HDSIZ));
                handleUnlock(handle);
                return(S_EVFILE_BADSIZEREQ);
            }

            /* If it's too big, return error */
            if (bufferSize > EV_BLOCKSIZE_MAX) {
if (debug) printf("evIoctl: error setting block target size, too large, must be <= %d\n", EV_BLOCKSIZE_MAX);
                handleUnlock(handle);
                return(S_EVFILE_BADSIZEREQ);
            }

            /* If we need a bigger buffer ... */
            if (bufferSize > a->bufRealSize && a->rw != EV_WRITEBUF) {
                /* Allocate buffer memory for increased size. If this fails
                 * we can still (theoretically) continue with writing. */
if (debug) printf("evIoctl: increasing internal buffer size to %u words\n", bufferSize);

                /* New buffers stored here */
                newBuf = (uint32_t *) malloc(4*bufferSize);
                uint32_t *newDataBuf = (uint32_t *) malloc(4*bufferSize);
                if (newBuf == NULL || newDataBuf == NULL) {
                    handleUnlock(handle);
                    return(S_EVFILE_ALLOCFAIL);
                }

                /* Free allocated memory for current block */
                free(a->buf);
                free(a->dataBuf);

                /* New buffer stored here */
                a->buf = newBuf;
                a->dataBuf = newDataBuf;
                /* Current header is at top of new buffer */
                a->currentHeader = a->buf;

                /* Remember size of new buffer */
                a->bufRealSize = bufferSize;

                /* Initialize block header */
                initBlockHeader2(a->buf, 1);
            }
            else if (debug) {
                printf("evIoctl: decreasing internal buffer size to %u words\n", bufferSize);
            }
            
            /* Reset some file struct members */

            /* Remember how much of buffer is actually being used */
            a->bufSize = bufferSize;

            /* Recalculate how many words are left to write in block */
            a->left = bufferSize - EV_HDSIZ_V6;
            /* Next word to write is right after header */
            a->next = a->buf + EV_HDSIZ_V6;

            a->dataNext = a->dataBuf;
            a->dataLeft = bufferSize + EV_HDSIZ_V6;

            break;
        

        /**************************/
        /* Getting version number */
        /**************************/
        case 'v':
        case 'V':
            /* Need to pass version back in pointer to int */
            if (argp == NULL) {
                handleUnlock(handle);
                return(S_EVFILE_BADARG);
            }

            *((int32_t *) argp) = a->buf[EV_HD_VER] & EV_VERSION_MASK;
            
            break;

        /*****************************/
        /* Getting block header info */
        /*****************************/
        case 'h':
        case 'H':
            /* Need to pass header data back in allocated int array */
            if (argp == NULL) {
                handleUnlock(handle);
                return(S_EVFILE_BADARG);
            }

            pHeader = (uint32_t *)malloc(EV_HDSIZ_V6*sizeof(uint32_t));
            if (pHeader == NULL) {
                handleUnlock(handle);
                return(S_EVFILE_ALLOCFAIL);
            }

            /* If reading ... */
            if (a->rw == EV_READFILE ||
                a->rw == EV_READPIPE ||
                a->rw == EV_READSOCK ||
                a->rw == EV_READBUF) {

                if (a->version > 4) {
                    memcpy((void *) pHeader, (const void *) a->buf, EV_HDSIZ_V6 * sizeof(uint32_t));
                }
                else {
                    memcpy((void *) pHeader, (const void *) a->buf, EV_HDSIZ * sizeof(uint32_t));
                }
            }
            else {
                if (a->version > 4) {
                    memcpy((void *) pHeader, (const void *) a->currentHeader, EV_HDSIZ_V6 * sizeof(uint32_t));
                }
                else {
                    memcpy((void *) pHeader, (const void *) a->currentHeader, EV_HDSIZ * sizeof(uint32_t));
                }
            }
            *((uint32_t **) argp) = pHeader;

            printf("evIoctl: current block # = %u\n", a->blknum);

            break;

        /**********************************************/
        /* Setting maximum number of events per block */
        /**********************************************/
        case 'n':
        case 'N':
            /* Need to specify # of events */
            if (argp == NULL) {
                handleUnlock(handle);
                return(S_EVFILE_BADARG);
            }
            
            eventsMax = *(uint32_t *) argp;
            if (eventsMax < 1) {
                handleUnlock(handle);
                return(S_EVFILE_BADSIZEREQ);
            }

            if (eventsMax > EV_EVENTS_MAX) {
                eventsMax = EV_EVENTS_MAX;
            }

            if ((a->eventLengths != NULL) && (eventsMax > a->blkEvMax)) {
                a->eventLengths = (uint32_t *) realloc(a->eventLengths, 4*eventsMax);
            }
            
            a->blkEvMax = eventsMax;
            break;

        /**************************************************/
        /* Setting number of bytes at which to split file */
        /**************************************************/
        case 's':
        case 'S':
            /* Need to specify split size */
            if (argp == NULL) {
                handleUnlock(handle);
                return(S_EVFILE_BADARG);
            }
            
            splitSize = *(uint64_t *) argp;

            /* Make sure it is at least 32 bytes below the max file size
             * on this platform. The algorithm used to split is only
             * accurate to within +1 block header. */

            /* If this is a 32 bit operating system ... */
            if (8*sizeof(0L) == 32) {
                uint64_t max = 0x00000000ffffffff;
                if (splitSize > max - 32) {
                    splitSize = max - 32;
                }
            }
            /* If this is a 64 bit operating system or higher ... */
            else {
                uint64_t max = UINT64_MAX;
                if (splitSize > max - 32) {
                    splitSize = max - 32;
                }
            }

            /* Smallest possible evio format file = 10 32-bit ints.
             * Must also be bigger than a single buffer? */
            if (splitSize < 4*10) {
if (debug) printf("evIoctl: split file size is too small! (%" PRIu64 " bytes), must be min 40\n", splitSize);
                handleUnlock(handle);
                return(S_EVFILE_BADSIZEREQ);
            }
            
            a->split = splitSize;
if (debug) printf("evIoctl: split file at %" PRIu64 " (0x%" PRIx64 ") bytes\n", splitSize, splitSize);
            break;

        /************************************************/
        /* Setting run number for file splitting/naming */
        /************************************************/
        case 'r':
        case 'R':
            /* Need to specify run # */
            if (argp == NULL) {
                handleUnlock(handle);
                return(S_EVFILE_BADARG);
            }
            
            runNumber = *(uint32_t *) argp;
            if (runNumber < 1) {
                handleUnlock(handle);
                return(S_EVFILE_BADSIZEREQ);
            }

            a->runNumber = runNumber;
            break;

        /************************************************/
        /* Setting run type for file splitting/naming   */
        /************************************************/
        case 't':
        case 'T':
            /* Need to specify run type */
            if (argp == NULL) {
                runType = NULL;
            }
            else {
                runType = strdup((char *) argp);
                if (runType == NULL) {
                    handleUnlock(handle);
                    return(S_EVFILE_BADSIZEREQ);
                }
            }
            
            a->runType = runType;
            break;

            /************************************************/
            /* Setting stream id for file naming            */
            /************************************************/
        case 'm':
        case 'M':
            /* Need to specify stream id */
            if (argp == NULL) {
                handleUnlock(handle);
                return(S_EVFILE_BADARG);
            }

            a->streamId = *(uint32_t *) argp;
            break;

            /************************************************/
            /* Setting total stream count for file naming            */
            /************************************************/
        case 'd':
        case 'D':
            /* Need to specify stream count */
            if (argp == NULL) {
                handleUnlock(handle);
                return(S_EVFILE_BADARG);
            }

            a->streamCount = *(uint32_t *) argp;
            break;

        /****************************/
        /* Getting number of events */
        /****************************/
        case 'e':
        case 'E':
            /* Need to pass # of events bank in pointer to int */
            if (argp == NULL) {
                handleUnlock(handle);
                return(S_EVFILE_BADARG);
            }

            err = getEventCount(a, (uint32_t *) argp);
            if (err != S_SUCCESS) {
                handleUnlock(handle);
                return(err);
            }

            break;

        default:
            handleUnlock(handle);
            return(S_EVFILE_UNKOPTION);
    }

    handleUnlock(handle);

    return(S_SUCCESS);
}



/**
 * This routine gets an array of event pointers if the handle was opened in
 * random access mode. User must not change the pointers in the array or the
 * data being pointed to (hence the consts in the second parameter).
 *
 * @param handle  evio handle
 * @param table   pointer to array of uint32_t pointers which gets filled
 *                with an array of event pointers.
 *                If this arg = NULL, error is returned.
 * @param len     pointer to int which gets filled with the number of
 *                pointers in the array. If this arg = NULL, error is returned.
 *                   
 * @return S_SUCCESS           if successful
 * @return S_EVFILE_BADMODE    if handle not opened in random access mode
 * @return S_EVFILE_BADARG     if table or len arg(s) is NULL
 * @return S_EVFILE_BADHANDLE  if bad handle arg
 */
int evGetRandomAccessTable(int handle, uint32_t *** const table, uint32_t *len) {
    EVFILE *a;

    
    if (handle < 1 || (size_t)handle > handleCount) {
        return(S_EVFILE_BADHANDLE);
    }

    /* For thread-safe function calls */
    handleLock(handle);

    /* Look up file struct from handle */
    a = handleList[handle-1];

    /* Check args */
    if (a == NULL) {
        handleUnlock(handle);
        return(S_EVFILE_BADHANDLE);
    }

    if (table == NULL || len == NULL) {
        handleUnlock(handle);
        return (S_EVFILE_BADARG);
    }

    /* Must be in random access mode */
    if (!a->randomAccess) {
        handleUnlock(handle);
        return(S_EVFILE_BADMODE);
    }
            
    *table = a->pTable;
    *len = a->eventCount;

    handleUnlock(handle);

    return S_SUCCESS;
}



/**
 * This routine gets the dictionary associated with this handle
 * if there is any. Memory must be freed by caller if a dictionary
 * was successfully returned.
 *
 * @param handle     evio handle
 * @param dictionary pointer to string which gets filled with dictionary
 *                   string (xml) if it exists, else gets filled with NULL.
 *                   Memory for dictionary allocated here, must be freed by
 *                   caller.
 * @param len        pointer to int which gets filled with dictionary
 *                   string length (# chars) if there is one, else filled with 0.
 *                   If this arg = NULL, no len is returned.
 *
 * @return S_SUCCESS           if successful
 * @return S_EVFILE_BADARG     if dictionary arg is NULL
 * @return S_EVFILE_ALLOCFAIL  if cannot allocate memory
 * @return S_EVFILE_BADHANDLE  if bad handle arg
 */
int evGetDictionary(int handle, char **dictionary, uint32_t *len) {
    EVFILE *a;
    char *dictCpy;

    
    if (handle < 1 || (size_t)handle > handleCount) {
        return(S_EVFILE_BADHANDLE);
    }

    /* For thread-safe function calls */
    handleLock(handle);

    /* Look up file struct from handle */
    a = handleList[handle-1];

    /* Check args */
    if (a == NULL) {
        handleUnlock(handle);
        return(S_EVFILE_BADHANDLE);
    }

    if (dictionary == NULL) {
        handleUnlock(handle);
        return (S_EVFILE_BADARG);
    }

    /* If we have a dictionary ... */
    if (a->dictionary != NULL) {
        /* Copy it and return it */
        dictCpy = strdup(a->dictionary);
        if (dictCpy == NULL) {
            handleUnlock(handle);
            return(S_EVFILE_ALLOCFAIL);
        }
        *dictionary = dictCpy;

        /* Send back the length if caller wants it */
        if (len != NULL) {
            *len = (uint32_t ) strlen(a->dictionary);
        }
    }
    else {
        *dictionary = NULL;
        if (len != NULL) {
            *len = 0;
        }
    }
    
    handleUnlock(handle);

    return S_SUCCESS;
}


/** @} */


/**
 * @addtogroup write
 * @{
 */


/**
 * This routine writes an array of strings, in evio format, into the given buffer.
 * This does NOT include any bank, segment, or tagsegment header.
 * The length of the written data in bytes is returned in the "dataLen" arg.
 * The written data is endian independent.
 *
 * @param buffer      buffer in which to place the evio format string data
 * @param bufLen      length of available room in which to write data in buffer in bytes
 * @param strings     array of strings to write as data
 * @param stringCount number of strings in string array
 * @param dataLen     pointer to int which gets filled the length of the written data in bytes
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADARG    if buffer or dataLen arg is NULL, bufLen < 4, stringCount < 0,
 *                            or a string is NULL
 * @return S_EVFILE_TRUNC     if not enough room in buffer
 */
int evStringsToBuf(uint32_t *buffer, int bufLen, char **strings, int stringCount, int *dataLen) {

    size_t len;
    char *buf = (char *) buffer;
    int i, size=0, padChars, pads[4] = {4,3,2,1};

    if (buffer == NULL || dataLen == NULL || bufLen < 4 || stringCount < 0) {
        return(S_EVFILE_BADARG);
    }

    if (strings == NULL || stringCount == 0) {
        return(S_SUCCESS);
    }

    /* Find out how much space we need. */
    for (i=0; i < stringCount; i++) {
        if (strings[i] == NULL) {
            return(S_EVFILE_BADARG);
        }
        size += strlen(strings[i]) + 1;
    }
    padChars = pads[size%4];
    size += padChars;

    if (size > bufLen) {
        return(S_EVFILE_TRUNC);
    }

    for (i=0; i < stringCount; i++) {
        len = strlen(strings[i]);
        memcpy((void *)buf, (const void *)strings[i], len);
        buf[len] = '\0';
        buf += len + 1;
    }

    /* Add any necessary padding to 4 byte boundaries.
       IMPORTANT: There must be at least one '\004'
       character at the end. This distinguishes evio
       string array version from earlier version. */
    /*fprintf(stderr,"evGetNewBuffer: read %d bytes from file\n", (int)bytesToRead);*/
    for (i=0; i < padChars; i++) {
        buf[i]='\4';
    }

    *dataLen = size;

    return(S_SUCCESS);
}


/** @} */



/**
 * @addtogroup read
 * @{
 */


/**
 * This routine adds a string to an array of strings.
 * If no array has been allocated, it's created.
 * If the existing array is too small, its size is doubled and the pointer is updated.
 * Existing strings are moved into the newly created array.<p>
 *
 * Array pointed to in pArray needs to be freed by the caller.
 * All strings contained in the array also need to be freed by the caller.
 *
 * @param pArray          address of array of strings
 *                        (array will be changed if more space was needed).
 * @param str             string to add to array
 * @param pTotalCount     address of total number of array elements
 *                        (number of elements will be changed if more space was needed)
 * @param pvalidStrCount  address of number of array elements with valid strings
 *                        (number will increase by one if no error)
 */
static int addStringToArray(char ***pArray, char *str, int *pTotalCount, int *pvalidStrCount) {

    int i, doubleLen;
    int totCount = *pTotalCount;
    int strCount = *pvalidStrCount;
    char **doubleArray, **oldArray = *pArray;

    if (str == NULL) {
        return(S_EVFILE_BADARG);
    }

    /* If first time a string's been added, start with space for 100 strings. */
    if (oldArray == NULL) {
        oldArray = (char **)calloc(1, 100*sizeof(char *));
        if (oldArray == NULL) {
            return(S_EVFILE_ALLOCFAIL);
        }
        *pArray = oldArray;
        *pTotalCount = 100;
        strCount = 0;
        totCount = 100;
    }

    /* If we don't need more space, place string in existing array and return */
    if (strCount < totCount) {
        str = strdup(str);
        if (str == NULL) {
            return(S_EVFILE_ALLOCFAIL);
        }
        oldArray[strCount] = str;
        *pvalidStrCount = strCount + 1;
        return(S_SUCCESS);
    }

    /* If here, we need more space so double capacity */
    doubleLen = 2 * strCount;
    doubleArray = (char **)calloc(1, doubleLen*sizeof(char *));
    if (doubleArray == NULL) {
        return(S_EVFILE_ALLOCFAIL);
    }

    /* Copy over all existing strings into new, larger array */
    for (i=0; i < strCount; i++) {
        doubleArray[i] = oldArray[i];
    }

    /* Add string */
    str = strdup(str);
    if (str == NULL) {
        return(S_EVFILE_ALLOCFAIL);
    }
    doubleArray[strCount] = str;

    free(oldArray);
    *pArray = doubleArray;
    *pTotalCount = doubleLen;
    *pvalidStrCount = strCount + 1;

    return(S_SUCCESS);
}


/**
 * This routine unpacks/parses an evio format buffer containing strings
 * into an array of strings. Evio string data is endian independent.
 * The array pointed to in pStrArray is allocated here and needs to be freed by the caller.
 * All strings contained in the array also need to be freed by the caller.
 *
 * @param buffer      buffer containing evio format string data (NOT including header)
 * @param bufLen      length of string data in bytes
 * @param pStrArray   address of string array which gets filled parsed strings
 * @param strCount    pointer to int which get filled with the number of strings in string array
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADARG    if buffer, pStrArray, or strCount arg is NULL, or if bufLen < 4.
 * @return S_EVFILE_TRUNC     if not enough room in buffer
 * @return S_FAILURE          if buffer not in proper evio format
 */
int evBufToStrings(char *buffer, int bufLen, char ***pStrArray, int *strCount) {

    int i, j, totalCount = 0, stringCount = 0;
    int badStringFormat = 1, nullCount = 0, noEnding4 = 0;
    char c, *strStart = buffer, *pChar = buffer;
    char **strArray = NULL;

    if (buffer == NULL    || bufLen < 4 ||
        pStrArray == NULL || strCount == NULL) {
        return(S_EVFILE_BADARG);
    }

    /*
       Each string is terminated with a null (char val = 0)
       and in addition, the end is padded by ASCII 4's (char val = 4).
       However, in the legacy versions of evio, there is only one
       null-terminated string and anything as padding. To accommodate legacy evio, if
       there is not an ending ASCII value 4, anything past the first null is ignored.
       After doing so, split at the nulls.
     */

    if (buffer[bufLen - 1] != '\4') {
        noEnding4 = 1;
    }

    for (i=0; i < bufLen; i++) {
        /* look at each character */
        c = *pChar++;

        /* If char is a NULL */
        if (c == '\0') {
            /* One string for each NULL */
            nullCount++;

            /* String starts where we started looking from, strStart, and ends at this NULL */
            addStringToArray(&strArray, strStart, &totalCount, &stringCount);
/*printf("  add %s\n", strStart);*/

            /* Start looking for next string right after this NULL */
            strStart = pChar;

            /* If evio v1, 2 or 3, only 1 null terminated string exists
               and padding is just junk or nonexistent. */
            if (noEnding4) {
                badStringFormat = 0;
                break;
            }
        }
        /* Look for any non-printing/control characters (not including NULL)
           and end the string there. Allow tab and newline whitespace. */
        else if ((c < 32 || c > 126) && c != 9 && c != 10) {
/*printf("unpackRawBytesToStrings: found non-printing char = 0x%x at i = %d\n", c, i);*/
            if (nullCount < 1) {
                /* Getting garbage before first NULL */
/*printf("BAD FORMAT 1, no null when garbage char found\n");*/
                break;
            }

            /* Already have at least one NULL & therefore a String.
               Now we have junk or non-printing ascii which is
               possibly the ending 4. */

            /* If we have a 4, investigate further to see if format
               is entirely valid. */
            if (c == '\4') {
                /* How many more chars are there? */
                int charsLeft = bufLen - (i+1);

                /* Should be no more than 3 additional 4's before the end */
                if (charsLeft > 3) {
/*printf("BAD FORMAT 2, too many chars, %d, after 4\n", charsLeft);*/
                    break;
                }
                else {
                    int error = 0;
                    /* Check to see if remaining chars are all 4's. If not, bad. */
                    for (j=1; j <= charsLeft; j++) {
                        c = buffer[i+j];
                        if (c != '\004') {
/*printf("BAD FORMAT 3, padding chars are NOT all 4's\n");*/
                            error = 1;
                            break;
                        }
                    }
                    if (error) break;
                    badStringFormat = 0;
                    break;
                }
            }
            else {
/*printf("BAD FORMAT 4, got bad char, ascii val = %d\n", c);*/
                break;
            }
        }
    }

    /* If the format is bad, free any allocated memory and return an error. */
    if (badStringFormat) {
        /* Have we allocated anything yet? If so, get rid of it since
              we ran into a badly formatted  part of the buffer. */
        if (nullCount > 0) {
            /* Free strings */
            for (i=0; i < nullCount; i++) {
                free(strArray[i]);
            }
            /* Free array */
            free(strArray);
        }

        *strCount = 0;
        *pStrArray = NULL;
        return(S_FAILURE);
    }

    *strCount  = nullCount;
    *pStrArray = strArray;

/*printf("  split into %d strings\n", nullCount);*/
    return(S_SUCCESS);
}


/** @} */



/**
 * @addtogroup getAndSet
 * @{
 */

/**
 * This routine returns a string representation of an evio type.
 *
 * @param type numerical value of an evio type
 * @return string representation of the given type
 */
const char *evGetTypename(int type) {

    switch (type) {

        case 0x0:
            return("unknown32");
            break;

        case 0x1:
            return("uint32");
            break;

        case 0x2:
            return("float32");
            break;

        case 0x3:
            return("string");
            break;

        case 0x4:
            return("int16");
            break;

        case 0x5:
            return("uint16");
            break;

        case 0x6:
            return("int8");
            break;

        case 0x7:
            return("uint8");
            break;

        case 0x8:
            return("float64");
            break;

        case 0x9:
            return("int64");
            break;

        case 0xa:
            return("uint64");
            break;

        case 0xb:
            return("int32");
            break;

        case 0xe:
        case 0x10:
            return("bank");
            break;

        case 0xd:
        case 0x20:
            return("segment");
            break;

        case 0xc:
            return("tagsegment");
            break;

        case 0xf:
            return("composite");
            break;

        default:
            return("unknown");
            break;
    }
}



/**
 * This routine return true (1) if given type is a container, else returns false (0).
 *
 * @param type numerical value of an evio type (eg. type = 0x10 = bank, returns 1)
 * @return 1 if given type is a container, else 0.
 */
int evIsContainer(int type) {

    switch (type) {
    
        case (0xc):
        case (0xd):
        case (0xe):
        case (0x10):
        case (0x20):
            return(1);

        default:
            return(0);
    }
}


/**
 * This routine returns a string describing the given error value.
 * The returned string is a static char array. This means it is not
 * thread-safe and will be overwritten on subsequent calls.
 *
 * @param error error condition
 *
 * @returns error string
 */
char *evPerror(int error) {

    static char temp[256];

    switch(error) {

        case S_SUCCESS:
            sprintf(temp, "S_SUCCESS:  action completed successfully\n");
            break;

        case S_FAILURE:
            sprintf(temp, "S_FAILURE:  action failed\n");
            break;

        case S_EVFILE:
            sprintf(temp, "S_EVFILE:  evfile.msg event file I/O\n");
            break;

        case S_EVFILE_TRUNC:
            sprintf(temp, "S_EVFILE_TRUNC:  event truncated, insufficient buffer space\n");
            break;

        case S_EVFILE_BADBLOCK:
            sprintf(temp, "S_EVFILE_BADBLOCK:  bad block (header) number\n");
            break;

        case S_EVFILE_BADHANDLE:
            sprintf(temp, "S_EVFILE_BADHANDLE:  bad handle (closed?) or no memory to create new handle\n");
            break;

        case S_EVFILE_BADFILE:
            sprintf(temp, "S_EVFILE_BADFILE:  bad file format\n");
            break;

        case S_EVFILE_BADARG:
            sprintf(temp, "S_EVFILE_BADARG:  invalid function argument\n");
            break;

        case S_EVFILE_ALLOCFAIL:
            sprintf(temp, "S_EVFILE_ALLOCFAIL:  failed to allocate memory\n");
            break;

        case S_EVFILE_UNKOPTION:
            sprintf(temp, "S_EVFILE_UNKOPTION:  unknown option specified\n");
            break;
        
        case S_EVFILE_UNXPTDEOF:
            sprintf(temp, "S_EVFILE_UNXPTDEOF:  unexpected end-of-file or end-of-valid_data while reading\n");
            break;

        case S_EVFILE_BADSIZEREQ:
            sprintf(temp, "S_EVFILE_BADSIZEREQ:  invalid buffer size request to evIoct\n");
            break;

        case S_EVFILE_BADMODE:
            sprintf(temp, "S_EVFILE_BADMODE:  invalid operation for current evOpen() mode\n");
            break;

        default:
            sprintf(temp, "?evPerror...no such error: %d\n",error);
            break;
    }

    return(temp);
}


/** @} */


