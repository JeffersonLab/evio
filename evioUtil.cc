/*
 *  evioUtil.cc
 *
 *   Author:  Elliott Wolin, JLab, 11-dec-2006
*/


#include <evioUtil.hxx>

using namespace evio;
using namespace std;



//--------------------------------------------------------------
//-------------------- local utilities -------------------------
//--------------------------------------------------------------


namespace evio {
  static bool isTrue(const evioDOMNodeP pNode) {
    return(true);
  }
}


//-----------------------------------------------------------------------
//-------------------------- evioException ------------------------------
//-----------------------------------------------------------------------


evioException::evioException(int typ, const string &txt, const string &aux) 
  : type(typ), text(txt), auxText(aux) {
}


//--------------------------------------------------------------


evioException::evioException(int typ, const string &txt, const string &file, int line) 
  : type(typ), text(txt) {

  ostringstream oss;
  oss <<  "    evioException occured in file " << file << ", line " << line << ends;
  auxText=oss.str();
}


//--------------------------------------------------------------


string evioException::toString(void) const {
  ostringstream oss;
  oss << "?evioException type = " << type << "    text = " << text << endl << endl << auxText << endl;
  return(oss.str());
}


//-----------------------------------------------------------------------
//------------------------- evioFileChannel -----------------------------
//-----------------------------------------------------------------------


evioFileChannel::evioFileChannel(const string &f, const string &m, int size) throw(evioException) 
  : filename(f), mode(m), bufSize(size), buf(NULL), handle(0) {

  // allocate buffer
  buf = new unsigned int[bufSize];
  if(buf==NULL)throw(evioException(0,"?evioFileChannel constructor...unable to allocate buffer",__FILE__,__LINE__));
}


//-----------------------------------------------------------------------


evioFileChannel::~evioFileChannel(void) {
  if(handle!=0)close();
  if(buf!=NULL)delete(buf);
}


//-----------------------------------------------------------------------



void evioFileChannel::open(void) throw(evioException) {

  if(buf==NULL)throw(evioException(0,"evioFileChannel::open...null buffer",__FILE__,__LINE__));
  if(evOpen(const_cast<char*>(filename.c_str()),const_cast<char*>(mode.c_str()),&handle)<0)
    throw(evioException(0,"?evioFileChannel::open...unable to open file",__FILE__,__LINE__));
  if(handle==0)throw(evioException(0,"?evioFileChannel::open...zero handle",__FILE__,__LINE__));
}


//-----------------------------------------------------------------------


bool evioFileChannel::read(void) throw(evioException) {
  if(buf==NULL)throw(evioException(0,"evioFileChannel::read...null buffer",__FILE__,__LINE__));
  if(handle==0)throw(evioException(0,"evioFileChannel::read...0 handle",__FILE__,__LINE__));
  return(evRead(handle,&buf[0],bufSize)==0);
}


//-----------------------------------------------------------------------


void evioFileChannel::write(void) throw(evioException) {
  if(buf==NULL)throw(evioException(0,"evioFileChannel::write...null buffer",__FILE__,__LINE__));
  if(handle==0)throw(evioException(0,"evioFileChannel::write...0 handle",__FILE__,__LINE__));
  if(evWrite(handle,buf)!=0) throw(evioException(0,"?evioFileChannel::write...unable to write",__FILE__,__LINE__));
}


//-----------------------------------------------------------------------


void evioFileChannel::write(const unsigned int *myBuf) throw(evioException) {
  if(myBuf==NULL)throw(evioException(0,"evioFileChannel::write...null myBuf",__FILE__,__LINE__));
  if(handle==0)throw(evioException(0,"evioFileChannel::write...0 handle",__FILE__,__LINE__));
  if(evWrite(handle,myBuf)!=0) throw(evioException(0,"?evioFileChannel::write...unable to write from myBuf",__FILE__,__LINE__));
}


//-----------------------------------------------------------------------


void evioFileChannel::write(const unsigned long *myBuf) throw(evioException) {
  write(reinterpret_cast<const unsigned int*>(myBuf));
}


//-----------------------------------------------------------------------


void evioFileChannel::write(const evioChannel &channel) throw(evioException) {
  if(handle==0)throw(evioException(0,"evioFileChannel::write...0 handle",__FILE__,__LINE__));
  if(evWrite(handle,channel.getBuffer())!=0) throw(evioException(0,"?evioFileChannel::write...unable to write from channel",__FILE__,__LINE__));
}


//-----------------------------------------------------------------------


void evioFileChannel::write(const evioChannel *channel) throw(evioException) {
  if(channel==NULL)throw(evioException(0,"evioFileChannel::write...null channel",__FILE__,__LINE__));
  evioFileChannel::write(*channel);
}


//-----------------------------------------------------------------------


void evioFileChannel::write(const evioDOMTree &tree) throw(evioException) {
  if(handle==0)throw(evioException(0,"evioFileChannel::write...0 handle",__FILE__,__LINE__));
  tree.toEVIOBuffer(buf,bufSize);
  evioFileChannel::write();
}


//-----------------------------------------------------------------------


void evioFileChannel::write(const evioDOMTree *tree) throw(evioException) {
  if(tree==NULL)throw(evioException(0,"evioFileChannel::write...null tree",__FILE__,__LINE__));
  evioFileChannel::write(*tree);
}


//-----------------------------------------------------------------------


void evioFileChannel::ioctl(const string &request, void *argp) throw(evioException) {
  if(handle==0)throw(evioException(0,"evioFileChannel::ioctl...0 handle",__FILE__,__LINE__));
  if(evIoctl(handle,const_cast<char*>(request.c_str()),argp)!=0)
    throw(evioException(0,"?evioFileChannel::ioCtl...error return",__FILE__,__LINE__));
}


//-----------------------------------------------------------------------


void evioFileChannel::close(void) throw(evioException) {
  if(handle==0)throw(evioException(0,"evioFileChannel::close...0 handle",__FILE__,__LINE__));
  evClose(handle);
  handle=0;
}


//-----------------------------------------------------------------------


string evioFileChannel::getFileName(void) const {
  return(filename);
}


//-----------------------------------------------------------------------


string evioFileChannel::getMode(void) const {
  return(mode);
}


//-----------------------------------------------------------------------


const unsigned int *evioFileChannel::getBuffer(void) const throw(evioException) {
  if(buf==NULL)throw(evioException(0,"evioFileChannel::getbuffer...null buffer",__FILE__,__LINE__));
  return(buf);
}


//-----------------------------------------------------------------------



int evioFileChannel::getBufSize(void) const {
  return(bufSize);
}


//-----------------------------------------------------------------------
//------------------------ evioStreamParser -----------------------------
//-----------------------------------------------------------------------


void *evioStreamParser::parse(const unsigned int *buf, 
                              evioStreamParserHandler &handler, void *userArg) throw(evioException) {
  
  if(buf==NULL)throw(evioException(0,"?evioStreamParser::parse...null buffer",__FILE__,__LINE__));

  return((void*)parseBank(buf,BANK,0,handler,userArg));
}


//--------------------------------------------------------------


void *evioStreamParser::parseBank(const unsigned int *buf, int bankType, int depth, 
                                 evioStreamParserHandler &handler, void *userArg) throw(evioException) {

  int length,contentType,dataOffset,p,bankLen;
  unsigned short tag;
  unsigned char num;
  const unsigned int *data;
  unsigned int mask;

  void *newUserArg = userArg;


  /* get type-dependent info */
  switch(bankType) {

  case 0xe:
  case 0x10:
    length  	= buf[0]+1;
    tag     	= buf[1]>>16;
    contentType	= (buf[1]>>8)&0xff;
    num     	= buf[1]&0xff;
    dataOffset  = 2;
    break;

  case 0xd:
  case 0x20:
    length  	= (buf[0]&0xffff)+1;
    tag     	= buf[0]>>24;
    contentType = (buf[0]>>16)&0xff;
    num     	= 0;
    dataOffset  = 1;
    break;
    
  case 0xc:
  case 0x40:
    length  	= (buf[0]&0xffff)+1;
    tag     	= buf[0]>>20;
    contentType	= (buf[0]>>16)&0xf;
    num     	= 0;
    dataOffset  = 1;
    break;

  default:
    ostringstream ss;
    ss << hex << showbase << bankType << ends;
    throw(evioException(0,"?evioStreamParser::parseBank...illegal bank type: " + ss.str(),__FILE__,__LINE__));
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
    // four-byte types
    handler.leafNodeHandler(length-dataOffset,tag,contentType,num,depth,&buf[dataOffset],userArg);
    break;

  case 0x3:
  case 0x6:
  case 0x7:
    // one-byte types
    handler.leafNodeHandler((length-dataOffset)*4,tag,contentType,num,depth,(char*)(&buf[dataOffset]),userArg);
    break;

  case 0x4:
  case 0x5:
    // two-byte types
    handler.leafNodeHandler((length-dataOffset)*2,tag,contentType,num,depth,(short*)(&buf[dataOffset]),userArg);
    break;

  case 0x8:
  case 0x9:
  case 0xa:
    // eight-byte types
    handler.leafNodeHandler((length-dataOffset)/2,tag,contentType,num,depth,(long long*)(&buf[dataOffset]),userArg);
    break;

  case 0xe:
  case 0x10:
  case 0xd:
  case 0x20:
  case 0xc:
  case 0x40:
    // container types
    newUserArg=handler.containerNodeHandler(length,tag,contentType,num,depth,userArg);


    // parse contained banks
    p       = 0;
    bankLen = length-dataOffset;
    data    = &buf[dataOffset];
    mask    = ((contentType==0xe)||(contentType==0x10))?0xffffffff:0xffff;

    depth++;
    while(p<bankLen) {
      parseBank(&data[p],contentType,depth,handler,newUserArg);
      p+=(data[p]&mask)+1;
    }
    depth--;

    break;


  default:
    ostringstream ss;
    ss << hex << showbase << contentType << ends;
    throw(evioException(0,"?evioStreamParser::parseBank...illegal content type: " + ss.str(),__FILE__,__LINE__));
    break;

  }


  // new user arg is pointer to parent node
  return(newUserArg);
}


//-----------------------------------------------------------------------------
//---------------------------- evioDOMNode ------------------------------------
//-----------------------------------------------------------------------------


evioDOMNode::evioDOMNode(evioDOMNodeP par, unsigned short tag, unsigned char num, int contentType) throw(evioException)
  : parent(par), parentTree(NULL), tag(tag), num(num), contentType(contentType) {
}


//-----------------------------------------------------------------------------


evioDOMNodeP evioDOMNode::createEvioDOMNode(unsigned short tag, unsigned char num, ContainerType cType) throw(evioException) {
  return(new evioDOMContainerNode(NULL,tag,num,cType));
}


//-----------------------------------------------------------------------------


evioDOMNodeP evioDOMNode::createEvioDOMNode(unsigned short tag, unsigned char num, const evioSerializable &o, ContainerType cType) 
  throw(evioException) {
  evioDOMContainerNode *c = new evioDOMContainerNode(NULL,tag,num,cType);
  o.serialize(c);
  return(c);
}


//-----------------------------------------------------------------------------


evioDOMNodeP evioDOMNode::createEvioDOMNode(unsigned short tag, unsigned char num, void (*f)(evioDOMNodeP c, void *userArg), 
                                            void *userArg, ContainerType cType) throw(evioException) {
  evioDOMContainerNode *c = new evioDOMContainerNode(NULL,tag,num,cType);
  f(c,userArg);
  return(c);
}


//-----------------------------------------------------------------------------


evioDOMNode::~evioDOMNode(void) {
  evioDOMContainerNode *c = dynamic_cast<evioDOMContainerNode*>(this);
  if(c!=NULL) {
    evioDOMNodeList::iterator iter;
    for(iter=c->childList.begin(); iter!=c->childList.end(); iter++) {
      delete(*iter);
    }
  }
}


//-----------------------------------------------------------------------------


evioDOMNodeP evioDOMNode::cut(void) throw(evioException) {
  if(parent!=NULL) {
    evioDOMContainerNode *par = static_cast<evioDOMContainerNode*>(parent);
    par->childList.remove(this);
    parent=NULL;
  } else if (parentTree!=NULL) {
    parentTree->root=NULL;
    parentTree=NULL;
  }
  return(this);
}


//-----------------------------------------------------------------------------


void evioDOMNode::cutAndDelete(void) throw(evioException) {
  cut();
  delete(this);
}


//-----------------------------------------------------------------------------


evioDOMNodeP evioDOMNode::move(evioDOMNodeP newParent) throw(evioException) {

  cut();

  evioDOMContainerNode *par = dynamic_cast<evioDOMContainerNode*>(newParent);
  if(par==NULL)throw(evioException(0,"?evioDOMNode::move...parent node not a container",__FILE__,__LINE__));
  
  par->childList.push_back(this);
  parent=newParent;
  return(this);
}


//-----------------------------------------------------------------------------


void evioDOMNode::addNode(evioDOMNodeP node) throw(evioException) {
  if(node==NULL)return;
  if(!isContainer())throw(evioException(0,"?evioDOMNode::addNode...not a container",__FILE__,__LINE__));
  node->move(this);
}


//-----------------------------------------------------------------------------


evioDOMNodeList *evioDOMNode::getChildList(void) throw(evioException) {
  if(!isContainer())return(NULL);
  evioDOMContainerNode *c = dynamic_cast<evioDOMContainerNode*>(this);
  return(&c->childList);
}


//-----------------------------------------------------------------------------


evioDOMNode& evioDOMNode::operator<<(evioDOMNodeP node) throw(evioException) {
  addNode(node);
  return(*this);
}


//-----------------------------------------------------------------------------


bool evioDOMNode::operator==(unsigned short tg) const {
  return(this->tag==tg);
}


//-----------------------------------------------------------------------------


bool evioDOMNode::operator!=(unsigned short tg) const {
  return(this->tag!=tg);
}


//-----------------------------------------------------------------------------


bool evioDOMNode::operator==(tagNum tnPair) const {
  return(
         (this->tag==tnPair.first) &&
         (this->num==tnPair.second)
         );
}


//-----------------------------------------------------------------------------


bool evioDOMNode::operator!=(tagNum tnPair) const {
  return(
         (this->tag!=tnPair.first) ||
         (this->num!=tnPair.second)
         );
}


//-----------------------------------------------------------------------------


const evioDOMNodeP evioDOMNode::getParent(void) const {
  return(parent);
}


//-----------------------------------------------------------------------------


const evioDOMTreeP evioDOMNode::getParentTree(void) const {
  return(parentTree);
}


//-----------------------------------------------------------------------------


int evioDOMNode::getContentType(void) const {
  return(contentType);
}


//-----------------------------------------------------------------------------


bool evioDOMNode::isContainer(void) const {
  return(::is_container(contentType)==1);
}


//-----------------------------------------------------------------------------


bool evioDOMNode::isLeaf(void) const {
  return(::is_container(contentType)==0);
}


//--------------------------------------------------------------


string evioDOMNode::getIndent(int depth) {
  string s;
  for(int i=0; i<depth; i++) s+="   ";
  return(s);
}


//-----------------------------------------------------------------------------
//----------------------- evioDOMContainerNode --------------------------------
//-----------------------------------------------------------------------------


evioDOMContainerNode::evioDOMContainerNode(evioDOMNodeP par, unsigned short tg, unsigned char num, ContainerType cType) throw(evioException)
  : evioDOMNode(par,tg,num,cType) {
}


//-----------------------------------------------------------------------------


string evioDOMContainerNode::toString(void) const {
  ostringstream os;
  os << getHeader(0) << getFooter(0);
  return(os.str());
}


//-----------------------------------------------------------------------------

                                   
string evioDOMContainerNode::getHeader(int depth) const {
  ostringstream os;
  os << getIndent(depth)
     <<  "<" << get_typename(parent==NULL?BANK:parent->getContentType()) << " content=\"" << get_typename(contentType)
     << "\" data_type=\"" << hex << showbase << getContentType()
     << dec << "\" tag=\""  << tag;
  if((parent==NULL)||((parent->getContentType()==0xe)||(parent->getContentType()==0x10))) os << dec << "\" num=\"" << (int)num;
  os << "\">" << endl;
  return(os.str());
}


//-----------------------------------------------------------------------------

                                   
string evioDOMContainerNode::getFooter(int depth) const {
  ostringstream os;
  os << getIndent(depth) << "</" << get_typename(this->parent==NULL?BANK:this->parent->getContentType()) << ">" << endl;
  return(os.str());
}


//-----------------------------------------------------------------------------
//------------------------------- evioDOMTree ---------------------------------
//-----------------------------------------------------------------------------


evioDOMTree::evioDOMTree(const evioChannel &channel, const string &n) throw(evioException) : root(NULL), name(n) {
  const unsigned int *buf = channel.getBuffer();
  if(buf==NULL)throw(evioException(0,"?evioDOMTree constructor...channel delivered null buffer",__FILE__,__LINE__));
  root=parse(buf);
  root->parentTree=this;
}


//-----------------------------------------------------------------------------


evioDOMTree::evioDOMTree(const evioChannel *channel, const string &name) throw(evioException) : root(NULL), name(name) {
  if(channel==NULL)throw(evioException(0,"?evioDOMTree constructor...null channel",__FILE__,__LINE__));
  const unsigned int *buf = channel->getBuffer();
  if(buf==NULL)throw(evioException(0,"?evioDOMTree constructor...channel delivered null buffer",__FILE__,__LINE__));
  root=parse(buf);
  root->parentTree=this;
}


//-----------------------------------------------------------------------------

evioDOMTree::evioDOMTree(const unsigned long *buf, const string &name) throw(evioException) : root(NULL), name(name) {
  if(buf==NULL)throw(evioException(0,"?evioDOMTree constructor...null buffer",__FILE__,__LINE__));
  root=parse(reinterpret_cast<const unsigned int*>(buf));
  root->parentTree=this;
}


//-----------------------------------------------------------------------------


evioDOMTree::evioDOMTree(const unsigned int *buf, const string &name) throw(evioException) : root(NULL), name(name) {
  if(buf==NULL)throw(evioException(0,"?evioDOMTree constructor...null buffer",__FILE__,__LINE__));
  root=parse(buf);
  root->parentTree=this;
}


//-----------------------------------------------------------------------------


evioDOMTree::evioDOMTree(evioDOMNodeP node, const string &name) throw(evioException) : root(NULL), name(name) {
  if(node==NULL)throw(evioException(0,"?evioDOMTree constructor...null evioDOMNode",__FILE__,__LINE__));
  root=node;
  root->parentTree=this;
}


//-----------------------------------------------------------------------------


evioDOMTree::evioDOMTree(unsigned short tag, unsigned char num, ContainerType cType, const string &name) throw(evioException) : root(NULL), name(name) {
  root=evioDOMNode::createEvioDOMNode(tag,num,cType);
  root->parentTree=this;
}


//-----------------------------------------------------------------------------


evioDOMTree::~evioDOMTree(void) {
  root->cutAndDelete();
}


//-----------------------------------------------------------------------------


evioDOMNodeP evioDOMTree::parse(const unsigned int *buf) throw(evioException) {
  evioStreamParser p;
  return((evioDOMNodeP)p.parse(buf,*this,NULL));
}


//-----------------------------------------------------------------------------


void *evioDOMTree::containerNodeHandler(int length, unsigned short tag, int contentType, unsigned char num, int depth, 
                               void *userArg) {
    
    
  // get parent pointer
  evioDOMContainerNode *parent = (evioDOMContainerNode*)userArg;
    

  // create new node
  evioDOMNodeP newNode = evioDOMNode::createEvioDOMNode(tag,num,(ContainerType)contentType);


  // set parent pointers
  if(parent!=NULL) {
    parent->childList.push_back(newNode);
    newNode->parent=parent;
  }
  

  // return pointer to new node
  return((void*)newNode);
}
  
  
//-----------------------------------------------------------------------------


void evioDOMTree::leafNodeHandler(int length, unsigned short tag, int contentType, unsigned char num, int depth, 
                              const void *data, void *userArg) {


  // create and fill new leaf
  evioDOMNodeP newLeaf;
  string s;
  ostringstream os;
  switch (contentType) {

  case 0x0:
  case 0x1:
    newLeaf = evioDOMNode::createEvioDOMNode(tag,num,(unsigned int*)data,length);
    break;
      
  case 0x2:
    newLeaf = evioDOMNode::createEvioDOMNode(tag,num,(float*)data,length);
    break;
      
  case 0x3:
    for(int i=0; i<length; i++) os << ((char*)data)[i];
    os << ends;
    s=os.str();
    newLeaf = evioDOMNode::createEvioDOMNode(tag,num,&s,1);
    break;

  case 0x4:
    newLeaf = evioDOMNode::createEvioDOMNode(tag,num,(short*)data,length);
    break;

  case 0x5:
    newLeaf = evioDOMNode::createEvioDOMNode(tag,num,(unsigned short*)data,length);
    break;

  case 0x6:
    newLeaf = evioDOMNode::createEvioDOMNode(tag,num,(signed char*)data,length);
    break;

  case 0x7:
    newLeaf = evioDOMNode::createEvioDOMNode(tag,num,(unsigned char*)data,length);
    break;

  case 0x8:
    newLeaf = evioDOMNode::createEvioDOMNode(tag,num,(double*)data,length);
    break;

  case 0x9:
    newLeaf = evioDOMNode::createEvioDOMNode(tag,num,(long long*)data,length);
    break;

  case 0xa:
    newLeaf = evioDOMNode::createEvioDOMNode(tag,num,(unsigned long long*)data,length);
    break;

  case 0xb:
    if(sizeof(long)==4) {
      newLeaf = evioDOMNode::createEvioDOMNode(tag,num,(long*)data,length);
    } else {
      newLeaf = evioDOMNode::createEvioDOMNode(tag,num,(int*)data,length);
    }
    break;

  default:
    ostringstream ss;
    ss << hex << showbase << contentType<< ends;
    throw(evioException(0,"?evioDOMTree::leafNodeHandler...illegal content type: " + ss.str(),__FILE__,__LINE__));
    break;
  }


  // add new leaf to parent
  evioDOMContainerNode *parent = (evioDOMContainerNode*)userArg;
  if(parent!=NULL) {
    parent->childList.push_back(newLeaf);
    newLeaf->parent=parent;
  }
}


//-----------------------------------------------------------------------------


void evioDOMTree::clear(void) throw(evioException) {
  if(root!=NULL) {
    root->cutAndDelete();
    root=NULL;
  }
}


//-----------------------------------------------------------------------------


void evioDOMTree::addBank(evioDOMNodeP node) throw(evioException) {

  node->cut();

  if(root==NULL) {
    root=node;
    root->parentTree=this;

  } else {
    evioDOMContainerNode *c = dynamic_cast<evioDOMContainerNode*>(root);
    if(c==NULL)throw(evioException(0,"?evioDOMTree::addBank...root is not container",__FILE__,__LINE__));
    c->childList.push_back(node);
    node->parent=root;
  }
}


//-----------------------------------------------------------------------------


evioDOMTree& evioDOMTree::operator<<(evioDOMNodeP node) throw(evioException) {
  addBank(node);
  return(*this);
}


//-----------------------------------------------------------------------------


void evioDOMTree::toEVIOBuffer(unsigned int *buf, int size) const throw(evioException) {
  toEVIOBuffer(buf,root,size);
}


//-----------------------------------------------------------------------------


int evioDOMTree::toEVIOBuffer(unsigned int *buf, const evioDOMNodeP pNode, int size) const throw(evioException) {

  int bankLen,bankType,dataOffset;

  if(size<=0)throw(evioException(0,"?evioDOMTree::toEVOIBuffer...illegal buffer size",__FILE__,__LINE__));


  if(pNode->parent==NULL) {
    bankType=BANK;
  } else {
    bankType=pNode->parent->contentType;
  }


  // add bank header word(s)
  switch (bankType) {

  case 0xe:
  case 0x10:
    buf[0]=0;
    buf[1] = (pNode->tag<<16) | (pNode->contentType<<8) | pNode->num;
    dataOffset=2;
    break;
  case 0xd:
  case 0x20:
    if(pNode->num!=0)cout << "?warning...num ignored in segment: " << pNode->num << endl;
    buf[0] = (pNode->tag<<24) | ( pNode->contentType<<16);
    dataOffset=1;
    break;
  case 0xc:
  case 0x40:
    if(pNode->tag>0xfff)cout << "?warning...tag truncated to 12 bits in tagsegment: " << pNode->tag << endl;
    if(pNode->num!=0)cout << "?warning...num ignored in tagsegment: " << pNode->num<< endl;
    buf[0] = (pNode->tag<<20) | ( pNode->contentType<<16);
    dataOffset=1;
    break;
  default:
    ostringstream ss;
    ss << hex << showbase << bankType<< ends;
    throw(evioException(0,"evioDOMTree::toEVIOBuffer...illegal bank type in boilerplate: " + ss.str(),__FILE__,__LINE__));
    break;
  }


  // set starting length
  bankLen=dataOffset;


  // loop over contained nodes if container node
  const evioDOMContainerNode *c = dynamic_cast<const evioDOMContainerNode*>(pNode);
  if(c!=NULL) {
    evioDOMNodeList::const_iterator iter;
    for(iter=c->childList.begin(); iter!=c->childList.end(); iter++) {
      bankLen+=toEVIOBuffer(&buf[bankLen],*iter,size-bankLen-1);
      if(bankLen>size)throw(evioException(0,"?evioDOMTree::toEVOIBuffer...buffer too small",__FILE__,__LINE__));
    }


  // leaf node...copy data, and don't forget to pad!
  } else {

    int nword,ndata,i;
    switch (pNode->contentType) {

    case 0x0:
    case 0x1:
    case 0x2:
    case 0xb:
      {
        const evioDOMLeafNode<unsigned int> *leaf = static_cast<const evioDOMLeafNode<unsigned int>*>(pNode);
        ndata = leaf->data.size();
        nword = ndata;
        if(bankLen+nword>size)throw(evioException(0,"?evioDOMTree::toEVOIBuffer...buffer too small",__FILE__,__LINE__));
        for(i=0; i<ndata; i++) buf[dataOffset+i]=(unsigned int)(leaf->data[i]);
      }
      break;
      
    case 0x3:
      {
        const evioDOMLeafNode<string> *leaf = static_cast<const evioDOMLeafNode<string>*>(pNode);
        string s = leaf->data[0];
        ndata - s.size();
        nword = (ndata+3)/4;
        if(bankLen+nword>size)throw(evioException(0,"?evioDOMTree::toEVOIBuffer...buffer too small",__FILE__,__LINE__));
        unsigned char *c = (unsigned char*)&buf[dataOffset];
        for(i=0; i<ndata; i++) c[i]=(s.c_str())[i];
        for(i=ndata; i<ndata+(4-ndata%4)%4; i++) c[i]='\0';
      }
      break;

    case 0x4:
    case 0x5:
      {
        const evioDOMLeafNode<unsigned short> *leaf = static_cast<const evioDOMLeafNode<unsigned short>*>(pNode);
        ndata = leaf->data.size();
        nword = (ndata+1)/2;
        if(bankLen+nword>size)throw(evioException(0,"?evioDOMTree::toEVOIBuffer...buffer too small",__FILE__,__LINE__));
        unsigned short *s = (unsigned short *)&buf[dataOffset];
        for(i=0; i<ndata; i++) s[i]=static_cast<unsigned short>(leaf->data[i]);
        if((ndata%2)!=0)buf[ndata]=0;
      }
      break;

    case 0x6:
    case 0x7:
      {
        const evioDOMLeafNode<unsigned char> *leaf = static_cast<const evioDOMLeafNode<unsigned char>*>(pNode);
        ndata = leaf->data.size();
        nword = (ndata+3)/4;
        if(bankLen+nword>size)throw(evioException(0,"?evioDOMTree::toEVOIBuffer...buffer too small",__FILE__,__LINE__));
        unsigned char *c = (unsigned char*)&buf[dataOffset];
        for(i=0; i<ndata; i++) c[i]=static_cast<unsigned char>(leaf->data[i]);
        for(i=ndata; i<ndata+(4-ndata%4)%4; i++) c[i]='\0';
      }
      break;

    case 0x8:
    case 0x9:
    case 0xa:
      {
        const evioDOMLeafNode<unsigned long long> *leaf = static_cast<const evioDOMLeafNode<unsigned long long>*>(pNode);
        ndata = leaf->data.size();
        nword = ndata*2;
        if(bankLen+nword>size)throw(evioException(0,"?evioDOMTree::toEVOIBuffer...buffer too small",__FILE__,__LINE__));
        unsigned long long *ll = (unsigned long long*)&buf[dataOffset];
        for(i=0; i<ndata; i++) ll[i]=static_cast<unsigned long long>(leaf->data[i]);
      }
      break;
    default:
      ostringstream ss;
      ss << pNode->contentType<< ends;
      throw(evioException(0,"?evioDOMTree::toEVOIBuffer...illegal leaf type: " + ss.str(),__FILE__,__LINE__));
      break;
    }

    // increment data count
    bankLen+=nword;
  }


  // finaly set node length in buffer
  switch (bankType) {

  case 0xe:
  case 0x10:
    buf[0]=bankLen-1;
    break;
  case 0xd:
  case 0x20:
  case 0xc:
  case 0x40:
    if((bankLen-1)>0xffff)throw(evioException(0,"?evioDOMTree::toEVIOBuffer...length too long for segment type",__FILE__,__LINE__));
    buf[0]|=(bankLen-1);
    break;
  default: 
    ostringstream ss;
    ss << bankType<< ends;
    throw(evioException(0,"?evioDOMTree::toEVIOBuffer...illegal bank type setting length: " + ss.str(),__FILE__,__LINE__));
    break;
  }


  // return length of this bank
  return(bankLen);
}


//-----------------------------------------------------------------------------


evioDOMNodeListP evioDOMTree::getNodeList(void) throw(evioException) {
  return(evioDOMNodeListP(addToNodeList(root,new evioDOMNodeList,isTrue)));
}


//-----------------------------------------------------------------------------


string evioDOMTree::toString(void) const {

  if(root==NULL)return("<!-- empty tree -->");

  ostringstream os;
  os << endl << endl << "<!-- Dump of tree: " << name << " -->" << endl << endl;
  toOstream(os,root,0);
  os << endl << endl;
  return(os.str());

}


//-----------------------------------------------------------------------------


void evioDOMTree::toOstream(ostream &os, const evioDOMNodeP pNode, int depth) const throw(evioException) {

  
  if(pNode==NULL)return;


  // get node header
  os << pNode->getHeader(depth);


  // dump contained banks if node is a container
  const evioDOMContainerNode *c = dynamic_cast<const evioDOMContainerNode*>(pNode);
  if(c!=NULL) {
    evioDOMNodeList::const_iterator iter;
    for(iter=c->childList.begin(); iter!=c->childList.end(); iter++) {
      toOstream(os,*iter,depth+1);
    }
  }


  // get footer
  os << pNode->getFooter(depth);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
