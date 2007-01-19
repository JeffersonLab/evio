// evioUtil.hxx

//  Author:  Elliott Wolin, JLab, 17-jan-2007


// should do:
//   Doxygen comments


//  would like to do:
//   shared pointer
//   cMsg channel
//   ET channel
//   exception stack trace


// not sure:
//   solaris: count_if and function objects?
//   split header files?
//   auto_ptr?
//   scheme for exception type codes?
//   AIDA interface?


#ifndef _evioUtil_hxx
#define _evioUtil_hxx


#include <evio.h>
#include <list>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <functional>
#include <utility>



/** All evio symbols reside in this namespace.*/
namespace evio {

using namespace std;


// evio classes:
class evioException;
class evioChannel;
class evioFileChannel;
class evioStreamParserHandler;
class evioStreamParser;
class evioDOMTree;
class evioDOMNode;
class evioDOMContainerNode;
template<typename T> class evioDOMLeafNode;
class evioSerializable;
template <typename T> class evioUtil;

// plus a number of function object classes defined below



//-----------------------------------------------------------------------------
//----------------------------- Typedefs --------------------------------------
//-----------------------------------------------------------------------------


typedef evioDOMTree* evioDOMTreeP;                   /**<Pointer to evioDOMTree.*/
typedef evioDOMNode* evioDOMNodeP;                   /**<Pointer to evioDOMNode, only way to access nodes.*/
typedef list<evioDOMNodeP>  evioDOMNodeList;         /**<List of pointers to evioDOMNode.*/
typedef auto_ptr<evioDOMNodeList> evioDOMNodeListP;  /**<auto-ptr of list of evioDOMNode pointers, returned by getNodeList.*/
/** Defines the three container bank types.*/
typedef enum {
  BANK       = 0xe,  /**<2-word header, 16-bit tag, 8-bit num, 8-bit type.*/
  SEGMENT    = 0xd,  /**<1-word header,  8-bit tag,    no num, 8-bit type.*/
  TAGSEGMENT = 0xc   /**<1-word header, 12-bit tag,    no num, 4-bit type.*/
} ContainerType;
typedef pair<unsigned short, unsigned char> tagNum;  /**<STL pair of tag,num.*/


//-----------------------------------------------------------------------------
//--------------------------- evio Classes ------------------------------------
//-----------------------------------------------------------------------------


/**
 * Basic evio exception class.  Includes integer type and two text fields.
 */
class evioException {

public:
  evioException(int typ = 0, const string &txt = "", const string &aux = "");
  evioException(int typ, const string &txt, const string &file, int line);
  virtual string toString(void) const;


public:
  int type;        /**<Exception type.*/
  string text;     /**<Primary text.*/
  string auxText;  /**<Auxiliary text.*/

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Virtual class defines EVIO I/O channel functionality.
 * Sub-class gets channel-specific info from constructor and implements 
 * evioChannel methods.
 */
class evioChannel {

public:
  virtual void open(void) throw(evioException) = 0;
  virtual bool read(void) throw(evioException) = 0;
  virtual void write(void) throw(evioException) = 0;
  virtual void write(const unsigned long *myBuf) throw(evioException) = 0;
  virtual void write(const unsigned int* myBuf) throw(evioException) = 0;
  virtual void write(const evioChannel &channel) throw(evioException) = 0;
  virtual void write(const evioChannel *channel) throw(evioException) = 0;
  virtual void write(const evioDOMTree &tree) throw(evioException) = 0;
  virtual void write(const evioDOMTree *tree) throw(evioException) = 0;
  virtual void close(void) throw(evioException) = 0;

  // not sure if these should be part of the interface
  virtual const unsigned int *getBuffer(void) const throw(evioException) = 0;
  virtual int getBufSize(void) const = 0;

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Implements evioChannel functionality for I/O to and from files.
 * Basically a wrapper around the original evio C library.
 */
class evioFileChannel : public evioChannel {

public:
  evioFileChannel(const string &fileName, const string &mode = "r", int size = 8192) throw(evioException);
  virtual ~evioFileChannel();

  void open(void) throw(evioException);
  bool read(void) throw(evioException);
  void write(void) throw(evioException);
  void write(const unsigned long *myBuf) throw(evioException);
  void write(const unsigned int *myBuf) throw(evioException);
  void write(const evioChannel &channel) throw(evioException);
  void write(const evioChannel *channel) throw(evioException);
  void write(const evioDOMTree &tree) throw(evioException);
  void write(const evioDOMTree *tree) throw(evioException);
  void close(void) throw(evioException);

  const unsigned int *getBuffer(void) const throw(evioException);
  int getBufSize(void) const;

  void ioctl(const string &request, void *argp) throw(evioException);
  string getFileName(void) const;
  string getMode(void) const;


private:
  string filename;    /**<Name of evio file.*/
  string mode;        /**<Open mode, "r" or "w".*/
  int handle;         /**<Internal evio file handle.*/
  unsigned int *buf;  /**<Pointer to internal buffer.*/
  int bufSize;        /**<Size of internal buffer.*/
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Interface defines node and leaf handlers for use with evioStreamParser.
 * Separate handlers defined for container nodes and leaf nodes.
 */
class evioStreamParserHandler {

public:
  virtual void *containerNodeHandler(int length, unsigned short tag, int contentType, unsigned char num, 
                            int depth, void *userArg) = 0;
  virtual void leafNodeHandler(int length, unsigned short tag, int contentType, unsigned char num, 
                           int depth, const void *data, void *userArg) = 0;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Stream parser dispatches to evioStreamParserHandler handlers when node or leaf reached.
 */
class evioStreamParser {

public:
  void *parse(const unsigned int *buf, evioStreamParserHandler &handler, void *userArg)
    throw(evioException);
  
private:
  void *parseBank(const unsigned int *buf, int bankType, int depth, 
                  evioStreamParserHandler &handler, void *userArg) throw(evioException);

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/** 
 * Virtual class represents an evio node in memory, concrete sub-classes evioDOMContainerNode and evioDOMLeafNode
 *   are hidden from users.
 * Users work with nodes via this class, and create nodes via factory method createEvioDOMNode.
 * Factory model ensures nodes are created on heap.  
 */
class evioDOMNode {

  friend class evioDOMTree;    /**<Allows evioDOMTree class to manipulate nodes.*/


protected:
  evioDOMNode(evioDOMNodeP parent, unsigned short tag, unsigned char num, int contentType) throw(evioException);
  ~evioDOMNode(void);


private:
  evioDOMNode(const evioDOMNode &node) throw(evioException);     /**<Not available to users */
  bool operator=(const evioDOMNode &node) const {return(false);} /**<Not available to users */


// public factory methods for node creation
public:
  static evioDOMNodeP createEvioDOMNode(unsigned short tag, unsigned char num, ContainerType cType=BANK) throw(evioException);
  template <typename T> static evioDOMNodeP createEvioDOMNode(unsigned short tag, unsigned char num, const vector<T> tVec)
    throw(evioException);
  template <typename T> static evioDOMNodeP createEvioDOMNode(unsigned short tag, unsigned char num, const T* t, int len)
    throw(evioException);
  static evioDOMNodeP createEvioDOMNode(unsigned short tag, unsigned char num, const evioSerializable &o, ContainerType cType=BANK) 
    throw(evioException);
  static evioDOMNodeP createEvioDOMNode(unsigned short tag, unsigned char num, void (*f)(evioDOMNodeP c, void *userArg), void *userArg, 
                                        ContainerType cType=BANK) throw(evioException);
  template <typename T> static evioDOMNodeP createEvioDOMNode(unsigned short tag, unsigned char num, T *t,
                                                              void *userArg, ContainerType cType=BANK) throw(evioException);
  template <typename T> static evioDOMNodeP createEvioDOMNode(unsigned short tag, unsigned char num, T *t, 
                                                              void* T::*mfp(evioDOMNodeP c, void *userArg),
                                                              void *userArg, ContainerType cType=BANK) throw(evioException);


public:
  virtual void addNode(evioDOMNodeP node) throw(evioException);
  template <typename T> void append(const vector<T> &tVec) throw(evioException);
  template <typename T> void append(const T* tBuf, int len) throw(evioException);
  template <typename T> void replace(const vector<T> &tVec) throw(evioException);
  template <typename T> void replace(const T* tBuf, int len) throw(evioException);


public:
  virtual evioDOMNodeP cut(void) throw(evioException);
  virtual void cutAndDelete(void) throw(evioException);
  virtual evioDOMNodeP move(evioDOMNodeP newParent) throw(evioException);


public:
  virtual bool operator==(unsigned short tag) const;
  virtual bool operator!=(unsigned short tag) const;
  bool operator==(tagNum tnPair) const;
  bool operator!=(tagNum tnPair) const;


public:
  evioDOMNode& operator<<(evioDOMNodeP node) throw(evioException);
  template <typename T> evioDOMNode& operator<<(T tVal) throw(evioException);
  template <typename T> evioDOMNode& operator<<(const vector<T> &tVec) throw(evioException);
  template <typename T> evioDOMNode& operator<<(      vector<T> &tVec) throw(evioException);


public:
  evioDOMNodeList *getChildList(void) throw(evioException);
  template <typename T> vector<T> *getVector(void) throw(evioException);


public:
  virtual string toString(void) const       = 0;
  virtual string getHeader(int depth) const = 0;
  virtual string getFooter(int depth) const = 0;

  const evioDOMNodeP getParent(void) const;
  int getContentType(void) const;
  const evioDOMTreeP getParentTree(void) const;
  bool isContainer(void) const;
  bool isLeaf(void) const;


protected:
  static string getIndent(int depth);


protected:
  evioDOMNodeP parent;           /**<Pointer to node parent.*/
  evioDOMTreeP parentTree;       /**<Pointer to parent tree if this node is the root.*/
  int contentType;               /**<Content type.*/


public:
  unsigned short tag;            /**<The node tag, max 16-bits depending on container type.*/
  unsigned char num;             /**<The node num, max 8 bits, only used by BANK container type (only one with 2-word header).*/
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Sub-class of evioDOMNode represents an evio container node.
 * Only accessible to users via pointer to evioDOMNode object.
 */
class evioDOMContainerNode : public evioDOMNode {

  friend class evioDOMNode;    /**<Allows evioDOMNode to use private subclass methods.*/


private:
  evioDOMContainerNode(evioDOMNodeP parent, unsigned short tag, unsigned char num, ContainerType cType) throw(evioException);
  evioDOMContainerNode(const evioDOMContainerNode &cNode) throw(evioException);  /**<Not available to user */
  bool operator=(const evioDOMContainerNode &node);                              /**<Not available to user */


public:
  string toString(void) const;
  string getHeader(int depth) const;
  string getFooter(int depth) const;


public:
  evioDOMNodeList childList;   /**<STL List of pointers to children.*/

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Sub-class of evioDOMNode represents an evio leaf node.
 * Only accessible to users via pointer to evioDOMNode object.
 */
template <typename T> class evioDOMLeafNode : public evioDOMNode {

  friend class evioDOMNode;     /**<Allows evioDOMNode to use private subclass methods.*/


private:
  evioDOMLeafNode(evioDOMNodeP par, unsigned short tag, unsigned char num, const vector<T> &v) throw(evioException);
  evioDOMLeafNode(evioDOMNodeP par, unsigned short tag, unsigned char num, const T *p, int ndata) throw(evioException);
  evioDOMLeafNode(const evioDOMLeafNode<T> &lNode) throw(evioException);  /**<Not available to user */
  bool operator=(const evioDOMLeafNode<T> &lNode);                        /**<Not available to user */


public:
  string toString(void) const;
  string getHeader(int depth) const;
  string getFooter(int depth) const;


public:
  vector<T> data;    /**<Vector<T> of node data.*/
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Represents an evio tree/event in memory.
 * Tree root is an evioDOMNode.
 */
class evioDOMTree : public evioStreamParserHandler {


public:
  evioDOMTree(const evioChannel &channel, const string &name = "evio") throw(evioException);
  evioDOMTree(const evioChannel *channel, const string &name = "evio") throw(evioException);
  evioDOMTree(const unsigned long *buf, const string &name = "evio") throw(evioException);
  evioDOMTree(const unsigned int *buf, const string &name = "evio") throw(evioException);
  evioDOMTree(evioDOMNodeP node, const string &name = "evio") throw(evioException);
  evioDOMTree(unsigned short tag, unsigned char num, ContainerType cType=BANK, const string &name = "evio")
    throw(evioException);
  virtual ~evioDOMTree(void);


private:
  evioDOMTree(const evioDOMTree &tree) throw(evioException);  /**<Not available to user */
  bool operator=(const evioDOMTree &tree);                    /**<Not available to user */


public:
  void clear(void) throw(evioException);
  void addBank(evioDOMNodeP node) throw(evioException);
  template <typename T> void addBank(unsigned short tag, unsigned char num, const vector<T> dataVec) throw(evioException);
  template <typename T> void addBank(unsigned short tag, unsigned char num, const T* dataBuf, int dataLen) throw(evioException);


public:
  evioDOMTree& operator<<(evioDOMNodeP node) throw(evioException);


public:
  void toEVIOBuffer(unsigned long *buf, int size) const throw(evioException);
  void toEVIOBuffer(unsigned int *buf, int size) const throw(evioException);


public:
  evioDOMNodeListP getNodeList(void) throw(evioException);
  template <class Predicate> evioDOMNodeListP getNodeList(Predicate pred) throw(evioException);


public:
  string toString(void) const;


private:
  evioDOMNodeP parse(const unsigned int *buf) throw(evioException);
  int toEVIOBuffer(unsigned int *buf, const evioDOMNodeP pNode, int size) const throw(evioException);
  template <class Predicate> evioDOMNodeList *addToNodeList(evioDOMNodeP pNode, evioDOMNodeList *pList, Predicate pred)
    throw(evioException);
  
  void toOstream(ostream &os, const evioDOMNodeP node, int depth) const throw(evioException);
  void *containerNodeHandler(int length, unsigned short tag, int contentType, unsigned char num, int depth, void *userArg);
  void leafNodeHandler(int length, unsigned short tag, int contentType, unsigned char num, int depth, const void *data, void *userArg);


public:
  evioDOMNodeP root;    /**<Pointer to root node of tree.*/
  string name;          /**<Name of tree.*/

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Interface for object serialization.
 * Just defines serialize method.
 */
class evioSerializable {

public:
  virtual void serialize(evioDOMNodeP node) const throw(evioException) = 0;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


// include templates
#include <evioUtilTemplates.hxx>



}  // namespace evio


#endif


