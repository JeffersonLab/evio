/*
 *  evioDictionary.cc
 *
 *   Author:  Elliott Wolin, JLab, 13-Apr-2012
*/


#include "evioDictionary.hxx"


using namespace std;
using namespace evio;


//-----------------------------------------------------------------------
//-----------------------------------------------------------------------


/**
 * No-arg constructor contains empty maps.
 */
evioDictionary::evioDictionary() {
}


//-----------------------------------------------------------------------


/**
 * Constructor fills dictionary maps from string.
 * @param dictionaryXML XML string parsed to create dictionary maps
 */
evioDictionary::evioDictionary(const string &dictXML, const string &dictTag) 
  : dictionaryXML(dictXML), dictTag(dictTag) {
  parseDictionary(dictionaryXML);
}


//-----------------------------------------------------------------------

/**
 * Destructor.
 */
evioDictionary::~evioDictionary() {
}


//-----------------------------------------------------------------------


/**
 * Sets dictionary tag
 * @param tag Dictionary tag
 */
void evioDictionary::setDictTag(const string &tag) {
  dictTag=tag;
}


//-----------------------------------------------------------------------


/**
 * Gets dictionary tag
 * @return dictionary tag
 */
string evioDictionary::getDictTag(void) const {
  return(dictTag);
}


//-----------------------------------------------------------------------


/**
 * Gets dictionary XML
 * @return dictionary XML
 */
string evioDictionary::getDictionaryXML(void) const {
  return(dictionaryXML);
}


//-----------------------------------------------------------------------


/**
 * Uses Expat to parse XML dictionary string and fill maps.
 * @param dictionaryXML XML string
 * @return True if parsing succeeded
 */
bool evioDictionary::parseDictionary(const string &dictionaryXML) {

  // init string parser and start element handler
  XML_Parser xmlParser = XML_ParserCreate(NULL);
  XML_SetElementHandler(xmlParser,startElementHandler,NULL);
  XML_SetUserData(xmlParser,reinterpret_cast<void*>(this));
      

  // parse XML dictionary
  bool ok = XML_Parse(xmlParser,dictionaryXML.c_str(),dictionaryXML.size(),true)!=0;
  if(!ok) {
    cerr << endl << "  ?evioDictionary::parseDictionary...parse error"
         << endl << endl << XML_ErrorString(XML_GetErrorCode(xmlParser));
  }
  XML_ParserFree(xmlParser);

  return(ok);
}


//-----------------------------------------------------------------------


/**
 * Expat start element handler, must be static.
 * @param userData void* pointer to evioDictionary
 * @param xmlname Name of current element
 * @param atts Array of attributes for this element
 */
void evioDictionary::startElementHandler(void *userData, const char *xmlname, const char **atts) {
  

  // userData points to dictionary 
  if(userData==NULL) {
    cerr << "?evioDictionary::startElement...NULL userData" << endl;
    return;
  }


  // get dictionary from user data
  evioDictionary *d = reinterpret_cast<evioDictionary*>(userData);



  // only process dictionary entries
  if( (strstr(xmlname,d->dictTag.c_str())==NULL) &&
      (strstr(xmlname,"dictEntry")==NULL)&&(strstr(xmlname,"DictEntry")==NULL)&&(strstr(xmlname,"dictentry")==NULL)
      ) return;
  

  string name = "";
  int tag = 0;
  int num = 0;
  for (int i=0; atts[i]; i+=2) {
    if(strcasecmp(atts[i],"name")==0) {
      name = string(atts[i+1]);
    } else if(strcasecmp(atts[i],"tag")==0) {
      tag = atoi(atts[i+1]);
    } else if(strcasecmp(atts[i],"num")==0) {
      num = atoi(atts[i+1]);
    }
  }

  // add tag/num pair and name to maps
  tagNum tn = tagNum(tag,num);
  d->getNameMap[tn]     = name;
  d->getTagNumMap[name] = tn;
}
    

//-----------------------------------------------------------------------


/**
 * Gets tagNum given name, throws exception if not found.
 * @param name Name of bank
 * @return tagNum
 */
tagNum evioDictionary::getTagNum(const string &name) const throw(evioException) {
  map<string,tagNum>::const_iterator iter = getTagNumMap.find(name);
  if(iter!=getTagNumMap.end()) {
    return((*iter).second);
  } else {
    throw(evioException(0,"?evioDictionary::getTagNum...no entry named "+name,__FILE__,__FUNCTION__,__LINE__));
  }
}



//-----------------------------------------------------------------------


/**
 * Gets name given tagNum, throws exception if not found.
 * @param tn tagNum of bank
 * @return name
 */
string evioDictionary::getName(tagNum tn) const throw(evioException) {
  map<tagNum,string>::const_iterator iter = getNameMap.find(tn);
  if(iter!=getNameMap.end()) {
    return((*iter).second);
  } else {
    ostringstream ss;
    ss << "?evioDictionary::getName...no entry with tagNum "<<  tn.first << "," << tn.second << ends;
    throw(evioException(0,ss.str(),__FILE__,__FUNCTION__,__LINE__));
  }
}


//-----------------------------------------------------------------------
//-----------------------------------------------------------------------

