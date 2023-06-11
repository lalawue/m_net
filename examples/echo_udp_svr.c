/*
 * Copyright (c) 2023 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifdef EXAMPLE_ECHO_UDP_SVR_C

#include <stdio.h>
#include "mnet_core.h"

int main(int argc, char *argv[]) {
   const char *ipaddr = argc > 1 ? argv[1] : "127.0.0.1:8090";

   chann_addr_t addr;
   if (mnet_parse_ipport(ipaddr, &addr) <= 0) {
      return 0;
   }

   mnet_init();

   chann_t *svr = mnet_chann_open(CHANN_TYPE_DGRAM);
   char buf[256];

   mnet_chann_listen(svr, addr.ip, addr.port, 100);
   printf("mnet version %d\n", mnet_version());
   printf("udp svr start listen: %s\n", ipaddr);

   // server will receive timer event every 5 second
   mnet_chann_active_event(svr, CHANN_EVENT_TIMER, 5 * MNET_MILLI_SECOND);

   int msg_count = 0;
   for (;;) {

      if (mnet_poll(0.1 * MNET_MILLI_SECOND) <= 0) {
         break;
      }

      chann_msg_t *msg = NULL;
      while ((msg = mnet_result_next())) {
         /* client event */
         if (msg->event == CHANN_EVENT_RECV) {
            msg_count += 1;
            chann_addr_t addr;
            int ret = mnet_dgram_recv(msg->n, &addr, buf, 256);
            if (msg_count <= 1) {
               char welcome[] = "Welcome to UDP echoServ\n";
               mnet_dgram_send(msg->n, &addr, welcome, sizeof(welcome));
            } else {
               mnet_dgram_send(msg->n, &addr, buf, ret);
            }
         }
      }
   }

   mnet_fini();

   return 0;
}

#endif  /* EXAMPLE_ECHO_SVR_UDP_C */
