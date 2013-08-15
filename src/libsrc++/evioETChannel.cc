/*
 *  evioETChannel.cc
 *
 *   Author:  Elliott Wolin, JLab, 27-Nov-2012
*/


// still to do:
//   more than one event in et buffer, use evio buffer functions
//   dictionary
//   const getBuffer()?
//   setChunkSize() and realloc?



#include <stdio.h>
#include "evioETChannel.hxx"


using namespace std;
using namespace evio;



//-----------------------------------------------------------------------
//------------------------- evioETChannel -----------------------------
//-----------------------------------------------------------------------


/**
 * Constructor opens ET channel for reading or writing, assumes ET connection already established.
 * User must close connection externally.
 * @param et_system_id ET system id
 * @param et_attach_id ET station id
 * @param m I/O mode, "r" or "rw" or "w"
 * @param chunk How many ET buffers to fetch at one go
 */
evioETChannel::evioETChannel(et_sys_id et_system_id, et_att_id et_attach_id, const string &m, int chunk, int etmode) throw(evioException) 
  : evioChannel(), et_system_id(et_system_id), et_attach_id(et_attach_id), mode(m), chunk(chunk), et_mode(etmode),
    bufferXMLDictionary(""), createdBufferDictionary(false), etBufReceived(0), etBufUsed(0) {

  if(et_system_id==NULL)throw(evioException(0,"?evioETChannel constructor...NULL system id",__FILE__,__FUNCTION__,__LINE__));
  if(et_attach_id<0)throw(evioException(0,"?evioETChannel constructor...bad station id",__FILE__,__FUNCTION__,__LINE__));
  
  // check chunk
  if(chunk<=0)chunk=1;

  // lowercase mode
  std::transform(mode.begin(), mode.end(), mode.begin(), (int(*)(int)) tolower);  // magic

  // allocate ETBuffers array
  ETBuffers = (et_event **) calloc(chunk, sizeof(et_event *));
  if (ETBuffers == NULL)throw(evioException(0,"?evioETChannel constructor...unable to create ETBuffers array",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Constructor opens ET channel for reading or writing, assumes ET connection already established.
 * User must close connection externally.
 * @param et_system_id ET system id
 * @param et_attach_id ET station id
 * @param dict Dictionary
 * @param m I/O mode, "r" or "rw" or "w"
 * @param chunk How many ET buffers to fetch at one go
 */
evioETChannel::evioETChannel(et_sys_id et_system_id, et_att_id et_attach_id, evioDictionary *dict, 
                             const string &m, int chunk, int etmode) throw(evioException) 
  : evioChannel(dict), et_system_id(et_system_id), et_attach_id(et_attach_id), mode(m), chunk(chunk), et_mode(etmode),
    bufferXMLDictionary(""), createdBufferDictionary(false), etBufReceived(0), etBufUsed(0) {

  if(et_system_id==NULL)throw(evioException(0,"?evioETChannel constructor...NULL system id",__FILE__,__FUNCTION__,__LINE__));
  if(et_attach_id<0)throw(evioException(0,"?evioETChannel constructor...bad station id",__FILE__,__FUNCTION__,__LINE__));
  
  // check chunk
  if(chunk<=0)chunk=1;

  // lowercase mode
  std::transform(mode.begin(), mode.end(), mode.begin(), (int(*)(int)) tolower);  // magic

  // allocate ETBuffers array
  ETBuffers = (et_event **) calloc(chunk, sizeof(et_event *));
  if(ETBuffers==NULL)throw(evioException(0,"?evioETChannel constructor...unable to create ETBuffers array",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Destructor.
 */
evioETChannel::~evioETChannel(void) {
  close();
  if(createdBufferDictionary)delete(dictionary), dictionary=NULL;
}


//-----------------------------------------------------------------------


/**
 * Does nothing.
 */
void evioETChannel::open(void) throw(evioException) {
}


//-----------------------------------------------------------------------


/**
 * Gets an event from set of ET buffers.
 * Gets another chunk when needed.
 * Note that an ET buffer may contain more than one event.
 * Must return buffers when no longer needed
 * 
 * @return true if successful, false if can't get event from ET
 */
bool evioETChannel::read(void) throw(evioException) {
  if((mode!="r")&&(mode!="rw"))throw(evioException(0,"evioETChannel::read...incorrect mode",__FILE__,__FUNCTION__,__LINE__));

  // get another event from ET buffer
  //  ??? only one event per ET buffer for the moment ???
  

  // no more events in buffer, move to next buffer, get more buffers if none left
  if(etBufReceived>etBufUsed) {
    etBufUsed++;
  } else {
    etBufUsed=0;
    if(etBufReceived>0)et_events_dump(et_system_id, et_attach_id, ETBuffers, etBufReceived);
    if((et_events_get(et_system_id, et_attach_id, ETBuffers, et_mode, NULL, chunk, &etBufReceived)!=ET_OK) || 
       (etBufReceived<=0))return(false);
    etBufUsed=1;
  }


  // use first event in current ET buffer
  return(true);

}


//-----------------------------------------------------------------------


/**
 * Reads one event from ET buffer, copies into user-supplied buffer.
 * @param myBuf User-supplied buffer.
 * @parem length Length of buffer in 4-byte words.
 * @return true if successful, false otherwise
 */
bool evioETChannel::read(uint32_t *myBuf, int length) throw(evioException) {
  if(myBuf==NULL)throw(evioException(0,"evioETChannel::read...null user buffer",__FILE__,__FUNCTION__,__LINE__));
  if((mode!="r")&&(mode!="rw"))throw(evioException(0,"evioETChannel::read...incorrect mode",__FILE__,__FUNCTION__,__LINE__));

  if(read()) {
    if(getBufSize()<=length)throw(evioException(0,"evioETChannel::read...user buffer not big enough",__FILE__,__FUNCTION__,__LINE__));
    memcpy((void*)myBuf,(void*)getBuffer(),getBufSize()*sizeof(uint32_t));
    return(true);
  } else {
    return(false);
  }
}


//-----------------------------------------------------------------------


/**
 * Reads from ET
 * @param buffer Pointer to pointer to allocated buffer.
 * @param len Length of buffer in 4-byte words.
 * @return true if successful, false on error.
 *
 * Note:  user MUST free the allocated buffer!
 *
 */

bool evioETChannel::readAlloc(uint32_t **buffer, uint32_t *bufLen) throw(evioException) {
  if((mode!="r")&&(mode!="rw"))throw(evioException(0,"evioETChannel::readAlloc...incorrect mode",__FILE__,__FUNCTION__,__LINE__));
  if(read()) {
    *buffer=(uint32_t*)malloc(getBufSize()*sizeof(uint32_t));
    *bufLen=getBufSize();
    memcpy((void*)*buffer,(void*)getBuffer(),getBufSize()*sizeof(uint32_t));
  } else {
    return(false);
  }
}


//-----------------------------------------------------------------------


/**
 * Reads from ET.
 * @return true if successful, false on EOF, throws exception for other error.
 */
bool evioETChannel::readNoCopy(void) throw(evioException) {
  if((mode!="r")&&(mode!="rw"))throw(evioException(0,"evioETChannel::readNoCopy...incorrect mode",__FILE__,__FUNCTION__,__LINE__));
  return(read());
}


//-----------------------------------------------------------------------


/**
 * Writes to buffer from internal buffer.
 */
void evioETChannel::write(void) throw(evioException) {
  if((mode!="rw")&&(mode!="w"))throw(evioException(0,"evioETChannel::write...incorrect mode",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Writes to buffer from user-supplied buffer.
 * @param myBuf Buffer containing event
 */
void evioETChannel::write(const uint32_t *myBuf) throw(evioException) {
  if(myBuf==NULL)throw(evioException(0,"evioETChannel::write...null myBuf",__FILE__,__FUNCTION__,__LINE__));
  if((mode!="rw")&&(mode!="w"))throw(evioException(0,"evioETChannel::write...incorrect mode",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Writes to buffer from internal buffer of another evioChannel object.
 * @param channel Channel object
 */
void evioETChannel::write(const evioChannel &channel) throw(evioException) {
  if((mode!="rw")&&(mode!="w"))throw(evioException(0,"evioETChannel::write...incorrect mode",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Writes from internal buffer of another evioChannel object.
 * @param channel Pointer to channel object
 */
void evioETChannel::write(const evioChannel *channel) throw(evioException) {
  if(channel==NULL)throw(evioException(0,"evioETChannel::write...null channel",__FILE__,__FUNCTION__,__LINE__));
  evioETChannel::write(*channel);
}


//-----------------------------------------------------------------------


/**
 * Writes from contents of evioChannelBufferizable object.
 * @param o evioChannelBufferizable object that can serialze to buffer
 */
void evioETChannel::write(const evioChannelBufferizable &o) throw(evioException) {
  if((mode!="rw")&&(mode!="w"))throw(evioException(0,"evioETChannel::write...incorrect mode",__FILE__,__FUNCTION__,__LINE__));
}


//-----------------------------------------------------------------------


/**
 * Writes from contents of evioChannelBufferizable object.
 * @param o Pointer to evioChannelBufferizable object that can serialize to buffer
 */
void evioETChannel::write(const evioChannelBufferizable *o) throw(evioException) {
  if(o==NULL)throw(evioException(0,"evioETChannel::write...null evioChannel Bufferizable pointer",__FILE__,__FUNCTION__,__LINE__));
  if((mode!="rw")&&(mode!="w"))throw(evioException(0,"evioETChannel::write...incorrect mode",__FILE__,__FUNCTION__,__LINE__));
  evioETChannel::write(*o);
}


//-----------------------------------------------------------------------


/**
 * For getting and setting evIoctl parameters.
 * @param request String containing evIoctl parameters
 * @param argp Additional evIoctl parameter
 */
int evioETChannel::ioctl(const string &request, void *argp) throw(evioException) {
  return(0);
}


//-----------------------------------------------------------------------


/**
 * Returns all events.
 */
void evioETChannel::close(void) throw(evioException) {
  if(etBufReceived>0)et_events_dump(et_system_id, et_attach_id, ETBuffers, etBufReceived);
  etBufReceived=0;
}


//-----------------------------------------------------------------------


/**
 * Returns channel I/O mode.
 * @return String containing I/O mode
 */
string evioETChannel::getMode(void) const {
  return(mode);
}


//-----------------------------------------------------------------------


/**
 * Returns pointer to event in current ET buffer.
 * @return Pointer to event in current ET buffer
 */
const uint32_t *evioETChannel::getBuffer(void) const throw(evioException) {
  if(etBufReceived<=0)throw(evioException(0,"evioETChannel::getBuffer...no buffer available",__FILE__,__FUNCTION__,__LINE__));
  if(ETBuffers[etBufUsed-1]==NULL)throw(evioException(0,"evioETChannel::getBuffer...null et buffer",__FILE__,__FUNCTION__,__LINE__));


  // get pointer to data in et buffer
  uint32_t *pdata;
  if(et_event_getdata(ETBuffers[etBufUsed-1],(void**)&pdata)!=ET_OK)
    throw(evioException(0,"evioETChannel::getBuffer...error return from et_event_getdata",__FILE__,__FUNCTION__,__LINE__));
  if(pdata==NULL)throw(evioException(0,"evioETChannel::getBuffer...null pdata",__FILE__,__FUNCTION__,__LINE__));

  // ??? will use evio buffer package when it is ready ???
  return(pdata+pdata[2]);
}


//-----------------------------------------------------------------------


/**
 * Returns current ET event size.
 * @return ET event size in 4-byte words
 */
int evioETChannel::getBufSize(void) const {
  return(getBuffer()[0]+1);
}


//-----------------------------------------------------------------------


/**
 * Returns pointer to internal buffer
 * @return Just returns pointer to internal buffer since no copy buf doesn't exist.
 */
const uint32_t *evioETChannel::getNoCopyBuffer(void) const throw(evioException) {
  return(getBuffer());
}


//-----------------------------------------------------------------------


/**
 * Returns XML dictionary read in from buffer
 * @return XML dictionary read in from buffer
 */
string evioETChannel::getBufferXMLDictionary(void) const {
  return(bufferXMLDictionary);
}


//-----------------------------------------------------------------------


/**
 * Gets chunk size
 * @return chunk size
 */
int evioETChannel::getChunkSize(void) const {
  return(chunk);
}


//-----------------------------------------------------------------------
