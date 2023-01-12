// evioFileChannel.hxx

//  Author:  Elliott Wolin, JLab, 18-feb-2010


#ifndef _evioFileChannel_hxx
#define _evioFileChannel_hxx


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
 * Implements evioChannel functionality for I/O to and from files.
 * Basically a wrapper around the original evio C library.
 */
class evioFileChannel : public evioChannel {

public:
  evioFileChannel(const string &fileName, const string &mode = "r", int size = 1000000) ;
  evioFileChannel(const string &fileName, evioDictionary *dict, const string &mode = "r", int size = 1000000) ;
  virtual ~evioFileChannel(void);


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

  string getFileName(void) const;
  string getMode(void) const;
  string getFileXMLDictionary(void) const;


private:
  string filename;            /**<Name of evio file.*/
  string mode;                /**<Open mode, "r" or "ra" or "w" or "a".*/
  int handle;                 /**<Internal evio handle.*/
  uint32_t *buf;              /**<Pointer to internal event buffer.*/
  int bufSize;                /**<Size of internal event buffer.*/
  const uint32_t *noCopyBuf;  /**<Pointer to no copy event buffer.*/
  const uint32_t *randomBuf;  /**<Pointer to random read buffer.*/
  string fileXMLDictionary;   /**<XML dictionary in file.*/
  bool createdFileDictionary; /**<true if internally created new dictionary from file.*/
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

} // namespace evio


#endif
