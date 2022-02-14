/*
 * Copyright (c) 2022 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include "mnet_openssl.h"

#define MNET_OPENSSL_MAGIC 0xbeef8bad

extern void process_chann_msg(chann_t *n, chann_event_t event, chann_t *r, int err);
extern int get_chann_errno();
extern chann_msg_t* get_chann_msg(chann_t *n);

static rw_result_t _rw;

/* mnet openssl ctx configure holder */
struct mnet_openssl {
    int magic;
    SSL_CTX *ctx;
};

/* chann_t opaque in openssl chann */
typedef struct {
    int state;
    mnet_openssl_t *ot;
    void *opaque;
    chann_msg_cb *cb;
    SSL *ssl;
} mnet_openssl_chann_wrapper_t;

static inline int
_is_valid_wrapper(mnet_openssl_chann_wrapper_t *wt) {
    if (wt && wt->ot && wt->ot->magic == MNET_OPENSSL_MAGIC) {
        return 1;
    } else {
        return 0;
    }
}

static int _has_init = 0;

mnet_openssl_t*
mnet_openssl_ctx_config(mnet_openssl_configurator configurator) {
    SSL_CTX *ctx = NULL;
    if (configurator == NULL) {
        goto CTX_FAILED_OUT;
    }
    if (!_has_init) {
        _has_init = 1;
        SSL_library_init();
    }
    ctx = configurator();
    if (!SSL_CTX_check_private_key(ctx)) {
        goto CTX_FAILED_OUT;
    }
    mnet_openssl_t *ot = (mnet_openssl_t *)calloc(1, sizeof(mnet_openssl_t));
    if (ot == NULL) {
        goto CTX_FAILED_OUT;
    }
    ot->magic = MNET_OPENSSL_MAGIC;
    ot->ctx = ctx;
    return ot;

CTX_FAILED_OUT:
    if (ctx) {
        SSL_CTX_free(ctx);
    }
    return NULL;
}

/* destroy openssl ctx */
void
mnet_openssl_ctx_release(mnet_openssl_t *ot) {
    if (ot && ot->ctx && ot->magic == MNET_OPENSSL_MAGIC) {
        SSL_CTX_free(ot->ctx);
        free(ot);
    }
}

/* check ssl read/write state
 */
static inline int
_is_ssl_rw(SSL *ssl, int ret) {
    int ssl_err = SSL_get_error(ssl, ret);
    if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE) {
        return 1;
    } else {
        return 0;
    }
}

/* filter msg first
 */
static int
_mnet_openssl_msg_filter(chann_msg_t *msg) {
    mnet_openssl_chann_wrapper_t *wt = (mnet_openssl_chann_wrapper_t *)msg->opaque;
    if (!_is_valid_wrapper(wt)) {
        return 0;
    }
    msg->opaque = wt->opaque;
    switch (msg->event) {
        case CHANN_EVENT_ACCEPT: {
            mnet_openssl_chann_wrapper_t *rwt = (mnet_openssl_chann_wrapper_t *)calloc(1, sizeof(mnet_openssl_chann_wrapper_t));
            rwt->ot = wt->ot;
            rwt->state = CHANN_STATE_LISTENING;
            rwt->ssl = SSL_new(wt->ot->ctx);
            SSL_set_mode(rwt->ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
            SSL_set_fd(rwt->ssl, mnet_chann_fd(msg->r));
            mnet_chann_set_opaque(msg->r, rwt);
            mnet_chann_set_filter(msg->r, _mnet_openssl_msg_filter);
            return 0;
        }
        case CHANN_EVENT_CONNECTED:
        case CHANN_EVENT_RECV: {
            if (wt->state == CHANN_STATE_CONNECTED) {
                return 1;
            }
            int ret = wt->state == CHANN_STATE_LISTENING ? SSL_accept(wt->ssl) : SSL_connect(wt->ssl);
            if (ret == 1) {
                if (wt->state == CHANN_STATE_LISTENING) {
                    msg->event = CHANN_EVENT_ACCEPT;
                    msg->r = msg->n;
                    msg->n = NULL;
                } else {
                    msg->event = CHANN_EVENT_CONNECTED;
                }
                wt->state = CHANN_STATE_CONNECTED;
                return 1;
            }
            if (_is_ssl_rw(wt->ssl, ret)) {
                return 0;
            } else {
                if (wt->state == CHANN_STATE_LISTENING) {
                    mnet_openssl_chann_disconnect(msg->n);
                    return 0;
                } else {
                    msg->event = CHANN_EVENT_DISCONNECT;
                    mnet_openssl_chann_disconnect(msg->n);
                    return 1;
                }
            }
        }
        case CHANN_EVENT_DISCONNECT: {
            mnet_openssl_chann_disconnect(msg->n);
            return 1;
        }
        default:
            break;
    }
    return 1;
}

/* create openssl chann
 */
chann_t*
mnet_openssl_chann_open(mnet_openssl_t *ot) {
    chann_t *n = mnet_chann_open(CHANN_TYPE_STREAM);
    if (n == NULL) {
        goto CHANN_FAILED_OUT;
    }
    mnet_openssl_chann_wrapper_t *wt = (mnet_openssl_chann_wrapper_t *)calloc(1, sizeof(mnet_openssl_chann_wrapper_t));
    if (wt == NULL) {
        goto CHANN_FAILED_OUT;
    }
    wt->ot = ot;
    wt->state = CHANN_STATE_DISCONNECT;
    mnet_chann_set_opaque(n, wt);
    mnet_chann_set_filter(n, _mnet_openssl_msg_filter);
    return n;

CHANN_FAILED_OUT:
    if (n) {
        mnet_chann_disconnect(n);
        mnet_chann_close(n);
    }
    return NULL;
}

void
mnet_openssl_chann_close(chann_t *n) {
    if (n == NULL) {
        return;
    }
    mnet_openssl_chann_wrapper_t *wt = (mnet_openssl_chann_wrapper_t *)mnet_chann_get_opaque(n);
    if (_is_valid_wrapper(wt)) {
        if (wt->ssl) {
            SSL_free(wt->ssl);
        }
        free(wt);
        mnet_chann_set_opaque(n, NULL);
    }
    mnet_chann_close(n);
}

int
mnet_openssl_chann_listen(chann_t *n, const char *host, int port, int backlog) {
    if (n == NULL || mnet_chann_state(n) != CHANN_STATE_DISCONNECT) {
        return 0;
    }
    mnet_openssl_chann_wrapper_t *wt = (mnet_openssl_chann_wrapper_t *)mnet_chann_get_opaque(n);
    if (!_is_valid_wrapper(wt)) {
        return 0;
    }
    if (!mnet_chann_listen(n, host, port, backlog)) {
        return 0;
    }
    wt->state = CHANN_STATE_LISTENING;
    return 1;
}

int
mnet_openssl_chann_connect(chann_t *n, const char *host, int port) {
    if (n == NULL || mnet_chann_state(n) != CHANN_STATE_DISCONNECT) {
        return 0;
    }
    mnet_openssl_chann_wrapper_t *wt = (mnet_openssl_chann_wrapper_t *)mnet_chann_get_opaque(n);
    if (!_is_valid_wrapper(wt)) {
        return 0;
    }
    if (!mnet_chann_connect(n, host, port)) {
        return 0;
    }
    if (wt->ssl == NULL) {
        wt->ssl = SSL_new(wt->ot->ctx);
    }
    SSL_set_mode(wt->ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
    SSL_set_fd(wt->ssl, mnet_chann_fd(n));
    int ret = SSL_connect(wt->ssl);
    if (ret == 1) {
        wt->state = CHANN_STATE_CONNECTED;
        return 1;
    }
    wt->state = CHANN_STATE_CONNECTING;
    if (_is_ssl_rw(wt->ssl, ret)) {
        return 1;
    } else {
        mnet_openssl_chann_close(n);
        return 0;
    }
}

void
mnet_openssl_chann_disconnect(chann_t *n) {
    if (n == NULL) {
        return;
    }
    mnet_openssl_chann_wrapper_t *wt = (mnet_openssl_chann_wrapper_t *)mnet_chann_get_opaque(n);
    if (!_is_valid_wrapper(wt) || wt->state <= CHANN_STATE_DISCONNECT) {
        return;
    }
    wt->state = CHANN_STATE_DISCONNECT;
    if (wt->ssl) {
        SSL_shutdown(wt->ssl);
        SSL_clear(wt->ssl);
    }
    mnet_chann_disconnect(n);
}

void
mnet_openssl_chann_set_cb(chann_t *n, chann_msg_cb cb) {
    mnet_chann_set_cb(n, cb);
}

void
mnet_openssl_chann_set_opaque(chann_t *n, void *opaque) {
    mnet_openssl_chann_wrapper_t *wt = (mnet_openssl_chann_wrapper_t *)mnet_chann_get_opaque(n);
    if (_is_valid_wrapper(wt)) {
        wt->opaque = opaque;
    }
}

void*
mnet_openssl_chann_get_opaque(chann_t *n) {
    mnet_openssl_chann_wrapper_t *wt = (mnet_openssl_chann_wrapper_t *)mnet_chann_get_opaque(n);
    if (_is_valid_wrapper(wt)) {
        return wt->opaque;
    } else {
        return NULL;
    }
}

void
mnet_openssl_chann_active_event(chann_t *n, chann_event_t et, int64_t value) {
    mnet_openssl_chann_wrapper_t *wt = (mnet_openssl_chann_wrapper_t *)mnet_chann_get_opaque(n);
    if (_is_valid_wrapper(wt) && wt->state == CHANN_STATE_CONNECTED) {
        mnet_chann_active_event(n, et, value);
    }
}

rw_result_t*
mnet_openssl_chann_recv(chann_t *n, void *buf, int len) {
    _rw.ret = -1;
    _rw.msg = NULL;
    if (n == NULL || buf == NULL || len <= 0) {
        return &_rw;
    }
    mnet_openssl_chann_wrapper_t *wt = (mnet_openssl_chann_wrapper_t *)mnet_chann_get_opaque(n);
    if (!_is_valid_wrapper(wt) || wt->state < CHANN_STATE_CONNECTED) {
        return &_rw;
    }
    _rw.ret = SSL_read(wt->ssl, buf, len);
    if (_rw.ret <= 0) {
        if (_is_ssl_rw(wt->ssl, _rw.ret)) {
            _rw.ret = 0;
        } else {
            mnet_chann_disconnect(n);
            process_chann_msg(n, CHANN_EVENT_DISCONNECT, NULL, get_chann_errno());
        }
        _rw.msg = get_chann_msg(n);
    }
    return &_rw;
}

rw_result_t*
mnet_openssl_chann_send(chann_t *n, void *buf, int len) {
    _rw.ret = -1;
    _rw.msg = NULL;
    if (n == NULL || buf == NULL || len <= 0) {
        return &_rw;
    }
    mnet_openssl_chann_wrapper_t *wt = (mnet_openssl_chann_wrapper_t *)mnet_chann_get_opaque(n);
    if (!_is_valid_wrapper(wt) || wt->state < CHANN_STATE_CONNECTED) {
        return &_rw;
    }
    _rw.ret = SSL_write(wt->ssl, buf, len);
    if (_rw.ret <= 0) {
        if (_is_ssl_rw(wt->ssl, _rw.ret)) {
            _rw.ret = 0;
        } else {
            mnet_chann_disconnect(n);
            process_chann_msg(n, CHANN_EVENT_DISCONNECT, NULL, get_chann_errno());
        }
        _rw.msg = get_chann_msg(n);
    }
    return &_rw;
}

int
mnet_openssl_chann_state(chann_t *n) {
    mnet_openssl_chann_wrapper_t *wt = (mnet_openssl_chann_wrapper_t *)mnet_chann_get_opaque(n);
    if (_is_valid_wrapper(wt)) {
        return wt->state;
    } else {
        return -1;
    }
}