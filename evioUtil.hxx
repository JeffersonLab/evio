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
  evioException(int t, string s);

  virtual void setType(int type);
  virtual int getType(void) const;
  virtual void setText(string text);
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
  virtual void *nodeHandler(int length, int nodeType, int tag, int contentType, int num, 
                            int depth, void *userArg) = 0;
  virtual void leafHandler(int length, int nodeType, int tag, int contentType, int num, 
                           int depth, const void *data, void *userArg) = 0;
  
};


//--------------------------------------------------------------
//--------------------------------------------------------------


class evioStreamParser {

public:
  void *parse(const unsigned long *buf, evioStreamHandler &handler, void *userArg) throw(evioException*);

private:
  void *parseBank(const unsigned long *buf, int nodeType, int depth, 
                 evioStreamHandler &handler, void *userArg) throw(evioException*);
  
};


//--------------------------------------------------------------
//--------------------------------------------------------------


#endif
