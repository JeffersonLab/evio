// evioUtil.hxx

//  Author:  Elliott Wolin, JLab, 31-aug-2005


#ifndef _evioUtil_hxx
#define _evioUtil_hxx

using namespace std;
#include <string>
#include <sstream>
#include <evio_util.h>



//--------------------------------------------------------------
//--------------------------------------------------------------


class evioException {

public:
  evioException(void);
  evioException(int t, string &s);

  virtual void setType(int type);
  virtual int getType(void) const;
  virtual void setText(string &text);
  virtual string getText(void) const;
  virtual string toString(void) const;


protected:
  int type;
  string text;

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


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
