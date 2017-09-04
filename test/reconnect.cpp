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
#include <vector>
#include <string>
#include <unistd.h>
#include "mnet_wrapper.h"

using std::cout;
using std::endl;

using std::vector;
using std::string;

using mnet::Chann;
using mnet::ChannAddr;
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
            activeEvent(CHANN_EVENT_SEND);
            send(buf, ret);
            cout << "recv from cnt: " << buf << endl;
         } else {
            cout << "fail to recv" << endl;
         }
      }
      else if (event == CHANN_EVENT_SEND ||
               event == CHANN_EVENT_DISCONNECT)
      {
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
            if (ret == send((void*)data, ret)) {
               cout << "send " << data << endl;
            } else {
               cout << "Fail to send !" << endl;
               delete this;
            }
            break;
         }

         case CHANN_EVENT_RECV: {
            char buf[64] = {0};
            int ret = recv(buf, 64);
            if (ret > 0) {
               cout << "recv from svr: " << buf << endl;
            } else {
               cout << "fail to recv" << endl;
               delete this;
            }
            break;
         }

         case CHANN_EVENT_DISCONNECT: {
            cout << "chann disconnect !" << endl;

            usleep(50*1000);

            if ( connect( remoteAddr().addr ) ) {
               cout << "try to connect " << remoteAddr().addr << endl;
            } else {
               cout << "fail to connect !" << endl;
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

      for (int i=0; i<1; i++) {
         CntChann *cnt = new CntChann("tcp");
         cnt->m_idx = i;
         if ( cnt->connect(ipaddr) ) {
            cout << "try connect " << ipaddr << endl;
         } else {
            break;
         }
      }

      while (true) {
         int ret = mnet_poll(50*1000);
         if (ret <= 0) {
            break;
         }
         usleep(50*1000);
      }
   }

   return 0;
}

#endif // TEST_RECONNECT
