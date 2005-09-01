/*
 *  evioUtil.cc
 *
 *   Assorted evio utilities, C++ version
 *
 *   Author:  Elliott Wolin, JLab, 31-aug-2005
*/


#include <evioUtil.hxx>


//--------------------------------------------------------------
//--------------------------------------------------------------


void *evioStreamParser::parse(const unsigned long *buf, 
                             evioStreamHandler &handler, void *userArg) throw(evioException*) {
  
  void *newUserArg = parseBank(buf,BANK,0,handler,userArg);
  return(newUserArg);
  
}


//--------------------------------------------------------------


void *evioStreamParser::parseBank(const unsigned long *buf, int ftype, int depth, 
                                 evioStreamHandler &handler, void *userArg) throw(evioException*) {

  int length,tag,type,num,dataOffset,p,bankLen;
  void *newUserArg = userArg;
  const unsigned long *data;



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
    ostringstream ss;
    ss << ftype;
    throw(new evioException(1,"?parseBank...illegal fragment type: " + ss.str()));
  }


  /* 
   * if a leaf node, call leaf handler.
   * if container node, call node handler and then parse contained banks.
   */
  switch (type) {

  case 0x0:
  case 0x1:
  case 0x2:
  case 0xb:
    handler.leafHandler(&buf[dataOffset],length-dataOffset,ftype,tag,type,num,depth,userArg);
    break;

  case 0x3:
  case 0x6:
  case 0x7:
    handler.leafHandler((char*)(&buf[dataOffset]),(length-dataOffset)*4,ftype,tag,type,num,depth,userArg);
    break;

  case 0x4:
  case 0x5:
    handler.leafHandler((short*)(&buf[dataOffset]),(length-dataOffset)*2,ftype,tag,type,num,depth,userArg);
    break;

  case 0x8:
  case 0x9:
  case 0xa:
    handler.leafHandler((long long*)(&buf[dataOffset]),(length-dataOffset)/2,ftype,tag,type,num,depth,userArg);
    break;

  case 0xe:
  case 0x10:
  case 0xd:
  case 0x20:
  case 0xc:
  case 0x40:
    // call node handler and get new userArg
    newUserArg=handler.nodeHandler(length,ftype,tag,type,num,depth,userArg);


    // parse contained banks
    depth++;
    p       = 0;
    bankLen = length-dataOffset;
    data    = &buf[dataOffset];

    switch (type) {

    case 0xe:
    case 0x10:
      while(p<bankLen) {
        parseBank(&data[p],BANK,depth,handler,newUserArg);
        p+=data[p]+1;
      }
      break;

    case 0xd:
    case 0x20:
      while(p<bankLen) {
        parseBank(&data[p],SEGMENT,depth,handler,newUserArg);
        p+=(data[p]&0xffff)+1;
      }
      break;

    case 0xc:
    case 0x40:
      while(p<bankLen) {
        parseBank(&data[p],TAGSEGMENT,depth,handler,newUserArg);
        p+=(data[p]&0xffff)+1;
      }
      break;

    }

    depth--;
    break;
  }

  return(newUserArg);
}


//--------------------------------------------------------------
//--------------------------------------------------------------


evioException::evioException() {
  type=0;
  text="";
}


//--------------------------------------------------------------


evioException::evioException(int t, string s) {
  type=t;
  text=s;
}


//--------------------------------------------------------------


void evioException::setType(int t) {
  type=t;
}


//--------------------------------------------------------------


int evioException::getType(void) const {
  return(type);
}


//--------------------------------------------------------------


void evioException::setText(string t) {
  text=t;
}


//--------------------------------------------------------------


string evioException::getText(void) const {
  return(text);
}


//--------------------------------------------------------------


string evioException::toString(void) const {
  ostringstream s;
  s << type;
  return(string("Type: ") + s.str() + string(", text: ") + text);
}


//--------------------------------------------------------------
//--------------------------------------------------------------
