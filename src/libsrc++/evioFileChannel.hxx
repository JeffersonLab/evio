// evioFileChannel.hxx

//  Author:  Elliott Wolin, JLab, 18-feb-2010


#ifndef _evioFileChannel_hxx
#define _evioFileChannel_hxx


#include <iostream>
#include <cstdint>
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
  evioFileChannel(const string &fileName, evioDictionary *dict,
                  const string &mode = "r", int size = 1000000) ;
  evioFileChannel(const string &fileName, evioDictionary *dict, const uint32_t *firstEvent,
                  const string &mode = "w", int size = 1000000) ;
  virtual ~evioFileChannel();


  void open() ;

  bool read() ;
  bool read(uint32_t *myEventBuf, int length) ;
  bool readAlloc(uint32_t **buffer, uint32_t *bufLen) ;
  bool readNoCopy() ;
  bool readRandom(uint32_t bufferNumber) ;

  void write() ;
  void write(const uint32_t *myEventBuf) ;
  void write(const evioChannel &channel) ;
  void write(const evioChannel *channel) ;
  void write(const evioChannelBufferizable &o) ;
  void write(const evioChannelBufferizable *o) ;

  void close() ;

  int ioctl(const string &request, void *argp) ;

  const uint32_t *getBuffer() const ;
  int getBufSize() const;
  const uint32_t *getNoCopyBuffer() const ;
  const uint32_t *getRandomBuffer() const ;
  void getRandomAccessTable(uint32_t *** const table, uint32_t *len) const ;

  string getFileName() const;
  string getMode() const;
  string getFileXMLDictionary() const;


private:
  string filename;            /**<Name of evio file.*/
  string mode;                /**<Open mode, "r" for read, "ra" for random access read,
                                * "w" for write, "a" for append, "s" for splitting while writing.*/
  int handle;                 /**<Internal evio handle.*/
  uint32_t *buf;              /**<Pointer to internal event buffer.*/
  int bufSize;                /**<Size of internal event buffer.*/
  const uint32_t *firstEvent; /**<Pointer first event buffer.*/
  const uint32_t *noCopyBuf;  /**<Pointer to no copy event buffer.*/
  const uint32_t *randomBuf;  /**<Pointer to random read buffer.*/
  string fileXMLDictionary;   /**<XML dictionary in file.*/
  bool createdFileDictionary; /**<true if internally created new dictionary from file.*/
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

} // namespace evio


#endif
