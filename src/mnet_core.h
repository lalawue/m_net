/*
 * Copyright (c) 2015 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef MNET_H
#define MNET_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MNET_BUF_SIZE (64*1024) /* 64kb default */
#define MNET_MILLI_SECOND (1000) /* milliseconds */

typedef enum {
   CHANN_TYPE_STREAM = 1,       /* TCP */
   CHANN_TYPE_DGRAM,            /* UDP */
   CHANN_TYPE_BROADCAST,
} chann_type_t;

typedef enum {
   CHANN_STATE_CLOSED = 0,      /* only for closed */
   CHANN_STATE_DISCONNECT,      /* for opened/disconnected */
   CHANN_STATE_CONNECTING,      /* for connecting */
   CHANN_STATE_CONNECTED,       /* for connected, can read/write data to peer */
   CHANN_STATE_LISTENING,       /* only for listening */
} chann_state_t;

typedef enum {
   CHANN_EVENT_RECV = 1,       /* socket has data to read */
   CHANN_EVENT_SEND,           /* socket send buf empty */
   CHANN_EVENT_ACCEPT,         /* socket accept */
   CHANN_EVENT_CONNECTED,      /* socket connected */
   CHANN_EVENT_DISCONNECT,     /* socket disconnect when EOF or error */
   CHANN_EVENT_TIMER,          /* user defined interval, highest priority */
} chann_event_t;

typedef struct s_chann chann_t;

typedef struct {
   chann_event_t event;         /* event type */
   int err;                     /* errno */
   chann_t *n;                  /* chann to emit event */
   chann_t *r;                  /* chann accept from remote */
   void *opaque;                /* user defined data */
} chann_msg_t;

typedef struct {
   char ip[17];
   int port;
} chann_addr_t;

typedef void (*chann_msg_cb)(chann_msg_t*);
typedef void (*mnet_log_cb)(chann_t*, int, const char *log_string);
typedef int (*mnet_balancer_cb)(void *context, int afd);

/* set allocator/log before init
 */
void mnet_allocator(void* (*new_malloc)(int),
                    void* (*new_realloc)(void*, int),
                    void  (*new_free)(void*));
/* 0:disable
 * 1:error
 * 2:info
 * 3:verbose
 */
void mnet_setlog(int level, mnet_log_cb);


/* init before use chann */
int mnet_init(void);
void mnet_fini(void);

/* return version */
int mnet_version();

/* report mnet chann status
 * 0: chann_count
 * 1: chann_detail
 */
int mnet_report(int level);

/* sync resolve host name, using after init under windows */
int mnet_resolve(const char *host, int port, chann_type_t ctype, chann_addr_t*);

/* multiprocessing accept balancer, return 0 in ac_before to disable accept */
void mnet_multi_accept_balancer(void *ac_context,
                              mnet_balancer_cb ac_before,
                              mnet_balancer_cb ac_after);

/* multiprocessing reset event queue */
void mnet_multi_reset_event();

/* dispatch chann event, milliseconds > 0, and it will cause
 * CHANN_EVENT_TIMER accurate
 * return opened chann count, -1 for error
 */
int mnet_poll(uint32_t milliseconds);

/* next msg after mnet_poll() */
chann_msg_t* mnet_result_next(void);

/* channel
 */
chann_t* mnet_chann_open(chann_type_t type); /* create chann */
void mnet_chann_close(chann_t *n);           /* destroy chann */

int mnet_chann_fd(chann_t *n);
chann_type_t mnet_chann_type(chann_t *n);

int mnet_chann_listen(chann_t *n, const char *host, int port, int backlog);

int mnet_chann_connect(chann_t *n, const char *host, int port);
void mnet_chann_disconnect(chann_t *n);

void mnet_chann_set_opaque(chann_t *n, void *opaque); /* user defined data, return with chann_msg_t */
void* mnet_chann_get_opaque(chann_t *n); /* user defined data in chann */

/* CHANN_EVENT_SEND: send buffer empty event, 0 to inactive, postive to active
 * CHANN_EVENT_TIMER: repeated timeout event, 0 to inactive, postive for milli second interval
 */
void mnet_chann_active_event(chann_t *n, chann_event_t et, int64_t value);

/* send/recv data, return -1 for error */
int mnet_chann_recv(chann_t *n, void *buf, int len);
int mnet_chann_send(chann_t *n, void *buf, int len); /* send will always cached would blocked data */

/* DGRAM send/recv data return -1 for error, and recv require listen first */
int mnet_dgram_recv(chann_t*, chann_addr_t *addr_in, void *buf, int len);
int mnet_dgram_send(chann_t*, chann_addr_t *addr_out, void *buf, int len);

/* cached bytes not send
 */
int mnet_chann_cached(chann_t *n);

int mnet_chann_state(chann_t *n);
long long mnet_chann_bytes(chann_t *n, int be_send);

/* underlying socket for chann
 */
int mnet_chann_socket_addr(chann_t *n, chann_addr_t*);
int mnet_chann_peer_addr(chann_t *n, chann_addr_t*);
int mnet_chann_socket_set_bufsize(chann_t *n, int bufsize); /* before listen/connect */

/* tools without init
 */
int64_t mnet_tm_current(void); /* micro seconds */
int mnet_parse_ipport(const char *ipport, chann_addr_t *addr);

/* Extension Interface
 *
 * mnet extension was a external defined chann_type_t build on top of
 * internal chann_type_t as STREAM/DGRAM/BROADCAST
 */

/* type/msg/operation/state/data handler
 */
typedef int (*mnet_ext_chann_raw_type)(void *ext_ctx, chann_type_t ctype); /* return STREAM/DGRAM/BROADCAST */
typedef int (*mnet_ext_chann_msg_filter)(void *ext_ctx, chann_msg_t *msg); /* return 0 to skip this event */
typedef void (*mnet_ext_chann_op_cb)(void *ext_ctx, chann_t *n); /* open/close/listen/accept/connect/disconnect */
typedef int (*mnet_ext_chann_state_wrapper)(void *ext_ctx, chann_t *n, int state); /* wrapper state */
typedef int (*mnet_ext_chann_data_wrapper)(void *ext_ctx, chann_t *n, void *buf, int len); /* wrapper recv/send */

/* context for ext chann_type_t
 */
typedef struct {
   uint8_t reserved;                      /* used by internal */
   void *ext_ctx;                         /* context for this chann_type, can be NULL */
   mnet_ext_chann_raw_type type_fn;       /* internal chann_type_t, NOT NULL */
   mnet_ext_chann_msg_filter filter_fn;   /* filter chann msg, NOT NULL */
   mnet_ext_chann_op_cb open_cb;          /* after open chann, NOT NULL */
   mnet_ext_chann_op_cb close_cb;         /* before close chann, NOT NULL */
   mnet_ext_chann_op_cb listen_cb;        /* after listen chann, NOT NULL */
   mnet_ext_chann_op_cb accept_cb;        /* after accept chann, NOT NULL */
   mnet_ext_chann_op_cb connect_cb;       /* after connect chann, NOT NULL */
   mnet_ext_chann_op_cb disconnect_cb;    /* before disconnect chann, NOT NULL */
   mnet_ext_chann_state_wrapper state_fn; /* actual state, NOT NULL */
   mnet_ext_chann_data_wrapper recv_fn;   /* internal recv, <0 for error, NOT NULL */
   mnet_ext_chann_data_wrapper send_fn;   /* internal send, <0 for error, NOT NULL */
} mnet_ext_t;

/* register chann ext type
 * ctype: chan extension type between (CHANN_TYPE_BROADCAST, 7]
 * ext: extension config, will be copied
 */
int mnet_ext_register(chann_type_t ctype, mnet_ext_t *ext);

/* set extension userdata for chann */
void mnet_ext_chann_set_ud(chann_t *n, void *ext_ud);

/* get extension userdata for chann */
void* mnet_ext_chann_get_ud(chann_t *n);

#ifdef __cplusplus
}
#endif

#endif
