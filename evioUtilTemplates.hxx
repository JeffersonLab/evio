// evioUtilTemplates.hxx

// template bodies for evio package, prototypes in evioUtil.hxx

//  ejw, 23-oct-2006





#ifndef _evioUtilTemplates_hxx
#define _evioUtilTemplates_hxx



//-----------------------------------------------------------------------------
//--------------------- evioDOMNode templated methods -------------------------
//-----------------------------------------------------------------------------


template <typename T> evioDOMNode* evioDOMNode::createEvioDOMNode(evioDOMNode *parent, int tag, int num, vector<T>)
  throw(evioException*) {
  return(new evioDOMLeafNode<T>(parent,tag,num,tVec));
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMNode* evioDOMNode::createEvioDOMNode(int tag, int num, vector<T>) throw(evioException*) {
  return(new evioDOMLeafNode<T>(tag,num,tVec));
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMNode* evioDOMNode::createEvioDOMNode(int tag, int num, T* t, int len) throw(evioException*) {
  return(new evioDOMLeafNode<T>(tag,num,t,len));
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMNode* evioDOMNode::createEvioDOMNode(evioDOMNode *parent, int tag, int num, T* t, int len) throw(evioException*) {
  return(new evioDOMLeafNode<T>(parent,tag,num,t,len));
}


//-----------------------------------------------------------------------------


template <typename T> const vector<T> *evioDOMNode::getVector(void) const throw(evioException*) {
  throw(new evioException(0,"?evioDOMNode::getVector...illegal usage",__FILE__,__LINE__));
}


//-----------------------------------------------------------------------------
//--------------------- evioDOMLeafNode templated methods ---------------------
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


  // dump data...odd what has to be done for char types 0x6,0x7 due to bugs in ostream operator<<
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


template <typename T> const vector<T> *evioDOMLeafNode<T>::getVector(void) const throw(evioException*) {
  return(&this->data);
}


//-----------------------------------------------------------------------------
//----------------------- evioDOMTree templated methods -----------------------
//-----------------------------------------------------------------------------


template <class Predicate> evioDOMNodeListP evioDOMTree::getNodeList(Predicate pred) const throw(evioException*) {
  evioDOMNodeList *pList = addToNodeList(root,new evioDOMNodeList(),pred);
  return(evioDOMNodeListP(pList));
}  


//-----------------------------------------------------------------------------


template <class Predicate> evioDOMNodeList *evioDOMTree::addToNodeList(const evioDOMNode *pNode, 
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


template <typename T> void evioDOMTree::addNode(int tag, int num, const vector<T> dataVec) throw(evioException*) {
  evioDOMContainerNode* p = dynamic_cast<evioDOMContainerNode*>(root);
  if(p!=NULL) {
    p->childList.push_back(new evioDOMLeafNode<T>(root,tag,num,dataVec));
  } else {
    throw(new evioException(0,"?evioDOMTree::addNode...root node not container node",__FILE__,__LINE__));
  }
}


//-----------------------------------------------------------------------------


template <typename T> void evioDOMTree::addNode(int tag, int num, const T* dataBuf, int dataLen) throw(evioException*) {
  evioDOMContainerNode* p = dynamic_cast<evioDOMContainerNode*>(root);
  if(p!=NULL) {
    p->childList.push_back(new evioDOMLeafNode<T>(root,tag,num,dataBuf,dataLen));
  } else {
    throw(new evioException(0,"?evioDOMTree::addNode...root node not container node",__FILE__,__LINE__));
  }
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#endif
