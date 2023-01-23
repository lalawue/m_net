/*
 * Copyright (c) 2017 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

/* NTP source code from https://github.com/edma2/ntp
 *
 * Thanks Eugene Ma (edma2)
 */

#ifdef EXAMPLE_NTP

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "mnet_core.h"

#define K_ONE_SECOND (1000000)

#define NTP_VERSION 		0xe3
#define UNIX_OFFSET 		2208988800L
#define VN_BITMASK(byte) 	((byte & 0x3f) >> 3)
#define LI_BITMASK(byte) 	(byte >> 6)
#define MODE_BITMASK(byte) 	(byte & 0x7)
#define ENDIAN_SWAP32(data)  	((data >> 24) | /* right shift 3 bytes */ \
				((data & 0x00ff0000) >> 8) | /* right shift 1 byte */ \
			        ((data & 0x0000ff00) << 8) | /* left shift 1 byte */ \
				((data & 0x000000ff) << 24)) /* left shift 3 bytes */

struct ntpPacket {
	uint8_t flags;
	uint8_t stratum;
	uint8_t poll;
	uint8_t precision;
	uint32_t root_delay;
	uint32_t root_dispersion;
	uint8_t referenceID[4];
	uint32_t ref_ts_sec;
	uint32_t ref_ts_frac;
	uint32_t origin_ts_sec;
	uint32_t origin_ts_frac;
	uint32_t recv_ts_sec;
	uint32_t recv_ts_frac;
	uint32_t trans_ts_sec;
	uint32_t trans_ts_frac;
} __attribute__((__packed__)); /* this is not strictly necessary,
				* structure follows alignment rules */

typedef struct {
   int second_left;
   chann_addr_t addr;
   struct ntpPacket packet;
} ntp_ctx_t;


static void
_ntp_parse_response(ntp_ctx_t *ctx) {

   time_t total_secs;
   struct tm *now;

   /* correct for right endianess */
   ctx->packet.root_delay = ENDIAN_SWAP32(ctx->packet.root_delay);
   ctx->packet.root_dispersion = ENDIAN_SWAP32(ctx->packet.root_dispersion);
   ctx->packet.ref_ts_sec = ENDIAN_SWAP32(ctx->packet.ref_ts_sec);
   ctx->packet.ref_ts_frac = ENDIAN_SWAP32(ctx->packet.ref_ts_frac);
   ctx->packet.origin_ts_sec = ENDIAN_SWAP32(ctx->packet.origin_ts_sec);
   ctx->packet.origin_ts_frac = ENDIAN_SWAP32(ctx->packet.origin_ts_frac);
   ctx->packet.recv_ts_sec = ENDIAN_SWAP32(ctx->packet.recv_ts_sec);
   ctx->packet.recv_ts_frac = ENDIAN_SWAP32(ctx->packet.recv_ts_frac);
   ctx->packet.trans_ts_sec = ENDIAN_SWAP32(ctx->packet.trans_ts_sec);
   ctx->packet.trans_ts_frac = ENDIAN_SWAP32(ctx->packet.trans_ts_frac);

   /* print raw data */
   printf("RAW data below:\n----------------\n");
   printf("LI: %u\n", LI_BITMASK(ctx->packet.flags));
   printf("VN: %u\n", VN_BITMASK(ctx->packet.flags));
   printf("Mode: %u\n", MODE_BITMASK(ctx->packet.flags));
   printf("stratum: %u\n", ctx->packet.stratum);
   printf("poll: %u\n", ctx->packet.poll);
   printf("precision: %u\n", ctx->packet.precision);
   printf("root delay: %u\n", ctx->packet.root_delay);
   printf("root dispersion: %u\n", ctx->packet.root_dispersion);
   printf("reference ID: %u.", ctx->packet.referenceID[0]);
   printf("%u.", ctx->packet.referenceID[1]);
   printf("%u.", ctx->packet.referenceID[2]);
   printf("%u\n", ctx->packet.referenceID[3]);
   printf("reference timestamp: %u.", ctx->packet.ref_ts_sec);
   printf("%u\n", ctx->packet.ref_ts_frac);
   printf("origin timestamp: %u.", ctx->packet.origin_ts_sec);
   printf("%u\n", ctx->packet.origin_ts_frac);
   printf("receive timestamp: %u.", ctx->packet.recv_ts_sec);
   printf("%u\n", ctx->packet.recv_ts_frac);
   printf("transmit timestamp: %u.", ctx->packet.trans_ts_sec);
   printf("%u\n", ctx->packet.trans_ts_frac);

   /* print date with receive timestamp */
   unsigned int recv_secs = ctx->packet.recv_ts_sec - UNIX_OFFSET; /* convert to unix time */
   total_secs = recv_secs;
   printf("\nUnix time: %u\n", (unsigned int)total_secs);
   now = localtime(&total_secs);
   printf("%02d/%02d/%d %02d:%02d:%02d\n", now->tm_mday, now->tm_mon+1, \
          now->tm_year+1900, now->tm_hour, now->tm_min, now->tm_sec);
   exit(0);
}


static void
_ntp_event_cb(chann_msg_t *e) {
   ntp_ctx_t *ctx = (ntp_ctx_t*)e->opaque;

   if (e->event == CHANN_EVENT_RECV) {
      int ret = mnet_chann_recv(e->n, &ctx->packet, sizeof(ctx->packet));
      if (ret == sizeof(ctx->packet)) {
         printf("ntp: get response from ntp server:\n\n");
         _ntp_parse_response(ctx);
      } else {
         printf("ntp: recieved invalid data length !\n");
      }

      mnet_chann_close(e->n);;
   }
}

static void
_ntp_loop(ntp_ctx_t *ctx) {

   // create chann
   chann_t *udp = mnet_chann_open(CHANN_TYPE_DGRAM);
   if (udp == NULL) {
      printf("ntp: fail to create chann !\n");
      return;
   }

   // set event callback
   //mnet_chann_set_cb(udp, _ntp_event_cb);
   mnet_chann_set_opaque(udp, ctx);

   // try connect ntp server
   if ( mnet_chann_connect(udp, ctx->addr.ip, ctx->addr.port) ) {
      printf("ntp: try connect to '%s:123'\n", ctx->addr.ip);
   } else {
      printf("ntp: fail to connect to '%s:123'\n", ctx->addr.ip);
      return;
   }

   // init packet
   memset(&ctx->packet, 0, sizeof(struct ntpPacket));
   ctx->packet.flags = NTP_VERSION;

   // send ntp request
   int ret = mnet_chann_send(udp, &ctx->packet, sizeof(ctx->packet));
   if (ret != sizeof(ctx->packet)) {
      printf("ntp: fail to send ntp request !\n");
      return;
   }

   // wait event
   ctx->second_left = 15;

   for (;;) {

      if (mnet_poll( K_ONE_SECOND ) == 0) {
         break;
      }

      chann_msg_t *msg = NULL;
      while ((msg = mnet_result_next())) {
         _ntp_event_cb(msg);
      }

      ctx->second_left -= 1;

      if (ctx->second_left > 0) {
         printf("ntp: timeout in %d second\n", ctx->second_left);
      } else {
         printf("ntp: timeout to get ntp server response !\n");
         break;
      }
   }
}

int main(int argc, char *argv[]) {
   ntp_ctx_t ctx;
   chann_addr_t addr;

   printf("\n try resolve 'cn.ntp.org.cn' ...\n\n");

   mnet_init();

   if ( mnet_resolve("cn.ntp.org.cn", 123, CHANN_TYPE_DGRAM, &addr) ) {

      memset(&ctx, 0, sizeof(ctx));
      ctx.addr = addr;

      _ntp_loop(&ctx);
   }

   mnet_fini();

   return 0;
}

#endif  /* EXAMPLE_NTP */
