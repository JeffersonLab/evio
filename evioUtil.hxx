// evioUtil.hxx

//  Author:  Elliott Wolin, JLab, 3-Oct-2005


// must do:
//   user's manual
//   Doxygen comments

// should do:
//   Interface for tree createion and modification, add and drop sub-trees, etc?  AIDA?
//   evioChannel and output, getBuffer() and const?

//  would like to do:
//   scheme for exception type codes
//   exception stack trace if supported on all platforms
//   equivalent of (unsupported) templated typedefs for evioDOMLeafNode to simplify user code




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


// typedefs simplify life a little for users
// ...unfortunately c++ does not accept templated typedefs (for leaf nodes)
typedef list<const evioDOMNode*>  evioDOMNodeList;
typedef auto_ptr<evioDOMNodeList> evioDOMNodeListP;



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


// basic evio exception class
class evioException {

public:
  evioException(int typ = 0, const string &txt = "", const string &aux = "");
  evioException(int typ, const string &txt, const string &file, int line);
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


//  wrapper around evio C library
class evioFileChannel : public evioChannel {

public:
  evioFileChannel(const string &fileName, const string &mode = "r", int size = 8192) throw(evioException*);
  virtual ~evioFileChannel();

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
  virtual void *containerNodeHandler(int length, int tag, int contentType, int num, 
                            int depth, void *userArg) = 0;
  virtual void leafNodeHandler(int length, int tag, int contentType, int num, 
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


//  manages tree representation of evio event in memory
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

  evioDOMNodeListP getNodeList(void) const throw(evioException*);
  evioDOMNodeListP getContainerNodeList(void) const throw(evioException*);
  evioDOMNodeListP getLeafNodeList(void) const throw(evioException*);
  template <typename T> auto_ptr< list<const evioDOMLeafNode<T>*> > getLeafNodeList(void) const throw(evioException*);

  string toString(void) const;


private:
  evioDOMNode *parse(const unsigned long *buf) throw(evioException*);
  int toEVIOBuffer(unsigned long *buf, const evioDOMNode *pNode) const throw(evioException*);
  evioDOMNodeList *addToNodeList(const evioDOMNode *pNode, evioDOMNodeList *pList) const 
    throw(evioException*);
  void toOstream(ostream &os, const evioDOMNode *node, int depth) const throw(evioException*);
  void *containerNodeHandler(int length, int tag, int contentType, int num, int depth, void *userArg);
  void leafNodeHandler(int length, int tag, int contentType, int num, int depth, const void *data, void *userArg);


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
  // virtual evioDOMNode& operator=(const evioDOMNode &rhs) throw(evioException*);  Does not seem to be needed...

  virtual evioDOMNode *clone(evioDOMNode *parent) const = 0;

  virtual const evioDOMNode *getParent(void) const;
  bool isContainer(void) const;
  bool isLeaf(void) const;

  virtual string toString(void) const = 0;
  virtual string getHeader(int depth) const = 0;
  virtual string getFooter(int depth) const = 0;


protected:
  evioDOMNode *parent;


public:
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


//  represents an evio leaf node in memory
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


template <typename T> auto_ptr< list<const evioDOMLeafNode<T>*> > evioDOMTree::getLeafNodeList(void) const
  throw(evioException*) {

  evioDOMNodeListP pNodeList        = getNodeList();
  auto_ptr< list<const evioDOMLeafNode<T>*> > pLeafList = auto_ptr< list<const evioDOMLeafNode<T>*> >(new list<const evioDOMLeafNode<T>*>);

  evioDOMNodeList::const_iterator iter;
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
