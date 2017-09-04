// 
// 
// Copyright (c) 2017 lalawue
// 
// This library is free software; you can redistribute it and/or modify it
// under the terms of the MIT license. See LICENSE for details.
// 
// 

#include <string>
#include <stdlib.h>
#include <string.h>
#include "mnet_core.h"

#ifdef __cplusplus

namespace mnet {

   using std::string;
   
   class ChannAddr {
     public:
      ChannAddr() { ChannAddr("0.0.0.0:8972"); }
      ChannAddr(string ipPort) {
         int f = ipPort.find(":");
         if (f > 0) {
            addr = ipPort;
            strncpy(ip, ipPort.substr(0, f).c_str(), 16);
            port = atoi(ipPort.substr(f+1, ipPort.length() - f).c_str());
         }
      }
      ~ChannAddr() { }
      char ip[16];
      int port;
      string addr;
   };


   class Chann;
   typedef void(*channEventHandler)(Chann *self, Chann *accept, chann_event_t);


   class Chann {
     public:

      /* chann creation 
       */
      Chann() {}

      Chann(string streamType) {
         chann_type_t ctype = CHANN_TYPE_STREAM;
         if (streamType == "udp") {
            ctype = CHANN_TYPE_DGRAM;
         } else if (streamType == "broadcast") {
            ctype = CHANN_TYPE_BROADCAST;
         }
         mnet_init();
         m_chann = mnet_chann_open( ctype );
      }

      virtual ~Chann() {
         mnet_chann_close(m_chann);
         m_chann = NULL;
         m_handler = NULL;
      }

      // update self then clear fromChann
      void updateChann(Chann *fromChann, channEventHandler handler) {
         m_chann = fromChann->m_chann;
         m_handler = handler;
         mnet_chann_set_cb(m_chann, Chann::channDispatchEvent, this);
         fromChann->m_chann = NULL;
         fromChann->m_handler = NULL;
      }


      /* build network 
       */
      bool listen(string ipPort) {
         if (m_chann && ipPort.length()>0) {
            m_addr = ChannAddr(ipPort);
            mnet_chann_set_cb(m_chann, Chann::channDispatchEvent, this);
            return mnet_chann_listen_ex(m_chann, m_addr.ip, m_addr.port, 1);
         }
         return false;
      }
   
      bool connect(string ipPort) {
         if (m_chann && ipPort.length()>0) {
            m_addr = ChannAddr(ipPort);
            mnet_chann_set_cb(m_chann, Chann::channDispatchEvent, this);
            return mnet_chann_connect(m_chann, m_addr.ip, m_addr.port);
         }
         return false;
      }

      void disconnect(void) {
         if (m_chann) {
            mnet_chann_disconnect(m_chann);
         }
      }


      /* data mantipulation
       */
      int recv(void *buf, int len) {
         if (mnet_chann_state(m_chann) == CHANN_STATE_CONNECTED) {
            return mnet_chann_recv(m_chann, buf, len);
         }
         return -1;
      }

      int send(void *buf, int len) {
         if (mnet_chann_state(m_chann) == CHANN_STATE_CONNECTED) {
            return mnet_chann_send(m_chann, buf, len);
         }
         return -1;
      }


      /* event handler
       */

      // only support MNET_EVENT_SEND, event send while send buffer emtpy
      void activeEvent(chann_event_t event) {
         mnet_chann_active_event(m_chann, event, 1);
      }
      void inActiveEvent(chann_event_t event) {
         mnet_chann_active_event(m_chann, event, 0);
      }

      // external event handler, overide defaultEventHandler
      void setEventHandler(channEventHandler handler) {
         m_handler = handler;
      }
      
      // use default if no external event handler, for subclass internal
      virtual void defaultEventHandler(Chann *accept, chann_event_t event) {
         if ( accept ) {
            delete accept;
         }
      }


      /* misc
       */
      ChannAddr remoteAddr(void) { return m_addr; };
      int dataCached(void) { return mnet_chann_cached(m_chann); }
      bool isConnected(void) { return mnet_chann_state(m_chann) == CHANN_STATE_CONNECTED; }


     private:

      Chann(chann_t *c) { m_chann = c; }

      /* event process
       */
      static void channDispatchEvent(chann_msg_t *m) {
         Chann *c = (Chann*)m->opaque;
         c->dispatchEvent(m);
      }

      void dispatchEvent(chann_msg_t *m) {
         if (m_handler) {
            if (m->event == CHANN_EVENT_ACCEPT) {
               Chann *nc = new Chann(m->r);
               mnet_chann_set_cb(m->r, Chann::channDispatchEvent, nc);
               m_handler(this, nc, m->event);
            } else {
               m_handler(this, NULL, m->event);
            }
         } else {
            if (m->event == CHANN_EVENT_ACCEPT) {
               Chann *nc = new Chann(m->r);
               mnet_chann_set_cb(m->r, Chann::channDispatchEvent, nc);
               defaultEventHandler(nc, m->event);
            } else {
               defaultEventHandler(NULL, m->event);
            }
         }
      }

      chann_t *m_chann;
      ChannAddr m_addr;
      channEventHandler m_handler; // external event handler
   };


   class ChannDispatcher {
     public:
      static void startEventLoop(void) {
         if ( !isRunning() ) {
            isRunning() = true;
            while ( isRunning() ) {
               mnet_poll(-1);
            }
         }
      }
      static void stopEventLoop(void) {
         isRunning() = false;
      }

     private:
      static bool& isRunning(void) {
         static bool s;
         return s;
      }
      ChannDispatcher();
      ~ChannDispatcher();
   };
};

#endif
