// evioDictionary.hxx

//  Author:  Elliott Wolin, JLab, 13-apr-2012



#ifndef _evioDictionary_hxx
#define _evioDictionary_hxx

#include <evioTypedefs.hxx>
#include <evioException.hxx>


namespace evio {

using namespace std;
using namespace evio;


/**
 * Parses XML dictionary string and holds two maps, one for each lookup direction.
 */
class evioDictionary {

public:
  evioDictionary();
  evioDictionary(const string &dictXML, const string &dictTag="");
  virtual ~evioDictionary();


public:
  void setDictTag(const string &tag);
  string getDictTag(void) const;
  bool parseDictionary(const string &dictionaryXML);
  tagNum getTagNum(const string &name) const throw(evioException);
  string getName(tagNum tn) const throw(evioException);
  string getDictionaryXML(void) const;


private:
  static void startElementHandler(void *userData, const char *xmlname, const char **atts);
  string dictionaryXML;


public:
  string dictTag;                    /**<User-supplied dictionary entry tag, default NULL.*/
  map<tagNum,string> getNameMap;     /**<Gets node name given tag/num.*/
  map<string,tagNum> getTagNumMap;   /**<Gets tag/num given node name.*/
};


}

#endif
  
