// 
// 
// Copyright (c) 2017 lalawue
// 
// This library is free software; you can redistribute it and/or modify it
// under the terms of the MIT license. See LICENSE for details.
// 
// 

#ifdef TEST_RWDATA

#include <iostream>
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
   void releaseSelf() {
      cout << "recved " << m_recved << endl
           << "sended " << m_sended << endl
           << "release self" << endl;
      delete this;
   }
   int m_sended;
   int m_recved;
   unsigned char m_buf[kBufSize];
};

class SvrChann : public BaseChann {
public:
   SvrChann(Chann *c) : BaseChann(c) {}

   void defaultEventHandler(Chann *accept, chann_event_t event, int err) {
      if (event == CHANN_EVENT_RECV) {
         rw_result_t *rw = channRecv(m_buf, kBufSize);
         if (rw->ret > 0) {
            m_recved += rw->ret;
            m_sended += channSend(m_buf, rw->ret)->ret;
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

   void fillDataBuf () {
      for (int i=0; i<kBufSize; i++) {
         m_buf[i] = (m_sended + i) & 0xff;
      }
   }

   void sendBatchData() {
      fillDataBuf();
      rw_result_t *rw = channSend(m_buf, kBufSize);
      if (rw->ret > 0) {
         m_sended += rw->ret;
      }
   }

   bool checkDataBuf(int len) {
      for (int i=0; len>0 && i<len; i++) {
         if (m_buf[i] != ((m_recved + i) & 0xff)) {
            return false;
         }
      }
      return true;
   }

   bool recvBatchData() {
      rw_result_t *rw = channRecv(m_buf, kBufSize);
      if (rw->ret>0 && checkDataBuf(rw->ret)) {
         m_recved += rw->ret;
         cout << "c recved " << m_recved << endl;
         return true;
      }
      return false;
   }

   void defaultEventHandler(Chann *accept, chann_event_t event, int err) {
      if (event == CHANN_EVENT_CONNECTED) {
         sendBatchData();
      }

      if (event == CHANN_EVENT_RECV) {
         if ( !recvBatchData() ) {
            cout << "fail to recv !" << endl;
            releaseSelf();
         }

         if (m_sended < kSendedPoint) {
            sendBatchData();
         }
         
         if (m_recved >= kSendedPoint ) {
            releaseSelf();
         }
      }

      if (event == CHANN_EVENT_DISCONNECT) {
         releaseSelf();
      }
   }
};

int main(int argc, char *argv[]) {

   if (argc < 3) {
      cout << "Usage: " << argv[0] << " [-s|-c] server_ip:port" << endl;
      return 0;
   }

   string option = argv[1];
   string ipaddr = argv[2];


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

      while (ChannDispatcher::pullEvent(-1)->chann_count > 0) {
      }
   }

   return 0;
}

#endif  // TEST_RWDATA
