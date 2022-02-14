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
_openssl_configurator(void)
{
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_server_method());
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    SSL_CTX_load_verify_locations(ctx, "ca.crt", NULL);
    SSL_CTX_use_certificate_file(ctx, "server.crt", SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ctx, "server.key.unsecure", SSL_FILETYPE_PEM);
    return ctx;
}

int main(int argc, char *argv[])
{
    const char *ipaddr = argc > 1 ? argv[1] : "127.0.0.1:8080";

    chann_addr_t addr;
    if (mnet_parse_ipport(ipaddr, &addr) <= 0)
    {
        return 0;
    }

    mnet_init(1); /* use pull style api */
    mnet_openssl_t *ot = mnet_openssl_ctx_config(_openssl_configurator);
    if (ot)
    {
        printf("config openssl ctx.\n");
    }
    else
    {
        printf("cailed to config openssl ctx !\n");
        return 0;
    }

    chann_t *svr = mnet_openssl_chann_open(ot);
    poll_result_t *results = NULL;
    uint8_t buf[256];

    mnet_openssl_chann_listen(svr, addr.ip, addr.port, 2);
    printf("svr start listen: %s\n", ipaddr);

    // server will receive timer event every 5 second
    //mnet_chann_active_event(svr, CHANN_EVENT_TIMER, 5 * MNET_MILLI_SECOND);

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
            printf("msg event: %d\n", msg->event);
            if (msg->event == CHANN_EVENT_ACCEPT)
            {
                /* msg->n is NLLL, sever accept client */
                chann_addr_t addr;
                mnet_chann_addr(msg->r, &addr);
                printf("svr accept cnt with chann %s:%d\n", addr.ip, addr.port);
            }
            else if (msg->event == CHANN_EVENT_RECV) {
                rw_result_t *rw = mnet_openssl_chann_recv(msg->n, buf, 256);
                if (rw->ret > 0) {
                    buf[rw->ret] = 0;
                    printf("receive: %d\n---\n", rw->ret);
                    for (int i=0; i<rw->ret; i++) {
                        fwrite(buf, rw->ret, 1, stdout);
                    }
                    printf("\n---\n");
                    //
                    char welcome[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: 18\r\nHello mnet openssl\r\n\r\n";
                    mnet_openssl_chann_send(msg->r, welcome, sizeof(welcome));
                } else {
                    printf("failed to recv with ret: %d\n", rw->ret);
                }
            }
            else if (msg->event == CHANN_EVENT_TIMER)
            {
                printf("svr current time: %zd\n", mnet_current());
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
