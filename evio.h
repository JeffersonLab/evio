#ifndef EVIO_H
#define EVIO_H

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct evfilestruct {
  FILE *file;
  int *buf;
  int *next;
  int left;
  int blksiz;
  int blknum;
  int rw;
  int magic;
  int evnum;         /* last events with evnum so far */
  int byte_swapped;
} EVFILE;


int evOpen(char *filename, char *flags, int *handle);
int evRead(int handle, int *buffer, int buflen);
int evGetNewBuffer(EVFILE *a);
int evWrite(int handle,int *buffer);
int evFlush(EVFILE *a);
int evIoctl(int handle,char *request,void *argp);
int evClose(int handle);
int evOpenSearch(int handle, int *b_handle);
int evSearch(int handle, int b_handle, int evn, int *buffer, int buflen, int *size);
int evCloseSearch(int b_handle);

#ifdef	__cplusplus
}
#endif

#endif
