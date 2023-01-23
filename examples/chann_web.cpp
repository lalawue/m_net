/*
 * Copyright (c) 2019 lalawue
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

#ifdef EXAMPLE_CHANN_WEB_CPP

#include <iostream>
#include <unistd.h>
#include <string.h>
#include "mnet_wrapper.h"

#define RB_LEN 400

using mnet::Chann;
using mnet::ChannDispatcher;
using std::cout;
using std::endl;
using std::string;

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

// subclass Chann to handle event
class CntChann : public Chann
{
public:
    CntChann(Chann *c) : Chann(c)
    {
        m_rb = (ReadBuffer_t *)calloc(1, sizeof(ReadBuffer_t));
        {
            long len;
            const char *buf = getFileContent(&len);
            m_content = (char *)calloc(1, len + 128);
            int sz = snprintf(m_content, len + 128, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n", len);
            memcpy(&m_content[sz], buf, len);
            m_content_len = len + sz;
        }
    }

    // implement virtual defaultEventHandler
    void defaultEventHandler(Chann *accept, chann_event_t event, int err)
    {
        switch (event)
        {
        case CHANN_EVENT_RECV:
        {
            /* only recv one message then disconnect */
            int ret = channRecv(&m_rb->buf[m_rb->len], RB_LEN - m_rb->len);
            int count = countHttpRequest(m_rb, ret);
            while (count > 0)
            {
                count -= 1;
                channSend(m_content, m_content_len);
            }
            break;
        }
        case CHANN_EVENT_DISCONNECT:
        {
            channDisconnect();
            delete this;
            break;
        }
        default:
            break;
        }
    }
    ReadBuffer_t *m_rb;
    char m_buf[256];
    char *m_content = NULL;
    int m_content_len = 0;
};

class ListenChann : public Chann
{
public:
    ListenChann(string streamType) : Chann(streamType) {}

    // implement virtual defaultEventHandler
    void defaultEventHandler(Chann *accept, chann_event_t event, int err)
    {
        if (event == CHANN_EVENT_ACCEPT)
        {
            new CntChann(accept);
            delete accept;
        }
    }
};

int main(int argc, char *argv[])
{
    string ipaddr = argc > 1 ? argv[1] : "127.0.0.1:8080";

    ListenChann webSvr("tcp");

    if (webSvr.channListen(ipaddr, 100))
    {
        cout << "svr version: " << ChannDispatcher::version() << endl;
        cout << "svr start listen: " << ipaddr << endl;

        // server will receive timer event every 5 second
        webSvr.channActiveEvent(CHANN_EVENT_TIMER, 5 * MNET_MILLI_SECOND);

        while (ChannDispatcher::pollEvent(0.1 * MNET_MILLI_SECOND) > 0)
        {
        }
    }

    return 0;
}

#endif // EXAMPLE_CHANN_WEB_CPP