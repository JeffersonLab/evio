/*  evio.h
 *
 * based on evfile_msg.h from SAW
 *
 *  E.Wolin, 19-jun-2001
 */



#ifndef __EVIO_h__
#define __EVIO_h__

#define EV_VERSION 4

#ifndef S_SUCCESS
#define S_SUCCESS 0
#define S_FAILURE -1
#endif

#define S_EVFILE    		0x00730000	/* evfile.msg Event File I/O */
#define S_EVFILE_TRUNC		0x40730001	/* Event truncated on read/write */
#define S_EVFILE_BADBLOCK	0x40730002	/* Bad block number encountered */
#define S_EVFILE_BADHANDLE	0x80730001	/* Bad handle (file/stream not open) */
#define S_EVFILE_ALLOCFAIL	0x80730002	/* Failed to allocate event I/O structure */
#define S_EVFILE_BADFILE	0x80730003	/* File format error */
#define S_EVFILE_UNKOPTION	0x80730004	/* Unknown option specified */
#define S_EVFILE_UNXPTDEOF	0x80730005	/* Unexpected end of file while reading event */
#define S_EVFILE_BADSIZEREQ 0x80730006  /* Invalid buffer size request to evIoct */
#define S_EVFILE_BADARG     0x80730007  /* Invalid function argument */

/* macros for swapping ints of various sizes */
#define EVIO_SWAP64(x) ( (((x) >> 56) & 0x00000000000000FFL) | \
                         (((x) >> 40) & 0x000000000000FF00L) | \
                         (((x) >> 24) & 0x0000000000FF0000L) | \
                         (((x) >> 8)  & 0x00000000FF000000L) | \
                         (((x) << 8)  & 0x000000FF00000000L) | \
                         (((x) << 24) & 0x0000FF0000000000L) | \
                         (((x) << 40) & 0x00FF000000000000L) | \
                         (((x) << 56) & 0xFF00000000000000L) )

#define EVIO_SWAP32(x) ( (((x) >> 24) & 0x000000FF) | \
                         (((x) >> 8)  & 0x0000FF00) | \
                         (((x) << 8)  & 0x00FF0000) | \
                         (((x) << 24) & 0xFF000000) )

#define EVIO_SWAP16(x) ( (((x) >> 8) & 0x00FF) | \
                         (((x) << 8) & 0xFF00) )

#ifdef sun
#include <sys/param.h>
#else
#include <stddef.h>
#ifdef _MSC_VER
   typedef __int64 int64_t;	// Define it from MSVC's internal type
   #include "msinttypes.h"
#else
   #include <stdint.h>		  // Use the C99 official header
#endif
#endif


#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strncasecmp strnicmp
#endif

/* prototypes */
#ifdef __cplusplus
extern "C" {
#endif

void set_user_frag_select_func( int32_t (*f) (int32_t tag) );
void evioswap(uint32_t *buffer, int tolocal, uint32_t *dest);

int evOpen(char *filename, char *flags, int *handle);
int evOpenBuffer(char *buffer, int bufLen, char *flags, int *handle);
int evOpenSocket(int sockFd, char *flags, int *handle);

int evRead(int handle, uint32_t *buffer, int size);
int evReadAlloc(int handle, uint32_t **buffer, int *buflen);
int evReadNoCopy(int handle, const uint32_t **buffer, int *buflen);

int evWrite(int handle, const uint32_t *buffer);
int evIoctl(int handle, char *request, void *argp);
int evClose(int handle);
int evGetBufferLength(int handle, int *length);

int evGetDictionary(int handle, char **dictionary, int *len);
int evWriteDictionary(int handle, char *xmlDictionary);

int evIsContainer(int type);
const char *evGetTypename(int type);
char *evPerror(int error);


#ifdef __cplusplus
}

#endif

#endif
