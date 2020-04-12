/* 
 * Copyright (c) 2020 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifdef TEST_MULTICHANNS_PULL_STYLE

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

   int cnt_count = 0;
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
            if (msg->event == CHANN_EVENT_ACCEPT) {
               cnt_count += 1;
               printf("accept cnt %p, count %d\n", msg->r, cnt_count);
            }
            continue;
         }
         if (msg->event == CHANN_EVENT_RECV) {
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
      if (mnet_chann_connect(cnt, addr->ip, addr->port)) {
         usleep(10);            // for system resources busy
      } else {
         mnet_chann_close(cnt);
      }
   }

   char buf[128];
   
   for (;;) {
      
      poll_result_t *results = mnet_poll(1000);
      if (results->chann_count <= 0) {
         printf("all cnt connected, send, recv done, exit !\n");
         break;
      }

      chann_msg_t *msg = NULL;      
      while ((msg = mnet_result_next(results))) {
         if (mnet_chann_state(msg->n) == CHANN_STATE_CLOSED) {
            continue;
         }
         
         int idx = (int)msg->opaque;

         if (msg->event == CHANN_EVENT_CONNECTED) {
            int ret = snprintf(buf, sizeof(buf), "HelloServ %d", idx);
            printf("%d: connected, send '%s'\n", idx, buf);
            mnet_chann_send(msg->n, buf, ret);
            continue;
         }

         if (msg->event == CHANN_EVENT_RECV) {
            mnet_chann_recv(msg->n, buf, sizeof(buf));
            printf("%d: recv '%s'\n", idx, buf);
            continue;
         }

         mnet_chann_close(msg->n);
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

#endif  /* TEST_MULTICHANNS_PULL_STYLE */
