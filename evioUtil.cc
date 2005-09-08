/*
 *  evioUtil.cc
 *
 *   Assorted evio utilities, C++ version
 *
 *   Author:  Elliott Wolin, JLab, 31-aug-2005
*/


#include <evioUtil.hxx>


// misc
static bool debug = true;


//--------------------------------------------------------------
//--------------------------------------------------------------


evioException::evioException() {
  type=0;
  text="";
}


//--------------------------------------------------------------


evioException::evioException(int t, string s) {
  type=t;
  text=s;
}


//--------------------------------------------------------------


void evioException::setType(int t) {
  type=t;
}


//--------------------------------------------------------------


int evioException::getType(void) const {
  return(type);
}


//--------------------------------------------------------------


void evioException::setText(string t) {
  text=t;
}


//--------------------------------------------------------------


string evioException::getText(void) const {
  return(text);
}


//--------------------------------------------------------------


string evioException::toString(void) const {
  ostringstream s;
  s << type;
  return(string("evioException type=") + s.str() + string(",  text:  ") + text);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


void *evioStreamParser::parse(const unsigned long *buf, 
                              evioStreamParserHandler &handler, void *userArg) throw(evioException*) {
  
  if(buf==NULL)throw(new evioException(0,"?evioStreamParser::parse...null buffer"));

  void *newUserArg = parseBank(buf,BANK,0,handler,userArg);
  return(newUserArg);
}


//--------------------------------------------------------------


void *evioStreamParser::parseBank(const unsigned long *buf, int nodeType, int depth, 
                                 evioStreamParserHandler &handler, void *userArg) throw(evioException*) {

  int length,tag,contentType,num,dataOffset,p,bankLen;
  void *newUserArg = userArg;
  const unsigned long *data;


  /* get type-dependent info */
  switch(nodeType) {
  case 0:
    length  	= buf[0]+1;
    tag     	= buf[1]>>16;
    contentType	= (buf[1]>>8)&0xff;
    num     	= buf[1]&0xff;
    dataOffset  = 2;
    break;

  case 1:
    length  	= (buf[0]&0xffff)+1;
    contentType = (buf[0]>>16)&0xff;
    tag     	= (buf[0]>>24)&0xff;
    num     	= 0;
    dataOffset  = 1;
    break;
    
  case 2:
    length  	= (buf[0]&0xffff)+1;
    contentType	= (buf[0]>>16)&0xf;
    tag     	= (buf[0]>>20)&0xfff;
    num     	= 0;
    dataOffset  = 1;
    break;

  default:
    ostringstream ss;
    ss << nodeType;
    throw(new evioException(1,"?parseBank...illegal node type: " + ss.str()));
  }


  /* 
   * if a leaf node, call leaf handler.
   * if container node, call node handler and then parse contained banks.
   */
  switch (contentType) {

  case 0x0:
  case 0x1:
  case 0x2:
  case 0xb:
    handler.leafHandler(length-dataOffset,nodeType,tag,contentType,num,depth,
                        &buf[dataOffset],userArg);
    break;

  case 0x3:
  case 0x6:
  case 0x7:
    handler.leafHandler((length-dataOffset)*4,nodeType,tag,contentType,num,depth,
                        (char*)(&buf[dataOffset]),userArg);
    break;

  case 0x4:
  case 0x5:
    handler.leafHandler((length-dataOffset)*2,nodeType,tag,contentType,num,depth,
                        (short*)(&buf[dataOffset]),userArg);
    break;

  case 0x8:
  case 0x9:
  case 0xa:
    handler.leafHandler((length-dataOffset)/2,nodeType,tag,contentType,num,
                        depth,(long long*)(&buf[dataOffset]),userArg);
    break;

  case 0xe:
  case 0x10:
  case 0xd:
  case 0x20:
  case 0xc:
  case 0x40:
    // call node handler and get new userArg
    newUserArg=handler.nodeHandler(length,nodeType,tag,contentType,num,depth,userArg);


    // parse contained banks
    depth++;
    p       = 0;
    bankLen = length-dataOffset;
    data    = &buf[dataOffset];

    switch (contentType) {

    case 0xe:
    case 0x10:
      while(p<bankLen) {
        parseBank(&data[p],BANK,depth,handler,newUserArg);
        p+=data[p]+1;
      }
      break;

    case 0xd:
    case 0x20:
      while(p<bankLen) {
        parseBank(&data[p],SEGMENT,depth,handler,newUserArg);
        p+=(data[p]&0xffff)+1;
      }
      break;

    case 0xc:
    case 0x40:
      while(p<bankLen) {
        parseBank(&data[p],TAGSEGMENT,depth,handler,newUserArg);
        p+=(data[p]&0xffff)+1;
      }
      break;

    }

    depth--;
    break;
  }

  return(newUserArg);
}


//--------------------------------------------------------------
//--------------------------------------------------------------


evioDOMTree::evioDOMTree(evioDOMNode *r) throw (evioException*) {

  if(r==NULL)throw(new evioException(0,"?evioDOMTree constructor...null evioDOMNode"));
  root = r;
}


//-----------------------------------------------------------------------------


evioDOMTree::evioDOMTree(const unsigned long *buf) throw (evioException*) {

  if(buf==NULL)throw(new evioException(0,"?evioDOMTree constructor...null buffer"));
  root=parse(buf);
}


//-----------------------------------------------------------------------------


evioDOMTree::~evioDOMTree(void) {
  delete(root);
  if(debug)cout << "tree deleted" << endl;
}


//-----------------------------------------------------------------------------


evioDOMNode *evioDOMTree::parse(const unsigned long *buf) throw(evioException*) {
  evioStreamParser p;
  return((evioDOMNode*)p.parse(buf,*this,NULL));
}


//-----------------------------------------------------------------------------


void *evioDOMTree::nodeHandler(int length, int containerType, int tag, int type, int num, int depth, 
                               void *userArg) {
    
  if(debug) printf("node   depth %2d  containerType %3d   type,tag,num,length:  %6d %6d %6d %6d\n",
                   depth,containerType,type,tag,num,length);
    
    
  // get parent pointer
  evioDOMContainerNode *parent = (evioDOMContainerNode*)userArg;
    

  // create new node
  evioDOMContainerNode *newNode = new evioDOMContainerNode(containerType,tag,type,num);


  // add new node to parent's list
  if(parent!=NULL)parent->childList.push_back(newNode);


  // return new userArg that points to the new node
  return((void*)newNode);
}
  
  
//-----------------------------------------------------------------------------


void evioDOMTree::leafHandler(int length, int containerType, int tag, int type, int num, int depth, 
                              const void *data, void *userArg) {

  if(debug) printf("leaf   depth %2d  containerType %3d   type,tag,num,length:  %6d %6d %6d %6d\n",
                   depth,containerType,type,tag,num,length);


  // get parent pointer
  evioDOMContainerNode *parent = (evioDOMContainerNode*)userArg;


  // create and fill new leaf
  evioDOMNode *newLeaf;
  string s;
  ostringstream os;
  switch (type) {

  case 0x0:
    newLeaf = new evioDOMLeafNode<unsigned long>(containerType,tag,type,num,(unsigned long *)data,length);
    break;
      
  case 0x1:
    newLeaf = new evioDOMLeafNode<unsigned long>(containerType,tag,type,num,(unsigned long*)data,length);
    break;
      
  case 0x2:
    newLeaf = new evioDOMLeafNode<float>(containerType,tag,type,num,(float*)data,length);
    break;
      
  case 0x3:
    for(int i=0; i<length; i++) os << ((char *)data)[i];
    os << ends;
    s=os.str();
    newLeaf = new evioDOMLeafNode<string>(containerType,tag,type,num,&s,1);
    break;

  case 0x4:
    newLeaf = new evioDOMLeafNode<short>(containerType,tag,type,num,(short*)data,length);
    break;

  case 0x5:
    newLeaf = new evioDOMLeafNode<unsigned short>(containerType,tag,type,num,(unsigned short*)data,length);
    break;

  case 0x6:
    newLeaf = new evioDOMLeafNode<char>(containerType,tag,type,num,(char*)data,length);
    break;

  case 0x7:
    newLeaf = new evioDOMLeafNode<unsigned char>(containerType,tag,type,num,(unsigned char*)data,length);
    break;

  case 0x8:
    newLeaf = new evioDOMLeafNode<double>(containerType,tag,type,num,(double*)data,length);
    break;

  case 0x9:
    newLeaf = new evioDOMLeafNode<long long>(containerType,tag,type,num,(long long*)data,length);
    break;

  case 0xa:
    newLeaf = new evioDOMLeafNode<unsigned long long>(containerType,tag,type,num,(unsigned long long*)data,length);
    break;

  case 0xb:
    newLeaf = new evioDOMLeafNode<long>(containerType,tag,type,num,(long*)data,length);
    break;
  }


  // add new leaf to parent list
  if(parent!=NULL)parent->childList.push_back(newLeaf);
}


//-----------------------------------------------------------------------------


string evioDOMTree::toString() const throw(evioException*) {

  if(root==NULL)return("");

  ostringstream os;
  toOstream(os,root,0);
  os << endl << endl;
  return(os.str());
}


//-----------------------------------------------------------------------------


void evioDOMTree::toOstream(ostream &os, const evioDOMNode *pNode, int depth) const throw(evioException*) {

  
  if(pNode==NULL)return;

  string indent;
  for(int i=0; i<depth; i++) indent +="    ";


  // dump node opening information into ostream
  pNode->toString(os,depth);


  // dump contained banks if node is a container
  const evioDOMContainerNode *c = dynamic_cast<const evioDOMContainerNode*>(pNode);
  if(c!=NULL) {
    list<evioDOMNode*>::const_iterator iter;
    for (iter=c->childList.begin(); iter!=c->childList.end(); iter++) {
      toOstream(os,*iter,depth+1);
    }
  }

  // node closing information
  os << indent << "</" << get_typename(pNode->contentType) << ">" << endl;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


evioDOMContainerNode::evioDOMContainerNode(int container, int tg, int content, int n)
  throw(evioException*) {

  containerType = container;
  tag           = tg;
  contentType   = content;
  num           = n;
  
}


//-----------------------------------------------------------------------------


string evioDOMContainerNode::toString(void) const {

  // just dump header
  ostringstream os;
  toString(os,0);
  return(os.str());
}


//-----------------------------------------------------------------------------

                                   
void evioDOMContainerNode::toString(ostream &os, int depth) const {

  string indent;
  for(int i=0; i<depth; i++) indent +="    ";

  os << indent
     <<  "<" << get_typename(contentType) << " tag=\'"  << tag << "\' contentType=\'" << contentType << "\' num=\'" << num << "\">" << endl;
}


//-----------------------------------------------------------------------------

                                   
evioDOMContainerNode::~evioDOMContainerNode(void) {
  if(debug)cout << "deleting node" << endl;
  list<evioDOMNode*>::const_iterator iter;
  for (iter=childList.begin(); iter!=childList.end(); iter++) {
    if(debug)cout << "deleting child" << endl;
    delete(*iter);
  }
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

                                   
template <class T> evioDOMLeafNode<T>::evioDOMLeafNode(int container, int tg, int content, int n,
                                                       T *p, int ndata) throw(evioException*) {
  
  containerType = container;
  tag           = tg;
  contentType   = content;
  num           = n;

  // fill vector with data
  for(int i=0; i<ndata; i++) data.push_back(p[i]);

}


//-----------------------------------------------------------------------------


template <class T> string evioDOMLeafNode<T>::toString(void) const {

  ostringstream os;
  toString(os,0);
  return(os.str());
}


//-----------------------------------------------------------------------------

                                   
template <class T> void evioDOMLeafNode<T>::toString(ostream &os, int depth) const {

  string indent,indent2;
  for(int i=0; i<depth; i++) indent +="    ";
  indent2 = indent + "    ";

  int wid;
  switch (contentType) {
  case 0x0:
  case 0x1:
  case 0x2:
  case 0xb:
    wid=5;
    break;
  case 0x4:
  case 0x5:
  case 0x6:
  case 0x7:
    wid=8;
    break;
  case 0x8:
  case 0x9:
  case 0xa:
    wid=2;
    break;
  default:
    wid=1;
    break;
  }


  // dump header
  os << indent
     <<  "<" << get_typename(contentType) <<  " tag=\'" << tag << "\' contentType=\'" << contentType 
     << "\' num=\'" << num << "\'>" << endl;


  // dump data (odd what has to be done for type 0x7 or unsigned char...)
  T i;
  int *j;
  typename vector<T>::const_iterator iter;
  for (iter=data.begin(); iter!=data.end();) {

    if(contentType!=0x3)os << indent2;
    for (int j=0; (j<wid)&&(iter!=data.end()); j++) {
      switch (contentType) {
      case 0x0:
      case 0x1:
      case 0x5:
      case 0xa:
        os << "0x" << hex << *iter << "  ";
        break;
      case 0x3:
        os << "<!CDATA[" << endl << *iter << endl << "]]>";
        break;
      case 0x6:  // ??? not sure what to do for signed byte
        i=*iter;
        os << ((*(int*)&i)&0xff) << "  ";
        break;
      case 0x7:
        i=*iter;
        os << "0x" << hex << ((*(int*)&i)&0xff) << "  ";
        break;
      case 0x2:
        os << setprecision(6) << fixed << *iter << "  ";
        break;
      case 0x8:
        os << setw(28) << setprecision(20) << scientific << *iter << "  ";
        break;
      default:
        os << *iter << "  ";
        break;
      }
      iter++;
    }
    os << dec << endl;

  }

  return;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
