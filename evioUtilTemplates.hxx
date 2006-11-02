// evioUtilTemplates.hxx

// template bodies for evio package, prototypes in evioUtil.hxx

//  ejw, 23-oct-2006





#ifndef _evioUtilTemplates_hxx
#define _evioUtilTemplates_hxx



//-----------------------------------------------------------------------------
//------------ evioDOMNode templated methods (not overridden) -----------------
//-----------------------------------------------------------------------------


template <typename T> const vector<T> *evioDOMNode::getContents(void) const throw(evioException*) {
  const evioDOMLeafNode<T> *l = dynamic_cast<const evioDOMLeafNode<T>*>(this);
  return((l==NULL)?(NULL):&l->data);
}


//-----------------------------------------------------------------------------
//--------- evioDOMContainerNode templated methods (not overridden) -----------
//-----------------------------------------------------------------------------


// template <typename T> evioDOMContainerNode& evioDOMContainerNode::operator<<(T t) throw(evioException*) {
//   evioDOMLeafNode<T> *leaf = new evioDOMLeafNode<T>(this,0,0,&t,1);
//   childList.push_back(leaf);
//   return(*this);
// }


// //-----------------------------------------------------------------------------


// template <typename T> evioDOMContainerNode& evioDOMContainerNode::operator<<(T *tP) throw(evioException*) {
//   evioDOMLeafNode<T> *leaf = new evioDOMLeafNode<T>(this,0,0,tP,streamArraySize);
//   childList.push_back(leaf);
//   return(*this);
// }


//-----------------------------------------------------------------------------


template <typename T> evioDOMContainerNode& evioDOMContainerNode::operator<<(const vector<T> &v) throw(evioException*) {
  evioDOMLeafNode<T> *leaf = new evioDOMLeafNode<T>(this,0,0,NULL,0);
  copy(v.begin(),v.end(),back_inserter(leaf->data));
  childList.push_back(leaf);
  return(*this);
}


//-----------------------------------------------------------------------------
//------------ evioDOMTree templated methods (not overridden) -----------------
//-----------------------------------------------------------------------------


template <typename Predicate> evioDOMNodeListP evioDOMTree::getNodeList(Predicate pred) const throw(evioException*) {
  evioDOMNodeList *pList = addToNodeList(root,new evioDOMNodeList,pred);
  return(evioDOMNodeListP(pList));
}  


//-----------------------------------------------------------------------------


template <typename Predicate> evioDOMNodeList *evioDOMTree::addToNodeList(const evioDOMNode *pNode, 
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


template <typename T> auto_ptr< list<const evioDOMLeafNode<T>*> > evioDOMTree::getLeafNodeList(void) const
  throw(evioException*) {

  evioDOMNodeListP pNodeList = getNodeList();
  auto_ptr< list<const evioDOMLeafNode<T>*> > pLeafList = 
    auto_ptr< list<const evioDOMLeafNode<T>*> >(new list<const evioDOMLeafNode<T>*>);
  
  evioDOMNodeList::const_iterator iter;
  const evioDOMLeafNode<T>* p;
  for(iter=pNodeList->begin(); iter!=pNodeList->end(); iter++) {
    p=dynamic_cast<const evioDOMLeafNode<T>*>(*iter);
    if(p!=NULL)pLeafList->push_back(p);
  }
  
  return(pLeafList);
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMTree::evioDOMTree(int tag, int num, const vector<T> &dataVec, const string &n)
  throw(evioException*) {

  name=n;
  root = new evioDOMLeafNode<T>(NULL,tag,num,dataVec);
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMTree::evioDOMTree(int tag, int num, const vector<T>* pDataVec, const string &n)
  throw(evioException*) {

  name=n;
  root = new evioDOMLeafNode<T>(NULL,tag,num,*dataVec);
}


//-----------------------------------------------------------------------------


template <typename T> evioDOMTree::evioDOMTree(int tag, int num, const T* dataBuf, int dataLen, const string &n)
  throw(evioException*) {

  name=n;
  root = new evioDOMLeafNode<T>(NULL,tag,num,dataBuf,dataLen);
}


//-----------------------------------------------------------------------------


template <typename T> void evioDOMTree::addBank(int tag, int num, const vector<T> dataVec) throw(evioException*) {
  evioDOMContainerNode* p = dynamic_cast<evioDOMContainerNode*>(root);
  if(p!=NULL) {
    p->childList.push_back(new evioDOMLeafNode<T>(root,tag,num,dataVec));
  } else {
    throw(new evioException(0,"?evioDOMTree::addBank...root node not container node",__FILE__,__LINE__));
  }
}


//-----------------------------------------------------------------------------


template <typename T> void evioDOMTree::addBank(int tag, int num, const T* dataBuf, int dataLen) throw(evioException*) {
  evioDOMContainerNode* p = dynamic_cast<evioDOMContainerNode*>(root);
  if(p!=NULL) {
    p->childList.push_back(new evioDOMLeafNode<T>(root,tag,num,dataBuf,dataLen));
  } else {
    throw(new evioException(0,"?evioDOMTree::addBank...root node not container node",__FILE__,__LINE__));
  }
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

#endif
