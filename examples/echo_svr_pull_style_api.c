/* 
 * Copyright (c) 2019 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include "mnet_core.h"

#ifdef EXAMPLE_ECHO_SVR_PULL_STYLE_API

int main(int argc, char *argv[]) {
   if (argc < 2) {
      printf("%s: 'svr_ip:port'", argv[0]);
      return 0;
   }

   chann_addr_t addr;   
   if (mnet_parse_ipport(argv[1], &addr) <= 0) {
      return 0;
   }

   mnet_init(1);                /* use pull style api */

   chann_t *svr = mnet_chann_open(CHANN_TYPE_STREAM);
   poll_result_t *results = NULL;
   char buf[256];

   mnet_chann_listen(svr, addr.ip, addr.port, 2);
   printf("svr start listen: %s\n", argv[1]);

   while (1) {
      results = mnet_poll(100000);
      if (results->chann_count <= 0) {
         break;
      }

      for (chann_msg_t *msg=results->msg; msg; msg=(chann_msg_t *)msg->opaque) {
         switch (msg->event) {
            case CHANN_EVENT_ACCEPT: {
               if (msg->n == svr) {
                  /* sever accept client */
                  chann_addr_t addr;
                  char welcome[] = "Welcome to echoServ\n";
                  mnet_chann_send(msg->r, welcome, sizeof(welcome));
                  mnet_chann_addr(msg->r, &addr);
                  printf("svr accept cnt with chann %s:%d\n", addr.ip, addr.port);
               }
               break;
            }
            case CHANN_EVENT_RECV: {
               rw_result_t *rw = mnet_chann_recv(msg->n, buf, 256);
               mnet_chann_send(msg->n, buf, rw->ret);
               break;
            }
            case CHANN_EVENT_DISCONNECT: {
               chann_addr_t addr;
               mnet_chann_addr(msg->n, &addr);
               printf("svr disconnect cnt with chann %s:%d\n", addr.ip, addr.port);
               mnet_chann_close(msg->n);
               break;
            }
            default: {
               break;
            }
         }
      }
   }

   mnet_fini();

   return 0;
}

#endif  /* EXAMPLE_ECHO_SVR_PULL_STYLE */
