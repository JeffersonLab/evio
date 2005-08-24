/*
 *  evio_util.c
 *
 *   Assorted evio utilities
 *
 *   Author:  Elliott Wolin, JLab, 23-aug-2005
*/

/* still to do
 * -----------
 *
*/


/* include files */
#include <stdio.h>
#include <stdlib.h>


/* fragment info */
static int fragment_offset[] = {2,1,1};
enum {
  BANK = 0,
  SEGMENT,
  TAGSEGMENT,
};


/* prototypes */
static void parse_bank(unsigned long *buf, int fragment_type, int depth,
                       void (*nh)(int length, int tag, int type, int num, int depth), 
                       void (*lh)(void *data, int length, int tag, int type, int num, int depth));
static void loop_over_banks(unsigned long *data, int length, int type, int depth,
                            void (*nh)(int length, int tag, int type, int num, int depth), 
                            void (*lh)(void *data, int length, int tag, int type, int num, int depth));



/*---------------------------------------------------------------- */


void evio_parse(unsigned long *buf, 
                void (*nh)(int length, int tag, int type, int num, int depth), 
                void (*lh)(void *data, int length, int tag, int type, int num, int depth)) {

  int depth=0;
  parse_bank(buf,BANK,depth,nh,lh);

  return;
}


/*---------------------------------------------------------------- */


static void parse_bank(unsigned long *buf, int fragment_type, int depth, 
                       void (*nh)(int length, int tag, int type, int num, int depth), 
                       void (*lh)(void *data, int length, int tag, int type, int num, int depth)) {

  int length,tag,type,num,dataOffset;


  /* get type-dependent info */
  switch(fragment_type) {
  case 0:
    length  	= buf[0]+1;
    tag     	= buf[1]>>16;
    type   	= (buf[1]>>8)&0xff;
    num     	= buf[1]&0xff;
    break;

  case 1:
    length  	= (buf[0]&0xffff)+1;
    type    	= (buf[0]>>16)&0xff;
    tag     	= (buf[0]>>24)&0xff;
    num     	= 0;
    break;
    
  case 2:
    length  	= (buf[0]&0xffff)+1;
    type    	= (buf[0]>>16)&0xf;
    tag     	= (buf[0]>>20)&0xfff;
    num     	= 0;
    break;

  default:
    printf("?illegal fragment_type in parse_bank: %d",fragment_type);
    exit(EXIT_FAILURE);
    break;
  }
  dataOffset=fragment_offset[fragment_type];


  /* 
   * if a leaf or data node, call leaf handler.
   * if an intermediate node, call node handler and then loop over contained banks.
  */
  switch (type) {

  case 0x0:
  case 0x1:
  case 0x2:
  case 0xb:
    if(lh!=NULL)lh(&buf[dataOffset],length-dataOffset,tag,type,num,depth);
    break;

  case 0x3:
  case 0x6:
  case 0x7:
    if(lh!=NULL)lh((char*)(&buf[dataOffset]),(length-dataOffset)*4,tag,type,num,depth);
    break;

  case 0x4:
  case 0x5:
    if(lh!=NULL)lh((short*)(&buf[dataOffset]),(length-dataOffset)*2,tag,type,num,depth);
    break;

  case 0x8:
  case 0x9:
  case 0xa:
    if(lh!=NULL)lh((long long*)(&buf[dataOffset]),(length-dataOffset)/2,tag,type,num,depth);
    break;

  case 0xe:
  case 0x10:
  case 0xd:
  case 0x20:
  case 0xc:
  case 0x40:
    if(nh!=NULL)nh(length,tag,type,num,depth);
    depth++;
    loop_over_banks(&buf[dataOffset],length-dataOffset,type,depth,nh,lh);
    depth--;
    break;
  }

  return;
}


/*---------------------------------------------------------------- */


static void loop_over_banks(unsigned long *data, int length, int type, int depth, 
                            void (*nh)(int length, int tag, int type, int num, int depth), 
                            void (*lh)(void *data, int length, int tag, int type, int num, int depth)) {

  int p=0;

  switch (type) {

  case 0xe:
  case 0x10:
    while(p<length) {
      parse_bank(&data[p],BANK,depth,nh,lh);
      p+=data[p]+1;
    }
    break;

  case 0xd:
  case 0x20:
    while(p<length) {
      parse_bank(&data[p],SEGMENT,depth,nh,lh);
      p+=(data[p]&0xffff)+1;
    }
    break;

  case 0xc:
  case 0x40:
    while(p<length) {
      parse_bank(&data[p],TAGSEGMENT,depth,nh,lh);
      p+=(data[p]&0xffff)+1;
    }
    break;

  }


  return;
}


/*---------------------------------------------------------------- */
