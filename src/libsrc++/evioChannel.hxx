// evioChannel.hxx

//  Author:  Elliott Wolin, JLab, 18-feb-2010


#ifndef _evioChannel_hxx
#define _evioChannel_hxx


#include <stdint.h>
#include <evioException.hxx>


using namespace std;


namespace evio {


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Pure virtual class defines single method enable serialization of 
 * object to evio buffer.  
 **/
class evioChannelBufferizable {

public:
  virtual int toEVIOBuffer(uint32_t *buf, int size) const throw(evioException) = 0;
  virtual ~evioChannelBufferizable() {}
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Virtual class defines EVIO I/O channel functionality.
 * Sub-class gets channel-specific info from constructor and implements 
 * evioChannel methods.
 */
class evioChannel {

public:
  virtual void open(void) throw(evioException) = 0;
  virtual bool read(void) throw(evioException) = 0;
  virtual bool read(uint32_t *myBuf, int length) throw(evioException) = 0;
  virtual void write(void) throw(evioException) = 0;
  virtual void write(const uint32_t* myBuf) throw(evioException) = 0;
  virtual void write(const evioChannel &channel) throw(evioException) = 0;
  virtual void write(const evioChannel *channel) throw(evioException) = 0;
  virtual void write(const evioChannelBufferizable &o) throw(evioException) = 0;
  virtual void write(const evioChannelBufferizable *o) throw(evioException) = 0;
  virtual void close(void) throw(evioException) = 0;
  virtual ~evioChannel(void) {};

  // not sure if these should be part of the interface
  virtual const uint32_t *getBuffer(void) const throw(evioException) = 0;
  virtual int getBufSize(void) const = 0;

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


} // namespace evio


#endif
