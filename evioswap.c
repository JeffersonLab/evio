/*
 *  evioswap.c
 *
 *   swaps in place one evio version 2 event
 *
 *   usage:  
 *        void evioswap(unsigned long* buffer, int tolocal)
 *
 *   Author: Elliott Wolin, JLab, 20-jun-2002
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
static void swap_fragment(unsigned long *buf, int fragment_type, int tolocal);
static void swap_data(unsigned long *data, int type, int length, int tolocal);
static void swap_long(unsigned long *data, int length);
static void swap_longlong(unsigned long long *data, int length);
static void swap_short (unsigned short *data, int length);


/*--------------------------------------------------------------------------*/


void evioswap(unsigned long *buf, int tolocal) {

  swap_fragment(buf,BANK,tolocal);

  return;
}


/*---------------------------------------------------------------- */


void swap_fragment(unsigned long *buf, int fragment_type, int tolocal) {

  int length,type;


  /* swap header words, then get length and contained type */
  switch(fragment_type) {
  case BANK:
    if(tolocal)swap_long(&buf[0],2);
    length  	= buf[0]+1;
    type    	= (buf[1]>>8)&0xff;
    if(!tolocal)swap_long(&buf[0],2);
    break;

  case SEGMENT:
    if(tolocal)swap_long(&buf[0],1);
    length  	= (buf[0]&0xffff)+1;
    type    	= (buf[0]>>16)&0xff;
    if(!tolocal)swap_long(&buf[0],1);
    break;
    
  case TAGSEGMENT:
    if(tolocal)swap_long(&buf[0],1);
    length  	= (buf[0]&0xffff)+1;
    type    	= (buf[0]>>16)&0xf;
    if(!tolocal)swap_long(&buf[0],1);
    break;

  default:
    printf("?illegal fragment_type in swap_fragment: %d",fragment_type);
    exit(EXIT_FAILURE);
    break;
  }


  swap_data(&buf[fragment_offset[fragment_type]],type,
	    length-fragment_offset[fragment_type],tolocal);
  
  return;
}


/*---------------------------------------------------------------- */


void swap_data(unsigned long *data, int type, int length, int tolocal) {

  int fraglen;
  int p=0;


  /* swap the data or call swap_fragment */
  switch (type) {


    /* long */
  case 0x0:
  case 0x1:
  case 0x2:
  case 0xb:
    swap_long(data,length);
    break;


    /* char or byte */
  case 0x3:
  case 0x6:
  case 0x7:
    break;


    /* short */
  case 0x4:
  case 0x5:
    swap_short((unsigned short*)data,length*2);
    break;


    /* longlong */
  case 0x8:
  case 0x9:
  case 0xa:
    swap_longlong((unsigned long long*)data,length/2);
    break;



    /* bank */
  case 0xe:
  case 0x10:
    while(p<length) {
      if(tolocal)swap_fragment(&data[p],BANK,tolocal);
      fraglen=data[p];
      if(!tolocal)swap_fragment(&data[p],BANK,tolocal);
      p+=fraglen+1;
    }
    break;


    /* segment */
  case 0xd:
  case 0x20:
    while(p<length) {
      if(tolocal)swap_fragment(&data[p],SEGMENT,tolocal);
      fraglen=data[p]&0xffff;
      if(!tolocal)swap_fragment(&data[p],SEGMENT,tolocal);
      p+=fraglen+1;
    }
    break;


    /* tagsegment */
  case 0xc:
  case 0x40:
    while(p<length) {
      if(tolocal)swap_fragment(&data[p],TAGSEGMENT,tolocal);
      fraglen=data[p]&0xffff;
      if(!tolocal)swap_fragment(&data[p],TAGSEGMENT,tolocal);
      p+=fraglen+1;
    }
    break;


    /* unknown, treat as unsigned long */
  default:
    swap_long(data,length);
    break;
  }

  return;
}


/*---------------------------------------------------------------- */


void swap_long(unsigned long *data, int length) {

  int i,j,temp;
  char *d,*t;

  t=(char*)&temp;
  for(i=0; i<length; i++) {
    temp=data[i];
    d=(char*)&(data[i]);
    for(j=0; j<sizeof(long); j++)d[j]=t[sizeof(long)-j-1];
  }

  return;
}


/*---------------------------------------------------------------- */


void swap_longlong(unsigned long long *data, int length) {

  int i,j;
  long long temp;
  char *d,*t;

  t=(char*)&temp;
  for(i=0; i<length; i++) {
    temp=data[i];
    d=(char*)&(data[i]);
    for(j=0; j<sizeof(long long); j++)d[j]=t[sizeof(long long)-j-1];
  }

  return;
}


/*---------------------------------------------------------------- */


void swap_short(unsigned short *data, int length) {

  int i,j;
  short temp;
  char *d,*t;

  t=(char*)&temp;
  for(i=0; i<length; i++) {
    temp=data[i];
    d=(char*)&(data[i]);
    for(j=0; j<sizeof(short); j++)d[j]=t[sizeof(short)-j-1];
  }

  return;
}


/*---------------------------------------------------------------- */


