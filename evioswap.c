/*
 *  evioswap.c
 *
 *   swaps one evio version 2 event
 *       - in place if dest is NULL
 *       - copy to dest if not NULL
 *   thread safe
 *
 *   usage:  
 *        void evioswap(unsigned long *buffer, int tolocal, unsigned long *dest)
 *
 *   Author: Elliott Wolin, JLab, 20-jun-2002
 *
 *  To do:
 *     finish copy version
*/


/* for posix */
#define _POSIX_SOURCE_ 1
#define __EXTENSIONS__


/* include files */
#include <stdlib.h>
#include <stdio.h>


/* fragment info */
static int fragment_offset[] = {2,1,1};
enum {
  BANK        = 0,
  SEGMENT     = 1,
  TAGSEGMENT  = 2,
};


/* prototypes */
static void swap_fragment(unsigned long *buf, int fragment_type, 
			  int tolocal, unsigned long *dest);
static void swap_data(unsigned long *data, int type, int length, 
		      int tolocal, unsigned long *dest);
static unsigned long *swap_long(unsigned long *data, int length, unsigned long *dest);
static void swap_longlong(unsigned long long *data, int length, 
			  unsigned long long *dest);
static void swap_short(unsigned short *data, int length, unsigned short *dest);


/*--------------------------------------------------------------------------*/


static void evioswap(unsigned long *buf, int tolocal, unsigned long *dest) {

  swap_fragment(buf,BANK,tolocal,dest);

  return;
}


/*---------------------------------------------------------------- */


static void swap_fragment(unsigned long *buf, int fragment_type, 
		   int tolocal, unsigned long *dest) {

  int length,type;
  unsigned long *p=buf;


  /* swap header word(s), then get length and contained type */
  switch(fragment_type) {
  case BANK:
    if(tolocal)p = swap_long(buf,2,dest);
    length  	 = p[0]+1;
    type    	 = (p[1]>>8)&0xff;
    if(!tolocal)swap_long(buf,2,dest);
    break;

  case SEGMENT:
    if(tolocal)p = swap_long(buf,1,dest);
    length  	 = (p[0]&0xffff)+1;
    type    	 = (p[0]>>16)&0xff;
    if(!tolocal)swap_long(buf,1,dest);
    break;
    
  case TAGSEGMENT:
    if(tolocal)p = swap_long(buf,1,dest);
    length  	 = (p[0]&0xffff)+1;
    type    	 = (p[0]>>16)&0xf;
    if(!tolocal)swap_long(buf,1,dest);
    break;

  default:
    printf("?illegal fragment_type in swap_fragment: %d",fragment_type);
    exit(EXIT_FAILURE);
    break;
  }


  swap_data(&buf[fragment_offset[fragment_type]], type,
	    length-fragment_offset[fragment_type], tolocal,
	    (dest==NULL)?NULL:&dest[fragment_offset[fragment_type]]);
  
  return;
}


/*---------------------------------------------------------------- */


static void swap_data(unsigned long *data, int type, int length, 
	       int tolocal, unsigned long *dest) {

  int fraglen;
  int l=0;
  unsigned long *p=data;


  /* swap the data or call swap_fragment */
  switch (type) {


    /* long */
  case 0x0:
  case 0x1:
  case 0x2:
  case 0xb:
    swap_long(data,length,dest);
    break;


    /* char or byte */
  case 0x3:
  case 0x6:
  case 0x7:
    break;


    /* short */
  case 0x4:
  case 0x5:
    swap_short((unsigned short*)data,length*2,(unsigned short*)dest);
    break;


    /* longlong */
  case 0x8:
  case 0x9:
  case 0xa:
    swap_longlong((unsigned long long*)data,length/2,(unsigned long long*)dest);
    break;



    /* bank */
  case 0xe:
  case 0x10:
    while(l<length) {
      if(tolocal)swap_fragment(&data[l],BANK,tolocal,(dest==NULL)?NULL:&dest[l]);
      fraglen=data[l];
      if(!tolocal)swap_fragment(&data[l],BANK,tolocal,(dest==NULL)?NULL:&dest[l]);
      l+=fraglen+1;
    }
    break;


    /* segment */
  case 0xd:
  case 0x20:
    while(l<length) {
      if(tolocal)swap_fragment(&data[l],SEGMENT,tolocal,(dest==NULL)?NULL:&dest[l]);
      fraglen=data[l]&0xffff;
      if(!tolocal)swap_fragment(&data[l],SEGMENT,tolocal,(dest==NULL)?NULL:&dest[l]);
      l+=fraglen+1;
    }
    break;


    /* tagsegment */
  case 0xc:
  case 0x40:
    while(l<length) {
      if(tolocal)swap_fragment(&data[l],TAGSEGMENT,tolocal,(dest==NULL)?NULL:&dest[l]);
      fraglen=data[l]&0xffff;
      if(!tolocal)swap_fragment(&data[l],TAGSEGMENT,tolocal,(dest==NULL)?NULL:&dest[l]);
      l+=fraglen+1;
    }
    break;


    /* unknown, treat as unsigned long */
  default:
    swap_long(data,length,dest);
    break;
  }

  return;
}


/*---------------------------------------------------------------- */


static unsigned long *swap_long(unsigned long *data, int length, unsigned long *dest) {

  int i,j,temp;
  char *d,*t, *des, *dat;

  if(dest==NULL) {
    t=(char*)&temp;
    for(i=0; i<length; i++) {
      temp=data[i];
      d=(char*)&(data[i]);
      for(j=0; j<sizeof(long); j++)d[j]=t[sizeof(long)-j-1];
    }
    return(data);

  } else {
    t=(char*)&temp;
    for(i=0; i<length; i++) {
      temp=data[i];
      d=(char*)&(dest[i]);
      for(j=0; j<sizeof(long); j++)d[j]=t[sizeof(long)-j-1];
    }
    return(dest);
  }
}


/*---------------------------------------------------------------- */


static void swap_longlong(unsigned long long *data, int length, unsigned long long *dest) {

  int i,j;
  long long temp;
  char *d,*t;

  if(dest==NULL) {
    t=(char*)&temp;
    for(i=0; i<length; i++) {
      temp=data[i];
      d=(char*)&(data[i]);
      for(j=0; j<sizeof(long long); j++)d[j]=t[sizeof(long long)-j-1];
    }

  } else {
  }
  
  return;
}


/*---------------------------------------------------------------- */


static void swap_short(unsigned short *data, int length, unsigned short *dest) {

  int i,j;
  short temp;
  char *d,*t;

  if(dest==NULL) {
    t=(char*)&temp;
    for(i=0; i<length; i++) {
      temp=data[i];
      d=(char*)&(data[i]);
      for(j=0; j<sizeof(short); j++)d[j]=t[sizeof(short)-j-1];
    }

  } else {
  }
  
  return;
}


/*---------------------------------------------------------------- */
