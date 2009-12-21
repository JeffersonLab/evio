/*
 *  evioswap.c
 *
 *   evioswap() swaps one evio version 2 event
 *       - in place if dest is NULL
 *       - copy to dest if not NULL
 *
 *   swap_int2_t_val() swaps one int32_t, call by val
 *   swap_int32_t() swaps an array of uint32_t's
 *
 *   thread safe
 *
 *
 *   Author: Elliott Wolin, JLab, 21-nov-2003
 *
 *
 *  Notes:
 *     simple loop in swap_xxx takes 50% longer than pointers and unrolled loops
 *     -O is over twice as fast as -g
 *
 *  To do:
 *     use in evio.c, replace swap_util.c
 *
*/


/* include files */
#include <evio.h>
#include <stdlib.h>


/* entry points */
void evioswap(uint32_t *buffer, int tolocal, uint32_t*dest);
int32_t swap_int32_t_value(int32_t val);
uint32_t *swap_int32_t(uint32_t *data, unsigned length, uint32_t *dest);


/* internal prototypes */
static void swap_bank(uint32_t *buf, int tolocal, uint32_t *dest);
static void swap_segment(uint32_t *buf, int tolocal, uint32_t *dest);
static void swap_tagsegment(uint32_t *buf, int tolocal, uint32_t *dest);
static void swap_data(uint32_t *data, uint32_t type, uint32_t length,
		      int tolocal,  uint32_t *dest);
static void swap_int64_t(uint64_t *data, uint32_t length, uint64_t *dest);
static void swap_short(unsigned short *data, uint32_t length, unsigned short *dest);
static void copy_data(uint32_t *data, uint32_t length, uint32_t *dest);


/*--------------------------------------------------------------------------*/


void evioswap(uint32_t *buf, int tolocal, uint32_t *dest) {

  swap_bank(buf,tolocal,dest);

  return;
}


/*---------------------------------------------------------------- */


static void swap_bank(uint32_t *buf, int tolocal, uint32_t *dest) {

  uint32_t data_length,data_type;
  uint32_t *p=buf;


  /* swap header, get length and contained type */
  if(tolocal)p = swap_int32_t(buf,2,dest);
  data_length  = p[0]-1;
  data_type    = (p[1]>>8)&0xff;
  if(!tolocal)swap_int32_t(buf,2,dest);
  
  swap_data(&buf[2], data_type, data_length, tolocal, (dest==NULL)?NULL:&dest[2]);

  return;
}


/*---------------------------------------------------------------- */


static void swap_segment(uint32_t *buf, int tolocal, uint32_t *dest) {

  uint32_t data_length,data_type;
  uint32_t *p=buf;


  /* swap header, get length and contained type */
  if(tolocal)p = swap_int32_t(buf,1,dest);
  data_length  = (p[0]&0xffff);
  data_type    = (p[0]>>16)&0xff;
  if(!tolocal)swap_int32_t(buf,1,dest);

  swap_data(&buf[1], data_type, data_length, tolocal, (dest==NULL)?NULL:&dest[1]);
  
  return;
}


/*---------------------------------------------------------------- */


static void swap_tagsegment(uint32_t *buf, int tolocal, uint32_t *dest) {

  uint32_t data_length,data_type;
  uint32_t *p=buf;


  /* swap header, get length and contained type */
  if(tolocal)p = swap_int32_t(buf,1,dest);
  data_length  = (p[0]&0xffff);
  data_type    = (p[0]>>16)&0xf;
  if(!tolocal)swap_int32_t(buf,1,dest);

  swap_data(&buf[1], data_type, data_length, tolocal, (dest==NULL)?NULL:&dest[1]);
  
  return;
}


/*---------------------------------------------------------------- */


static void swap_data(uint32_t *data, uint32_t type, uint32_t length, 
	       int tolocal, uint32_t *dest) {

  uint32_t fraglen;
  uint32_t l=0;


  /* swap the data or call swap_fragment */
  switch (type) {


    /* undefined...no swap */
  case 0x0:
    copy_data(data,length,dest);
    break;


    /* int32_t */
  case 0x1:
  case 0x2:
  case 0xb:
    swap_int32_t(data,length,dest);
    break;


    /* char or byte */
  case 0x3:
  case 0x6:
  case 0x7:
    copy_data(data,length,dest);
    break;


    /* short */
  case 0x4:
  case 0x5:
    swap_short((unsigned short*)data,length*2,(unsigned short*)dest);
    break;


    /* int64_t */
  case 0x8:
  case 0x9:
  case 0xa:
    swap_int64_t((uint64_t*)data,length/2,(uint64_t*)dest);
    break;



    /* bank */
  case 0xe:
  case 0x10:
    while(l<length) {
      if(tolocal) {
	swap_bank(&data[l],tolocal,(dest==NULL)?NULL:&dest[l]);
	fraglen=(dest==NULL)?data[l]+1:dest[l]+1;
      } else {
	fraglen=data[l]+1;
	swap_bank(&data[l],tolocal,(dest==NULL)?NULL:&dest[l]);
      }
      l+=fraglen;
    }
    break;


    /* segment */
  case 0xd:
  case 0x20:
    while(l<length) {
      if(tolocal) {
	swap_segment(&data[l],tolocal,(dest==NULL)?NULL:&dest[l]);
	fraglen=(dest==NULL)?(data[l]&0xffff)+1:(dest[l]&0xffff)+1;
      } else {
	fraglen=(data[l]&0xffff)+1;
	swap_segment(&data[l],tolocal,(dest==NULL)?NULL:&dest[l]);
      }
      l+=fraglen;
    }
    break;


    /* tagsegment */
  case 0xc:
  case 0x40:
    while(l<length) {
      if(tolocal) {
	swap_tagsegment(&data[l],tolocal,(dest==NULL)?NULL:&dest[l]);
	fraglen=(dest==NULL)?(data[l]&0xffff)+1:(dest[l]&0xffff)+1;
      } else {
	fraglen=(data[l]&0xffff)+1;
	swap_tagsegment(&data[l],tolocal,(dest==NULL)?NULL:&dest[l]);
      }
      l+=fraglen;
    }
    break;


    /* unknown type, just copy */
  default:
    copy_data(data,length,dest);
    break;
  }

  return;
}


/*---------------------------------------------------------------- */


int32_t swap_int32_t_value(int32_t val) {

  int temp;
  char *t = (char*)&temp+4;
  char *v = (char*)&val;
  
  *--t=*(v++);
  *--t=*(v++);
  *--t=*(v++);
  *--t=*(v++);

  return(temp);
}


/*---------------------------------------------------------------- */


uint32_t *swap_int32_t(uint32_t *data, uint32_t length, uint32_t *dest) {

  uint32_t i;
  uint32_t temp;
  char *d,*t;

  if(dest==NULL) {

    d=(char*)data+4;
    for(i=0; i<length; i++) {
      temp=data[i];
      t=(char*)&temp;
      *--d=*(t++);
      *--d=*(t++);
      *--d=*(t++);
      *--d=*(t++);
      d+=8;
    }
    return(data);

  } else {

    d=(char*)dest+4;
    t=(char*)data;
    for(i=0; i<length; i++) {
      *--d=*(t++);
      *--d=*(t++);
      *--d=*(t++);
      *--d=*(t++);
      d+=8;
    }
    return(dest);
  }
}


/*---------------------------------------------------------------- */


static void swap_int64_t(uint64_t *data, uint32_t length, uint64_t *dest) {

  uint32_t i;
  uint64_t temp;
  char *d,*t;

  if(dest==NULL) {

    d=(char*)data+8;
    for(i=0; i<length; i++) {
      temp=data[i];
      t=(char*)&temp;
      *--d=*(t++);
      *--d=*(t++);
      *--d=*(t++);
      *--d=*(t++);
      *--d=*(t++);
      *--d=*(t++);
      *--d=*(t++);
      *--d=*(t++);
      d+=16;
    }

  } else {
    
    d=(char*)dest+8;
    t=(char*)data;
    for(i=0; i<length; i++) {
      *--d=*(t++);
      *--d=*(t++);
      *--d=*(t++);
      *--d=*(t++);
      *--d=*(t++);
      *--d=*(t++);
      *--d=*(t++);
      *--d=*(t++);
      d+=16;
    }
    
    return;
  }
}


/*---------------------------------------------------------------- */


static void swap_short(unsigned short *data, uint32_t length, unsigned short *dest) {

  uint32_t i;
  unsigned short temp;
  char *d,*t;

  if(dest==NULL) {

    d=(char*)data+2;
    for(i=0; i<length; i++) {
      temp=data[i];
      t=(char*)&temp;
      *--d=*(t++);
      *--d=*(t++);
      d+=4;
    }

  } else {

    d=(char*)dest+2;
    t=(char*)data;
    for(i=0; i<length; i++) {
      *--d=*(t++);
      *--d=*(t++);
      d+=4;
    }
    
    return;
  }
}
 
 
/*---------------------------------------------------------------- */
 
 
static void copy_data(uint32_t *data, uint32_t length, uint32_t *dest) {
   
  int i;
  
  if(dest==NULL)return;
  for(i=0; i<length; i++)dest[i]=data[i];
  return;
}
 
 
/*---------------------------------------------------------------- */
