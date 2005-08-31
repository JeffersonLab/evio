// evioUtil.hxx


using namespace std;
#include <evio_util.h>



//--------------------------------------------------------------
//--------------------------------------------------------------


class evioHandler {

public:
  virtual void nodeHandler(int length, int ftype, int tag, int type, int num, int depth) = 0;
  virtual void leafHandler(void *data, int length, int ftype, int tag, int type, int num, int depth) = 0;

};


//--------------------------------------------------------------
//--------------------------------------------------------------


class evioParser {

private:
  int depth;


public:
  void parse(unsigned long *buf, evioHandler &handler);
  void parseBank(unsigned long *buf, int ftype, int depth, evioHandler &handler);
  void loopOverBanks(unsigned long *data, int length, int type, int depth, evioHandler &handler);

};


//--------------------------------------------------------------
//--------------------------------------------------------------
