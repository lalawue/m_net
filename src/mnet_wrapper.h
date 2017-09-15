// 
// 
// Copyright (c) 2017 lalawue
// 
// This library is free software; you can redistribute it and/or modify it
// under the terms of the MIT license. See LICENSE for details.
// 
// 

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mnet_core.h"

#ifdef __cplusplus

namespace mnet {

   class Chann;
   using std::string;

   typedef void(*channEventHandler)(Chann *self, Chann *accept, chann_event_t, int err);

   static chann_type_t channType(string streamType) {
      if (streamType == "tcp") return CHANN_TYPE_STREAM;
      if (streamType == "udp") return CHANN_TYPE_DGRAM;
      return CHANN_TYPE_BROADCAST;
   }


   class ChannAddr {
     public:
      ChannAddr() { ChannAddr("0.0.0.0:8972"); }
      ChannAddr(string ipPort) {
         addrString = ipPort;
         mnet_parse_ipport((char*)ipPort.c_str(), &addr);
      }
      ~ChannAddr() { }
      static ChannAddr resolveHost(string hostPort, string streamType) {
         if (hostPort.length()>0 && streamType.length()>0) {
            chann_addr_t addr;
            int port=0, f=hostPort.find(":");
            if (f > 0) {
               port = atoi(hostPort.substr(f+1, hostPort.length() - f).c_str());
               hostPort = hostPort.substr(0,f);
            }
            if ( mnet_resolve((char*)hostPort.c_str(), port, channType(streamType), &addr) ) {
               char buf[32]; sprintf(buf, "%s:%d", addr.ip, addr.port);
               return ChannAddr(buf);
            }
         }
         return ChannAddr();
      }
      chann_addr_t addr;
      string addrString;
   };


   class Chann {
     public:

      /* chann creation 
       */
      Chann() {}

      Chann(string streamType) {
         mnet_init();
         m_chann = mnet_chann_open( channType(streamType) );
      }

      virtual ~Chann() {
         mnet_chann_close(m_chann);
         m_chann = NULL;
         m_handler = NULL;
      }

      // update self then clear fromChann
      void channUpdate(Chann *fromChann, channEventHandler handler) {
         m_chann = fromChann->m_chann;
         m_handler = handler;
         mnet_chann_set_cb(m_chann, Chann::channDispatchEvent, this);
         fromChann->m_chann = NULL;
         fromChann->m_handler = NULL;
      }


      /* build network 
       */
      bool channListen(string ipPort) {
         if (m_chann && ipPort.length()>0) {
            m_addr = ChannAddr(ipPort);
            mnet_chann_set_cb(m_chann, Chann::channDispatchEvent, this);
            return mnet_chann_listen(m_chann, m_addr.addr.ip, m_addr.addr.port, 16);
         }
         return false;
      }
   
      bool channConnect(string ipPort) {
         if (m_chann && ipPort.length()>0) {
            m_addr = ChannAddr(ipPort);
            mnet_chann_set_cb(m_chann, Chann::channDispatchEvent, this);
            return mnet_chann_connect(m_chann, m_addr.addr.ip, m_addr.addr.port);
         }
         return false;
      }

      void channDisconnect(void) {
         if (m_chann) {
            mnet_chann_disconnect(m_chann);
         }
      }


      /* data mantipulation
       */
      int channRecv(void *buf, int len) {
         if (mnet_chann_state(m_chann) == CHANN_STATE_CONNECTED) {
            return mnet_chann_recv(m_chann, buf, len);
         }
         return -1;
      }

      int channSend(void *buf, int len) {
         if (mnet_chann_state(m_chann) == CHANN_STATE_CONNECTED) {
            return mnet_chann_send(m_chann, buf, len);
         }
         return -1;
      }


      /* event handler
       */

      // only support MNET_EVENT_SEND, event send while send buffer emtpy
      void channEnableEvent(chann_event_t event) {
         mnet_chann_active_event(m_chann, event, 1);
      }
      void channDisableEvent(chann_event_t event) {
         mnet_chann_active_event(m_chann, event, 0);
      }

      // external event handler, overide defaultEventHandler
      void setEventHandler(channEventHandler handler) {
         m_handler = handler;
      }
      
      // use default if no external event handler, for subclass internal
      virtual void defaultEventHandler(Chann *accept, chann_event_t event, int err) {
         if ( accept ) {
            delete accept;
         }
      }


      /* misc
       */
      inline ChannAddr peerAddr(void) { return m_addr; };
      inline int dataCached(void) { return mnet_chann_cached(m_chann); }
      inline bool isConnected(void) { return mnet_chann_state(m_chann) == CHANN_STATE_CONNECTED; } /*  */



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
               m_handler(this, nc, m->event, 0);
            } else {
               m_handler(this, NULL, m->event, m->err);
            }
         } else {
            if (m->event == CHANN_EVENT_ACCEPT) {
               Chann *nc = new Chann(m->r);
               mnet_chann_set_cb(m->r, Chann::channDispatchEvent, nc);
               defaultEventHandler(nc, m->event, m->err);
            } else {
               defaultEventHandler(NULL, m->event, m->err);
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
