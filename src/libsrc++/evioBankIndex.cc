/*
 *  evioBankIndex.cc
 *
 *   Author:  Elliott Wolin, JLab, 1-May-2012
*/


#include "evioBankIndex.hxx"
#include "evioUtil.hxx"

using namespace evio;
using namespace std;



//--------------------------------------------------------------
//--------------------------------------------------------------


// used internally to stream-parse the event and fill the tagNum map
class myHandler : public evioStreamParserHandler {
  
  void *containerNodeHandler(int length, unsigned short tag, int contentType, unsigned char num, 
                             int depth, void *userArg) {
    return(userArg);
  }
  
  
  //--------------------------------------------------------------
  
  
  void *leafNodeHandler(int length, unsigned short tag, int contentType, unsigned char num, 
                        int depth, const void *data, void *userArg) {
    
    // adds bank index to map
    evioBankIndex *bi = static_cast<evioBankIndex*>(userArg);
    bi->tagNumMap.insert(bankIndexMap::value_type(tagNum(tag,num),boost::make_tuple(contentType,data,length)));
    return(userArg);
  }

};


//-----------------------------------------------------------------------
//-----------------------------------------------------------------------


/**
 * Constructor.
 */
evioBankIndex::evioBankIndex() {
}


//-----------------------------------------------------------------------


/**
 * Constructor from buffer.
 *
 * @param buffer Event buffer to index
 */
evioBankIndex::evioBankIndex(const uint32_t *buffer) {
  parseBuffer(buffer);
}


//-----------------------------------------------------------------------


/**
 * Destructor.
 */
evioBankIndex::~evioBankIndex() {
}


//-----------------------------------------------------------------------


/**
 * Indexes buffer and fills map.
 * @param buffer Buffer containing serialized event
 * @return true if indexing succeeded
 */
bool evioBankIndex::parseBuffer(const uint32_t *buffer) {

  // create parser and handler
  evioStreamParser p;
  myHandler h;
  p.parse(buffer,h,((void*)this));

  return(true);
}


//-----------------------------------------------------------------------


/**
 * True if tagNum is in map at least once.
 * @param tn tagNum
 * @return true if tagNum is in map
 */
bool evioBankIndex::tagNumExists(const tagNum& tn) const {
  bankIndexMap::const_iterator iter = tagNumMap.find(tn);
  return(iter!=tagNumMap.end());
}


//-----------------------------------------------------------------------


/**
 * Returns count of tagNum in map.
 * @param tn tagNum
 * @return Count of tagNum in map.
 */
int evioBankIndex::tagNumCount(const tagNum& tn) const {
  return((int)tagNumMap.count(tn));
}


//-----------------------------------------------------------------------


/**
 * Returns pair of iterators defining range of equal keys in tagNumMap.
 * @param tn tagNum
 * @return Pair of iterators defining range
 */
bankIndexRange evioBankIndex::getRange(const tagNum& tn) const {
  return(tagNumMap.equal_range(tn));
}


//-----------------------------------------------------------------------


/**
 * Returns bankIndex given tagNum, throws exception if no entry found
 * @param tn tagNum
 * @return bankIndex for tagNum
 */
bankIndex evioBankIndex::getBankIndex(const tagNum &tn) const throw(evioException) {

  bankIndexMap::const_iterator iter = tagNumMap.find(tn);
  if(iter!=tagNumMap.end()) {
    return((*iter).second);
  } else {
    throw(evioException(0,"?evioBankIndex::getBankIndex...tagNum not found",__FILE__,__FUNCTION__,__LINE__));
  }
}


//-----------------------------------------------------------------------
//-----------------------------------------------------------------------
