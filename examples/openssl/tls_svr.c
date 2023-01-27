/*
 * Copyright (c) 2022 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include <assert.h>
#include "mnet_tls.h"

#ifdef MNET_OPENSSL_SVR_C

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

int main(int argc, char *argv[])
{
    const char *ipaddr = argc > 1 ? argv[1] : "127.0.0.1:8080";

    chann_addr_t addr;
    if (mnet_parse_ipport(ipaddr, &addr) <= 0)
    {
        printf("svr failed to parse ip:port\n");
        return 0;
    }

    mnet_init();
    SSL_CTX *ctx = _openssl_ctx();
    if (!mnet_tls_config(ctx))
    {
        SSL_CTX_free(ctx);
        printf("svr failed to config tls !\n");
        return 0;
    }

    chann_t *svr = mnet_chann_open(CHANN_TYPE_TLS);
    uint8_t buf[256];

    mnet_chann_listen(svr, addr.ip, addr.port, 2);
    printf("svr start listen: %s\n", ipaddr);

    // server will receive timer event every 5 second
    mnet_chann_active_event(svr, CHANN_EVENT_TIMER, 5 * MNET_MILLI_SECOND);

    for (;;)
    {
        if (mnet_poll(0.1 * MNET_MILLI_SECOND) <= 0)
        {
            printf("cnt no more channs, exit mnet_poll\n");
            break;
        }

        chann_msg_t *msg = NULL;
        while ((msg = mnet_result_next()))
        {
            if (msg->n == svr)
            {
                if (msg->event == CHANN_EVENT_ACCEPT)
                {
                    /* msg->n is NLLL, sever ssl accept client */
                    chann_addr_t addr;
                    mnet_chann_socket_addr(msg->r, &addr);
                    printf("svr accept cnt with chann %s:%d\n", addr.ip, addr.port);
                }
                else if (msg->event == CHANN_EVENT_TIMER)
                {
                    printf("svr current time: %zd\n", mnet_tm_current());
                }
                continue;
            }

            if (msg->event == CHANN_EVENT_RECV)
            {
                int len = mnet_chann_recv(msg->n, buf, 256);
                if (len > 0)
                {
                    buf[len] = 0;
                    printf("svr recv resquest: %d\n---\n", len);
                    fwrite(buf, len, 1, stdout);
                    printf("\n---\n");
                    //
                    char welcome[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 18\r\n\r\nHello MNet/OpenSSL\r\n\r\n";
                    len = mnet_chann_send(msg->n, welcome, sizeof(welcome));
                    printf("svr send response %d\n---\n%s\n---\n", len, welcome);
                }
                else if (len < 0)
                {
                    printf("svr failed to recv with ret: %d\n", len);
                }
            }
            else if (msg->event == CHANN_EVENT_DISCONNECT)
            {
                chann_addr_t addr;
                mnet_chann_socket_addr(msg->n, &addr);
                printf("svr disconnect cnt with chann %s:%d\n", addr.ip, addr.port);
                mnet_chann_close(msg->n);
            }
        }
    }

    mnet_fini();

    return 0;
}

#endif // MNET_OPENSSL_SVR_C