/*
 *  evioFileChannel.cc
 *
 *   Author:  Elliott Wolin, JLab, 18-feb-2010
*/


#include "evioFileChannel.hxx"


using namespace std;
using namespace evio;



//-----------------------------------------------------------------------
//------------------------- evioFileChannel -----------------------------
//-----------------------------------------------------------------------


/**
 * Constructor opens file for reading or writing.
 * @param f File name
 * @param m I/O mode, "r" or "w"
 * @param size Internal buffer size
 */
evioFileChannel::evioFileChannel(const string &f, const string &m, int size) throw(evioException) 
  : filename(f), mode(m), handle(0), bufSize(size) {

  // allocate buffer
  buf = new uint32_t[bufSize];
  if(buf==NULL)throw(evioException(0,"?evioFileChannel constructor...unable to allocate buffer",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Destructor closes file, deletes internal buffer.
 */
evioFileChannel::~evioFileChannel(void) {
  if(handle!=0)close();
  if(buf!=NULL) {
    delete[](buf),buf=NULL;
  }
}


//-----------------------------------------------------------------------


/**
 * Opens channel for reading or writing.
 */
void evioFileChannel::open(void) throw(evioException) {

  if(buf==NULL)throw(evioException(0,"evioFileChannel::open...null buffer",__FILE__,__FUNCTION__,__LINE__));
  if(evOpen(const_cast<char*>(filename.c_str()),const_cast<char*>(mode.c_str()),&handle)<0)
    throw(evioException(0,"?evioFileChannel::open...unable to open file",__FILE__,__FUNCTION__,__LINE__));
  if(handle==0)throw(evioException(0,"?evioFileChannel::open...zero handle",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Reads from file into internal buffer.
 * @return true if successful, false on EOF or other evRead error condition
 */
bool evioFileChannel::read(void) throw(evioException) {
  if(buf==NULL)throw(evioException(0,"evioFileChannel::read...null buffer",__FILE__,__FUNCTION__,__LINE__));
  if(handle==0)throw(evioException(0,"evioFileChannel::read...0 handle",__FILE__,__FUNCTION__,__LINE__));
  return(evRead(handle,&buf[0],bufSize)==0);
}


//-----------------------------------------------------------------------


/**
 * Reads from file into user-supplied buffer.
 * @param myBuf User-supplied buffer.
 * @parem length Length of buffer in 4-byte words.
 * @return true if successful, false on EOF or other evRead error condition
 */
bool evioFileChannel::read(uint32_t *myBuf, int length) throw(evioException) {
  if(myBuf==NULL)throw(evioException(0,"evioFileChannel::read...null user buffer",__FILE__,__FUNCTION__,__LINE__));
  if(handle==0)throw(evioException(0,"evioFileChannel::read...0 handle",__FILE__,__FUNCTION__,__LINE__));
  return(evRead(handle,&myBuf[0],length)==0);
}


//-----------------------------------------------------------------------


/**
 * Writes to file from internal buffer.
 */
void evioFileChannel::write(void) throw(evioException) {
  if(buf==NULL)throw(evioException(0,"evioFileChannel::write...null buffer",__FILE__,__FUNCTION__,__LINE__));
  if(handle==0)throw(evioException(0,"evioFileChannel::write...0 handle",__FILE__,__FUNCTION__,__LINE__));
  if(evWrite(handle,buf)!=0) throw(evioException(0,"?evioFileChannel::write...unable to write",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Writes to file from user-supplied buffer.
 * @param myBuf Buffer containing event
 */
void evioFileChannel::write(const uint32_t *myBuf) throw(evioException) {
  if(myBuf==NULL)throw(evioException(0,"evioFileChannel::write...null myBuf",__FILE__,__FUNCTION__,__LINE__));
  if(handle==0)throw(evioException(0,"evioFileChannel::write...0 handle",__FILE__,__FUNCTION__,__LINE__));
  if(evWrite(handle,myBuf)!=0) throw(evioException(0,"?evioFileChannel::write...unable to write from myBuf",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Writes to file from internal buffer of another evioChannel object.
 * @param channel Channel object
 */
void evioFileChannel::write(const evioChannel &channel) throw(evioException) {
  if(handle==0)throw(evioException(0,"evioFileChannel::write...0 handle",__FILE__,__FUNCTION__,__LINE__));
  if(evWrite(handle,channel.getBuffer())!=0) throw(evioException(0,"?evioFileChannel::write...unable to write from channel",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Writes from internal buffer of another evioChannel object.
 * @param channel Pointer to channel object
 */
void evioFileChannel::write(const evioChannel *channel) throw(evioException) {
  if(channel==NULL)throw(evioException(0,"evioFileChannel::write...null channel",__FILE__,__FUNCTION__,__LINE__));
  evioFileChannel::write(*channel);
}


//-----------------------------------------------------------------------


/**
 * Writes from contents of evioChannelBufferizable object.
 * @param o evioChannelBufferizable object that can serialze to buffer
 */
void evioFileChannel::write(const evioChannelBufferizable &o) throw(evioException) {
  if(handle==0)throw(evioException(0,"evioFileChannel::write...0 handle",__FILE__,__FUNCTION__,__LINE__));
  o.toEVIOBuffer(buf,bufSize);
  evioFileChannel::write();
}


//-----------------------------------------------------------------------


/**
 * Writes from contents of evioChannelBufferizable object.
 * @param o Pointer to evioChannelBufferizable object that can serialize to buffer
 */
void evioFileChannel::write(const evioChannelBufferizable *o) throw(evioException) {
  if(o==NULL)throw(evioException(0,"evioFileChannel::write...null evioChannel Bufferizable pointer",__FILE__,__FUNCTION__,__LINE__));
  evioFileChannel::write(*o);
}


//-----------------------------------------------------------------------


/**
 * For setting evIoctl parameters.
 * @param request String containing evIoctl parameters
 * @param argp Additional evIoctl parameter
 */
void evioFileChannel::ioctl(const string &request, void *argp) throw(evioException) {
  if(handle==0)throw(evioException(0,"evioFileChannel::ioctl...0 handle",__FILE__,__FUNCTION__,__LINE__));
  if(evIoctl(handle,const_cast<char*>(request.c_str()),argp)!=0)
    throw(evioException(0,"?evioFileChannel::ioCtl...error return",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Closes channel.
 */
void evioFileChannel::close(void) throw(evioException) {
  if(handle==0)throw(evioException(0,"evioFileChannel::close...0 handle",__FILE__,__FUNCTION__,__LINE__));
  evClose(handle);
  handle=0;
}


//-----------------------------------------------------------------------


/**
 * Returns channel file name.
 * @return String containing file name
 */
string evioFileChannel::getFileName(void) const {
  return(filename);
}


//-----------------------------------------------------------------------


/**
 * Returns channel I/O mode.
 * @return String containing I/O mode
 */
string evioFileChannel::getMode(void) const {
  return(mode);
}


//-----------------------------------------------------------------------


/**
 * Returns pointer to internal channel buffer.
 * @return Pointer to internal buffer
 */
const uint32_t *evioFileChannel::getBuffer(void) const throw(evioException) {
  if(buf==NULL)throw(evioException(0,"evioFileChannel::getbuffer...null buffer",__FILE__,__FUNCTION__,__LINE__));
  return(buf);
}


//-----------------------------------------------------------------------



/**
 * Returns internal channel buffer size.
 * @return Internal buffer size in 4-byte words
 */
int evioFileChannel::getBufSize(void) const {
  return(bufSize);
}


//-----------------------------------------------------------------------
