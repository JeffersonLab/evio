// parses event using stream parser

// builds a tree from selected nodes
// builds an index to banks in the event, searches index for particular banks

// in principle more than one bank may have the same evioDictEntry,
//   in this case must use the getRange() method to get them all, see Doxygen doc


// ejw, 1-may-2012



#include <iostream>
#include <stdio.h>
#include "evioUtil.hxx"
#include "evioFileChannel.hxx"
#include "evioBankIndex.hxx"


using namespace evio;
using namespace std;


//--------------------------------------------------------------
//--------------------------------------------------------------


// callbacks for stream parser
class handler: public evioStreamParserHandler {


  // do nothing with container nodes
  void *containerNodeHandler(int bankLength, int containterType, int contentType, unsigned short tag, unsigned char num, 
                             int depth, const uint32_t *bankPointer, int payloadLength, const uint32_t *payload, void *userArg) {
    return(userArg);
  }
  
  
//--------------------------------------------------------------


  void *leafNodeHandler(int bankLength, int containterType, int contentType, unsigned short tag, unsigned char num, 
                        int depth, const uint32_t *bankPointer, int dataLength, const void *data, void *userArg) {
    
    // add banks containing doubles to event tree
    // alternatively, skip the tree entirely and process the data now, or store the data someplace for later processing
    if(contentType==0x8) ((evioDOMTree*)(userArg))->addBank(tag,num,(double*)data,dataLength);

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
    handler h;
    

    // read events (no copy) from channel, then parse them
    // event tree will get filled by parser callbacks with selected banks
    // alternatively, you can just process the data in the callback and skip event trees altogether
    while(chan->readNoCopy()) {

      // create empty tree, then stream parse event and fill tree with selected banks
      evioDOMTree event(1,0);
      p.parse(chan->getNoCopyBuffer(),h,((void*)(&event)));
      cout << endl << event.toString() << endl;


      // create bank index from contents of noCopy buffer
      evioBankIndex bi(chan->getNoCopyBuffer());


      // query the index and get <double> data for some evioDictEntry
      int len;
      evioDictEntry tn(11,21);

      cout << endl << endl << "Count of banks with evioDictEntry " << tn.getTag() << "," << (int)tn.getNum() << " is: " << bi.tagNumCount(tn) << endl;
      const double *d = bi.getData<double>(tn,&len);
      if(d!=NULL) {
        cout << "data length: " << len << endl;
        cout << "some data <double> for evioDictEntry " << tn.getTag() << "," << (int)tn.getNum()<< ":  " << endl;
        for(int i=0; i<min(len,10); i++) cout << d[i] << "  ";
        cout << endl;
      } else {
        cout << "?cannot find <double> data for: " << tn.getTag() << "," << (int)tn.getNum() << endl;
      }
    

      // query the index and get <int32_t> data for some evioDictEntry
      evioDictEntry tn2(32,37);
      cout << endl << endl << "Count of banks with evioDictEntry " << tn2.getTag() << "," << (int)tn2.getNum() << " is: " << bi.tagNumCount(tn2) << endl;
      const int32_t *ii = bi.getData<int32_t>(tn2,&len);
      if(ii!=NULL) {
        cout << "data length: " << len << endl;
        cout << "some data <int32_t> for evioDictEntry " << tn2.getTag() << "," << (int)tn2.getNum()<< ":  " << endl;
        for(int i=0; i<min(len,10); i++) cout << ii[i] << "  ";
        cout << endl;
      } else {
        cout << "?cannot find <int32_t> data for: " << tn2.getTag() << "," << (int)tn2.getNum() << endl;
      }
    }    
    
    
  } catch (evioException e) {
    cerr << e.toString() << endl;
    exit(EXIT_FAILURE);
  }
  
}


//--------------------------------------------------------------
//--------------------------------------------------------------
