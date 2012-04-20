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
    
    // just add banks containing doubles to event tree
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
    
    
    // read event from channel, then stream parse the event
    // create event tree, will get filled by parser
    // container node and leaf handlers called as appropriate for each bank in the event
    while(chan->read()) {
      evioDOMTree event(1,0);
      evioStreamParser p;
      myHandler h;
      p.parse(chan->getBuffer(),h,((void*)(&event)));
      cout << event.toString() << endl;
    }    
    
    
  } catch (evioException e) {
    cerr << e.toString() << endl;
    exit(EXIT_FAILURE);
  }
  
}


//--------------------------------------------------------------
//--------------------------------------------------------------
