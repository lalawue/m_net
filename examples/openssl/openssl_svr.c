/*
 * Copyright (c) 2022 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include "mnet_openssl.h"

/* config openssl
 */
static SSL_CTX *
_openssl_ctx(void)
{
    SSL_library_init();
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_server_method());
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    SSL_CTX_load_verify_locations(ctx, "examples/openssl/ca.crt", NULL);
    SSL_CTX_use_certificate_file(ctx, "examples/openssl/server.crt", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ctx, "examples/openssl/server.key", SSL_FILETYPE_PEM);
    return ctx;
}

int main(int argc, char *argv[])
{
    const char *ipaddr = argc > 0 ? argv[1] : "127.0.0.1:8080";

    chann_addr_t addr;
    if (mnet_parse_ipport(ipaddr, &addr) <= 0)
    {
        return 0;
    }

    // use pull style api
    mnet_init(1);
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

    chann_t *svr = mnet_openssl_chann_open(ot);
    poll_result_t *results = NULL;
    uint8_t buf[256];

    mnet_openssl_chann_listen(svr, addr.ip, addr.port, 2);
    printf("svr start listen: %s\n", ipaddr);

    for (;;)
    {
        results = mnet_poll(0.1 * MNET_MILLI_SECOND);
        if (results->chann_count <= 0)
        {
            break;
        }
        chann_msg_t *msg = NULL;
        while ((msg = mnet_result_next(results)))
        {
            if (msg->event == CHANN_EVENT_ACCEPT)
            {
                /* msg->n is NLLL, sever ssl accept client */
                chann_addr_t addr;
                mnet_chann_addr(msg->r, &addr);
                printf("svr accept cnt with chann %s:%d\n", addr.ip, addr.port);
            }
            else if (msg->event == CHANN_EVENT_RECV)
            {
                rw_result_t *rw = mnet_openssl_chann_recv(msg->n, buf, 256);
                if (rw->ret > 0)
                {
                    buf[rw->ret] = 0;
                    printf("resquest: %d\n---\n", rw->ret);
                    fwrite(buf, rw->ret, 1, stdout);
                    printf("\n---\n");
                    //
                    char welcome[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 18\r\n\r\nHello MNet/OpenSSL\r\n\r\n";
                    rw = mnet_openssl_chann_send(msg->n, welcome, sizeof(welcome));
                    printf("response %d\n---\n%s\n---\n", rw->ret, welcome);
                }
                else if (rw->ret < 0)
                {
                    printf("failed to recv with ret: %d\n", rw->ret);
                }
            }
            else if (msg->event == CHANN_EVENT_DISCONNECT)
            {
                chann_addr_t addr;
                mnet_chann_addr(msg->n, &addr);
                printf("svr disconnect cnt with chann %s:%d\n", addr.ip, addr.port);
                mnet_openssl_chann_close(msg->n);
            }
        }
    }

    mnet_fini();

    return 0;
}
