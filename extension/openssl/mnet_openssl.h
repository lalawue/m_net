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

/*
 * run mnet_init() first !
 */

/* config ssl from ctx builder */
typedef void (*mnet_openssl_configurator)(SSL_CTX *ctx);

/* create openssl SSL_CTX, call user to config it */
mnet_openssl_t* mnet_openssl_ctx_config(mnet_openssl_configurator configurator);

/* release openssl ctx */
void mnet_openssl_ctx_release(mnet_openssl_t *);

chann_t* mnet_openssl_chann_open(mnet_openssl_t *);
void mnet_openssl_chann_close(chann_t *);

int mnet_openssl_chann_listen(chann_t *n, const char *host, int port, int backlog);

int mnet_openssl_chann_connect(chann_t *n, const char *host, int port);
void mnet_openssl_chann_disconnect(chann_t *n);

void mnet_openssl_chann_set_opaque(chann_t *n, void *opaque);
void* mnet_openssl_chann_get_opaque(chann_t *n);

rw_result_t* mnet_openssl_chann_recv(chann_t *n, void *buf, int len);
rw_result_t* mnet_openssl_chann_send(chann_t *n, void *buf, int len);

int mnet_openssl_chann_state(chann_t *n);

#endif