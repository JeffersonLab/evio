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
 *
 * Routines:
 * ---------
 * 
 * int  evOpen            (char *filename, char *flags, int *handle)
 * int  evOpenBuffer      (char *buffer, int bufLen, char *flags, int *handle)
 * int  evOpenSocket      (int sockFd, char *flags, int *handle)
 * int  evRead            (int handle, uint32_t *buffer, int size)
 * int  evReadAlloc       (int handle, uint32_t **buffer, int *buflen)
 * int  evWrite           (int handle, const uint32_t *buffer)
 * int  evIoctl           (int handle, char *request, void *argp)
 * int  evClose           (int handle)
 * int  evGetBufferLength (int handle, int *length)
 * int  evGetDictionary   (int handle, char **dictionary, int *len)
 * int  evWriteDictionary (int handle, char *xmlDictionary)
 * int  evIsContainer     (int type)
 * char *evPerror         (int error)
 * const char *evGetTypename (int type)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <evio.h>

/**
 * This structure contains information about file
 * opened for either reading or writing.
 */
typedef struct evfilestruct {
  FILE    *file;         /**< pointer to file. */
  uint32_t *buf;          /**< pointer to buffer of block being read/written. */
  uint32_t *next;         /**< pointer to next word in block to be read/written. */
  int   left;            /**< # of valid 32 bit unread/unwritten words in block. */
  int   blksiz;          /**< size of block in 32 bit words - v3 or
                          *   size of actual data - v4. */
  int   blknum;          /**< block number. */
  int   blkNumDiff;      /**< When reading, the difference between blknum read in and
                          *   the expected (sequential) value. Used in debug message. */
  int   rw;              /**< are we reading, writing, piping? */
  int   magic;           /**< magic number. */
  int   evnum;           /**< total # of events written so far. */
  
  int   byte_swapped;    /**< bytes do NOT need swapping = 0 else 1 */

  /* new struct members for version 4 */
  int   version;         /**< evio version number. */

  /* buffer stuff */
  char *rwBuf;           /**< pointer to buffer if reading/writing from/to buffer. */
  int   rwBufSize;       /**< size of rwBuf buffer in bytes. */
  int   rwBufUsed;       /**< number of bytes read/written from/to rwBuf so far (after each flush). */
  int   rwBytesOut;      /**< number of bytes read/written from/to rwBuf so far (after each evWrite). */

  /* socket stuff */
  int   sockFd;          /**< socket file descriptor if reading/writing from/to socket. */

  /* block stuff */
  int   blkSizeTarget;   /**< target size of block in 32 bit words (including block header). */
  int   bufSize;         /**< size of block buffer (buf) in 32 bit words. */
  int   eventsMax;       /**< max number of events per block. */
  int   isLastBlock;     /**< 1 if buf contains last block of file/sock/buf, else 0. */
  int   evCount;         /**< number of events written to block so far. */

  /* dictionary */
  char *dictionary;      /**< xml format dictionary to either read or write. */

} EVFILE;


/* A few items to make the code more readable */
#define EV_READFILE  0
#define EV_READPIPE  1
#define EV_READSOCK  2
#define EV_READBUF   3

#define EV_WRITEFILE 4
#define EV_WRITEPIPE 5
#define EV_WRITESOCK 6
#define EV_WRITEBUF  7


/** Version 3's fixed block size in 32 bit words */
#define EV_BLOCKSIZE_V3 8192

/** Version 4's target block size in 32 bit words (2MB) */
#define EV_BLOCKSIZE_V4 500000

/** Number used to determine data endian */
#define EV_MAGIC 0xc0da0100

/** Size of block header in 32 bit words */
#define EV_HDSIZ 8

/** Minimum block size allowed if size reset */
#define EV_BLOCKSIZE_MIN (EV_HDSIZ + 1024)

/** In version 4, lowest 8 bits are version, rest is bit info */
#define EV_VERSION_MASK 0xFF

/** In version 4, dictionary presence is 9th bit in version/info word */
#define EV_DICTIONARY_MASK 0x100

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
 *      Header Length      = number of ints in this header (always 8)
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
#define setDictionaryBit(a)  (a->buf[EV_HD_VER] |= 0x100)
/** Is there a dictionary in this block? */
#define hasDictionary(a)    ((a->buf[EV_HD_VER] & 0x100) > 0 ? 1 : 0)
/** Turn on 10th bit to indicate last block of file/transmission */
#define setLastBlockBit(a)   (a->buf[EV_HD_VER] |= 0x200)
/** Is this the last block of file/transmission? */
#define isLastBlock(a)      ((a->buf[EV_HD_VER] & 0x200) > 0 ? 1 : 0)

/* Prototypes for static routines */
static  int      evOpenImpl(char *srcDest, int bufLen, int sockFd, char *flags, int *handle);
static  int      evGetNewBuffer(EVFILE *a);
static  int      evFlush(EVFILE *a);
static  void     initBlockHeader(EVFILE *a);
static  char *   evTrim(char *s, int skip);
static  int      tcpWrite(int fd, const void *vptr, int n);
static  int      tcpRead(int fd, void *vptr, int n);
static  int      evReadAllocImpl(EVFILE *a, uint32_t **buffer, int *buflen);

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
 * @param flags     pointer to case-independent string of "w" for writing to
 *                  or "r" for reading from a file
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
    if (strcasecmp(flags, "w") != 0 && strcasecmp(flags, "r") != 0) {
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
 * @param flags   pointer to case-independent string of "w" & "r"
 *                for reading/writing from/to a buffer.
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
int evOpenBuffer(char *buffer, int bufLen, char *flags, int *handle)
{
    char *flag;
    
    /* Check flags & translate them */
    if (strcasecmp(flags, "w") == 0) {
        flag = "wb";
    }
    else if (strcasecmp(flags, "r") == 0) {
        flag = "rb";
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
 *                reading/writing from/to a socket
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
 * @param flags   pointer to case-independent string of "w" for writing to
 *                or "r" for reading from a file, "ws" & "rs" for reading/writing
 *                from/to a socket, and "wb" & "rb" for reading/writing from/to a buffer.
 * @param handle  pointer to int which gets filled with handle
 * 
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_BADARG    if flags or handle arg is NULL;
 *                            if srcDest arg is NULL when using file or buffer;
 *                            if unrecognizable flags;
 *                            if buffer size too small when using buffer
 * @return S_EVFILE_ALLOCFAIL if memory allocation failed
 * @return S_EVFILE_BADFILE   if error reading file, unsupported version,
 *                            or contradictory data in file 
 * @return S_EVFILE_BADHANDLE if no memory available to store handle structure
 *                            (increase MAXHANDLES in evio.c and recompile)
 */
static int evOpenImpl(char *srcDest, int bufLen, int sockFd, char *flags, int *handle)
{
    EVFILE *a;
    char *filename, *buffer;
    int useFile=0, useBuffer=0, useSocket=0, reading=0;
    int i, nBytes, rwBufSize, blk_size, hdr_size, version, ihandle, bytesToRead;
    int32_t temp, header[EV_HDSIZ], headerInfo;

    /* Check args */
    if (flags == NULL || handle == NULL) {
        return(S_EVFILE_BADARG);
    }

    
    /* Are we dealing with a file, buffer, or socket? */
    if (strcasecmp(flags, "w") == 0 || strcasecmp(flags, "r") == 0) {
        useFile = 1;
        
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
    else if (strcasecmp(flags, "wb") == 0 || strcasecmp(flags, "rb") == 0) {
        useBuffer = 1;
        
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
    else if (strcasecmp(flags, "ws") == 0 || strcasecmp(flags, "rs") == 0) {
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
#else
            a->rw = EV_READFILE;

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
            else {
                a->file = fopen(filename,"r");
            }
#endif
            if (a->file == NULL) {
                free(a);
#ifdef DEBUG
                fprintf(stderr,"evOpen: Error opening file %s, flag %s\n", filename,flags);
                perror(NULL);
#endif
                *handle = 0;
                free(filename);
                return(errno);
            }

            /* Read in header */
            nBytes = sizeof(header)*fread(header, sizeof(header), 1, a->file);
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
            memcpy(header, (a->rwBuf + a->rwBufUsed), nBytes);
            a->rwBufUsed += nBytes;
        }

        /***********************************/
        /* Run header through some checks. */
        /***********************************/

        /* Check to see if all bytes are there */
        if (nBytes != sizeof(header)) {
            /* Close file and free memory */
            if (useFile) {
                if (a->rw == EV_READFILE) {
                    fclose(a->file);
                }
                else if (a->rw == EV_READPIPE) {
                    pclose(a->file);
                }
                free(filename);
            }
            free(a);
            return(S_EVFILE_BADFILE);
        }
printf("Header bytes are all read in, magic # = %#10.8x, swapped = %#10.8x\n",
       header[EV_HD_MAGIC],  EVIO_SWAP32(header[EV_HD_MAGIC]));

        /* Check endianness */
        if (header[EV_HD_MAGIC] != EV_MAGIC) {
            temp = EVIO_SWAP32(header[EV_HD_MAGIC]);
            if (temp == EV_MAGIC) {
printf("SWAPPING\n");
                a->byte_swapped = 1;
            }
            else {
printf("Magic # is a bad value\n");
                if (useFile) {
                    if (a->rw == EV_READFILE) {
                        fclose(a->file);
                    }
                    else if (a->rw == EV_READPIPE) {
                        pclose(a->file);
                    }
                    free(filename);
                }
                free(a);
                return(S_EVFILE_BADFILE);
            }
        }
        else {
printf("NO SWAPPING\n");
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
printf("VERSION = %d\n", version);
        if (version < 1 || version > 4) {
printf("Header has unsupported evio version (%d), quit\n", version);
            if (useFile) {
                if (a->rw == EV_READFILE) {
                    fclose(a->file);
                }
                else if (a->rw == EV_READPIPE) {
                    pclose(a->file);
                }
                free(filename);
            }
            free(a);
            return(S_EVFILE_BADFILE);
        }
        a->version = version;

        /* Check the header's value for header size with our assumption. */
        hdr_size = header[EV_HD_HDSIZ];
        if (a->byte_swapped) {
            hdr_size = EVIO_SWAP32(hdr_size);
        }
        if (hdr_size != EV_HDSIZ) {
printf("Header size was assumed to be %d but the file said it was %d, quit\n", EV_HDSIZ, hdr_size);
            if (useFile) {
                if (a->rw == EV_READFILE) {
                    fclose(a->file);
                }
                else if (a->rw == EV_READPIPE) {
                    pclose(a->file);
                }
                free(filename);
            }
            free(a);
            return(S_EVFILE_BADFILE);
        }
        
        /**********************************/
        /* Allocate buffer to store block */
        /**********************************/

        /* size of block we're reading */
        blk_size = header[EV_HD_BLKSIZ];
        if (a->byte_swapped) {
            blk_size = EVIO_SWAP32(blk_size);
        }
        a->blksiz = blk_size;
printf("BLOCK SIZE = %d\n", blk_size);

        /* How big do we make this buffer? Use a minimum size. */
        a->bufSize = blk_size < EV_BLOCKSIZE_MIN ? EV_BLOCKSIZE_MIN : blk_size;
        a->buf = (uint32_t *)malloc(4*a->bufSize);

        /* Error if can't allocate buffer memory */
        if (a->buf == NULL) {
            if (useFile) {
                if (a->rw == EV_READFILE) {
                    fclose(a->file);
                }
                else if (a->rw == EV_READPIPE) {
                    pclose(a->file);
                }
                free(filename);
            }
            free(a);
            return(S_EVFILE_ALLOCFAIL);
        }

        /* Copy header into block (swapping if necessary) */
        if (a->byte_swapped) {
            swap_int32_t((uint32_t *)header, EV_HDSIZ, (uint32_t *)a->buf);
        }
        else {
            memcpy(a->buf, header, EV_HDSIZ*4);
        }

        
        /**********************/
        /* Read rest of block */
        /**********************/
        bytesToRead = 4*(blk_size - EV_HDSIZ);
        if (useFile) {
            nBytes = 4*fread(a->buf+EV_HDSIZ, 4, bytesToRead/4, a->file);
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
            nBytes = 4*(blk_size - EV_HDSIZ);
            memcpy(a->buf+EV_HDSIZ, (a->rwBuf + a->rwBufUsed), bytesToRead);
            a->rwBufUsed += nBytes;
        }

        /* Check to see if all bytes were read in */
        if (nBytes != bytesToRead) {
            if (useFile) {
                if (a->rw == EV_READFILE) {
                    fclose(a->file);
                }
                else if (a->rw == EV_READPIPE) {
                    pclose(a->file);
                }
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
            a->next = a->buf + EV_HDSIZ;
            
            /* Number of valid 32 bit words = (full block size - header size) in v4+ */
            a->left = (a->buf)[EV_HD_BLKSIZ] - EV_HDSIZ;
        
            /* Is this the last block? */
            a->isLastBlock = isLastBlock(a);
if (a->isLastBlock) printf("LAST BLOCK\n");
            /* Pull out dictionary if there is one (works only after header is swapped). */
            if (hasDictionary(a)) {
printf("HAVE DICTIONARY\n");
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
        
    /*************************/
    /* If we're writing  ... */
    /*************************/
    else {

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
            else {
                a->file = fopen(filename,"w");
            }
#endif
            if (a->file == NULL) {
                free(a);
#ifdef DEBUG
                fprintf(stderr,"evOpen: Error opening file %s, flag %s\n", filename,flags);
                perror(NULL);
#endif
                *handle = 0;
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

    } /* if writing */
    
          
    /* Store general info in handle structure */
    a->blknum = a->buf[EV_HD_BLKNUM];

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
        if (a->rw == EV_WRITEFILE || a->rw == EV_READFILE) {
            fclose(a->file);
        }
        else if (a->rw == EV_WRITEPIPE || a->rw == EV_READPIPE) {
            pclose(a->file);
        }
        free(filename);
    }
    free(a->buf);
    free(a);
    return(S_EVFILE_BADHANDLE);        /* A better error code would help */
}


/**
 * This function opens an evio format file for either reading or writing.
 * Works with all versions of evio for reading, but only writes version 4
 * format. A handle is returned for use with calling other evio routines.
 * NO LONGER USED.
 *
 * @param fname  name of file
 * @param flags  pointer to string containing "w" for write or "r" for read
 * @param handle pointer to int which gets filled with handle
 *
 * @return S_SUCCESS          if successful
 * @return S_EVFILE_ALLOCFAIL if memory allocation failed
 * @return S_EVFILE_UNKOPTION if unknown flags specified
 * @return S_EVFILE_BADFILE   if error reading file, unsupported version,
 *                            or contradictory data in file
 * @return S_EVFILE_BADHANDLE if no memory available to store handle structure
 *                            (increase MAXHANDLES in evio.c and recompile)
 * @return errno              if file could not be opened (handle = 0)
 */
static int32_t evOpenOrig(char *fname, char *flags, int32_t *handle)
{
    EVFILE *a;
    size_t nBytes;
    char *filename;
    int32_t ihandle, temp, blk_size, hdr_size, header[EV_HDSIZ], version;
  
    filename = strdup(fname);
    if (filename == NULL) {
        return(S_EVFILE_ALLOCFAIL);
    }
    
    /* Allocate control structure (mem zeroed) or quit */
    a = (EVFILE *)calloc(1, sizeof(EVFILE));
    if (!a) {
        free(filename);
        return(S_EVFILE_ALLOCFAIL);
    }

    /* Trim whitespace from filename front & end */
    evTrim(filename, 0);

    switch (*flags) {

        case '\0':
        case 'r':
        case 'R':
            /*************************************************/
            /* If we're reading a version 1, 2 or 3 file ... */
            /*************************************************/
            
#if defined VXWORKS || defined _MSC_VER
            /* No pipe or zip/unzip support in vxWorks */
            a->file = fopen(filename,"r");
            a->rw = EV_READFILE;
#else
            a->rw = EV_READFILE;
            if (strcmp(filename,"-") == 0) {
                a->file = stdin;
            }
            /* If input filename is standard output of command ... */
            else if(filename[0] == '|') {
                /* Open a process by creating a unidirectional pipe, forking, and
                invoking the shell. The "filename" is a shell command line.
                This command is passed to /bin/sh using the -c flag.
                Make sure we know to use pclose instead of fclose. */
                a->file = popen(filename+1,"r");
                a->rw = EV_READPIPE;
            }
            else {
                a->file = fopen(filename,"r");
            }
#endif
            if (a->file) {
                /* Read in header */
                nBytes = sizeof(header)*fread(header, sizeof(header), 1, a->file);
                            
                /* Check to see if all bytes are there */
                if (nBytes != sizeof(header)) {
                    /* Close file and free memory */
                    fclose(a->file);
                    free(a);
                    free(filename);
                    return(S_EVFILE_BADFILE);
                }
            
                /* Check endianness */
                if (header[EV_HD_MAGIC] != EV_MAGIC) {
                    temp = EVIO_SWAP32(header[EV_HD_MAGIC]);
                    if (temp == EV_MAGIC) {
                        a->byte_swapped = 1;
                    }
                    else {
                        fclose(a->file);
                        free(a);
                        free(filename);
                        return(S_EVFILE_BADFILE);
                    }
                }
                else {
                    a->byte_swapped = 0;
                }
            
                /* Check VERSION */
                version = header[EV_HD_VER];
                if (a->byte_swapped) {
                    version = EVIO_SWAP32(version);
                }
                /* only lowest 8 bits count in version 4's header word */
                version &= EV_VERSION_MASK;
                if (version < 1 || version > 4) {
printf("Header has unsupported evio version (%d), quit\n", version);
                    fclose(a->file);
                    free(a);
                    free(filename);
                    return(S_EVFILE_BADFILE);
                }
            
                /* Check the header's value for header size with our assumption. */
                hdr_size = header[EV_HD_HDSIZ];
                if (a->byte_swapped) {
                    hdr_size = EVIO_SWAP32(hdr_size);
                }
                if (hdr_size != EV_HDSIZ) {
printf("Header size was assumed to be %d but the file said it was %d, quit\n", EV_HDSIZ, hdr_size);
                    fclose(a->file);
                    free(a);
                    free(filename);
                    return(S_EVFILE_BADFILE);
                }
            
                /* Allocate bytes for 1 fixed-size block */
                blk_size = header[EV_HD_BLKSIZ];
                if (a->byte_swapped) {
                    blk_size = EVIO_SWAP32(blk_size);
                }
                a->blksiz = blk_size;
                a->buf = (uint32_t *)malloc(blk_size*4);
               
                /* Error if can't allocate buffer memory */
                if (!(a->buf)) {
                    free(a);
                    free(filename);
                    return(S_EVFILE_ALLOCFAIL);
                }
            
                /* Copy header into block (swapping if necessary) */
                if (a->byte_swapped) {
                    swap_int32_t((uint32_t *)header, EV_HDSIZ, (uint32_t *)a->buf);
                }
                else {
                    memcpy(a->buf, header, EV_HDSIZ*4);
                }
                            
                /* Read rest of block from file */
                fread(a->buf+EV_HDSIZ, 4, (blk_size - EV_HDSIZ), a->file);
            
                /* Pointer to where start of first event header occurs. */
                a->next = a->buf + (a->buf)[EV_HD_START];
            
                /* Number of valid 32 bit words from start of first event to end of block */
                a->left = (a->buf)[EV_HD_USED] - (a->buf)[EV_HD_START];

            }
            break;
        
            
        case 'w':
        case 'W':
            /*******************************/
            /* If we're writing a file ... */
            /*******************************/

#if defined  VXWORKS || defined _MSC_VER
            a->file = fopen(filename,"w");
            a->rw = EV_WRITEFILE;
#else
            a->rw = EV_WRITEFILE;
            if(strcmp(filename,"-") == 0) {
                a->file = stdout;
            }
            else if(filename[0] == '|') {
                a->file = popen(filename+1,"w");
                a->rw = EV_WRITEPIPE;   /* Make sure we know to use pclose */
            }
            else {
                a->file = fopen(filename,"w");
            }
#endif

            if (a->file) {
                /* Allocate memory for a block */
                a->buf = (uint32_t *) malloc(EV_BLOCKSIZE_V4*4);
                if (!(a->buf)) {
                    free(a);
                    free(filename);
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
                /* Max # of events/block */
                a->eventsMax = EV_EVENTS_MAX;
                /* Remember size of block buffer */
                a->bufSize = EV_BLOCKSIZE_V4;
                
            }
            break;
            
        default:
            free(a);
            free(filename);
            return(S_EVFILE_UNKOPTION);
            break;
            
    }   /* switch (*flags) */
          
    
    /* If we successfully opened a file for either reading or writing ... */
    if (a->file) {
        a->magic  = EV_MAGIC;
        a->blknum = a->buf[EV_HD_BLKNUM];
          
        for (ihandle=0; ihandle < MAXHANDLES; ihandle++) {
            /* If a slot is available ... */
            if (handle_list[ihandle] == 0) {
                handle_list[ihandle] = a;
                *handle = ihandle+1;
                free(filename);
                return(S_SUCCESS);
            }
        }
        /* No slots left */
        *handle = 0;
        free(a->buf);
        free(a);
        free(filename);
        return(S_EVFILE_BADHANDLE);        /* A better error code would help */
    }
    else {
        free(a);
#ifdef DEBUG
        fprintf(stderr,"evOpen: Error opening file %s, flag %s\n", filename,flags);
        perror(NULL);
#endif
        *handle = 0;
        free(filename);
        return(errno);
    }
  
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
 * @return S_FAILURE          if opened for writing in {@link evOpen}
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
static int evReadAllocImpl(EVFILE *a, uint32_t **buffer, int *buflen)
{
    uint32_t *buf, *pBuf;
    int       nleft, ncopy, error, status, len;


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
        return(S_FAILURE);
    }
    
    /* If no more data left to read from current block, get a new block */
    if (a->left <= 0) {
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
        if (a->left <= 0) {
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
 * @return S_FAILURE          if opened for writing in {@link evOpen}
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
int evReadAlloc(int handle, uint32_t **buffer, int *buflen)
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
(int *handle, uint32_t *buffer, int *buflen)
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
 * @return S_FAILURE          if opened for writing in {@link evOpen}
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
int evRead(int handle, uint32_t *buffer, int buflen)
{
    EVFILE *a;
    int     nleft, ncopy, error, status;
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
        return(S_FAILURE);
    }
    
    /* If no more data left to read from current block, get a new block */
    if (a->left <= 0) {
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
        if (a->left <= 0) {
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
 * If the data needs to be swapped, it is stored in a static buffer. Thus any other
 * call to read routines will cause the swapped data to be overwritten.
 * No writing to the returned pointer is allowed.
 * Works only with evio version 4 and up. A status is returned.
 * NOTE: a bug is that the last event read which needs swapping, does NOT get freed
 *
 * @param handle evio handle
 * @param buffer pointer to pointer to buffer gets filled with pointer to location in
 *               internal buffer which is guaranteed to be valid only until the next
 *              {@link evRead}, {@link evReadNoAlloc}, or {@link evReadNoCopy} call.
 * @param buflen pointer to int gets filled with length of buffer in 32 bit words
 *               including the full (8 byte) header
 *
 * @return S_SUCCESS          if successful
 * @return S_FAILURE          if opened for writing in {@link evOpen}
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
int evReadNoCopy(int handle, const uint32_t **buffer, int *buflen)
{
    EVFILE *a;
    int     nleft, status;
    static uint32_t *tempBuf = NULL;


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

    /* Need to be reading (not from socket) and not writing */
    if (a->rw != EV_READFILE && a->rw != EV_READPIPE &&
        a->rw != EV_READBUF  && a->rw != EV_READSOCK) {
        return(S_FAILURE);
    }
    
    /* If no more data left to read from current block, get a new block */
    if (a->left <= 0) {
        status = evGetNewBuffer(a);
        if (status != S_SUCCESS) {
            return(status);
        }
    }

    /* Get rid of the last event read in & swapped. */
    if (tempBuf != NULL) {
        free(tempBuf);
        tempBuf = NULL;
    }

    /* Find number of words to read in next event (including header) */
    if (a->byte_swapped) {
        /* Length of next bank, including header, in 32 bit words */
        nleft = EVIO_SWAP32(*(a->next)) + 1;
        
        /* Create temp buffer for swapping */
        tempBuf = (uint32_t *) malloc(nleft*sizeof(uint32_t));
        if (tempBuf == NULL) return(S_EVFILE_ALLOCFAIL);
                
        /* swap data into buffer */
        evioswap(a->next, 1, tempBuf);

        /* return location of this temp buffer */
        *buffer = tempBuf;
    }
    else {
        /* Length of next bank, including header, in 32 bit words */
        nleft = *(a->next) + 1;
        
        /* return location of place of event in block buffer */
        *buffer = a->next;
    }

    *buflen = nleft;

    a->next += nleft;
    a->left -= nleft;

    return(S_SUCCESS);
}


/**
 * Routine to get the next block.
 * NO LONGER USED.
 * 
 * @param a pointer file structure
 * 
 * @return S_SUCCESS          if successful
 * @return EOF                if end-of-file reached
 * @return errno              if file read error
 * @return stream error       if file stream error
 * @return S_EVFILE_BADFILE   if file has bad magic #
 * @return S_EVFILE_UNXPTDEOF if unexpected EOF while reading data (perhaps bad block header)
 */
static int32_t evGetNewBufferOrig(EVFILE *a)
{
    int32_t nread, status;
    
    status = S_SUCCESS;

    /* If end-of-file, return EOF as status */
    if (feof(a->file)) {
        return(EOF);
    }

    /* Clear EOF and error indicators for file stream */
    clearerr(a->file);

    /* Why set magic # to zero ?? Will make NO difference. */
    // TODO: get rid of useless statement
    a->buf[EV_HD_MAGIC] = 0;

    /* Read a block of data from file */
    nread = fread(a->buf, 4, a->blksiz, a->file);

    /* Swap header if necessary */
    if (a->byte_swapped) {
        swap_int32_t((unsigned int*)a->buf, EV_HDSIZ, NULL);
    }

    /* Return end-of-file if so */
    if (feof(a->file)) return(EOF);
    
    /* Return any error condition of file stream */
    if (ferror(a->file)) return(ferror(a->file));
    
    /* Return any error condition of read attempt */
    if (nread != a->blksiz) return(errno);

    /* Check magic # */
    if (a->buf[EV_HD_MAGIC] != EV_MAGIC) {
        return(S_EVFILE_BADFILE);
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
     * event, this should point to the next one.*/
    a->next = a->buf + (a->buf)[EV_HD_HDSIZ];

    /* Number of valid words left to read in block */
    a->left = (a->buf)[EV_HD_USED] - (a->buf)[EV_HD_HDSIZ];

    /* If there are (unexpectedly) no valid data left in block, return error */
    if (a->left <= 0) {
        return(S_EVFILE_UNXPTDEOF);
    }

    return(status);
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
    uint32_t *newBuf;
    int nread, nBytes, status, blkSize, bytesToRead;

    status = S_SUCCESS;

    /* See if we read in the last block the last time this was called (v4) */
    if (a->version > 3 && a->isLastBlock) {
/*printf("evGetNewBuffer: read in LAST BLOCK, return EOF\n");*/
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
        nBytes = 4*fread(a->buf, 4, bytesToRead/4, a->file);
        
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
        if (a->rwBufSize < a->rwBufUsed + bytesToRead) {
#ifdef DEBUG
            fprintf(stderr,"evGetNewBuffer: ran out of buffer to read 1\n");
#endif
            return(S_EVFILE_UNXPTDEOF);
        }
        memcpy(a->buf, (a->rwBuf + a->rwBufUsed), bytesToRead);
        nBytes = bytesToRead;
        a->rwBufUsed += bytesToRead;
    }

    /* Return any read error */
    if (nBytes != bytesToRead) {
        return(errno);
    }

    /* Swap header in place if necessary */
    if (a->byte_swapped) {
        swap_int32_t((uint32_t *)a->buf, EV_HDSIZ, NULL);
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
        memcpy((void *)newBuf, (void *)a->buf, 4*EV_HDSIZ);
        
        /* bookkeeping */
        a->bufSize = a->blksiz;
        free(a->buf);
        a->buf = newBuf;
    }

    /* Read rest of block */
    bytesToRead = 4*(a->blksiz - EV_HDSIZ);
    if (a->rw == EV_READFILE) {
        nBytes = 4*fread((a->buf + EV_HDSIZ), 4, bytesToRead/4, a->file);
        if (feof(a->file)) {return(EOF);}
        if (ferror(a->file)) {return(ferror(a->file));}
    }
    else if (a->rw == EV_READSOCK) {
        nBytes = tcpRead(a->sockFd, (a->buf + EV_HDSIZ), bytesToRead);
    }
    else if (a->rw == EV_READBUF) {
        if (a->rwBufSize < a->rwBufUsed + bytesToRead) {
#ifdef DEBUG
            fprintf(stderr,"evGetNewBuffer: ran out of buffer to read 2\n");
#endif
            return(S_EVFILE_UNXPTDEOF);
        }
        memcpy((a->buf + EV_HDSIZ), (a->rwBuf + a->rwBufUsed), bytesToRead);
        nBytes = bytesToRead;
        a->rwBufUsed += bytesToRead;
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
/*printf("evGetNewBuffer: read in last block, #%d\n", a->blknum);*/
        a->isLastBlock = 1;
    }

    /* Start out pointing to the data right after the block header.
     * If we're in the middle of reading an event, this will allow
     * us to continue reading it. If we've looking to read a new
     * event, this should point to the next one. */
    a->next = a->buf + (a->buf)[EV_HD_HDSIZ];

    /* Find number of valid words left to read (w/ evRead) in block */
    if (a->version < 4) {
        a->left = (a->buf)[EV_HD_USED] - (a->buf)[EV_HD_HDSIZ];
    }
    else {
        a->left = a->blksiz - EV_HDSIZ;
    }

    /* If there are no valid data left in block ... */
    if (a->left <= 0) {
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
 * @return S_FAILURE          if opened for reading in {@link evOpen}
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
    EVFILE *a;
    int     nToWrite, ncopy, status;
    
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
        return(S_FAILURE);
    }

    /* Number of words left to write = full event size + bank header */
    nToWrite = buffer[0] + 1;

    /* If adding this event to existing data would put us over the target block size ... */
    while (nToWrite + a->blksiz > a->blkSizeTarget) {

        /* Have a single large event to write which is bigger than the target
         * block size & larger than the block buffer - need larger buffer */
        if (a->evCount < 1 && (nToWrite > a->bufSize - EV_HDSIZ)) {

            /* Keep old buffer around for a bit */
            uint32_t *pOldBuf = a->buf;
            
            /* Allocate more space */
            a->buf = (uint32_t *) malloc((nToWrite + EV_HDSIZ)*4);
            if (!(a->buf)) {
                /* Set things back to the way they were so if more memory
                 * is freed somewhere, can continue to call this function */
                a->buf = pOldBuf;
                return(S_EVFILE_ALLOCFAIL);
            }

            /* Copy old header into it */
            memcpy((void *)a->buf, (const void *)pOldBuf, EV_HDSIZ*4);

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
            status = evFlush(a);
            if (status != S_SUCCESS) {
                return(status);
            }
        }
    }

    /* Store number of events written, but do NOT include dictionary */
    if (!hasDictionary(a) || a->evCount > 0) {
        a->evnum++;
        a->evCount++;
    }

    /* Increased data size by this event size */
    a->blksiz += nToWrite;

    /* Copy data from buffer into file struct */
    memcpy((void *)a->next, (const void *)buffer, nToWrite*4);
        
    a->next += nToWrite;
    a->left -= nToWrite;
    a->rwBytesOut += 4*nToWrite;

    /* If no space for writing left in block or reached the maximum
    * number of events per block, flush block to file/buf/socket */
    if (a->left <= 0 || a->evCount >= a->eventsMax) {
        status = evFlush(a);
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
int evGetBufferLength(int handle, int *length)
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
 *
 * @param a pointer file structure
 *
 * @return S_SUCCESS      if successful
 * @return S_EVFILE_TRUNC if not enough room writing to a user-given buffer in {@link evOpen}
 * @return errno          if file/socket write error
 * @return stream error   if file stream error
 */
static int evFlush(EVFILE *a)
{
    int nBytes, bytesToWrite;

    /* Store, in header, the actual, final block size */
    a->buf[EV_HD_BLKSIZ] = a->blksiz;
    
    /* Store, in header, the number of events in block */
    a->buf[EV_HD_COUNT] = a->evCount;

    // TODO: DO we really need this?? Is it ever used?
    /* Reserved spot used to store total # of events written so far */
    a->buf[EV_HD_RESVD2] = a->evnum;

    /* Write data */
    bytesToWrite = 4*a->blksiz;
    if (a->rw == EV_WRITEFILE) {
        /* Clear EOF and error indicators for file stream */
        clearerr(a->file);
        /* Write block to file */
        nBytes = 4*fwrite((const void *)a->buf, 4, bytesToWrite/4, a->file);
        /* Return any error condition of file stream */
        if (ferror(a->file)) return(ferror(a->file));
    }
    else if (a->rw == EV_WRITESOCK) {
        nBytes = tcpWrite(a->sockFd, a->buf, bytesToWrite);
    }
    else if (a->rw == EV_WRITEBUF) {
        /* If there's not enough space in the user-given
         * buffer to contain this block, return an error. */
        if (a->rwBufSize < a->rwBufUsed + 4*a->blksiz) {
            return(S_EVFILE_TRUNC);
        }
        memcpy((a->rwBuf + a->rwBufUsed), a->buf, bytesToWrite);
        nBytes = bytesToWrite;
        a->rwBufUsed += bytesToWrite;
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
    /* Total written size = block header size so far */
    a->blksiz = EV_HDSIZ;
    a->rwBytesOut += 4*EV_HDSIZ;
    /* No events in block yet */
    a->evCount = 0;
 
    return(S_SUCCESS);
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
 * This routine changes the target block size for writes if request arg = b or B.
 * If setting block size fails, writes can still continue with original
 * block size. Minimum size = 1K + 32(header) bytes.<p>
 * It returns the version number if request arg = v or V.<p>
 * It changes the maximum number of events/block if request arg = n or N,
 * used only in version 4.<p>
 * It returns a pointer to the 8 block header ints if request arg = h or H.
 * This pointer must be freed by the caller to avoid a memory leak.
 * Used only in version 4.<p>
 *
 * @param handle  evio handle
 * @param request string value of "b" "B" for setting (target) block size;
 *                "v" or "V" for getting evio version #;
 *                "n" or "N" for setting max # of events/block;
 *                "h" or "H" for getting 8 ints of block header info;
 * @param argp    pointer to 32 bit int:
 *                  1) containing new block size if request = b or B, or
 *                  2) containing new max number of events/block if request = n or N, or
 *                  3) returning version # if request = v or V, or
 *                address of pointer to 32 bit int:
 *                  4) returning pointer to 8 ints of block header if request = h or H.
 *                     This pointer must be freed by caller since it points
 *                     to an allocated 8, 32-bit ints or 256 bytes.
 *
 * @return S_SUCCESS           if successful
 * @return S_EVFILE_BADARG     if request is NULL or argp is NULL
 * @return S_EVFILE_BADHANDLE  if bad handle arg or wrong magic # in handle
 * @return S_EVFILE_ALLOCFAIL  if cannot allocate memory
 * @return S_EVFILE_UNKOPTION  if unknown option specified in request arg
 * @return S_EVFILE_BADSIZEREQ when setting block size - if currently reading,
 *                              have already written events with different block size,
 *                              or is smaller than minimum allowed size (header size + 1K);
 *                              when setting max # events/blk - if val < 1
 */
int evIoctl(int handle, char *request, void *argp)
{
    EVFILE *a;
    uint32_t *newBuf, *pHeader;
    int eventsMax, blockSize;

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
            if (a->blknum != 1 || a->evCount != 0) {
                return(S_EVFILE_BADSIZEREQ);
            }

            /* Read in requested target block size */
            blockSize = *(int32_t *) argp;

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
            pHeader = (uint32_t *)malloc(8*sizeof(int));
            if (pHeader == NULL) {
                return(S_EVFILE_ALLOCFAIL);
            }
            memcpy((void *)pHeader, (const void *)a->buf, 8*sizeof(int));
            *((int32_t **) argp) = pHeader;
            
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
            
            eventsMax = *(int *) argp;
            if (eventsMax < 1) {
                return(S_EVFILE_BADSIZEREQ);
            }
            
            a->eventsMax = eventsMax;
            break;

        default:
            return(S_EVFILE_UNKOPTION);
    }

    return(S_SUCCESS);
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
int evGetDictionary(int handle, char **dictionary, int *len) {
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
 * @return S_FAILURE           if reading or have already written events
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

    // TODO: better check out format
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
        return(S_FAILURE);
    }
    
    /* Cannot have already written events */
    if (a->blknum != 1 || a->evCount != 0) {
        return(S_FAILURE);
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
        a->rw == EV_WRITEBUF  || a->rw == EV_WRITESOCK) {
        /* Mark the block as the last one to be written. */
        setLastBlockBit(a);
        a->isLastBlock = 1;

        /* Write last block. If no events left to write,
         * just write an empty block (header only). */
        status = evFlush(a);
    }

    /* Close file */
    if (a->rw == EV_WRITEFILE || a->rw == EV_READFILE) {
        status2 = fclose(a->file);
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

        default:
            sprintf(temp, "?evPerror...no such error: %d\n",error);
            break;
    }

    return(temp);
}




