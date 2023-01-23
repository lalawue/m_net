/*
 * Copyright (c) 2022 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifdef MNET_TLS_TEST_RECONNECT_C

#define _BSD_SOURCE
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "mnet_tls.h"

#define kMultiChannCount 256  // default for 'ulimit -n'
#define kTestCount 5          // disconnect/connect count for each chann

typedef struct {
   int idx;
   int count;
} ctx_t;

static void
_print_help(char *argv[]) {
   printf("%s: [-s|-c] [ip:port]\n", argv[0]);
}

static void
_as_server(chann_addr_t *addr) {
   chann_t *svr = mnet_chann_open(CHANN_TYPE_TLS);
   mnet_chann_listen(svr, addr->ip, addr->port, kMultiChannCount * kTestCount);
   printf("svr listen %s:%d\n", addr->ip, addr->port);

   char buf[128];

   for (;;) {
      if (mnet_poll(MNET_MILLI_SECOND) < 0) {
         printf("poll error !\n");
         break;
      }

      chann_msg_t *msg = NULL;
      while ((msg = mnet_result_next())) {
         if (msg->n == svr) {
            if (msg->event == CHANN_EVENT_ACCEPT) {
               printf("accept cnt %p\n", msg->r);
            }
            continue;
         }
         if (msg->event == CHANN_EVENT_RECV) {
            int len = mnet_chann_recv(msg->n, buf, sizeof(buf));
            if (len > 0) {
               mnet_chann_active_event(msg->n, CHANN_EVENT_SEND, 1);
               mnet_chann_send(msg->n, buf, len);
            }
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
      chann_t *cnt = mnet_chann_open(CHANN_TYPE_TLS);
      ctx_t *ctx = calloc(1, sizeof(ctx_t));
      ctx->idx = i;
      mnet_chann_set_opaque(cnt, ctx); /* set opaque */
      if (mnet_chann_connect(cnt, addr->ip, addr->port)) {
         printf("%d begin connect %s:%d\n", (int)i, addr->ip, addr->port);
      } else {
         mnet_chann_close(cnt);
      }
   }

   char buf[128];

   for (;;) {

      if (mnet_poll(MNET_MILLI_SECOND) <= 0) {
         printf("all cnt tested, exit !\n");
         break;
      }

      chann_msg_t *msg = NULL;
      while ((msg = mnet_result_next())) {

         ctx_t *ctx = (ctx_t *)msg->opaque;

         if (msg->event == CHANN_EVENT_CONNECTED) {
            int ret = snprintf(buf, sizeof(buf), "HelloServ %d", ctx->idx);
            if (ret == mnet_chann_send(msg->n, buf, ret)) {
               printf("%d: connected, send '%s'\n", ctx->idx, buf);
            } else {
               printf("%d: connected, fail to send with ret %d\n", ctx->idx, ret);
               mnet_chann_close(msg->n);
            }
         }

         if (msg->event == CHANN_EVENT_DISCONNECT) {

            ctx->count += 1;
            if (ctx->count >= kTestCount) {
               mnet_chann_close(msg->n);
               continue;
            }

            usleep(1000);

            printf("%d: disconnect, try to connect %s:%d\n", ctx->idx, addr->ip, addr->port);
            if ( !mnet_chann_connect(msg->n, addr->ip, addr->port) ) {
               printf("%d: disconnect, fail to connect !\n", ctx->idx);
               mnet_chann_close(msg->n);
            }
         }
      }
   }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        _print_help(argv);
        return 0;
    }

    char *option = argv[1];
    char *ipport = argc > 2 ? argv[2] : "127.0.0.1:8090";

    chann_addr_t addr;
    if (mnet_parse_ipport(ipport, &addr) <= 0)
    {
        _print_help(argv);
        return 0;
    }

    mnet_init(); /* use pull style api */

    SSL_library_init();
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_method());
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    SSL_CTX_load_verify_locations(ctx, "examples/openssl/ca.crt", NULL);
    SSL_CTX_use_certificate_file(ctx, "examples/openssl/server.crt", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ctx, "examples/openssl/server.key", SSL_FILETYPE_PEM);
    assert(SSL_CTX_check_private_key(ctx) == 1);

    if (!mnet_tls_config(ctx)) {
        printf("cnt failed to config tls !\n");
        return 0;
    }

    if (strcmp(option, "-s") == 0)
    {
        _as_server(&addr);
    }
    else if (strcmp(option, "-c") == 0)
    {
        _as_client(&addr);
    }
    else
    {
        _print_help(argv);
        return 0;
    }

    mnet_fini();

    return 0;
}

#endif /* MNET_TLS_TEST_RECONNECT_C */