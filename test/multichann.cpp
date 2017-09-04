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
#include <list>
#include <string>
#include <stdio.h>
#include "mnet_wrapper.h"

using std::cout;
using std::endl;

using std::list;
using std::string;

using mnet::Chann;
using mnet::ChannDispatcher;


class SvrChann : public Chann {
public:
   SvrChann(Chann *nc) {
      updateChann(nc, NULL);
   }
   
   void defaultEventHandler(Chann *accept, chann_event_t event) {
      if (event == CHANN_EVENT_RECV) {
         char buf[64] = {0};
         int ret = recv(buf, 64);
         if (ret > 0) {
            send(buf, ret);
            cout << "recv from cnt: " << buf << endl;
         } else {
            cout << "fail to recv" << endl;
         }
      }
      else if (event == CHANN_EVENT_DISCONNECT) {
         cout << "cnt " << this << " disconnect" << endl;
         delete this;
      }
   }
};


class CntChann : public Chann {
public:
   CntChann(string streamType) {
      Chann c(streamType);
      updateChann(&c, NULL);
   }

   void defaultEventHandler(Chann *accept, chann_event_t event) {
      switch (event) {
         case CHANN_EVENT_CONNECTED: {
            cout << "cnt " << m_idx << " connected !" << endl;
            char data[64] = {0};
            int ret = snprintf(data, 64, "HelloServ %d", m_idx);
            send((void*)data, ret);
            break;
         }

         case CHANN_EVENT_RECV: {
            char buf[64] = {0};
            int ret = recv(buf, 64);
            if (ret > 0) {
               cout << "recv from svr: " << buf << endl;
            } else {
               cout << "fail to recv" << endl;
            }
            delete this;
            break;
         }

         default: {             // MNET_EVENT_DISCONNECT
            delete this;
            break;
         }
      }
   }

   int m_idx;
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
      svrListen.listen(ipaddr);

      cout << "svr listen " << ipaddr << endl;

      static int cntCount = 0;

      svrListen.setEventHandler([](Chann *self, Chann *accept, chann_event_t event){
            if (event == CHANN_EVENT_ACCEPT) {
               cntCount += 1;
               SvrChann *nc = new SvrChann(accept);
               cout << "accept cnt " << nc << ", count " << cntCount << endl;
               delete accept;
            }
         });

      ChannDispatcher::startEventLoop();
   }
   else if (option == "-c") {
      // client side
      list<CntChann*> cntList;

      for (int i=0; i<2048; i++) {
         CntChann *cnt = new CntChann("tcp");
         cnt->m_idx = i;
         if ( cnt->connect(ipaddr) ) {
            cntList.push_back(cnt);
         }
      }

      for (int cntCount=0; ; ) {
         cntCount = mnet_poll(2000);
         if (cntCount <= 0) {
            break;
         }
      }
      
      cout << "all cnt connect, send, recv done, exit !" << endl;
      cntList.clear();
   }

   return 0;
}

#endif // TEST_MULTICHANNS
