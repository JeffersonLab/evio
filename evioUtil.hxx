// evioUtil.hxx

//  Author:  Elliott Wolin, JLab, 31-aug-2005


#ifndef _evioUtil_hxx
#define _evioUtil_hxx

using namespace std;
#include <evio_util.h>
#include <string>
#include <list>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>


class evioDOMTree;
class evioDOMNode;


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


class evioDOMHandler {

public:
  virtual void *nodeHandler(int length, int ftype, int tag, int type, int num, int depth, void *userArg) = 0;
  virtual void leafHandler(int length, int ftype, int tag, int type, int num, int depth,
                           const void *data, void *userArg) = 0;

};


//--------------------------------------------------------------
//--------------------------------------------------------------


class evioDOMParser {

public:
  evioDOMNode *parse(const unsigned long *buf) throw(evioException*);
  
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class evioDOMTree {

public:
  evioDOMTree(const unsigned long *buf) throw(evioException*);
  evioDOMTree(evioDOMNode *root) throw(evioException*);
  virtual ~evioDOMTree(void);

  string toString(void) const throw(evioException*);
  //  void toEVIOBuffer(unsigned long *buf) const throw(evioException*);


private:
  evioDOMNode *root;
  void toOstream(ostream &os, const evioDOMNode *node, int depth) const throw(evioException*);
};



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class evioDOMNode {

public:
  virtual ~evioDOMNode(void) {};

  virtual string toString(void) const = 0;
  virtual void toString(ostream &os, int depth) const = 0;

public:
  int containerType;
  int tag;
  int contentType;
  int num;

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class evioDOMContainerNode:public evioDOMNode {

public:
  evioDOMContainerNode(int containerType, int tag, int contentType, int num) throw(evioException*);
  virtual ~evioDOMContainerNode(void);

  string toString(void) const;
  void toString(ostream &os, int depth) const;

  list<evioDOMNode*> childList;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


template <class T> class evioDOMLeafNode:public evioDOMNode {

public:
  evioDOMLeafNode(int containerType, int tag, int contentType, int num, T *p, int ndata) throw(evioException*);
  string toString(void) const;
  void toString(ostream &os, int depth) const;

  vector<T> data;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


#endif
