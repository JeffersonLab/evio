//  Example program creates simple fake events using EVIO C++ library
//  Events consist of a root node with a single level of leaf nodes below
//  Need to use a more complicated API to construct trees of arbitrary depth

//  ejw, 14-nov-2006



#define NEVFAKE 3


#include <vector>
#include "evioUtil.hxx"

using namespace evio;
using namespace std;


// misc
int tag,num,len;
vector<unsigned long> uvec;
vector<float> fvec;
unsigned long ubuf[100];
long lbuf[100];
double dbuf[100];



int main(int argc, char **argv) {
  
  // fill fake data buffers
  for(int i=0; i<10; i++)uvec.push_back(i),lbuf[i]=-i,dbuf[i]=10.*i,fvec.push_back(i/10.);


  try {

    // create fileChannel object for writing
    evioFileChannel chan("fakeEvents.dat","w");


    // open file
    chan.open();
    

    // generate fake events
    for(int i=0; i<NEVFAKE; i++) {

      // create an event tree, root node has (tag=1,num=0)
      evioDOMTree event(tag=1, num=0);

      // add banks to event in a single level below root node
      event.addBank(tag=2, num=9,  uvec);
      event.addBank(tag=3, num=10, lbuf,  len=8);
      event.addBank(tag=4, num=11, dbuf,  len=6);
      event.addBank(tag=5, num=12, fvec);
      event.addBank(tag=6, num=13, dbuf,  len=10);

      //  write out event tree
      chan.write(event);
    }


    // close file
    chan.close();

    
  } catch (evioException e) {
    cerr << e.toString() << endl;
  }


  // done
  cout << endl << "   *** fake event data file created ***" << endl << endl;
}


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
