//
//
// Copyright (c) 2017 lalawue
//
// This library is free software; you can redistribute it and/or modify it
// under the terms of the MIT license. See LICENSE for details.
//
//

#ifdef TEST_RWDATA_CPP

#include <iostream>
#include <assert.h>
#include "mnet_wrapper.h"

using std::cout;
using std::endl;
using std::string;

using mnet::Chann;
using mnet::ChannDispatcher;

static const int kBufSize = 256*1024; // even number
static const int kSendedPoint = 1024*1024*1024;

class BaseChann : public Chann {
public:
   BaseChann(Chann *c) : Chann(c) { m_sended = m_recved = 0; }
   BaseChann(string streamType) : Chann(streamType) {}
   bool checkDataBuf(int base, int len) {
      for (int i=0; i<len; i++) {
         if (m_buf[i] != ((base + i) & 0xff)) {
            return false;
         }
      }
      return true;
   }
   void releaseSelf() {
      cout << "received " << m_recved << endl
           << "sended " << m_sended << endl
           << "release self" << endl;
      delete this;
   }
   int m_sended;
   int m_recved;
   uint8_t m_buf[kBufSize];
};

class SvrChann : public BaseChann {
public:
   SvrChann(Chann *c) : BaseChann(c) {}


   void defaultEventHandler(Chann *accept, chann_event_t event, int err) {
      if (event == CHANN_EVENT_RECV) {
         int ret = channRecv(m_buf, kBufSize);
         if (checkDataBuf(m_recved, ret)) {
            m_recved += ret;
            ret = channSend(m_buf, ret);
            if (ret > 0) {
               m_sended += ret;
               assert(m_recved == m_sended);
            } else {
               cout << "invalid send " << endl;
               releaseSelf();
            }
         } else {
            cout << "svr failed to recv ret code: " << ret << endl;
            releaseSelf();
         }
      }
      if (event == CHANN_EVENT_DISCONNECT) {
         releaseSelf();
      }
   }
};

class CntChann : public BaseChann {
public:
   CntChann(string streamType) : BaseChann(streamType) {}

   void sendBatchData() {
      for (int i=0; i<kBufSize; i++) {
         m_buf[i] = (m_sended + i) & 0xff;
      }
      int ret = channSend(m_buf, kBufSize);
      if (ret > 0) {
         assert(ret == kBufSize);
         m_sended += ret;
      }
   }

   bool recvBatchData() {
      int ret = channRecv(m_buf, kBufSize);
      if (checkDataBuf(m_recved, ret)) {
         m_recved += ret;
         cout << "c recved " << m_recved << endl;
         return true;
      } else {
         cout << "c failed to checked data: " << ret << endl;
         return false;
      }
   }

   void defaultEventHandler(Chann *accept, chann_event_t event, int err) {
      if (event == CHANN_EVENT_CONNECTED) {
         channActiveEvent(CHANN_EVENT_SEND, 1);
      }
      if (event == CHANN_EVENT_RECV) {
         if (recvBatchData() && m_recved < kSendedPoint) {
         } else {
            releaseSelf();
         }
      }
      if (event == CHANN_EVENT_SEND) {
         if (m_sended < kSendedPoint) {
            sendBatchData();
         } else {
            cout << "send enough data " << m_sended << endl;
            channActiveEvent(CHANN_EVENT_SEND, 0);
         }
      }
      if (event == CHANN_EVENT_DISCONNECT) {
         releaseSelf();
      }
   }
};

int main(int argc, char *argv[]) {

   if (argc < 2) {
      cout << "Usage: " << argv[0] << " [-s|-c] [ip:port]" << endl;
      return 0;
   }

   string option = argv[1];
   string ipaddr = argc > 2 ? argv[2] : "127.0.0.1:8090";


   if (option == "-s") {

      Chann svrListen("tcp");

      if ( svrListen.channListen(ipaddr) ) {

         cout << "svr listen " << ipaddr << endl;

         svrListen.setEventHandler([](Chann *self, Chann *accept, chann_event_t event, int err){
               if (event == CHANN_EVENT_ACCEPT) {
                  new SvrChann(accept);
                  delete accept;
               }
            });

         ChannDispatcher::startEventLoop();
      }
   }
   else if (option == "-c") {

      CntChann *cnt = new CntChann("tcp");
      if ( !cnt->channConnect(ipaddr) ) {
         delete cnt;
      }

      while (ChannDispatcher::pollEvent(1000) > 0) {
      }
   }

   return 0;
}

#endif  // TEST_RWDATA_CPP
