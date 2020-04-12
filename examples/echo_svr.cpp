/* 
 * Copyright (c) 2017 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <iostream>
#include <unistd.h>
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
         rw_result_t *rw = channRecv(m_buf, 256);
         channSend(m_buf, rw->ret);
      }
      else if (event == CHANN_EVENT_DISCONNECT) {
         usleep(1000);
         cout << "svr disconnect cnt with chann " << this->myAddr().addrString << endl;
         delete this;           // release chann
      }
      else if (event == CHANN_EVENT_TIMER) {
         cout << "svr current time: " << ChannDispatcher::currentTime() << endl;
      }
   }
   char m_buf[256];
};

int main(int argc, char *argv[]) {
   string ipaddr = argc > 1 ? argv[1] : "127.0.0.1:8090";

   Chann echoSvr("tcp");

   if ( echoSvr.channListen(ipaddr, 100) ) {
      cout << "svr start listen: " << ipaddr << endl;

      echoSvr.setEventHandler([](Chann *self, Chann *accept, chann_event_t event, int err) {
                                 if (event == CHANN_EVENT_ACCEPT) {
                                    CntChann *cnt = new CntChann(accept);
                                    char welcome[] = "Welcome to echoServ\n";
                                    cnt->channSend((void*)welcome, sizeof(welcome));
                                    cnt->channActiveEvent(CHANN_EVENT_TIMER, 5 * 1000000);
                                    cout << "svr accept cnt with chann " << cnt->myAddr().addrString << endl;
                                    delete accept;
                                 }
                              });

      while (ChannDispatcher::pollEvent(1000)->chann_count > 0) {
      }
   }
   return 0;
}

#endif  /* EXAMPLE_ECHO */
