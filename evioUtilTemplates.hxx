// evioUtilTemplates.hxx

//  all the evio templates, some prototypes in evioUtil.hxx

//  ejw, 5-dec-2006



#ifndef _evioUtilTemplates_hxx
#define _evioUtilTemplates_hxx




//--------------------------------------------------------------
//----------------------- local class  -------------------------
//--------------------------------------------------------------


//  only way to do this on solaris...ejw, 9-jan-2007
template <typename T> class evioUtil {
public:
  static int getContentType(void) throw(evioException) {
    throw(evioException(0,"?evioUtil<T>::getContentType...illegal type",__FILE__,__LINE__));
    return(0);
  }
};

template <> class evioUtil<unsigned int>       {public: static int getContentType(void)  throw(evioException) {return(0x1);}};
template <> class evioUtil<float>              {public: static int getContentType(void)  throw(evioException) {return(0x2);}};
template <> class evioUtil<string&>            {public: static int getContentType(void)  throw(evioException) {return(0x3);}};
template <> class evioUtil<short>              {public: static int getContentType(void)  throw(evioException) {return(0x4);}};
template <> class evioUtil<unsigned short>     {public: static int getContentType(void)  throw(evioException) {return(0x5);}};
template <> class evioUtil<char>               {public: static int getContentType(void)  throw(evioException) {return(0x6);}};
template <> class evioUtil<unsigned char>      {public: static int getContentType(void)  throw(evioException) {return(0x7);}};
template <> class evioUtil<double>             {public: static int getContentType(void)  throw(evioException) {return(0x8);}};
template <> class evioUtil<long long>          {public: static int getContentType(void)  throw(evioException) {return(0x9);}};
template <> class evioUtil<unsigned long long> {public: static int getContentType(void)  throw(evioException) {return(0xa);}};
template <> class evioUtil<int>                {public: static int getContentType(void)  throw(evioException) {return(0xb);}};
  
template <> class evioUtil<unsigned long> {
public:
  static int getContentType(void) throw(evioException) {
    if(sizeof(unsigned long)==8) {
      return(0xa);
    } else {
      return(0x1);
    }
  }
};

template <> class evioUtil<long> { 
public:
  static int getContentType(void) throw(evioException) {
    if(sizeof(long)==8) {
      return(0x9);
    } else {
      return(0xb);
    }
  }
};


//-----------------------------------------------------------------------------
//--------------------- evioDOMNode templated methods -------------------------
//-----------------------------------------------------------------------------


template <typename T> evioDOMNodeP evioDOMNode::createEvioDOMNode(evioDOMNodeP parent, unsigned int tag, unsigned char num, const vector<T> tVec)
  throw(evioException) {
  return(new evioDOMLeafNode<T>(parent,tag,num,tVec));
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMNodeP evioDOMNode::createEvioDOMNode(unsigned int tag, unsigned char num, const vector<T> tVec) throw(evioException) {
  return(new evioDOMLeafNode<T>(NULL,tag,num,tVec));
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMNodeP evioDOMNode::createEvioDOMNode(unsigned int tag, unsigned char num, const T* t, int len) throw(evioException) {
  return(new evioDOMLeafNode<T>(NULL,tag,num,t,len));
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMNodeP evioDOMNode::createEvioDOMNode(evioDOMNodeP parent, unsigned int tag, unsigned char num, const T* t, int len) 
  throw(evioException) {
  return(new evioDOMLeafNode<T>(parent,tag,num,t,len));
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMNodeP evioDOMNode::createEvioDOMNode(unsigned int tag, unsigned char num, T *t, 
                                                                  void *userArg, ContainerType cType) throw(evioException) {
  return(evioDOMNode::createEvioDOMNode(NULL,tag,num,t,userArg,cType));
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMNodeP evioDOMNode::createEvioDOMNode(evioDOMNodeP parent, unsigned int tag, unsigned char num, T *t, 
                                                                  void *userArg, ContainerType cType) throw(evioException) {
  evioDOMContainerNode *c = new evioDOMContainerNode(parent,tag,num,cType);
  t->serialize(c,userArg);
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMNodeP evioDOMNode::createEvioDOMNode(unsigned int tag, unsigned char num, T *t, 
                                                                  void* T::*mfp(evioDOMNodeP c, void *userArg),
                                                                  void *userArg, ContainerType cType) throw(evioException) {
  return(evioDOMNode::createEvioDOMNode(NULL,tag,num,t,mfp,userArg,cType));
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMNodeP evioDOMNode::createEvioDOMNode(evioDOMNodeP parent, unsigned int tag, unsigned char num, T *t, 
                                                                  void* T::*mfp(evioDOMNodeP c, void *userArg),
                                                                  void *userArg, ContainerType cType) throw(evioException) {
  evioDOMContainerNode *c = new evioDOMContainerNode(parent,tag,num,cType);
  t->mfp(c,userArg);
}


//-----------------------------------------------------------------------------


// must be done this way because C++ forbids templated virtual functions...ejw, dec-2006
template <typename T> void evioDOMNode::append(const vector<T> &tVec) throw(evioException) {
  evioDOMLeafNode<T> *l = dynamic_cast<evioDOMLeafNode<T>*>(this);
  if(l!=NULL) {
    copy(tVec.begin(),tVec.end(),inserter(l->data,l->data.end()));
  } else {
    throw(evioException(0,"?evioDOMNode::append...not a leaf node",__FILE__,__LINE__));
  }
}


//-----------------------------------------------------------------------------


// must be done this way because C++ forbids templated virtual functions...ejw, dec-2006
template <typename T> void evioDOMNode::append(const T* tBuf, int len) throw(evioException) {
  evioDOMLeafNode<T> *l = dynamic_cast<evioDOMLeafNode<T>*>(this);
  if(l!=NULL) {
    for(int i=0; i<len; i++) l->data.push_back(tBuf[i]);
  } else {
    throw(evioException(0,"?evioDOMNode::append...not a leaf node",__FILE__,__LINE__));
  }
}


//-----------------------------------------------------------------------------


// must be done this way because C++ forbids templated virtual functions...ejw, dec-2006
template <typename T> void evioDOMNode::replace(const vector<T> &tVec) throw(evioException) {
  evioDOMLeafNode<T> *l = dynamic_cast<evioDOMLeafNode<T>*>(this);
  if(l!=NULL) {
    l->data.clear();
    copy(tVec.begin(),tVec.end(),inserter(l->data,l->data.begin()));
  } else {
    throw(evioException(0,"?evioDOMNode::replace...not a leaf node",__FILE__,__LINE__));
  }
}


//-----------------------------------------------------------------------------


// must be done this way because C++ forbids templated virtual functions...ejw, dec-2006
template <typename T> void evioDOMNode::replace(const T* tBuf, int len) throw(evioException) {
  evioDOMLeafNode<T> *l = dynamic_cast<evioDOMLeafNode<T>*>(this);
  if(l!=NULL) {
    l->data.clear();
    for(int i=0; i<len; i++) l->data.push_back(tBuf[i]);
  } else {
    throw(evioException(0,"?evioDOMNode::replace...not a leaf node",__FILE__,__LINE__));
  }
}


//-----------------------------------------------------------------------------


// must be done this way because C++ forbids templated virtual functions...ejw, dec-2006
template <typename T> vector<T> *evioDOMNode::getVector(void) throw(evioException) {
  if(!isLeaf())return(NULL);
  evioDOMLeafNode<T> *l = dynamic_cast<evioDOMLeafNode<T>*>(this);
  return((l==NULL)?NULL:(&l->data));
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMNode& evioDOMNode::operator<<(const vector<T> &tVec) throw(evioException) {
  append(tVec);
  return(*this);
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMNode& evioDOMNode::operator<<(T tVal) throw(evioException) {
  append(&tVal,1);
  return(*this);
}


//-----------------------------------------------------------------------------
//------------------ evioDOMContainerNode templated methods -------------------
//-----------------------------------------------------------------------------



//-----------------------------------------------------------------------------
//--------------------- evioDOMLeafNode templated methods ---------------------
//-----------------------------------------------------------------------------


template <typename T> evioDOMLeafNode<T>::evioDOMLeafNode(evioDOMNodeP par, unsigned int tg, unsigned char num, const vector<T> &v)
  throw(evioException) : evioDOMNode(par,tg,num,evioUtil<T>::getContentType()) {
  
  copy(v.begin(),v.end(),inserter(data,data.begin()));
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMLeafNode<T>::evioDOMLeafNode(evioDOMNodeP parent, unsigned int tg, unsigned char num, const T* p, int ndata) 
  throw(evioException) : evioDOMNode(parent,tg,num,evioUtil<T>::getContentType()) {
  
  // fill vector with data
  for(int i=0; i<ndata; i++) data.push_back(p[i]);
}


//-----------------------------------------------------------------------------


template <typename T> string evioDOMLeafNode<T>::getHeader(int depth) const {

  ostringstream os;
  string indent = getIndent(depth);
  string indent2 = indent + "    ";

  int wid,swid;
  switch (contentType) {

  case 0x0:
  case 0x1:
  case 0x2:
  case 0xb:
    wid=5;
    swid=10;
    break;
  case 0x4:
  case 0x5:
    wid=8;
    swid=6;
    break;
  case 0x6:
  case 0x7:
    wid=8;
    swid=4;
    break;
  case 0x8:
  case 0x9:
  case 0xa:
    wid=2;
    swid=28;
    break;
  default:
    wid=1;
   swid=30;
    break;
  }


  // dump header
  os << indent
     <<  "<" << get_typename(contentType) 
     << " data_type=\"" << hex << showbase << contentType
     << dec << "\" tag=\"" << tag;
  if((parent==NULL)||((parent->getContentType()==0xe)||(parent->getContentType()==0x10))) os << dec << "\" num=\"" << num;
  os << "\">" << endl;


  // dump data...odd what has to be done for char types 0x6,0x7 due to bugs in ostream operator <<
  int *j;
  short k;
  typename vector<T>::const_iterator iter;
  for(iter=data.begin(); iter!=data.end();) {

    if(contentType!=0x3)os << indent2;
    for(int j=0; (j<wid)&&(iter!=data.end()); j++) {
      switch (contentType) {

      case 0x0:
      case 0x1:
      case 0x5:
      case 0xa:
        os << hex << showbase << setw(swid) << *iter << "  ";
        break;
      case 0x2:
        os << setprecision(6) << showpoint << setw(swid) << *iter << "  ";
        break;
      case 0x3:
        os << "<!CDATA[" << endl << *iter << endl << "]]>";
        break;
      case 0x6:
        k = (*((short*)(&(*iter)))) & 0xff;
        if((k&0x80)!=0)k|=0xff00;
        os << setw(swid) << k << "  ";
        break;
      case 0x7:
        os << hex << showbase << setw(swid) << ((*(int*)&(*iter))&0xff) << "  ";
        break;
      case 0x8:
        os << setw(swid) << setprecision(20) << scientific << *iter << "  ";
        break;
      default:
        os << setw(swid) << *iter << "  ";
        break;
      }
      iter++;
    }
    os << dec << endl;

  }

  return(os.str());
}


//-----------------------------------------------------------------------------


template <typename T> string evioDOMLeafNode<T>::getFooter(int depth) const {
  ostringstream os;
  os << getIndent(depth) << "</" << get_typename(this->contentType) << ">" << endl;
  return(os.str());
}


//-----------------------------------------------------------------------------

                                   
template <typename T> string evioDOMLeafNode<T>::toString(void) const {
  ostringstream os;
  os << getHeader(0) << getFooter(0);
  return(os.str());
}


//-----------------------------------------------------------------------------
//----------------------- evioDOMTree templated methods -----------------------
//-----------------------------------------------------------------------------


template <class Predicate> evioDOMNodeListP evioDOMTree::getNodeList(Predicate pred) throw(evioException) {
  evioDOMNodeList *pList = addToNodeList(root, new evioDOMNodeList(), pred);
  return(evioDOMNodeListP(pList));
}  


//-----------------------------------------------------------------------------


template <class Predicate> evioDOMNodeList *evioDOMTree::addToNodeList(evioDOMNodeP pNode, evioDOMNodeList *pList, Predicate pred)
  throw(evioException) {

  if(pNode==NULL)return(pList);


  // add this node to list
  if(pred(pNode))pList->push_back(pNode);
  
  
  // add children to list
  evioDOMContainerNode *c = dynamic_cast<evioDOMContainerNode*>(pNode);
  if(c!=NULL) {
    evioDOMNodeList::iterator iter;
    for(iter=c->childList.begin(); iter!=c->childList.end(); iter++) {
      addToNodeList(*iter,pList,pred);
    }
  }


  // return the list
  return(pList);
}


//-----------------------------------------------------------------------------


template <typename T> void evioDOMTree::addBank(unsigned int tag, unsigned char num, const vector<T> dataVec) throw(evioException) {

  if(root==NULL) {
    root = evioDOMNode::createEvioDOMNode(tag,num,dataVec);
    root->parentTree=this;

  } else {
    evioDOMContainerNode* c = dynamic_cast<evioDOMContainerNode*>(root);
    if(c==NULL)throw(evioException(0,"?evioDOMTree::addBank...root not a container node",__FILE__,__LINE__));
    evioDOMNodeP node = evioDOMNode::createEvioDOMNode(tag,num,dataVec);
    c->childList.push_back(node);
    node->parent=root;
  }
}


//-----------------------------------------------------------------------------


template <typename T> void evioDOMTree::addBank(unsigned int tag, unsigned char num, const T* dataBuf, int dataLen) throw(evioException) {

  if(root==NULL) {
    root = evioDOMNode::createEvioDOMNode(tag,num,dataBuf,dataLen);
    root->parentTree=this;

  } else {
    evioDOMContainerNode* c = dynamic_cast<evioDOMContainerNode*>(root);
    if(c==NULL)throw(evioException(0,"?evioDOMTree::addBank...root not a container node",__FILE__,__LINE__));
    evioDOMNodeP node = evioDOMNode::createEvioDOMNode(tag,num,dataBuf,dataLen);
    c->childList.push_back(node);
    node->parent=root;
  }
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//--------------------- Misc Function Objects ---------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


template <typename T> class typeIs : unary_function<const evioDOMNodeP,bool> {

public:
  typeIs(void) : type(evioUtil<T>::getContentType()) {}
  bool operator()(const evioDOMNodeP node) const {return(node->getContentType()==type);}
private:
  int type;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class typeEquals : unary_function<const evioDOMNodeP,bool> {

public:
  typeEquals(int aType) : type(aType) {}
  bool operator()(const evioDOMNodeP node) const {return(node->getContentType()==type);}
private:
  int type;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class tagEquals : unary_function<const evioDOMNodeP,bool> {

public:
  tagEquals(unsigned int aTag) : tag(aTag) {}
  bool operator()(const evioDOMNodeP node) const {return(node->tag==tag);}
private:
  unsigned int tag;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class numEquals : unary_function<const evioDOMNodeP,bool> {

public:
  numEquals(unsigned char aNum) : num(aNum) {}
  bool operator()(const evioDOMNodeP node) const {return(node->num==num);}
private:
  unsigned char num;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class tagNumEquals : unary_function<const evioDOMNodeP, bool> {

public:
  tagNumEquals(unsigned int aTag, unsigned char aNum) : tag(aTag), num(aNum) {}
  bool operator()(const evioDOMNodeP node) const {return((node->tag==tag)&&(node->num==num));}
private:
  unsigned int tag;
  unsigned char num;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class parentTypeEquals : unary_function<const evioDOMNodeP, bool> {

public:
  parentTypeEquals(int aType) : type(aType) {}
  bool operator()(const evioDOMNodeP node) const {return((node->getParent()==NULL)?false:(node->getParent()->getContentType()==type));}
private:
  int type;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class parentTagEquals : unary_function<const evioDOMNodeP, bool> {

public:
  parentTagEquals(unsigned int aTag) : tag(aTag) {}
  bool operator()(const evioDOMNodeP node) const {return((node->getParent()==NULL)?false:(node->getParent()->tag==tag));}
private:
  unsigned int tag;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class parentNumEquals : unary_function<const evioDOMNodeP, bool> {

public:
  parentNumEquals(unsigned char aNum) : num(aNum) {}
  bool operator()(const evioDOMNodeP node) const {return((node->getParent()==NULL)?false:(node->getParent()->num==num));}
private:
  unsigned char num;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class parentTagNumEquals : unary_function<const evioDOMNodeP, bool> {

public:
  parentTagNumEquals(unsigned int aTag, unsigned char aNum) : tag(aTag), num(aNum) {}
  bool operator()(const evioDOMNodeP node) const {
    return((node->getParent()==NULL)?false:((node->getParent()->tag==tag)&&(node->getParent()->num==num)));}
private:
  unsigned int tag;
  unsigned char num;
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class isContainer : unary_function<const evioDOMNodeP,bool> {

public:
  isContainer(void) {}
  bool operator()(const evioDOMNodeP node) const {return(node->isContainer());}
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class isLeaf : unary_function<const evioDOMNodeP,bool> {

public:
  isLeaf(void) {}
  bool operator()(const evioDOMNodeP node) const {return(node->isLeaf());}
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


class toCout: public unary_function<const evioDOMNodeP, void> {

public:
  toCout(void) {}
  void operator()(const evioDOMNodeP node) const {cout << node->toString() << endl;}
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#endif
