// tests serialization


// ejw, 23-apr-2012



#define MAXBUFLEN  4096

#include <iostream>
#include <stdio.h>
#include <evioUtil.hxx>
#include "evioFileChannel.hxx"

using namespace evio;
using namespace std;


//--------------------------------------------------------------
//--------------------------------------------------------------


int main(int argc, char **argv) {

  uint32_t buffer[100000];



  try {

    // create and open file channel
    evioFileChannel *chan;
    if(argc>1) {
      chan = new evioFileChannel(argv[1],"r");
    } else {
      chan = new evioFileChannel("fakeEvents.dat","r");
    }
    chan->open();
    
    
    // read events from channel
    while(chan->read()) {
      evioDOMTree event(chan);
      event.toEVIOBuffer(buffer,sizeof(buffer)/sizeof(uint32_t));
      cout << "getSerializedLength:  " << event.getSerializedLength() << endl;
      cout << "event length:  " << (buffer[0]+1) << endl;
    }    
    
    
  } catch (evioException e) {
    cerr << e.toString() << endl;
    exit(EXIT_FAILURE);
  }
  

  cout << "done" << endl;
}


//--------------------------------------------------------------
//--------------------------------------------------------------
