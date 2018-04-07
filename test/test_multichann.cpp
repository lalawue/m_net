// 
// 
// Copyright (c) 2017 lalawue
// 
// This library is free software; you can redistribute it and/or modify it
// under the terms of the MIT license. See LICENSE for details.
// 
// 

#ifdef TEST_MULTICHANNS

#include <iostream>
#include <string>
#include <unistd.h>
#include "mnet_wrapper.h"

using std::cout;
using std::endl;
using std::string;

using mnet::Chann;
using mnet::ChannDispatcher;


class SvrChann : public Chann {
public:
   SvrChann(Chann *nc) {
      channUpdate(nc, NULL);
   }
   
   void defaultEventHandler(Chann *accept, chann_event_t event, int err) {
      if (event == CHANN_EVENT_RECV) {
         int ret = channRecv(m_buf, sizeof(m_buf));
         ret = snprintf(&m_buf[ret-1], sizeof(m_buf)-ret, ". Echo.");
         channSend(m_buf, ret);
      }
      else if (event == CHANN_EVENT_DISCONNECT) {
         cout << "cnt " << this << " disconnect" << endl;
         delete this;
      }
   }
   char m_buf[64];
};


class CntChann : public Chann {
public:
   CntChann(string streamType) {
      Chann c(streamType);
      channUpdate(&c, NULL);
   }

   void defaultEventHandler(Chann *accept, chann_event_t event, int err) {
      switch (event) {
         case CHANN_EVENT_CONNECTED: {
            int ret = snprintf(m_buf, sizeof(m_buf), "HelloServ %d", m_idx);
            cout << m_idx << ": connected, send '" << m_buf << "'" << endl;
            channSend(m_buf, ret);
            break;
         }

         case CHANN_EVENT_RECV: {
            channRecv(m_buf, sizeof(m_buf));
            cout << m_idx << ": recv '" << m_buf << "'" << endl;
         }

         default: {             // disconnect
            delete this;
            break;
         }
      }
   }

   int m_idx;
   char m_buf[64];
};

int main(int argc, char *argv[]) {

   if (argc < 3) {
      cout << "Usage: " << argv[0] << " [-s|-c] server_ip:port" << endl;
      return 0;
   }

   string option = argv[1];
   string ipaddr = argv[2];


   if (option == "-s") {
      // server side

      Chann svrListen("tcp");

      if ( svrListen.channListen(ipaddr) ) {
         cout << "svr listen " << ipaddr << endl;

         static int cntCount = 0;
         svrListen.setEventHandler([](Chann *self, Chann *accept, chann_event_t event, int err){
               if (event == CHANN_EVENT_ACCEPT) {
                  cntCount += 1;
                  SvrChann *nc = new SvrChann(accept);
                  cout << "accept cnt " << nc << ", count " << cntCount << endl;
                  delete accept;
               }
            });

         ChannDispatcher::startEventLoop();
      }
   }
   else if (option == "-c") {
      // client side

      for (int i=0; i<4096; i++) {
         CntChann *cnt = new CntChann("tcp");
         cnt->m_idx = i;
         if ( cnt->channConnect(ipaddr) ) {
            usleep(10);            // for system resources busy
         } else {
            delete cnt;
         }
      }

      while (mnet_poll(1000) > 0) {
         cout << mnet_report(0) << endl;
      }

      cout << "\nall cnt connected, send, recv done, exit !" << endl;
   }

   mnet_fini();

   return 0;
}

#endif // TEST_MULTICHANNS
