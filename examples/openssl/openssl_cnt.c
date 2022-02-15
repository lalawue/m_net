/*
 * Copyright (c) 2022 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include <string.h>
#include "mnet_openssl.h"

#ifdef MNET_OPENSSL_CNT

/* config openssl
 */
static SSL_CTX *
_openssl_ctx(void)
{
    SSL_library_init();
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_method());
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    SSL_CTX_load_verify_locations(ctx, "examples/openssl/ca.crt", NULL);
    SSL_CTX_use_certificate_file(ctx, "examples/openssl/server.crt", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ctx, "examples/openssl/server.key", SSL_FILETYPE_PEM);
    return ctx;
}

typedef struct {
    char buf[256];
} cnt_ctx_t;

static void
_cnt_msg_callback(chann_msg_t *msg)
{
    cnt_ctx_t *cnt_ctx = mnet_openssl_chann_get_opaque(msg->n);

    if (msg->event == CHANN_EVENT_CONNECTED)
    {
        /* msg->n is NLLL, sever ssl accept client */
        chann_addr_t addr;
        mnet_chann_socket_addr(msg->n, &addr);
        printf("cnt connected with chann %s:%d\n", addr.ip, addr.port);

        int ret = snprintf(cnt_ctx->buf, 256, "GET / HTTP/1.1\r\nUser-Agent: MNet/OpenSSL\r\nAccept: */*\r\n\r\n");
        rw_result_t *rw = mnet_openssl_chann_send(msg->n, cnt_ctx->buf, ret);
        if (rw->ret > 0)
        {
            printf("cnt send request to serevr with ret: %d, wanted: %d\n---\n%s\n---\n", rw->ret, ret, cnt_ctx->buf);
        }
        else
        {
            printf("cnt failed to write with ret %d\n", rw->ret);
        }
    }
    else if (msg->event == CHANN_EVENT_RECV)
    {
        rw_result_t *rw = mnet_openssl_chann_recv(msg->n, cnt_ctx->buf, 256);
        if (rw->ret > 0)
        {
            cnt_ctx->buf[rw->ret] = 0;
            printf("cnt recv response: %d\n---\n", rw->ret);
            fwrite(cnt_ctx->buf, rw->ret, 1, stdout);
            printf("\n---\n");
        }
        else if (rw->ret < 0)
        {
            printf("cnt failed to recv with ret: %d\n", rw->ret);
        }
    }
    else if (msg->event == CHANN_EVENT_DISCONNECT)
    {
        chann_addr_t addr;
        mnet_chann_socket_addr(msg->n, &addr);
        printf("cnt disconnect cnt with chann %s:%d\n", addr.ip, addr.port);
        mnet_openssl_chann_close(msg->n);
    }
}

int main(int argc, char *argv[])
{
    const char *ipaddr = argc > 1 ? argv[1] : "127.0.0.1:8080";

    chann_addr_t addr;
    if (mnet_parse_ipport(ipaddr, &addr) <= 0)
    {
        printf("cnt failed to parse ip:port\n");
        return 0;
    }

    // use callback style api
    mnet_init(0);
    SSL_CTX *ctx = _openssl_ctx();
    mnet_openssl_t *ot = mnet_openssl_ctx_config(ctx);
    if (ot)
    {
        printf("config openssl ctx.\n");
    }
    else
    {
        SSL_CTX_free(ctx);
        printf("failed to config openssl ctx !\n");
        return 0;
    }

    chann_t *cnt = mnet_openssl_chann_open(ot);
    poll_result_t *results = NULL;

    cnt_ctx_t cnt_ctx;
    memset(&cnt_ctx, 0, sizeof(cnt_ctx_t));

    mnet_openssl_chann_set_opaque(cnt, &cnt_ctx);
    mnet_openssl_chann_set_cb(cnt, _cnt_msg_callback);

    mnet_openssl_chann_connect(cnt, addr.ip, addr.port);
    printf("cnt start connect: %s\n", ipaddr);

    for (;;)
    {
        results = mnet_poll(0.1 * MNET_MILLI_SECOND);
        if (results->chann_count <= 0)
        {
            break;
        }
    }

    mnet_fini();

    return 0;
}

#endif