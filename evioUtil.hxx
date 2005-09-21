// evioUtil.hxx

//  Author:  Elliott Wolin, JLab, 31-aug-2005


// still to do
//   signed byte in toString()
//   copy constructors?
//   toString() compatible with evio2xml
//   more exceptions, get types correct
//   string vs string&

//   Interface for tree modification, add and drip trees, etc?  AIDA?

//   wrap entire evio c library

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


// simple exception contains int and text field
class evioException {

public:
  evioException(void);
  evioException(int t, const string &s);

  virtual string toString(void) const;


public:
  int type;
  string text;

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


//  pure virtual node and leaf handler class for stream parsing of evio event
class evioStreamParserHandler {

public:
  virtual void *nodeHandler(int length, int tag, int contentType, int num, 
                            int depth, void *userArg) = 0;
  virtual void leafHandler(int length, int tag, int contentType, int num, 
                           int depth, const void *data, void *userArg) = 0;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


//  evio event stream parser class dispatches to handler class when node or leaf reached
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


//  contains object-based in-memory tree representation of evio event
class evioDOMTree : public evioStreamParserHandler {

public:
  evioDOMTree(const unsigned long *buf, const string &name = "root") throw(evioException*);
  evioDOMTree(evioDOMNode *root, const string &name = "root") throw(evioException*);
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
  virtual ~evioDOMNode(void) {};
  virtual bool operator==(int tag) const;
  virtual bool operator!=(int tag) const;
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
  virtual ~evioDOMContainerNode(void);

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
  evioDOMLeafNode(evioDOMNode *parent, int tag, int contentType, int num, T *p, int ndata) 
    throw(evioException*);
  virtual ~evioDOMLeafNode(void);

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
