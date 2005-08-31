/*
 *  evio_util.h
 *
 *  evio utility header file
 *
 *   Author:  Elliott Wolin, JLab, 25-aug-2005
*/

#include <evio.h>


/* container types */
enum {
  BANK = 0,
  SEGMENT,
  TAGSEGMENT,
};


/* node and leaf handler typedefs */
typedef void (*NH_TYPE)(int length, int ftype, int tag, int type, int num, int depth);
typedef void (*LH_TYPE)(void *data, int length, int ftype, int tag, int type, int num, int depth);


/* prototypes */
void evio_parse(unsigned long *buf, NH_TYPE nh, LH_TYPE lh);
