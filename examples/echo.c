/* 
 * Copyright (c) 2017 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include <string.h>
#include "plat_net.h"

#ifdef EXAMPLE_ECHO

typedef struct {
   int is_server;
   char ip_addr[16];
   int client_count;
} echo_ctx_t;


/* server side
 */

static void
_server_event_cb(chann_event_t *e) {
   echo_ctx_t *ctx = e->opaque;

   if (e->event == MNET_EVENT_ACCEPT) {

      // client accepted event

      ctx->client_count += 1;
      printf("svr: client %p accepted, count %d\n", e->r, ctx->client_count);

      mnet_chann_set_cb(e->r, _server_event_cb, e->opaque);
   }
   else if (e->event == MNET_EVENT_RECV) {

      // client data arrived event
      
      char buf[128] = {0};
      if ( mnet_chann_recv(e->n, buf, 128) ) {
         printf("svr: client %p recv %s", e->n, buf);
         mnet_chann_send(e->n, buf, strlen(buf));
      }
   }
   else if (e->event == MNET_EVENT_DISCONNECT) {

      // client exit event

      ctx->client_count -= 1;
      printf("svr: client %p exit, count %d\n", e->n, ctx->client_count);
      mnet_chann_close(e->n);;
   }
}

static void
_server_loop(echo_ctx_t *ctx) {

   // create chann
   chann_t *tcp_listen = mnet_chann_open(CHANN_TYPE_STREAM);
   if (tcp_listen == NULL) {
      printf("svr: fail to open chann !\n");
      return;
   }

   // set event callback
   mnet_chann_set_cb(tcp_listen, _server_event_cb, ctx);

   // listen ip:port
   if ( mnet_chann_listen_ex(tcp_listen, ctx->ip_addr, 8090, 1) ) {
      printf("svr: listen port 8090\n");
   } else {
      printf("svr: fail to listen !\n");
   }

   // wait event
   for (;;) {
      mnet_poll( -1 );
   }
}


/* client side
 */

static void
_client_event_cb(chann_event_t *e) {

   if (e->event == MNET_EVENT_CONNECTED) {

      // connected server

      printf("client: connected server.\n");
   }
   else if (e->event == MNET_EVENT_RECV) {

      // server data arrived

      char buf[128] = {0};
      if ( mnet_chann_recv(e->n, buf, 128) ) {
         printf("client: recv %s", buf);
      }
   }
}

static void
_client_loop(echo_ctx_t *ctx) {

   // create chann
   chann_t *tcp_echo = mnet_chann_open(CHANN_TYPE_STREAM);
   if (tcp_echo == NULL) {
      printf("client: fail to open chann !\n");
   }

   // set event callback
   mnet_chann_set_cb(tcp_echo, _client_event_cb, ctx);
   
   // try connect ip:port
   if ( mnet_chann_connect(tcp_echo, ctx->ip_addr, 8090) ) {
      printf("client: try connect to '%s:8090'\n", ctx->ip_addr);
   } else {
      printf("client: fail to connect to '%s:8090' !\n", ctx->ip_addr);
   }

   for (;;) {
      char buf[128] = { 0 };

      // wait event
      mnet_poll( -1 );

      // get data from user input
      if ( fgets(buf, 128, stdin) ) {
         if ( mnet_chann_send(tcp_echo, buf, strlen(buf)) <= 0) {
            printf("client: fail to send '%s'\n", buf);
         }
      } else {
         printf("client: stdin EOF\n");
         break;
      }
   }
}

int
main(int argc, char *argv[]) {
   echo_ctx_t ctx;

   if (argc < 3) {
      printf("%s -s server_ip | -c server_ip\n", argv[0]);
      return 0;
   }


   // get server ip
   memset(&ctx, 0, sizeof(ctx));

   if (strncmp(argv[1], "-s", 2) == 0) {
      ctx.is_server = 1;
   } else {
      ctx.is_server = 0;
   }

   strncpy(ctx.ip_addr, argv[2], 16);


   // init before use
   mnet_init();

   if (ctx.is_server) {
      _server_loop(&ctx);
   } else {
      _client_loop(&ctx);
   }

   mnet_fini();

   return 0;
}

#endif  /* EXAMPLE_ECHO */
