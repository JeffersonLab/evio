/*
 *  evioBufferChannel.cc
 *
 *   Author:  Elliott Wolin, JLab, 12-Apr-2012
*/


#include "evioBufferChannel.hxx"


using namespace std;
using namespace evio;



//-----------------------------------------------------------------------
//------------------------- evioBufferChannel -----------------------------
//-----------------------------------------------------------------------


/**
 * Constructor opens buffer for reading or writing.
 * @param streamBuf Stream buffer specified by the user
 * @param bufLen size of the stream buffer
 * @param m I/O mode, "r" or "w"
 * @param size Internal event buffer size
 */
evioBufferChannel::evioBufferChannel(uint32_t *streamBuf, int bufLen, const string &m, int size) throw(evioException) 
  : evioChannel(), streamBuf(streamBuf), streamBufSize(bufLen), mode(m), handle(0), bufSize(size), bufferXMLDictionary("") {
  if(streamBuf==NULL)throw(evioException(0,"?evioBufferChannel constructor...NULL buffer",__FILE__,__FUNCTION__,__LINE__));

  // allocate internal event buffer
  buf = new uint32_t[bufSize];
  if(buf==NULL)throw(evioException(0,"?evioBufferChannel constructor...unable to allocate buffer",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Destructor closes buffer, deletes internal buffer and dictionary.
 */
evioBufferChannel::~evioBufferChannel(void) {
  if(handle!=0)close();
  if(buf!=NULL)delete[](buf),buf=NULL;
}


//-----------------------------------------------------------------------


/**
 * Opens channel for reading or writing.
 * For read, user-supplied dictionary overrides one found in buffer.
 */
void evioBufferChannel::open(void) throw(evioException) {

  if(buf==NULL)throw(evioException(0,"evioBufferChannel::open...null buffer",__FILE__,__FUNCTION__,__LINE__));
  if(evOpenBuffer((char*)streamBuf,streamBufSize,const_cast<char*>(mode.c_str()),&handle)<0)
    throw(evioException(0,"?evioBufferChannel::open...unable to open buffer",__FILE__,__FUNCTION__,__LINE__));
  if(handle==0)throw(evioException(0,"?evioBufferChannel::open...zero handle",__FILE__,__FUNCTION__,__LINE__));


  // on read check if buffer has dictionary, warn if conflict with user dictionary
  // store buffer XML just in case
  // set dictionary on write
  if((mode=="r")||(mode=="R")) {
    char *d;
    int len;
    int stat=evGetDictionary(handle,&d,&len);
    if((stat==S_SUCCESS)&&(d!=NULL)&&(len>0))bufferXMLDictionary = string(d);

    if(dictionary==NULL) {
      if(stat!=S_SUCCESS)throw(evioException(0,"?evioBufferChannel::open...bad dictionary in buffer",__FILE__,__FUNCTION__,__LINE__));
      if((d!=NULL)&&(len>0))dictionary = new evioDictionary(d);
    } else {
      cout << "evioBufferChannel::open...user-supplied dictionary overrides dictionary in buffer" << endl;
    }

  } else if((dictionary!=NULL) && ((mode=="w")||(mode=="W"))) {
    evWriteDictionary(handle,const_cast<char*>(dictionary->getDictionaryXML().c_str()));
  }

}


//-----------------------------------------------------------------------


/**
 * Reads from buffer
 * @return true if successful, false on EOF or other evRead error condition
 */
bool evioBufferChannel::read(void) throw(evioException) {
  if(buf==NULL)throw(evioException(0,"evioBufferChannel::read...null buffer",__FILE__,__FUNCTION__,__LINE__));
  if(handle==0)throw(evioException(0,"evioBufferChannel::read...0 handle",__FILE__,__FUNCTION__,__LINE__));
  return(evRead(handle,&buf[0],bufSize)==0);
}


//-----------------------------------------------------------------------


/**
 * Reads from buffer into user-supplied event buffer.
 * @param myBuf User-supplied buffer.
 * @parem length Length of buffer in 4-byte words.
 * @return true if successful, false on EOF or other evRead error condition
 */
bool evioBufferChannel::read(uint32_t *myBuf, int length) throw(evioException) {
  if(myBuf==NULL)throw(evioException(0,"evioBufferChannel::read...null user buffer",__FILE__,__FUNCTION__,__LINE__));
  if(handle==0)throw(evioException(0,"evioBufferChannel::read...0 handle",__FILE__,__FUNCTION__,__LINE__));
  return(evRead(handle,&myBuf[0],length)==0);
}


//-----------------------------------------------------------------------


/**
 * Writes to buffer from internal buffer.
 */
void evioBufferChannel::write(void) throw(evioException) {
  if(buf==NULL)throw(evioException(0,"evioBufferChannel::write...null buffer",__FILE__,__FUNCTION__,__LINE__));
  if(handle==0)throw(evioException(0,"evioBufferChannel::write...0 handle",__FILE__,__FUNCTION__,__LINE__));
  if(evWrite(handle,buf)!=0) throw(evioException(0,"?evioBufferChannel::write...unable to write",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Writes to buffer from user-supplied buffer.
 * @param myBuf Buffer containing event
 */
void evioBufferChannel::write(const uint32_t *myBuf) throw(evioException) {
  if(myBuf==NULL)throw(evioException(0,"evioBufferChannel::write...null myBuf",__FILE__,__FUNCTION__,__LINE__));
  if(handle==0)throw(evioException(0,"evioBufferChannel::write...0 handle",__FILE__,__FUNCTION__,__LINE__));
  if(evWrite(handle,myBuf)!=0) throw(evioException(0,"?evioBufferChannel::write...unable to write from myBuf",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Writes to buffer from internal buffer of another evioChannel object.
 * @param channel Channel object
 */
void evioBufferChannel::write(const evioChannel &channel) throw(evioException) {
  if(handle==0)throw(evioException(0,"evioBufferChannel::write...0 handle",__FILE__,__FUNCTION__,__LINE__));
  if(evWrite(handle,channel.getBuffer())!=0) throw(evioException(0,"?evioBufferChannel::write...unable to write from channel",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Writes from internal buffer of another evioChannel object.
 * @param channel Pointer to channel object
 */
void evioBufferChannel::write(const evioChannel *channel) throw(evioException) {
  if(channel==NULL)throw(evioException(0,"evioBufferChannel::write...null channel",__FILE__,__FUNCTION__,__LINE__));
  evioBufferChannel::write(*channel);
}


//-----------------------------------------------------------------------


/**
 * Writes from contents of evioChannelBufferizable object.
 * @param o evioChannelBufferizable object that can serialze to buffer
 */
void evioBufferChannel::write(const evioChannelBufferizable &o) throw(evioException) {
  if(handle==0)throw(evioException(0,"evioBufferChannel::write...0 handle",__FILE__,__FUNCTION__,__LINE__));
  o.toEVIOBuffer(buf,bufSize);
  evioBufferChannel::write();
}


//-----------------------------------------------------------------------


/**
 * Writes from contents of evioChannelBufferizable object.
 * @param o Pointer to evioChannelBufferizable object that can serialize to buffer
 */
void evioBufferChannel::write(const evioChannelBufferizable *o) throw(evioException) {
  if(o==NULL)throw(evioException(0,"evioBufferChannel::write...null evioChannel Bufferizable pointer",__FILE__,__FUNCTION__,__LINE__));
  evioBufferChannel::write(*o);
}


//-----------------------------------------------------------------------


/**
 * For getting and setting evIoctl parameters.
 * @param request String containing evIoctl parameters
 * @param argp Additional evIoctl parameter
 */
int evioBufferChannel::ioctl(const string &request, void *argp) throw(evioException) {
  int stat;
  if(handle==0)throw(evioException(0,"evioBufferChannel::ioctl...0 handle",__FILE__,__FUNCTION__,__LINE__));
  stat=evIoctl(handle,const_cast<char*>(request.c_str()),argp)!=0;
  if(stat!=S_SUCCESS)throw(evioException(stat,"?evioBufferChannel::ioCtl...error return",__FILE__,__FUNCTION__,__LINE__));
  return(stat);
}


//-----------------------------------------------------------------------


/**
 * Closes channel.
 */
void evioBufferChannel::close(void) throw(evioException) {
  if(handle==0)throw(evioException(0,"evioBufferChannel::close...0 handle",__FILE__,__FUNCTION__,__LINE__));
  evClose(handle);
  handle=0;
}


//-----------------------------------------------------------------------


/**
 * Returns channel I/O mode.
 * @return String containing I/O mode
 */
string evioBufferChannel::getMode(void) const {
  return(mode);
}


//-----------------------------------------------------------------------


/**
 * Returns pointer to internal channel buffer.
 * @return Pointer to internal buffer
 */
const uint32_t *evioBufferChannel::getBuffer(void) const throw(evioException) {
  if(buf==NULL)throw(evioException(0,"evioBufferChannel::getbuffer...null buffer",__FILE__,__FUNCTION__,__LINE__));
  return(buf);
}


//-----------------------------------------------------------------------



/**
 * Returns internal channel buffer size.
 * @return Internal buffer size in 4-byte words
 */
int evioBufferChannel::getBufSize(void) const {
  return(bufSize);
}


//-----------------------------------------------------------------------


/**
 * Returns pointer to stream buffer.
 * @return Pointer to stream buffer
 */
const uint32_t *evioBufferChannel::getStreamBuffer(void) const throw(evioException) {
  return(streamBuf);
}


//-----------------------------------------------------------------------



/**
 * Returns internal channel buffer size.
 * @return Internal buffer size in 4-byte words
 */
int evioBufferChannel::getStreamBufSize(void) const {
  return(streamBufSize);
}


//-----------------------------------------------------------------------


/**
 * Returns XML dictionary read in from buffer
 * @return XML dictionary read in from buffer
 */
string evioBufferChannel::getBufferXMLDictionary(void) const {
  return(bufferXMLDictionary);
}


//-----------------------------------------------------------------------
