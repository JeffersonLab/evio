/*
 *  evioUtil.cc
 *
 *   Assorted evio utilities, C++ version
 *
 *   Author:  Elliott Wolin, JLab, 31-aug-2005
*/


#include <evioUtil.hxx>

#define BANK 0xe

// debug ???
unsigned long *bufp;


//--------------------------------------------------------------
//-------------------- local variables -------------------------
//--------------------------------------------------------------


static bool debug = true;


//--------------------------------------------------------------
//----------------------local utilities ------------------------
//--------------------------------------------------------------


static string getIndent(int depth) {
  string s;
  for(int i=0; i<depth; i++) s+="   ";
  return(s);
}

//--------------------------------------------------------------

template <typename T> static void deleteIt(T *t) {
  delete(t);
}


//--------------------------------------------------------------
//------------------- class methods ----------------------------
//--------------------------------------------------------------


evioException::evioException() {
  type=0;
  text="";
}


//--------------------------------------------------------------


evioException::evioException(int t, const string &s) {
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


void evioException::setText(const string &t) {
  text=t;
}


//--------------------------------------------------------------


string evioException::getText(void) const {
  return(text);
}


//--------------------------------------------------------------


string evioException::toString(void) const {
  ostringstream s;
  s << type << ends;
  return(string("evioException type: ") + s.str() + string(",   text:  ") + text);
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


void *evioStreamParser::parseBank(const unsigned long *buf, int bankType, int depth, 
                                 evioStreamParserHandler &handler, void *userArg) throw(evioException*) {

  int length,tag,contentType,num,dataOffset,p,bankLen;
  void *newUserArg = userArg;
  const unsigned long *data;


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
    ss << hex << "0x" << bankType << ends;
    throw(new evioException(0,"?evioStreamParser::parseBank...illegal bank type: " + ss.str()));
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
    handler.leafHandler(length-dataOffset,tag,contentType,num,depth,
                        &buf[dataOffset],userArg);
    break;

  case 0x3:
  case 0x6:
  case 0x7:
    handler.leafHandler((length-dataOffset)*4,tag,contentType,num,depth,
                        (char*)(&buf[dataOffset]),userArg);
    break;

  case 0x4:
  case 0x5:
    handler.leafHandler((length-dataOffset)*2,tag,contentType,num,depth,
                        (short*)(&buf[dataOffset]),userArg);
    break;

  case 0x8:
  case 0x9:
  case 0xa:
    handler.leafHandler((length-dataOffset)/2,tag,contentType,num,
                        depth,(long long*)(&buf[dataOffset]),userArg);
    break;

  case 0xe:
  case 0x10:
  case 0xd:
  case 0x20:
  case 0xc:
  case 0x40:
    // call node handler and get new userArg
    newUserArg=handler.nodeHandler(length,tag,contentType,num,depth,userArg);


    // parse contained banks
    p       = 0;
    bankLen = length-dataOffset;
    data    = &buf[dataOffset];

    depth++;
    switch (contentType) {
    case 0xe:
    case 0x10:
      while(p<bankLen) {
        parseBank(&data[p],contentType,depth,handler,newUserArg);
        p+=data[p]+1;
      }
      break;

    case 0xd:
    case 0x20:
      while(p<bankLen) {
        parseBank(&data[p],contentType,depth,handler,newUserArg);
        p+=(data[p]&0xffff)+1;
      }
      break;

    case 0xc:
    case 0x40:
      while(p<bankLen) {
        parseBank(&data[p],contentType,depth,handler,newUserArg);
        p+=(data[p]&0xffff)+1;
      }
      break;
    }
    depth--;
    break;


  default:
    ostringstream ss;
    ss << hex << "0x" << contentType << ends;
    throw(new evioException(0,"?evioStreamParser::parseBank...illegal content type: " + ss.str()));
    break;

  }  // end main switch(contentType)


  // new user arg is pointer to parent node
  return(newUserArg);
}


//--------------------------------------------------------------
//--------------------------------------------------------------


evioDOMTree::evioDOMTree(evioDOMNode *r, const string &n) throw(evioException*) {
  if(r==NULL)throw(new evioException(0,"?evioDOMTree constructor...null evioDOMNode"));
  name=n;
  root=r;
}


//-----------------------------------------------------------------------------


evioDOMTree::evioDOMTree(const unsigned long *buf, const string &n) throw(evioException*) {
  if(buf==NULL)throw(new evioException(0,"?evioDOMTree constructor...null buffer"));
  name=n;
  root=parse(buf);
}


//-----------------------------------------------------------------------------


evioDOMTree::~evioDOMTree(void) {
  delete(root);
  if(debug)cout << "deleted tree " << name << endl;
}


//-----------------------------------------------------------------------------


evioDOMNode *evioDOMTree::parse(const unsigned long *buf) throw(evioException*) {
  evioStreamParser p;
  return((evioDOMNode*)p.parse(buf,*this,NULL));
}


//-----------------------------------------------------------------------------


void *evioDOMTree::nodeHandler(int length, int tag, int contentType, int num, int depth, 
                               void *userArg) {
    
  if(debug) printf("node   depth %2d  contentType,tag,num,length:  0x%-6x %6d %6d %6d\n",
                   depth,contentType,tag,num,length);
    
    
  // get parent pointer
  evioDOMContainerNode *parent = (evioDOMContainerNode*)userArg;
    

  // create new node
  evioDOMContainerNode *newNode = new evioDOMContainerNode(parent,tag,contentType,num);


  // add new node to parent's list
  if(parent!=NULL)parent->childList.push_back(newNode);


  // return new userArg that points to the new node
  return((void*)newNode);
}
  
  
//-----------------------------------------------------------------------------


void evioDOMTree::leafHandler(int length, int tag, int contentType, int num, int depth, 
                              const void *data, void *userArg) {

  if(debug) printf("leaf   depth %2d  contentType,tag,num,length:  0x%-6x %6d %6d %6d\n",
                   depth,contentType,tag,num,length);


  // get parent pointer
  evioDOMContainerNode *parent = (evioDOMContainerNode*)userArg;


  // create and fill new leaf
  evioDOMNode *newLeaf;
  string s;
  ostringstream os;
  switch (contentType) {

  case 0x0:
  case 0x1:
    newLeaf = new evioDOMLeafNode<unsigned long>(parent,tag,contentType,num,(unsigned long*)data,length);
    break;
      
  case 0x2:
    newLeaf = new evioDOMLeafNode<float>(parent,tag,contentType,num,(float*)data,length);
    break;
      
  case 0x3:
    for(int i=0; i<length; i++) os << ((char *)data)[i];
    os << ends;
    s=os.str();
    newLeaf = new evioDOMLeafNode<string>(parent,tag,contentType,num,&s,1);
    break;

  case 0x4:
    newLeaf = new evioDOMLeafNode<short>(parent,tag,contentType,num,(short*)data,length);
    break;

  case 0x5:
    newLeaf = new evioDOMLeafNode<unsigned short>(parent,tag,contentType,num,(unsigned short*)data,length);
    break;

  case 0x6:
    newLeaf = new evioDOMLeafNode<char>(parent,tag,contentType,num,(char*)data,length);
    break;

  case 0x7:
    newLeaf = new evioDOMLeafNode<unsigned char>(parent,tag,contentType,num,(unsigned char*)data,length);
    break;

  case 0x8:
    newLeaf = new evioDOMLeafNode<double>(parent,tag,contentType,num,(double*)data,length);
    break;

  case 0x9:
    newLeaf = new evioDOMLeafNode<long long>(parent,tag,contentType,num,(long long*)data,length);
    break;

  case 0xa:
    newLeaf = new evioDOMLeafNode<unsigned long long>(parent,tag,contentType,num,(unsigned long long*)data,length);
    break;

  case 0xb:
    newLeaf = new evioDOMLeafNode<long>(parent,tag,contentType,num,(long*)data,length);
    break;

  default:
    ostringstream ss;
    ss << hex << "0x" << contentType<< ends;
    throw(new evioException(0,"?evioDOMTree::leafHandler...illegal content type: " + ss.str()));
    break;
  }


  // add new leaf to parent list
  if(parent!=NULL)parent->childList.push_back(newLeaf);
}


//-----------------------------------------------------------------------------


void evioDOMTree::toEVIOBuffer(unsigned long *buf) const throw(evioException*) {
  bufp=buf;
  toEVIOBuffer(buf,root);
}


//-----------------------------------------------------------------------------


int evioDOMTree::toEVIOBuffer(unsigned long *buf, evioDOMNode *pNode) const throw(evioException*) {

  int bankLen,bankType,dataOffset;


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
    buf[0] = (pNode->tag<<24) | ( pNode->contentType<<16);
    dataOffset=1;
    break;
  case 0xc:
  case 0x40:
    buf[0] = (pNode->tag<<20) | ( pNode->contentType<<16);
    dataOffset=1;
    break;
  default:
    ostringstream ss;
    ss << hex << "0x" << bankType<< ends;
    throw(new evioException(0,"evioDOMTree::toEVIOBuffer...illegal bank type in boilerplate: " + ss.str()));
    break;
  }


  // set starting length
  bankLen=dataOffset;


  // if container node loop over contained nodes
  const evioDOMContainerNode *c = dynamic_cast<const evioDOMContainerNode*>(pNode);
  if(c!=NULL) {
    list<evioDOMNode*>::const_iterator iter;
    for(iter=c->childList.begin(); iter!=c->childList.end(); iter++) {
      bankLen+=toEVIOBuffer(&buf[bankLen],*iter);
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
        const evioDOMLeafNode<unsigned long*> *leaf = static_cast<const evioDOMLeafNode<unsigned long*>*>(pNode);
        ndata = leaf->data.size();
        nword = ndata;
        for(i=0; i<ndata; i++) buf[dataOffset+i]=(unsigned long)(leaf->data[i]);
      }
      break;
      
    case 0x3:
      {
        const evioDOMLeafNode<string> *leaf = static_cast<const evioDOMLeafNode<string>*>(pNode);
        string s = leaf->data[0];
        ndata - s.size();
        nword = (ndata+3)/4;
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
        unsigned long long *ll = (unsigned long long*)&buf[dataOffset];
        for(i=0; i<ndata; i++) ll[i]=static_cast<unsigned long long>(leaf->data[i]);
      }
      break;
    default:
      ostringstream ss;
      ss << pNode->contentType<< ends;
      throw(new evioException(0,"?evioDOMTree::toEVOIBuffer...illegal leaf type: " + ss.str()));
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
    if((bankLen-1)>0xffff)throw(new evioException(0,"?evioDOMTree::toEVIOVuffer...length too long for segment type"));
    buf[0]|=bankLen-1;
    break;
  default: 
    ostringstream ss;
    ss << bankType<< ends;
    throw(new evioException(0,"?evioDOMTree::toEVIOVuffer...illegal bank type setting length: " + ss.str()));
    break;
  }


  // return length of this bank
  return(bankLen);
}


//-----------------------------------------------------------------------------

const evioDOMNode *evioDOMTree::getRoot(void) const {
  return(root);
}


//-----------------------------------------------------------------------------


string evioDOMTree::getName(void) const {
  return(name);
}


//-----------------------------------------------------------------------------


void evioDOMTree::setName(const string &newName) {
  name=newName;
  return;
}


//-----------------------------------------------------------------------------


string evioDOMTree::toString(void) const {

  if(root==NULL)return("");

  ostringstream os;
  os << endl << endl << "<!-- Dump of tree: " << getName() << " -->" << endl << endl;
  toOstream(os,root,0);
  os << endl << endl;
  return(os.str());
}


//-----------------------------------------------------------------------------


void evioDOMTree::toOstream(ostream &os, const evioDOMNode *pNode, int depth) const throw(evioException*) {

  
  if(pNode==NULL)return;


  // dump node opening information into ostream
  pNode->toString(os,depth);


  // dump contained banks if node is a container
  const evioDOMContainerNode *c = dynamic_cast<const evioDOMContainerNode*>(pNode);
  if(c!=NULL) {
    list<evioDOMNode*>::const_iterator iter;
    for(iter=c->childList.begin(); iter!=c->childList.end(); iter++) {
      toOstream(os,*iter,depth+1);
    }
  }


  // closing tags different for container or leaf
  if(c!=NULL) {
    os << getIndent(depth) << "</" << get_typename(pNode->parent==NULL?BANK:pNode->parent->contentType) << ">" << endl;
  } else {
    os << getIndent(depth) << "</" << get_typename(pNode->contentType) << ">" << endl;
  }
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


evioDOMContainerNode::evioDOMContainerNode(evioDOMNode *par, int tg, int content, int n)
  throw(evioException*) {

  parent        = par;
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
  os << getIndent(depth)
     <<  "<" << get_typename(parent==NULL?BANK:parent->contentType) << " tag=\'"  << tag << "\' data_type=\'" 
     << hex << "0x" << contentType << dec << "\' num=\'" << num << "\">" << endl;
}


//-----------------------------------------------------------------------------

                                   
evioDOMContainerNode::~evioDOMContainerNode(void) {
  if(debug)cout << "deleting container node" << endl;
  for_each(childList.begin(),childList.end(),deleteIt<evioDOMNode>);
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

                                   
template <class T> evioDOMLeafNode<T>::evioDOMLeafNode(evioDOMNode *par, int tg, int content, int n,
                                                       T *p, int ndata) throw(evioException*) {
  
  parent        = par;
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

  string indent = getIndent(depth);
  string indent2 = indent + "    ";

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
     <<  "<" << get_typename(contentType) <<  " tag=\'" << tag << "\' data_type=\'" << hex << "0x" << contentType 
     << dec << "\' num=\'" << num << "\'>" << endl;


  // dump data...odd what has to be done for unsigned char type 0x7
  T i;
  int *j;
  typename vector<T>::const_iterator iter;
  for(iter=data.begin(); iter!=data.end();) {

    if(contentType!=0x3)os << indent2;
    for(int j=0; (j<wid)&&(iter!=data.end()); j++) {
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


template <class T> evioDOMLeafNode<T>::~evioDOMLeafNode(void) {
  if(debug)cout << "deleting leaf node" << endl;
}
  

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
