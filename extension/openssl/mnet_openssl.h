/*
 * Copyright (c) 2022 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifndef MNET_OPENSSL_H
#define MNET_OPENSSL_H

#include "mnet_core.h"
#include <openssl/ssl.h>

typedef struct mnet_openssl mnet_openssl_t;

/** run these before ctx_config
 * 1. mnet_init()
 * 2. SSL_library_init()
 * 3. SSL_CTX_new()
 */
mnet_openssl_t* mnet_openssl_ctx_config(SSL_CTX *ctx);
void mnet_openssl_ctx_release(mnet_openssl_t *);

chann_t* mnet_openssl_chann_open(mnet_openssl_t *);
void mnet_openssl_chann_close(chann_t *);

int mnet_openssl_chann_fd(chann_t *n);

int mnet_openssl_chann_listen(chann_t *n, const char *host, int port, int backlog);

int mnet_openssl_chann_connect(chann_t *n, const char *host, int port);
void mnet_openssl_chann_disconnect(chann_t *n);

// when CHANN_EVENT_ACCEPT, msg->n would be NULL
void mnet_openssl_chann_set_cb(chann_t *n, chann_msg_cb cb);

void mnet_openssl_chann_set_opaque(chann_t *n, void *opaque);
void* mnet_openssl_chann_get_opaque(chann_t *n);

/* active openssl chann events after receive connected event
 * CHANN_EVENT_SEND: send buffer empty event, 0 to inactive, postive to active
 * CHANN_EVENT_TIMER: repeated timeout event, 0 to inactive, postive for milli second interval
 */
void mnet_openssl_chann_active_event(chann_t *n, chann_event_t et, int64_t value);

rw_result_t* mnet_openssl_chann_recv(chann_t *n, void *buf, int len);
rw_result_t* mnet_openssl_chann_send(chann_t *n, void *buf, int len);

int mnet_openssl_chann_state(chann_t *n);

#endif