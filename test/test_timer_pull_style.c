/* 
 * Copyright (c) 2020 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifdef TEST_TIMER_PULL_STYLE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mnet_core.h"

#define kBufSize 128
#define kMultiChannCount 256    // default for 'ulimit -n'

typedef struct {
   int idx;
   int count;
   int64_t connected_time;
   int64_t duration;
} ctx_t;

static void
_print_help(char *argv[]) {
   printf("%s: [-s|-c] [ip:port]\n", argv[0]);
}

static void
_as_server(chann_addr_t *addr) {
   chann_t *svr = mnet_chann_open(CHANN_TYPE_STREAM);
   mnet_chann_listen(svr, addr->ip, addr->port, kMultiChannCount);
   printf("svr listen %s:%d\n", addr->ip, addr->port);

   char buf[kBufSize];
   poll_result_t *results = NULL;

   for (;;) {
      results = mnet_poll(0.5 * MNET_SECOND_MS);
      if (results->chann_count < 0) {
         printf("poll error !\n");
         break;
      }

      chann_msg_t *msg = NULL;
      while ((msg = mnet_result_next(results))) {
         if (msg->n == svr) {
            if (msg->event == CHANN_EVENT_ACCEPT) {
               printf("accept cnt %p\n", msg->r);
            }
         } else {
            if (msg->event == CHANN_EVENT_RECV) {
               rw_result_t *rw = mnet_chann_recv(msg->n, buf, kBufSize);
               mnet_chann_send(msg->n, buf, rw->ret);
            }
         }
      }
   }
}

static void
_as_client(chann_addr_t *addr) {
   for (size_t i=0; i<kMultiChannCount; i++) {
      chann_t *cnt = mnet_chann_open(CHANN_TYPE_STREAM);
      ctx_t *ctx = calloc(1, sizeof(ctx_t));
      ctx->idx = i;
      mnet_chann_set_opaque(cnt, ctx); /* set opaque */
      if (mnet_chann_connect(cnt, addr->ip, addr->port)) {
         printf("%d begin try connect %s:%d\n", (int)i, addr->ip, addr->port);
         int64_t interval = ((random() + i) % 5 + 1) * MNET_SECOND_MS;;
         mnet_chann_active_event(cnt, CHANN_EVENT_TIMER, interval);         
      } else {
         mnet_chann_close(cnt);
      }
   }

   char buf[kBufSize];
   poll_result_t *results = NULL;
   
   for (;;) {
      results = mnet_poll(0.5 * MNET_SECOND_MS);
      if (results->chann_count <= 0) {
         printf("all cnt tested, exit !\n");
         break;
      }
      
      chann_msg_t *msg = NULL;      
      while ((msg = mnet_result_next(results))) {

         ctx_t *ctx = (ctx_t *)msg->opaque;
         
         if (msg->event == CHANN_EVENT_CONNECTED) {
            ctx->connected_time = mnet_current();
            printf("%d: connected time %lld\n", ctx->idx, ctx->connected_time);
         } else if (msg->event == CHANN_EVENT_RECV) {
            mnet_chann_recv(msg->n, buf, kBufSize);
            printf("%d: recv '%s'\n", ctx->idx, buf);
         } else if (msg->event == CHANN_EVENT_TIMER) {
            ctx->duration = (mnet_current() - ctx->connected_time) / MNET_SECOND_MS;
            if (ctx->duration > 10) {
               printf("%d: over 10 seconds, time %lld\n", ctx->idx, mnet_current());
               mnet_chann_close(msg->n);
            } else {
               int ret = snprintf(buf, kBufSize, "HelloServ duration %lld", ctx->duration);
               mnet_chann_send(msg->n, buf, ret);
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
