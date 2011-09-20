// evioFileChannel.hxx

//  Author:  Elliott Wolin, JLab, 18-feb-2010


#ifndef _evioFileChannel_hxx
#define _evioFileChannel_hxx


#include <stdint.h>
#include <evioChannel.hxx>
#include <evio.h>


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
  evioFileChannel(const string &fileName, const string &mode = "r", int size = 10000) throw(evioException);
  virtual ~evioFileChannel(void);


  void open(void) throw(evioException);
  bool read(void) throw(evioException);
  bool read(uint32_t *myBuf, int length) throw(evioException);
  void write(void) throw(evioException);
  void write(const uint32_t *myBuf) throw(evioException);
  void write(const evioChannel &channel) throw(evioException);
  void write(const evioChannel *channel) throw(evioException);
  void write(const evioChannelBufferizable &o) throw(evioException);
  void write(const evioChannelBufferizable *o) throw(evioException);
  void close(void) throw(evioException);

  const uint32_t *getBuffer(void) const throw(evioException);
  int getBufSize(void) const;

  void ioctl(const string &request, void *argp) throw(evioException);
  string getFileName(void) const;
  string getMode(void) const;


private:
  string filename;    /**<Name of evio file.*/
  string mode;        /**<Open mode, "r" or "w".*/
  int handle;         /**<Internal evio file handle.*/
  uint32_t *buf;      /**<Pointer to internal buffer.*/
  int bufSize;        /**<Size of internal buffer.*/
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

} // namespace evio


#endif
