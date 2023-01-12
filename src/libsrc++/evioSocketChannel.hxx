// evioSocketChannel.hxx

//  Author:  Elliott Wolin, JLab, 12-Apr-2012


#ifndef _evioSocketChannel_hxx
#define _evioSocketChannel_hxx


#include <iostream>
#include <stdint.h>
#include "evioChannel.hxx"
#include "evioUtil.hxx"
#include "evio.h"


using namespace std;


namespace evio {


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Implements evioChannel functionality for I/O to and from socket.
 */
class evioSocketChannel : public evioChannel {

public:
  evioSocketChannel(int socFd, const string &mode = "r", int size=100000) ;
  evioSocketChannel(int socFd, evioDictionary *dict, const string &mode = "r", int size=100000) ;
  virtual ~evioSocketChannel(void);

  void open(void) ;

  bool read(void) ;
  bool read(uint32_t *myEventBuf, int length) ;
  bool readAlloc(uint32_t **buffer, uint32_t *bufLen) ;
  bool readNoCopy(void) ;

  void write(void) ;
  void write(const uint32_t *myEventBuf) ;
  void write(const evioChannel &channel) ;
  void write(const evioChannel *channel) ;
  void write(const evioChannelBufferizable &o) ;
  void write(const evioChannelBufferizable *o) ;

  void close(void) ;
  int ioctl(const string &request, void *argp) ;

  const uint32_t *getBuffer(void) const ;
  int getBufSize(void) const;
  const uint32_t *getNoCopyBuffer(void) const ;

  string getMode(void) const;
  int getSocketFD(void) const ;
  string getSocketXMLDictionary(void) const;


private:
  int sockFD;                     /**<Socket file descriptor.*/
  string mode;                    /**<Open mode, "r" or "w".*/
  int handle;                     /**<Internal evio handle.*/
  uint32_t *buf;                  /**<Pointer to internal event socket.*/
  int bufSize;                    /**<Size of internal socket.*/
  const uint32_t *noCopyBuf;      /**<Pointer to no copy buffer.*/
  string socketXMLDictionary;     /**<XML dictionary in socket.*/
  bool createdSocketDictionary;   /**<true if created dictionary from socket.*/
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

} // namespace evio


#endif
