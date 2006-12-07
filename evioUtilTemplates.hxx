// evioUtilTemplates.hxx

//  all the evio templates, some prototypes in evioUtil.hxx

//  ejw, 5-dec-2006



#ifndef _evioUtilTemplates_hxx
#define _evioUtilTemplates_hxx




//--------------------------------------------------------------
//--------------------- global functions -----------------------
//--------------------------------------------------------------


template <typename T> static int getContentType(void)           {return(0x0);}
template <> static int getContentType<unsigned int>(void)       {return(0x1);}
template <> static int getContentType<float>(void)              {return(0x2);}
template <> static int getContentType<string&>(void)            {return(0x3);}
template <> static int getContentType<short>(void)              {return(0x4);}
template <> static int getContentType<unsigned short>(void)     {return(0x5);}
template <> static int getContentType<char>(void)               {return(0x6);}
template <> static int getContentType<unsigned char>(void)      {return(0x7);}
template <> static int getContentType<double>(void)             {return(0x8);}
template <> static int getContentType<long long>(void)          {return(0x9);}
template <> static int getContentType<unsigned long long>(void) {return(0xa);}
template <> static int getContentType<int>(void)                {return(0xb);}

template <> static int getContentType<unsigned long>(void) {
  if(sizeof(unsigned long)==8) {
    return(0xa);
  } else {
    return(0x1);
  }
}
template <> static int getContentType<long>(void) {
  if(sizeof(long)==8) {
    return(0x9);
  } else {
    return(0xb);
  }
}


//-----------------------------------------------------------------------------


static string getIndent(int depth) {
  string s;
  for(int i=0; i<depth; i++) s+="   ";
  return(s);
}


//-----------------------------------------------------------------------------
//--------------------- evioDOMNode templated methods -------------------------
//-----------------------------------------------------------------------------


template <typename T> evioDOMNode* evioDOMNode::createEvioDOMNode(evioDOMNode *parent, int tag, int num, const vector<T> tVec)
  throw(evioException) {
  return(new evioDOMLeafNode<T>(parent,tag,num,tVec));
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMNode* evioDOMNode::createEvioDOMNode(int tag, int num, const vector<T> tVec) throw(evioException) {
  return(new evioDOMLeafNode<T>(tag,num,tVec));
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMNode* evioDOMNode::createEvioDOMNode(int tag, int num, const T* t, int len) throw(evioException) {
  return(new evioDOMLeafNode<T>(tag,num,t,len));
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMNode* evioDOMNode::createEvioDOMNode(evioDOMNode *parent, int tag, int num, const T* t, int len) 
  throw(evioException) {
  return(new evioDOMLeafNode<T>(parent,tag,num,t,len));
}


//-----------------------------------------------------------------------------


template <typename T> void evioDOMNode::append(const vector<T> &tVec) throw(evioException) {
  // must be done this way because C++ forbids templated virtual functions...ejw, dec-2006
  evioDOMLeafNode<T> *l = dynamic_cast<evioDOMLeafNode<T>*>(this);
  if(l!=NULL) {
    copy(tVec.begin(),tVec.end(),inserter(l->data,l->data.end()));
  } else {
    throw(evioException(0,"?evioDOMNode::append...not a leaf node",__FILE__,__LINE__));
  }
}


//-----------------------------------------------------------------------------


template <typename T> void evioDOMNode::append(T *tBuf, int len) throw(evioException) {
  // must be done this way because C++ forbids templated virtual functions...ejw, dec-2006
  evioDOMLeafNode<T> *l = dynamic_cast<evioDOMLeafNode<T>*>(this);
  if(l!=NULL) {
    for(int i=0; i<len; i++) l->data.push_back(tBuf[i]);
  } else {
    throw(evioException(0,"?evioDOMNode::append...not a leaf node",__FILE__,__LINE__));
  }
}


//-----------------------------------------------------------------------------


template <typename T> void evioDOMNode::replace(const vector<T> &tVec) throw(evioException) {
  // must be done this way because C++ forbids templated virtual functions...ejw, dec-2006
  evioDOMLeafNode<T> *l = dynamic_cast<evioDOMLeafNode<T>*>(this);
  if(l!=NULL) {
    l->data.clear();
    copy(tVec.begin(),tVec.end(),inserter(l->data,l->data.begin()));
  } else {
    throw(evioException(0,"?evioDOMNode::replace...not a leaf node",__FILE__,__LINE__));
  }
}


//-----------------------------------------------------------------------------


template <typename T> void evioDOMNode::replace(T *tBuf, int len) throw(evioException) {
  // must be done this way because C++ forbids templated virtual functions...ejw, dec-2006
  evioDOMLeafNode<T> *l = dynamic_cast<evioDOMLeafNode<T>*>(this);
  if(l!=NULL) {
    l->data.clear();
    for(int i=0; i<len; i++) l->data.push_back(tBuf[i]);
  } else {
    throw(evioException(0,"?evioDOMNode::replace...not a leaf node",__FILE__,__LINE__));
  }
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMNode& evioDOMNode::operator<<(vector<T> &tVec) throw(evioException) {
  append(tVec);
  return(*this);
}


//-----------------------------------------------------------------------------


template <typename T> const vector<T> *evioDOMNode::getVector(void) const throw(evioException) {
  // must be done this way because C++ forbids templated virtual functions...ejw, dec-2006
  const evioDOMLeafNode<T> *l = dynamic_cast<const evioDOMLeafNode<T>*>(this);
  if(l!=NULL) {
    return(&l->data);
  } else {
    throw(evioException(0,"?evioDOMNode::getVector...not a leaf node",__FILE__,__LINE__));
  }
}


//-----------------------------------------------------------------------------
//--------------------- evioDOMLeafNode templated methods ---------------------
//-----------------------------------------------------------------------------


template <typename T> evioDOMLeafNode<T>::evioDOMLeafNode(evioDOMNode *par, int tg, int num, const vector<T> &v)
  throw(evioException) : evioDOMNode(par,tg,num,getContentType<T>()) {
  
  copy(v.begin(),v.end(),inserter(data,data.begin()));
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMLeafNode<T>::evioDOMLeafNode(int tg, int num, const vector<T> &v)
  throw(evioException) : evioDOMNode(tg,num,getContentType<T>()) {

  copy(v.begin(),v.end(),inserter(data,data.begin()));
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMLeafNode<T>::evioDOMLeafNode(evioDOMNode *parent, int tg, int num, const T* p, int ndata) 
  throw(evioException) : evioDOMNode(parent,tg,num,getContentType<T>()) {
  
  // fill vector with data
  for(int i=0; i<ndata; i++) data.push_back(p[i]);
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMLeafNode<T>::evioDOMLeafNode(int tg, int num, const T* p, int ndata)
  throw(evioException) : evioDOMNode(tg,num,getContentType<T>()) {
  
  // fill vector with data
  for(int i=0; i<ndata; i++) data.push_back(p[i]);
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMLeafNode<T>::evioDOMLeafNode(const evioDOMLeafNode<T> &lNode)
  throw(evioException) : evioDOMNode(NULL,lNode.tag,lNode.num,lNode.getContentType<T>())  {

  copy(lNode.begin(),lNode.end(),inserter(data,data.begin()));
}


//-----------------------------------------------------------------------------

                                   
template <typename T> evioDOMLeafNode<T> *evioDOMLeafNode<T>::clone(evioDOMNode *newParent) const {
  return(new evioDOMLeafNode(newParent,tag,num,data));
}


//-----------------------------------------------------------------------------


// template <typename T> evioDOMLeafNode<T> &evioDOMLeafNode<T>::operator=(const evioDOMLeafNode<T> &rhs) 
//   throw(evioException) {

//   if(&rhs!=this) {
//     parent=rhs.parent;
//     tag=rhs.tag;
//     contentType=rhs.contentType;
//     num=rhs.num;
    
//     copy(rhs.data.begin(),rhs.data.end(),inserter(data,data.begin()));
//   }

//   return(*this);
// }


//-----------------------------------------------------------------------------

                                
template <typename T> evioDOMLeafNode<T>::~evioDOMLeafNode(void) {
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
  if((parent==NULL)||((parent->contentType==0xe)||(parent->contentType==0x10))) os << dec << "\" num=\"" << num;
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


template <class Predicate> evioDOMNodeListP evioDOMTree::getNodeList(Predicate pred) const throw(evioException) {
  evioDOMNodeList *pList = addToNodeList(root,new evioDOMNodeList(),pred);
  return(evioDOMNodeListP(pList));
}  


//-----------------------------------------------------------------------------


template <class Predicate> evioDOMNodeList *evioDOMTree::addToNodeList(const evioDOMNode *pNode, 
                                                                          evioDOMNodeList *pList, Predicate pred) const
  throw(evioException) {

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


template <typename T> void evioDOMTree::addBank(int tag, int num, const vector<T> dataVec) throw(evioException) {
  evioDOMContainerNode* p = dynamic_cast<evioDOMContainerNode*>(root);
  if(p!=NULL) {
    p->childList.push_back(evioDOMNode::createEvioDOMNode(root,tag,num,dataVec));
  } else {
    root = evioDOMNode::createEvioDOMNode(tag,num,dataVec);
  }
}


//-----------------------------------------------------------------------------


template <typename T> void evioDOMTree::addBank(int tag, int num, const T* dataBuf, int dataLen) throw(evioException) {
  evioDOMContainerNode* p = dynamic_cast<evioDOMContainerNode*>(root);
  if(p!=NULL) {
    p->childList.push_back(evioDOMNode::createEvioDOMNode(root,tag,num,dataBuf,dataLen));
  } else {
    root = evioDOMNode::createEvioDOMNode(tag,num,dataBuf,dataLen);
  }
}


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

#endif
