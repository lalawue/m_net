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

#define kMultiChannCount 256    // default for 'ulimit -n'

using std::cout;
using std::endl;
using std::string;

using mnet::Chann;
using mnet::ChannDispatcher;


class SvrChann : public Chann {
public:
   SvrChann(Chann *c, int idx) : Chann(c) { m_idx = idx; }
   
   void defaultEventHandler(Chann *accept, chann_event_t event, int err) {
      if (event == CHANN_EVENT_RECV) {
         rw_result_t *rw = channRecv(m_buf, sizeof(m_buf));
         int ret = snprintf(&m_buf[rw->ret-1], sizeof(m_buf)-rw->ret, ". Echo.");
         channSend(m_buf, ret);
      }
      else if (event == CHANN_EVENT_DISCONNECT) {
         cout << "cnt " << m_idx << " disconnect" << endl;
         delete this;
      }
   }
   int m_idx;
   char m_buf[64];
};


class CntChann : public Chann {
public:
   CntChann(string streamType) : Chann(streamType) {}

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

   if (argc < 2) {
      cout << "Usage: " << argv[0] << " [-s|-c] [ip:port]" << endl;
      return 0;
   }

   string option = argv[1];
   string ipaddr = argc > 2 ? argv[2] : "127.0.0.1:8090";


   if (option == "-s") {
      // server side

      Chann svrListen("tcp");

      if ( svrListen.channListen(ipaddr) ) {
         cout << "svr listen " << ipaddr << endl;

         static int cntCount = 0;
         svrListen.setEventHandler([](Chann *self, Chann *accept, chann_event_t event, int err){
               if (event == CHANN_EVENT_ACCEPT) {
                  cntCount += 1;
                  SvrChann *nc = new SvrChann(accept, cntCount);
                  cout << "accept cnt " << nc << ", count " << cntCount << endl;
                  delete accept;
               }
            });

         ChannDispatcher::startEventLoop();
      }
   }
   else if (option == "-c") {
      // client side

      for (int i=0; i<kMultiChannCount; i++) {
         CntChann *cnt = new CntChann("tcp");
         cnt->m_idx = i;
         if ( cnt->channConnect(ipaddr) ) {
            usleep(10);            // for system resources busy
         } else {
            delete cnt;
         }
      }

      while (ChannDispatcher::pullEvent(1000)->chann_count > 0) {
         cout << mnet_report(0) << endl;
      }

      cout << "\nall cnt connected, send, recv done, exit !" << endl;
   }

   mnet_fini();

   return 0;
}

#endif // TEST_MULTICHANNS
