/*
 * Copyright (c) 2023 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifdef EXAMPLE_ECHO_UDP_CNT_C

#include <stdio.h>
#include <string.h>
#include "mnet_core.h"

static void
_on_cnt_event(chann_msg_t *msg) {
   if (msg->event == CHANN_EVENT_RECV) {
      char buf[256] = {0};
      int ret = mnet_chann_recv(msg->n, buf, 256);
      if (ret > 0) {
         printf("%s", buf);
         if ( fgets(buf, 256, stdin) ) {
            mnet_chann_send(msg->n, buf, strlen(buf));
         } else {
            mnet_chann_close(msg->n);
         }
      }
   }
}

int
main(int argc, char *argv[]) {
   const char *ipaddr = argc > 1 ? argv[1] : "127.0.0.1:8090";
   chann_addr_t addr;

   if (mnet_parse_ipport(ipaddr, &addr) > 0)  {
      mnet_init();
      chann_t *cnt = mnet_chann_open(CHANN_TYPE_DGRAM);

      printf("mnet version %d\n", mnet_version());
      printf("udp cnt always connected %s:%d --\n", addr.ip, addr.port);

      //mnet_chann_connect(cnt, addr.ip, addr.port);
      mnet_dgram_send(cnt, &addr, ".", 1);

      while (1) {
         if (mnet_poll(1000) <= 0) {
            break;
         }
         chann_msg_t *msg = NULL;
         while ((msg = mnet_result_next())) {
            _on_cnt_event(msg);
         }
      }
      mnet_fini();
   }

   return 0;
}

#endif  /* EXAMPLE_ECHO_UDP_CNT_C */
