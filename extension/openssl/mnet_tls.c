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
    chann_t *ln;    /* listen chann */
    int state;      /* TLS state */
    SSL *ssl;
} mnet_tls_ud_t;

static inline int
_is_ssl_rw(SSL *ssl, int ret) {
    int ssl_err = SSL_get_error(ssl, ret);
    if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE) {
        return 1;
    } else {
        return 0;
    }
}

static int
_tls_type_fn(void *ext_ctx, chann_type_t ctype) {
    return CHANN_TYPE_STREAM;
}

static int
_tls_filter_fn(void *ext_ctx, chann_msg_t *msg) {
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
            if (tu == NULL) {
                return 0;
            }
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
                return 1;
            }
            if (_is_ssl_rw(tu->ssl, ret)) {
                return 0;
            } else {
                if (tu->state == CHANN_STATE_LISTENING) {
                    mnet_chann_disconnect(msg->n);
                    return 0;
                } else {
                    msg->event = CHANN_EVENT_DISCONNECT;
                    mnet_chann_disconnect(msg->n);
                    return 1;
                }
            }
        }
        default:
            break;
    }
    return 1;
}

static void
_tls_open_cb(void *ext_ctx, chann_t *n) {
    mnet_tls_ud_t *tu = (mnet_tls_ud_t *)calloc(1, sizeof(mnet_tls_ud_t));
    if (tu) {
        tu->state = CHANN_STATE_DISCONNECT;
        mnet_ext_chann_set_ud(n, tu);
    }
}

static void
_tls_close_cb(void *ext_ctx, chann_t *n) {
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
}

static void
_tls_accept_cb(void *ext_ctx, chann_t *n) {
    mnet_tls_ud_t *tu = mnet_ext_chann_get_ud(n);
    if (tu == NULL) {
        return;
    }
    tu->ssl = SSL_new((SSL_CTX *)ext_ctx);
    SSL_set_mode(tu->ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
    SSL_set_fd(tu->ssl, mnet_chann_fd(n));
    int ret = SSL_accept(tu->ssl);
    if (ret == 1) {
        tu->state = CHANN_STATE_CONNECTED;
    } else if (_is_ssl_rw(tu->ssl, ret)) {
        tu->state = CHANN_STATE_LISTENING;
    } else {
        mnet_chann_disconnect(n);
    }
}

static void
_tls_connect_cb(void *ext_ctx, chann_t *n) {
    mnet_tls_ud_t *tu = mnet_ext_chann_get_ud(n);
    if (tu == NULL) {
        return;
    }
    tu->ssl = SSL_new((SSL_CTX *)ext_ctx);
    SSL_set_mode(tu->ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
    SSL_set_fd(tu->ssl, mnet_chann_fd(n));
    int ret = SSL_connect(tu->ssl);
    if (ret == 1) {
        tu->state = CHANN_STATE_CONNECTED;
    } else if (_is_ssl_rw(tu->ssl, ret)) {
        tu->state = CHANN_STATE_CONNECTING;
    } else {
        mnet_chann_disconnect(n);
    }
}

static void
_tls_disconnect_cb(void *ext_ctx, chann_t *n) {
    mnet_tls_ud_t *tu = mnet_ext_chann_get_ud(n);
    if (tu && tu->state > CHANN_STATE_DISCONNECT) {
        tu->state = CHANN_STATE_DISCONNECT;
        SSL_shutdown(tu->ssl);
        SSL_clear(tu->ssl);
    }
}

static int
_tls_state_fn(void *ext_ctx, chann_t *n, int state) {
    mnet_tls_ud_t *tu = mnet_ext_chann_get_ud(n);
    return tu ? tu->state : -1;
}

static int
_tls_recv_fn(void *ext_ctx, chann_t *n, void *buf, int len) {
    mnet_tls_ud_t *tu = mnet_ext_chann_get_ud(n);
    if (tu) {
        int ret = SSL_read(tu->ssl, buf, len);
        if (ret > 0) {
            return ret;
        }
        if (_is_ssl_rw(tu->ssl, ret)) {
            return 0;
        }
        return ret;
    } else {
        return -1;
    }
}

static int
_tls_send_fn(void *ext_ctx, chann_t *n, void *buf, int len) {
    mnet_tls_ud_t *tu = mnet_ext_chann_get_ud(n);
    if (tu) {
        int ret = SSL_write(tu->ssl, buf, len);
        if (ret > 0) {
            return ret;
        }
        if (_is_ssl_rw(tu->ssl, ret)) {
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