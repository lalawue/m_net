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
#include "plat_net.h"

#ifdef __cplusplus

namespace mnet {

   using std::string;
   
   class ChannAddr {
     public:
      ChannAddr(string ipPort) {
         int f = ipPort.find(":");
         if (f > 0) {
            strncpy(ip, ipPort.substr(0, f).c_str(), 16);
            port = atoi(ipPort.substr(f+1, ipPort.length() - f).c_str());
         }
      }
      ~ChannAddr() { }
      char ip[16];
      int port;
   };


   class Chann;
   typedef void(*channEventHandler)(Chann *self, Chann *accept, mnet_event_type_t);


   class Chann {
     public:
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

      bool listen(string ipPort) {
         if (m_chann && ipPort.length()>0) {
            ChannAddr addr(ipPort);
            mnet_chann_set_cb(m_chann, Chann::channDispatchEvent, this);
            return mnet_chann_listen_ex(m_chann, addr.ip, addr.port, 1);
         }
         return false;
      }
   
      bool connect(string ipPort) {
         if (m_chann && ipPort.length()>0) {
            ChannAddr addr(ipPort);
            mnet_chann_set_cb(m_chann, Chann::channDispatchEvent, this);
            return mnet_chann_connect(m_chann, addr.ip, addr.port);
         }
         return false;
      }

      void disconnect(void) {
         if (m_chann) {
            mnet_chann_disconnect(m_chann);
         }
      }

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

      // external event handler
      void setEventHandler(channEventHandler handler) {
         m_handler = handler;
      }
      
      // if no external handler then use default, subclass and re-implement
      virtual void defaultEventHandler(Chann *accept, mnet_event_type_t event) {
         if ( accept ) {
            delete accept;
         }
      }

      // update this and clear nc
      void updateChann(Chann *fromChann, channEventHandler handler) {
         m_chann = fromChann->m_chann;
         m_handler = handler;
         mnet_chann_set_cb(m_chann, Chann::channDispatchEvent, this);
         fromChann->m_chann = NULL;
         fromChann->m_handler = NULL;
      }



     private:

      Chann(chann_t *c) { m_chann = c; }

      static void channDispatchEvent(chann_event_t *e) {
         Chann *c = (Chann*)e->opaque;
         c->dispatchEvent(e);
      }

      virtual void dispatchEvent(chann_event_t *e) {
         if (m_handler) {
            if (e->event == MNET_EVENT_ACCEPT) {
               Chann *nc = new Chann(e->r);
               mnet_chann_set_cb(e->r, Chann::channDispatchEvent, nc);
               m_handler(this, nc, e->event);
            } else {
               m_handler(this, NULL, e->event);
            }
         } else {
            if (e->event == MNET_EVENT_ACCEPT) {
               Chann *nc = new Chann(e->r);
               mnet_chann_set_cb(e->r, Chann::channDispatchEvent, nc);
               defaultEventHandler(nc, e->event);
            } else {
               defaultEventHandler(NULL, e->event);
            }
         }
      }

      chann_t *m_chann;
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
