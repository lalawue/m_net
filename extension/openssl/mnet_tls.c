/*
 * Copyright (c) 2022 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include "mnet_tls.h"

chann_type_t const CHANN_TYPE_TLS = 4;

typedef struct {
    chann_t *ln;    /* listen chann for next ACCEPT EVENT */
    int state;      /* TLS state */
    SSL *ssl;
} mnet_tls_ud_t;

/** man SSL_accept/SSL_connect/SSL_read/SSL_write
 */
static inline int
_ssl_is_rw(SSL *ssl, int ret) {
    int ssl_err = SSL_get_error(ssl, ret);
    //printf("SSL_get_error %d\n", ssl_err);
    if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE) {
        return 1;
    } else {
        return 0;
    }
}

static inline void
_ssl_set_fd(SSL *ssl, int fd) {
    SSL_set_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
    SSL_set_mode(ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    SSL_set_fd(ssl, fd);
}

static int
_tls_type_fn(void *ext_ctx, chann_type_t ctype) {
    return CHANN_TYPE_STREAM;
}

/** only filter chann required TLS handshake
 */
static int
_tls_filter_fn(void *ext_ctx, chann_msg_t *msg) {
    //printf("event:%d, n:%p, r:%p, tu:%p\n", msg->event, msg->n, msg->r, mnet_ext_chann_get_ud(msg->n));
    switch (msg->event) {
        case CHANN_EVENT_ACCEPT: {
            mnet_tls_ud_t *tu = mnet_ext_chann_get_ud(msg->r);
            if (tu) {
                tu->ln = msg->n;
            }
            return 0;
        }
        case CHANN_EVENT_CONNECTED:
        case CHANN_EVENT_RECV: {
            mnet_tls_ud_t *tu = mnet_ext_chann_get_ud(msg->n);
            if (tu == NULL || tu->state == CHANN_STATE_CONNECTED) {
                // no need handshake or just TLS connected, emit event
                return 1;
            }
            //printf("tu:%p, ssl:%p, state:%d\n", tu, tu->ssl, tu->state);
            int ret = tu->state == CHANN_STATE_LISTENING ? SSL_accept(tu->ssl) : SSL_connect(tu->ssl);
            if (ret == 1) {
                if (tu->state == CHANN_STATE_LISTENING) {
                    msg->event = CHANN_EVENT_ACCEPT;
                    msg->r = msg->n;
                    msg->n = tu->ln;
                    tu->ln = NULL;
                } else {
                    msg->event = CHANN_EVENT_CONNECTED;
                }
                tu->state = CHANN_STATE_CONNECTED;
                // emit accept/connected event
                return 1;
            }
            if (_ssl_is_rw(tu->ssl, ret)) {
                // read/write in process, block event
                return 0;
            } else {
                if (tu->state == CHANN_STATE_LISTENING) {
                    mnet_chann_disconnect(msg->n);
                    // block listening event
                    return 0;
                } else {
                    msg->event = CHANN_EVENT_DISCONNECT;
                    mnet_chann_disconnect(msg->n);
                    // emit disconnect event
                    return 1;
                }
            }
        }
        default:
            break;
    }
    return 1;
}

/** after chann alloc new context, link with TLS ud
 */
static void
_tls_open_cb(void *ext_ctx, chann_t *n) {
    //printf("open %p\n", n);
    mnet_tls_ud_t *tu = (mnet_tls_ud_t *)calloc(1, sizeof(mnet_tls_ud_t));
    if (tu) {
        tu->state = CHANN_STATE_DISCONNECT;
        mnet_ext_chann_set_ud(n, tu);
    }
}

/** before chann free all resouces, free TLS ud
 */
static void
_tls_close_cb(void *ext_ctx, chann_t *n) {
    //printf("close %p\n", n);
    mnet_tls_ud_t *tu = mnet_ext_chann_get_ud(n);
    if (tu && tu->state > CHANN_STATE_CLOSED) {
        tu->state = CHANN_STATE_CLOSED;
        if (tu->ssl) {
            SSL_free(tu->ssl);
        }
        free(tu);
        mnet_ext_chann_set_ud(n, NULL);
    }
}

static void
_tls_listen_cb(void *ext_ctx, chann_t *n) {
    //printf("listen %p\n", n);
    mnet_tls_ud_t *tu = mnet_ext_chann_get_ud(n);
    if (tu) {
        // listen chann no need TLS handshake
        mnet_ext_chann_set_ud(n, NULL);
        free(tu);
    }
}

/** chann accept new socket fd, link with new TLS ud and SSL
 */
static void
_tls_accept_cb(void *ext_ctx, chann_t *n) {
    //printf("accept %p\n", n);
    mnet_tls_ud_t *tu = (mnet_tls_ud_t *)calloc(1, sizeof(mnet_tls_ud_t));
    mnet_ext_chann_set_ud(n, tu);
    tu->state = CHANN_STATE_LISTENING;

    tu->ssl = SSL_new((SSL_CTX *)ext_ctx);
    _ssl_set_fd(tu->ssl, mnet_chann_fd(n));

    int ret = SSL_accept(tu->ssl);
    if (ret == 1) {
        tu->state = CHANN_STATE_CONNECTED;
    } else if (!_ssl_is_rw(tu->ssl, ret)) {
        mnet_chann_disconnect(n);
    }
}

/** chann create new socket fd, create new SSL
 */
static void
_tls_connect_cb(void *ext_ctx, chann_t *n) {
    //printf("connect %p\n", n);
    mnet_tls_ud_t *tu = mnet_ext_chann_get_ud(n);
    if (tu == NULL) {
        return;
    }
    tu->state = CHANN_STATE_CONNECTING;

    tu->ssl = SSL_new((SSL_CTX *)ext_ctx);
    _ssl_set_fd(tu->ssl, mnet_chann_fd(n));

    int ret = SSL_connect(tu->ssl);
    if (ret == 1 ) {
        tu->state = CHANN_STATE_CONNECTED;
    } else if (!_ssl_is_rw(tu->ssl, ret)) {
        mnet_chann_disconnect(n);
    }
}

/** before chann close fd, shutdown & clear SSL
 */
static void
_tls_disconnect_cb(void *ext_ctx, chann_t *n) {
    //printf("disconnect %p\n", n);
    mnet_tls_ud_t *tu = mnet_ext_chann_get_ud(n);
    if (tu && tu->state > CHANN_STATE_DISCONNECT) {
        tu->state = CHANN_STATE_DISCONNECT;
        SSL_shutdown(tu->ssl);
        SSL_clear(tu->ssl);
    }
}

/** return TLS state
 */
static int
_tls_state_fn(void *ext_ctx, chann_t *n, int state) {
    mnet_tls_ud_t *tu = mnet_ext_chann_get_ud(n);
    return tu ? tu->state : state;
}

static int
_tls_recv_fn(void *ext_ctx, chann_t *n, void *buf, int len) {
    //printf("recv %p\n", n);
    mnet_tls_ud_t *tu = mnet_ext_chann_get_ud(n);
    if (tu) {
        int ret = SSL_read(tu->ssl, buf, len);
        if (ret > 0) {
            return ret;
        }
        if (_ssl_is_rw(tu->ssl, ret)) {
            return 0;
        }
        return ret;
    } else {
        return -1;
    }
}

static int
_tls_send_fn(void *ext_ctx, chann_t *n, void *buf, int len) {
    //printf("send %p: %p,%d\n", n, buf, len);
    mnet_tls_ud_t *tu = mnet_ext_chann_get_ud(n);
    if (tu) {
        int ret = SSL_write(tu->ssl, buf, len);
        if (ret > 0) {
            return ret;
        }
        if (_ssl_is_rw(tu->ssl, ret)) {
            return 0;
        }
        return ret;
    } else {
        return -1;
    }
}

int
mnet_tls_config(SSL_CTX *ext_ctx) {
    if (ext_ctx) {
        mnet_ext_t ext = {
            .ext_ctx = ext_ctx,
            .type_fn = _tls_type_fn,
            .filter_fn = _tls_filter_fn,
            .open_cb = _tls_open_cb,
            .close_cb = _tls_close_cb,
            .listen_cb = _tls_listen_cb,
            .accept_cb = _tls_accept_cb,
            .connect_cb = _tls_connect_cb,
            .disconnect_cb = _tls_disconnect_cb,
            .state_fn = _tls_state_fn,
            .recv_fn = _tls_recv_fn,
            .send_fn = _tls_send_fn,
        };
        return mnet_ext_register(CHANN_TYPE_TLS, &ext);
    } else {
        return 0;
    }
}