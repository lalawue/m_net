/* 
 * Copyright (c) 2020 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifdef TEST_RECONNECT_PULL_STYLE

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mnet_core.h"

#define kMultiChannCount 256    // default for 'ulimit -n'

static void
_print_help(char *argv[]) {
   printf("%s: [-s|-c] [ip:port]\n", argv[0]);
}

static void
_as_server(chann_addr_t *addr) {
   chann_t *svr = mnet_chann_open(CHANN_TYPE_STREAM);
   mnet_chann_listen(svr, addr->ip, addr->port, 1);
   printf("svr listen %s:%d\n", addr->ip, addr->port);

   char buf[128];   
   poll_result_t *results = NULL;

   for (;;) {
      results = mnet_poll(1000000);
      if (results->chann_count < 0) {
         printf("poll error !\n");
         break;
      }

      chann_msg_t *msg = NULL;
      while ((msg = mnet_result_next(results))) {
         if (msg->n == svr) {
            // ignore this
            continue;
         }
         if (msg->event == CHANN_EVENT_RECV) {
            rw_result_t *rw = mnet_chann_recv(msg->n, buf, sizeof(buf));
            mnet_chann_active_event(msg->n, CHANN_EVENT_SEND, 1);
            mnet_chann_send(msg->n, buf, rw->ret);
         }
         else if (msg->event == CHANN_EVENT_SEND ||
                  msg->event == CHANN_EVENT_DISCONNECT)
         {
            printf("cnt %p disconnect\n", msg->n);
            mnet_chann_close(msg->n);
         }
      }
   }
}

static void
_as_client(chann_addr_t *addr) {
   for (size_t i=0; i<kMultiChannCount; i++) {
      chann_t *cnt = mnet_chann_open(CHANN_TYPE_STREAM);
      mnet_chann_set_opaque(cnt, (void *)i); /* set opaque */
      mnet_chann_connect(cnt, addr->ip, addr->port);
      printf("%d begin connect %s:%d\n", (int)i, addr->ip, addr->port);
   }

   char buf[128];
   poll_result_t *results = NULL;
   
   for (;;) {
      
      results = mnet_poll(1000000);
      if (results->chann_count <= 0) {
         break;
      }
      
      chann_msg_t *msg = NULL;      
      while ((msg = mnet_result_next(results))) {
         
         int idx = (int)msg->opaque;
         
         if (msg->event == CHANN_EVENT_CONNECTED) {
            int ret = snprintf(buf, sizeof(buf), "HelloServ %d", idx);
            if (ret == mnet_chann_send(msg->n, buf, ret)->ret) {
               printf("%d: connected, send '%s'\n", idx, buf);
            } else {
               printf("%d: connected, fail to send with ret %d\n", idx, ret);
               mnet_chann_close(msg->n);
            }
         }

         if (msg->event == CHANN_EVENT_DISCONNECT) {
            usleep(1000);
            printf("%d: disconnect, try to connect %s:%d\n", idx, addr->ip, addr->port);
            if ( !mnet_chann_connect(msg->n, addr->ip, addr->port) ) {
               printf("%d: disconnect, fail to connect !\n", idx);
               mnet_chann_close(msg->n);
            }
         }         
      }
   }
}

int
main(int argc, char *argv[]) {
   if (argc < 2) {
      _print_help(argv);
      return 0;
   }

   char *option = argv[1];
   char *ipport = argc > 2 ? argv[2] : "127.0.0.1:8090";

   chann_addr_t addr;
   if (mnet_parse_ipport(ipport, &addr) <= 0) {
      _print_help(argv);
      return 0;
   }

   mnet_init(1);                /* use pull style api */

   if (strcmp(option, "-s") == 0) {
      _as_server(&addr);
   } else if (strcmp(option, "-c") == 0) {
      _as_client(&addr);
   } else {
      _print_help(argv);
      return 0;
   }

   mnet_fini();

   return 0;
}



#endif  /* TEST_RECONNECT_PULL_STYLE */
