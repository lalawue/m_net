// 
// 
// Copyright (c) 2017 lalawue
// 
// This library is free software; you can redistribute it and/or modify it
// under the terms of the MIT license. See LICENSE for details.
// 
// 

#ifdef TEST_RECONNECT

#include <iostream>
#include <string>
#include <unistd.h>
#include "mnet_wrapper.h"

using std::cout;
using std::endl;
using std::string;

using mnet::Chann;
using mnet::ChannAddr;
using mnet::ChannDispatcher;


class SvrChann : public Chann {
public:
   SvrChann(Chann *nc) {
      channUpdate(nc, NULL);
   }
   
   void defaultEventHandler(Chann *accept, chann_event_t event) {
      if (event == CHANN_EVENT_RECV) {
         int ret = channRecv(m_buf, sizeof(m_buf));
         channEnableEvent(CHANN_EVENT_SEND);
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
   CntChann(string streamType) {
      Chann c(streamType);
      channUpdate(&c, NULL);
   }

   void defaultEventHandler(Chann *accept, chann_event_t event) {
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

            usleep(50000);

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
      svrListen.channListen(ipaddr);

      cout << "svr listen " << ipaddr << endl;

      svrListen.setEventHandler([](Chann *self, Chann *accept, chann_event_t event){
            if (event == CHANN_EVENT_ACCEPT) {
               SvrChann *nc = new SvrChann(accept);
               cout << "accept cnt " << nc << endl;
               delete accept;
            }
         });

      ChannDispatcher::startEventLoop();
   }
   else if (option == "-c") {
      // client side

      CntChann *cnt = new CntChann("tcp");
      if ( cnt->channConnect(ipaddr) ) {
         cout << "begin try connect " << ipaddr << endl;
      } else {
         delete cnt;
      }

      ChannDispatcher::startEventLoop();
   }

   return 0;
}

#endif // TEST_RECONNECT
