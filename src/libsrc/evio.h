/*  evio.h
 *
 * based on evfile_msg.h from SAW
 *
 *  E.Wolin, 19-jun-2001
 */



#ifndef EVIO
#define EVIO


#ifndef S_SUCCESS
#define S_SUCCESS 0
#define S_FAILURE -1
#endif

#define S_EVFILE    		0x00730000	/* evfile.msg Event File I/O */
#define S_EVFILE_TRUNC		0x40730001	/* Event truncated on read */
#define S_EVFILE_BADBLOCK	0x40730002	/* Bad block number encountered */
#define S_EVFILE_BADHANDLE	0x80730001	/* Bad handle (file/stream not open) */
#define S_EVFILE_ALLOCFAIL	0x80730002	/* Failed to allocate event I/O structure */
#define S_EVFILE_BADFILE	0x80730003	/* File format error */
#define S_EVFILE_UNKOPTION	0x80730004	/* Unknown option specified */
#define S_EVFILE_UNXPTDEOF	0x80730005	/* Unexpected end of file while reading event */
#define S_EVFILE_BADSIZEREQ	0x80730006	/* Invalid buffer size request to evIoct */


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


 /* node and leaf handler typedefs */
typedef void (*NH_TYPE)(int32_t length, int32_t ftype, int32_t tag, int32_t type, int32_t num, int32_t depth, void *userArg);
typedef void (*LH_TYPE)(void *data, int32_t length, int32_t ftype, int32_t tag, int32_t type, int32_t num, int32_t depth, void *userArg);


/* prototypes */
#ifdef __cplusplus
extern "C" {
#endif

void set_user_frag_select_func( int32_t (*f) (int32_t tag) );

int32_t evOpen(char *fileName, char *mode, int32_t *handle);
int32_t evRead(int32_t handle, uint32_t *buffer, int32_t size);
int32_t evWrite(int32_t handle, const uint32_t *buffer);
int32_t evIoctl(int32_t handle, char *request, void *argp);
int32_t evClose(int32_t handle);

void evioswap(uint32_t *buffer, int32_t tolocal, uint32_t *dest);

void evio_stream_parse(uint32_t *buf, NH_TYPE nh, LH_TYPE lh, void *userArg);
const char *get_typename(int32_t type);
int32_t is_container(int32_t type);

#ifdef __cplusplus
}
#endif

#endif
