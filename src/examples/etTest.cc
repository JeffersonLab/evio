//  etTest.cc

//  example program tests evioETChannel

// to run:  etTest node_name et_file_name chunk_size

//  ejw, 5-Dec-2012



extern "C" {
#include "et.h"
}
#include "evioUtil.hxx"
#include "evioETChannel.hxx"


using namespace evio;



//-------------------------------------------------------------------------------


int main(int argc, char **argv) {
  
  et_openconfig   openconfig;
  et_sys_id       et_system_id;
  et_stat_id      et_station_id;
  et_att_id       et_attach_id;


  try {

    // connect to ET
    et_open_config_init(&openconfig);
    et_open_config_setcast(openconfig, ET_DIRECT);
    et_open_config_setwait(openconfig, ET_OPEN_WAIT);
    et_open_config_sethost(openconfig, argv[1]);

    

    // open et system
    if(et_open(&et_system_id, argv[2], openconfig)!=ET_OK) {
      cerr << "?unable to open et file " << argv[2] << endl;
      exit(EXIT_FAILURE);
    }
    et_open_config_destroy(openconfig);

    
    // create station
    if(et_station_create(et_system_id,&et_station_id,"testStation",NULL)!=ET_OK) {
      cerr << "?unable to create testStation" << endl;
      exit(EXIT_FAILURE);
    }


    // attach to station
    if(et_station_attach(et_system_id, et_station_id, &et_attach_id)<0) {
      cerr << "?unable to attach to testStation" << endl;
      exit(EXIT_FAILURE);
    }



    // create and open ET channel
    evioETChannel chan(et_system_id,et_attach_id,"r",atoi(argv[3]),ET_SLEEP);
    chan.open();


    // get events
    int n=0;
    while(chan.read()) {
      n++;
      evioDOMTree event(chan);
      if(n%100000==1)cout << event.toString();
    }
    

    // done
    chan.close();
    et_close(et_system_id);


  } catch (evioException e) {
    cerr << e.toString() << endl;

  } catch (...) {
    cerr << "?unknown exception" << endl;
  }
  
}


//-------------------------------------------------------------------------------
//-------------------------------------------------------------------------------
