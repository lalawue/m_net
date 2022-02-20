/*
 * Copyright (c) 2020 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifdef TEST_RWDATA_PULL_STYLE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "mnet_core.h"

#define kBufSize 256*1024 // even number
#define kSendedPoint 1024*1024*1024

typedef struct {
   int sended;
   int recved;
   uint8_t buf[kBufSize];
}  ctx_t;

static void
_print_data_info(ctx_t *ctx) {
   printf("received %d\n", ctx->recved);
   printf("sended %d\n", ctx->sended);
   printf("release self\n");
}

static int
_check_data_buf(ctx_t *ctx, int base, int len) {
   for (int i=0; i<len; i++) {
      if (ctx->buf[i] != ((base + i) & 0xff)) {
         return 0;
      }
   }
   return 1;
}

static void
_client_send_batch_data(chann_t *n, ctx_t *ctx) {
   for (int i=0; i<kBufSize; i++) {
      ctx->buf[i] = (ctx->sended + i) & 0xff;
   }
   rw_result_t *rw = mnet_chann_send(n, ctx->buf, kBufSize);
   if (rw->ret > 0) {
      assert(rw->ret == kBufSize);
      ctx->sended += rw->ret;
   }
}

static int
_client_recv_batch_data(chann_t *n, ctx_t *ctx) {
   rw_result_t *rw = mnet_chann_recv(n, ctx->buf, kBufSize);
   if (_check_data_buf(ctx, ctx->recved, rw->ret)) {
      ctx->recved += rw->ret;
      printf("c recved %d\n", ctx->recved);
      return 1;
   } else {
      printf("c failed to checked data: %d\n", rw->ret);
      return 0;
   }
}

static void
_as_client(chann_addr_t *addr) {
   chann_t *cnt = mnet_chann_open(CHANN_TYPE_STREAM);
   mnet_chann_connect(cnt, addr->ip, addr->port);
   printf("client connect %s:%d\n", addr->ip, addr->port);

   poll_result_t *results = NULL;
   ctx_t *ctx = malloc(sizeof(*ctx));
   memset(ctx, 0, sizeof(*ctx));

   for (;;) {

      results = mnet_poll(MNET_MILLI_SECOND);
      if (results->chann_count <= 0) {
         break;
      }

      chann_msg_t *msg = NULL;
      while ((msg = mnet_result_next(results))) {

         if (msg->event == CHANN_EVENT_CONNECTED) {
            printf("client connected\n");
            mnet_chann_active_event(msg->n, CHANN_EVENT_SEND, 1);
         }

         if (msg->event == CHANN_EVENT_RECV) {
            if (_client_recv_batch_data(msg->n, ctx) && ctx->recved < kSendedPoint) {
            } else {
               _print_data_info(ctx);
               mnet_chann_close(msg->n);
               continue;
            }
         }

         if (msg->event == CHANN_EVENT_SEND) {
            if (ctx->sended < kSendedPoint) {
               _client_send_batch_data(msg->n, ctx);
            } else {
               printf("send enough data %d\n", ctx->sended);
               mnet_chann_active_event(msg->n, CHANN_EVENT_SEND, 0);
            }
         }

         if (msg->event == CHANN_EVENT_DISCONNECT) {
            _print_data_info(ctx);
            mnet_chann_close(msg->n);
         }
      }
   }
}

static void
_as_server(chann_addr_t *addr) {
   chann_t *svr = mnet_chann_open(CHANN_TYPE_STREAM);
   mnet_chann_listen(svr, addr->ip, addr->port, 1);
   printf("svr listen %s:%d\n", addr->ip, addr->port);

   poll_result_t *results = NULL;
   ctx_t *ctx = malloc(sizeof(*ctx));
   memset(ctx, 0, sizeof(*ctx));

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
         if (msg->event == CHANN_EVENT_DISCONNECT) {
            _print_data_info(ctx);
            mnet_chann_close(msg->n);
            continue;
         }
         if (msg->event == CHANN_EVENT_RECV) {
            rw_result_t *rw = mnet_chann_recv(msg->n, ctx->buf, kBufSize);
            if (_check_data_buf(ctx, ctx->recved, rw->ret)) {
               int ret = rw->ret;
               ctx->recved += ret;
               rw = mnet_chann_send(msg->n, ctx->buf, ret);
               if (rw->ret > 0) {
                  ctx->sended += rw->ret;
                  assert(ctx->recved == ctx->sended);
               } else {
                  printf("invalid send\n");
                  mnet_chann_close(msg->n);
               }
            } else {
               printf("svr failed to recv rw code: %d\n", rw->ret);
               mnet_chann_close(msg->n);
            }
         }
      }
   }
}

static void
_print_help(char *argv[]) {
   printf("%s: [-s|-c] [ip:port]\n", argv[0]);
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

#endif  /* TEST_RWDATA_PULL_STYLE */
