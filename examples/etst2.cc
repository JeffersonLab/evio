//  example program reads and processes EVIO events 

//  ejw, 14-nov-2006




#include "evioUtil.hxx"
#include <vector>

using namespace evio;
using namespace std;



//-------------------------------------------------------------------------------


void myProcessingFunction(const evioDOMNodeP pNode) {

  // can implement arbitrary node processing algorithm here
  cout << hex << left << "content type:  0x" << setw(6) << pNode->getContentType() << "   tag:  0x" << setw(6) << pNode->tag 
       << "   num:  0x" << setw(6) << pNode->num << endl;
  cout << pNode->toString() << endl;
}


//-------------------------------------------------------------------------------


bool myNodeChooser(const evioDOMNodeP pNode) {

  // can implement arbitrary selection criteria here
  return((pNode->tag==2)&&(pNode->num==9));
}


//-------------------------------------------------------------------------------


int main(int argc, char **argv) {
  


  int nread=0;
  try {
    evioFileChannel chan("fakeEvents.dat","r");
    chan.open();
    

    // loop over all events in file
    while(chan.read()) {

      cout << endl << " --- processing event " << ++nread << " ---" << endl;


      // create event tree from channel contents
      evioDOMTree event(chan);


      // get lists of pointers to various types of nodes in event tree
      // note:  there are many functions available to select nodes, and it is easy to 
      //        write additional ones...contact EJW or see the EVIO manual for more information
      evioDOMNodeListP fullList   = event.getNodeList();
      evioDOMNodeListP longList   = event.getNodeList(typeIs<long>());
      evioDOMNodeListP floatList  = event.getNodeList(typeIs<float>());
      evioDOMNodeListP doubleList = event.getNodeList(typeIs<double>());
      evioDOMNodeListP myList     = event.getNodeList(myNodeChooser);


      // apply myProcessingFunction to all float nodes using STL for_each() algorithm
      cout << endl << endl << "Applying myProcessingFunction to all float nodes:" << endl << endl;
      for_each(floatList->begin(),floatList->end(),myProcessingFunction);
      

      // dump all double nodes to cout using toCout()
      cout << endl << endl<< "Dumping double nodes using toCout:" << endl << endl;
      for_each(doubleList->begin(),doubleList->end(),toCout());


      // dump all nodes selected by myNodeChooser
      cout << endl << endl<< "Dumping nodes selected by myNodeChooser using toCout:" << endl << endl;
      for_each(myList->begin(),myList->end(),toCout());


      // print out data from all <long> nodes manually
      cout << endl << endl<< "Dumping long nodes manually:" << endl << endl;
      evioDOMNodeList::const_iterator iter;
      for(iter=longList->begin(); iter!=longList->end(); iter++) {
        cout << "bank tag,type,num are: " << dec << (*iter)->tag << "  " << (*iter)->getContentType() 
             << "  " << (*iter)->num << endl;

        const evioDOMNodeP np = *iter;                              // solaris bug...
        const vector<long> *vec = np->getVector<long>();
        for(int i=0; i<vec->size(); i++) cout  << "   " << (*vec)[i];
        cout << endl;
      }
      cout << endl << endl;
      
      
      // get child list from container node
      evioDOMNodeList *list = event.root->getChildList();
      cout << "Root child list length is " << list->size() << endl;
      cout << endl << endl;
    }
      

    // done...close channel
    chan.close();
    

  } catch (evioException e) {
    cerr << e.toString() << endl;
  }
  
}


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
