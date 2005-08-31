// evioUtil.hxx


#ifndef _evioUtil_hxx
#define _evioUtil_hxx

using namespace std;
#include <evio_util.h>



//--------------------------------------------------------------
//--------------------------------------------------------------


class evioStreamHandler {

public:
  virtual void nodeHandler(int length, int ftype, int tag, int type, int num, int depth) = 0;
  virtual void leafHandler(const void *data, int length, int ftype, int tag, int type, int num, 
                           int depth) = 0;

};


//--------------------------------------------------------------
//--------------------------------------------------------------


class evioStreamParser {

private:
  int depth;


public:
  void parse(const unsigned long *buf, evioStreamHandler &handler);

protected:
  void parseBank(const unsigned long *buf, int ftype, int depth, evioStreamHandler &handler);
  void loopOverBanks(const unsigned long *data, int length, int type, int depth, evioStreamHandler &handler);

};


//--------------------------------------------------------------
//--------------------------------------------------------------


#endif
