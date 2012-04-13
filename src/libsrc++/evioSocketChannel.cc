/*
 *  evioSocketChannel.cc
 *
 *   Author:  Elliott Wolin, JLab, 12-Apr-2012
*/


#include "evioSocketChannel.hxx"


using namespace std;
using namespace evio;



//-----------------------------------------------------------------------
//------------------------- evioSocketChannel -----------------------------
//-----------------------------------------------------------------------


/**
 * Constructor opens socket for reading or writing.
 * @param sockFD Socket file descriptor
 * @param m I/O mode, "r" or "w"
 * @param size Internal event buffer size
 */
evioSocketChannel::evioSocketChannel(int sockFD, const string &m, int size) throw(evioException) 
  : evioChannel(), sockFD(sockFD), mode(m), handle(0), bufSize(size), socketXMLDictionary(""), createdSocketDictionary(false)  {
  if(sockFD==0)throw(evioException(0,"?evioSocketChannel constructor...zero socket file descriptor",__FILE__,__FUNCTION__,__LINE__));

  // allocate internal event buffer
  buf = new uint32_t[bufSize];
  if(buf==NULL)throw(evioException(0,"?evioSocketChannel constructor...unable to allocate buffer",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Destructor closes socket, deletes internal buffer
 */
evioSocketChannel::~evioSocketChannel(void) {
  if(handle!=0)close();
  if(buf!=NULL)delete[](buf),buf=NULL;
  if(createdSocketDictionary)delete(dictionary),dictionary=NULL;
}


//-----------------------------------------------------------------------


/**
 * Opens channel for reading or writing.
 * For read, user-supplied dictionary overrides one received from socket.
 */
void evioSocketChannel::open(void) throw(evioException) {

  if(buf==NULL)throw(evioException(0,"evioSocketChannel::open...null buffer",__FILE__,__FUNCTION__,__LINE__));
  if(evOpenSocket(sockFD,const_cast<char*>(mode.c_str()),&handle)<0)
    throw(evioException(0,"?evioSocketChannel::open...unable to open socket",__FILE__,__FUNCTION__,__LINE__));
  if(handle==0)throw(evioException(0,"?evioSocketChannel::open...zero handle",__FILE__,__FUNCTION__,__LINE__));


  // on read check if socket has dictionary, warn if conflict with user dictionary
  // store socket XML just in case
  // set dictionary on write
  if((mode=="r")||(mode=="R")) {
    char *d;
    int len;
    int stat=evGetDictionary(handle,&d,&len);
    if((stat==S_SUCCESS)&&(d!=NULL)&&(len>0))socketXMLDictionary = string(d);

    if(dictionary==NULL) {
      if(stat!=S_SUCCESS)throw(evioException(0,"?evioSocketChannel::open...bad dictionary in socket",__FILE__,__FUNCTION__,__LINE__));
      if((d!=NULL)&&(len>0)) {
        dictionary = new evioDictionary(d);
        createdSocketDictionary=true;
      }
    } else {
      cout << "evioSocketChannel::open...user-supplied dictionary overrides dictionary in socket" << endl;
    }

  } else if((dictionary!=NULL) && ((mode=="w")||(mode=="W"))) {
    evWriteDictionary(handle,const_cast<char*>(dictionary->getDictionaryXML().c_str()));
  }

}


//-----------------------------------------------------------------------


/**
 * Reads from socket
 * @return true if successful, false on EOF or other evRead error condition
 */
bool evioSocketChannel::read(void) throw(evioException) {
  if(buf==NULL)throw(evioException(0,"evioSocketChannel::read...null buffer",__FILE__,__FUNCTION__,__LINE__));
  if(handle==0)throw(evioException(0,"evioSocketChannel::read...0 handle",__FILE__,__FUNCTION__,__LINE__));
  return(evRead(handle,&buf[0],bufSize)==0);
}


//-----------------------------------------------------------------------


/**
 * Reads from socket into user-supplied event buffer.
 * @param myBuf User-supplied buffer.
 * @parem length Length of buffer in 4-byte words.
 * @return true if successful, false on EOF or other evRead error condition
 */
bool evioSocketChannel::read(uint32_t *myBuf, int length) throw(evioException) {
  if(myBuf==NULL)throw(evioException(0,"evioSocketChannel::read...null user buffer",__FILE__,__FUNCTION__,__LINE__));
  if(handle==0)throw(evioException(0,"evioSocketChannel::read...0 handle",__FILE__,__FUNCTION__,__LINE__));
  return(evRead(handle,&myBuf[0],length)==0);
}


//-----------------------------------------------------------------------


/**
 * Writes to socket from internal buffer.
 */
void evioSocketChannel::write(void) throw(evioException) {
  if(buf==NULL)throw(evioException(0,"evioSocketChannel::write...null buffer",__FILE__,__FUNCTION__,__LINE__));
  if(handle==0)throw(evioException(0,"evioSocketChannel::write...0 handle",__FILE__,__FUNCTION__,__LINE__));
  if(evWrite(handle,buf)!=0) throw(evioException(0,"?evioSocketChannel::write...unable to write",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Writes to socket from user-supplied buffer.
 * @param myBuf Buffer containing event
 */
void evioSocketChannel::write(const uint32_t *myBuf) throw(evioException) {
  if(myBuf==NULL)throw(evioException(0,"evioSocketChannel::write...null myBuf",__FILE__,__FUNCTION__,__LINE__));
  if(handle==0)throw(evioException(0,"evioSocketChannel::write...0 handle",__FILE__,__FUNCTION__,__LINE__));
  if(evWrite(handle,myBuf)!=0) throw(evioException(0,"?evioSocketChannel::write...unable to write from myBuf",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Writes to socket from internal buffer of another evioChannel object.
 * @param channel Channel object
 */
void evioSocketChannel::write(const evioChannel &channel) throw(evioException) {
  if(handle==0)throw(evioException(0,"evioSocketChannel::write...0 handle",__FILE__,__FUNCTION__,__LINE__));
  if(evWrite(handle,channel.getBuffer())!=0) throw(evioException(0,"?evioSocketChannel::write...unable to write from buffer",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Writes from internal buffer of another evioChannel object.
 * @param channel Pointer to channel object
 */
void evioSocketChannel::write(const evioChannel *channel) throw(evioException) {
  if(channel==NULL)throw(evioException(0,"evioSocketChannel::write...null channel",__FILE__,__FUNCTION__,__LINE__));
  evioSocketChannel::write(*channel);
}


//-----------------------------------------------------------------------


/**
 * Writes from contents of evioChannelBufferizable object.
 * @param o evioChannelBufferizable object that can serialize to socket
 */
void evioSocketChannel::write(const evioChannelBufferizable &o) throw(evioException) {
  if(handle==0)throw(evioException(0,"evioSocketChannel::write...0 handle",__FILE__,__FUNCTION__,__LINE__));
  o.toEVIOBuffer(buf,bufSize);
  evioSocketChannel::write();
}


//-----------------------------------------------------------------------


/**
 * Writes from contents of evioChannelBufferizable object.
 * @param o Pointer to evioChannelBufferizable object that can serialize to socket
 */
void evioSocketChannel::write(const evioChannelBufferizable *o) throw(evioException) {
  if(o==NULL)throw(evioException(0,"evioSocketChannel::write...null evioChannelBufferizable pointer",__FILE__,__FUNCTION__,__LINE__));
  evioSocketChannel::write(*o);
}


//-----------------------------------------------------------------------


/**
 * For getting and setting evIoctl parameters.
 * @param request String containing evIoctl parameters
 * @param argp Additional evIoctl parameter
 */
int evioSocketChannel::ioctl(const string &request, void *argp) throw(evioException) {
  int stat;
  if(handle==0)throw(evioException(0,"evioSocketChannel::ioctl...0 handle",__FILE__,__FUNCTION__,__LINE__));
  stat=evIoctl(handle,const_cast<char*>(request.c_str()),argp)!=0;
  if(stat!=S_SUCCESS)throw(evioException(stat,"?evioSocketChannel::ioctl...error return",__FILE__,__FUNCTION__,__LINE__));
  return(stat);
}


//-----------------------------------------------------------------------


/**
 * Closes channel.
 */
void evioSocketChannel::close(void) throw(evioException) {
  if(handle==0)throw(evioException(0,"evioSocketChannel::close...0 handle",__FILE__,__FUNCTION__,__LINE__));
  evClose(handle);
  handle=0;
}


//-----------------------------------------------------------------------


/**
 * Returns channel I/O mode.
 * @return String containing I/O mode
 */
string evioSocketChannel::getMode(void) const {
  return(mode);
}


//-----------------------------------------------------------------------


/**
 * Returns pointer to internal channel buffer.
 * @return Pointer to internal buffer
 */
const uint32_t *evioSocketChannel::getBuffer(void) const throw(evioException) {
  if(buf==NULL)throw(evioException(0,"evioSocketChannel::getBuffer...null buffer",__FILE__,__FUNCTION__,__LINE__));
  return(buf);
}


//-----------------------------------------------------------------------


/**
 * Returns internal channel buffer size.
 * @return Internal buffer size in 4-byte words
 */
int evioSocketChannel::getBufSize(void) const {
  return(bufSize);
}


//-----------------------------------------------------------------------


/**
 * Returns socket file descriptor.
 * @return socket file descriptor
 */
 int evioSocketChannel::getSocketFD(void) const throw(evioException) {
  return(sockFD);
}


//-----------------------------------------------------------------------



/**
 * Returns XML dictionary read in from socket
 * @return XML dictionary read in from socket
 */
string evioSocketChannel::getSocketXMLDictionary(void) const {
  return(socketXMLDictionary);
}


//-----------------------------------------------------------------------
