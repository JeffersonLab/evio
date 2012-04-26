// parses event using stream parser

// builds a tree with just selected nodes


// ejw, 20-apr-2012



#define MAXBUFLEN  4096

#include <iostream>
#include <stdio.h>
#include <evioUtil.hxx>
#include "evioFileChannel.hxx"

using namespace evio;
using namespace std;


//--------------------------------------------------------------
//--------------------------------------------------------------


class myHandler: public evioStreamParserHandler {


  void *containerNodeHandler(int length, unsigned short tag, int contentType, unsigned char num, 
                             int depth, void *userArg) {
    return(userArg);
  }
  
  
//--------------------------------------------------------------


  void *leafNodeHandler(int length, unsigned short tag, int contentType, unsigned char num, 
                        int depth, const void *data, void *userArg) {
    
    // adds selected banks (e.g. containing doubles) to event tree
    // alternatively, skip the tree and process the data now, or store the data someplace for later processing
    if(contentType==0x8) ((evioDOMTree*)(userArg))->addBank(tag,num,(double*)data,length);

    return(userArg);
  } 

};


//--------------------------------------------------------------
//--------------------------------------------------------------


int main(int argc, char **argv) {

  try {

    // create and open file channel
    evioFileChannel *chan;
    if(argc>1) {
      chan = new evioFileChannel(argv[1],"r");
    } else {
      chan = new evioFileChannel("fakeEvents.dat","r");
    }
    chan->open();
    
    
    // create parser and handler
    evioStreamParser p;
    myHandler h;


    // read events (no copy) from channel, then stream parse them
    // event tree will get filled by parser with selected banks
    // alternatively, you can just process the data in the callback and skip event trees altogether
    // container node and leaf handlers called as appropriate for each bank in the event
    while(chan->readNoCopy()) {
      evioDOMTree event(1,0);
      p.parse(chan->getNoCopyBuffer(),h,((void*)(&event)));
      cout << event.toString() << endl;
    }    
    
    
  } catch (evioException e) {
    cerr << e.toString() << endl;
    exit(EXIT_FAILURE);
  }
  
}


//--------------------------------------------------------------
//--------------------------------------------------------------
