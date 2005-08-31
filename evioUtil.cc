/*
 *  evioUtil.cc
 *
 *   Assorted evio utilities, C++ version
 *
 *   Author:  Elliott Wolin, JLab, 31-aug-2005
*/

/* still to do
 * -----------
 *    confusion over container and data types in nh,lh
*/


#include <iostream>
#include <evioUtil.hxx>


//--------------------------------------------------------------
//--------------------------------------------------------------


void evioParser::parse(unsigned long *buf, evioHandler &handler) {
  
  depth=0;
  parseBank(buf,BANK,depth,handler);
  return;
  
}


//--------------------------------------------------------------


void evioParser::parseBank(unsigned long *buf, int ftype, int depth, evioHandler &handler) {

  int length,tag,type,num,dataOffset;


  /* get type-dependent info */
  switch(ftype) {
  case 0:
    length  	= buf[0]+1;
    tag     	= buf[1]>>16;
    type   	= (buf[1]>>8)&0xff;
    num     	= buf[1]&0xff;
    dataOffset  = 2;
    break;

  case 1:
    length  	= (buf[0]&0xffff)+1;
    type    	= (buf[0]>>16)&0xff;
    tag     	= (buf[0]>>24)&0xff;
    num     	= 0;
    dataOffset  = 1;
    break;
    
  case 2:
    length  	= (buf[0]&0xffff)+1;
    type    	= (buf[0]>>16)&0xf;
    tag     	= (buf[0]>>20)&0xfff;
    num     	= 0;
    dataOffset  = 1;
    break;

  default:
    cerr << "?illegal fragment type in parseBank: " << ftype << endl;
    exit(EXIT_FAILURE);
    break;
  }


  /* 
   * if a leaf or data node, call leaf handler.
   * if an intermediate node, call node handler and then loop over contained banks.
   */
  switch (type) {

  case 0x0:
  case 0x1:
  case 0x2:
  case 0xb:
    handler.leafHandler(&buf[dataOffset],length-dataOffset,ftype,tag,type,num,depth);
    break;

  case 0x3:
  case 0x6:
  case 0x7:
    handler.leafHandler((char*)(&buf[dataOffset]),(length-dataOffset)*4,ftype,tag,type,num,depth);
    break;

  case 0x4:
  case 0x5:
    handler.leafHandler((short*)(&buf[dataOffset]),(length-dataOffset)*2,ftype,tag,type,num,depth);
    break;

  case 0x8:
  case 0x9:
  case 0xa:
    handler.leafHandler((long long*)(&buf[dataOffset]),(length-dataOffset)/2,ftype,tag,type,num,depth);
    break;

  case 0xe:
  case 0x10:
  case 0xd:
  case 0x20:
  case 0xc:
  case 0x40:
    handler.nodeHandler(length,ftype,tag,type,num,depth);
    depth++;
    loopOverBanks(&buf[dataOffset],length-dataOffset,type,depth,handler);
    depth--;
    break;
  }

  return;
}


//--------------------------------------------------------------


void evioParser::loopOverBanks(unsigned long *data, int length, int type, int depth, evioHandler &handler) {

  int p=0;

  switch (type) {

  case 0xe:
  case 0x10:
    while(p<length) {
      parseBank(&data[p],BANK,depth,handler);
      p+=data[p]+1;
    }
    break;

  case 0xd:
  case 0x20:
    while(p<length) {
      parseBank(&data[p],SEGMENT,depth,handler);
      p+=(data[p]&0xffff)+1;
    }
    break;

  case 0xc:
  case 0x40:
    while(p<length) {
      parseBank(&data[p],TAGSEGMENT,depth,handler);
      p+=(data[p]&0xffff)+1;
    }
    break;

  }


  return;
}


//--------------------------------------------------------------
//--------------------------------------------------------------
