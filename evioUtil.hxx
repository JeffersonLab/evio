// evioUtil.hxx

//  Author:  Elliott Wolin, JLab, 17-Oct-2005


// must do:
//   operator<< and operator>> for container node class, virtual class streamable 
//   use vector in container node?  use node instead of node* in containers?
//   API for manual tree creation and modification, add/drop/move sub-trees, etc?
//   Doxygen comments

// should do:
//   namespaces
//   evioChannel and output, getBuffer() and const?

//  would like to do:
//   cMsg channel
//   replace isNull(auto_ptr<T>)
//   scheme for exception type codes
//   exception stack trace if supported on all platforms
//   templated typedefs for evioDOMLeafNode
//   AIDA interface?




#ifndef _evioUtil_hxx
#define _evioUtil_hxx


using namespace std;
#include <evio.h>
#include <list>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <functional>


// prototypes
class evioDOMNode;
class evioDOMContainerNode;
template<typename T> class evioDOMLeafNode;




// can't get either of these to work...

// bool operator!=(evioDOMNodeListP &lP, int i) {return(lP.get()!=(void*)i);}

// template <typename T> class evio_auto_ptr : public auto_ptr<T> {
// public:
//   evio_auto_ptr(T t) : auto_ptr<T>(t){};
//   bool operator==(evioDOMNode *pNode) const {return(this->get()==pNode);}
// };




//-----------------------------------------------------------------------------
//----------------------------- Typedefs --------------------------------------
//-----------------------------------------------------------------------------


typedef list<const evioDOMNode*>  evioDOMNodeList;
typedef auto_ptr<evioDOMNodeList> evioDOMNodeListP;


//-----------------------------------------------------------------------------
//---------------------------- Misc Structs -----------------------------------
//-----------------------------------------------------------------------------


class setEvioArraySize {public: int val; setEvioArraySize(int i):val(i){} };
class setEvioTag {public: int val; setEvioTag(int i):val(i){} };
class setEvioNum {public: int val; setEvioNum(int i):val(i){} };


//-----------------------------------------------------------------------------
//------------------------- Global Functions ----------------------------------
//-----------------------------------------------------------------------------


template <typename T> bool isNull(auto_ptr<T> &p) {return(p.get()==NULL);}
template <typename T> int getContentType(void);


//-----------------------------------------------------------------------------
//--------------------------- Evio Classes ------------------------------------
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
class evioChannel {

public:
  virtual void open(void) throw(evioException*) = 0;
  virtual bool read(void) throw(evioException*) = 0;
  virtual void write(void) throw(evioException*) = 0;
  virtual void write(const unsigned long *myBuf) throw(evioException*) = 0;
  virtual void write(const evioChannel &channel) throw(evioException*) = 0;
  virtual void close(void) throw(evioException*) = 0;
  virtual const unsigned long *getBuffer(void) const throw(evioException*) = 0;
  virtual int getBufSize(void) const = 0;

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


// evio serializable interface, hook for generic object serialization
class evioSerializable {

public:
  virtual void serialize(evioDOMContainerNode &cNode) throw(evioException*) = 0;

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
  void write(const unsigned long *myBuf) throw(evioException*);
  void write(const evioChannel &channel) throw(evioException*);
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
  evioDOMTree(const evioChannel &channel, const string &name = "evio") throw(evioException*);
  evioDOMTree(const unsigned long *buf, const string &name = "evio") throw(evioException*);
  evioDOMTree(const evioDOMNode *node, const string &name = "evio") throw(evioException*);
  evioDOMTree(const evioDOMTree &tree) throw(evioException*);
  evioDOMTree& operator=(const evioDOMTree &rhs) throw(evioException*);
  virtual ~evioDOMTree(void);

  void toEVIOBuffer(unsigned long *buf) const throw(evioException*);
  const evioDOMNode *getRoot(void) const;

  evioDOMNodeListP getNodeList(void) const throw(evioException*);
  template <typename Predicate> evioDOMNodeListP getNodeList(Predicate pred) const throw(evioException*);
  template <typename T> auto_ptr< list<const evioDOMLeafNode<T>*> > getLeafNodeList(void) const throw(evioException*);

  string toString(void) const;


private:
  evioDOMNode *parse(const unsigned long *buf) throw(evioException*);
  int toEVIOBuffer(unsigned long *buf, const evioDOMNode *pNode) const throw(evioException*);
  template <typename Predicate> evioDOMNodeList *addToNodeList(const evioDOMNode *pNode,evioDOMNodeList *pList, Predicate pred) const
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

  virtual evioDOMNode *clone(evioDOMNode *parent) const = 0;

  virtual const evioDOMNode *getParent(void) const;
  bool isContainer(void) const;
  bool isLeaf(void) const;

  evioDOMNodeListP getContents(void) const throw(evioException*);
  template <typename T> const vector<T> *getContents(void) const throw(evioException*);

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


  // stream operators for building trees and filling nodes
  evioDOMContainerNode& operator<<(const setEvioTag &s) throw(evioException*);
  evioDOMContainerNode& operator<<(const setEvioNum &s) throw(evioException*);
  evioDOMContainerNode& operator<<(const setEvioArraySize &s) throw(evioException*);
  evioDOMContainerNode& operator<<(evioDOMNode *pNode) throw(evioException*);


  // stream operators for object serializtion
  evioDOMContainerNode& operator<<(char c) throw(evioException*);
  evioDOMContainerNode& operator<<(unsigned char uc) throw(evioException*);
  evioDOMContainerNode& operator<<(short s) throw(evioException*);
  evioDOMContainerNode& operator<<(unsigned short us) throw(evioException*);
  evioDOMContainerNode& operator<<(long l) throw(evioException*);
  evioDOMContainerNode& operator<<(unsigned long ul) throw(evioException*);
  evioDOMContainerNode& operator<<(long long ll) throw(evioException*);
  evioDOMContainerNode& operator<<(unsigned long long ull) throw(evioException*);
  evioDOMContainerNode& operator<<(float f) throw(evioException*);
  evioDOMContainerNode& operator<<(double d) throw(evioException*);
  evioDOMContainerNode& operator<<(string &s) throw(evioException*);

  template <typename T> evioDOMContainerNode& operator<<(T *tP) throw(evioException*);
  template <typename T> evioDOMContainerNode& operator<<(vector<T> &v) throw(evioException*);

  evioDOMContainerNode& operator<<(evioSerializable &e) throw(evioException*);


  string toString(void) const;
  string getHeader(int depth) const;
  string getFooter(int depth) const;


public:
  list<evioDOMNode*> childList;

private:
  list<evioDOMNode*>::iterator mark;  // marks current item to stream out
  int streamArraySize;
  int streamTag;
  int streamNum;

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


//  represents an evio leaf node in memory
template <typename T> class evioDOMLeafNode : public evioDOMNode {

public:
  evioDOMLeafNode(evioDOMNode *par, int tg, int n, T *p, int ndata) throw(evioException*);
  evioDOMLeafNode(evioDOMNode *parent, int tag, int num, const vector<T> &v) throw(evioException*);
  evioDOMLeafNode(const evioDOMLeafNode<T> &lNode) throw(evioException*);
  evioDOMLeafNode<T>& operator=(const evioDOMLeafNode<T> &rhs) throw(evioException*);
  virtual ~evioDOMLeafNode(void);

  evioDOMLeafNode<T>* clone(evioDOMNode *newParent) const;

  string toString(void) const;
  string getHeader(int depth) const;
  string getFooter(int depth) const;

public:
  vector<T> data;
};


//-----------------------------------------------------------------------------
//------------ evioDOMNode templated methods (not overridden) -----------------
//-----------------------------------------------------------------------------


template <typename T> const vector<T> *evioDOMNode::getContents(void) const throw(evioException*) {
  const evioDOMLeafNode<T> *l = dynamic_cast<const evioDOMLeafNode<T>*>(this);
  return((l==NULL)?(NULL):&l->data);
}


//-----------------------------------------------------------------------------
//--------- evioDOMContainerNode templated methods (not overridden) -----------
//-----------------------------------------------------------------------------


template <typename T> evioDOMContainerNode& evioDOMContainerNode::operator<<(vector<T> &v) throw(evioException*) {
  evioDOMLeafNode<T> *leaf = new evioDOMLeafNode<T>(this,0,0,NULL,0);
  copy(v.begin(),v.end(),back_inserter(leaf->data));
  childList.push_back(leaf);
  return(*this);
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMContainerNode& evioDOMContainerNode::operator<<(T *tP) throw(evioException*) {
  evioDOMLeafNode<T> *leaf = new evioDOMLeafNode<T>(this,0,0,tP,streamArraySize);
  childList.push_back(leaf);
  return(*this);
}


//-----------------------------------------------------------------------------
//------------ evioDOMTree templated methods (not overridden) -----------------
//-----------------------------------------------------------------------------


template <typename Predicate> evioDOMNodeListP evioDOMTree::getNodeList(Predicate pred) const throw(evioException*) {
  evioDOMNodeList *pList = addToNodeList(root,new evioDOMNodeList,pred);
  return(evioDOMNodeListP(pList));
}  


//-----------------------------------------------------------------------------


template <typename Predicate> evioDOMNodeList *evioDOMTree::addToNodeList(const evioDOMNode *pNode, 
                                                                          evioDOMNodeList *pList, Predicate pred) const
  throw(evioException*) {

  if(pNode==NULL)return(pList);


  // add this node to list
  if(pred(pNode))pList->push_back(pNode);
  
  
  // add children to list
  const evioDOMContainerNode *c = dynamic_cast<const evioDOMContainerNode*>(pNode);
  if(c!=NULL) {
   list<evioDOMNode*>::const_iterator iter;
    for(iter=c->childList.begin(); iter!=c->childList.end(); iter++) {
      addToNodeList(*iter,pList,pred);
    }
  }


  // return the list
  return(pList);
}


//-----------------------------------------------------------------------------


template <typename T> auto_ptr< list<const evioDOMLeafNode<T>*> > evioDOMTree::getLeafNodeList(void) const
  throw(evioException*) {

  evioDOMNodeListP pNodeList = getNodeList();
  auto_ptr< list<const evioDOMLeafNode<T>*> > pLeafList = 
    auto_ptr< list<const evioDOMLeafNode<T>*> >(new list<const evioDOMLeafNode<T>*>);
  
  evioDOMNodeList::const_iterator iter;
  const evioDOMLeafNode<T>* p;
  for(iter=pNodeList->begin(); iter!=pNodeList->end(); iter++) {
    p=dynamic_cast<const evioDOMLeafNode<T>*>(*iter);
    if(p!=NULL)pLeafList->push_back(p);
  }
  
  return(pLeafList);
}


//-----------------------------------------------------------------------------
//--------------------- Misc Function Objects ---------------------------------
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


class parentTypeEquals : unary_function<const evioDOMNode*, bool> {

public:
  parentTypeEquals(int aType) : type(aType) {}
  bool operator()(const evioDOMNode* node) const {return((node->parent==NULL)?false:(node->parent->contentType==type));}
private:
  int type;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class parentTagEquals : unary_function<const evioDOMNode*, bool> {

public:
  parentTagEquals(int aTag) : tag(aTag) {}
  bool operator()(const evioDOMNode* node) const {return((node->parent==NULL)?false:(node->parent->tag==tag));}
private:
  int tag;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class parentNumEquals : unary_function<const evioDOMNode*, bool> {

public:
  parentNumEquals(int aNum) : num(aNum) {}
  bool operator()(const evioDOMNode* node) const {return((node->parent==NULL)?false:(node->parent->num==num));}
private:
  int num;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class parentTagNumEquals : unary_function<const evioDOMNode*, bool> {

public:
  parentTagNumEquals(int aTag, int aNum) : tag(aTag), num(aNum) {}
  bool operator()(const evioDOMNode* node) const {
    return((node->parent==NULL)?false:((node->parent->tag==tag)&&(node->parent->num==num)));}
private:
  int tag;
  int num;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class isContainer : unary_function<const evioDOMNode*,bool> {

public:
  isContainer(void) {}
  bool operator()(const evioDOMNode* node) const {return(node->isContainer());}
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class isLeaf : unary_function<const evioDOMNode*,bool> {

public:
  isLeaf(void) {}
  bool operator()(const evioDOMNode* node) const {return(node->isLeaf());}
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class toCout: public unary_function<const evioDOMNode*, void> {

public:
  toCout(void) {}
  void operator()(const evioDOMNode* node) const {cout << node->toString() << endl;}
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#endif
