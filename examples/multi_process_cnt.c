/*
 * Copyright (c) 2023 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifdef EXAMPLE_MULTI_PROCESS_CNT_C
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "mnet_core.h"

typedef struct
{
    unsigned char buf[32];
    int len;
} buf_t;

int token = 0;

static void
_clear_chann(chann_t *n)
{
    void *p = mnet_chann_get_opaque(n);
    if (p)
    {
        free(p);
        mnet_chann_set_opaque(n, NULL);
    }
}

static void
_on_cnt_event(chann_msg_t *msg)
{
    if (msg->event == CHANN_EVENT_CONNECTED)
    {
        printf("multi cnt connected\n");
        buf_t *bt = (buf_t *)mnet_chann_get_opaque(msg->n);
        token += 1;
        snprintf(bt->buf, 32, "%010d", token);
        // printf("multi cnt send %s\n", bt->buf);
        mnet_chann_send(msg->n, bt->buf, 10);
    }
    if (msg->event == CHANN_EVENT_RECV)
    {
        buf_t *bt = (buf_t *)mnet_chann_get_opaque(msg->n);
        int ret = mnet_chann_recv(msg->n, &bt->buf[bt->len], 10 - bt->len);
        if (ret + bt->len < 10)
        {
            bt->len += ret;
        }
        else
        {
            bt->len = 0;
            bt->buf[10] = '\0';
            printf("multi cnt recv %s\n", bt->buf);
            _clear_chann(msg->n);
            mnet_chann_close(msg->n);
        }
    }
    if (msg->event == CHANN_EVENT_DISCONNECT)
    {
        printf("multi cnt disconnect !\n");
        _clear_chann(msg->n);
        mnet_chann_close(msg->n);
    }
}

int main(int argc, char *argv[])
{
    const char *ipaddr = argc > 1 ? argv[1] : "127.0.0.1:8090";
    chann_addr_t addr;

    if (mnet_parse_ipport(ipaddr, &addr) > 0)
    {
        mnet_init();
        printf("mnet version %d\n", mnet_version());

        while (1)
        {
            int cnt_count = mnet_poll(50);

            for (int i = cnt_count; i < (random() % 6); i++)
            {
                chann_t *cnt = mnet_chann_open(CHANN_TYPE_STREAM);
                mnet_chann_set_opaque(cnt, calloc(1, sizeof(buf_t)));
                mnet_chann_connect(cnt, addr.ip, addr.port);
                printf("multi cnt %d try connect %s:%d...\n", mnet_chann_fd(cnt), addr.ip, addr.port);
            }

            chann_msg_t *msg = NULL;
            while ((msg = mnet_result_next()))
            {
                _on_cnt_event(msg);
            }

            usleep(10);
        }
        mnet_fini();
    }

    return 0;
}

#endif // EXAMPLE_MULTI_PROCESS_CNT_C