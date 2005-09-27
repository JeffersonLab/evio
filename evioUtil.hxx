// evioUtil.hxx

//  Author:  Elliott Wolin, JLab, 27-Sep-2005


// still to do
//   copy constructors?  operator=?  What should they do?
//   auto_ptr and exceptions?  smart pointers?

//   signed byte in toString()
//   toString() compatible with evio2xml
//   more exceptions, get types correct, add debug info

//   Interface for tree modification, add and drip trees, etc?  AIDA?
//   evioChannel for output, getBuffer() and const?

//   user's manual


#ifndef _evioUtil_hxx
#define _evioUtil_hxx


using namespace std;
#include <evio_util.h>
#include <list>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>


// prototypes
class evioDOMNode;
class evioDOMContainerNode;
template<typename T> class evioDOMLeafNode;


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


// basic evio exception class
class evioException {

public:
  evioException(int t = 0, const string &s = "", const string &aux = "");
  virtual string toString(void) const;


public:
  int type;
  string text;
  string auxText;

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


// evio channel interface
class evioChannel{

public:
  virtual void open(void) throw(evioException*) = 0;
  virtual bool read(void) throw(evioException*) = 0;
  virtual void write(void) throw(evioException*) = 0;
  virtual void close(void) throw(evioException*) = 0;
  virtual const unsigned long *getBuffer(void) const throw(evioException*) = 0;
  virtual int getBufSize(void) const = 0;

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


//  wrapper around evio C library, acts as channel that performs basic event i/o functions
class evioFile : public evioChannel {

public:
  evioFile(const string &fileName, const string &mode, int size = 8192) throw(evioException*);
  virtual ~evioFile();

  void open(void) throw(evioException*);
  bool read(void) throw(evioException*);
  void write(void) throw(evioException*);
  void close(void) throw(evioException*);
  const unsigned long *getBuffer(void) const throw(evioException*);
  int getBufSize(void) const;

  void ioctl(const string &request, void *argp) throw(evioException*);

  string getFileName(void) const;
  string getMode(void) const;


private:
  string filename;
  string mode;
  int handle;
  unsigned long *buf;
  int bufSize;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


// interface defines node and leaf handlers for stream parsing
class evioStreamParserHandler {

public:
  virtual void *nodeHandler(int length, int tag, int contentType, int num, 
                            int depth, void *userArg) = 0;
  virtual void leafHandler(int length, int tag, int contentType, int num, 
                           int depth, const void *data, void *userArg) = 0;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


// stream parser dispatches to handlers when node or leaf reached
class evioStreamParser {

public:
  void *parse(const unsigned long *buf, evioStreamParserHandler &handler, void *userArg)
    throw(evioException*);
  
private:
  void *parseBank(const unsigned long *buf, int bankType, int depth, 
                  evioStreamParserHandler &handler, void *userArg) throw(evioException*);

};


//--------------------------------------------------------------
//--------------------------------------------------------------


//  contains tree representation of evio event in memory
class evioDOMTree : public evioStreamParserHandler {

public:
  evioDOMTree(const evioChannel &channel, const string &name = "root") throw(evioException*);
  evioDOMTree(const unsigned long *buf, const string &name = "root") throw(evioException*);
  evioDOMTree(const evioDOMNode *node, const string &name = "root") throw(evioException*);
  evioDOMTree(const evioDOMTree &tree) throw(evioException*);
  evioDOMTree& operator=(const evioDOMTree &rhs) throw(evioException*);
  virtual ~evioDOMTree(void);

  void toEVIOBuffer(unsigned long *buf) const throw(evioException*);
  const evioDOMNode *getRoot(void) const;

  list<const evioDOMNode*> *getNodeList(void) const throw(evioException*);
  list<const evioDOMNode*> *getContainerNodeList(void) const throw(evioException*);
  list<const evioDOMNode*> *getLeafNodeList(void) const throw(evioException*);
  template <typename T> list<const evioDOMLeafNode<T>*> *getLeafNodeList(void) const throw(evioException*);

  string toString(void) const;


private:
  evioDOMNode *parse(const unsigned long *buf) throw(evioException*);
  int toEVIOBuffer(unsigned long *buf, const evioDOMNode *pNode) const throw(evioException*);
  list<const evioDOMNode*> *addToNodeList(const evioDOMNode *pNode, list<const evioDOMNode*> *pList) const 
    throw(evioException*);
  void toOstream(ostream &os, const evioDOMNode *node, int depth) const throw(evioException*);
  void *nodeHandler(int length, int tag, int contentType, int num, int depth, void *userArg);
  void leafHandler(int length, int tag, int contentType, int num, int depth, const void *data, void *userArg);


private:
  evioDOMNode *root;

public:
  string name;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


//  represents an evio node in memory
class evioDOMNode {

public:
  evioDOMNode(evioDOMNode *parent, int tag, int contentType, int num) throw(evioException*);
  virtual ~evioDOMNode(void) {};

  virtual bool operator==(int tag) const;
  virtual bool operator!=(int tag) const;
  // virtual evioDOMNode& operator=(const evioDOMNode &rhs) throw(evioException*); ???

  virtual evioDOMNode *clone(evioDOMNode *parent) const = 0;

  bool isContainer(void) const;
  bool isLeaf(void) const;

  virtual string toString(void) const = 0;
  virtual string getHeader(int depth) const = 0;
  virtual string getFooter(int depth) const = 0;

public:
  evioDOMNode *parent;
  int tag;
  int contentType;
  int num;

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


//  represents an evio container node in memory
class evioDOMContainerNode : public evioDOMNode {

public:
  evioDOMContainerNode(evioDOMNode *parent, int tag, int contentType, int num) throw(evioException*);
  evioDOMContainerNode(const evioDOMContainerNode &cNode) throw (evioException*);
  evioDOMContainerNode& operator=(const evioDOMContainerNode &rhs) throw(evioException*);
  virtual ~evioDOMContainerNode(void);

  evioDOMContainerNode *clone(evioDOMNode *newParent) const;

  string toString(void) const;
  string getHeader(int depth) const;
  string getFooter(int depth) const;


public:
  list<evioDOMNode*> childList;

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


//  represents the many types of evio leaf nodes in memory
template <typename T> class evioDOMLeafNode : public evioDOMNode {

public:
  evioDOMLeafNode(evioDOMNode *parent, int tag, int contentType, int num, T *p, int ndata) throw(evioException*);
  evioDOMLeafNode(evioDOMNode *parent, int tag, int contentType, int num, const vector<T> &v) throw(evioException*);
  evioDOMLeafNode(const evioDOMLeafNode<T> &lNode) throw(evioException*);
  evioDOMLeafNode<T>& operator=(const evioDOMLeafNode<T> &rhs) throw(evioException*);
  virtual ~evioDOMLeafNode(void);

  evioDOMLeafNode<T>* clone(evioDOMNode *newParent) const;

  const vector<T> *getData(void) const;
  string toString(void) const;
  string getHeader(int depth) const;
  string getFooter(int depth) const;

public:
  vector<T> data;
};


//-----------------------------------------------------------------------------
//------------------ templates for non-overloaded methods ---------------------
//-----------------------------------------------------------------------------


template <typename T> const vector<T> *evioDOMLeafNode<T>::getData(void) const {
  return(&data);
}


//-----------------------------------------------------------------------------


template <typename T> list<const evioDOMLeafNode<T>*> *evioDOMTree::getLeafNodeList(void) const
  throw(evioException*) {

  list<const evioDOMNode*> *pNodeList        = getNodeList();
  list<const evioDOMLeafNode<T>*> *pLeafList = new list<const evioDOMLeafNode<T>*>;

  list<const evioDOMNode*>::const_iterator iter;
  const evioDOMLeafNode<T>* p;
  for(iter=pNodeList->begin(); iter!=pNodeList->end(); iter++) {
    p=dynamic_cast<const evioDOMLeafNode<T>*>(*iter);
    if(p!=NULL)pLeafList->push_back(p);
  }
  
  return(pLeafList);
}


//-----------------------------------------------------------------------------
//------------------------ Function Objects ---------------------------------
//-----------------------------------------------------------------------------


class typeEquals : unary_function<const evioDOMNode*,bool> {

public:
  typeEquals(int aType):type(aType) {}
  bool operator()(const evioDOMNode* node) const {return(node->contentType==type);}
private:
  int type;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class tagEquals : unary_function<const evioDOMNode*,bool> {

public:
  tagEquals(int aTag) : tag(aTag) {}
  bool operator()(const evioDOMNode* node) const {return(node->tag==tag);}
private:
  int tag;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class numEquals : unary_function<const evioDOMNode*,bool> {

public:
  numEquals(int aNum) : num(aNum) {}
  bool operator()(const evioDOMNode* node) const {return(node->num==num);}
private:
  int num;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class tagNumEquals : unary_function<const evioDOMNode*, bool> {

public:
  tagNumEquals(int aTag, int aNum) : tag(aTag), num(aNum) {}
  bool operator()(const evioDOMNode* node) const {return((node->tag==tag)&&(node->num==num));}
private:
  int tag;
  int num;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class isContainerType : unary_function<const evioDOMNode*,bool> {

public:
  isContainerType(void) {}
  bool operator()(const evioDOMNode* node) const {return(node->isContainer());}
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class isLeafType  : unary_function<const evioDOMNode*,bool> {

public:
  isLeafType(void) {}
  bool operator()(const evioDOMNode* node) const {return(node->isLeaf());}
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class toString : public unary_function<const evioDOMNode*, void> {

public:
  toString(void) {}
  void operator()(const evioDOMNode* node) const {cout << node->toString() << endl;}
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


#endif
