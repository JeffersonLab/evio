/*
 *  evio_util.h
 *
 *  E.Wolin, 19-feb-2010
 */



#ifndef EVIO_UTIL
#define EVIO_UTIL


#ifdef sun
#include <sys/param.h>
#else
#include <stddef.h>
#ifdef _MSC_VER
   typedef __int64 int64_t;	// Define it from MSVC's internal type
   #include "msinttypes.h"
#else
 #ifndef VXWORKS
   #include <stdint.h>		  // Use the C99 official header
 #endif
#endif
#endif



/* node and leaf handler typedefs */
typedef void (*NH_TYPE) (int32_t length, int32_t ftype, int32_t tag, int32_t type, 
                         int32_t num, int32_t depth, void *userArg);
typedef void (*LH_TYPE) (void *data, int32_t length, int32_t ftype, int32_t tag, 
                         int32_t type, int32_t num, int32_t depth, void *userArg);



/* prototypes */
#ifdef __cplusplus
extern "C" {
#endif
  
  void evio_stream_parse(int32_t *buf, NH_TYPE nh, LH_TYPE lh, void *userArg);
  const char *get_typename(int type);
  int is_container(int type);

  
#ifdef __cplusplus
}
#endif

#endif
