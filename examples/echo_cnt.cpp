/* 
 * Copyright (c) 2017 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include <string.h>
#include "mnet_core.h"

#ifdef EXAMPLE_ECHO_CNT

static void
_on_cnt_event(chann_msg_t *msg) {
   if (msg->event == CHANN_EVENT_RECV) {
      char buf[256] = {0};
      rw_result *rw = mnet_chann_recv(msg->n, buf, 256);
      if (rw->ret > 0) {
         printf("%s", buf);;
         if ( fgets(buf, 256, stdin) ) {
            mnet_chann_send(msg->n, buf, strlen(buf));
         } else {
            mnet_chann_close(msg->n);
         }
      }
   }
   if (msg->event == CHANN_EVENT_DISCONNECT) {
      mnet_chann_close(msg->n);
   }
}

int
main(int argc, char *argv[]) {

   if (argc < 2) {
      printf("%s 'cnt_ip:port'\n", argv[0]);
   } else {
      chann_addr_t addr;
      if (mnet_parse_ipport(argv[1], &addr) > 0)  {
         mnet_init(0);
         chann_t *cnt = mnet_chann_open(CHANN_TYPE_STREAM);
         mnet_chann_set_cb(cnt, _on_cnt_event, NULL);

         printf("cnt try connect %s:%d...\n", addr.ip, addr.port);
         mnet_chann_connect(cnt, addr.ip, addr.port);
         while (mnet_poll(-1)->chann_count > 0) {
         }
         mnet_fini();
      }
   }
   return 0;
}

#endif  /* EXAMPLE_ECHO_CNT */
