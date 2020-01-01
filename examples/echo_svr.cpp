/* 
 * Copyright (c) 2017 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <iostream>
#include "mnet_wrapper.h"

#ifdef EXAMPLE_ECHO_SVR

using std::cout;
using std::endl;
using std::string;
using mnet::Chann;
using mnet::ChannDispatcher;

// subclass Chann to handle event
class CntChann : public Chann {
public:
   CntChann(Chann *c) : Chann(c) {}

   // implement virtual defaultEventHandler
   void defaultEventHandler(Chann *accept, chann_event_t event, int err) {

      if (event == CHANN_EVENT_RECV) {
         rw_result *rw = channRecv(m_buf, 256);
         channSend(m_buf, rw->ret);
      }
      if (event == CHANN_EVENT_DISCONNECT) {
         cout << "svr disconnect cnt with chann " << this->myAddr().addrString << endl;         
         delete this;           // release chann
      }
   }
   char m_buf[256];
};

int main(int argc, char *argv[]) {
   if (argc < 2) {
      cout << argv[0] << ": 'svr_ip:port'" << endl;
   } else {
      Chann echoSvr("tcp");

      if ( echoSvr.channListen(argv[1]) ) {
         cout << "svr start listen: " << argv[1] << endl;

         echoSvr.setEventHandler([](Chann *self, Chann *accept, chann_event_t event, int err) {
               if (event == CHANN_EVENT_ACCEPT) {
                  CntChann *cnt = new CntChann(accept);
                  char welcome[] = "Welcome to echoServ\n";
                  cnt->channSend((void*)welcome, sizeof(welcome));
                  cout << "svr accept cnt with chann " << cnt->myAddr().addrString << endl;
                  delete accept;
               }
            });

         ChannDispatcher::startEventLoop();
      }
   }
   return 0;
}

#endif  /* EXAMPLE_ECHO */
