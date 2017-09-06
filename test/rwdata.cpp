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

static const int kBufSize = 32*1024; // even number
static const int kCachedSize = 1024*1024;
static const int kSendedPoint = 1024*1024*1024;

class BaseChann : public Chann {
public:
   BaseChann() { m_sended = m_recved = 0; }
   void fillDataBuf () {
      for (int i=0; i<kBufSize; i++) {
         m_buf[i] = (m_sended + i) & 0xff;
      }
   }
   bool checkDataBuf(int len) {
      if (len < 0) {
         return false;
      }
      for (int i=0; i<len; i++) {
         if (m_buf[i] != ((m_recved + i) & 0xff)) {
            return false;
         }
      }
      return true;
   }
   void releaseSelf() {
      cout << "recved " << m_recved << endl;
      cout << "sended " << m_sended << endl;
      delete this;
   }
   int m_sended;
   int m_recved;
   unsigned char m_buf[kBufSize];
};

class SvrChann : public BaseChann {
public:
   SvrChann(Chann *nc) {
      channUpdate(nc, NULL);
   }
   void defaultEventHandler(Chann *accept, chann_event_t event) {
      if (event == CHANN_EVENT_RECV) {
         int ret = 0;
         do {
            ret = channRecv(m_buf, kBufSize);
            m_recved += ret > 0 ? ret : 0;

            ret = channSend(m_buf, ret);
            m_sended += ret > 0 ? ret : 0;

         } while (ret > 0);
      }
      if (event == CHANN_EVENT_DISCONNECT) {
         releaseSelf();
      }
   }
};

class CntChann : public BaseChann {
public:
   CntChann(string streamType) {
      Chann c(streamType);
      channUpdate(&c, NULL);
   }

   void sendBatchData() {
      for (int ret=1; ret>0 && (dataCached() < kCachedSize); ) {
         fillDataBuf();
         ret = channSend(m_buf, kBufSize);
         if (ret > 0) {
            m_sended += ret;
         }
      }
   }

   bool recvBatchData() {
      int ret = 0;
      do {
         ret = channRecv(m_buf, kBufSize);
         if ( checkDataBuf(ret) ) {
            m_recved += ret;
         }
      } while (ret > 0);
      cout << "c recved " << m_recved << endl;
      return ret >= 0;
   }

   void defaultEventHandler(Chann *accept, chann_event_t event) {
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
         } else if ( !m_sendByteData ){
            cout << "send bye data" << endl;
            channSend((m_buf[0]=0x88, m_buf), 1);
            m_sendByteData = true;
         }
      }

      if (event == CHANN_EVENT_DISCONNECT) {
         releaseSelf();
      }
   }
   bool m_sendByteData;
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
      svrListen.channListen(ipaddr);

      cout << "svr listen " << ipaddr << endl;

      svrListen.setEventHandler([](Chann *self, Chann *accept, chann_event_t event){
            if (event == CHANN_EVENT_ACCEPT) {
               new SvrChann(accept);
               delete accept;
            }
         });

      ChannDispatcher::startEventLoop();
      ChannDispatcher::stopEventLoop();
   }
   else if (option == "-c") {

      CntChann *cnt = new CntChann("tcp");
      cnt->channConnect(ipaddr);
      while (mnet_poll(-1) > 0) {
      }
   }

   return 0;
}

#endif  // TEST_RWDATA
