/*
 * Copyright (c) 2022 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mnet_tls.h"

#ifdef MNET_OPENSSL_CNT_C

/* config openssl
 */
static SSL_CTX *
_openssl_ctx(void)
{
    SSL_library_init();
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_method());
#if 0
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
#else
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    SSL_CTX_load_verify_locations(ctx, "examples/openssl/ca.crt", NULL);
    SSL_CTX_use_certificate_file(ctx, "examples/openssl/server.crt", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ctx, "examples/openssl/server.key", SSL_FILETYPE_PEM);
    assert(SSL_CTX_check_private_key(ctx) == 1);
#endif
    return ctx;
}

typedef struct
{
    char buf[256];
} cnt_ctx_t;

static void
_cnt_msg_callback(chann_msg_t *msg)
{
    cnt_ctx_t *cnt_ctx = mnet_chann_get_opaque(msg->n);

    if (msg->event == CHANN_EVENT_CONNECTED)
    {
        /* msg->n is NLLL, sever ssl accept client */
        chann_addr_t addr;
        mnet_chann_socket_addr(msg->n, &addr);
        printf("cnt connected with chann %s:%d\n", addr.ip, addr.port);

        int ret = snprintf(cnt_ctx->buf, 256, "GET / HTTP/1.1\r\nUser-Agent: MNet/OpenSSL\r\nAccept: */*\r\n\r\n");
        int len = mnet_chann_send(msg->n, cnt_ctx->buf, ret);
        printf("cnt send request to serevr with ret: %d, wanted: %d\n---\n%s\n---\n", len, ret, cnt_ctx->buf);
    }
    else if (msg->event == CHANN_EVENT_RECV)
    {
        int len = mnet_chann_recv(msg->n, cnt_ctx->buf, 256);
        if (len > 0)
        {
            cnt_ctx->buf[len] = 0;
            printf("cnt recv response: %d\n---\n", len);
            fwrite(cnt_ctx->buf, len, 1, stdout);
            printf("\n---\n");
        }
        else if (len < 0)
        {
            printf("cnt failed to recv with ret: %d\n", len);
        }
    }
    else if (msg->event == CHANN_EVENT_DISCONNECT)
    {
        chann_addr_t addr;
        mnet_chann_socket_addr(msg->n, &addr);
        printf("cnt disconnect cnt with chann %s:%d\n", addr.ip, addr.port);
        mnet_chann_close(msg->n);
    }
}

int main(int argc, char *argv[])
{
    const char *ipaddr = argc > 1 ? argv[1] : "127.0.0.1:8080";

    chann_addr_t addr;
    if (mnet_parse_ipport(ipaddr, &addr) <= 0)
    {
        printf("cnt failed to parse ip:port !\n");
        return 0;
    }

    // use callback style api
    mnet_init();
    SSL_CTX *ctx = _openssl_ctx();
    if (!mnet_tls_config(ctx))
    {
        SSL_CTX_free(ctx);
        printf("cnt failed to config tls !\n");
        return 0;
    }

    chann_t *cnt = mnet_chann_open(CHANN_TYPE_TLS);

    cnt_ctx_t cnt_ctx;
    memset(&cnt_ctx, 0, sizeof(cnt_ctx_t));

    mnet_chann_set_opaque(cnt, &cnt_ctx);
    //mnet_chann_set_cb(cnt, _cnt_msg_callback);

    mnet_chann_connect(cnt, addr.ip, addr.port);
    printf("cnt start connect: %s\n", ipaddr);

    for (;;)
    {
        if (mnet_poll(0.1 * MNET_MILLI_SECOND) <= 0)
        {
            break;
        }

        chann_msg_t *msg = NULL;
        while ((msg = mnet_result_next())) {
            _cnt_msg_callback(msg);
        }
    }

    mnet_fini();

    return 0;
}

#endif // MNET_OPENSSL_CNT_C