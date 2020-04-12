// 
// 
// Copyright (c) 2020 lalawue
// 
// This library is free software; you can redistribute it and/or modify it
// under the terms of the MIT license. See LICENSE for details.
// 
// 

#ifdef TEST_TIMER

#include <iostream>
#include <string>
#include "mnet_wrapper.h"

#define kBufSize 128
#define kMultiChannCount 2
#define kOneSecond 1000000

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
         rw_result_t *rw = channRecv(m_buf, sizeof(m_buf));
         channSend(m_buf, rw->ret);
      }
   }
   char m_buf[kBufSize];
};


class CntChann : public Chann {
public:
   CntChann(string streamType, int idx) : Chann(streamType) { m_idx = idx; }

   void defaultEventHandler(Chann *accept, chann_event_t event, int err) {
      switch (event) {
         case CHANN_EVENT_CONNECTED: {
            m_connected_time = ChannDispatcher::currentTime();
            cout << m_idx << ": connected time " << m_connected_time << endl;
            break;
         }
         case CHANN_EVENT_RECV: {
            channRecv(m_buf, kBufSize);
            cout << "recv from svr: '" << m_buf << "'" << endl;
            break;
         }

         case CHANN_EVENT_TIMER: {
            m_duration = (int64_t)(ChannDispatcher::currentTime() - m_connected_time) / kOneSecond;
            if (m_duration > 20) {
               cout << m_idx << ": over 20 seconds, time " << ChannDispatcher::currentTime() << endl;
               delete this;               
            } else {
               int ret = snprintf(m_buf, sizeof(m_buf), "HelloServ %d %lld", m_idx, m_duration);
               channSend(m_buf, ret);
            }
            break;
         }

         default: {
            break;
         }
      }
   }

   int m_idx;
   char m_buf[kBufSize];   
   int64_t m_duration;
   int64_t m_connected_time;   
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
            int64_t interval = ((random() + i) % 10 + 1) * kOneSecond;;
            cnt->channActiveEvent(CHANN_EVENT_TIMER, interval);
         } else {
            delete cnt;
         }
      } 

      poll_result_t *result;
      while ((result = ChannDispatcher::pollEvent(1000000)) && result->chann_count > 0) {
         cout << result->chann_count << endl;
      }

      cout << "\nall cnt tested, exit !" << endl;      
   }

   return 0;
}

#endif // TEST_RECONNECT
