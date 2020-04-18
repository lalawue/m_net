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
#define MNET_SECOND_MS (1000000) /* micro seconds */

typedef enum {
   CHANN_TYPE_STREAM = 1,       /* TCP */
   CHANN_TYPE_DGRAM,            /* UDP */
   CHANN_TYPE_BROADCAST,
} chann_type_t;

typedef enum {
   CHANN_STATE_CLOSED = 0,      /* only for closed */
   CHANN_STATE_DISCONNECT,      /* for opened/disconnected */
   CHANN_STATE_CONNECTING,      /* for connecting */
   CHANN_STATE_CONNECTED,       /* for connected */
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

typedef struct s_mchann chann_t;

typedef struct s_chann_msg {
   chann_event_t event;         /* event type */
   int err;                     /* errno */
   chann_t *n;                  /* chann to emit event */
   chann_t *r;                  /* chann accept from remote */
   void *opaque;                /* user defined data */
} chann_msg_t;

typedef struct {
   int chann_count;             /* active chann count, -1 for error */
   void *reserved;              /* reserved for mnet */
} poll_result_t;

typedef struct {
   int ret;                     /* recv/send data length, -1 for error */
   chann_msg_t *msg;            /* for pull style */
} rw_result_t;

typedef struct {
   char ip[16];
   int port;
} chann_addr_t;

typedef void (*chann_msg_cb)(chann_msg_t*);
typedef void (*mnet_log_cb)(chann_t*, int, const char *log_string);

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


/* init before use chann
 * 0: callback style
 * 1: pull style API
 */   
int mnet_init(int);
void mnet_fini(void);

/* report mnet chann status
 * 0: chann_count 
 * 1: chann_detail
 */
int mnet_report(int level);

/* micro seconds */
int64_t mnet_current(void);

/* dispatch chann event, microseconds > 0, and it will cause
 * CHANN_EVENT_TIMER accurate
*/
poll_result_t* mnet_poll(uint32_t microseconds);

/* next msg for pull style */   
chann_msg_t* mnet_result_next(poll_result_t *result);

/* channel
 */
chann_t* mnet_chann_open(chann_type_t type); /* create chann */
void mnet_chann_close(chann_t *n);           /* destroy chann */

int mnet_chann_fd(chann_t *n);
chann_type_t mnet_chann_type(chann_t *n);

int mnet_chann_listen(chann_t *n, const char *host, int port, int backlog);

int mnet_chann_connect(chann_t *n, const char *host, int port);
void mnet_chann_disconnect(chann_t *n);

void mnet_chann_set_cb(chann_t *n, chann_msg_cb cb); /* only for callback style */   
void mnet_chann_set_opaque(chann_t *n, void *opaque); /* user defined data, return with chann_msg_t */

/* CHANN_EVENT_SEND: send buffer empty event, 0 to inactive, postive to active
 * CHANN_EVENT_TIMER: repeated timeout event, 0 to inactive, postive for micro second interval
 */
void mnet_chann_active_event(chann_t *n, chann_event_t et, int64_t value);

rw_result_t* mnet_chann_recv(chann_t *n, void *buf, int len);
rw_result_t* mnet_chann_send(chann_t *n, void *buf, int len); /* always cached would blocked data */

int mnet_chann_set_bufsize(chann_t *n, int bufsize); /* set socket bufsize */
int mnet_chann_cached(chann_t *n);

int mnet_chann_addr(chann_t *n, chann_addr_t*);

int mnet_chann_state(chann_t *n);
long long mnet_chann_bytes(chann_t *n, int be_send);


/* tools without init
 */
int mnet_resolve(char *host, int port, chann_type_t ctype, chann_addr_t*);
int mnet_parse_ipport(const char *ipport, chann_addr_t *addr);

#ifdef __cplusplus
}
#endif

#endif
