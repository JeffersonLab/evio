// evioUtil.hxx

//  Author:  Elliott Wolin, JLab, 6-Nov-2006


// must do:
//   const vs non-const access to tree
//   check bufsize in toEVIOBuffer(), parseBank, etc.
//   evioDOMTree tree(tag=1, num=0, string("fred"), 0x30) should generate error
//   API for manual tree creation and modification, add/drop/move sub-trees, etc?
//   const correctness
//   Doxygen comments

// should do:
//   evioChannel and output, getBuffer() and const?
//   operator<< and operator>> for container node class, virtual class streamable 
//   mark node in evioDOMTree

//  would like to do:
//   cMsg channel
//   scheme for exception type codes
//   exception stack trace if supported on all platforms
//   templated typedefs for evioDOMLeafNode
//   AIDA interface?

// not sure:
//   namespaces?
//   remove auto_ptr<>?
//   remove isTrue()?
//   evioOutputFileChannel, etc.
//   write(node)?


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
class evioDOMTree;
class evioDOMNode;
class evioDOMContainerNode;
template<typename T> class evioDOMLeafNode;




//-----------------------------------------------------------------------------
//----------------------------- Typedefs --------------------------------------
//-----------------------------------------------------------------------------


typedef list<const evioDOMNode*>  evioDOMNodeList;
typedef auto_ptr<evioDOMNodeList> evioDOMNodeListP;


//-----------------------------------------------------------------------------
//----------------------------- for stream API --------------------------------
//-----------------------------------------------------------------------------


class setEvioArraySize {public: int val; setEvioArraySize(int i) : val(i){} };
class setEvioTag       {public: int val; setEvioTag(int i)       : val(i){} };
class setEvioNum       {public: int val; setEvioNum(int i)       : val(i){} };


//-----------------------------------------------------------------------------
//------------------------- Global Functions ----------------------------------
//-----------------------------------------------------------------------------


//template <typename T> bool isNull(auto_ptr<T> &p) {return(p.get()==NULL);}
template <typename T> int getContentType(void);


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//--------------------------- evio Classes ------------------------------------
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
class evioChannel {

public:
  virtual void open(void) throw(evioException*) = 0;
  virtual bool read(void) throw(evioException*) = 0;
  virtual void write(void) throw(evioException*) = 0;
  virtual void write(const unsigned long *myBuf) throw(evioException*) = 0;
  virtual void write(const evioChannel &channel) throw(evioException*) = 0;
  virtual void write(const evioChannel *channel) throw(evioException*) = 0;
  virtual void write(const evioDOMTree &tree) throw(evioException*) = 0;
  virtual void write(const evioDOMTree *tree) throw(evioException*) = 0;
  virtual void close(void) throw(evioException*) = 0;

  // not sure if these should be part of the interface
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
  void write(const unsigned long *myBuf) throw(evioException*);
  void write(const evioChannel &channel) throw(evioException*);
  void write(const evioChannel *channel) throw(evioException*);
  void write(const evioDOMTree &tree) throw(evioException*);
  void write(const evioDOMTree *tree) throw(evioException*);
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


// interface defines node and leaf handlers for stream parsing of evio buffers
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
  evioDOMTree(const evioChannel *channel, const string &name = "evio") throw(evioException*);
  evioDOMTree(const unsigned long *buf, const string &name = "evio") throw(evioException*);
  evioDOMTree(const evioDOMNode *node, const string &name = "evio") throw(evioException*);

  evioDOMTree(int tag, int num, const string &name = "evio", int contentType = 0x10) throw(evioException*);
  template <typename T> evioDOMTree(int tag, int num, const vector<T> &dataVec, const string &name = "evio") throw(evioException*);
  template <typename T> evioDOMTree(int tag, int num, const vector<T> *pDataVec, const string &name = "evio") throw(evioException*);
  template <typename T> evioDOMTree(int tag, int num, const T* dataBuf, int dataLen, const string &name = "evio") throw(evioException*);


  evioDOMTree(const evioDOMTree &tree) throw(evioException*);
  evioDOMTree& operator=(const evioDOMTree &rhs) throw(evioException*);
  virtual ~evioDOMTree(void);


  template <typename T> void addBank(int tag, int num, const vector<T> dataVec) throw(evioException*);
  template <typename T> void addBank(int tag, int num, const T* dataBuf, int dataLen) throw(evioException*);


  void toEVIOBuffer(unsigned long *buf, int size) const throw(evioException*);
  const evioDOMNode *getRoot(void) const;


  evioDOMNodeListP getNodeList(void) const throw(evioException*);
  template <class Predicate> evioDOMNodeListP getNodeList(Predicate pred) const throw(evioException*);

  //  template <typename T> auto_ptr< list<const evioDOMLeafNode<T>*> > getLeafNodeList(void) const throw(evioException*);

  string toString(void) const;


private:
  evioDOMNode *parse(const unsigned long *buf) throw(evioException*);
  int toEVIOBuffer(unsigned long *buf, const evioDOMNode *pNode, int size) const throw(evioException*);
  template <class Predicate> evioDOMNodeList *addToNodeList(const evioDOMNode *pNode,evioDOMNodeList *pList, Predicate pred) const
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


// evio serializable interface, hook for generic object serialization
class evioSerializable {

public:
  // what about leaf nodes?
  virtual void serialize(evioDOMContainerNode &cNode) const throw(evioException*) = 0;

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


//  represents an evio node in memory
class evioDOMNode {

public:
  evioDOMNode(evioDOMNode *parent, int tag, int contentType, int num) throw(evioException*);
  virtual ~evioDOMNode(void);

  virtual bool operator==(int tag) const;
  virtual bool operator!=(int tag) const;

  virtual evioDOMNode *clone(evioDOMNode *parent) const = 0;

  virtual const evioDOMNode *getParent(void) const;
  bool isContainer(void) const;
  bool isLeaf(void) const;

  evioDOMNodeListP getChildList(void) const throw(evioException*);
  template <typename T> const vector<T> *getVector(void) const throw(evioException*);

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

  evioDOMContainerNode& operator<<(long *lp) throw(evioException*);
  evioDOMContainerNode& operator<<(evioSerializable &e) throw(evioException*);


//   template <typename T> evioDOMContainerNode& operator<<(T t) throw(evioException*);
//   template <typename T> evioDOMContainerNode& operator<<(T *tP) throw(evioException*);
  template <typename T> evioDOMContainerNode& operator<<(const vector<T> &v) throw(evioException*);



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
  evioDOMLeafNode(evioDOMNode *par, int tg, int n, const vector<T> &v) throw(evioException*);
  evioDOMLeafNode(evioDOMNode *par, int tg, int n, const T *p, int ndata) throw(evioException*);
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
//-----------------------------------------------------------------------------






//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//--------------------- Misc Function Objects ---------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


template <typename T> class typeIs : unary_function<const evioDOMNode*,bool> {

public:
  typeIs(void) : type(getContentType<T>()) {}
  bool operator()(const evioDOMNode* node) const {return(node->contentType==type);}
private:
  int type;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class typeEquals : unary_function<const evioDOMNode*,bool> {

public:
  typeEquals(int aType) : type(aType) {}
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



// include template bodies
#include <evioUtilTemplates.hxx>

#endif
