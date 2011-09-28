//  Example program creates simple fake events using EVIO C++ library
//  Events consist of a root node with a single level of leaf nodes below

//  ejw, 14-nov-2006



#define NEVFAKE 1


#include <vector>
#include "evioFileChannel.hxx"
#include "evioUtil.hxx"

using namespace evio;
using namespace std;


// misc
uint8_t num;
uint16_t tag;
int len;
vector<uint32_t> uivec;
vector<float> fvec;
vector<string> svec;
vector<string> svec2;

int32_t ibuf[100];
int64_t lbuf[100];
double dbuf[100];

string sa[3] = {"I", "am", "bored"};;
string sca[3] = {"in", "the", "end"};;

const char *ca[3];



int main(int argc, char **argv) {
  
  // fill fake data buffers
  for(int i=0; i<10; i++) {
    uivec.push_back((uint32_t)i);
    ibuf[i]=-i;
    lbuf[i]=2*i;
    dbuf[i]=10.*i;
    fvec.push_back((float)i/10.);
    svec.push_back("hello");
    svec2.push_back("goodbye");
  }
  for(int i=0; i<3; i++) ca[i]=sca[i].c_str();
  char *fred="fred";

  
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
      event.addBank(tag=2, num=9,  uivec);
      event.addBank(tag=3, num=10, ibuf,  len=8);
      event.addBank(tag=4, num=11, dbuf,  len=6);
      event.addBank(tag=5, num=12, fvec);
      event.addBank(tag=6, num=13, dbuf,  len=10);
      event.addBank(tag=7, num=14, lbuf,  len=8);
      event.addBank(tag=8, num=15, svec);
      
      evioDOMNodeP sbank = evioDOMNode::createEvioDOMNode<string>(tag=9,num=16);
      event << sbank;

      sbank->append(string("abcdef"));
      sbank->append("ghijkl");
      *sbank << string("goodbye");
      //      *sbank << svec;
      //      sbank->append(svec2);
      *sbank << "mnopqrs";
      sbank->append(sa,3);
      sbank->append(ca,3);
      *sbank << fred;


      evioDOMNodeP tbank = evioDOMNode::createEvioDOMNode<string>(tag=10,num=17);
      event << tbank;

      *tbank << "1234";


      cout << event.toString() << endl;

      //  write out event tree
      chan.write(event);
    }


    // close file
    chan.close();

    
  } catch (evioException e) {
    cerr << e.toString() << endl;
  }


  // done
  cout << "fakeEvents.dat created" << endl;
}


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
