// evioBufferChannel.hxx

//  Author:  Elliott Wolin, JLab, 12-Apr-2012


#ifndef _evioBufferChannel_hxx
#define _evioBufferChannel_hxx


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
 * Implements evioChannel functionality for I/O to and from user-supplied evio buffer.
 */
class evioBufferChannel : public evioChannel {

public:
  evioBufferChannel(uint32_t *streamBuf, int bufLen, const string &mode = "r", int size=100000) ;
  evioBufferChannel(uint32_t *streamBuf, int bufLen, evioDictionary *dict, const string &mode = "r", int size=100000) ;
  virtual ~evioBufferChannel(void);


  void open(void) ;

  bool read(void) ;
  bool read(uint32_t *myEventBuf, int length) ;
  bool readAlloc(uint32_t **buffer, uint32_t *bufLen) ;
  bool readNoCopy(void) ;
  bool readRandom(uint32_t bufferNumber) ;

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

  const uint32_t *getRandomBuffer(void) const ;
  void getRandomAccessTable(uint32_t *** const table, uint32_t *len) const ;


  const uint32_t *getStreamBuffer(void) const ;
  int getStreamBufSize(void) const;
  string getMode(void) const;
  uint32_t getEVIOBufferLength(void) const ;
  string getBufferXMLDictionary(void) const;


private:
  uint32_t *streamBuf;           /**<Pointer to user-supplied stream i/o buffer.*/
  int streamBufSize;             /**<Size of user-supplied stream buffer.*/
  string mode;                   /**<Open mode, "r" or "ra" or "w" or "a".*/
  int handle;                    /**<Internal evio handle.*/
  uint32_t *buf;                 /**<Pointer to internal event buffer.*/
  int bufSize;                   /**<Size of internal buffer.*/
  const uint32_t *noCopyBuf;     /**<Pointer to no copy buffer.*/
  const uint32_t *randomBuf;     /**<Pointer to random read buffer.*/
  string bufferXMLDictionary;    /**<XML dictionary in buffer.*/
  bool createdBufferDictionary;  /**<true if dictionary created from buffer.*/
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

} // namespace evio


#endif
