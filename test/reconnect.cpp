// 
// 
// Copyright (c) 2017 lalawue
// 
// This library is free software; you can redistribute it and/or modify it
// under the terms of the MIT license. See LICENSE for details.
// 
// 

#include <iostream>
#include <vector>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "plat_net.h"

#ifdef TEST_RECONNECT

using namespace std;

static void _tcpChannEvent(chann_event_t *e);

class BaseChann {
public:
   BaseChann() { m_tcp = NULL; }
   ~BaseChann() { mnet_chann_close(m_tcp); }
   virtual void tcpChannEvent(chann_event_t *e) {}
   chann_t *m_tcp;   
};

class SvrChann : BaseChann {
public:
   bool startListen(char *ipAddr, int port) {
      if (m_tcp) {
         return true;
      }

      if ( (m_tcp = mnet_chann_open(CHANN_TYPE_STREAM)) ) {

         mnet_chann_set_cb(m_tcp, _tcpChannEvent, this);

         if ( mnet_chann_listen_ex(m_tcp, ipAddr, port, 1) ) {
            cout << "start to listen " << ipAddr << ":" << port << endl;
            return true;
         } else {
            cerr << "fail to listen " << endl;
            mnet_chann_close(m_tcp);
         }
      }

      return false;
   }

   void tcpChannEvent(chann_event_t *e) {
      if (e->event == MNET_EVENT_ACCEPT) {
         cout << "------ accept client " << e->r << ", count " << (mnet_report(0)-1) << endl;
         mnet_chann_set_cb(e->r, _tcpChannEvent, this);
      }
      else if (e->event == MNET_EVENT_RECV) {
         char buf[64] = { 0 };
         if ( mnet_chann_recv(e->n, buf, 64) ) {
            cout << "recv <" << buf << ">" << endl;
            int ret = snprintf(buf, 64, "hello, stranger");
            mnet_chann_active_event(e->n, MNET_EVENT_SEND, 1);
            mnet_chann_send(e->n, buf, ret);
         } else {
            cerr << "fail to recv" << endl;
         }
      }
      else if (e->event == MNET_EVENT_CLOSE) {
         if (e->n == m_tcp) {
            m_tcp = NULL;
         }
      }
      else if (e->event == MNET_EVENT_SEND) {
         cout << "disconnect client " << e->n << endl << endl;
         mnet_chann_close(e->n);
      }
      else if (e->event == MNET_EVENT_ERROR) {
         mnet_chann_close(e->n);
      }
   }
};

class CntChann : BaseChann {
public:
   bool connectSvr(char *ipAddr, int port, int mark) {
      if (m_tcp) {
         return true;
      }

      m_ipAddr = ipAddr;
      m_port = port;
      m_mark = mark;

      if ( (m_tcp = mnet_chann_open(CHANN_TYPE_STREAM)) ) {

         mnet_chann_set_cb(m_tcp, _tcpChannEvent, this);

         if ( mnet_chann_connect(m_tcp, ipAddr, port) ) {
            cout << m_mark << "---- try connect server " << ipAddr << ":" << port << endl;
            return true;
         } else {
            cout << m_mark << ": fail to connect server" << endl;
            mnet_chann_close(m_tcp);;
         }
      }
      return false;
   }

   void tcpChannEvent(chann_event_t *e) {
      if (e->event == MNET_EVENT_CONNECTED) {
         cout << m_mark << ": connected." << endl;
         char buf[64] = {0};
         int ret = snprintf(buf, 64, "hello, server");
         if ( mnet_chann_send(e->n, buf, ret) ) {
            cout << m_mark << ": send server <" << buf << ">" << endl;
         } else {
            cerr << m_mark << ": fail to send server data" << endl;
         }
      }
      else if (e->event == MNET_EVENT_RECV) {
         char buf[64] = {0};
         if ( mnet_chann_recv(e->n, buf, 64) ) {
            cout << m_mark << ": recv from server <" << buf << ">" << endl;
         }
      }
      else if (e->event == MNET_EVENT_DISCONNECT) {
         cout << m_mark << ": server disconnect" << endl << endl;
         m_tcp = NULL;

         //sleep(1);

         if ( !this->connectSvr(m_ipAddr, m_port, m_mark) ) {
            cerr << m_mark << ": fail to cnnect server" << endl;
            mnet_chann_close(e->n);
         }
      }
      else if (e->event == MNET_EVENT_CLOSE) {
         cout << m_mark << ": cilent closed" << endl;
      }
   }

private:
   char *m_ipAddr;
   int m_port;
   int m_mark;
};

void _tcpChannEvent(chann_event_t *e) {
   BaseChann *c = (BaseChann*)e->opaque;
   if (c) {
      c->tcpChannEvent(e);
   }
}

int main(int argc, char *argv[]) {
   bool isServer = false;
   bool isRunning = false;
   SvrChann *sc = NULL;
   vector<CntChann*> channsVec;

   if (argc < 3) {
      cout << "Usage: .out [-s|-c] server_ip" << endl;
      return 0;
   }

   // get model
   if (strncmp(argv[1], "-s", 2) == 0) {
      isServer = true;
   }

   mnet_init();

   if (isServer) {
      sc = new SvrChann;
      if ( sc->startListen(argv[2], 8299) ) {
         isRunning = true;
      }
   } else {
      for (int i=0; i<1024; i++) {
         CntChann *cc = new CntChann;
         if ( cc->connectSvr(argv[2], 8299, i++) ) {
            isRunning = true;
         }
         channsVec.push_back(cc);
      }
   }

   while (isRunning) {
      mnet_poll(-1);
   }

   mnet_fini();

   return 0;
}


#endif // TEST_RECONNECT
