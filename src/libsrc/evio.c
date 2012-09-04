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
 *         14) Additional options for evIoctl including getting # of events in file/buffer
 *
 * Routines:
 * ---------
 * 
 * int  evOpen                 (char *filename, char *flags, int *handle)
 * int  evOpenBuffer           (char *buffer, int bufLen, char *flags, int *handle)
 * int  evOpenSocket           (int sockFd, char *flags, int *handle)
 * int  evRead                 (int handle, uint32_t *buffer, size_t buflen)
 * int  evReadAlloc            (int handle, uint32_t **buffer, uint64_t *buflen)
 * int  evReadNoCopy           (int handle, const uint32_t **buffer, uint64_t *buflen)
 * int  evReadRandom           (int handle, const uint32_t **pEvent, size_t eventNumber)
 * int  evWrite                (int handle, const uint32_t *buffer)
 * int  evIoctl                (int handle, char *request, void *argp)
 * int  evClose                (int handle)
 * int  evGetBufferLength      (int handle, uint64_t *length)
 * int  evGetRandomAccessTable (int handle, const uint32_t *** const table, uint64_t *len)
 * int  evGetDictionary        (int handle, char **dictionary, int *len)
 * int  evWriteDictionary      (int handle, char *xmlDictionary)
 * int  evIsContainer          (int type)
 * char *evPerror              (int error)
 * const char *evGetTypename   (int type)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <evio.h>

/**
 * This structure contains information about file
 * opened for either reading or writing.
 */
typedef struct evfilestruct {
  FILE    *file;         /**< pointer to file. */
  uint32_t *buf;         /**< pointer to buffer of block being read/written. */
  uint32_t *next;        /**< pointer to next word in block to be read/written. */
  uint32_t left;         /**< # of valid 32 bit unread/unwritten words in block. */
  uint32_t blksiz;       /**< size of block in 32 bit words - v3 or
                          *   size of actual data - v4. */
  uint32_t blknum;       /**< block number of block being read/written (block #s start at 1). */
  int      blkNumDiff;   /**< When reading, the difference between blknum read in and
                          *   the expected (sequential) value. Used in debug message. */
  int      rw;           /**< are we reading, writing, piping? */
  int      magic;        /**< magic number. */

  int      byte_swapped; /**< bytes do NOT need swapping = 0 else 1 */

  /* new struct members for version 4 */
  int      version;      /**< evio version number. */
  int      append;       /**< open buffer or file for writing in append mode. */
  uint32_t eventCount;   /**< current number of events in (or written to) file/buffer
                               NOT including dictionary. */

  /* buffer stuff */
  char     *rwBuf;         /**< pointer to buffer if reading/writing from/to buffer. */
  uint32_t  rwBufSize;     /**< size of rwBuf buffer in bytes. */
  uint32_t  rwBytesOut;    /**< number of bytes written to rwBuf with evWrite,
                            *   but not necessary in the actual buffer yet. */
  uint32_t  rwBytesUsed;   /**< number of bytes read/written from/to rwBuf so far
                            * (after each flush) i.e. # bytes in buffer already used.*/

  /* socket stuff */
  int   sockFd;          /**< socket file descriptor if reading/writing from/to socket. */

  /* block stuff */
  uint32_t  blkSizeTarget; /**< target size of block in 32 bit words (including block header). */
  uint32_t  bufSize;       /**< size of block buffer (buf) in 32 bit words. */
  uint32_t  eventsMax;     /**< max number of events per block. */
  uint32_t  blkEvCount;    /**< number of events written to block so far. */
  int       isLastBlock;   /**< 1 if buf contains last block of file/sock/buf, else 0. */
  int       lastBlockOut;  /**< 1 if evFlush called and last block written (for buf/file), else 0. */

  /* randomAcess stuff */
  int        randomAccess; /**< if true, use random access file/buffer reading. */
  size_t     mmapFileSize; /**< size of mapped file in bytes. */
  uint32_t  *mmapFile;     /**< pointer to memory mapped file. */
  uint32_t  **pTable;      /**< array of pointers to events in memory mapped file or buffer. */

  /* dictionary */
  int   wroteDictionary;   /**< dictionary already written out. */
  char *dictionary;        /**< xml format dictionary to either read or write. */

} EVFILE;


/* A few items to make the code more readable */
#define EV_READFILE     0
#define EV_READPIPE     1
#define EV_READSOCK     2
#define EV_READBUF      3

#define EV_WRITEFILE    4
#define EV_WRITEPIPE    5
#define EV_WRITESOCK    6
#define EV_WRITEBUF     7


/** Version 3's fixed block size in 32 bit words */
#define EV_BLOCKSIZE_V3 8192

/** Version 4's target block size in 32 bit words (2MB) */
#define EV_BLOCKSIZE_V4 512000

/** Number used to determine data endian */
#define EV_MAGIC 0xc0da0100

/** Minimum block size allowed if size reset */
#define EV_BLOCKSIZE_MIN (EV_HDSIZ + 1024)

/** In version 4, lowest 8 bits are version, rest is bit info */
#define EV_VERSION_MASK 0xFF

/** In version 4, dictionary presence is 9th bit in version/info word */
#define EV_DICTIONARY_MASK 0x100

/** In version 4, "last block" is 10th bit in version/info word */
#define EV_LASTBLOCK_MASK 0x200

/** In version 4, max number of events per block */
#define EV_EVENTS_MAX 10000

/**
 * Evio block header, also known as a physical record header.
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
 * _______________________________________
 * |             Block Size              |
 * |_____________________________________|
 * |            Block Number             |
 * |_____________________________________|
 * |           Header Size = 8           |
 * |_____________________________________|
 * |               Start                 |
 * |_____________________________________|
 * |                Used                 |
 * |_____________________________________|
 * |              Version                |
 * |_____________________________________|
 * |              Reserved               |
 * |_____________________________________|
 * |              Magic #                |
 * |_____________________________________|
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
 * Evio block header, version 4+:
 * ################################
 *
 * MSB(31)                          LSB(0)
 * <---  32 bits ------------------------>
 * _______________________________________
 * |             Block Size              |
 * |_____________________________________|
 * |            Block Number             |
 * |_____________________________________|
 * |          Header Length = 8          |
 * |_____________________________________|
 * |             Event Count             |
 * |_____________________________________|
 * |              Reserved               |
 * |_____________________________________|
 * |          Bit info         | Version |
 * |_____________________________________|
 * |              Reserved               |
 * |_____________________________________|
 * |            Magic Number             |
 * |_____________________________________|
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
 *
 * Bit info (24 bits) has the first (lowest) 2 bits taken:
 *   Bit 0 = true if dictionary is included (relevant for first block only)
 *   Bit 1 = true if this block is the last block in file or network transmission
 *
 *
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
 * _______________________________________
 * |  tag    | type |    length          | --> tagsegment header
 * |_________|______|____________________|
 * |        Data Format String           |
 * |                                     |
 * |_____________________________________|
 * |              length                 | \
 * |_____________________________________|  \  bank header
 * |       tag      |  type   |   num    |  /
 * |________________|_________|__________| /
 * |               Data                  |
 * |                                     |
 * |_____________________________________|
 *
 *   The beginning tagsegment is a normal evio tagsegment containing a string
 *   (type = 0x3). Currently its type and tag are not used - at least not for
 *   data formatting.
 *   The bank is a normal evio bank header with data following.
 *   The format string is used to read/write this data so that takes care of any
 *   padding that may exist. As with the tagsegment, the tags and type are ignored.
 * 
 */

#define EV_HD_BLKSIZ 0	/**< Position of blk hdr word for size of block in 32-bit words. */
#define EV_HD_BLKNUM 1	/**< Position of blk hdr word for block number, starting at 0. */
#define EV_HD_HDSIZ  2	/**< Position of blk hdr word for size of header in 32-bit words (=8). */
#define EV_HD_COUNT  3  /**< Position of blk hdr word for number of events in block (version 4+). */
#define EV_HD_START  3  /**< Position of blk hdr word for first start of event in this block (ver 1-3). */
#define EV_HD_USED   4	/**< Position of blk hdr word for number of words used in block (<= BLKSIZ) (ver 1-3). */
#define EV_HD_RESVD1 4  /**< Position of blk hdr word for reserved (ver 4+). */
#define EV_HD_VER    5	/**< Position of blk hdr word for version of file format (+ bit info in ver 4+). */
#define EV_HD_RESVD2 6	/**< Position of blk hdr word for reserved. */
#define EV_HD_MAGIC  7	/**< Position of blk hdr word for magic number for endianness tracking. */


/** Turn on 9th bit to indicate dictionary included in block */
#define setDictionaryBit(a)     (a->buf[EV_HD_VER] |= EV_DICTIONARY_MASK)
/** Turn off 9th bit to indicate dictionary included in block */
#define clearDictionaryBit(a)   (a->buf[EV_HD_VER] &= ~EV_DICTIONARY_MASK)
/** Is there a dictionary in this block? */
#define hasDictionary(a)        ((a->buf[EV_HD_VER] & EV_DICTIONARY_MASK) > 0 ? 1 : 0)
#define hasDictionaryInt(i)     ((i & EV_DICTIONARY_MASK) > 0 ? 1 : 0)
/** Turn on 10th bit to indicate last block of file/transmission */
#define setLastBlockBit(a)      (a->buf[EV_HD_VER] |= EV_LASTBLOCK_MASK)
/** Turn off 10th bit to indicate last block of file/transmission */
#define clearLastBlockBit(a)    (a->buf[EV_HD_VER] &= ~EV_LASTBLOCK_MASK)
#define clearLastBlockBitInt(i) (i &= ~EV_LASTBLOCK_MASK)
/** Is this the last block of file/transmission? */
#define isLastBlock(a)          ((a->buf[EV_HD_VER] & EV_LASTBLOCK_MASK) > 0 ? 1 : 0)
#define isLastBlockInt(i)       ((i & EV_LASTBLOCK_MASK) > 0 ? 1 : 0)

/* Prototypes for static routines */
static  int      evOpenImpl(char *srcDest, uint32_t bufLen, int sockFd, char *flags, int *handle);
static  int      evGetNewBuffer(EVFILE *a);
static  int      evFlush(EVFILE *a, int closing);
static  void     initBlockHeader(EVFILE *a);
static  char *   evTrim(char *s, int skip);
static  int      tcpWrite(int fd, const void *vptr, int n);
static  int      tcpRead(int fd, void *vptr, int n);
static  int      evReadAllocImpl(EVFILE *a, uint32_t **buffer, uint32_t *buflen);
static  void     localClose(EVFILE *a);
static  int      getEventCount(EVFILE *a, uint32_t *count);

/* Append Mode */
static  int      toAppendPosition(EVFILE *a);

/* Random Access Mode */
static  int      memoryMapFile(EVFILE *a, const char *fileName);
static  int      generatePointerTable(EVFILE *a);

/*  These replace routines from swap_util.c, ejw, 1-dec-03 */
extern int32_t   swap_int32_t_value(int32_t val);
extern uint32_t *swap_uint32_t(uint32_t *data, uint32_t length, uint32_t *dest);

/** Maximum number of structures we can keep track of at once. */
#define MAXHANDLES 20
EVFILE *handle_list[20] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};


/*-----------------------------------------------------------------------------*/


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
        if ( (nwritten = write(fd, (char*)ptr, nleft)) <= 0) {
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
        if ( (nread = read(fd, ptr, nleft)) < 0) {
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
    int i, len, frontCount=0;
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
 * Function to initialize block header.
 * @param a pointer to file structure containing block header
 */
static void initBlockHeader(EVFILE *a) {
    a->buf[EV_HD_BLKSIZ] = 0; /* final block size unknown yet */
    a->buf[EV_HD_BLKNUM] = 1;
    a->buf[EV_HD_HDSIZ]  = EV_HDSIZ;
    a->buf[EV_HD_COUNT]  = 0;
    a->buf[EV_HD_RESVD1] = 0;
    a->buf[EV_HD_VER]    = EV_VERSION;
    a->buf[EV_HD_RESVD2] = 0;
    a->buf[EV_HD_MAGIC]  = EV_MAGIC;
}


/** Fortran interface to {@link evOpen}. */
#ifdef AbsoftUNIXFortran
int evopen
#else
int evopen_
#endif
(char *filename, char *flags, int *handle, int fnlen, int flen)
{
    char *fn, *fl;
    int status;
    fn = (char *) malloc(fnlen+1);
    strncpy(fn,filename,fnlen);
    fn[fnlen] = 0;      /* insure filename is null terminated */
    fl = (char *) malloc(flen+1);
    strncpy(fl,flags,flen);
    fl[flen] = 0;           /* insure flags is null terminated */
    status = evOpen(fn,fl,handle);
    free(fn);
    free(fl);
    return(status);
}



/**
 * This function opens a file for either reading or writing evio format data.
 * Works with all versions of evio for reading, but only writes version 4
 * format. A handle is returned for use with calling other evio routines.
 *
 * @param filename  name of file
 * @param flags     pointer to case-independent string of "w" for writing,
 *                  "r" for reading, "a" for appending, or "ra" for random
 *                  access reading of a file
 * @param handle    pointer to int which gets filled with handle
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADARG    if filename, flags or handle arg is NULL; unrecognizable flags;
 * @return S_EVFILE_ALLOCFAIL if memory allocation failed
 * @return S_EVFILE_BADFILE   if error reading file, unsupported version,
 *                            or contradictory data in file
 * @return S_EVFILE_BADHANDLE if no memory available to store handle structure
 *                            (increase MAXHANDLES in evio.c and recompile)
 * @return errno              if file could not be opened (handle = 0)
 */
int evOpen(char *filename, char *flags, int *handle)
{
    if (strcasecmp(flags, "w")  != 0 &&
        strcasecmp(flags, "r")  != 0 &&
        strcasecmp(flags, "a")  != 0 &&
        strcasecmp(flags, "ra") != 0)  {
        return(S_EVFILE_BADARG);
    }
    
    return(evOpenImpl(filename, 0, 0, flags, handle));
}



/**
 * This function allows for either reading or writing evio format data from a buffer.
 * Works with all versions of evio for reading, but only writes version 4
 * format. A handle is returned for use with calling other evio routines.
 *
 * @param buffer  pointer to buffer
 * @param bufLen  length of buffer in 32 bit ints
 * @param flags   pointer to case-independent string of "w", "r", "a", or "ra"
 *                for writing/reading/appending/random-access-reading to/from a buffer.
 * @param handle  pointer to int which gets filled with handle
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADARG    if buffer, flags or handle arg is NULL; unrecognizable flags;
 *                            buffer size too small
 * @return S_EVFILE_ALLOCFAIL if memory allocation failed
 * @return S_EVFILE_BADFILE   if error reading buffer, unsupported version,
 *                            or contradictory data
 * @return S_EVFILE_BADHANDLE if no memory available to store handle structure
 *                            (increase MAXHANDLES in evio.c and recompile)
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
    else {
        return(S_EVFILE_BADARG);
    }
    
    return(evOpenImpl(buffer, bufLen, 0, flag, handle));
}


/**
 * This function allows for either reading or writing evio format data from a TCP socket.
 * Works with all versions of evio for reading, but only writes version 4
 * format. A handle is returned for use with calling other evio routines.
 *
 * @param sockFd  TCP socket file descriptor
 * @param flags   pointer to case-independent string of "w" & "r" for
 *                writing/reading to/from a socket
 * @param handle  pointer to int which gets filled with handle
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADARG    if flags or handle arg is NULL; bad file descriptor arg;
 *                            unrecognizable flags
 * @return S_EVFILE_ALLOCFAIL if memory allocation failed
 * @return S_EVFILE_BADFILE   if error reading socket, unsupported version,
 *                            or contradictory data
 * @return S_EVFILE_BADHANDLE if no memory available to store handle structure
 *                            (increase MAXHANDLES in evio.c and recompile)
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
    else {
        return(S_EVFILE_BADARG);
    }

    return(evOpenImpl((char *)NULL, 0, sockFd, flag, handle));
}


    
/**
 * This function opens a file, socket, or buffer for either reading or writing
 * evio format data.
 * Works with all versions of evio for reading, but only writes version 4
 * format. A handle is returned for use with calling other evio routines.
 *
 * @param srcDest name of file if flags = "w" or "r";
 *                pointer to buffer if flags = "wb" or "rb"
 * @param bufLen  length of buffer (srcDest) if flags = "wb" or "rb"
 * @param sockFd  socket file descriptor if flags = "ws" or "rs"
 * @param flags   pointer to case-independent string:
 *                "w", "r", "a", & "ra" for writing/reading/append/random-access-reading to/from a file;
 *                "ws" & "rs" for writing/reading to/from a socket;
 *                "wb", "rb", "ab", & "rab" for writing/reading/append/random-access-reading to/from a buffer.
 * @param handle  pointer to int which gets filled with handle
 * 
 * @return S_SUCCESS          if successful
 * @return S_FAILURE          if code compiled so that block header < 8, 32bit words long
 * @return S_EVFILE_BADARG    if flags or handle arg is NULL;
 *                            if srcDest arg is NULL when using file or buffer;
 *                            if unrecognizable flags;
 *                            if buffer size too small when using buffer;
 *                            if specifying random access on vxWorks or Windows
 * @return S_EVFILE_ALLOCFAIL if memory allocation failed
 * @return S_EVFILE_BADFILE   if error reading file, unsupported version,
 *                            or contradictory data in file
 * @return S_EVFILE_UNXPTDEOF if reading buffer which is too small
 * @return S_EVFILE_BADHANDLE if no memory available to store handle structure
 *                            (increase MAXHANDLES in evio.c and recompile)
 * @return errno              for file opening or socket reading problems
 */
static int evOpenImpl(char *srcDest, uint32_t bufLen, int sockFd, char *flags, int *handle)
{
    EVFILE *a;
    char *filename, *buffer;
    
    uint32_t blk_size, temp, headerInfo, blkHdrSize, header[EV_HDSIZ];
    uint32_t rwBufSize, nBytes, bytesToRead;

    int i, err, version, ihandle;
    int useFile=0, useBuffer=0, useSocket=0, reading=0, randomAccess=0, append=0;

    
    /* Check args */
    if (flags == NULL || handle == NULL) {
        return(S_EVFILE_BADARG);
    }

    /* Check to see if someone set the length of the block header to be too small. */
    if (EV_HDSIZ < 8) {
printf("EV_HDSIZ in evio.h set to be too small (%d). Must be >= 8.\n", EV_HDSIZ);
        return(S_FAILURE);
    }

    /* Are we dealing with a file, buffer, or socket? */
    if (strcasecmp(flags, "w")  == 0  ||
        strcasecmp(flags, "r")  == 0  ||
        strcasecmp(flags, "a")  == 0  ||
        strcasecmp(flags, "ra") == 0 )  {
        
        useFile = 1;
        
        if (strcasecmp(flags,  "a") == 0) append = 1;
        if (strcasecmp(flags, "ra") == 0) randomAccess = 1;

#if defined VXWORKS || defined _MSC_VER
        /* No random access support in vxWorks or Windows */
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
    else if (strcasecmp(flags, "wb")  == 0  ||
             strcasecmp(flags, "rb")  == 0  ||
             strcasecmp(flags, "ab")  == 0  ||
             strcasecmp(flags, "rab") == 0 )  {
        
        useBuffer = 1;
        
        if (strcasecmp(flags, "ab")  == 0) append = 1;
        if (strcasecmp(flags, "rab") == 0) randomAccess = 1;

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
    else if (strcasecmp(flags, "ws") == 0 ||
             strcasecmp(flags, "rs") == 0)  {
        
        useSocket = 1;
        if (sockFd < 0) {
            return(S_EVFILE_BADARG);
        }
    }
    else {
        return(S_EVFILE_BADARG);
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

    
    /*********************************************************/
    /* If we're reading a version 1-4 file/socket/buffer ... */
    /*********************************************************/
    if (reading) {
        
        if (useFile) {
#if defined VXWORKS || defined _MSC_VER
            /* No pipe or zip/unzip support in vxWorks */
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
            }
            else if (randomAccess) {
                err = memoryMapFile(a, filename);
                if (err != S_SUCCESS) {
                    return(errno);
                }
            }
            else {
                a->file = fopen(filename,"r");
            }
#endif
            if (randomAccess) {
                /* Read (copy) in header */
                nBytes = sizeof(header);
                memcpy((void *)header, (const void *)a->mmapFile, nBytes);
            }
            else {
                if (a->file == NULL) {
                    free(a);
                    *handle = 0;
                    free(filename);
                    return(errno);
                }

                /* Read in header */
                nBytes = fread(header, 1, sizeof(header), a->file);
            }
        }
        else if (useSocket) {
            a->sockFd = sockFd;
            a->rw = EV_READSOCK;
            
            /* Read in header */
            nBytes = tcpRead(sockFd, header, sizeof(header));
            if (nBytes < 0) return(errno);
        }
        else if (useBuffer) {
            a->rwBuf = buffer;
            a->rw = EV_READBUF;
            a->rwBufSize = rwBufSize;
            
            /* Read (copy) in header */
            nBytes = sizeof(header);
            memcpy((void *)header, (const void *)(a->rwBuf + a->rwBytesUsed), nBytes);
            a->rwBytesUsed += nBytes;
        }

        /**********************************/
        /* Run header through some checks */
        /**********************************/

        /* Check to see if all bytes are there */
        if (nBytes != sizeof(header)) {
            /* Close file and free memory */
            if (useFile) {
                localClose(a);
                free(filename);
            }
            free(a);
            return(S_EVFILE_BADFILE);
        }

        /* Check endianness */
        if (header[EV_HD_MAGIC] != EV_MAGIC) {
            temp = EVIO_SWAP32(header[EV_HD_MAGIC]);
            if (temp == EV_MAGIC) {
                a->byte_swapped = 1;
            }
            else {
printf("Magic # is a bad value\n");
                if (useFile) {
                    localClose(a);
                    free(filename);
                }
                free(a);
                return(S_EVFILE_BADFILE);
            }
        }
        else {
            a->byte_swapped = 0;
        }
        a->magic = EV_MAGIC;

        /* Check VERSION */
        headerInfo = header[EV_HD_VER];
        if (a->byte_swapped) {
            headerInfo = EVIO_SWAP32(headerInfo);
        }
        /* Only lowest 8 bits count in version 4's header word */
        version = headerInfo & EV_VERSION_MASK;
        if (version < 1 || version > 4) {
printf("Header has unsupported evio version (%d), quit\n", version);
            if (useFile) {
                localClose(a);
                free(filename);
            }
            free(a);
            return(S_EVFILE_BADFILE);
        }
        a->version = version;

        /* Check the header's value for header size against our assumption. */
        blkHdrSize = header[EV_HD_HDSIZ];
        if (a->byte_swapped) {
            blkHdrSize = EVIO_SWAP32(blkHdrSize);
        }

        /* If actual header size not what we're expecting ... */
        if (blkHdrSize != EV_HDSIZ) {
            int restOfHeader = blkHdrSize - EV_HDSIZ;
printf("Header size was assumed to be %d but it was actually %u\n", EV_HDSIZ, blkHdrSize);
            if (restOfHeader < 0) {
printf("Header size is too small (%u), return error\n", blkHdrSize);
                if (useFile) {
                    localClose(a);
                    free(filename);
                }
                free(a);
                return(S_EVFILE_BADFILE);
            }
        }

        if (randomAccess) {
            /* Random access only available for version 4+ */
            if (version < 4) {
                return(S_EVFILE_BADFILE);
            }

            /* Find pointers to all the events (skips over any dictionary) */
            err = generatePointerTable(a);
            if (err != S_SUCCESS) {
                return(err);
            }
        }
        else {
            /**********************************/
            /* Allocate buffer to store block */
            /**********************************/
    
            /* size of block we're reading */
            blk_size = header[EV_HD_BLKSIZ];
            if (a->byte_swapped) {
                blk_size = EVIO_SWAP32(blk_size);
            }
            a->blksiz = blk_size;

            /* How big do we make this buffer? Use a minimum size. */
            a->bufSize = blk_size < EV_BLOCKSIZE_MIN ? EV_BLOCKSIZE_MIN : blk_size;
            a->buf = (uint32_t *)malloc(4*a->bufSize);
    
            /* Error if can't allocate buffer memory */
            if (a->buf == NULL) {
                if (useFile) {
                    localClose(a);
                    free(filename);
                }
                free(a);
                return(S_EVFILE_ALLOCFAIL);
            }
    
            /* Copy header (the part we read in) into block (swapping if necessary) */
            if (a->byte_swapped) {
                swap_int32_t((uint32_t *)header, EV_HDSIZ, (uint32_t *)a->buf);
            }
            else {
                memcpy(a->buf, header, 4*EV_HDSIZ);
            }
    
            /*********************************************************/
            /* Read rest of block & any remaining, over-sized header */
            /*********************************************************/
            bytesToRead = 4*(blk_size - EV_HDSIZ);
            if (useFile) {
                nBytes = fread(a->buf+EV_HDSIZ, 1, bytesToRead, a->file);
            }
            else if (useSocket) {
                nBytes = tcpRead(sockFd, a->buf+EV_HDSIZ, bytesToRead);
                if (nBytes < 0) {
                    free(a->buf);
                    free(a);
                    return(errno);
                }
            }
            else if (useBuffer) {
                nBytes = bytesToRead;
                memcpy(a->buf+EV_HDSIZ, (a->rwBuf + a->rwBytesUsed), bytesToRead);
                a->rwBytesUsed += bytesToRead;
            }
    
            /* Check to see if all bytes were read in */
            if (nBytes != bytesToRead) {
                if (useFile) {
                    localClose(a);
                    free(filename);
                }
                free(a->buf);
                free(a);
                return(S_EVFILE_BADFILE);
            }
    
            if (version < 4) {
                /* Pointer to where start of first event header occurs. */
                a->next = a->buf + (a->buf)[EV_HD_START];
                /* Number of valid 32 bit words from start of first event to end of block */
                a->left = (a->buf)[EV_HD_USED] - (a->buf)[EV_HD_START];
            }
            else {
                /* Pointer to where start of first event header occurs =
                 * right after header for version 4+. */
                a->next = a->buf + blkHdrSize;
                
                /* Number of valid 32 bit words = (full block size - header size) in v4+ */
                a->left = (a->buf)[EV_HD_BLKSIZ] - blkHdrSize;
            
                /* Is this the last block? */
                a->isLastBlock = isLastBlock(a);
                
                /* Pull out dictionary if there is one (works only after header is swapped). */
                if (hasDictionary(a)) {
                    int status;
                    uint32_t buflen;
                    uint32_t *buf;
    
                    /* Read in first bank which will be dictionary */
                    status = evReadAllocImpl(a, &buf, &buflen);
                    if (status == S_SUCCESS) {
                        // Trim off any whitespace/padding, skipping over event header (8 bytes)
                        a->dictionary = evTrim((char *)buf, 8);
                    }
                    else {
                        printf("ERROR retrieving DICTIONARY, status = %#.8x\n", status);
                    }
                }
            }
        }
    }
        
    /*************************/
    /* If we're writing  ... */
    /*************************/
    else {

        a->append = append;

        if (useFile) {
#if defined  VXWORKS || defined _MSC_VER
            a->file = fopen(filename,"w");
            a->rw = EV_WRITEFILE;
#else
            a->rw = EV_WRITEFILE;
            if (strcmp(filename,"-") == 0) {
                a->file = stdout;
            }
            else if (filename[0] == '|') {
                a->file = popen(filename+1,"w");
                a->rw = EV_WRITEPIPE;	/* Make sure we know to use pclose */
            }
            else if (append) {
                /* Must be able to read & write since we may
                 * need to write over last block header. Do NOT
                 * truncate (erase) the file here! */
                a->file = fopen(filename,"r+");
                
                /* Read in header */
                nBytes = sizeof(header)*fread(header, sizeof(header), 1, a->file);
                
                /* Check to see if read the whole header */
                if (nBytes != sizeof(header)) {
                    /* Close file and free memory */
                    fclose(a->file);
                    free(filename);
                    free(a);
                    return(S_EVFILE_BADFILE);
                }
            }
            else {
                a->file = fopen(filename,"w");
            }
#endif
            if (a->file == NULL) {
                *handle = 0;
                free(a);
                free(filename);
                return(errno);
            }
        }
        else if (useSocket) {
            a->sockFd = sockFd;
            a->rw = EV_WRITESOCK;
        }
        else if (useBuffer) {
            a->rwBuf = buffer;
            a->rw = EV_WRITEBUF;
            a->rwBufSize = rwBufSize;

            /* if appending, read in first header */
            if (append) {
                nBytes = sizeof(header);
                /* unexpected EOF or end-of-buffer in this case */
                if (rwBufSize < nBytes) {
                    free(a);
                    return(S_EVFILE_UNXPTDEOF);
                }
                /* Read (copy) in header */
                memcpy(header, (a->rwBuf + a->rwBytesUsed), nBytes);
                a->rwBytesUsed += nBytes;
            }
        }


        /*********************************************************************/
        /* If we're appending, we already read in the first block header     */
        /* so check a few things like version number and endianness.         */
        /*********************************************************************/
        if (append) {
            /* Check endianness */
            if (header[EV_HD_MAGIC] != EV_MAGIC) {
                temp = EVIO_SWAP32(header[EV_HD_MAGIC]);
                if (temp == EV_MAGIC) {
                    a->byte_swapped = 1;
                }
                else {
                    printf("Magic # is a bad value\n");
                    if (useFile) {
                        if (a->rw == EV_WRITEFILE) {
                            fclose(a->file);
                        }
                        free(filename);
                    }
                    free(a);
                    return(S_EVFILE_BADFILE);
                }
            }
            else {
                a->byte_swapped = 0;
            }
            a->magic = EV_MAGIC;
        
            /* Check VERSION */
            headerInfo = header[EV_HD_VER];
            if (a->byte_swapped) {
                headerInfo = EVIO_SWAP32(headerInfo);
            }
            /* Only lowest 8 bits count in version 4's header word */
            version = headerInfo & EV_VERSION_MASK;
            if (version < 4) {
printf("Header has unsupported evio version (%d) for append mode, quit\n", version);
                if (useFile) {
                    if (a->rw == EV_WRITEFILE) {
                        fclose(a->file);
                    }
                    free(filename);
                }
                free(a);
                return(S_EVFILE_BADFILE);
            }
            a->version = version;
        }
        
        /* Allocate memory for a block */
        a->buf = (uint32_t *) calloc(1,4*EV_BLOCKSIZE_V4);
        if (!(a->buf)) {
            if (useFile) {
                if (a->rw == EV_WRITEFILE) {
                    fclose(a->file);
                }
                else if (a->rw == EV_WRITEPIPE) {
                    pclose(a->file);
                }
                free(filename);
            }
            free(a);
            return(S_EVFILE_ALLOCFAIL);
        }

        /* Initialize block header */
        initBlockHeader(a);

        /* Initialize other file struct members */

        /* Pointer to where next to write. In this case, the start
         * of first event header will be right after header. */
        a->next = a->buf + EV_HDSIZ;
        /* Space in number of words, not in header, left for writing */
        a->left = EV_BLOCKSIZE_V4 - EV_HDSIZ;
        /* Target block size (final size may be larger or smaller) */
        a->blkSizeTarget = EV_BLOCKSIZE_V4;
        /* Total data written = block header size so far */
        a->blksiz = EV_HDSIZ;
        a->rwBytesOut = 4*EV_HDSIZ;
        /* Max # of events/block */
        a->eventsMax = EV_EVENTS_MAX;
        /* Remember size of block buffer */
        a->bufSize = EV_BLOCKSIZE_V4;
        /* EVIO version number */
        a->version = EV_VERSION;
        /* Magic number */
        a->magic = EV_MAGIC;

        /* Position file stream / buffer for next write.
         * If not appending this is does nothing. */
        err = toAppendPosition(a);
        if (err != S_SUCCESS) {
            return(err);
        }
        
    } /* if writing */
    
    /* Store general info in handle structure */
    if (!randomAccess) a->blknum = a->buf[EV_HD_BLKNUM];

    for (ihandle=0; ihandle < MAXHANDLES; ihandle++) {
        /* If a slot is available ... */
        if (handle_list[ihandle] == 0) {
            handle_list[ihandle] = a;
            *handle = ihandle+1;
            if (useFile) {
                free(filename);
            }
            return(S_SUCCESS);
        }
    }

    /* No slots left */
    *handle = 0;
    if (useFile) {
        localClose(a);
        free(filename);
    }
    if (a->buf != NULL) free(a->buf);
    free(a);
    return(S_EVFILE_BADHANDLE);        /* A better error code would help */
}


/**
 * This routine closes any open files and unmaps any mapped memory.
 * @param handle evio handle
 */
static void localClose(EVFILE *a)
{
    /* Close file */
    if (a->rw == EV_WRITEFILE) {
        fclose(a->file);
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
static int memoryMapFile(EVFILE *a, const char *fileName)
{
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
    fstat(fd, &fileInfo);
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

    return(S_SUCCESS);
}


/**
 * This function returns a count of the number of events in a file or buffer.
 * If reading with random access, it returns the count taken when initially
 * generating the table of event pointers. If regular reading, the count is
 * generated when asked for in evIoctl. If writing, the count gets incremented
 * by 1 for each evWrite. If appending, the count is set when moving to the
 * correct file position uring evOpen and is thereafter incremented with each
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
static int getEventCount(EVFILE *a, uint32_t *count)
{
    int        i, usingBuffer = 0, nBytes;
    ssize_t    startingPosition;
    uint32_t   bytesUsed, blockEventCount, blockSize, blockHeaderSize, header[EV_HDSIZ];

    
    /* Reject if using sockets */
    if (a->rw == EV_WRITESOCK || a->rw == EV_READSOCK) {
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

        /* Go to back to beginning of file */
        if (fseek(a->file, 0L, SEEK_SET) < 0) return(errno);
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
            nBytes = sizeof(header)*fread(header, sizeof(header), 1, a->file);
                
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
        blockHeaderSize = header[EV_HD_HDSIZ];
        blockEventCount = header[EV_HD_COUNT];
//printf("getEventCount: ver = 0x%x, blk size = %u, blk hdr sz = %u, blk ev cnt = %u\n",
//       i, blockSize, blockHeaderSize, blockEventCount);
        
        /* Add to the number of events. Dictionary is NOT
         * included in the header's event count. */
        a->eventCount += blockEventCount;

        /* Stop at the last block */
        if (isLastBlockInt(i)) {
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
 * @return S_SUCCESS          if successful or not random access handle (does nothing)
 * @return S_EVFILE_ALLOCFAIL if memory cannot be allocated
 * @return S_EVFILE_UNXPTDEOF if unexpected EOF or end-of-valid-data
 *                            while reading data (perhaps bad block header)
 */
static int generatePointerTable(EVFILE *a)
{
    int        i, usingBuffer=0, lastBlock=0, firstBlock=1;
    size_t     bytesLeft;
    uint32_t  *pmem, len, numPointers, blockEventCount, blockHdrSize, evIndex = 0L;

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

    if (usingBuffer) {
        pmem = (uint32_t *)a->rwBuf;
        bytesLeft = a->rwBufSize; // limit on size only
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
        for (i=0; i < blockEventCount; i++) {
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
                numPointers += 10000;
                a->pTable = realloc(a->pTable, numPointers*sizeof(uint32_t *));
                if (a->pTable == NULL) {
                    free(a->pTable);
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
 * This function positions a file or buffer for the first {@link evWrite}
 * in append mode. It makes sure that the last block header is an empty one
 * with its "last block" bit set.
 *
 * @param a  handle structure
 *
 * @return S_SUCCESS          if successful or not random access handle (does nothing)
 * @return S_EVFILE_TRUNC     if not enough space in buffer to write ending header
 * @return S_EVFILE_BADFILE   if bad file format - cannot read
 * @return S_EVFILE_UNXPTDEOF if unexpected EOF or end-of-valid-data
 *                            while reading data (perhaps bad block header)
 * @return errno              if any file seeking/writing errors
 */
static int toAppendPosition(EVFILE *a)
{
    int         i, err, usingBuffer=0;
    uint32_t    nBytes, bytesToWrite;
    uint32_t   *pmem, sixthWord, header[EV_HDSIZ];
    uint32_t    blockEventCount, blockSize, blockHeaderSize, blockNumber=1;
    
    /* Only for append mode */
    if (!a->append) {
        return(S_SUCCESS);
    }

    if (a->rw == EV_WRITEBUF) {
        usingBuffer = 1;
    }

    if (usingBuffer) {
        /* Go to back to beginning of buffer */
        a->rwBytesOut  = 0;  /* # bytes written to rwBuf so far (after each evWrite) */
        a->rwBytesUsed = 0;  /* # bytes written to rwBuf so far (after each flush). */
    }
    else {
        /* Go to back to beginning of file */
        if (fseek(a->file, 0L, SEEK_SET) < 0) return(errno);
    }

    while (1) {
        /* Read in EV_HDSIZ (8) ints of header */
        if (usingBuffer) {
            /* Is there enough data to read in header? */
            if (a->rwBufSize - a->rwBytesUsed < sizeof(header)) {
                /* unexpected EOF or end-of-buffer in this case */
                return(S_EVFILE_UNXPTDEOF);
            }
            /* Read (copy) in header */
            memcpy(header, (a->rwBuf + a->rwBytesUsed), 4*EV_HDSIZ);
        }
        else {
            nBytes = sizeof(header)*fread(header, sizeof(header), 1, a->file);
                
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
        blockHeaderSize = header[EV_HD_HDSIZ];
        blockEventCount = header[EV_HD_COUNT];

        /* Add to the number of events */
        a->eventCount += blockEventCount;

        /* Next block has this number. */
        blockNumber++;

        /* Stop at the last block */
        if (isLastBlockInt(i)) {
            break;
        }

        /* Hop to next block header */
        if (usingBuffer) {
            /* Is there enough buffer space to hop over block? */
            if (a->rwBufSize - a->rwBytesUsed < 4*blockSize) {
                /* unexpected EOF or end-of-buffer in this case */
                return(S_EVFILE_UNXPTDEOF);
            }
            a->rwBytesOut  += 4*blockSize;
            a->rwBytesUsed += 4*blockSize;
        }
        else {
            if (fseek(a->file, 4*(blockSize-EV_HDSIZ), SEEK_CUR) < 0) return(errno);
        }
    }

    /* If we're here, we've just read the last block header (at least EV_HDSIZ words of it).
     * Easiest to rewrite it in our own image at this point. But first we
     * must check to see if the last block contains data. If it does, we
     * change a bit so it's not labeled as the last block. Then we write
     * an empty last block after the data. If, on the other hand, this
     * last block contains no data, just write over it with a new one. */
    if (blockSize > blockHeaderSize) {
        /* Clear last block bit in 6th header word */
        sixthWord = clearLastBlockBitInt(header[EV_HD_VER]);
        
        /* Rewrite header word with bit info & hop over block */
        if (usingBuffer) {
/*printf("toAppendPosition: writing over last block's 6th word for buffer\n");*/
            /* Write over 6th block header word */
             pmem = (uint32_t *) (a->rwBuf + a->rwBytesUsed + 4*EV_HD_VER);
            *pmem = sixthWord;

            /* Hop over the entire block */
            a->rwBytesOut  += 4*blockSize;
            a->rwBytesUsed += 4*blockSize;
        }
        else {
            /* Back up to before 6th block header word */
/*printf("toAppendPosition: writing over last block's 6th word, back up %d words\n", (EV_HDSIZ - EV_HD_VER));*/
            if (fseek(a->file, -4*(EV_HDSIZ - EV_HD_VER), SEEK_CUR) < 0) return(errno);

            /* Write over 6th block header word */
            if (fwrite((const void *)&sixthWord, 1, sizeof(uint32_t), a->file) != sizeof(uint32_t)) {
                return(errno);
            }

            /* Hop over the entire block */
/*printf("toAppendPosition: wrote over last block's 6th word, hop over %d words\n", (blockSize - (EV_HD_VER + 1)));*/
            if (fseek(a->file, 4*(blockSize - (EV_HD_VER + 1)), SEEK_CUR) < 0) return(errno);
        }
    }
    else {
        /* We already read in the block header, now back up so we can overwrite it.
         * If using buffer, we never incremented the position, so we're OK. */
        blockNumber--;
        if (!usingBuffer) {
            if (fseek(a->file, -4*EV_HDSIZ, SEEK_CUR) < 0) return(errno);
        }
    }

    /* Write empty last block header. This function gets called right after
     * the handle's block header memory is initialized and other members of
     * the handle structure are also initialized. Some of the values need to
     * be set properly here - like the block number - since we've skipped over
     * all existing blocks. */
    a->blknum = a->buf[EV_HD_BLKNUM] = blockNumber;

    /* Mark this block as the last one to be written. */
    setLastBlockBit(a);
    a->isLastBlock  = 1;
    a->lastBlockOut = 1;
  
    a->buf[EV_HD_BLKSIZ] = EV_HDSIZ;
    bytesToWrite = 4*EV_HDSIZ;

    if (usingBuffer) {
        /* If there's not enough space in the user-given
         * buffer to contain this block, return an error. */
        if (a->rwBufSize < a->rwBytesUsed + bytesToWrite) {
            return(S_EVFILE_TRUNC);
        }

        memcpy((a->rwBuf + a->rwBytesUsed), a->buf, bytesToWrite);
        nBytes = bytesToWrite;
        /* How many bytes will end up in the buffer we're writing to? */
        a->rwBytesOut  += bytesToWrite;
        a->rwBytesUsed += bytesToWrite;
    }
    else {
        /* Clear EOF and error indicators for file stream */
        clearerr(a->file);
        /* This last block is always EV_HDSIZ ints long. */
        nBytes = fwrite((const void *)a->buf, 1, bytesToWrite, a->file);
        /* Return any error condition of file stream */
        if (ferror(a->file)) return(ferror(a->file));
    }

    /* Return any error condition of write attempt */
    if (nBytes != bytesToWrite) return(errno);
       
    /* We should now be in a state identical to that if we had
     * just now written everything currently in the file/buffer. */

    return(S_SUCCESS);
}


/**
 * This routine reads an evio bank from an evio format file/socket/buffer opened
 * with routine {@link evOpen}, allocates a buffer and fills it with the bank.
 * Works with all versions of evio. A status is returned.
 *
 * @param a      pointer to handle structure
 * @param buffer pointer to pointer to buffer gets filled with
 *               pointer to allocated buffer (caller must free)
 * @param buflen pointer to int gets filled with length of buffer in 32 bit words
 *               including the full (8 byte) header
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADMODE   if opened for writing or random-access reading in {@link evOpen}
 * @return S_EVFILE_BADARG    if buffer or buflen is NULL
 * @return S_EVFILE_BADHANDLE if wrong magic # in handle
 * @return S_EVFILE_ALLOCFAIL if memory cannot be allocated
 * @return S_EVFILE_BADFILE   if file has bad magic #
 * @return S_EVFILE_UNXPTDEOF if unexpected EOF or end-of-valid-data
 *                            while reading data (perhaps bad block header)
 * @return EOF                if end-of-file or end-of-valid-data reached
 * @return errno              if file/socket read error
 * @return stream error       if file stream error
 */
static int evReadAllocImpl(EVFILE *a, uint32_t **buffer, uint32_t *buflen)
{
    uint32_t *buf, *pBuf;
    int       error, status;
    uint32_t  nleft, ncopy, len;


    if (buffer == NULL || buflen == NULL) {
        return(S_EVFILE_BADARG);
    }

    /* Check magic # */
    if (a->magic != EV_MAGIC) {
        return(S_EVFILE_BADHANDLE);
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
 * This routine reads an evio bank from an evio format file/socket/buffer opened
 * with routine {@link evOpen}, allocates a buffer and fills it with the bank.
 * Works with all versions of evio. A status is returned.
 *
 * @param handle evio handle
 * @param buffer pointer to pointer to buffer gets filled with
 *               pointer to allocated buffer (caller must free)
 * @param buflen pointer to int gets filled with length of buffer in 32 bit words
 *               including the full (8 byte) bank header
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADMODE   if opened for writing or random-access reading in {@link evOpen}
 * @return S_EVFILE_BADARG    if buffer or buflen is NULL
 * @return S_EVFILE_BADHANDLE if bad handle arg or wrong magic # in handle
 * @return S_EVFILE_ALLOCFAIL if memory cannot be allocated
 * @return S_EVFILE_BADFILE   if file has bad magic #
 * @return S_EVFILE_UNXPTDEOF if unexpected EOF or end-of-valid-data
 *                            while reading data (perhaps bad block header)
 * @return EOF                if end-of-file or end-of-valid-data reached
 * @return errno              if file/socket read error
 * @return stream error       if file stream error
 */
int evReadAlloc(int handle, uint32_t **buffer, uint32_t *buflen)
{
    EVFILE *a;

    /* Look up file struct (which contains block buffer) from handle */
    a = handle_list[handle-1];

    /* Check args */
    if (a == NULL) {
        return(S_EVFILE_BADHANDLE);
    }

    return evReadAllocImpl(a, buffer, buflen);
}


/** Fortran interface to {@link evRead}. */
#ifdef AbsoftUNIXFortran
int evread
#else
int evread_
#endif
(int *handle, uint32_t *buffer, uint32_t *buflen)
{
    return(evRead(*handle, buffer, *buflen));
}


/**
 * This routine reads from an evio format file/socket/buffer opened with routine
 * {@link evOpen} and returns the next event in the buffer arg. Works with all
 * versions of evio. A status is returned.
 *
 * @param handle evio handle
 * @param buffer pointer to buffer
 * @param buflen length of buffer in 32 bit words
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADMODE   if opened for writing or random-access reading in {@link evOpen}
 * @return S_EVFILE_TRUNC     if buffer provided by caller is too small for event read
 * @return S_EVFILE_BADARG    if buffer is NULL or buflen < 3
 * @return S_EVFILE_BADHANDLE if bad handle arg or wrong magic # in handle
 * @return S_EVFILE_ALLOCFAIL if memory cannot be allocated
 * @return S_EVFILE_BADFILE   if data has bad magic #
 * @return S_EVFILE_UNXPTDEOF if unexpected EOF or end-of-valid-data
 *                            while reading data (perhaps bad block header)
 * @return EOF                if end-of-file or end-of-valid-data reached
 * @return errno              if file/socket read error
 * @return stream error       if file stream error
 */
int evRead(int handle, uint32_t *buffer, uint32_t buflen)
{
    EVFILE   *a;
    int       error, status;
    uint32_t  nleft, ncopy;
    uint32_t *temp_buffer, *temp_ptr=NULL;


    if (buffer == NULL || buflen < 3) {
        return(S_EVFILE_BADARG);
    }

    /* Look up file struct (which contains block buffer) from handle */
    a = handle_list[handle-1];

    /* Check args */
    if (a == NULL) {
        return(S_EVFILE_BADHANDLE);
    }

    /* Check magic # */
    if (a->magic != EV_MAGIC) {
        return(S_EVFILE_BADHANDLE);
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
    
    /* If no more data left to read from current block, get a new block */
    if (a->left < 1) {
        status = evGetNewBuffer(a);
        if (status != S_SUCCESS) {
            return(status);
        }
    }

    /* Find number of words to read in next event (including header) */
    if (a->byte_swapped) {
        /* Create temp buffer for swapping */
        temp_ptr = temp_buffer = (uint32_t *) malloc(buflen*sizeof(uint32_t));
        if (temp_ptr == NULL) return(S_EVFILE_ALLOCFAIL);
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
        if (temp_ptr != NULL) free(temp_ptr);
        return(S_EVFILE_TRUNC);
    }

    /* While there is more event data left to read ... */
    while (nleft > 0) {

        /* If no more data left to read from current block, get a new block */
        if (a->left < 1) {
            status = evGetNewBuffer(a);
            if (status != S_SUCCESS) {
                if (temp_ptr != NULL) free(temp_ptr);
                return(status);
            }
        }

        /* If # words left to read in event <= # words left in block,
         * copy # words left to read in event to buffer, else
         * copy # left in block to buffer.*/
        ncopy = (nleft <= a->left) ? nleft : a->left;

        if (a->byte_swapped) {
            memcpy(temp_buffer, a->next, ncopy*4);
            temp_buffer += ncopy;
        }
        else{
            memcpy(buffer, a->next, ncopy*4);
            buffer += ncopy;
        }
        
        nleft   -= ncopy;
        a->next += ncopy;
        a->left -= ncopy;
    }

    /* Swap event if necessary */
    if (a->byte_swapped) {
        evioswap((uint32_t*)temp_ptr, 1, (uint32_t*)buffer);
        free(temp_ptr);
    }
    
    return(S_SUCCESS);
}


/**
 * This routine reads from an evio format file/buffer/socket opened with routine
 * {@link evOpen} and returns a pointer to the next event residing in an internal buffer.
 * If the data needs to be swapped, it is swapped in place. Any other
 * calls to read routines may cause the data to be overwritten.
 * No writing to the returned pointer is allowed.
 * Works only with evio version 4 and up. A status is returned.
 *
 * @param handle evio handle
 * @param buffer pointer to pointer to buffer gets filled with pointer to location in
 *               internal buffer which is guaranteed to be valid only until the next
 *              {@link evRead}, {@link evReadNoAlloc}, or {@link evReadNoCopy} call.
 * @param buflen pointer to int gets filled with length of buffer in 32 bit words
 *               including the full (8 byte) bank header
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADMODE   if opened for writing or random-access reading in {@link evOpen}
 * @return S_EVFILE_TRUNC     if buffer provided by caller is too small for event read
 * @return S_EVFILE_BADARG    if buffer or buflen is NULL
 * @return S_EVFILE_BADFILE   if version < 4, unsupported or bad format
 * @return S_EVFILE_BADHANDLE if bad handle arg or wrong magic # in handle
 * @return S_EVFILE_ALLOCFAIL if memory cannot be allocated
 * @return S_EVFILE_BADFILE   if data has bad magic #
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


    if (buffer == NULL || buflen == NULL) {
        return(S_EVFILE_BADARG);
    }

    /* Look up file struct (which contains block buffer) from handle */
    a = handle_list[handle-1];

    /* Check args */
    if (a == NULL) {
        return(S_EVFILE_BADHANDLE);
    }

    /* Returning a pointer into a block only works in evio version 4 and
     * up since in earlier versions events may be split between blocks. */
    if (a->version < 4) {
        return(S_EVFILE_BADFILE);
    }

    /* Check magic # */
    if (a->magic != EV_MAGIC) {
        return(S_EVFILE_BADHANDLE);
    }

    /* Need to be reading and not writing */
    if (a->rw != EV_READFILE && a->rw != EV_READPIPE &&
        a->rw != EV_READBUF  && a->rw != EV_READSOCK) {
        return(S_EVFILE_BADMODE);
    }
    
    /* Cannot be random access reading */
    if (a->randomAccess) {
        return(S_EVFILE_BADMODE);
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

    return(S_SUCCESS);
}


/**
 * This routine does a random access read from an evio format file or buffer opened with
 * routine {@link evOpen}. It returns a pointer to the desired event residing in either
 * a memory mapped file or buffer when opened in random access mode.
 * 
 * If the data needs to be swapped, it is swapped in place.
 * No writing to the returned pointer is allowed.
 * Works only with evio version 4 and up. A status is returned.
 *
 * @param handle evio handle
 * @param buffer pointer which gets filled with pointer to event in buffer or
 *               memory mapped file
 * @param eventNumber the number of the event to be read (returned) starting at 1.
 *
 * @return S_SUCCESS          if successful
 * @return S_FAILURE          if no events found in file or failure to make random access map
 * @return S_EVFILE_BADMODE   if not opened for random access reading in {@link evOpen}
 * @return S_EVFILE_BADARG    if pEvent arg is NULL
 * @return S_EVFILE_BADFILE   if version < 4, unsupported or bad format
 * @return S_EVFILE_BADHANDLE if bad handle arg or wrong magic # in handle
 */
int evReadRandom(int handle, const uint32_t **pEvent, uint32_t eventNumber)
{
    EVFILE   *a;
    uint32_t *pev;

    if (pEvent == NULL) {
        return(S_EVFILE_BADARG);
    }

    /* Look up file struct (which contains block buffer) from handle */
    a = handle_list[handle-1];

    /* Check args */
    if (a == NULL) {
        return(S_EVFILE_BADHANDLE);
    }

    /* Returning a pointer into a block only works in evio version 4 and
    * up since in earlier versions events may be split between blocks. */
    if (a->version < 4) {
        return(S_EVFILE_BADFILE);
    }

    /* Check magic # */
    if (a->magic != EV_MAGIC) {
        return(S_EVFILE_BADHANDLE);
    }

    /* Need to be *** random access *** reading (not from socket or pipe) and not writing */
    if ((a->rw != EV_READFILE && a->rw != EV_READBUF) || !a->randomAccess) {
        return(S_EVFILE_BADMODE);
    }

    /* event not in file/buf */
    if (eventNumber > a->eventCount || a->pTable == NULL) {
        return(S_FAILURE);
    }

    pev = a->pTable[eventNumber - 1];

    /* event not in file/buf */
    if (pev == NULL) {
        return(S_FAILURE);
    }

    /* swap data in buf/mem-map if necessary */
    if (a->byte_swapped) {
        evioswap(pev, 1, NULL);
    }

    /* return pointer to event in memory map / buffer */
    *pEvent = pev;

    return(S_SUCCESS);
}


/**
 * Routine to get the next block.
 *
 * @param a pointer file structure
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADFILE   if data has bad magic #
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
    uint32_t *newBuf, blkHdrSize;
    size_t    nBytes, bytesToRead;
    int       status = S_SUCCESS;

    /* See if we read in the last block the last time this was called (v4) */
    if (a->version > 3 && a->isLastBlock) {
        return(EOF);
    }

    /* First read block header from file/sock/buf */
    bytesToRead = 4*EV_HDSIZ;
    if (a->rw == EV_READFILE) {
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
        nBytes = tcpRead(a->sockFd, a->buf, bytesToRead);
    }
    else if (a->rw == EV_READBUF) {
        if (a->rwBufSize < a->rwBytesUsed + bytesToRead) {
            return(S_EVFILE_UNXPTDEOF);
        }
        memcpy(a->buf, (a->rwBuf + a->rwBytesUsed), bytesToRead);
        nBytes = bytesToRead;
        a->rwBytesUsed += bytesToRead;
    }

    /* Return any read error */
    if (nBytes != bytesToRead) {
        return(errno);
    }

    /* Swap header in place if necessary */
    if (a->byte_swapped) {
        swap_int32_t((uint32_t *)a->buf, EV_HDSIZ, NULL);
    }
    
    /* It is possible that the block header size is > EV_HDSIZ.
     * I think that the only way this could happen is if someone wrote
     * out an evio file "by hand", that is, not writing using the evio libs
     * but writing the bits directly to file. Be sure to check for it
     * and read any extra words in the header. They may need to be swapped. */
    blkHdrSize = a->buf[EV_HD_HDSIZ];
    if (blkHdrSize > EV_HDSIZ) {
        /* Read rest of block header from file/sock/buf ... */
        bytesToRead = 4*(blkHdrSize - EV_HDSIZ);
printf("HEADER IS TOO BIG, reading an extra %lu bytes\n", bytesToRead);
        if (a->rw == EV_READFILE) {
            nBytes = fread(a->buf + EV_HDSIZ, 1, bytesToRead, a->file);
        
            if (feof(a->file)) return(EOF);
            if (ferror(a->file)) return(ferror(a->file));
        }
        else if (a->rw == EV_READSOCK) {
            nBytes = tcpRead(a->sockFd, a->buf + EV_HDSIZ, bytesToRead);
        }
        else if (a->rw == EV_READBUF) {
            if (a->rwBufSize < a->rwBytesUsed + bytesToRead) return(S_EVFILE_UNXPTDEOF);
            memcpy(a->buf + EV_HDSIZ, a->rwBuf + a->rwBytesUsed, bytesToRead);
            nBytes = bytesToRead;
            a->rwBytesUsed += bytesToRead;
        }

        /* Return any read error */
        if (nBytes != bytesToRead) return(errno);
                
        /* Swap in place if necessary */
        if (a->byte_swapped) {
            swap_int32_t(a->buf + EV_HDSIZ, bytesToRead/4, NULL);
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
        nBytes = tcpRead(a->sockFd, (a->buf + blkHdrSize), bytesToRead);
    }
    else if (a->rw == EV_READBUF) {
        if (a->rwBufSize < a->rwBytesUsed + bytesToRead) {
            return(S_EVFILE_UNXPTDEOF);
        }
        memcpy(a->buf + blkHdrSize, a->rwBuf + a->rwBytesUsed, bytesToRead);
        nBytes = bytesToRead;
        a->rwBytesUsed += bytesToRead;
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

    /* Check to see if we just read in the last block (v4) */
    if (a->version > 3 && isLastBlock(a)) {
        a->isLastBlock = 1;
    }

    /* Start out pointing to the data right after the block header.
     * If we're in the middle of reading an event, this will allow
     * us to continue reading it. If we've looking to read a new
     * event, this should point to the next one. */
    a->next = a->buf + blkHdrSize;

    /* Find number of valid words left to read (w/ evRead) in block */
    if (a->version < 4) {
        a->left = (a->buf)[EV_HD_USED] - blkHdrSize;
    }
    else {
        a->left = a->blksiz - blkHdrSize;
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


/** Fortran interface to {@link evWrite}. */
#ifdef AbsoftUNIXFortran
int evwrite
#else
int evwrite_
#endif
(int *handle, const uint32_t *buffer)
{
    return(evWrite(*handle, buffer));
}


/**
 * This routine writes an evio event to an internal buffer containing evio data.
 * If that internal buffer is full, it is flushed to the final destination
 * file/socket/buffer opened with routine {@link evOpen}.
 * It writes data in evio version 4 format and returns a status.
 *
 * @param handle evio handle
 * @param buffer pointer to buffer containing event to write
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADMODE   if opened for reading in {@link evOpen}
 * @return S_EVFILE_TRUNC     if not enough room writing to a user-given buffer in {@link evOpen}
 * @return S_EVFILE_BADARG    if buffer is NULL
 * @return S_EVFILE_BADHANDLE if bad handle arg or wrong magic # in handle
 * @return S_EVFILE_ALLOCFAIL if memory cannot be allocated
 *                            (can still try this calling this function again)
 * @return errno              if file/socket write error
 * @return stream error       if file stream error
 */
int evWrite(int handle, const uint32_t *buffer)
{
    EVFILE   *a;
    int       status;
    uint32_t  nToWrite;

    /* Look up file struct (which contains block buffer) from handle */
    a = handle_list[handle-1];

    /* Check args */
    if (a == NULL) {
        return(S_EVFILE_BADHANDLE);
    }

    if (buffer == NULL) {
        return(S_EVFILE_BADARG);
    }

    /* Check magic # */
    if (a->magic != EV_MAGIC) {
        return(S_EVFILE_BADHANDLE);
        
    }
    
    /* Need to be open for writing not reading */
    if (a->rw != EV_WRITEFILE && a->rw != EV_WRITEPIPE &&
        a->rw != EV_WRITEBUF  && a->rw != EV_WRITESOCK) {
        return(S_EVFILE_BADMODE);
    }

    /* Number of words left to write = full event size + bank header */
    nToWrite = buffer[0] + 1;

    /* If adding this event to existing data would put us over the target block size ... */
    while (nToWrite + a->blksiz > a->blkSizeTarget) {

        /* Have a single large event to write which is bigger than the target
         * block size & larger than the block buffer - need larger buffer */
        if (a->blkEvCount < 1 && (nToWrite > a->bufSize - EV_HDSIZ)) {

            /* Keep old buffer around for a bit */
            uint32_t *pOldBuf = a->buf;
            
            /* Allocate more space */
            a->buf = (uint32_t *) malloc(4*(nToWrite + EV_HDSIZ));
            if (!(a->buf)) {
                /* Set things back to the way they were so if more memory
                 * is freed somewhere, can continue to call this function */
                a->buf = pOldBuf;
                return(S_EVFILE_ALLOCFAIL);
            }

            /* Copy old header into it */
            memcpy((void *)a->buf, (const void *)pOldBuf, 4*EV_HDSIZ);

            /* Update free space size, pointer to writing space, & block buffer size */
            a->left = nToWrite;
            a->next = a->buf + EV_HDSIZ;
            a->bufSize = nToWrite + EV_HDSIZ;
            
            /* Free old mem */
            free(pOldBuf);
            break;
        }
        /* Write what we already have and try again with a new, empty block */
        else {
            status = evFlush(a, 0);
            if (status != S_SUCCESS) {
                return(status);
            }
        }
    }
    
    /* Store number of events written, but do NOT include dictionary */
    if (!hasDictionary(a) || a->wroteDictionary > 0) {
        a->blkEvCount++;
        a->eventCount++;
    }
    else {
        a->wroteDictionary = 1;
    }

    /* Increased data size by this event size */
    a->blksiz += nToWrite;

    /* Copy data from buffer into file struct */
    memcpy((void *)a->next, (const void *)buffer, 4*nToWrite);
        
    a->next += nToWrite;
    a->left -= nToWrite;
    a->rwBytesOut += 4*nToWrite;

    /* If no space for writing left in block or reached the maximum
     * number of events per block, flush block to file/buf/socket */
    if (a->left < 1 || a->blkEvCount >= a->eventsMax) {
        status = evFlush(a, 0);
        if (status != S_SUCCESS) {
            return(status);
        }
    }

    return(S_SUCCESS);
}


/**
 * This routine returns the number of bytes written into a buffer so
 * far when given a handle provided by calling {@link evOpenBuffer}.
 * After the handle is closed, this no longer returns anything valid.
 *
 * @param handle evio handle
 * @param length pointer to int which gets filled with number of bytes
 *               written to buffer so far
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADHANDLE if bad handle arg or wrong magic # in handle
 */
int evGetBufferLength(int handle, uint32_t *length)
{
    EVFILE *a;

    /* Look up file struct (which contains block buffer) from handle */
    a = handle_list[handle-1];

    /* Check arg */
    if (a == NULL) {
        return(S_EVFILE_BADHANDLE);
    }
    
    /* Check magic # */
    if (a->magic != EV_MAGIC) {
        return(S_EVFILE_BADHANDLE);
    }

    if (length != NULL) {
        *length = a->rwBytesOut;
    }
    
    return(S_SUCCESS);
}


/**
 * This routine writes any existing evio format data in an internal buffer
 * (written to with {@link evWrite}) to the final destination
 * file/socket/buffer opened with routine {@link evOpen}.
 * It writes data in evio version 4 format and returns a status.
 * If writing to a buffer or file, it always places an empty block header
 * at the end - marked as the last block. If more events are written, the
 * last block header is overwritten.
 *
 * @param a pointer file structure
 * @param closing if != 0 then {@link evClose} is calling this function
 *
 * @return S_SUCCESS      if successful
 * @return S_EVFILE_TRUNC if not enough room writing to a user-given buffer in {@link evOpen}
 * @return errno          if file/socket write error
 * @return stream error   if file stream error
 */
static int evFlush(EVFILE *a, int closing)
{
    uint32_t  nBytes, bytesToWrite=0, blockHeaderBytes = 4*EV_HDSIZ;
    long pos;

    /* Store, in header, the actual, final block size */
    a->buf[EV_HD_BLKSIZ] = a->blksiz;
    
    /* Store, in header, the number of events in block */
    a->buf[EV_HD_COUNT] = a->blkEvCount;

    /* How much data do we write? */
    bytesToWrite = 4*a->blksiz;

    /* Write data only if there is something besides the block header to write */
    if (bytesToWrite > blockHeaderBytes) {
        /* Unmark this block as the last one to be written. */
        clearLastBlockBit(a);
        a->isLastBlock = 0;

        if (a->rw == EV_WRITEFILE) {
            /* Clear EOF and error indicators for file stream */
            clearerr(a->file);
            /* If "last block" (header only) already written out,
             * back up one block header and write over it.  */
            if (a->lastBlockOut) {
                /* Carefully change unsigned int (blockHeaderBytes) into neg # */
                pos = -1L * blockHeaderBytes;
                fseek(a->file, pos, SEEK_CUR);
            }
            /* Write block to file */
            nBytes = fwrite((const void *)a->buf, 1, bytesToWrite, a->file);
            /* Return any error condition of file stream */
            if (ferror(a->file)) return(ferror(a->file));
        }
        else if (a->rw == EV_WRITESOCK) {
            nBytes = tcpWrite(a->sockFd, a->buf, bytesToWrite);
        }
        else if (a->rw == EV_WRITEBUF) {
            /* If "last block" (header only) already written out,
             * back up one block header and write over it.  */
            if (a->lastBlockOut) {
                a->rwBytesUsed -= blockHeaderBytes;
            }
            
            /* If there's not enough space in the user-given
             * buffer to contain this block, return an error. */
            if (a->rwBufSize < a->rwBytesUsed + bytesToWrite) {
                return(S_EVFILE_TRUNC);
            }
            
            memcpy((a->rwBuf + a->rwBytesUsed), a->buf, bytesToWrite);
            a->rwBytesUsed += bytesToWrite;
            nBytes = bytesToWrite;
        }
    
        /* Return any error condition of write attempt - will only happen for file & socket */
        if (nBytes != bytesToWrite) return(errno);

        /* Initialize block header for next write */
        initBlockHeader(a);

        /* Track how many blocks written (first block # = 0) & put in header */
        a->buf[EV_HD_BLKNUM] = ++(a->blknum);
        /* Pointer to where next to write. In this case, the start
         * of first event header will be right after header. */
        a->next = a->buf + EV_HDSIZ;
        /* Space in number of words, not in header, left for writing in block buffer */
        a->left = a->bufSize - EV_HDSIZ;
        /* Total written size (words) = block header size so far */
        a->blksiz = EV_HDSIZ;
        /* How many bytes will end up in the buffer we're writing to? */
        a->rwBytesOut += blockHeaderBytes;
        /* No events in block yet */
        a->blkEvCount = 0;
    }

    /* If we're closing socket communications, send the last block header */
    if (closing && a->rw == EV_WRITESOCK) {
        /* Mark this block as the last one to be written. */
        setLastBlockBit(a);
        a->isLastBlock = 1;
        a->buf[EV_HD_BLKSIZ] = a->blksiz;
        bytesToWrite = 4*a->blksiz;
       
        nBytes = tcpWrite(a->sockFd, a->buf, bytesToWrite);
        if (nBytes != bytesToWrite) return(errno);
    }
    /* Now always write an empty, last block (just the header) since this is not a socket.
     * This ensures that if the data taking software crashes in the mean time,
     * we still have a file or buffer in the proper format.
     * Make sure that the "last block" bit is set. If we're not closing,
     * the block header simply gets over written in the next flush. */
    else if (a->rw != EV_WRITESOCK) {
        /* We only get here if all data has been written and therefore the
         * block buffer's header has been initialized. No need to do it again. */

        /* If we had no data to write, but flush was called previously and
         * an ending block header has already been written, we're done. */
        if (a->lastBlockOut && bytesToWrite <= blockHeaderBytes) {
            return(S_SUCCESS);
        }
        
        /* Mark this block as the last one to be written. */
        setLastBlockBit(a);
        a->isLastBlock = 1;
        a->buf[EV_HD_BLKSIZ] = EV_HDSIZ;
        bytesToWrite = 4*EV_HDSIZ;

        if (a->rw == EV_WRITEFILE) {
            /* Clear EOF and error indicators for file stream */
            clearerr(a->file);
            /* Write block header to file. This last block is always EV_HDSIZ ints long.
             * The regular block header may be larger but here we need only the
             * minimal length as a place holder for the next flush or as a marker
             * of the last block. */
            nBytes = fwrite((const void *)a->buf, 1, bytesToWrite, a->file);
            /* Return any error condition of file stream */
            if (ferror(a->file)) return(ferror(a->file));
        }
        else if (a->rw == EV_WRITEBUF) {
            /* If there's not enough space in the user-given
             * buffer to contain this block, return an error. */
            if (a->rwBufSize < a->rwBytesUsed + bytesToWrite) {
                return(S_EVFILE_TRUNC);
            }
            
            memcpy((a->rwBuf + a->rwBytesUsed), a->buf, bytesToWrite);
            nBytes = bytesToWrite;
            a->rwBytesUsed += bytesToWrite;
        }

        /* Return any error condition of write attempt */
        if (nBytes != bytesToWrite) return(errno);
       
        /* Remember that a "last" block was written out (perhaps to be
         * overwritten for files or buffers if evWrite called again). */
        a->lastBlockOut = 1;
    }
 
    return(S_SUCCESS);
}


/** Fortran interface to {@link evClose}. */
#ifdef AbsoftUNIXFortran
int evclose
#else
int evclose_
#endif
(int *handle)
{
    return(evClose(*handle));
}


/**
 * This routine flushes any existing evio format data in an internal buffer
 * (written to with {@link evWrite}) to the final destination
 * file/socket/buffer opened with routine {@link evOpen}.
 * It also frees up the handle so it cannot be used any more without calling
 * {@link evOpen} again.
 * Any data written is in evio version 4 format and any opened file is closed.
 * If reading, nothing is done.
 *
 * @param handle evio handle
 *
 * @return S_SUCCESS          if successful
 * @return S_FAILURE          if mapped memory does not unmap
 * @return S_EVFILE_TRUNC     if not enough room writing to a user-given buffer in {@link evOpen}
 * @return S_EVFILE_BADHANDLE if bad handle arg or wrong magic # in handle
 * @return p/fclose error     if pclose/fclose error
 */
int evClose(int handle)
{
    EVFILE *a;
    int status = S_SUCCESS, status2 = S_SUCCESS;
    
    /* Look up file struct from handle */
    a = handle_list[handle-1];

    /* Check arg */
    if (a == NULL) {
        return(S_EVFILE_BADHANDLE);
    }

    /* Check magic # */
    if (a->magic != EV_MAGIC) {
        return(S_EVFILE_BADHANDLE);
    }

    /* Flush everything to file/socket/buffer if writing */
    if (a->rw == EV_WRITEFILE || a->rw == EV_WRITEPIPE ||
        a->rw == EV_WRITEBUF  || a->rw == EV_WRITESOCK)  {
        /* Write last block. If no events left to write,
         * just write an empty block (header only). */
        status = evFlush(a, 1);
    }

    /* Close file */
    if (a->rw == EV_WRITEFILE || a->rw == EV_READFILE) {
        if (a->randomAccess) {
            status2 = munmap(a->mmapFile, a->mmapFileSize);
        }
        else {
            status2 = fclose(a->file);
        }
    }
    /* Pipes requires special close */
    else if (a->rw == EV_READPIPE || a->rw == EV_WRITEPIPE) {
        status2 = pclose(a->file);
    }

    /* Free up resources */
    handle_list[handle-1] = 0;
    if (a->buf != NULL) free((void *)(a->buf));
    if (a->dictionary != NULL) free(a->dictionary);
    free((void *)a);
    
    if (status == S_SUCCESS) {
        status = status2;
    }
    
    return(status);
}


/** Fortran interface to {@link evIoctl}. */
#ifdef AbsoftUNIXFortran
int evioctl
#else
int evioctl_
#endif
(int *handle, char *request, void *argp, int reqlen)
{
    char *req;
    int32_t status;
    req = (char *)malloc(reqlen+1);
    strncpy(req,request,reqlen);
    req[reqlen]=0;		/* insure request is null terminated */
    status = evIoctl(*handle,req,argp);
    free(req);
    return(status);
}


/**
 * This routine changes the target block size (in 32-bit words) for writes if request
 * = "B". If setting block size fails, writes can still continue with original
 * block size. Minimum size = 1K + 8(header) words.<p>
 * It returns the version number if request = "V".<p>
 * It changes the maximum number of events/block if request = "N".
 * Used only in version 4.<p>
 * It returns the total number of events in a file/buffer
 * opened for reading or writing if request = "E". Includes any
 * event added with {@link evWrite} call. Used only in version 4.<p>
 * It returns a pointer to the EV_HDSIZ block header ints if request = "H".
 * This pointer must be freed by the caller to avoid a memory leak.<p>
 * NOTE: all request strings are case insensitive. All version 4 commands to
 * version 3 files are ignored.
 *
 * @param handle  evio handle
 * @param request case independent string value of:
 *                "B"  for setting (target) block size;
 *                "V"  for getting evio version #;
 *                "N"  for setting max # of events/block;
 *                "H"  for getting EV_HDSIZ ints of block header info;
 *                "E"  for getting # of events in file/buffer;
 * @param argp    pointer to 32 bit int:
 *                  1) containing new block size in 32-bit words if request = B, or
 *                  2) containing new max number of events/block if request = N, or
 *                  3) returning version # if request = V, or
 *                  4) returning total # of original events in existing
 *                     file/buffer when reading or appending if request = E, or
 *                address of pointer to 32 bit int:
 *                  5) returning pointer to EV_HDSIZ uint32_t's of block header if request = H.
 *                     This pointer must be freed by caller since it points
 *                     to allocated memory.
 *
 * @return S_SUCCESS           if successful
 * @return S_FAILURE           if using sockets when request = E
 * @return S_EVFILE_BADARG     if request is NULL or argp is NULL
 * @return S_EVFILE_BADFILE    if file too small or problem reading file when request = E
 * @return S_EVFILE_BADHANDLE  if bad handle arg or wrong magic # in handle
 * @return S_EVFILE_ALLOCFAIL  if cannot allocate memory
 * @return S_EVFILE_UNXPTDEOF  if buffer too small when request = E
 * @return S_EVFILE_UNKOPTION  if unknown option specified in request arg
 * @return S_EVFILE_BADSIZEREQ when setting block size - if currently reading,
 *                              have already written events with different block size,
 *                              or is smaller than minimum allowed size (header size + 1K);
 *                              when setting max # events/blk - if val < 1
 * @return errno                if error in fseek, ftell when request = E
 */
int evIoctl(int handle, char *request, void *argp)
{
    EVFILE   *a;
    uint32_t *newBuf, *pHeader;
    int       err;
    uint32_t  eventsMax, blockSize;

    /* Look up file struct from handle */
    a = handle_list[handle-1];

    /* Check args */
    if (a == NULL) {
        return(S_EVFILE_BADHANDLE);
    }
    
    if (request == NULL) {
        return(S_EVFILE_BADARG);
    }

    /* Check magic # */
    if (a->magic != EV_MAGIC) {
        return(S_EVFILE_BADHANDLE);
    }

    switch (*request) {
        /************************/
        /* Specifing block size */
        /************************/
        case 'b':
        case 'B':
            /* Need to specify block size */
            if (argp == NULL) {
                return(S_EVFILE_BADARG);
            }
            
            /* Need to be writing not reading */
            if (a->rw != EV_WRITEFILE && a->rw != EV_WRITEPIPE) {
                return(S_EVFILE_BADSIZEREQ);
            }
            
            /* Cannot have already written events */
            if (a->blknum != 1 || a->blkEvCount != 0) {
                return(S_EVFILE_BADSIZEREQ);
            }

            /* Read in requested target block size */
            blockSize = *(uint32_t *) argp;

            /* If there is no change, return success */
            if (blockSize == a->blkSizeTarget) {
                return(S_SUCCESS);
            }
            
            /* If it's too small, return error */
            if (blockSize < EV_BLOCKSIZE_MIN) {
                return(S_EVFILE_BADSIZEREQ);
            }

            /* If we need a bigger block buffer ... */
            if (blockSize > a->bufSize) {
                /* Allocate memory for increased block size. If this fails
                 * we can still (theoretically) continue with writing. */
                newBuf = (uint32_t *) malloc(blockSize*4);
                if (newBuf == NULL) {
                    return(S_EVFILE_ALLOCFAIL);
                }
            
                /* Free allocated memory for current block */
                free(a->buf);

                a->buf = newBuf;

                /* Initialize block header */
                initBlockHeader(a);
                
                /* Remember size of new block buffer */
                a->bufSize = blockSize;
            }
                  
            /* Reset some file struct members */

            /* Recalculate how many words are left to write in block */
            a->left = blockSize - EV_HDSIZ;
            /* Store new target block size (final size,
             * a->blksiz, may be larger or smaller) */
            a->blkSizeTarget = blockSize;
            /* Next word to write is right after header */
            a->next = a->buf + EV_HDSIZ;

            break;

        /**************************/
        /* Getting version number */
        /**************************/
        case 'v':
        case 'V':
            /* Need to pass version back in pointer to int */
            if (argp == NULL) {
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
                return(S_EVFILE_BADARG);
            }
            
            pHeader = (uint32_t *)malloc(EV_HDSIZ*sizeof(int));
            if (pHeader == NULL) {
                return(S_EVFILE_ALLOCFAIL);
            }
            
            memcpy((void *)pHeader, (const void *)a->buf, EV_HDSIZ*sizeof(int));
            *((uint32_t **) argp) = pHeader;
            
            break;

        /**********************************************/
        /* Setting maximum number of events per block */
        /**********************************************/
        case 'n':
        case 'N':
            /* Need to specify # of events */
            if (argp == NULL) {
                return(S_EVFILE_BADARG);
            }
            
            eventsMax = *(uint32_t *) argp;
            if (eventsMax < 1) {
                return(S_EVFILE_BADSIZEREQ);
            }
            
            a->eventsMax = eventsMax;
            break;

        /****************************/
        /* Getting number of events */
        /****************************/
        case 'e':
        case 'E':
            /* Need to pass # of events bank in pointer to int */
            if (argp == NULL) {
                return(S_EVFILE_BADARG);
            }

            err = getEventCount(a, (int32_t *) argp);
            if (err != S_SUCCESS) {
                return(err);
            }

            break;

        default:
            return(S_EVFILE_UNKOPTION);
    }

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
int evGetRandomAccessTable(int handle, const uint32_t ***table, uint32_t *len) {
    EVFILE *a;

    /* Look up file struct from handle */
    a = handle_list[handle-1];

    /* Check args */
    if (a == NULL) {
        return(S_EVFILE_BADHANDLE);
    }

    if (table == NULL || len == NULL) {
        return (S_EVFILE_BADARG);
    }

    /* Must be in random access mode */
    if (!a->randomAccess) {
        return(S_EVFILE_BADMODE);
    }
            
    *table = a->pTable;
    *len = a->eventCount;

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
 *                   string length if there is one, else filled with 0.
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

    /* Look up file struct from handle */
    a = handle_list[handle-1];

    /* Check args */
    if (a == NULL) {
        return(S_EVFILE_BADHANDLE);
    }

    if (dictionary == NULL) {
        return (S_EVFILE_BADARG);
    }

    /* If we have a dictionary ... */
    if (a->dictionary != NULL) {
        /* Copy it and return it */
        dictCpy = strdup(a->dictionary);
        if (dictCpy == NULL) {
            return(S_EVFILE_ALLOCFAIL);
        }
        *dictionary = dictCpy;

        /* Send back the length if caller wants it */
        if (len != NULL) {
            *len = strlen(a->dictionary);
        }
    }
    else {
        *dictionary = NULL;
        if (len != NULL) {
            *len = 0;
        }
    }
    
    return S_SUCCESS;
}


/**
 * This routine writes an optional dictionary as the first event of an
 * evio file/sokcet/buffer. The dictionary is <b>not</b> included in
 * any event count.
 *
 * @param handle        evio handle
 * @param xmlDictionary string containing xml format dictionary or
 *                      NULL to remove previously specified dictionary
 *
 * @return S_SUCCESS           if successful
 * @return S_FAILURE           if already written events/dictionary
 * @return S_EVFILE_BADMODE    if reading or appending
 * @return S_EVFILE_BADARG     if dictionary in wrong format
 * @return S_EVFILE_ALLOCFAIL  if cannot allocate memory
 * @return S_EVFILE_BADHANDLE  if bad handle arg or wrong magic # in handle
 * @return errno               if file/socket write error
 * @return stream error        if file stream error
 */
int evWriteDictionary(int handle, char *xmlDictionary)
{
    EVFILE  *a;
    char    *pChar;
    uint32_t *dictBuf;
    int      i, status, dictionaryLen, padSize, bufSize, pads[] = {4,3,2,1};

    /* Look up file struct from handle */
    a = handle_list[handle-1];

    /* Check args */
    if (a == NULL) {
        return(S_EVFILE_BADHANDLE);
    }

/* TODO: better check out format */
    /* Minimum length of dictionary xml to be in proper format is 35 chars. */
    if (strlen(xmlDictionary) < 35) {
        return(S_EVFILE_BADARG);
    }

    /* Check magic # */
    if (a->magic != EV_MAGIC) {
        return(S_EVFILE_BADHANDLE);
    }
            
    /* Need to be writing not reading */
    if (a->rw != EV_WRITEFILE && a->rw != EV_WRITEPIPE &&
        a->rw != EV_WRITEBUF  && a->rw != EV_WRITESOCK) {
        return(S_EVFILE_BADMODE);
    }
    
    /* Cannot have already written event or dictionary */
    if (a->blknum != 1 || a->blkEvCount != 0 || a->wroteDictionary) {
        return(S_FAILURE);
    }

    /* Cannot be appending */
    if (a->append) {
        return(S_EVFILE_BADMODE);
    }

    /* Clear any previously specified dictionary (should never happen) */
    if (a->dictionary != NULL) {
        free(a->dictionary);
    }

    /* Store dictionary */
    a->dictionary = strdup(xmlDictionary);
    if (a->dictionary == NULL) {
        return(S_EVFILE_ALLOCFAIL);
    }
    dictionaryLen = strlen(xmlDictionary);
    
    /* Wrap dictionary in bank. Format for string array requires that
     * each string is terminated by a NULL and there must be at least
     * one ASCII char = '\4' for padding at the end (tells us that this
     * is the new format capable of storing arrays and not just a
     * single string). Add any necessary padding to 4 byte boundaries
     * with char = '\4'. */
    padSize = pads[ (dictionaryLen + 1)%4 ];
    
    /* Size (in bytes) = 2 header ints + dictionary chars + NULL + padding */
    bufSize = 2*sizeof(int32_t) + dictionaryLen + 1 + padSize;
    
    /* Allocate memory */
    dictBuf = (uint32_t *) malloc(bufSize);
    if (dictBuf == NULL) {
        free(a->dictionary);
        return(S_EVFILE_ALLOCFAIL);
    }
    
    /* Write bank length (in 32 bit ints) */
    dictBuf[0] = bufSize/4 - 1;
    /* Write tag(0), type(8 bit char *), & num(0) */
    dictBuf[1] = 0x3 << 8;
    /* Write chars */
    pChar = (char *) (dictBuf + 2);
    memcpy((void *) pChar, (const void *)xmlDictionary, dictionaryLen);
    pChar += dictionaryLen;
    /* Write NULL */
    *pChar = '\0';
    pChar++;
    /* Write padding */
    for (i=0; i < padSize; i++) {
        *pChar = '\4';
        pChar++;
    }

    /* Set bit in block header that there is a dictionary */
    setDictionaryBit(a);

    /* Write event */
    status = evWrite(handle, dictBuf);

    free(dictBuf);
    
    return(status);
}


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
 * @param type numerical value of an evio type
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




