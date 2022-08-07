/*
 * Copyright (c) 2019 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mnet_core.h"

#ifdef EXAMPLE_CHANN_WEB_CALLBACK_STYLE

#define RB_LEN 400

static const char *getFileContent(long *len)
{
    FILE *fp = fopen("README.md", "rb");
    if (fp)
    {
        fseek(fp, 0, SEEK_END);
        *len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        char *buf = (char *)calloc(1, *len + 1);
        fread(buf, 1, *len, fp);
        fclose(fp);
        return buf;
    }
    return NULL;
}

typedef struct
{
    int len;
    unsigned char buf[RB_LEN];
} ReadBuffer_t;

// assume HTTP body is empty
static int countHttpRequest(ReadBuffer_t *rb, int in_len)
{
    if (in_len < 16)
    {
        return 0;
    }
    rb->len += in_len;
    int count = 0;
    int i = 0;
    int last_i = 0;
    for (i = 0; i < in_len - 3; i++)
    {
        if (rb->buf[i] == '\r' && rb->buf[i + 1] == '\n' && rb->buf[i + 2] == '\r' && rb->buf[i + 3] == '\n')
        {
            last_i = i + 4;
            i += 20;
            count += 1;
        }
    }
    if (count > 0)
    {
        rb->len = in_len - last_i;
        if (rb->len > 0)
        {
            memcpy(rb->buf, &rb->buf[last_i], rb->len);
        }
    }
    return count;
}

char *content = NULL;
int content_len = 0;

static void client_cb(chann_msg_t *msg)
{
    switch (msg->event)
    {
    case CHANN_EVENT_RECV:
    {
        /* only recv one message then disconnect */
        ReadBuffer_t *rb = (ReadBuffer_t *)mnet_chann_get_opaque(msg->n);
        rw_result_t *rw = mnet_chann_recv(msg->n, &rb->buf[rb->len], RB_LEN - rb->len);
        int count = countHttpRequest(rb, rw->ret);
        while (count > 0)
        {
            count -= 1;
            mnet_chann_send(msg->n, content, content_len);
        }
        break;
    }
    case CHANN_EVENT_DISCONNECT:
    {
        ReadBuffer_t *rb = (ReadBuffer_t *)mnet_chann_get_opaque(msg->n);
        if (rb)
        {
            free(rb);
        }
        mnet_chann_close(msg->n);
        break;
    }
    default:
        break;
    }
}

static void listen_cb(chann_msg_t *msg)
{
    if (msg->event == CHANN_EVENT_ACCEPT)
    {
        ReadBuffer_t *rb = (ReadBuffer_t *)calloc(1, sizeof(ReadBuffer_t));
        mnet_chann_set_opaque(msg->r, rb);
        mnet_chann_set_cb(msg->r, client_cb);
    }
}

int main(int argc, char *argv[])
{
    const char *ipaddr = argc > 1 ? argv[1] : "127.0.0.1:8080";

    chann_addr_t addr;
    if (mnet_parse_ipport(ipaddr, &addr) <= 0)
    {
        return 0;
    }

    mnet_init(0); /* use callback style api */

    // get 'README.md' as HTTP body
    {
        long len;
        const char *buf = getFileContent(&len);
        content = (char *)calloc(1, len + 128);
        int sz = snprintf(content, len + 128, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n", len);
        memcpy(&content[sz], buf, len);
        content_len = len + sz;
    }

    chann_t *svr = mnet_chann_open(CHANN_TYPE_STREAM);
    mnet_chann_set_cb(svr, listen_cb);
    mnet_chann_listen(svr, addr.ip, addr.port, 4);
    printf("svr start listen: %s\n", ipaddr);

    poll_result_t *results = NULL;
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