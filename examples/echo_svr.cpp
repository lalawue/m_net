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
   CntChann(Chann *c) { channUpdate(c, NULL); }

   // implement virtual defaultEventHandler
   void defaultEventHandler(Chann *accept, chann_event_t event, int err) {

      if (event == CHANN_EVENT_RECV) {
         memset(m_buf, 0, 256);
         int ret = channRecv(m_buf, 256);
         channSend(m_buf, ret);
      }
      if (event == CHANN_EVENT_DISCONNECT) {
         delete this;           // release chann
      }
   }
   char m_buf[256];
};

int main(int argc, char *argv[]) {
   if (argc < 2) {
      cout << argv[0] << ": 'svr_ip:port'" << endl;
   } else {
      cout << "svr start listen: " << argv[1] << endl;

      Chann echoSvr("tcp");
      echoSvr.channListen(argv[1]);
   
      echoSvr.setEventHandler([](Chann *self, Chann *accept, chann_event_t event, int err) {
            if (event == CHANN_EVENT_ACCEPT) {
               CntChann *cnt = new CntChann(accept);
               cnt->channSend((void*)("Welcome to echoServ\n"), 20);
               delete accept;
            }
         });
      ChannDispatcher::startEventLoop();
   }
   return 0;
}

#endif  /* EXAMPLE_ECHO */
