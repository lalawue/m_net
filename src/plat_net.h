/* 
 * Copyright (c) 2015 lalawue
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef MNET_H
#define MNET_H

#ifndef MNET_BUF_SIZE
#define MNET_BUF_SIZE (64*1024) /* 64kb */
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
   CHANN_TYPE_STREAM = 1,
   CHANN_TYPE_DGRAM,
   CHANN_TYPE_BROADCAST,
} chann_type_t;

typedef enum {
   CHANN_STATE_CLOSED = 0,
   CHANN_STATE_DISCONNCT,
   CHANN_STATE_CONNECTING,
   CHANN_STATE_CONNECTED,
   CHANN_STATE_LISTENING,
} chann_state_t;

typedef enum {
   MNET_EVENT_RECV = 1,     /* socket has data to read */
   MNET_EVENT_SEND,         /* socket send buf empty, inactive default */
   MNET_EVENT_ACCEPT,       /* socket accept */
   MNET_EVENT_CONNECTED,    /* socket connected */
   MNET_EVENT_DISCONNECT,   /* socket disconnect */
   MNET_EVENT_ERROR,        /* socket error */
} mnet_event_type_t;

typedef struct s_mchann chann_t;
typedef struct {
   mnet_event_type_t event;     /* event type */
   int err;                     /* errno */
   chann_t *n;                  /* chann emit the event */
   chann_t *r;                  /* chann accept from remote */
   void *opaque;                /* opaque in set_cb */
} chann_event_t;

typedef void (*chann_cb)(chann_event_t*);

/* init before use */
int mnet_init(void);
void mnet_fini(void);

#define MNET_ONE_SECOND_BIT 20  /* 1 seconds == (1<<20) microseconds */

/* using poll to get data received/sended event */
int mnet_poll(int microseconds);
int mnet_report(int level);

/* channel */
chann_t* mnet_chann_open(chann_type_t type);
void mnet_chann_close(chann_t *n);

int mnet_chann_state(chann_t *n);

int mnet_chann_connect(chann_t *n, const char *host, int port);
#define mnet_chann_to(n,h,p) mnet_chann_connect(n,h,p) /* for UDP */

void mnet_chann_disconnect(chann_t *n);

int mnet_chann_listen_ex(chann_t *n, const char *host, int port, int backlog);
#define mnet_chann_listen(n, p) mnet_chann_listen_ex(n, NULL, p, 5)

void mnet_chann_set_cb(chann_t *n, chann_cb cb, void *opaque);
void mnet_chann_active_event(chann_t *n, mnet_event_type_t et, int active);

int mnet_chann_recv(chann_t *n, void *buf, int len);
int mnet_chann_send(chann_t *n, void *buf, int len);

int mnet_chann_cached(chann_t *n);
char* mnet_chann_addr(chann_t *n);
int mnet_chann_port(chann_t *n);

long long mnet_chann_bytes(chann_t *n, int be_send);

#ifdef __cplusplus
}
#endif

#endif
