// evioUtil.hxx

//  Author:  Elliott Wolin, JLab, 5-apr-2010


//  must do:
//   char* arrays
//   string bank and version number
//   update word doc


//  should do:
//   shared pointer


//  would like to do:
//   cMsg channel
//   ET channel


// not sure:
//   scheme for exception type codes?


#ifndef _evioUtil_hxx
#define _evioUtil_hxx


#include <list>
#include <vector>
#include <sstream>
#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <cstring>
#include <typeinfo>

#ifdef vxworks
#include <iostream.h>
#else
#include <iostream>
#include <iomanip>
#endif

#include <evio_util.h>
#include <evioException.hxx>
#include <evioChannel.hxx>



#ifdef sun
#define __FUNCTION__ "unknown"
#endif



/** @mainpage  EVIO Event I/O Package.
 * @author Elliott Wolin.
 * @version 2.0.
 * @date 5-Feb-2007.
 *
 * @section intro Introduction
 * The EVIO package is an object-oriented extension of the original EVIO C event I/O utility.  
 *
 * The base utility reads and writes EVIO format events in an event buffer to and from disk.  
 * Events are blocked into standard block sizes, and endian swapping is performed if necessary.
 *
 * This package maps the event in the event buffer into an in-memory tree-like bank hierarchy.  
 * Event trees can be queried, modified, or created from scratch.  In-memory trees can be automatically 
 * serialized into a buffer and written to disk.
 * 
 * @warning Internally EVIO uses only the unambiguous types int8_t, uint8_t, ... int64_t, uint64_t.  
 * The long and long long data types are not supported, as their interpretion varies among different compilers
 * and architectures.  The unambiguous types are compatible with char, short, and int wherever I have checked.
 * But only the 64-bit types int64_t and uint64_t work consistently across architectures.
 *
 * @warning The safest route, of course, is to use the unambiguous types exclusively.  
 * But the example programs use char, short, int, and int64_t and so far work fine.  No guarantees 
 * for the future, though...
 */


/** All evio symbols reside in the evio namespace.*/
namespace evio {

using namespace std;
using namespace evio;


// evio classes:
class evioStreamParserHandler;
class evioStreamParser;
class evioDOMTree;
class evioDOMNode;
class evioDOMContainerNode;
template<typename T> class evioDOMLeafNode;
class evioSerializable;
template <typename T> class evioUtil;

// plus a number of function object classes defined below




// not sure whether to use this or not...ejw
template <class X> class counted_ptr {

public:
    typedef X element_type;

    explicit counted_ptr(X* p = 0) // allocate a new counter
        : itsCounter(0) {if (p) itsCounter = new counter(p);}
    ~counted_ptr()
        {release();}
    counted_ptr(const counted_ptr& r) throw()
        {acquire(r.itsCounter);}
    counted_ptr& operator=(const counted_ptr& r)
    {
        if (this != &r) {
            release();
            acquire(r.itsCounter);
        }
        return *this;
    }

    X& operator*()  const throw()   {return *itsCounter->ptr;}
    X* operator->() const throw()   {return itsCounter->ptr;}
    X* get()        const throw()   {return itsCounter ? itsCounter->ptr : 0;}
    bool unique()   const throw()
        {return (itsCounter ? itsCounter->count == 1 : true);}

private:

    struct counter {
        counter(X* p = 0, unsigned c = 1) : ptr(p), count(c) {}
        X*          ptr;
        unsigned    count;
    }* itsCounter;

    void acquire(counter* c) throw()
    { // increment the count
        itsCounter = c;
        if (c) ++c->count;
    }

    void release()
    { // decrement the count, delete if it is 0
        if (itsCounter) {
            if (--itsCounter->count == 0) {
                delete itsCounter->ptr;
                delete itsCounter;
            }
            itsCounter = 0;
        }
    }
};





//-----------------------------------------------------------------------------
//----------------------------- Typedefs --------------------------------------
//-----------------------------------------------------------------------------


typedef evioDOMTree* evioDOMTreeP;                   /**<Pointer to evioDOMTree.*/


typedef evioDOMNode* evioDOMNodeP;                   /**<Pointer to evioDOMNode, only way to access nodes.*/
//typedef counted_ptr<evioDOMNode*> evioDOMNodeP;                   /**<Pointer to evioDOMNode, only way to access nodes.*/



typedef list<evioDOMNodeP>  evioDOMNodeList;         /**<List of pointers to evioDOMNode.*/
typedef auto_ptr<evioDOMNodeList> evioDOMNodeListP;  /**<auto-ptr of list of evioDOMNode pointers, returned by getNodeList.*/
/** Defines the container bank types.*/
typedef enum {
  BANK       = 0xe,  /**<2-word header, 16-bit tag, 8-bit num, 8-bit type.*/
  SEGMENT    = 0xd,  /**<1-word header,  8-bit tag,    no num, 8-bit type.*/
  TAGSEGMENT = 0xc   /**<1-word header, 12-bit tag,    no num, 4-bit type.*/
} ContainerType;
typedef pair<uint16_t, uint8_t> tagNum;  /**<STL pair of tag,num.*/


//-----------------------------------------------------------------------------
//--------------------------- evio Classes ------------------------------------
//-----------------------------------------------------------------------------


/**
 * Interface defines node and leaf handlers for use with evioStreamParser.
 * Separate handlers defined for container nodes and leaf nodes.
 */
class evioStreamParserHandler {

public:
  virtual void *containerNodeHandler(int length, uint16_t tag, int contentType, uint8_t num, 
                            int depth, void *userArg) = 0;
  virtual void *leafNodeHandler(int length, uint16_t tag, int contentType, uint8_t num, 
                                int depth, const void *data, void *userArg) = 0;
  virtual ~evioStreamParserHandler(void) {};
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Stream parser dispatches to evioStreamParserHandler handlers when node or leaf reached.
 */
class evioStreamParser {

public:
  void *parse(const uint32_t *buf, evioStreamParserHandler &handler, void *userArg)
    throw(evioException);
  virtual ~evioStreamParser(void) {};

  
private:
  void *parseBank(const uint32_t *buf, int bankType, int depth, 
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
  evioDOMNode(evioDOMNodeP parent, uint16_t tag, uint8_t num, int contentType) throw(evioException);

public:
  virtual ~evioDOMNode(void);


private:
  evioDOMNode(const evioDOMNode &node) throw(evioException);
  bool operator=(const evioDOMNode &node) const {return(false);}


// public factory methods for node creation
public:
  static evioDOMNodeP createEvioDOMNode(uint16_t tag, uint8_t num, ContainerType cType=BANK) throw(evioException);
  template <typename T> static evioDOMNodeP createEvioDOMNode(uint16_t tag, uint8_t num) throw(evioException);
  template <typename T> static evioDOMNodeP createEvioDOMNode(uint16_t tag, uint8_t num, const vector<T> tVec)
    throw(evioException);
  template <typename T> static evioDOMNodeP createEvioDOMNode(uint16_t tag, uint8_t num, const T* t, int len)
    throw(evioException);
  static evioDOMNodeP createEvioDOMNode(uint16_t tag, uint8_t num, const evioSerializable &o, ContainerType cType=BANK) 
    throw(evioException);
  static evioDOMNodeP createEvioDOMNode(uint16_t tag, uint8_t num, void (*f)(evioDOMNodeP c, void *userArg), void *userArg, 
                                        ContainerType cType=BANK) throw(evioException);
  template <typename T> static evioDOMNodeP createEvioDOMNode(uint16_t tag, uint8_t num, T *t,
                                                              void *userArg, ContainerType cType=BANK) throw(evioException);
  template <typename T> static evioDOMNodeP createEvioDOMNode(uint16_t tag, uint8_t num, T *t, 
                                                              void* T::*mfp(evioDOMNodeP c, void *userArg),
                                                              void *userArg, ContainerType cType=BANK) throw(evioException);


public:
  virtual void addNode(evioDOMNodeP node) throw(evioException);
  void append(const string &s) throw(evioException);
  void append(const char *s) throw(evioException);
  void append(char *s) throw(evioException);
  void append(const char **ca, int len) throw(evioException);
  void append(char **ca, int len) throw(evioException);

  template <typename T> void append(T tVal) throw(evioException);
  template <typename T> void append(const vector<T> &tVec) throw(evioException);
  template <typename T> void append(const T* tBuf, int len) throw(evioException);
  template <typename T> void replace(const vector<T> &tVec) throw(evioException);
  template <typename T> void replace(const T* tBuf, int len) throw(evioException);


public:
  virtual evioDOMNodeP cut(void) throw(evioException);
  virtual void cutAndDelete(void) throw(evioException);
  virtual evioDOMNodeP move(evioDOMNodeP newParent) throw(evioException);


public:
  virtual bool operator==(uint16_t tag) const;
  virtual bool operator!=(uint16_t tag) const;
  bool operator==(tagNum tnPair) const;
  bool operator!=(tagNum tnPair) const;


public:
  evioDOMNode& operator<<(evioDOMNodeP node) throw(evioException);
  evioDOMNode& operator<<(const string &s) throw(evioException);
  evioDOMNode& operator<<(const char *s) throw(evioException);
  evioDOMNode& operator<<(char *s) throw(evioException);
  template <typename T> evioDOMNode& operator<<(T tVal) throw(evioException);
  template <typename T> evioDOMNode& operator<<(const vector<T> &tVec) throw(evioException);


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
  uint16_t tag;            /**<The node tag, max 16-bits depending on container type.*/
  uint8_t num;             /**<The node num, max 8 bits, used by BANK and String container types (2-word header).*/
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
  evioDOMContainerNode(evioDOMNodeP parent, uint16_t tag, uint8_t num, ContainerType cType) throw(evioException);
 ~evioDOMContainerNode(void);

  evioDOMContainerNode(const evioDOMContainerNode &cNode) throw(evioException);
  bool operator=(const evioDOMContainerNode &node);


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
  evioDOMLeafNode(evioDOMNodeP par, uint16_t tag, uint8_t num) throw(evioException);
  evioDOMLeafNode(evioDOMNodeP par, uint16_t tag, uint8_t num, const vector<T> &v) throw(evioException);
  evioDOMLeafNode(evioDOMNodeP par, uint16_t tag, uint8_t num, const T *p, int ndata) throw(evioException);
  evioDOMLeafNode(const evioDOMLeafNode<T> &lNode) throw(evioException);
  bool operator=(const evioDOMLeafNode<T> &lNode);


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
class evioDOMTree : public evioStreamParserHandler, public evioChannelBufferizable {


public:
  evioDOMTree(const evioChannel &channel, const string &name = "evio") throw(evioException);
  evioDOMTree(const evioChannel *channel, const string &name = "evio") throw(evioException);
  evioDOMTree(const uint32_t *buf, const string &name = "evio") throw(evioException);
  evioDOMTree(evioDOMNodeP node, const string &name = "evio") throw(evioException);
  evioDOMTree(uint16_t tag, uint8_t num, ContainerType cType=BANK, const string &name = "evio")
    throw(evioException);
  virtual ~evioDOMTree(void);


private:
  evioDOMTree(const evioDOMTree &tree) throw(evioException);
  bool operator=(const evioDOMTree &tree);


public:
  void clear(void) throw(evioException);
  void addBank(evioDOMNodeP node) throw(evioException);
  template <typename T> void addBank(uint16_t tag, uint8_t num, const vector<T> dataVec) throw(evioException);
  template <typename T> void addBank(uint16_t tag, uint8_t num, const T* dataBuf, int dataLen) throw(evioException);


public:
  evioDOMTree& operator<<(evioDOMNodeP node) throw(evioException);


public:
  int getSerializedLength(void) const throw(evioException);
  int toEVIOBuffer(uint32_t *buf, int size) const throw(evioException);


public:
  evioDOMNodeListP getNodeList(void) throw(evioException);
  template <class Predicate> evioDOMNodeListP getNodeList(Predicate pred) throw(evioException);


public:
  string toString(void) const;


private:
  evioDOMNodeP parse(const uint32_t *buf) throw(evioException);
  int getSerializedLength(const evioDOMNodeP pNode) const throw(evioException);
  int toEVIOBuffer(uint32_t *buf, const evioDOMNodeP pNode, int size) const throw(evioException);
  template <class Predicate> evioDOMNodeList *addToNodeList(evioDOMNodeP pNode, evioDOMNodeList *pList, Predicate pred)
    throw(evioException);
  
  void toOstream(ostream &os, const evioDOMNodeP node, int depth) const throw(evioException);
  void *containerNodeHandler(int length, uint16_t tag, int contentType, uint8_t num, int depth, void *userArg);
  void *leafNodeHandler(int length, uint16_t tag, int contentType, uint8_t num, int depth, const void *data, void *userArg);


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
  virtual ~evioSerializable(void) {};
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


// include templates
#include "evioUtilTemplates.hxx"



}  // namespace evio


#endif


