// evioUtil.hxx

//  Author:  Elliott Wolin, JLab, 31-aug-2005


// still to do
//   toEVIOBuffer()

//   stl algrorithms?
//   get private, protected, public straight
//   get const straight
//   signed byte in toString()
//   toString() compatible with evio2xml

//   AIDA interface?
//   traverse functions
//   add,drop sub-trees
//   copy constructors?
//   query functions, node lists, etc.


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
#include <algorithm>


class evioDOMNode;


//--------------------------------------------------------------
//--------------------------------------------------------------


// simple exception contains int and text field
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


//  node and leaf handlers for stream parsing of evio event
class evioStreamParserHandler {

public:
  virtual void *nodeHandler(int length, int nodeType, int tag, int contentType, int num, 
                            int depth, void *userArg) = 0;
  virtual void leafHandler(int length, int nodeType, int tag, int contentType, int num, 
                           int depth, const void *data, void *userArg) = 0;
};


//--------------------------------------------------------------
//--------------------------------------------------------------


//  evio event stream parser dispatches to handlers when node or leaf reached
class evioStreamParser {

public:
  void *parse(const unsigned long *buf, evioStreamParserHandler &handler, void *userArg)
    throw(evioException*);

private:
  void *parseBank(const unsigned long *buf, int nodeType, int depth, 
                 evioStreamParserHandler &handler, void *userArg) throw(evioException*);

};


//--------------------------------------------------------------
//--------------------------------------------------------------


//  contains object-based in-memory tree representation of evio event
class evioDOMTree:public evioStreamParserHandler {

public:
  evioDOMTree(const unsigned long *buf, const string &name = "root") throw(evioException*);
  evioDOMTree(evioDOMNode *root, const string &name = "root") throw(evioException*);
  ~evioDOMTree(void);

  //  void toEVIOBuffer(unsigned long *buf) const throw(evioException*);
  string getName(void) const;
  void setName(const string &newName);
  string toString(void) const throw(evioException*);


private:
  evioDOMNode *parse(const unsigned long *buf) throw(evioException*);
  void toOstream(ostream &os, const evioDOMNode *node, int depth) const throw(evioException*);
  void *nodeHandler(int length, int nodeType, int tag, int contentType, int num, 
                    int depth, void *userArg);
  void leafHandler(int length, int nodeType, int tag, int contentType, int num, 
                   int depth, const void *data, void *userArg);

  evioDOMNode *root;
  string name;
};



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


// represents an evio node in memory, pure virtual
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


//  represents an evio container node in memory
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


//  represents the many types of evio leaf nodes in memory
template <class T> class evioDOMLeafNode:public evioDOMNode {

public:
  evioDOMLeafNode(int containerType, int tag, int contentType, int num, T *p, int ndata) throw(evioException*);
  virtual ~evioDOMLeafNode(void);

  string toString(void) const;
  void toString(ostream &os, int depth) const;

  vector<T> data;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#endif
