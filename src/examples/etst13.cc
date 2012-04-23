// tests serialization


// ejw, 23-apr-2012



#define MAXBUFLEN  4096

#include <iostream>
#include <stdio.h>
#include <evioUtil.hxx>
#include "evioFileChannel.hxx"
#include "evioBufferChannel.hxx"

using namespace evio;
using namespace std;


//--------------------------------------------------------------
//--------------------------------------------------------------


int main(int argc, char **argv) {

  uint32_t buf[100000];
  uint32_t outbuf[100000];



  try {

    // create and open input file channel
    evioFileChannel *chan1;
    if(argc>1) {
      chan1 = new evioFileChannel(argv[1],"r");
    } else {
      chan1 = new evioFileChannel("fakeEvents.dat","r");
    }
    chan1->open();
    

    // create and open output buffer channel
    evioBufferChannel *chan2 = new evioBufferChannel(outbuf,sizeof(outbuf),"w");
    chan2->open();


    
    // loop over events in file channel
    while(chan1->read()) {

      // create tree from input file channel
      evioDOMTree event(chan1);
      cout << "Event length from input file channel:  " << (chan1->getBuffer()[0]+1) << endl;


      // serialize event only to buffer
      event.toEVIOBuffer(buf,sizeof(buf));
      cout << "Event length after serialization:      " << (buf[0]+1) << endl;


      // write serialized event to output stream buffer channel
      chan2->write(buf);
      cout << "Event length in stream buffer:         " << chan2->getBufLength() << endl;
    }    
    

    // done
    chan1->close();
    chan2->close();
    
  } catch (evioException e) {
    cerr << e.toString() << endl;
    exit(EXIT_FAILURE);
  }
  
}


//--------------------------------------------------------------
//--------------------------------------------------------------
