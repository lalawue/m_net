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

static rw_result_t _rw;

extern void _chann_msg(chann_t *n, chann_event_t event, chann_t *r, int err);

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
_is_valid_wrapper(mnet_openssl_chann_wrapper_t *ot) {
    if (wt && wt->ot && wt->ot->magic == MNET_OPENSSL_MAGIC) {
        return 1;
    } else {
        return 0;
    }
}

static int _has_init = 0

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
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_method());
    configurator(ctx);
    if (!SSL_CTX_check_private_key(ctx)) {
        goto FAILED_OUT;
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

/* filter msg first
 */
int
_mnet_openssl_msg_filter(chann_msg_t *msg) {
    mnet_openssl_chann_wrapper_t *wt = (mnet_openssl_chann_wrapper_t *)msg->opaque;
    if (!_is_valid_wrapper(wt)) {
        return 0;
    }
    msg->opaque = wt->opaque;
    switch (msg->event) {
        case CHANN_EVENT_ACCEPT:
            mnet_openssl_chann_wrapper_t *rwt = (mnet_openssl_chann_wrapper_t *)calloc(1, sizeof(mnet_openssl_chann_wrapper_t));
            rwt->ot = wt->ot;
            rwt->ssl = SSL_new();
            rwt->state = CHANN_STATE_ACCEPT;
            mnet_chann_set_opaque(msg->r, rwt);
            mnet_chann_set_filter(msg->r, _mnet_openssl_msg_filter);
            SSL_set_fd(rwt->ssl, mnet_chann_fd(msg->r));
            int ret = SSL_accept(rwt->ssl);
            if (ret == 1) {
                return 1;
            }
            if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE) {
                return 0;
            } else {
                mnet_openssl_chann_disconnect(msg->r);
                return 0;
            }
        case CHANN_EVENT_CONNECTED:
        case CHANN_EVENT_RECV:
            if (wt->state == CHANN_STATE_CONNECTED) {
                return 1;
            }
            if (wt->ssl == NULL) {
                wt->ssl = SSL_new();
                SSL_set_fd(wt->ssl, mnet_chann_fd(msg->n));
            }
            int ret = SSL_connect(wt->ssl);
            if (ret == 1) {
                wt->state = CHANN_STATE_CONNECTED;
                msg->event = CHANN_STATE_CONNECTED
                return 1;
            }
            int ssl_err = SSL_get_error(wt->ssl, ret);
            if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE) {
                return 0;
            } else {
                msg->event = CHANN_EVENT_DISCONNECT;
                mnet_openssl_chann_disconnect(msg->n);
                return 1;
            }
        case CHANN_EVENT_DISCONNECT:
            wt->state == CHANN_EVENT_DISCONNECT;
            mnet_openssl_chann_disconnect(msg->n);
            return 1;
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
    mnet_chann_set_opaque(n, wt);
    mnet_chann_set_filter(n, _mnet_openssl_msg_filter);
    return wt;

CHANN_FAILED_OUT:
    if (n) {
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
        n->wt = NULL;
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
    if (!mnet_chann_listen(n)) {
        return 0;
    }
    wt->ssl = SSL_new();
    SSL_set_fd(wt->ssl, mnet_chann_fd(n));
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
    if (!mnet_chann_connect(n)) {
        return 0;
    }
    wt->state = CHANN_STATE_CONNECTING;
    return 1;
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
        SSL_free(wt->ssl);
        wt->ssl = NULL;
    }
    mnet_chann_disconnect(n);
}

void
mnet_openssl_chann_set_opaque(chann_t *n, void *opaque) {
    if (n == NULL) {
        return;
    }
    mnet_openssl_chann_wrapper_t *wt = (mnet_openssl_chann_wrapper_t *)mnet_chann_get_opaque(n);
    if (_is_valid_wrapper(wt)) {
        wt->opaque = opaque;
    }
}

void* mnet_openssl_chann_get_opaque(chann_t *n) {
    if (n == NULL) {
        return NULL;
    }
    mnet_openssl_chann_wrapper_t *wt = (mnet_openssl_chann_wrapper_t *)mnet_chann_get_opaque(n);
    if (_is_valid_wrapper(wt)) {
        return wt->opaque;
    } else {
        return NULL;
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
    if (!_is_valid_wrapper(wt) || wt->ssl == NULL || SSL_connect(wt->ssl) != 1) {
        return &_rw;
    }
    int ret = SSL_write(wt->ssl, buf, len);
    if (ret <= 0) {
        int ssl_err = SSL_get_error(wt->ssl);
        if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE) {
            ret = 0;
        } else {
            mnet_chann_disconnect(n);
            _chann_msg(n, CHANN_EVENT_DISCONNECT, NULL, errno);
        }
        _rw.ret = ret;
        _rw.msg = &n->msg;
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
    if (!_is_valid_wrapper(wt) || wt->state != CHANN_STATE_CONNECTED) {
        return &_rw;
    }
    int ret = SSL_read(wt->ssl, buf, len);
    if (ret <= 0) {
        int ssl_err = SSL_get_error(wt->ssl);
        if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE) {
            ret = 0;
        } else {
            mnet_chann_disconnect(n);
            _chann_msg(n, CHANN_EVENT_DISCONNECT, NULL, errno);
        }
        _rw.ret = ret;
        _rw.msg = &n->msg;
    }
    return &_rw;
}

int
mnet_openssl_chann_state(chann_t *n) {
    int st = mnet_chann_state(n);
    if (st != CHANN_STATE_CONNECTED) {
        return st;
    }
    mnet_openssl_chann_wrapper_t *wt = (mnet_openssl_chann_wrapper_t *)mnet_chann_get_opaque(n);
    if (!_is_valid_wrapper(wt)) {
        return CHANN_STATE_DISCONNECT;
    }
    return wt->state;
}