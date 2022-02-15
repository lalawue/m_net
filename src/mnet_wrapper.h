// 
// 
// Copyright (c) 2017 lalawue
// 
// This library is free software; you can redistribute it and/or modify it
// under the terms of the MIT license. See LICENSE for details.
// 
// 

#include <string>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
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
      ChannAddr() { addr.port = 0; }
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
      void copyAddr(chann_addr_t *a) {
         if (a) {
            addr = *a;
            std::ostringstream os;
            os << a->port;
            addrString = string(a->ip, sizeof(a->ip)) + ":" + os.str();
         }
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
         mnet_init(0);
         m_chann = mnet_chann_open( channType(streamType) );
         m_handler = NULL;
      }

      Chann(Chann *c) {
         if (c) {
            m_chann = c->m_chann;
            m_handler = NULL;
            mnet_chann_set_cb(m_chann, Chann::channDispatchEvent);
            mnet_chann_set_opaque(m_chann, this);
            c->m_chann = NULL;
            c->m_handler = NULL;
         }
      }

      virtual ~Chann() {
         mnet_chann_close(m_chann);
      }


      /* build network 
       */
      bool channListen(string ipPort, int backlog = 16) {
         if (m_chann && ipPort.length()>0) {
            ChannAddr addr = ChannAddr(ipPort);
            mnet_chann_set_cb(m_chann, Chann::channDispatchEvent);
            mnet_chann_set_opaque(m_chann, this);
            return mnet_chann_listen(m_chann, addr.addr.ip, addr.addr.port, backlog);
         }
         return false;
      }
   
      bool channConnect(string ipPort) {
         if (m_chann && ipPort.length()>0) {
            m_peer = ChannAddr(ipPort);
            mnet_chann_set_cb(m_chann, Chann::channDispatchEvent);
            mnet_chann_set_opaque(m_chann, this);
            return mnet_chann_connect(m_chann, m_peer.addr.ip, m_peer.addr.port);
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
      rw_result_t* channRecv(void *buf, int len) {
         if (mnet_chann_state(m_chann) == CHANN_STATE_CONNECTED) {
            return mnet_chann_recv(m_chann, buf, len);
         }
         m_rw_dummy.ret = 0;
         m_rw_dummy.msg = NULL;
         return &m_rw_dummy;
      }

      rw_result_t* channSend(void *buf, int len) {
         if (mnet_chann_state(m_chann) == CHANN_STATE_CONNECTED) {
            return mnet_chann_send(m_chann, buf, len);
         }
         m_rw_dummy.ret = 0;
         m_rw_dummy.msg = NULL;
         return &m_rw_dummy;
      }


      /* event handler
       */

      // MNET_EVENT_SEND: event send while send buffer emtpy
      // MNET_EVENT_TIMER: micro seconds repeat event
      void channActiveEvent(chann_event_t event, int64_t value) {
         mnet_chann_active_event(m_chann, event, value);
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
      inline ChannAddr& myAddr(void) {
         if (m_addr.addr.port <= 0) {
            chann_addr_t addr;
            mnet_chann_socket_addr(m_chann, &addr);
            m_addr.copyAddr(&addr);
         }
         return m_addr;
      }
      inline ChannAddr& peerAddr(void) { return m_peer; };
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
               mnet_chann_set_cb(m->r, Chann::channDispatchEvent);
               mnet_chann_set_opaque(m->r, nc);
               m_handler(this, nc, m->event, 0);
            } else {
               m_handler(this, NULL, m->event, m->err);
            }
         } else {
            if (m->event == CHANN_EVENT_ACCEPT) {
               Chann *nc = new Chann(m->r);
               mnet_chann_set_cb(m->r, Chann::channDispatchEvent);
               mnet_chann_set_opaque(m->r, nc);
               defaultEventHandler(nc, m->event, m->err);
            } else {
               defaultEventHandler(NULL, m->event, m->err);
            }
         }
      }

      chann_t *m_chann;
      ChannAddr m_peer;            // peer addr
      ChannAddr m_addr;            // my addr
      channEventHandler m_handler; // external event handler
      rw_result_t m_rw_dummy;
   };


   class ChannDispatcher {
     public:
      
      // startEventLoop until user call stopEventLoop
      static void startEventLoop(void) {
         if ( !isRunning() ) {
            isRunning() = true;
            while ( isRunning() ) {
               mnet_poll(1000000);
            }
         }
      }      
      static void stopEventLoop(void) {
         isRunning() = false;
      }
      
      // pullEvent with waiting microseconds at most
      static poll_result_t* pollEvent(int microseconds) {
         return mnet_poll(microseconds);
      }

      static int64_t currentTime(void) {
         return mnet_current();
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
