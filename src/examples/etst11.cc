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
    event1.addBank(4, 11, x, 10);
    event1.addBank(5, 12, x, 5);
    cout << "initial event: " << endl << event1.toString(dict) << endl << endl;
    chan->write(event1);

    chan->close();
    delete(chan);



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
