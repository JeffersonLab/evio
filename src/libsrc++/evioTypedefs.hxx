#ifndef _evioTypedefs_hxx
#define _evioTypedefs_hxx


#include <string>
#include <list>
#include <map>
#include <memory>
#include <algorithm>


#include <iostream>
#include <iomanip>

#include <evio.h>


/**
 * Old "tagNum" typedef for pair<uint16_t,uint8_t> has changed to be a
 * #define for evioDictEntry since that is now used instead of tagNum.
 * This is a hack in order to help backwards compatibility.
 */
#define tagNum evioDictEntry


//-----------------------------------------------------------------------------
//----------------------------- Typedefs --------------------------------------
//-----------------------------------------------------------------------------

namespace evio {

using namespace std;
using namespace evio;


// evio classes
class evioStreamParserHandler;
class evioStreamParser;
class evioDOMTree;
class evioDOMNode;
class evioDOMContainerNode;
template<typename T> class evioDOMLeafNode;
class evioSerializable;
template <typename T> class evioUtil;
class evioDictionary;
class evioToStringConfig;



typedef evioDOMTree* evioDOMTreeP;                   /**<Pointer to evioDOMTree.*/
typedef evioDOMNode* evioDOMNodeP;                   /**<Pointer to evioDOMNode, only way to access nodes.*/
typedef list<evioDOMNodeP>  evioDOMNodeList;         /**<List of pointers to evioDOMNode.*/
typedef unique_ptr<evioDOMNodeList> evioDOMNodeListP;  /**<unique-ptr of list of evioDOMNode pointers, returned by getNodeList.*/


// should lists contain shared pointers?
//typedef shared_ptr<evioDOMNode> evioDOMNodeP;          /** Node pointer.*/
//typedef shared_ptr<evioDOMTree> evioDOMTreeP;          /** Tree pointer.*/
//typedef shared_ptr<evioDOMNodeList> evioDOMNodeListP;  /** List of node pointers.*/


/** Defines the container bank types.*/
enum ContainerType {
  BANK       = 0xe,  /**<2-word header, 16-bit tag, 8-bit num, 8-bit type.*/
  SEGMENT    = 0xd,  /**<1-word header,  8-bit tag,    no num, 8-bit type.*/
  TAGSEGMENT = 0xc   /**<1-word header, 12-bit tag,    no num, 4-bit type.*/
};

}

#endif
