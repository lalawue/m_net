//
//
// Copyright (c) 2017 lalawue
//
// This library is free software; you can redistribute it and/or modify it
// under the terms of the MIT license. See LICENSE for details.
//
//

#ifdef TEST_RECONNECT_CPP

#include <iostream>
#include <string>
#include <unistd.h>
#include "mnet_wrapper.h"

#define kMultiChannCount 256    // default for 'ulimit -n'
#define kTestCount 5            // disconnect/connect count for each chann

using std::cout;
using std::endl;
using std::string;

using mnet::Chann;
using mnet::ChannAddr;
using mnet::ChannDispatcher;


class SvrChann : public Chann {
public:
   SvrChann(Chann *c) : Chann(c) {}

   void defaultEventHandler(Chann *accept, chann_event_t event, int err) {
      if (event == CHANN_EVENT_RECV) {
         int ret = channRecv(m_buf, sizeof(m_buf));
         channActiveEvent(CHANN_EVENT_SEND, 1);
         channSend(m_buf, ret);
      }
      else if (event == CHANN_EVENT_SEND ||
               event == CHANN_EVENT_DISCONNECT)
      {
         cout << "cnt " << this << " disconnect" << endl;
         delete this;
      }
   }
   char m_buf[128];
};


class CntChann : public Chann {
public:
   CntChann(string streamType, int idx) : Chann(streamType) { m_idx = idx; }

   void defaultEventHandler(Chann *accept, chann_event_t event, int err) {
      switch (event) {
         case CHANN_EVENT_CONNECTED: {
            int ret = snprintf(m_buf, sizeof(m_buf), "HelloServ %d", m_idx);
            if (ret == channSend(m_buf, ret)) {
               cout << m_idx << ": connected, send '" << m_buf << "'" << endl;
            } else {
               cout << m_idx << ": connected, fail to send with ret " << ret << endl;
               delete this;
            }
            break;
         }

         case CHANN_EVENT_DISCONNECT: {

            m_test_count += 1;
            if (m_test_count >= kTestCount) {
               delete this;
               return;
            }

            usleep(1000);

            cout << m_idx << ": disconnect, try to connect " << peerAddr().addrString << endl;
            if ( !channConnect( peerAddr().addrString ) ) {
               cout << m_idx << ": disconnect, fail to connect !" << endl;
               delete this;
            }
            break;
         }

         default: {
            break;
         }
      }
   }

   int m_idx;
   char m_buf[128];
   int m_test_count;
};

int main(int argc, char *argv[]) {

   if (argc < 2) {
      cout << "Usage: " << argv[0] << " [-s|-c] [ip:port]" << endl;
      return 0;
   }

   string option = argv[1];
   string ipaddr = argc > 2 ?  argv[2] : "127.0.0.1:8090";


   if (option == "-s") {
      // server side

      Chann svrListen("tcp");

      if ( svrListen.channListen(ipaddr, kMultiChannCount) ) {

         cout << "svr listen " << ipaddr << endl;

         svrListen.setEventHandler([](Chann *self, Chann *accept, chann_event_t event, int err){
               if (event == CHANN_EVENT_ACCEPT) {
                  SvrChann *svr = new SvrChann(accept);
                  cout << "accept cnt " << svr << endl;
                  delete accept;
               }
            });

         ChannDispatcher::startEventLoop();
      }
   }
   else if (option == "-c") {
      // client side

      for (int i=0; i<kMultiChannCount; i++) {
         CntChann *cnt = new CntChann("tcp", i);
         if ( cnt->channConnect(ipaddr) ) {
            cout << i << " begin try connect " << ipaddr << endl;
         } else {
            delete cnt;
         }
      }

      while (ChannDispatcher::pollEvent(1000) > 0) {
      }

      cout << "\nall cnt tested, exit !" << endl;
   }

   return 0;
}

#endif // TEST_RECONNECT_CPP
