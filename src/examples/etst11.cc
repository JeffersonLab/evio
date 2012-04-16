//  example program reads and writes to buffer

//  ejw, 12-apr-2012




#include "evioUtil.hxx"
#include "evioBufferChannel.hxx"


using namespace evio;



//-------------------------------------------------------------------------------


int main(int argc, char **argv) {
  
  float x[10] = {1,2,3,4,5,6,7,8,9,10};
  string dictXML = "<dict>\n"
    "<dictEntry name=\"fred\"   tag=\"1\" num=\"0\"/> \n"
    "<dictEntry name=\"wilma\"  tag=\"4\" num=\"11\"/>\n"
    "<dictEntry name=\"barney\" tag=\"5\" num=\"12\"/>\n"
    "<dictEntry name=\"betty\"  tag=\"6\" num=\"13\"/>\n"
    "<bank name=\"b1\"     tag=\"7\" num=\"14\">\n"
    "<bank name=\"b2\"    tag=\"8\" num=\"15\">\n"
    "<leaf name=\"l21\"   tag=\"9\" num=\"16\"/>\n"
    "<leaf name=\"l22\"        tag=\"10\" num=\"17\"/>\n"
    "</bank>\n"
    "<leaf name=\"l13\"        tag=\"11\" num=\"18\"/>\n"
    "</bank>\n"
    "</dict>\n";


  evioBufferChannel * chan;

  try {

    // create dictionary
    evioDictionary dict(dictXML);

    // create stream buffer
    int bufLen=100000;
    uint32_t buf[bufLen];


    // create buffer channel, write to it, then close
    chan = new evioBufferChannel(buf,bufLen,"w");
    chan->setDictionary(dict);
    chan->open();

    evioDOMTree event1(1, 0);
    event1.addBank(4, 11, x, 4);
    event1.addBank(5, 12, x, 5);
    event1.addBank(6, 13, x, 6);
    event1.addBank(7, 14, x, 2);
    event1.addBank(8, 15, x, 3);
    event1.addBank(9, 16, x, 4);
    event1.addBank(10, 17, x, 8);
    event1.addBank(11, 18, x, 7);
    cout << "initial event: " << endl << event1.toString(dict) << endl << endl;
    chan->write(event1);

    chan->close();
    delete(chan);

    evioDOMNodeListP l1 = event1.root->getChildren();
    cout << "l1 size is " << l1->size() << endl;
    evioDOMNodeListP l2 = event1.root->getChildren(tagNumEquals(4,11));
    cout << "l2 size is " << l2->size() << endl;
    cout << endl << endl;
    

    // create buffer channel, read from it, then close
    chan = new evioBufferChannel(buf,bufLen,"r");
    chan->open();

    chan->read();
    evioDOMTree event2(chan);
    cout << "final event: " << endl << event2.toString() << endl << endl;

    chan->close();
    delete(chan);



  } catch (evioException e) {
    cerr << e.toString() << endl;

  } catch (...) {
    cerr << "?unknown exception" << endl;
  }
  
}


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
