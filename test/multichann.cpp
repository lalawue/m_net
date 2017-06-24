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
#include <string.h>
#include "plat_net.h"

#ifdef TEST_MULTICHANNS

using namespace std;

static void _tcpChannEvent(chann_event_t*);
static unsigned const kMaxClientCount = 4096;


class TestChanns {

public:
   TestChanns() {
      m_isServer = -1;
      m_value = 0;
      m_tcp = NULL;
   }

   ~TestChanns() { 
      mnet_chann_set_cb(m_tcp, NULL, NULL);
      mnet_chann_close(m_tcp);
   }


   void tcpChannEvent(chann_event_t *e) {
      switch (e->event) {

         case MNET_EVENT_ACCEPT: {
            m_value += 1;
            cout << "client connected count " << m_value << endl;
            mnet_chann_set_cb(e->r, _tcpChannEvent, this);
            break;
         }

         case MNET_EVENT_CONNECTED: {
            if (mnet_chann_send(e->n, &m_value, sizeof(m_value)) > 0) {
               cout << m_value << ": send data " << m_value << endl;
            } else {
               cerr << m_value << ": fail to send data !" << endl;
            }
            break;
         }

         case MNET_EVENT_RECV: {
            int value = 0;
            if (m_isServer == 1) {
               if (mnet_chann_recv(e->n, &value, sizeof(value)) > 0) {
                  cout << "recv client value " << value << endl;
               } else {
                  cerr << "fail to recv data !" << endl;
               }

               value += 10;
               if (mnet_chann_send(e->n, &value, sizeof(value)) <= 0) {
                  cerr << "fail to send Thanks" << endl;
               }
            } else {
               if (mnet_chann_recv(e->n, &value, sizeof(value)) > 0) {
                  cout << m_value << ": recv server data " << value << endl;
               } else {
                  cerr << m_value << ": fail to recv server data !" << endl;
               }
               mnet_chann_close(m_tcp);
               m_tcp = NULL;
            }
            break;
         }

         case MNET_EVENT_DISCONNECT: {
            cout << m_value << ": disconnect." << endl;
            mnet_chann_close(e->n);
            m_tcp = NULL;
            break;
         }

         case MNET_EVENT_ERROR: {
            cout << m_value << ": error then exit ------------------------------- " << e->err << endl;
            mnet_chann_close(e->n);
            break;
         }

         default: {
            break;
         }
      }
   }


   bool serverListen(const char *ipAddr, int port) {
      if (m_isServer >= 0) {
         return false;
      }
      m_isServer = 1;

      m_tcp = mnet_chann_open(CHANN_TYPE_STREAM);
      if (m_tcp == NULL) {
         cerr << "Fail to create listen chann !" << endl;
         return false;
      }

      mnet_chann_set_cb(m_tcp, _tcpChannEvent, this);

      if (mnet_chann_listen(m_tcp, port) <= 0) {
         cerr << "Fail to listen !" << endl;
         return false;
      }

      cout << "start listen: " << ipAddr << ":" << port << endl;
      return true;
   }


   bool clientConnect(const char *ipAddr, int port, int value) {
      if (m_isServer >= 0) {
         return false;
      }
      m_isServer = 0;
      m_value = value;

      m_tcp = mnet_chann_open(CHANN_TYPE_STREAM);
      if (m_tcp == NULL) {
         cerr << "Fail to create listen chann !" << endl;
         return false;
      }

      mnet_chann_set_cb(m_tcp, _tcpChannEvent, this);

      if (mnet_chann_connect(m_tcp, ipAddr, port) <= 0) {
         cerr << "Fail to connect !" << endl;
         return false;
      }

      cout << m_value << ":try connect " << ipAddr << ":" << port << " with value " << m_value << endl;
      return true;
   }

private:
   int m_isServer;
   int m_value;
   chann_t *m_tcp;
};

void _tcpChannEvent(chann_event_t *e) {
   TestChanns *tc = (TestChanns*)e->opaque;
   tc->tcpChannEvent(e);
}

int main(int argc, char *argv[]) {
   bool isServer = false;
   vector<TestChanns*> channsVec;


   if (argc < 3) {
      cout << "Usage: .out [-s|-c] server_ip" << endl;
      return 0;
   }


   // get model
   if (strncmp(argv[1], "-s", 2) == 0) {
      isServer = true;
   }
   

   // init mnet
   mnet_init();


   if (isServer) {
      TestChanns *tc = new TestChanns;
      if ( tc->serverListen(argv[2], 8099) ) {
         channsVec.push_back(tc);
      }
   }

   for (;;) {

      if ( !isServer ) {
         if (channsVec.size() < kMaxClientCount) {
            TestChanns *tc = new TestChanns;
            if ( tc->clientConnect((const char*)argv[2], 8099, channsVec.size()) ) {
               channsVec.push_back(tc);
            }
         }
      }

      int count = mnet_poll( 2000 );
      if (count == 0) {
         break;
      }
   }

   mnet_fini();

   cout << "channs vec " << channsVec.size() << endl;

   return 0;
}

#endif // TEST_MULTICHANNS
